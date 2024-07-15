                                                                            
   
                  
                                                            
   
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                                            
   
#include "postgres_fe.h"

#include "pg_backup_archiver.h"
#include "pg_backup_utils.h"
#include "pg_dump.h"

#include "catalog/pg_class_d.h"

   
                                            
                                                          
   
                                                                             
                                                                          
                                                                        
                                                                          
                                                                        
                                                                              
                                                                              
                                                                          
                                      
   
                                                                            
                                                                          
                                                                        
                                                                        
                                                                       
                                                                           
                                                                           
                         
   
                                                                           
                                                                               
                                                                             
                           
   
static const int dbObjectTypePriority[] = {
    1,                    
    4,                    
    5,               
    5,                     
    7,               
    8,              
    9,                   
    9,                        
    10,                 
    10,                  
    3,                    
    11,                    
    18,               
    20,                 
    28,               
    29,                      
    30,                  
    31,              
    32,                 
    27,                    
    33,                       
    2,                   
    6,               
    23,                    
    24,                      
    19,                    
    12,                  
    14,                
    13,                    
    15,                  
    16,             
    17,                        
    38,                                          
    3,                    
    21,              
    25,                   
    22,                           
    26,                            
    39,                                         
    40,                                   
    34,                
    35,                     
    36,                         
    37                       
};

static DumpId preDataBoundId;
static DumpId postDataBoundId;

static int
DOTypeNameCompare(const void *p1, const void *p2);
static bool
TopoSort(DumpableObject **objs, int numObjs, DumpableObject **ordering, int *nOrdering);
static void
addHeapElement(int val, int *heap, int heapLength);
static int
removeHeapElement(int *heap, int heapLength);
static void
findDependencyLoops(DumpableObject **objs, int nObjs, int totObjs);
static int
findLoop(DumpableObject *obj, DumpId startPoint, bool *processed, DumpId *searchFailed, DumpableObject **workspace, int depth);
static void
repairDependencyLoop(DumpableObject **loop, int nLoop);
static void
describeDumpableObject(DumpableObject *obj, char *buf, int bufsize);

   
                                                          
   
                                                                     
             
   
void
sortDumpableObjectsByTypeName(DumpableObject **objs, int numObjs)
{
  if (numObjs > 1)
  {
    qsort((void *)objs, numObjs, sizeof(DumpableObject *), DOTypeNameCompare);
  }
}

static int
DOTypeNameCompare(const void *p1, const void *p2)
{
  DumpableObject *obj1 = *(DumpableObject *const *)p1;
  DumpableObject *obj2 = *(DumpableObject *const *)p2;
  int cmpval;

                               
  cmpval = dbObjectTypePriority[obj1->objType] - dbObjectTypePriority[obj2->objType];

  if (cmpval != 0)
  {
    return cmpval;
  }

     
                                                                           
                                                                         
                                                       
     
  if (obj1->namespace)
  {
    if (obj2->namespace)
    {
      cmpval = strcmp(obj1->namespace->dobj.name, obj2->namespace->dobj.name);
      if (cmpval != 0)
      {
        return cmpval;
      }
    }
    else
    {
      return -1;
    }
  }
  else if (obj2->namespace)
  {
    return 1;
  }

                    
  cmpval = strcmp(obj1->name, obj2->name);
  if (cmpval != 0)
  {
    return cmpval;
  }

                                                                     
  if (obj1->objType == DO_FUNC || obj1->objType == DO_AGG)
  {
    FuncInfo *fobj1 = *(FuncInfo *const *)p1;
    FuncInfo *fobj2 = *(FuncInfo *const *)p2;
    int i;

                                                               
    cmpval = fobj1->nargs - fobj2->nargs;
    if (cmpval != 0)
    {
      return cmpval;
    }
    for (i = 0; i < fobj1->nargs; i++)
    {
      TypeInfo *argtype1 = findTypeByOid(fobj1->argtypes[i]);
      TypeInfo *argtype2 = findTypeByOid(fobj2->argtypes[i]);

      if (argtype1 && argtype2)
      {
        if (argtype1->dobj.namespace && argtype2->dobj.namespace)
        {
          cmpval = strcmp(argtype1->dobj.namespace->dobj.name, argtype2->dobj.namespace->dobj.name);
          if (cmpval != 0)
          {
            return cmpval;
          }
        }
        cmpval = strcmp(argtype1->dobj.name, argtype2->dobj.name);
        if (cmpval != 0)
        {
          return cmpval;
        }
      }
    }
  }
  else if (obj1->objType == DO_OPERATOR)
  {
    OprInfo *oobj1 = *(OprInfo *const *)p1;
    OprInfo *oobj2 = *(OprInfo *const *)p2;

                                                                        
    cmpval = (oobj2->oprkind - oobj1->oprkind);
    if (cmpval != 0)
    {
      return cmpval;
    }
  }
  else if (obj1->objType == DO_ATTRDEF)
  {
    AttrDefInfo *adobj1 = *(AttrDefInfo *const *)p1;
    AttrDefInfo *adobj2 = *(AttrDefInfo *const *)p2;

                                  
    cmpval = (adobj1->adnum - adobj2->adnum);
    if (cmpval != 0)
    {
      return cmpval;
    }
  }
  else if (obj1->objType == DO_POLICY)
  {
    PolicyInfo *pobj1 = *(PolicyInfo *const *)p1;
    PolicyInfo *pobj2 = *(PolicyInfo *const *)p2;

                                                                     
    cmpval = strcmp(pobj1->poltable->dobj.name, pobj2->poltable->dobj.name);
    if (cmpval != 0)
    {
      return cmpval;
    }
  }
  else if (obj1->objType == DO_TRIGGER)
  {
    TriggerInfo *tobj1 = *(TriggerInfo *const *)p1;
    TriggerInfo *tobj2 = *(TriggerInfo *const *)p2;

                                                                     
    cmpval = strcmp(tobj1->tgtable->dobj.name, tobj2->tgtable->dobj.name);
    if (cmpval != 0)
    {
      return cmpval;
    }
  }

                                                             
  return oidcmp(obj1->catId.oid, obj2->catId.oid);
}

   
                                                                  
                                                     
   
                                                                           
                                                                             
   
void
sortDumpableObjects(DumpableObject **objs, int numObjs, DumpId preBoundaryId, DumpId postBoundaryId)
{
  DumpableObject **ordering;
  int nOrdering;

  if (numObjs <= 0)                               
  {
    return;
  }

     
                                                                            
                                                                         
     
  preDataBoundId = preBoundaryId;
  postDataBoundId = postBoundaryId;

  ordering = (DumpableObject **)pg_malloc(numObjs * sizeof(DumpableObject *));
  while (!TopoSort(objs, numObjs, ordering, &nOrdering))
  {
    findDependencyLoops(ordering, nOrdering, numObjs);
  }

  memcpy(objs, ordering, numObjs * sizeof(DumpableObject *));

  free(ordering);
}

   
                                               
   
                                                                             
                                                                             
                                                                        
                                 
   
                                                                         
             
   
                                                                    
                                                                    
   
                                                                         
                                                                      
   
                                                                             
                                                                          
                                                                              
                                                                             
                                                                           
                                                               
   
                                                                           
   
static bool
TopoSort(DumpableObject **objs, int numObjs, DumpableObject **ordering,                      
    int *nOrdering)                                                                          
{
  DumpId maxDumpId = getMaxDumpId();
  int *pendingHeap;
  int *beforeConstraints;
  int *idMap;
  DumpableObject *obj;
  int heapLength;
  int i, j, k;

     
                                                                           
                                                                       
                                                                            
                                                                          
                                                                        
                                                                            
                                                                        
                                                                           
                                                                        
                                              
     

  *nOrdering = numObjs;                         

                               
  if (numObjs <= 0)
  {
    return true;
  }

                                                     
  pendingHeap = (int *)pg_malloc(numObjs * sizeof(int));

     
                                                                            
                                                                             
                                                                             
                                                                       
               
     
  beforeConstraints = (int *)pg_malloc0((maxDumpId + 1) * sizeof(int));
  idMap = (int *)pg_malloc((maxDumpId + 1) * sizeof(int));
  for (i = 0; i < numObjs; i++)
  {
    obj = objs[i];
    j = obj->dumpId;
    if (j <= 0 || j > maxDumpId)
    {
      fatal("invalid dumpId %d", j);
    }
    idMap[j] = i;
    for (j = 0; j < obj->nDeps; j++)
    {
      k = obj->dependencies[j];
      if (k <= 0 || k > maxDumpId)
      {
        fatal("invalid dependency %d", k);
      }
      beforeConstraints[k]++;
    }
  }

     
                                                                             
                                                                    
     
                                                                             
                                                                        
                                                                             
                                                                            
                                                                        
                                                         
     
  heapLength = 0;
  for (i = numObjs; --i >= 0;)
  {
    if (beforeConstraints[objs[i]->dumpId] == 0)
    {
      pendingHeap[heapLength++] = i;
    }
  }

                         
                                                                            
                                                                            
                                                                          
                                                                         
                                                                             
                                                                             
                                                                          
                                                                       
                          
                                                     
                                                
                                                      
                         
     
  i = numObjs;
  while (heapLength > 0)
  {
                                                                 
    j = removeHeapElement(pendingHeap, heapLength--);
    obj = objs[j];
                                        
    ordering[--i] = obj;
                                                             
    for (k = 0; k < obj->nDeps; k++)
    {
      int id = obj->dependencies[k];

      if ((--beforeConstraints[id]) == 0)
      {
        addHeapElement(idMap[id], pendingHeap, heapLength++);
      }
    }
  }

     
                                                                             
                                                  
     
  if (i != 0)
  {
    k = 0;
    for (j = 1; j <= maxDumpId; j++)
    {
      if (beforeConstraints[j] != 0)
      {
        ordering[k++] = objs[idMap[j]];
      }
    }
    *nOrdering = k;
  }

            
  free(pendingHeap);
  free(beforeConstraints);
  free(idMap);

  return (i == 0);
}

   
                                          
   
                                                                             
                                                                         
   
static void
addHeapElement(int val, int *heap, int heapLength)
{
  int j;

     
                                                                            
                                               
     
  j = heapLength;
  while (j > 0)
  {
    int i = (j - 1) >> 1;

    if (val <= heap[i])
    {
      break;
    }
    heap[j] = heap[i];
    j = i;
  }
  heap[j] = val;
}

   
                                                              
   
                                                                             
                             
   
                                                                        
                                                                
   
static int
removeHeapElement(int *heap, int heapLength)
{
  int result = heap[0];
  int val;
  int i;

  if (--heapLength <= 0)
  {
    return result;
  }
  val = heap[heapLength];                                    
  i = 0;                                                
  for (;;)
  {
    int j = 2 * i + 1;

    if (j >= heapLength)
    {
      break;
    }
    if (j + 1 < heapLength && heap[j] < heap[j + 1])
    {
      j++;
    }
    if (val >= heap[j])
    {
      break;
    }
    heap[i] = heap[j];
    i = j;
  }
  heap[i] = val;
  return result;
}

   
                                                                      
                                                                 
   
                                                                        
                                                                       
                                                                      
                                                                      
                                                                        
                                                                     
                                       
   
                                                   
                                       
                                                          
   
static void
findDependencyLoops(DumpableObject **objs, int nObjs, int totObjs)
{
     
                                        
     
                                                                         
                                                                        
     
                                                                             
                                                                         
                                                                          
                                                                             
                                                                        
                                                                            
                                                                            
                                                                             
            
     
                                                                            
                                                                           
                                                                        
                                                                             
                                                        
     
  bool *processed;
  DumpId *searchFailed;
  DumpableObject **workspace;
  bool fixedloop;
  int i;

  processed = (bool *)pg_malloc0((getMaxDumpId() + 1) * sizeof(bool));
  searchFailed = (DumpId *)pg_malloc0((getMaxDumpId() + 1) * sizeof(DumpId));
  workspace = (DumpableObject **)pg_malloc(totObjs * sizeof(DumpableObject *));
  fixedloop = false;

  for (i = 0; i < nObjs; i++)
  {
    DumpableObject *obj = objs[i];
    int looplen;
    int j;

    looplen = findLoop(obj, obj->dumpId, processed, searchFailed, workspace, 0);

    if (looplen > 0)
    {
                                   
      repairDependencyLoop(workspace, looplen);
      fixedloop = true;
                                          
      for (j = 0; j < looplen; j++)
      {
        processed[workspace[j]->dumpId] = true;
      }
    }
    else
    {
         
                                                                        
                                                                         
                                                                        
                         
         
      processed[obj->dumpId] = true;
    }
  }

                                                
  if (!fixedloop)
  {
    fatal("could not identify dependency loop");
  }

  free(workspace);
  free(searchFailed);
  free(processed);
}

   
                                                                          
                                  
   
                                    
                                                                         
                                                             
                                                                             
                                                                         
                                                         
   
                                                                             
                                                                       
   
                                                                           
                                                                       
   
static int
findLoop(DumpableObject *obj, DumpId startPoint, bool *processed, DumpId *searchFailed, DumpableObject **workspace, int depth)
{
  int i;

     
                                                                             
                                                    
     
  if (processed[obj->dumpId])
  {
    return 0;
  }

     
                                                                           
                            
     
  if (searchFailed[obj->dumpId] == startPoint)
  {
    return 0;
  }

     
                                                                           
                                                                            
                                                                           
                                                       
     
  for (i = 0; i < depth; i++)
  {
    if (workspace[i] == obj)
    {
      return 0;
    }
  }

     
                                            
     
  workspace[depth++] = obj;

     
                                                                           
     
  for (i = 0; i < obj->nDeps; i++)
  {
    if (obj->dependencies[i] == startPoint)
    {
      return depth;
    }
  }

     
                                       
     
  for (i = 0; i < obj->nDeps; i++)
  {
    DumpableObject *nextobj = findObjectByDumpId(obj->dependencies[i]);
    int newDepth;

    if (!nextobj)
    {
      continue;                                              
    }
    newDepth = findLoop(nextobj, startPoint, processed, searchFailed, workspace, depth);
    if (newDepth > 0)
    {
      return newDepth;
    }
  }

     
                                                            
     
  searchFailed[obj->dumpId] = startPoint;

  return 0;
}

   
                                                                        
                                                                     
                                                                            
                                                                           
                        
   
static void
repairTypeFuncLoop(DumpableObject *typeobj, DumpableObject *funcobj)
{
  TypeInfo *typeInfo = (TypeInfo *)typeobj;

                                            
  removeObjectDependency(funcobj, typeobj->dumpId);

                                                        
  if (typeInfo->shellType)
  {
    addObjectDependency(funcobj, typeInfo->shellType->dobj.dumpId);

       
                                                                        
                                                                          
                               
       
    if (funcobj->dump)
    {
      typeInfo->shellType->dobj.dump = funcobj->dump | DUMP_COMPONENT_DEFINITION;
    }
  }
}

   
                                                                        
                                                                           
                                                                           
                                                                      
                                      
   
static void
repairViewRuleLoop(DumpableObject *viewobj, DumpableObject *ruleobj)
{
                                        
  removeObjectDependency(ruleobj, viewobj->dumpId);
                                                                        
}

   
                                                                           
                                                            
   
                                                                           
                                                                           
                                                                           
                            
   
                                                                    
   
static void
repairViewRuleMultiLoop(DumpableObject *viewobj, DumpableObject *ruleobj)
{
  TableInfo *viewinfo = (TableInfo *)viewobj;
  RuleInfo *ruleinfo = (RuleInfo *)ruleobj;

                                        
  removeObjectDependency(viewobj, ruleobj->dumpId);
                                                       
  viewinfo->dummy_view = true;
                                         
  ruleinfo->separate = true;
                                          
  addObjectDependency(ruleobj, viewobj->dumpId);
                                                       
  addObjectDependency(ruleobj, postDataBoundId);
}

   
                                                                           
                                                                      
                                                                               
                                                                             
                                                          
   
                                                                      
                                                                             
                                                                           
                                                                           
                                                                            
                                                                         
                                                     
   
static void
repairMatViewBoundaryMultiLoop(DumpableObject *boundaryobj, DumpableObject *nextobj)
{
                                                               
  removeObjectDependency(boundaryobj, nextobj->dumpId);
                                                                        
  if (nextobj->objType == DO_TABLE)
  {
    TableInfo *nextinfo = (TableInfo *)nextobj;

    if (nextinfo->relkind == RELKIND_MATVIEW)
    {
      nextinfo->postponed_def = true;
    }
  }
}

   
                                                                         
                                                                            
                                                                           
                                                                         
   
static void
repairTableConstraintLoop(DumpableObject *tableobj, DumpableObject *constraintobj)
{
                                               
  removeObjectDependency(constraintobj, tableobj->dumpId);
}

   
                                                                           
                                                              
   
                                                                           
                                                                      
                                                                            
                                                       
   
static void
repairTableConstraintMultiLoop(DumpableObject *tableobj, DumpableObject *constraintobj)
{
                                               
  removeObjectDependency(tableobj, constraintobj->dumpId);
                                               
  ((ConstraintInfo *)constraintobj)->separate = true;
                                                 
  addObjectDependency(constraintobj, tableobj->dumpId);
                                                             
  addObjectDependency(constraintobj, postDataBoundId);
}

   
                                                                      
   
static void
repairTableAttrDefLoop(DumpableObject *tableobj, DumpableObject *attrdefobj)
{
                                            
  removeObjectDependency(attrdefobj, tableobj->dumpId);
}

static void
repairTableAttrDefMultiLoop(DumpableObject *tableobj, DumpableObject *attrdefobj)
{
                                            
  removeObjectDependency(tableobj, attrdefobj->dumpId);
                                            
  ((AttrDefInfo *)attrdefobj)->separate = true;
                                              
  addObjectDependency(attrdefobj, tableobj->dumpId);
}

   
                                                                   
   
static void
repairDomainConstraintLoop(DumpableObject *domainobj, DumpableObject *constraintobj)
{
                                                
  removeObjectDependency(constraintobj, domainobj->dumpId);
}

static void
repairDomainConstraintMultiLoop(DumpableObject *domainobj, DumpableObject *constraintobj)
{
                                                
  removeObjectDependency(domainobj, constraintobj->dumpId);
                                               
  ((ConstraintInfo *)constraintobj)->separate = true;
                                                  
  addObjectDependency(constraintobj, domainobj->dumpId);
                                                             
  addObjectDependency(constraintobj, postDataBoundId);
}

static void
repairIndexLoop(DumpableObject *partedindex, DumpableObject *partindex)
{
  removeObjectDependency(partedindex, partindex->dumpId);
}

   
                                            
   
                                                                         
                                                                   
                           
   
static void
repairDependencyLoop(DumpableObject **loop, int nLoop)
{
  int i, j;

                                                             
  if (nLoop == 2 && loop[0]->objType == DO_TYPE && loop[1]->objType == DO_FUNC)
  {
    repairTypeFuncLoop(loop[0], loop[1]);
    return;
  }
  if (nLoop == 2 && loop[1]->objType == DO_TYPE && loop[0]->objType == DO_FUNC)
  {
    repairTypeFuncLoop(loop[1], loop[0]);
    return;
  }

                                                       
  if (nLoop == 2 && loop[0]->objType == DO_TABLE && loop[1]->objType == DO_RULE && (((TableInfo *)loop[0])->relkind == RELKIND_VIEW || ((TableInfo *)loop[0])->relkind == RELKIND_MATVIEW) && ((RuleInfo *)loop[1])->ev_type == '1' && ((RuleInfo *)loop[1])->is_instead && ((RuleInfo *)loop[1])->ruletable == (TableInfo *)loop[0])
  {
    repairViewRuleLoop(loop[0], loop[1]);
    return;
  }
  if (nLoop == 2 && loop[1]->objType == DO_TABLE && loop[0]->objType == DO_RULE && (((TableInfo *)loop[1])->relkind == RELKIND_VIEW || ((TableInfo *)loop[1])->relkind == RELKIND_MATVIEW) && ((RuleInfo *)loop[0])->ev_type == '1' && ((RuleInfo *)loop[0])->is_instead && ((RuleInfo *)loop[0])->ruletable == (TableInfo *)loop[1])
  {
    repairViewRuleLoop(loop[1], loop[0]);
    return;
  }

                                                                         
  if (nLoop > 2)
  {
    for (i = 0; i < nLoop; i++)
    {
      if (loop[i]->objType == DO_TABLE && ((TableInfo *)loop[i])->relkind == RELKIND_VIEW)
      {
        for (j = 0; j < nLoop; j++)
        {
          if (loop[j]->objType == DO_RULE && ((RuleInfo *)loop[j])->ev_type == '1' && ((RuleInfo *)loop[j])->is_instead && ((RuleInfo *)loop[j])->ruletable == (TableInfo *)loop[i])
          {
            repairViewRuleMultiLoop(loop[i], loop[j]);
            return;
          }
        }
      }
    }
  }

                                                         
  if (nLoop > 2)
  {
    for (i = 0; i < nLoop; i++)
    {
      if (loop[i]->objType == DO_TABLE && ((TableInfo *)loop[i])->relkind == RELKIND_MATVIEW)
      {
        for (j = 0; j < nLoop; j++)
        {
          if (loop[j]->objType == DO_PRE_DATA_BOUNDARY)
          {
            DumpableObject *nextobj;

            nextobj = (j < nLoop - 1) ? loop[j + 1] : loop[0];
            repairMatViewBoundaryMultiLoop(loop[j], nextobj);
            return;
          }
        }
      }
    }
  }

                                  
  if (nLoop == 2 && loop[0]->objType == DO_TABLE && loop[1]->objType == DO_CONSTRAINT && ((ConstraintInfo *)loop[1])->contype == 'c' && ((ConstraintInfo *)loop[1])->contable == (TableInfo *)loop[0])
  {
    repairTableConstraintLoop(loop[0], loop[1]);
    return;
  }
  if (nLoop == 2 && loop[1]->objType == DO_TABLE && loop[0]->objType == DO_CONSTRAINT && ((ConstraintInfo *)loop[0])->contype == 'c' && ((ConstraintInfo *)loop[0])->contable == (TableInfo *)loop[1])
  {
    repairTableConstraintLoop(loop[1], loop[0]);
    return;
  }

                                                          
  if (nLoop > 2)
  {
    for (i = 0; i < nLoop; i++)
    {
      if (loop[i]->objType == DO_TABLE)
      {
        for (j = 0; j < nLoop; j++)
        {
          if (loop[j]->objType == DO_CONSTRAINT && ((ConstraintInfo *)loop[j])->contype == 'c' && ((ConstraintInfo *)loop[j])->contable == (TableInfo *)loop[i])
          {
            repairTableConstraintMultiLoop(loop[i], loop[j]);
            return;
          }
        }
      }
    }
  }

                                   
  if (nLoop == 2 && loop[0]->objType == DO_TABLE && loop[1]->objType == DO_ATTRDEF && ((AttrDefInfo *)loop[1])->adtable == (TableInfo *)loop[0])
  {
    repairTableAttrDefLoop(loop[0], loop[1]);
    return;
  }
  if (nLoop == 2 && loop[1]->objType == DO_TABLE && loop[0]->objType == DO_ATTRDEF && ((AttrDefInfo *)loop[0])->adtable == (TableInfo *)loop[1])
  {
    repairTableAttrDefLoop(loop[1], loop[0]);
    return;
  }

                                                                       
  if (nLoop == 2 && loop[0]->objType == DO_INDEX && loop[1]->objType == DO_INDEX)
  {
    if (((IndxInfo *)loop[0])->parentidx == loop[1]->catId.oid)
    {
      repairIndexLoop(loop[0], loop[1]);
      return;
    }
    else if (((IndxInfo *)loop[1])->parentidx == loop[0]->catId.oid)
    {
      repairIndexLoop(loop[1], loop[0]);
      return;
    }
  }

                                                           
  if (nLoop > 2)
  {
    for (i = 0; i < nLoop; i++)
    {
      if (loop[i]->objType == DO_TABLE)
      {
        for (j = 0; j < nLoop; j++)
        {
          if (loop[j]->objType == DO_ATTRDEF && ((AttrDefInfo *)loop[j])->adtable == (TableInfo *)loop[i])
          {
            repairTableAttrDefMultiLoop(loop[i], loop[j]);
            return;
          }
        }
      }
    }
  }

                                   
  if (nLoop == 2 && loop[0]->objType == DO_TYPE && loop[1]->objType == DO_CONSTRAINT && ((ConstraintInfo *)loop[1])->contype == 'c' && ((ConstraintInfo *)loop[1])->condomain == (TypeInfo *)loop[0])
  {
    repairDomainConstraintLoop(loop[0], loop[1]);
    return;
  }
  if (nLoop == 2 && loop[1]->objType == DO_TYPE && loop[0]->objType == DO_CONSTRAINT && ((ConstraintInfo *)loop[0])->contype == 'c' && ((ConstraintInfo *)loop[0])->condomain == (TypeInfo *)loop[1])
  {
    repairDomainConstraintLoop(loop[1], loop[0]);
    return;
  }

                                                           
  if (nLoop > 2)
  {
    for (i = 0; i < nLoop; i++)
    {
      if (loop[i]->objType == DO_TYPE)
      {
        for (j = 0; j < nLoop; j++)
        {
          if (loop[j]->objType == DO_CONSTRAINT && ((ConstraintInfo *)loop[j])->contype == 'c' && ((ConstraintInfo *)loop[j])->condomain == (TypeInfo *)loop[i])
          {
            repairDomainConstraintMultiLoop(loop[i], loop[j]);
            return;
          }
        }
      }
    }
  }

     
                                                   
     
                                                                           
                                                                           
                                                                            
                                                                        
                                                  
     
  if (nLoop == 1)
  {
    if (loop[0]->objType == DO_TABLE)
    {
      removeObjectDependency(loop[0], loop[0]->dumpId);
      return;
    }
  }

     
                                                                     
                                                                           
                                                                             
     
  for (i = 0; i < nLoop; i++)
  {
    if (loop[i]->objType != DO_TABLE_DATA)
    {
      break;
    }
  }
  if (i >= nLoop)
  {
    pg_log_warning(ngettext("there are circular foreign-key constraints on this table:", "there are circular foreign-key constraints among these tables:", nLoop));
    for (i = 0; i < nLoop; i++)
    {
      pg_log_generic(PG_LOG_INFO, "  %s", loop[i]->name);
    }
    pg_log_generic(PG_LOG_INFO, "You might not be able to restore the dump without using --disable-triggers or temporarily dropping the constraints.");
    pg_log_generic(PG_LOG_INFO, "Consider using a full dump instead of a --data-only dump to avoid this problem.");
    if (nLoop > 1)
    {
      removeObjectDependency(loop[0], loop[1]->dumpId);
    }
    else                                
    {
      removeObjectDependency(loop[0], loop[0]->dumpId);
    }
    return;
  }

     
                                                                             
                                 
     
  pg_log_warning("could not resolve dependency loop among these items:");
  for (i = 0; i < nLoop; i++)
  {
    char buf[1024];

    describeDumpableObject(loop[i], buf, sizeof(buf));
    pg_log_generic(PG_LOG_INFO, "  %s", buf);
  }

  if (nLoop > 1)
  {
    removeObjectDependency(loop[0], loop[1]->dumpId);
  }
  else                                
  {
    removeObjectDependency(loop[0], loop[0]->dumpId);
  }
}

   
                                                  
   
                                             
   
static void
describeDumpableObject(DumpableObject *obj, char *buf, int bufsize)
{
  switch (obj->objType)
  {
  case DO_NAMESPACE:
    snprintf(buf, bufsize, "SCHEMA %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_EXTENSION:
    snprintf(buf, bufsize, "EXTENSION %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_TYPE:
    snprintf(buf, bufsize, "TYPE %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_SHELL_TYPE:
    snprintf(buf, bufsize, "SHELL TYPE %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_FUNC:
    snprintf(buf, bufsize, "FUNCTION %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_AGG:
    snprintf(buf, bufsize, "AGGREGATE %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_OPERATOR:
    snprintf(buf, bufsize, "OPERATOR %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_ACCESS_METHOD:
    snprintf(buf, bufsize, "ACCESS METHOD %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_OPCLASS:
    snprintf(buf, bufsize, "OPERATOR CLASS %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_OPFAMILY:
    snprintf(buf, bufsize, "OPERATOR FAMILY %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_COLLATION:
    snprintf(buf, bufsize, "COLLATION %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_CONVERSION:
    snprintf(buf, bufsize, "CONVERSION %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_TABLE:
    snprintf(buf, bufsize, "TABLE %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_ATTRDEF:
    snprintf(buf, bufsize, "ATTRDEF %s.%s  (ID %d OID %u)", ((AttrDefInfo *)obj)->adtable->dobj.name, ((AttrDefInfo *)obj)->adtable->attnames[((AttrDefInfo *)obj)->adnum - 1], obj->dumpId, obj->catId.oid);
    return;
  case DO_INDEX:
    snprintf(buf, bufsize, "INDEX %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_INDEX_ATTACH:
    snprintf(buf, bufsize, "INDEX ATTACH %s  (ID %d)", obj->name, obj->dumpId);
    return;
  case DO_STATSEXT:
    snprintf(buf, bufsize, "STATISTICS %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_REFRESH_MATVIEW:
    snprintf(buf, bufsize, "REFRESH MATERIALIZED VIEW %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_RULE:
    snprintf(buf, bufsize, "RULE %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_TRIGGER:
    snprintf(buf, bufsize, "TRIGGER %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_EVENT_TRIGGER:
    snprintf(buf, bufsize, "EVENT TRIGGER %s (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_CONSTRAINT:
    snprintf(buf, bufsize, "CONSTRAINT %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_FK_CONSTRAINT:
    snprintf(buf, bufsize, "FK CONSTRAINT %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_PROCLANG:
    snprintf(buf, bufsize, "PROCEDURAL LANGUAGE %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_CAST:
    snprintf(buf, bufsize, "CAST %u to %u  (ID %d OID %u)", ((CastInfo *)obj)->castsource, ((CastInfo *)obj)->casttarget, obj->dumpId, obj->catId.oid);
    return;
  case DO_TRANSFORM:
    snprintf(buf, bufsize, "TRANSFORM %u lang %u  (ID %d OID %u)", ((TransformInfo *)obj)->trftype, ((TransformInfo *)obj)->trflang, obj->dumpId, obj->catId.oid);
    return;
  case DO_TABLE_DATA:
    snprintf(buf, bufsize, "TABLE DATA %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_SEQUENCE_SET:
    snprintf(buf, bufsize, "SEQUENCE SET %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_DUMMY_TYPE:
    snprintf(buf, bufsize, "DUMMY TYPE %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_TSPARSER:
    snprintf(buf, bufsize, "TEXT SEARCH PARSER %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_TSDICT:
    snprintf(buf, bufsize, "TEXT SEARCH DICTIONARY %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_TSTEMPLATE:
    snprintf(buf, bufsize, "TEXT SEARCH TEMPLATE %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_TSCONFIG:
    snprintf(buf, bufsize, "TEXT SEARCH CONFIGURATION %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_FDW:
    snprintf(buf, bufsize, "FOREIGN DATA WRAPPER %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_FOREIGN_SERVER:
    snprintf(buf, bufsize, "FOREIGN SERVER %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_DEFAULT_ACL:
    snprintf(buf, bufsize, "DEFAULT ACL %s  (ID %d OID %u)", obj->name, obj->dumpId, obj->catId.oid);
    return;
  case DO_BLOB:
    snprintf(buf, bufsize, "BLOB  (ID %d OID %u)", obj->dumpId, obj->catId.oid);
    return;
  case DO_BLOB_DATA:
    snprintf(buf, bufsize, "BLOB DATA  (ID %d)", obj->dumpId);
    return;
  case DO_POLICY:
    snprintf(buf, bufsize, "POLICY (ID %d OID %u)", obj->dumpId, obj->catId.oid);
    return;
  case DO_PUBLICATION:
    snprintf(buf, bufsize, "PUBLICATION (ID %d OID %u)", obj->dumpId, obj->catId.oid);
    return;
  case DO_PUBLICATION_REL:
    snprintf(buf, bufsize, "PUBLICATION TABLE (ID %d OID %u)", obj->dumpId, obj->catId.oid);
    return;
  case DO_SUBSCRIPTION:
    snprintf(buf, bufsize, "SUBSCRIPTION (ID %d OID %u)", obj->dumpId, obj->catId.oid);
    return;
  case DO_PRE_DATA_BOUNDARY:
    snprintf(buf, bufsize, "PRE-DATA BOUNDARY  (ID %d)", obj->dumpId);
    return;
  case DO_POST_DATA_BOUNDARY:
    snprintf(buf, bufsize, "POST-DATA BOUNDARY  (ID %d)", obj->dumpId);
    return;
  }
                          
  snprintf(buf, bufsize, "object type %d  (ID %d OID %u)", (int)obj->objType, obj->dumpId, obj->catId.oid);
}
