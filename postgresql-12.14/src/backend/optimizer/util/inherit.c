                                                                            
   
             
                                                              
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/partition.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "optimizer/appendinfo.h"
#include "optimizer/inherit.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "optimizer/restrictinfo.h"
#include "parser/parsetree.h"
#include "partitioning/partdesc.h"
#include "partitioning/partprune.h"
#include "utils/rel.h"

                                                                  
#define make_restrictinfo(a, b, c, d, e, f, g, h, i) make_restrictinfo_new(a, b, c, d, e, f, g, h, i)

static void
expand_partitioned_rtentry(PlannerInfo *root, RelOptInfo *relinfo, RangeTblEntry *parentrte, Index parentRTindex, Relation parentrel, PlanRowMark *top_parentrc, LOCKMODE lockmode);
static void
expand_single_inheritance_child(PlannerInfo *root, RangeTblEntry *parentrte, Index parentRTindex, Relation parentrel, PlanRowMark *top_parentrc, Relation childrel, RangeTblEntry **childrte_p, Index *childRTindex_p);
static Bitmapset *
translate_col_privs(const Bitmapset *parent_privs, List *translated_vars);
static void
expand_appendrel_subquery(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte, Index rti);

   
                            
                                                          
   
                                                                   
   
                                                                             
                                                                              
                                                                             
                                                                       
                                              
   
                                                                               
                                                                            
                                                                             
                                                                            
                                                                          
                                               
   
                                                                            
                                                                             
                                                                             
                                                                              
   
void
expand_inherited_rtentry(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte, Index rti)
{
  Oid parentOID;
  Relation oldrelation;
  LOCKMODE lockmode;
  PlanRowMark *oldrc;
  bool old_isParent = false;
  int old_allMarkTypes = 0;

  Assert(rte->inh);                        

  if (rte->rtekind == RTE_SUBQUERY)
  {
    expand_appendrel_subquery(root, rel, rte, rti);
    return;
  }

  Assert(rte->rtekind == RTE_RELATION);

  parentOID = rte->relid;

     
                                                                          
                                               
     

     
                                                                           
                                                                             
                                                                           
                                                                            
                                                                            
                                            
     
  oldrelation = table_open(parentOID, NoLock);
  lockmode = rte->rellockmode;

     
                                                                          
                                                                             
            
     
  oldrc = get_plan_rowmark(root->rowMarks, rti);
  if (oldrc)
  {
    old_isParent = oldrc->isParent;
    oldrc->isParent = true;
                                                                      
    old_allMarkTypes = oldrc->allMarkTypes;
  }

                                              
  if (oldrelation->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
       
                                                      
       
    Assert(rte->relkind == RELKIND_PARTITIONED_TABLE);

       
                                                                      
                                                                        
       
    expand_partitioned_rtentry(root, rel, rte, rti, oldrelation, oldrc, lockmode);
  }
  else
  {
       
                                                                           
                                                                   
                                                                
       
    List *inhOIDs;
    ListCell *l;

                                                                       
    inhOIDs = find_all_inheritors(parentOID, lockmode, NULL);

       
                                                                           
                                                                       
                                                                          
                                                                      
                                                                         
                                
       
    Assert(inhOIDs != NIL);
    Assert(linitial_oid(inhOIDs) == parentOID);

                                                                    
    expand_planner_arrays(root, list_length(inhOIDs));

       
                                                                          
                            
       
    foreach (l, inhOIDs)
    {
      Oid childOID = lfirst_oid(l);
      Relation newrelation;
      RangeTblEntry *childrte;
      Index childRTindex;

                                                              
      if (childOID != parentOID)
      {
        newrelation = table_open(childOID, NoLock);
      }
      else
      {
        newrelation = oldrelation;
      }

         
                                                                         
                                                                        
                                                                       
                                        
         
      if (childOID != parentOID && RELATION_IS_OTHER_TEMP(newrelation))
      {
        table_close(newrelation, lockmode);
        continue;
      }

                                                                     
      expand_single_inheritance_child(root, rte, rti, oldrelation, oldrc, newrelation, &childrte, &childRTindex);

                                               
      (void)build_simple_rel(root, childRTindex, rel);

                                                 
      if (childOID != parentOID)
      {
        table_close(newrelation, NoLock);
      }
    }
  }

     
                                                                           
                                                                        
                                                                           
                                                                         
                           
     
  if (oldrc)
  {
    int new_allMarkTypes = oldrc->allMarkTypes;
    Var *var;
    TargetEntry *tle;
    char resname[32];
    List *newvars = NIL;

                                                              
    if (new_allMarkTypes & ~(1 << ROW_MARK_COPY) && !(old_allMarkTypes & ~(1 << ROW_MARK_COPY)))
    {
                             
      var = makeVar(oldrc->rti, SelfItemPointerAttributeNumber, TIDOID, -1, InvalidOid, 0);
      snprintf(resname, sizeof(resname), "ctid%u", oldrc->rowmarkId);
      tle = makeTargetEntry((Expr *)var, list_length(root->processed_tlist) + 1, pstrdup(resname), true);
      root->processed_tlist = lappend(root->processed_tlist, tle);
      newvars = lappend(newvars, var);
    }

                                                                    
    if ((new_allMarkTypes & (1 << ROW_MARK_COPY)) && !(old_allMarkTypes & (1 << ROW_MARK_COPY)))
    {
      var = makeWholeRowVar(planner_rt_fetch(oldrc->rti, root), oldrc->rti, 0, false);
      snprintf(resname, sizeof(resname), "wholerow%u", oldrc->rowmarkId);
      tle = makeTargetEntry((Expr *)var, list_length(root->processed_tlist) + 1, pstrdup(resname), true);
      root->processed_tlist = lappend(root->processed_tlist, tle);
      newvars = lappend(newvars, var);
    }

                                                         
    if (!old_isParent)
    {
      var = makeVar(oldrc->rti, TableOidAttributeNumber, OIDOID, -1, InvalidOid, 0);
      snprintf(resname, sizeof(resname), "tableoid%u", oldrc->rowmarkId);
      tle = makeTargetEntry((Expr *)var, list_length(root->processed_tlist) + 1, pstrdup(resname), true);
      root->processed_tlist = lappend(root->processed_tlist, tle);
      newvars = lappend(newvars, var);
    }

       
                                                                         
                                                               
       
    add_vars_to_targetlist(root, newvars, bms_make_singleton(0), false);
  }

  table_close(oldrelation, NoLock);
}

   
                              
                                                       
   
static void
expand_partitioned_rtentry(PlannerInfo *root, RelOptInfo *relinfo, RangeTblEntry *parentrte, Index parentRTindex, Relation parentrel, PlanRowMark *top_parentrc, LOCKMODE lockmode)
{
  PartitionDesc partdesc;
  Bitmapset *live_parts;
  int num_live_parts;
  int i;

  check_stack_depth();

  Assert(parentrte->inh);

  partdesc = PartitionDirectoryLookup(root->glob->partition_directory, parentrel);

                                                                      
  Assert(partdesc);

     
                                                                             
                                                                       
                                                                      
                                                                             
                                                   
     
  if (!root->partColsUpdated)
  {
    root->partColsUpdated = has_partition_attrs(parentrel, parentrte->updatedCols, NULL);
  }

     
                                                                    
     
  Assert(!has_partition_attrs(parentrel, parentrte->extraUpdatedCols, NULL));

                                                              
  if (partdesc->nparts == 0)
  {
    return;
  }

     
                                                                            
                                                                            
                                                                            
                           
     
  live_parts = prune_append_rel_partitions(relinfo);

                                                                  
  num_live_parts = bms_num_members(live_parts);
  if (num_live_parts > 0)
  {
    expand_planner_arrays(root, num_live_parts);
  }

     
                                                                         
                                                                            
                   
     
  Assert(relinfo->part_rels == NULL);
  relinfo->part_rels = (RelOptInfo **)palloc0(relinfo->nparts * sizeof(RelOptInfo *));

     
                                                                   
                                                                            
                                                         
     
  i = -1;
  while ((i = bms_next_member(live_parts, i)) >= 0)
  {
    Oid childOID = partdesc->oids[i];
    Relation childrel;
    RangeTblEntry *childrte;
    Index childRTindex;
    RelOptInfo *childrelinfo;

                                            
    childrel = table_open(childOID, lockmode);

       
                                                                         
                                                                       
              
       
    if (RELATION_IS_OTHER_TEMP(childrel))
    {
      elog(ERROR, "temporary relation from another session found as partition");
    }

                                                                   
    expand_single_inheritance_child(root, parentrte, parentRTindex, parentrel, top_parentrc, childrel, &childrte, &childRTindex);

                                             
    childrelinfo = build_simple_rel(root, childRTindex, relinfo);
    relinfo->part_rels[i] = childrelinfo;

                                                      
    if (childrel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
    {
      expand_partitioned_rtentry(root, childrelinfo, childrte, childRTindex, childrel, top_parentrc, lockmode);
    }

                                              
    table_close(childrel, NoLock);
  }
}

   
                                   
                                                                          
   
                                                                    
                                                                         
                                                                        
                                                                             
                                                                            
                                                           
   
                                                                       
                                                                     
   
                                                                  
                                                    
   
                                                                        
                          
   
static void
expand_single_inheritance_child(PlannerInfo *root, RangeTblEntry *parentrte, Index parentRTindex, Relation parentrel, PlanRowMark *top_parentrc, Relation childrel, RangeTblEntry **childrte_p, Index *childRTindex_p)
{
  Query *parse = root->parse;
  Oid parentOID = RelationGetRelid(parentrel);
  Oid childOID = RelationGetRelid(childrel);
  RangeTblEntry *childrte;
  Index childRTindex;
  AppendRelInfo *appinfo;

     
                                                                           
                                                                     
                                                                            
                                                                             
                                                                           
                                                                   
                                                                           
                                                                     
                                                                        
                                                          
     
  childrte = copyObject(parentrte);
  *childrte_p = childrte;
  childrte->relid = childOID;
  childrte->relkind = childrel->rd_rel->relkind;
                                                             
  if (childrte->relkind == RELKIND_PARTITIONED_TABLE)
  {
    Assert(childOID != parentOID);
    childrte->inh = true;
  }
  else
  {
    childrte->inh = false;
  }
  childrte->requiredPerms = 0;
  childrte->securityQuals = NIL;
  parse->rtable = lappend(parse->rtable, childrte);
  childRTindex = list_length(parse->rtable);
  *childRTindex_p = childRTindex;

     
                                                               
     
  appinfo = make_append_rel_info(parentrel, childrel, parentRTindex, childRTindex);
  root->append_rel_list = lappend(root->append_rel_list, appinfo);

     
                                                                         
                                                                            
                                                                       
     
                                                                     
                                                                        
                                                          
     
  if (childOID != parentOID)
  {
    childrte->selectedCols = translate_col_privs(parentrte->selectedCols, appinfo->translated_vars);
    childrte->insertedCols = translate_col_privs(parentrte->insertedCols, appinfo->translated_vars);
    childrte->updatedCols = translate_col_privs(parentrte->updatedCols, appinfo->translated_vars);
    childrte->extraUpdatedCols = translate_col_privs(parentrte->extraUpdatedCols, appinfo->translated_vars);
  }

     
                                                                           
                                                       
     
  Assert(childRTindex < root->simple_rel_array_size);
  Assert(root->simple_rte_array[childRTindex] == NULL);
  root->simple_rte_array[childRTindex] = childrte;
  Assert(root->append_rel_array[childRTindex] == NULL);
  root->append_rel_array[childRTindex] = appinfo;

     
                                                               
     
  if (top_parentrc)
  {
    PlanRowMark *childrc = makeNode(PlanRowMark);

    childrc->rti = childRTindex;
    childrc->prti = top_parentrc->rti;
    childrc->rowmarkId = top_parentrc->rowmarkId;
                                                                       
    childrc->markType = select_rowmark_type(childrte, top_parentrc->strength);
    childrc->allMarkTypes = (1 << childrc->markType);
    childrc->strength = top_parentrc->strength;
    childrc->waitPolicy = top_parentrc->waitPolicy;

       
                                                                           
                                                                         
                                                                    
       
    childrc->isParent = (childrte->relkind == RELKIND_PARTITIONED_TABLE);

                                                                   
    top_parentrc->allMarkTypes |= childrc->allMarkTypes;

    root->rowMarks = lappend(root->rowMarks, childrc);
  }
}

   
                       
                                                                       
                                                      
   
                                                                        
                                                                          
                                                                       
                                                                           
                                                         
   
static Bitmapset *
translate_col_privs(const Bitmapset *parent_privs, List *translated_vars)
{
  Bitmapset *child_privs = NULL;
  bool whole_row;
  int attno;
  ListCell *lc;

                                                             
  for (attno = FirstLowInvalidHeapAttributeNumber + 1; attno < 0; attno++)
  {
    if (bms_is_member(attno - FirstLowInvalidHeapAttributeNumber, parent_privs))
    {
      child_privs = bms_add_member(child_privs, attno - FirstLowInvalidHeapAttributeNumber);
    }
  }

                                               
  whole_row = bms_is_member(InvalidAttrNumber - FirstLowInvalidHeapAttributeNumber, parent_privs);

                                                                          
  attno = InvalidAttrNumber;
  foreach (lc, translated_vars)
  {
    Var *var = lfirst_node(Var, lc);

    attno++;
    if (var == NULL)                             
    {
      continue;
    }
    if (whole_row || bms_is_member(attno - FirstLowInvalidHeapAttributeNumber, parent_privs))
    {
      child_privs = bms_add_member(child_privs, var->varattno - FirstLowInvalidHeapAttributeNumber);
    }
  }

  return child_privs;
}

   
                             
                                                                         
   
                                                                           
                                                                         
                                                                       
                                                                           
   
static void
expand_appendrel_subquery(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte, Index rti)
{
  ListCell *l;

  foreach (l, root->append_rel_list)
  {
    AppendRelInfo *appinfo = (AppendRelInfo *)lfirst(l);
    Index childRTindex = appinfo->child_relid;
    RangeTblEntry *childrte;
    RelOptInfo *childrel;

                                                                 
    if (appinfo->parent_relid != rti)
    {
      continue;
    }

                                                        
    Assert(childRTindex < root->simple_rel_array_size);
    childrte = root->simple_rte_array[childRTindex];
    Assert(childrte != NULL);

                                     
    childrel = build_simple_rel(root, childRTindex, rel);

                                                                         
    if (childrte->inh)
    {
      expand_inherited_rtentry(root, childrel, childrte, childRTindex);
    }
  }
}

   
                         
                                                                        
                                    
   
                                                                          
                                                                               
                                                                  
   
bool
apply_child_basequals(PlannerInfo *root, RelOptInfo *parentrel, RelOptInfo *childrel, RangeTblEntry *childRTE, AppendRelInfo *appinfo)
{
  List *childquals;
  Index cq_min_security;
  ListCell *lc;

     
                                                                         
                                                                            
                                                                             
                                                                      
                                                                           
                                                
     
  childquals = NIL;
  cq_min_security = UINT_MAX;
  foreach (lc, parentrel->baserestrictinfo)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);
    Node *childqual;
    ListCell *lc2;

    Assert(IsA(rinfo, RestrictInfo));
    childqual = adjust_appendrel_attrs(root, (Node *)rinfo->clause, 1, &appinfo);
    childqual = eval_const_expressions(root, childqual);
                                     
    if (childqual && IsA(childqual, Const))
    {
      if (((Const *)childqual)->constisnull || !DatumGetBool(((Const *)childqual)->constvalue))
      {
                                                           
        return false;
      }
                                                            
      continue;
    }
                                                           
    foreach (lc2, make_ands_implicit((Expr *)childqual))
    {
      Node *onecq = (Node *)lfirst(lc2);
      bool pseudoconstant;

                                                                    
      pseudoconstant = !contain_vars_of_level(onecq, 0) && !contain_volatile_functions(onecq);
      if (pseudoconstant)
      {
                                                         
        root->hasPseudoConstantQuals = true;
      }
                                                                 
      childquals = lappend(childquals, make_restrictinfo(root, (Expr *)onecq, rinfo->is_pushed_down, rinfo->outerjoin_delayed, pseudoconstant, rinfo->security_level, NULL, NULL, NULL));
                                                          
      cq_min_security = Min(cq_min_security, rinfo->security_level);
    }
  }

     
                                                                       
                                                                           
                                                                    
                                                                      
                                                                         
                                                                  
                                                                         
                                                                         
                                   
     
  if (childRTE->securityQuals)
  {
    Index security_level = 0;

    foreach (lc, childRTE->securityQuals)
    {
      List *qualset = (List *)lfirst(lc);
      ListCell *lc2;

      foreach (lc2, qualset)
      {
        Expr *qual = (Expr *)lfirst(lc2);

                                                                  
        childquals = lappend(childquals, make_restrictinfo(root, qual, true, false, false, security_level, NULL, NULL, NULL));
        cq_min_security = Min(cq_min_security, security_level);
      }
      security_level++;
    }
    Assert(security_level <= root->qual_security_level);
  }

     
                                                                  
     
  childrel->baserestrictinfo = childquals;
  childrel->baserestrict_min_security = cq_min_security;

  return true;
}
