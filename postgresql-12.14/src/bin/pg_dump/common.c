                                                                            
   
            
                                                                
                                          
   
                                                                         
                                                                        
   
   
                  
                              
   
                                                                            
   
#include "postgres_fe.h"

#include "pg_backup_archiver.h"
#include "pg_backup_utils.h"
#include "pg_dump.h"

#include <ctype.h>

#include "catalog/pg_class_d.h"
#include "fe_utils/string_utils.h"

   
                                                  
   
static DumpableObject **dumpIdMap = NULL;
static int allocedDumpIds = 0;
static DumpId lastDumpId = 0;                               

   
                                                     
   
static bool catalogIdMapValid = false;
static DumpableObject **catalogIdMap = NULL;
static int numCatalogIds = 0;

   
                                                                              
                                                                               
                                                                          
                                                                              
                                                                              
                                                                   
   
static DumpableObject **tblinfoindex;
static DumpableObject **typinfoindex;
static DumpableObject **funinfoindex;
static DumpableObject **oprinfoindex;
static DumpableObject **collinfoindex;
static DumpableObject **nspinfoindex;
static DumpableObject **extinfoindex;
static DumpableObject **pubinfoindex;
static int numTables;
static int numTypes;
static int numFuncs;
static int numOperators;
static int numCollations;
static int numNamespaces;
static int numExtensions;
static int numPublications;

                                                                       
static ExtensionMemberId *extmembers;
static int numextmembers;

static void
flagInhTables(Archive *fout, TableInfo *tbinfo, int numTables, InhInfo *inhinfo, int numInherits);
static void
flagInhIndexes(Archive *fout, TableInfo *tblinfo, int numTables);
static void
flagInhAttrs(DumpOptions *dopt, TableInfo *tblinfo, int numTables);
static DumpableObject **
buildIndexArray(void *objArray, int numObjs, Size objSize);
static int
DOCatalogIdCompare(const void *p1, const void *p2);
static int
ExtensionMemberIdCompare(const void *p1, const void *p2);
static void
findParentsByOid(TableInfo *self, InhInfo *inhinfo, int numInherits);
static int
strInArray(const char *pattern, char **arr, int arr_size);
static IndxInfo *
findIndexByOid(Oid oid, DumpableObject **idxinfoindex, int numIndexes);

   
                 
                                                                
   
TableInfo *
getSchemaData(Archive *fout, int *numTablesPtr)
{
  TableInfo *tblinfo;
  TypeInfo *typinfo;
  FuncInfo *funinfo;
  OprInfo *oprinfo;
  CollInfo *collinfo;
  NamespaceInfo *nspinfo;
  ExtensionInfo *extinfo;
  PublicationInfo *pubinfo;
  InhInfo *inhinfo;
  int numAggregates;
  int numInherits;
  int numRules;
  int numProcLangs;
  int numCasts;
  int numTransforms;
  int numAccessMethods;
  int numOpclasses;
  int numOpfamilies;
  int numConversions;
  int numTSParsers;
  int numTSTemplates;
  int numTSDicts;
  int numTSConfigs;
  int numForeignDataWrappers;
  int numForeignServers;
  int numDefaultACLs;
  int numEventTriggers;

     
                                                                          
                                                                         
                                             
     
  pg_log_info("reading extensions");
  extinfo = getExtensions(fout, &numExtensions);
  extinfoindex = buildIndexArray(extinfo, numExtensions, sizeof(ExtensionInfo));

  pg_log_info("identifying extension members");
  getExtensionMembership(fout, extinfo, numExtensions);

  pg_log_info("reading schemas");
  nspinfo = getNamespaces(fout, &numNamespaces);
  nspinfoindex = buildIndexArray(nspinfo, numNamespaces, sizeof(NamespaceInfo));

     
                                                                         
                                                                            
                                                                       
                                                             
     
  pg_log_info("reading user-defined tables");
  tblinfo = getTables(fout, &numTables);
  tblinfoindex = buildIndexArray(tblinfo, numTables, sizeof(TableInfo));

                                              
  getOwnedSeqs(fout, tblinfo, numTables);

  pg_log_info("reading user-defined functions");
  funinfo = getFuncs(fout, &numFuncs);
  funinfoindex = buildIndexArray(funinfo, numFuncs, sizeof(FuncInfo));

                                                 
  pg_log_info("reading user-defined types");
  typinfo = getTypes(fout, &numTypes);
  typinfoindex = buildIndexArray(typinfo, numTypes, sizeof(TypeInfo));

                                        
  pg_log_info("reading procedural languages");
  getProcLangs(fout, &numProcLangs);

  pg_log_info("reading user-defined aggregate functions");
  getAggregates(fout, &numAggregates);

  pg_log_info("reading user-defined operators");
  oprinfo = getOperators(fout, &numOperators);
  oprinfoindex = buildIndexArray(oprinfo, numOperators, sizeof(OprInfo));

  pg_log_info("reading user-defined access methods");
  getAccessMethods(fout, &numAccessMethods);

  pg_log_info("reading user-defined operator classes");
  getOpclasses(fout, &numOpclasses);

  pg_log_info("reading user-defined operator families");
  getOpfamilies(fout, &numOpfamilies);

  pg_log_info("reading user-defined text search parsers");
  getTSParsers(fout, &numTSParsers);

  pg_log_info("reading user-defined text search templates");
  getTSTemplates(fout, &numTSTemplates);

  pg_log_info("reading user-defined text search dictionaries");
  getTSDictionaries(fout, &numTSDicts);

  pg_log_info("reading user-defined text search configurations");
  getTSConfigurations(fout, &numTSConfigs);

  pg_log_info("reading user-defined foreign-data wrappers");
  getForeignDataWrappers(fout, &numForeignDataWrappers);

  pg_log_info("reading user-defined foreign servers");
  getForeignServers(fout, &numForeignServers);

  pg_log_info("reading default privileges");
  getDefaultACLs(fout, &numDefaultACLs);

  pg_log_info("reading user-defined collations");
  collinfo = getCollations(fout, &numCollations);
  collinfoindex = buildIndexArray(collinfo, numCollations, sizeof(CollInfo));

  pg_log_info("reading user-defined conversions");
  getConversions(fout, &numConversions);

  pg_log_info("reading type casts");
  getCasts(fout, &numCasts);

  pg_log_info("reading transforms");
  getTransforms(fout, &numTransforms);

  pg_log_info("reading table inheritance information");
  inhinfo = getInherits(fout, &numInherits);

  pg_log_info("reading event triggers");
  getEventTriggers(fout, &numEventTriggers);

                                                                     
  pg_log_info("finding extension tables");
  processExtensionTables(fout, extinfo, numExtensions);

                                                                         
  pg_log_info("finding inheritance relationships");
  flagInhTables(fout, tblinfo, numTables, inhinfo, numInherits);

  pg_log_info("reading column info for interesting tables");
  getTableAttrs(fout, tblinfo, numTables);

  pg_log_info("flagging inherited columns in subtables");
  flagInhAttrs(fout->dopt, tblinfo, numTables);

  pg_log_info("reading indexes");
  getIndexes(fout, tblinfo, numTables);

  pg_log_info("flagging indexes in partitioned tables");
  flagInhIndexes(fout, tblinfo, numTables);

  pg_log_info("reading extended statistics");
  getExtendedStatistics(fout);

  pg_log_info("reading constraints");
  getConstraints(fout, tblinfo, numTables);

  pg_log_info("reading triggers");
  getTriggers(fout, tblinfo, numTables);

  pg_log_info("reading rewrite rules");
  getRules(fout, &numRules);

  pg_log_info("reading policies");
  getPolicies(fout, tblinfo, numTables);

  pg_log_info("reading publications");
  pubinfo = getPublications(fout, &numPublications);
  pubinfoindex = buildIndexArray(pubinfo, numPublications, sizeof(PublicationInfo));

  pg_log_info("reading publication membership");
  getPublicationTables(fout, tblinfo, numTables);

  pg_log_info("reading subscriptions");
  getSubscriptions(fout);

  *numTablesPtr = numTables;
  return tblinfo;
}

                   
                                                                             
                                                     
   
                                                                      
                                                                       
                      
   
                    
   
static void
flagInhTables(Archive *fout, TableInfo *tblinfo, int numTables, InhInfo *inhinfo, int numInherits)
{
  DumpOptions *dopt = fout->dopt;
  int i, j;

  for (i = 0; i < numTables; i++)
  {
    bool find_parents = true;
    bool mark_parents = true;

                                       
    if (tblinfo[i].relkind == RELKIND_SEQUENCE || tblinfo[i].relkind == RELKIND_VIEW || tblinfo[i].relkind == RELKIND_MATVIEW)
    {
      continue;
    }

       
                                                                           
                                                                          
                                                                           
                                                                         
                                                                          
       
    if (!tblinfo[i].dobj.dump)
    {
      mark_parents = false;

      if (!dopt->load_via_partition_root || !tblinfo[i].ispartition)
      {
        find_parents = false;
      }
    }

                                                          
    if (find_parents)
    {
      findParentsByOid(&tblinfo[i], inhinfo, numInherits);
    }

       
                                                                        
                   
       
    if (mark_parents)
    {
      int numParents = tblinfo[i].numParents;
      TableInfo **parents = tblinfo[i].parents;

      for (j = 0; j < numParents; j++)
      {
        parents[j]->interesting = true;
      }
    }
  }
}

   
                    
                                                                    
                                  
   
static void
flagInhIndexes(Archive *fout, TableInfo tblinfo[], int numTables)
{
  int i, j, k;
  DumpableObject ***parentIndexArray;

  parentIndexArray = (DumpableObject ***)pg_malloc0(getMaxDumpId() * sizeof(DumpableObject **));

  for (i = 0; i < numTables; i++)
  {
    TableInfo *parenttbl;
    IndexAttachInfo *attachinfo;

    if (!tblinfo[i].ispartition || tblinfo[i].numParents == 0)
    {
      continue;
    }

    Assert(tblinfo[i].numParents == 1);
    parenttbl = tblinfo[i].parents[0];

       
                                                                         
                                                                         
                                                                          
                                                                         
                            
       
    if (parentIndexArray[parenttbl->dobj.dumpId] == NULL)
    {
      parentIndexArray[parenttbl->dobj.dumpId] = buildIndexArray(parenttbl->indexes, parenttbl->numIndexes, sizeof(IndxInfo));
    }

    attachinfo = (IndexAttachInfo *)pg_malloc0(tblinfo[i].numIndexes * sizeof(IndexAttachInfo));
    for (j = 0, k = 0; j < tblinfo[i].numIndexes; j++)
    {
      IndxInfo *index = &(tblinfo[i].indexes[j]);
      IndxInfo *parentidx;

      if (index->parentidx == 0)
      {
        continue;
      }

      parentidx = findIndexByOid(index->parentidx, parentIndexArray[parenttbl->dobj.dumpId], parenttbl->numIndexes);
      if (parentidx == NULL)
      {
        continue;
      }

      attachinfo[k].dobj.objType = DO_INDEX_ATTACH;
      attachinfo[k].dobj.catId.tableoid = 0;
      attachinfo[k].dobj.catId.oid = 0;
      AssignDumpId(&attachinfo[k].dobj);
      attachinfo[k].dobj.name = pg_strdup(index->dobj.name);
      attachinfo[k].dobj.namespace = index->indextable->dobj.namespace;
      attachinfo[k].parentIdx = parentidx;
      attachinfo[k].partitionIdx = index;

         
                                                                 
                                                                    
         
                                                                         
                                                                   
                                                                       
              
         
                                                                      
                                                                    
                                                                       
                                                                
                                     
         
      addObjectDependency(&attachinfo[k].dobj, index->dobj.dumpId);
      addObjectDependency(&attachinfo[k].dobj, parentidx->dobj.dumpId);
      addObjectDependency(&attachinfo[k].dobj, index->indextable->dobj.dumpId);
      addObjectDependency(&attachinfo[k].dobj, parentidx->indextable->dobj.dumpId);

                                                                    
      simple_ptr_list_append(&parentidx->partattaches, &attachinfo[k].dobj);

      k++;
    }
  }

  for (i = 0; i < numTables; i++)
  {
    if (parentIndexArray[i])
    {
      pg_free(parentIndexArray[i]);
    }
  }
  pg_free(parentIndexArray);
}

                  
                                                                      
   
                               
   
                                                                            
                                                       
   
                                                                             
                                                                               
                                                                            
                                                                
   
                                                                               
                                                                              
                                                                              
                                                                          
                                                                              
                                                                              
                                                                           
                                                     
   
                    
   
static void
flagInhAttrs(DumpOptions *dopt, TableInfo *tblinfo, int numTables)
{
  int i, j, k;

  for (i = 0; i < numTables; i++)
  {
    TableInfo *tbinfo = &(tblinfo[i]);
    int numParents;
    TableInfo **parents;

                                       
    if (tbinfo->relkind == RELKIND_SEQUENCE || tbinfo->relkind == RELKIND_VIEW || tbinfo->relkind == RELKIND_MATVIEW)
    {
      continue;
    }

                                                                       
    if (!tbinfo->dobj.dump)
    {
      continue;
    }

    numParents = tbinfo->numParents;
    parents = tbinfo->parents;

    if (numParents == 0)
    {
      continue;                                      
    }

                                                                        
    for (j = 0; j < tbinfo->numatts; j++)
    {
      bool foundNotNull;                                      
      bool foundDefault;                                    
      bool foundGenerated;                                    

                                                 
      if (tbinfo->attisdropped[j])
      {
        continue;
      }

      foundNotNull = false;
      foundDefault = false;
      foundGenerated = false;
      for (k = 0; k < numParents; k++)
      {
        TableInfo *parent = parents[k];
        int inhAttrInd;

        inhAttrInd = strInArray(tbinfo->attnames[j], parent->attnames, parent->numatts);
        if (inhAttrInd >= 0)
        {
          foundNotNull |= parent->notnull[inhAttrInd];
          foundDefault |= (parent->attrdefs[inhAttrInd] != NULL && !parent->attgenerated[inhAttrInd]);
          foundGenerated |= parent->attgenerated[inhAttrInd];
        }
      }

                                                   
      tbinfo->inhNotNull[j] = foundNotNull;

                                                          
      if (foundDefault && tbinfo->attrdefs[j] == NULL)
      {
        AttrDefInfo *attrDef;

        attrDef = (AttrDefInfo *)pg_malloc(sizeof(AttrDefInfo));
        attrDef->dobj.objType = DO_ATTRDEF;
        attrDef->dobj.catId.tableoid = 0;
        attrDef->dobj.catId.oid = 0;
        AssignDumpId(&attrDef->dobj);
        attrDef->dobj.name = pg_strdup(tbinfo->dobj.name);
        attrDef->dobj.namespace = tbinfo->dobj.namespace;
        attrDef->dobj.dump = tbinfo->dobj.dump;

        attrDef->adtable = tbinfo;
        attrDef->adnum = j + 1;
        attrDef->adef_expr = pg_strdup("NULL");

                                               
        if (shouldPrintColumn(dopt, tbinfo, j))
        {
          attrDef->separate = false;
                                                                   
        }
        else
        {
                                                                   
          attrDef->separate = true;
                                                   
          addObjectDependency(&attrDef->dobj, tbinfo->dobj.dumpId);
        }

        tbinfo->attrdefs[j] = attrDef;
      }

                                                   
      if (foundGenerated && !tbinfo->ispartition && !dopt->binary_upgrade)
      {
        tbinfo->attrdefs[j] = NULL;
      }
    }
  }
}

   
                
                                                             
                                                
   
                                                               
                                                                 
   
void
AssignDumpId(DumpableObject *dobj)
{
  dobj->dumpId = ++lastDumpId;
  dobj->name = NULL;                                      
  dobj->namespace = NULL;                                
  dobj->dump = DUMP_COMPONENT_ALL;                         
  dobj->ext_member = false;                                
  dobj->depends_on_ext = false;                            
  dobj->dependencies = NULL;
  dobj->nDeps = 0;
  dobj->allocDeps = 0;

  while (dobj->dumpId >= allocedDumpIds)
  {
    int newAlloc;

    if (allocedDumpIds <= 0)
    {
      newAlloc = 256;
      dumpIdMap = (DumpableObject **)pg_malloc(newAlloc * sizeof(DumpableObject *));
    }
    else
    {
      newAlloc = allocedDumpIds * 2;
      dumpIdMap = (DumpableObject **)pg_realloc(dumpIdMap, newAlloc * sizeof(DumpableObject *));
    }
    memset(dumpIdMap + allocedDumpIds, 0, (newAlloc - allocedDumpIds) * sizeof(DumpableObject *));
    allocedDumpIds = newAlloc;
  }
  dumpIdMap[dobj->dumpId] = dobj;

                                                           
  catalogIdMapValid = false;
}

   
                                                        
   
                                                                          
                                     
   
DumpId
createDumpId(void)
{
  return ++lastDumpId;
}

   
                                             
   
DumpId
getMaxDumpId(void)
{
  return lastDumpId;
}

   
                                    
   
                               
   
DumpableObject *
findObjectByDumpId(DumpId dumpId)
{
  if (dumpId <= 0 || dumpId >= allocedDumpIds)
  {
    return NULL;                    
  }
  return dumpIdMap[dumpId];
}

   
                                       
   
                               
   
                                                                      
                                                                               
                                                                         
                                                                         
   
DumpableObject *
findObjectByCatalogId(CatalogId catalogId)
{
  DumpableObject **low;
  DumpableObject **high;

  if (!catalogIdMapValid)
  {
    if (catalogIdMap)
    {
      free(catalogIdMap);
    }
    getDumpableObjects(&catalogIdMap, &numCatalogIds);
    if (numCatalogIds > 1)
    {
      qsort((void *)catalogIdMap, numCatalogIds, sizeof(DumpableObject *), DOCatalogIdCompare);
    }
    catalogIdMapValid = true;
  }

     
                                                                      
                                                                         
                                                    
     
  if (numCatalogIds <= 0)
  {
    return NULL;
  }
  low = catalogIdMap;
  high = catalogIdMap + (numCatalogIds - 1);
  while (low <= high)
  {
    DumpableObject **middle;
    int difference;

    middle = low + (high - low) / 2;
                                                         
    difference = oidcmp((*middle)->catId.oid, catalogId.oid);
    if (difference == 0)
    {
      difference = oidcmp((*middle)->catId.tableoid, catalogId.tableoid);
    }
    if (difference == 0)
    {
      return *middle;
    }
    else if (difference < 0)
    {
      low = middle + 1;
    }
    else
    {
      high = middle - 1;
    }
  }
  return NULL;
}

   
                                                                             
   
                                
   
static DumpableObject *
findObjectByOid(Oid oid, DumpableObject **indexArray, int numObjs)
{
  DumpableObject **low;
  DumpableObject **high;

     
                                                                            
                                                                  
     
                                                                      
                                                                         
                                                    
     
  if (numObjs <= 0)
  {
    return NULL;
  }
  low = indexArray;
  high = indexArray + (numObjs - 1);
  while (low <= high)
  {
    DumpableObject **middle;
    int difference;

    middle = low + (high - low) / 2;
    difference = oidcmp((*middle)->catId.oid, oid);
    if (difference == 0)
    {
      return *middle;
    }
    else if (difference < 0)
    {
      low = middle + 1;
    }
    else
    {
      high = middle - 1;
    }
  }
  return NULL;
}

   
                                                                  
   
static DumpableObject **
buildIndexArray(void *objArray, int numObjs, Size objSize)
{
  DumpableObject **ptrs;
  int i;

  ptrs = (DumpableObject **)pg_malloc(numObjs * sizeof(DumpableObject *));
  for (i = 0; i < numObjs; i++)
  {
    ptrs[i] = (DumpableObject *)((char *)objArray + i * objSize);
  }

                                                                        
  if (numObjs > 1)
  {
    qsort((void *)ptrs, numObjs, sizeof(DumpableObject *), DOCatalogIdCompare);
  }

  return ptrs;
}

   
                                                    
   
static int
DOCatalogIdCompare(const void *p1, const void *p2)
{
  const DumpableObject *obj1 = *(DumpableObject *const *)p1;
  const DumpableObject *obj2 = *(DumpableObject *const *)p2;
  int cmpval;

     
                                                                             
                                        
     
  cmpval = oidcmp(obj1->catId.oid, obj2->catId.oid);
  if (cmpval == 0)
  {
    cmpval = oidcmp(obj1->catId.tableoid, obj2->catId.tableoid);
  }
  return cmpval;
}

   
                                                            
   
                                                              
   
void
getDumpableObjects(DumpableObject ***objs, int *numObjs)
{
  int i, j;

  *objs = (DumpableObject **)pg_malloc(allocedDumpIds * sizeof(DumpableObject *));
  j = 0;
  for (i = 1; i < allocedDumpIds; i++)
  {
    if (dumpIdMap[i])
    {
      (*objs)[j++] = dumpIdMap[i];
    }
  }
  *numObjs = j;
}

   
                                             
   
                                                             
   
void
addObjectDependency(DumpableObject *dobj, DumpId refId)
{
  if (dobj->nDeps >= dobj->allocDeps)
  {
    if (dobj->allocDeps <= 0)
    {
      dobj->allocDeps = 16;
      dobj->dependencies = (DumpId *)pg_malloc(dobj->allocDeps * sizeof(DumpId));
    }
    else
    {
      dobj->allocDeps *= 2;
      dobj->dependencies = (DumpId *)pg_realloc(dobj->dependencies, dobj->allocDeps * sizeof(DumpId));
    }
  }
  dobj->dependencies[dobj->nDeps++] = refId;
}

   
                                                  
   
                                                
   
void
removeObjectDependency(DumpableObject *dobj, DumpId refId)
{
  int i;
  int j = 0;

  for (i = 0; i < dobj->nDeps; i++)
  {
    if (dobj->dependencies[i] != refId)
    {
      dobj->dependencies[j++] = dobj->dependencies[i];
    }
  }
  dobj->nDeps = j;
}

   
                  
                                                                  
                               
   
TableInfo *
findTableByOid(Oid oid)
{
  return (TableInfo *)findObjectByOid(oid, tblinfoindex, numTables);
}

   
                 
                                                                 
                               
   
TypeInfo *
findTypeByOid(Oid oid)
{
  return (TypeInfo *)findObjectByOid(oid, typinfoindex, numTypes);
}

   
                 
                                                                     
                               
   
FuncInfo *
findFuncByOid(Oid oid)
{
  return (FuncInfo *)findObjectByOid(oid, funinfoindex, numFuncs);
}

   
                
                                                                     
                               
   
OprInfo *
findOprByOid(Oid oid)
{
  return (OprInfo *)findObjectByOid(oid, oprinfoindex, numOperators);
}

   
                      
                                                                       
                               
   
CollInfo *
findCollationByOid(Oid oid)
{
  return (CollInfo *)findObjectByOid(oid, collinfoindex, numCollations);
}

   
                      
                                                                      
                               
   
NamespaceInfo *
findNamespaceByOid(Oid oid)
{
  return (NamespaceInfo *)findObjectByOid(oid, nspinfoindex, numNamespaces);
}

   
                      
                                                                      
                               
   
ExtensionInfo *
findExtensionByOid(Oid oid)
{
  return (ExtensionInfo *)findObjectByOid(oid, extinfoindex, numExtensions);
}

   
                        
                                                                        
                               
   
PublicationInfo *
findPublicationByOid(Oid oid)
{
  return (PublicationInfo *)findObjectByOid(oid, pubinfoindex, numPublications);
}

   
                  
                                                   
   
                                                                              
                                                                             
   
static IndxInfo *
findIndexByOid(Oid oid, DumpableObject **idxinfoindex, int numIndexes)
{
  return (IndxInfo *)findObjectByOid(oid, idxinfoindex, numIndexes);
}

   
                          
                                                                   
   
void
setExtensionMembership(ExtensionMemberId *extmems, int nextmems)
{
                                                     
  if (nextmems > 1)
  {
    qsort((void *)extmems, nextmems, sizeof(ExtensionMemberId), ExtensionMemberIdCompare);
  }
                
  extmembers = extmems;
  numextmembers = nextmems;
}

   
                       
                                                                       
   
ExtensionInfo *
findOwningExtension(CatalogId catalogId)
{
  ExtensionMemberId *low;
  ExtensionMemberId *high;

     
                                                                      
                                                                         
                                                    
     
  if (numextmembers <= 0)
  {
    return NULL;
  }
  low = extmembers;
  high = extmembers + (numextmembers - 1);
  while (low <= high)
  {
    ExtensionMemberId *middle;
    int difference;

    middle = low + (high - low) / 2;
                                                               
    difference = oidcmp(middle->catId.oid, catalogId.oid);
    if (difference == 0)
    {
      difference = oidcmp(middle->catId.tableoid, catalogId.tableoid);
    }
    if (difference == 0)
    {
      return middle->ext;
    }
    else if (difference < 0)
    {
      low = middle + 1;
    }
    else
    {
      high = middle - 1;
    }
  }
  return NULL;
}

   
                                           
   
static int
ExtensionMemberIdCompare(const void *p1, const void *p2)
{
  const ExtensionMemberId *obj1 = (const ExtensionMemberId *)p1;
  const ExtensionMemberId *obj2 = (const ExtensionMemberId *)p2;
  int cmpval;

     
                                                                             
                                        
     
  cmpval = oidcmp(obj1->catId.oid, obj2->catId.oid);
  if (cmpval == 0)
  {
    cmpval = oidcmp(obj1->catId.tableoid, obj2->catId.tableoid);
  }
  return cmpval;
}

   
                    
                                         
   
static void
findParentsByOid(TableInfo *self, InhInfo *inhinfo, int numInherits)
{
  Oid oid = self->dobj.catId.oid;
  int i, j;
  int numParents;

  numParents = 0;
  for (i = 0; i < numInherits; i++)
  {
    if (inhinfo[i].inhrelid == oid)
    {
      numParents++;
    }
  }

  self->numParents = numParents;

  if (numParents > 0)
  {
    self->parents = (TableInfo **)pg_malloc(sizeof(TableInfo *) * numParents);
    j = 0;
    for (i = 0; i < numInherits; i++)
    {
      if (inhinfo[i].inhrelid == oid)
      {
        TableInfo *parent;

        parent = findTableByOid(inhinfo[i].inhparent);
        if (parent == NULL)
        {
          pg_log_error("failed sanity check, parent OID %u of table \"%s\" (OID %u) not found", inhinfo[i].inhparent, self->dobj.name, oid);
          exit_nicely(1);
        }
        self->parents[j++] = parent;
      }
    }
  }
  else
  {
    self->parents = NULL;
  }
}

   
                 
                                                                          
   
                                                                    
                                                                        
                                                                             
   

void
parseOidArray(const char *str, Oid *array, int arraysize)
{
  int j, argNum;
  char temp[100];
  char s;

  argNum = 0;
  j = 0;
  for (;;)
  {
    s = *str++;
    if (s == ' ' || s == '\0')
    {
      if (j > 0)
      {
        if (argNum >= arraysize)
        {
          pg_log_error("could not parse numeric array \"%s\": too many numbers", str);
          exit_nicely(1);
        }
        temp[j] = '\0';
        array[argNum++] = atooid(temp);
        j = 0;
      }
      if (s == '\0')
      {
        break;
      }
    }
    else
    {
      if (!(isdigit((unsigned char)s) || s == '-') || j >= sizeof(temp) - 1)
      {
        pg_log_error("could not parse numeric array \"%s\": invalid character in number", str);
        exit_nicely(1);
      }
      temp[j++] = s;
    }
  }

  while (argNum < arraysize)
  {
    array[argNum++] = InvalidOid;
  }
}

   
               
                                                                            
                 
                                                                             
   

static int
strInArray(const char *pattern, char **arr, int arr_size)
{
  int i;

  for (i = 0; i < arr_size; i++)
  {
    if (strcmp(pattern, arr[i]) == 0)
    {
      return i;
    }
  }
  return -1;
}
