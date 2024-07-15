                                                                            
   
                    
                                                    
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>

#include "access/htup_details.h"
#include "access/relation.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/heap.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "parser/parse_enr.h"
#include "parser/parse_relation.h"
#include "parser/parse_type.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/varlena.h"

#define MAX_FUZZY_DISTANCE 3

static RangeTblEntry *
scanNameSpaceForRefname(ParseState *pstate, const char *refname, int location);
static RangeTblEntry *
scanNameSpaceForRelid(ParseState *pstate, Oid relid, int location);
static void
check_lateral_ref_ok(ParseState *pstate, ParseNamespaceItem *nsitem, int location);
static void
markRTEForSelectPriv(ParseState *pstate, RangeTblEntry *rte, int rtindex, AttrNumber col);
static void
expandRelation(Oid relid, Alias *eref, int rtindex, int sublevels_up, int location, bool include_dropped, List **colnames, List **colvars);
static void
expandTupleDesc(TupleDesc tupdesc, Alias *eref, int count, int offset, int rtindex, int sublevels_up, int location, bool include_dropped, List **colnames, List **colvars);
static int
specialAttNum(const char *attname);
static bool
isQueryUsingTempRelation_walker(Node *node, void *context);

   
                        
                                                                            
                                                                     
   
                                                                          
                                                                         
            
   
                                                                               
                                                                             
                                                                          
                                                                         
              
   
                                                                          
                                                                        
                                                                        
                                                                         
                                           
   
RangeTblEntry *
refnameRangeTblEntry(ParseState *pstate, const char *schemaname, const char *refname, int location, int *sublevels_up)
{
  Oid relId = InvalidOid;

  if (sublevels_up)
  {
    *sublevels_up = 0;
  }

  if (schemaname != NULL)
  {
    Oid namespaceId;

       
                                                                    
                                                                          
                                                                          
                                                                       
                                                                       
                                                          
       
    namespaceId = LookupNamespaceNoError(schemaname);
    if (!OidIsValid(namespaceId))
    {
      return NULL;
    }
    relId = get_relname_relid(refname, namespaceId);
    if (!OidIsValid(relId))
    {
      return NULL;
    }
  }

  while (pstate != NULL)
  {
    RangeTblEntry *result;

    if (OidIsValid(relId))
    {
      result = scanNameSpaceForRelid(pstate, relId, location);
    }
    else
    {
      result = scanNameSpaceForRefname(pstate, refname, location);
    }

    if (result)
    {
      return result;
    }

    if (sublevels_up)
    {
      (*sublevels_up)++;
    }
    else
    {
      break;
    }

    pstate = pstate->parentParseState;
  }
  return NULL;
}

   
                                                              
                                                                         
                                                  
   
                                                                             
                                                                              
                                                                             
                                                
                                                                   
                                                                         
                                                                            
                                                                           
                                                                           
                                                                      
                     
   
static RangeTblEntry *
scanNameSpaceForRefname(ParseState *pstate, const char *refname, int location)
{
  RangeTblEntry *result = NULL;
  ListCell *l;

  foreach (l, pstate->p_namespace)
  {
    ParseNamespaceItem *nsitem = (ParseNamespaceItem *)lfirst(l);
    RangeTblEntry *rte = nsitem->p_rte;

                                   
    if (!nsitem->p_rel_visible)
    {
      continue;
    }
                                                          
    if (nsitem->p_lateral_only && !pstate->p_lateral_active)
    {
      continue;
    }

    if (strcmp(rte->eref->aliasname, refname) == 0)
    {
      if (result)
      {
        ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_ALIAS), errmsg("table reference \"%s\" is ambiguous", refname), parser_errposition(pstate, location)));
      }
      check_lateral_ref_ok(pstate, nsitem, location);
      result = rte;
    }
  }
  return result;
}

   
                                                                      
                                                                  
                                                  
   
                                                                    
                         
   
static RangeTblEntry *
scanNameSpaceForRelid(ParseState *pstate, Oid relid, int location)
{
  RangeTblEntry *result = NULL;
  ListCell *l;

  foreach (l, pstate->p_namespace)
  {
    ParseNamespaceItem *nsitem = (ParseNamespaceItem *)lfirst(l);
    RangeTblEntry *rte = nsitem->p_rte;

                                   
    if (!nsitem->p_rel_visible)
    {
      continue;
    }
                                                          
    if (nsitem->p_lateral_only && !pstate->p_lateral_active)
    {
      continue;
    }

                                                            
    if (rte->rtekind == RTE_RELATION && rte->relid == relid && rte->alias == NULL)
    {
      if (result)
      {
        ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_ALIAS), errmsg("table reference %u is ambiguous", relid), parser_errposition(pstate, location)));
      }
      check_lateral_ref_ok(pstate, nsitem, location);
      result = rte;
    }
  }
  return result;
}

   
                                                                             
                                                                         
                                                                             
                                                      
   
CommonTableExpr *
scanNameSpaceForCTE(ParseState *pstate, const char *refname, Index *ctelevelsup)
{
  Index levelsup;

  for (levelsup = 0; pstate != NULL; pstate = pstate->parentParseState, levelsup++)
  {
    ListCell *lc;

    foreach (lc, pstate->p_ctenamespace)
    {
      CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);

      if (strcmp(cte->ctename, refname) == 0)
      {
        *ctelevelsup = levelsup;
        return cte;
      }
    }
  }
  return NULL;
}

   
                                                                            
                                                                           
                                                                   
   
static bool
isFutureCTE(ParseState *pstate, const char *refname)
{
  for (; pstate != NULL; pstate = pstate->parentParseState)
  {
    ListCell *lc;

    foreach (lc, pstate->p_future_ctes)
    {
      CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);

      if (strcmp(cte->ctename, refname) == 0)
      {
        return true;
      }
    }
  }
  return false;
}

   
                                                                        
                                           
   
bool
scanNameSpaceForENR(ParseState *pstate, const char *refname)
{
  return name_matches_visible_ENR(pstate, refname);
}

   
                          
                                                                 
                                                                     
   
                                                                          
                                                                              
                                                                             
                                                                            
                                                                          
                                                                           
   
                                                                         
                         
   
static RangeTblEntry *
searchRangeTableForRel(ParseState *pstate, RangeVar *relation)
{
  const char *refname = relation->relname;
  Oid relId = InvalidOid;
  CommonTableExpr *cte = NULL;
  bool isenr = false;
  Index ctelevelsup = 0;
  Index levelsup;

     
                                                                        
                                                                      
               
     
                                                                           
                                                                        
                                                                           
                                                                        
                                                                    
               
     
  if (!relation->schemaname)
  {
    cte = scanNameSpaceForCTE(pstate, refname, &ctelevelsup);
    if (!cte)
    {
      isenr = scanNameSpaceForENR(pstate, refname);
    }
  }

  if (!cte && !isenr)
  {
    relId = RangeVarGetRelid(relation, NoLock, true);
  }

                                                                           
  for (levelsup = 0; pstate != NULL; pstate = pstate->parentParseState, levelsup++)
  {
    ListCell *l;

    foreach (l, pstate->p_rtable)
    {
      RangeTblEntry *rte = (RangeTblEntry *)lfirst(l);

      if (rte->rtekind == RTE_RELATION && OidIsValid(relId) && rte->relid == relId)
      {
        return rte;
      }
      if (rte->rtekind == RTE_CTE && cte != NULL && rte->ctelevelsup + levelsup == ctelevelsup && strcmp(rte->ctename, refname) == 0)
      {
        return rte;
      }
      if (rte->rtekind == RTE_NAMEDTUPLESTORE && isenr && strcmp(rte->enrname, refname) == 0)
      {
        return rte;
      }
      if (strcmp(rte->eref->aliasname, refname) == 0)
      {
        return rte;
      }
    }
  }
  return NULL;
}

   
                                                                  
                                   
   
                                                                       
                                                                   
   
                                                                       
                                                                        
                                                                             
   
                                                                           
                                                                       
                                         
   
void
checkNameSpaceConflicts(ParseState *pstate, List *namespace1, List *namespace2)
{
  ListCell *l1;

  foreach (l1, namespace1)
  {
    ParseNamespaceItem *nsitem1 = (ParseNamespaceItem *)lfirst(l1);
    RangeTblEntry *rte1 = nsitem1->p_rte;
    const char *aliasname1 = rte1->eref->aliasname;
    ListCell *l2;

    if (!nsitem1->p_rel_visible)
    {
      continue;
    }

    foreach (l2, namespace2)
    {
      ParseNamespaceItem *nsitem2 = (ParseNamespaceItem *)lfirst(l2);
      RangeTblEntry *rte2 = nsitem2->p_rte;

      if (!nsitem2->p_rel_visible)
      {
        continue;
      }
      if (strcmp(rte2->eref->aliasname, aliasname1) != 0)
      {
        continue;                             
      }
      if (rte1->rtekind == RTE_RELATION && rte1->alias == NULL && rte2->rtekind == RTE_RELATION && rte2->alias == NULL && rte1->relid != rte2->relid)
      {
        continue;                               
      }
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_ALIAS), errmsg("table name \"%s\" specified more than once", aliasname1)));
    }
  }
}

   
                                                                                
                                                                              
                                                                            
                                                                              
                           
   
                                                                             
   
static void
check_lateral_ref_ok(ParseState *pstate, ParseNamespaceItem *nsitem, int location)
{
  if (nsitem->p_lateral_only && !nsitem->p_lateral_ok)
  {
                                                                  
    RangeTblEntry *rte = nsitem->p_rte;
    char *refname = rte->eref->aliasname;

    ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("invalid reference to FROM-clause entry for table \"%s\"", refname), (rte == pstate->p_target_rangetblentry) ? errhint("There is an entry for table \"%s\", but it cannot be referenced from this part of the query.", refname) : errdetail("The combining JOIN type must be INNER or LEFT for a LATERAL reference."), parser_errposition(pstate, location)));
  }
}

   
                                                                 
                                                                        
                                                             
                                  
   
int
RTERangeTablePosn(ParseState *pstate, RangeTblEntry *rte, int *sublevels_up)
{
  int index;
  ListCell *l;

  if (sublevels_up)
  {
    *sublevels_up = 0;
  }

  while (pstate != NULL)
  {
    index = 1;
    foreach (l, pstate->p_rtable)
    {
      if (rte == (RangeTblEntry *)lfirst(l))
      {
        return index;
      }
      index++;
    }
    pstate = pstate->parentParseState;
    if (sublevels_up)
    {
      (*sublevels_up)++;
    }
    else
    {
      break;
    }
  }

  elog(ERROR, "RTE not found (internal error)");
  return 0;                          
}

   
                                                                    
                                             
   
RangeTblEntry *
GetRTEByRangeTablePosn(ParseState *pstate, int varno, int sublevels_up)
{
  while (sublevels_up-- > 0)
  {
    pstate = pstate->parentParseState;
    Assert(pstate != NULL);
  }
  Assert(varno > 0 && varno <= list_length(pstate->p_rtable));
  return rt_fetch(varno, pstate->p_rtable);
}

   
                                          
   
                                                                             
                                                                              
                        
   
CommonTableExpr *
GetCTEForRTE(ParseState *pstate, RangeTblEntry *rte, int rtelevelsup)
{
  Index levelsup;
  ListCell *lc;

                                                         
  if (rtelevelsup < 0)
  {
    (void)RTERangeTablePosn(pstate, rte, &rtelevelsup);
  }

  Assert(rte->rtekind == RTE_CTE);
  levelsup = rte->ctelevelsup + rtelevelsup;
  while (levelsup-- > 0)
  {
    pstate = pstate->parentParseState;
    if (!pstate)                       
    {
      elog(ERROR, "bad levelsup for CTE \"%s\"", rte->ctename);
    }
  }
  foreach (lc, pstate->p_ctenamespace)
  {
    CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);

    if (strcmp(cte->ctename, rte->ctename) == 0)
    {
      return cte;
    }
  }
                        
  elog(ERROR, "could not find CTE \"%s\"", rte->ctename);
  return NULL;                          
}

   
                             
                                                                         
   
static void
updateFuzzyAttrMatchState(int fuzzy_rte_penalty, FuzzyAttrMatchState *fuzzystate, RangeTblEntry *rte, const char *actual, const char *match, int attnum)
{
  int columndistance;
  int matchlen;

                                                                          
  if (fuzzy_rte_penalty > fuzzystate->distance)
  {
    return;
  }

     
                                                                          
                                                                
     
  if (actual[0] == '\0')
  {
    return;
  }

                                                  
  matchlen = strlen(match);
  columndistance = varstr_levenshtein_less_equal(actual, strlen(actual), match, matchlen, 1, 1, 1, fuzzystate->distance + 1 - fuzzy_rte_penalty, true);

     
                                                                         
                                                    
     
  if (columndistance > matchlen / 2)
  {
    return;
  }

     
                                                                            
                                            
     
  columndistance += fuzzy_rte_penalty;

     
                                                                         
                                      
     
  if (columndistance < fuzzystate->distance)
  {
                                                    
    fuzzystate->distance = columndistance;
    fuzzystate->rfirst = rte;
    fuzzystate->first = attnum;
    fuzzystate->rsecond = NULL;
    fuzzystate->second = InvalidAttrNumber;
  }
  else if (columndistance == fuzzystate->distance)
  {
       
                                                                          
                                                                         
                                                                          
                                                                     
                         
       
    if (AttributeNumberIsValid(fuzzystate->second))
    {
                                      
      fuzzystate->rfirst = NULL;
      fuzzystate->first = InvalidAttrNumber;
      fuzzystate->rsecond = NULL;
      fuzzystate->second = InvalidAttrNumber;
                                                              
      fuzzystate->distance = columndistance - 1;
    }
    else if (AttributeNumberIsValid(fuzzystate->first))
    {
                                                      
      fuzzystate->rsecond = rte;
      fuzzystate->second = attnum;
    }
    else if (fuzzystate->distance <= MAX_FUZZY_DISTANCE)
    {
         
                                                                        
                                                                      
                                                  
         
      fuzzystate->rfirst = rte;
      fuzzystate->first = attnum;
    }
  }
}

   
                    
                                                                 
                                                                 
                                                                
   
                                                                          
                   
   
                                                                               
                                                               
   
Node *
scanRTEForColumn(ParseState *pstate, RangeTblEntry *rte, const char *colname, int location, int fuzzy_rte_penalty, FuzzyAttrMatchState *fuzzystate)
{
  Node *result = NULL;
  int attnum = 0;
  Var *var;
  ListCell *c;

     
                                                                      
                       
     
                                                                             
                                                                             
                                              
     
                                                                         
                                                     
                                                                            
                                                                            
                                                                       
                                    
     
  foreach (c, rte->eref->colnames)
  {
    const char *attcolname = strVal(lfirst(c));

    attnum++;
    if (strcmp(attcolname, colname) == 0)
    {
      if (result)
      {
        ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_COLUMN), errmsg("column reference \"%s\" is ambiguous", colname), parser_errposition(pstate, location)));
      }
      var = make_var(pstate, rte, attnum, location);
                                             
      markVarForSelectPriv(pstate, var, rte);
      result = (Node *)var;
    }

                                                  
    if (fuzzystate != NULL)
    {
      updateFuzzyAttrMatchState(fuzzy_rte_penalty, fuzzystate, rte, attcolname, colname, attnum);
    }
  }

     
                                                                         
                                                                         
     
  if (result)
  {
    return result;
  }

     
                                                                          
                                                                      
               
     
  if (rte->rtekind == RTE_RELATION && rte->relkind != RELKIND_COMPOSITE_TYPE)
  {
                                                             
    attnum = specialAttNum(colname);

                                                                          
    if (pstate->p_expr_kind == EXPR_KIND_CHECK_CONSTRAINT && attnum < InvalidAttrNumber && attnum != TableOidAttributeNumber)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("system column \"%s\" reference in check constraint is invalid", colname), parser_errposition(pstate, location)));
    }

       
                                                                         
       
    if (pstate->p_expr_kind == EXPR_KIND_GENERATED_COLUMN && attnum < InvalidAttrNumber && attnum != TableOidAttributeNumber)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("cannot use system column \"%s\" in column generation expression", colname), parser_errposition(pstate, location)));
    }

    if (attnum != InvalidAttrNumber)
    {
                                                          
      if (SearchSysCacheExists2(ATTNUM, ObjectIdGetDatum(rte->relid), Int16GetDatum(attnum)))
      {
        var = make_var(pstate, rte, attnum, location);
                                               
        markVarForSelectPriv(pstate, var, rte);
        result = (Node *)var;
      }
    }
  }

  return result;
}

   
                
                                            
                                                                
                                                                            
                                                                             
   
Node *
colNameToVar(ParseState *pstate, const char *colname, bool localonly, int location)
{
  Node *result = NULL;
  ParseState *orig_pstate = pstate;

  while (pstate != NULL)
  {
    ListCell *l;

    foreach (l, pstate->p_namespace)
    {
      ParseNamespaceItem *nsitem = (ParseNamespaceItem *)lfirst(l);
      RangeTblEntry *rte = nsitem->p_rte;
      Node *newresult;

                                   
      if (!nsitem->p_cols_visible)
      {
        continue;
      }
                                                            
      if (nsitem->p_lateral_only && !pstate->p_lateral_active)
      {
        continue;
      }

                                                              
      newresult = scanRTEForColumn(orig_pstate, rte, colname, location, 0, NULL);

      if (newresult)
      {
        if (result)
        {
          ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_COLUMN), errmsg("column reference \"%s\" is ambiguous", colname), parser_errposition(pstate, location)));
        }
        check_lateral_ref_ok(pstate, nsitem, location);
        result = newresult;
      }
    }

    if (result != NULL || localonly)
    {
      break;                                             
    }

    pstate = pstate->parentParseState;
  }

  return result;
}

   
                          
                                                                               
                                                                           
   
                                                                           
                                                                             
                                                                           
                                                                            
                                                                         
                                                                         
   
                                                                        
                                                                             
                                                                           
                                       
   
                                                                       
                                                                          
                                                                                
                                                                            
                                                                           
                                                                        
   
static FuzzyAttrMatchState *
searchRangeTableForCol(ParseState *pstate, const char *alias, const char *colname, int location)
{
  ParseState *orig_pstate = pstate;
  FuzzyAttrMatchState *fuzzystate = palloc(sizeof(FuzzyAttrMatchState));

  fuzzystate->distance = MAX_FUZZY_DISTANCE + 1;
  fuzzystate->rfirst = NULL;
  fuzzystate->rsecond = NULL;
  fuzzystate->first = InvalidAttrNumber;
  fuzzystate->second = InvalidAttrNumber;

  while (pstate != NULL)
  {
    ListCell *l;

    foreach (l, pstate->p_rtable)
    {
      RangeTblEntry *rte = (RangeTblEntry *)lfirst(l);
      int fuzzy_rte_penalty = 0;

         
                                                                     
                                                                       
                                                                         
                                                    
         
      if (rte->rtekind == RTE_JOIN)
      {
        continue;
      }

         
                                                                       
                                                                     
                                                                        
                                            
         
      if (alias != NULL)
      {
        fuzzy_rte_penalty = varstr_levenshtein_less_equal(alias, strlen(alias), rte->eref->aliasname, strlen(rte->eref->aliasname), 1, 1, 1, MAX_FUZZY_DISTANCE + 1, true);
      }

         
                                                                      
                                              
         
      if (scanRTEForColumn(orig_pstate, rte, colname, location, fuzzy_rte_penalty, fuzzystate) && fuzzy_rte_penalty == 0)
      {
        fuzzystate->rfirst = rte;
        fuzzystate->first = InvalidAttrNumber;
        fuzzystate->rsecond = NULL;
        fuzzystate->second = InvalidAttrNumber;
        return fuzzystate;
      }
    }

    pstate = pstate->parentParseState;
  }

  return fuzzystate;
}

   
                        
                                                                        
   
                                                          
   
                                                                            
                                                                      
                                                                         
   
static void
markRTEForSelectPriv(ParseState *pstate, RangeTblEntry *rte, int rtindex, AttrNumber col)
{
  if (rte == NULL)
  {
    rte = rt_fetch(rtindex, pstate->p_rtable);
  }

  if (rte->rtekind == RTE_RELATION)
  {
                                                                  
    rte->requiredPerms |= ACL_SELECT;
                                                      
    rte->selectedCols = bms_add_member(rte->selectedCols, col - FirstLowInvalidHeapAttributeNumber);
  }
  else if (rte->rtekind == RTE_JOIN)
  {
    if (col == InvalidAttrNumber)
    {
         
                                                                        
                                       
         
      JoinExpr *j;

      if (rtindex > 0 && rtindex <= list_length(pstate->p_joinexprs))
      {
        j = list_nth_node(JoinExpr, pstate->p_joinexprs, rtindex - 1);
      }
      else
      {
        j = NULL;
      }
      if (j == NULL)
      {
        elog(ERROR, "could not find JoinExpr for whole-row reference");
      }

                                            
      if (IsA(j->larg, RangeTblRef))
      {
        int varno = ((RangeTblRef *)j->larg)->rtindex;

        markRTEForSelectPriv(pstate, NULL, varno, InvalidAttrNumber);
      }
      else if (IsA(j->larg, JoinExpr))
      {
        int varno = ((JoinExpr *)j->larg)->rtindex;

        markRTEForSelectPriv(pstate, NULL, varno, InvalidAttrNumber);
      }
      else
      {
        elog(ERROR, "unrecognized node type: %d", (int)nodeTag(j->larg));
      }
      if (IsA(j->rarg, RangeTblRef))
      {
        int varno = ((RangeTblRef *)j->rarg)->rtindex;

        markRTEForSelectPriv(pstate, NULL, varno, InvalidAttrNumber);
      }
      else if (IsA(j->rarg, JoinExpr))
      {
        int varno = ((JoinExpr *)j->rarg)->rtindex;

        markRTEForSelectPriv(pstate, NULL, varno, InvalidAttrNumber);
      }
      else
      {
        elog(ERROR, "unrecognized node type: %d", (int)nodeTag(j->rarg));
      }
    }
    else
    {
         
                                                                  
         
                                                                      
                                                                      
                                                                       
                                                                       
                                                                  
         
      Var *aliasvar;

      Assert(col > 0 && col <= list_length(rte->joinaliasvars));
      aliasvar = (Var *)list_nth(rte->joinaliasvars, col - 1);
      aliasvar = (Var *)strip_implicit_coercions((Node *)aliasvar);
      if (aliasvar && IsA(aliasvar, Var))
      {
        markVarForSelectPriv(pstate, aliasvar, NULL);
      }
    }
  }
                                                       
}

   
                        
                                                                     
   
                                                                      
                                         
   
void
markVarForSelectPriv(ParseState *pstate, Var *var, RangeTblEntry *rte)
{
  Index lv;

  Assert(IsA(var, Var));
                                                          
  for (lv = 0; lv < var->varlevelsup; lv++)
  {
    pstate = pstate->parentParseState;
  }
  markRTEForSelectPriv(pstate, rte, var->varno, var->varattno);
}

   
                        
                                                            
                                              
   
                                            
                                                   
                                                 
   
                                                                            
                                                                             
                            
   
                                                                      
   
static void
buildRelationAliases(TupleDesc tupdesc, Alias *alias, Alias *eref)
{
  int maxattrs = tupdesc->natts;
  ListCell *aliaslc;
  int numaliases;
  int varattno;
  int numdropped = 0;

  Assert(eref->colnames == NIL);

  if (alias)
  {
    aliaslc = list_head(alias->colnames);
    numaliases = list_length(alias->colnames);
                                              
    alias->colnames = NIL;
  }
  else
  {
    aliaslc = NULL;
    numaliases = 0;
  }

  for (varattno = 0; varattno < maxattrs; varattno++)
  {
    Form_pg_attribute attr = TupleDescAttr(tupdesc, varattno);
    Value *attrname;

    if (attr->attisdropped)
    {
                                                              
      attrname = makeString(pstrdup(""));
      if (aliaslc)
      {
        alias->colnames = lappend(alias->colnames, attrname);
      }
      numdropped++;
    }
    else if (aliaslc)
    {
                                            
      attrname = (Value *)lfirst(aliaslc);
      aliaslc = lnext(aliaslc);
      alias->colnames = lappend(alias->colnames, attrname);
    }
    else
    {
      attrname = makeString(pstrdup(NameStr(attr->attname)));
                                            
    }

    eref->colnames = lappend(eref->colnames, attrname);
  }

                                       
  if (aliaslc)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("table \"%s\" has %d columns available but %d columns specified", eref->aliasname, maxattrs - numdropped, numaliases)));
  }
}

   
                             
                                                              
                                                                       
   
                                                               
                                                            
                                                               
                                                                 
   
                                                                             
                                                                  
   
static char *
chooseScalarFunctionAlias(Node *funcexpr, char *funcname, Alias *alias, int nfuncs)
{
  char *pname;

     
                                                                         
                                                                   
     
  if (funcexpr && IsA(funcexpr, FuncExpr))
  {
    pname = get_func_result_name(((FuncExpr *)funcexpr)->funcid);
    if (pname)
    {
      return pname;
    }
  }

     
                                                                             
                                                                           
                                              
     
  if (nfuncs == 1 && alias)
  {
    return alias->aliasname;
  }

     
                                      
     
  return funcname;
}

   
                                      
   
                                                                              
                                                                           
                                                                    
   
                                                                          
                                                                        
                                                                  
   
Relation
parserOpenTable(ParseState *pstate, const RangeVar *relation, int lockmode)
{
  Relation rel;
  ParseCallbackState pcbstate;

  setup_parser_errposition_callback(&pcbstate, pstate, relation->location);
  rel = table_openrv_extended(relation, lockmode, true);
  if (rel == NULL)
  {
    if (relation->schemaname)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("relation \"%s.%s\" does not exist", relation->schemaname, relation->relname)));
    }
    else
    {
         
                                                                     
                                                                       
                                                                         
                                                    
         
      if (isFutureCTE(pstate, relation->relname))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("relation \"%s\" does not exist", relation->relname), errdetail("There is a WITH item named \"%s\", but it cannot be referenced from this part of the query.", relation->relname), errhint("Use WITH RECURSIVE, or re-order the WITH items to remove forward references.")));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("relation \"%s\" does not exist", relation->relname)));
      }
    }
  }
  cancel_parser_errposition_callback(&pcbstate);
  return rel;
}

   
                                                                       
   
                                                                        
                                                                              
   
RangeTblEntry *
addRangeTableEntry(ParseState *pstate, RangeVar *relation, Alias *alias, bool inh, bool inFromCl)
{
  RangeTblEntry *rte = makeNode(RangeTblEntry);
  char *refname = alias ? alias->aliasname : relation->relname;
  LOCKMODE lockmode;
  Relation rel;

  Assert(pstate != NULL);

  rte->rtekind = RTE_RELATION;
  rte->alias = alias;

     
                                                                          
                                                                       
                                                                      
                                
     
  lockmode = isLockedRefname(pstate, refname) ? RowShareLock : AccessShareLock;

     
                                                                             
                                                                           
                                                                             
     
  rel = parserOpenTable(pstate, relation, lockmode);
  rte->relid = RelationGetRelid(rel);
  rte->relkind = rel->rd_rel->relkind;
  rte->rellockmode = lockmode;

     
                                                                          
                                 
     
  rte->eref = makeAlias(refname, NIL);
  buildRelationAliases(rel->rd_att, alias, rte->eref);

     
                                                                             
                                                                    
                    
     
  table_close(rel, NoLock);

     
                                       
     
                                                                           
                                                            
     
  rte->lateral = false;
  rte->inh = inh;
  rte->inFromCl = inFromCl;

  rte->requiredPerms = ACL_SELECT;
  rte->checkAsUser = InvalidOid;                                     
  rte->selectedCols = NULL;
  rte->insertedCols = NULL;
  rte->updatedCols = NULL;
  rte->extraUpdatedCols = NULL;

     
                                                                          
                                                           
     
  pstate->p_rtable = lappend(pstate->p_rtable, rte);

  return rte;
}

   
                                                                       
   
                                                                      
                                                                   
   
                                                                          
                                                                          
                                                                     
                      
   
                                                                          
                                                                        
                                                                  
   
RangeTblEntry *
addRangeTableEntryForRelation(ParseState *pstate, Relation rel, int lockmode, Alias *alias, bool inh, bool inFromCl)
{
  RangeTblEntry *rte = makeNode(RangeTblEntry);
  char *refname = alias ? alias->aliasname : RelationGetRelationName(rel);

  Assert(pstate != NULL);

  Assert(lockmode == AccessShareLock || lockmode == RowShareLock || lockmode == RowExclusiveLock);
  Assert(CheckRelationLockedByMe(rel, lockmode, true));

  rte->rtekind = RTE_RELATION;
  rte->alias = alias;
  rte->relid = RelationGetRelid(rel);
  rte->relkind = rel->rd_rel->relkind;
  rte->rellockmode = lockmode;

     
                                                                          
                                 
     
  rte->eref = makeAlias(refname, NIL);
  buildRelationAliases(rel->rd_att, alias, rte->eref);

     
                                       
     
                                                                           
                                                            
     
  rte->lateral = false;
  rte->inh = inh;
  rte->inFromCl = inFromCl;

  rte->requiredPerms = ACL_SELECT;
  rte->checkAsUser = InvalidOid;                                     
  rte->selectedCols = NULL;
  rte->insertedCols = NULL;
  rte->updatedCols = NULL;
  rte->extraUpdatedCols = NULL;

     
                                                                          
                                                           
     
  pstate->p_rtable = lappend(pstate->p_rtable, rte);

  return rte;
}

   
                                                                       
   
                                                                               
                                                 
   
RangeTblEntry *
addRangeTableEntryForSubquery(ParseState *pstate, Query *subquery, Alias *alias, bool lateral, bool inFromCl)
{
  RangeTblEntry *rte = makeNode(RangeTblEntry);
  char *refname = alias->aliasname;
  Alias *eref;
  int numaliases;
  int varattno;
  ListCell *tlistitem;

  Assert(pstate != NULL);

  rte->rtekind = RTE_SUBQUERY;
  rte->subquery = subquery;
  rte->alias = alias;

  eref = copyObject(alias);
  numaliases = list_length(eref->colnames);

                                             
  varattno = 0;
  foreach (tlistitem, subquery->targetList)
  {
    TargetEntry *te = (TargetEntry *)lfirst(tlistitem);

    if (te->resjunk)
    {
      continue;
    }
    varattno++;
    Assert(varattno == te->resno);
    if (varattno > numaliases)
    {
      char *attrname;

      attrname = pstrdup(te->resname);
      eref->colnames = lappend(eref->colnames, makeString(attrname));
    }
  }
  if (varattno < numaliases)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("table \"%s\" has %d columns available but %d columns specified", refname, varattno, numaliases)));
  }

  rte->eref = eref;

     
                                       
     
                                                     
     
  rte->lateral = lateral;
  rte->inh = false;                                
  rte->inFromCl = inFromCl;

  rte->requiredPerms = 0;
  rte->checkAsUser = InvalidOid;
  rte->selectedCols = NULL;
  rte->insertedCols = NULL;
  rte->updatedCols = NULL;
  rte->extraUpdatedCols = NULL;

     
                                                                          
                                                           
     
  pstate->p_rtable = lappend(pstate->p_rtable, rte);

  return rte;
}

   
                                                                          
               
   
                                                                               
   
RangeTblEntry *
addRangeTableEntryForFunction(ParseState *pstate, List *funcnames, List *funcexprs, List *coldeflists, RangeFunction *rangefunc, bool lateral, bool inFromCl)
{
  RangeTblEntry *rte = makeNode(RangeTblEntry);
  Alias *alias = rangefunc->alias;
  Alias *eref;
  char *aliasname;
  int nfuncs = list_length(funcexprs);
  TupleDesc *functupdescs;
  TupleDesc tupdesc;
  ListCell *lc1, *lc2, *lc3;
  int i;
  int j;
  int funcno;
  int natts, totalatts;

  Assert(pstate != NULL);

  rte->rtekind = RTE_FUNCTION;
  rte->relid = InvalidOid;
  rte->subquery = NULL;
  rte->functions = NIL;                                 
  rte->funcordinality = rangefunc->ordinality;
  rte->alias = alias;

     
                                                                          
                                                                             
                                            
     
  if (alias)
  {
    aliasname = alias->aliasname;
  }
  else
  {
    aliasname = linitial(funcnames);
  }

  eref = makeAlias(aliasname, NIL);
  rte->eref = eref;

                                 
  functupdescs = (TupleDesc *)palloc(nfuncs * sizeof(TupleDesc));

  totalatts = 0;
  funcno = 0;
  forthree(lc1, funcexprs, lc2, funcnames, lc3, coldeflists)
  {
    Node *funcexpr = (Node *)lfirst(lc1);
    char *funcname = (char *)lfirst(lc2);
    List *coldeflist = (List *)lfirst(lc3);
    RangeTblFunction *rtfunc = makeNode(RangeTblFunction);
    TypeFuncClass functypclass;
    Oid funcrettype;

                                          
    rtfunc->funcexpr = funcexpr;
    rtfunc->funccolnames = NIL;
    rtfunc->funccoltypes = NIL;
    rtfunc->funccoltypmods = NIL;
    rtfunc->funccolcollations = NIL;
    rtfunc->funcparams = NULL;                             

       
                                                                         
       
    functypclass = get_expr_result_type(funcexpr, &funcrettype, &tupdesc);

       
                                                                          
                                                                     
       
    if (coldeflist != NIL)
    {
      if (functypclass != TYPEFUNC_RECORD)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("a column definition list is only allowed for functions returning \"record\""), parser_errposition(pstate, exprLocation((Node *)coldeflist))));
      }
    }
    else
    {
      if (functypclass == TYPEFUNC_RECORD)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("a column definition list is required for functions returning \"record\""), parser_errposition(pstate, exprLocation(funcexpr))));
      }
    }

    if (functypclass == TYPEFUNC_COMPOSITE || functypclass == TYPEFUNC_COMPOSITE_DOMAIN)
    {
                                                        
      Assert(tupdesc);
    }
    else if (functypclass == TYPEFUNC_SCALAR)
    {
                                       
      tupdesc = CreateTemplateTupleDesc(1);
      TupleDescInitEntry(tupdesc, (AttrNumber)1, chooseScalarFunctionAlias(funcexpr, funcname, alias, nfuncs), funcrettype, -1, 0);
    }
    else if (functypclass == TYPEFUNC_RECORD)
    {
      ListCell *col;

         
                                                                        
                                                                      
                                                                        
         
      if (list_length(coldeflist) > MaxHeapAttributeNumber)
      {
        ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("column definition lists can have at most %d entries", MaxHeapAttributeNumber), parser_errposition(pstate, exprLocation((Node *)coldeflist))));
      }
      tupdesc = CreateTemplateTupleDesc(list_length(coldeflist));
      i = 1;
      foreach (col, coldeflist)
      {
        ColumnDef *n = (ColumnDef *)lfirst(col);
        char *attrname;
        Oid attrtype;
        int32 attrtypmod;
        Oid attrcollation;

        attrname = n->colname;
        if (n->typeName->setof)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("column \"%s\" cannot be declared SETOF", attrname), parser_errposition(pstate, n->location)));
        }
        typenameTypeIdAndMod(pstate, n->typeName, &attrtype, &attrtypmod);
        attrcollation = GetColumnDefCollation(pstate, n, attrtype);
        TupleDescInitEntry(tupdesc, (AttrNumber)i, attrname, attrtype, attrtypmod, 0);
        TupleDescInitEntryCollation(tupdesc, (AttrNumber)i, attrcollation);
        rtfunc->funccolnames = lappend(rtfunc->funccolnames, makeString(pstrdup(attrname)));
        rtfunc->funccoltypes = lappend_oid(rtfunc->funccoltypes, attrtype);
        rtfunc->funccoltypmods = lappend_int(rtfunc->funccoltypmods, attrtypmod);
        rtfunc->funccolcollations = lappend_oid(rtfunc->funccolcollations, attrcollation);

        i++;
      }

         
                                                                     
                                                                         
                                                                     
                                                                      
                                                                     
                                                                       
                         
         
      CheckAttributeNamesTypes(tupdesc, RELKIND_COMPOSITE_TYPE, CHKATYPE_ANYRECORD);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("function \"%s\" in FROM has unsupported return type %s", funcname, format_type_be(funcrettype)), parser_errposition(pstate, exprLocation(funcexpr))));
    }

                                                                      
    rtfunc->funccolcount = tupdesc->natts;
    rte->functions = lappend(rte->functions, rtfunc);

                                        
    functupdescs[funcno] = tupdesc;
    totalatts += tupdesc->natts;
    funcno++;
  }

     
                                                                            
                                       
     
  if (nfuncs > 1 || rangefunc->ordinality)
  {
    if (rangefunc->ordinality)
    {
      totalatts++;
    }

                                                        
    if (totalatts > MaxTupleAttributeNumber)
    {
      ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("functions in FROM can return at most %d columns", MaxTupleAttributeNumber), parser_errposition(pstate, exprLocation((Node *)funcexprs))));
    }

                                                                     
    tupdesc = CreateTemplateTupleDesc(totalatts);
    natts = 0;
    for (i = 0; i < nfuncs; i++)
    {
      for (j = 1; j <= functupdescs[i]->natts; j++)
      {
        TupleDescCopyEntry(tupdesc, ++natts, functupdescs[i], j);
      }
    }

                                             
    if (rangefunc->ordinality)
    {
      TupleDescInitEntry(tupdesc, (AttrNumber)++natts, "ordinality", INT8OID, -1, 0);
    }

    Assert(natts == totalatts);
  }
  else
  {
                                                             
    tupdesc = functupdescs[0];
  }

                                                                  
  buildRelationAliases(tupdesc, alias, eref);

     
                                       
     
                                                                             
                             
     
  rte->lateral = lateral;
  rte->inh = false;                               
  rte->inFromCl = inFromCl;

  rte->requiredPerms = 0;
  rte->checkAsUser = InvalidOid;
  rte->selectedCols = NULL;
  rte->insertedCols = NULL;
  rte->updatedCols = NULL;
  rte->extraUpdatedCols = NULL;

     
                                                                          
                                                           
     
  pstate->p_rtable = lappend(pstate->p_rtable, rte);

  return rte;
}

   
                                                                             
   
                                                                                
   
RangeTblEntry *
addRangeTableEntryForTableFunc(ParseState *pstate, TableFunc *tf, Alias *alias, bool lateral, bool inFromCl)
{
  RangeTblEntry *rte = makeNode(RangeTblEntry);
  char *refname = alias ? alias->aliasname : pstrdup("xmltable");
  Alias *eref;
  int numaliases;

  Assert(pstate != NULL);

                                                      
  if (list_length(tf->colnames) > MaxTupleAttributeNumber)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("functions in FROM can return at most %d columns", MaxTupleAttributeNumber), parser_errposition(pstate, exprLocation((Node *)tf))));
  }
  Assert(list_length(tf->coltypes) == list_length(tf->colnames));
  Assert(list_length(tf->coltypmods) == list_length(tf->colnames));
  Assert(list_length(tf->colcollations) == list_length(tf->colnames));

  rte->rtekind = RTE_TABLEFUNC;
  rte->relid = InvalidOid;
  rte->subquery = NULL;
  rte->tablefunc = tf;
  rte->coltypes = tf->coltypes;
  rte->coltypmods = tf->coltypmods;
  rte->colcollations = tf->colcollations;
  rte->alias = alias;

  eref = alias ? copyObject(alias) : makeAlias(refname, NIL);
  numaliases = list_length(eref->colnames);

                                             
  if (numaliases < list_length(tf->colnames))
  {
    eref->colnames = list_concat(eref->colnames, list_copy_tail(tf->colnames, numaliases));
  }

  if (numaliases > list_length(tf->colnames))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("%s function has %d columns available but %d columns specified", "XMLTABLE", list_length(tf->colnames), numaliases)));
  }

  rte->eref = eref;

     
                                       
     
                                                                          
                                 
     
  rte->lateral = lateral;
  rte->inh = false;                                    
  rte->inFromCl = inFromCl;

  rte->requiredPerms = 0;
  rte->checkAsUser = InvalidOid;
  rte->selectedCols = NULL;
  rte->insertedCols = NULL;
  rte->updatedCols = NULL;
  rte->extraUpdatedCols = NULL;

     
                                                                          
                                                           
     
  pstate->p_rtable = lappend(pstate->p_rtable, rte);

  return rte;
}

   
                                                                          
   
                                                                             
   
RangeTblEntry *
addRangeTableEntryForValues(ParseState *pstate, List *exprs, List *coltypes, List *coltypmods, List *colcollations, Alias *alias, bool lateral, bool inFromCl)
{
  RangeTblEntry *rte = makeNode(RangeTblEntry);
  char *refname = alias ? alias->aliasname : pstrdup("*VALUES*");
  Alias *eref;
  int numaliases;
  int numcolumns;

  Assert(pstate != NULL);

  rte->rtekind = RTE_VALUES;
  rte->relid = InvalidOid;
  rte->subquery = NULL;
  rte->values_lists = exprs;
  rte->coltypes = coltypes;
  rte->coltypmods = coltypmods;
  rte->colcollations = colcollations;
  rte->alias = alias;

  eref = alias ? copyObject(alias) : makeAlias(refname, NIL);

                                             
  numcolumns = list_length((List *)linitial(exprs));
  numaliases = list_length(eref->colnames);
  while (numaliases < numcolumns)
  {
    char attrname[64];

    numaliases++;
    snprintf(attrname, sizeof(attrname), "column%d", numaliases);
    eref->colnames = lappend(eref->colnames, makeString(pstrdup(attrname)));
  }
  if (numcolumns < numaliases)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("VALUES lists \"%s\" have %d columns available but %d columns specified", refname, numcolumns, numaliases)));
  }

  rte->eref = eref;

     
                                       
     
                                                     
     
  rte->lateral = lateral;
  rte->inh = false;                                 
  rte->inFromCl = inFromCl;

  rte->requiredPerms = 0;
  rte->checkAsUser = InvalidOid;
  rte->selectedCols = NULL;
  rte->insertedCols = NULL;
  rte->updatedCols = NULL;
  rte->extraUpdatedCols = NULL;

     
                                                                          
                                                           
     
  pstate->p_rtable = lappend(pstate->p_rtable, rte);

  return rte;
}

   
                                                                   
   
                                                                           
   
RangeTblEntry *
addRangeTableEntryForJoin(ParseState *pstate, List *colnames, JoinType jointype, List *aliasvars, Alias *alias, bool inFromCl)
{
  RangeTblEntry *rte = makeNode(RangeTblEntry);
  Alias *eref;
  int numaliases;

  Assert(pstate != NULL);

     
                                                                            
                                        
     
  if (list_length(aliasvars) > MaxAttrNumber)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("joins can have at most %d columns", MaxAttrNumber)));
  }

  rte->rtekind = RTE_JOIN;
  rte->relid = InvalidOid;
  rte->subquery = NULL;
  rte->jointype = jointype;
  rte->joinaliasvars = aliasvars;
  rte->alias = alias;

  eref = alias ? copyObject(alias) : makeAlias("unnamed_join", NIL);
  numaliases = list_length(eref->colnames);

                                             
  if (numaliases < list_length(colnames))
  {
    eref->colnames = list_concat(eref->colnames, list_copy_tail(colnames, numaliases));
  }

  if (numaliases > list_length(colnames))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("join expression \"%s\" has %d columns available but %d columns specified", eref->aliasname, list_length(colnames), numaliases)));
  }

  rte->eref = eref;

     
                                       
     
                                                
     
  rte->lateral = false;
  rte->inh = false;                           
  rte->inFromCl = inFromCl;

  rte->requiredPerms = 0;
  rte->checkAsUser = InvalidOid;
  rte->selectedCols = NULL;
  rte->insertedCols = NULL;
  rte->updatedCols = NULL;
  rte->extraUpdatedCols = NULL;

     
                                                                          
                                                           
     
  pstate->p_rtable = lappend(pstate->p_rtable, rte);

  return rte;
}

   
                                                                            
   
                                                                          
   
RangeTblEntry *
addRangeTableEntryForCTE(ParseState *pstate, CommonTableExpr *cte, Index levelsup, RangeVar *rv, bool inFromCl)
{
  RangeTblEntry *rte = makeNode(RangeTblEntry);
  Alias *alias = rv->alias;
  char *refname = alias ? alias->aliasname : cte->ctename;
  Alias *eref;
  int numaliases;
  int varattno;
  ListCell *lc;

  Assert(pstate != NULL);

  rte->rtekind = RTE_CTE;
  rte->ctename = cte->ctename;
  rte->ctelevelsup = levelsup;

                                                                          
  rte->self_reference = !IsA(cte->ctequery, Query);
  Assert(cte->cterecursive || !rte->self_reference);
                                                              
  if (!rte->self_reference)
  {
    cte->cterefcount++;
  }

     
                                                                          
                                                                       
                                                                        
     
  if (IsA(cte->ctequery, Query))
  {
    Query *ctequery = (Query *)cte->ctequery;

    if (ctequery->commandType != CMD_SELECT && ctequery->returningList == NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("WITH query \"%s\" does not have a RETURNING clause", cte->ctename), parser_errposition(pstate, rv->location)));
    }
  }

  rte->coltypes = cte->ctecoltypes;
  rte->coltypmods = cte->ctecoltypmods;
  rte->colcollations = cte->ctecolcollations;

  rte->alias = alias;
  if (alias)
  {
    eref = copyObject(alias);
  }
  else
  {
    eref = makeAlias(refname, NIL);
  }
  numaliases = list_length(eref->colnames);

                                             
  varattno = 0;
  foreach (lc, cte->ctecolnames)
  {
    varattno++;
    if (varattno > numaliases)
    {
      eref->colnames = lappend(eref->colnames, lfirst(lc));
    }
  }
  if (varattno < numaliases)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("table \"%s\" has %d columns available but %d columns specified", refname, varattno, numaliases)));
  }

  rte->eref = eref;

     
                                       
     
                                                     
     
  rte->lateral = false;
  rte->inh = false;                                
  rte->inFromCl = inFromCl;

  rte->requiredPerms = 0;
  rte->checkAsUser = InvalidOid;
  rte->selectedCols = NULL;
  rte->insertedCols = NULL;
  rte->updatedCols = NULL;
  rte->extraUpdatedCols = NULL;

     
                                                                          
                                                           
     
  pstate->p_rtable = lappend(pstate->p_rtable, rte);

  return rte;
}

   
                                                                          
                           
   
                                                                               
                                                                               
                                                                              
                                     
   
                                                                             
                             
   
RangeTblEntry *
addRangeTableEntryForENR(ParseState *pstate, RangeVar *rv, bool inFromCl)
{
  RangeTblEntry *rte = makeNode(RangeTblEntry);
  Alias *alias = rv->alias;
  char *refname = alias ? alias->aliasname : rv->relname;
  EphemeralNamedRelationMetadata enrmd;
  TupleDesc tupdesc;
  int attno;

  Assert(pstate != NULL);
  enrmd = get_visible_ENR(pstate, rv->relname);
  Assert(enrmd != NULL);

  switch (enrmd->enrtype)
  {
  case ENR_NAMED_TUPLESTORE:
    rte->rtekind = RTE_NAMEDTUPLESTORE;
    break;

  default:
    elog(ERROR, "unexpected enrtype: %d", enrmd->enrtype);
    return NULL;                          
  }

     
                                                                           
                                                                         
     
  rte->relid = enrmd->reliddesc;

     
                                                                          
                                 
     
  tupdesc = ENRMetadataGetTupDesc(enrmd);
  rte->eref = makeAlias(refname, NIL);
  buildRelationAliases(tupdesc, alias, rte->eref);

                                                                  
  rte->enrname = enrmd->name;
  rte->enrtuples = enrmd->enrtuples;
  rte->coltypes = NIL;
  rte->coltypmods = NIL;
  rte->colcollations = NIL;
  for (attno = 1; attno <= tupdesc->natts; ++attno)
  {
    Form_pg_attribute att = TupleDescAttr(tupdesc, attno - 1);

    if (att->attisdropped)
    {
                                              
      rte->coltypes = lappend_oid(rte->coltypes, InvalidOid);
      rte->coltypmods = lappend_int(rte->coltypmods, 0);
      rte->colcollations = lappend_oid(rte->colcollations, InvalidOid);
    }
    else
    {
                                                               
      if (att->atttypid == InvalidOid)
      {
        elog(ERROR, "atttypid is invalid for non-dropped column in \"%s\"", rv->relname);
      }
      rte->coltypes = lappend_oid(rte->coltypes, att->atttypid);
      rte->coltypmods = lappend_int(rte->coltypmods, att->atttypmod);
      rte->colcollations = lappend_oid(rte->colcollations, att->attcollation);
    }
  }

     
                                       
     
                                               
     
  rte->lateral = false;
  rte->inh = false;                          
  rte->inFromCl = inFromCl;

  rte->requiredPerms = 0;
  rte->checkAsUser = InvalidOid;
  rte->selectedCols = NULL;

     
                                                                          
                                                           
     
  pstate->p_rtable = lappend(pstate->p_rtable, rte);

  return rte;
}

   
                                                                 
   
                                                                           
                                                                         
   
                                                                      
                                                      
   
bool
isLockedRefname(ParseState *pstate, const char *refname)
{
  ListCell *l;

     
                                                                       
                                                                         
     
  if (pstate->p_locked_from_parent)
  {
    return true;
  }

  foreach (l, pstate->p_locking_clause)
  {
    LockingClause *lc = (LockingClause *)lfirst(l);

    if (lc->lockedRels == NIL)
    {
                                    
      return true;
    }
    else
    {
                                 
      ListCell *l2;

      foreach (l2, lc->lockedRels)
      {
        RangeVar *thisrel = (RangeVar *)lfirst(l2);

        if (strcmp(refname, thisrel->relname) == 0)
        {
          return true;
        }
      }
    }
  }
  return false;
}

   
                                                                    
                                                                 
                                                                      
                                       
   
                                                                         
                                                                           
                                                     
   
void
addRTEtoQuery(ParseState *pstate, RangeTblEntry *rte, bool addToJoinList, bool addToRelNameSpace, bool addToVarNameSpace)
{
  if (addToJoinList)
  {
    int rtindex = RTERangeTablePosn(pstate, rte, NULL);
    RangeTblRef *rtr = makeNode(RangeTblRef);

    rtr->rtindex = rtindex;
    pstate->p_joinlist = lappend(pstate->p_joinlist, rtr);
  }
  if (addToRelNameSpace || addToVarNameSpace)
  {
    ParseNamespaceItem *nsitem;

    nsitem = (ParseNamespaceItem *)palloc(sizeof(ParseNamespaceItem));
    nsitem->p_rte = rte;
    nsitem->p_rel_visible = addToRelNameSpace;
    nsitem->p_cols_visible = addToVarNameSpace;
    nsitem->p_lateral_only = false;
    nsitem->p_lateral_ok = true;
    pstate->p_namespace = lappend(pstate->p_namespace, nsitem);
  }
}

   
                                                         
   
                                                                          
                                                                            
                                                                         
                                                                              
                                                 
   
                                                                                
                                                                           
                                                 
   
                                                    
                                                                            
                                        
   
void
expandRTE(RangeTblEntry *rte, int rtindex, int sublevels_up, int location, bool include_dropped, List **colnames, List **colvars)
{
  int varattno;

  if (colnames)
  {
    *colnames = NIL;
  }
  if (colvars)
  {
    *colvars = NIL;
  }

  switch (rte->rtekind)
  {
  case RTE_RELATION:
                               
    expandRelation(rte->relid, rte->eref, rtindex, sublevels_up, location, include_dropped, colnames, colvars);
    break;
  case RTE_SUBQUERY:
  {
                      
    ListCell *aliasp_item = list_head(rte->eref->colnames);
    ListCell *tlistitem;

    varattno = 0;
    foreach (tlistitem, rte->subquery->targetList)
    {
      TargetEntry *te = (TargetEntry *)lfirst(tlistitem);

      if (te->resjunk)
      {
        continue;
      }
      varattno++;
      Assert(varattno == te->resno);

         
                                                              
                                                                
                                                            
                                                               
                                                          
                                                    
         
      if (!aliasp_item)
      {
        break;
      }

      if (colnames)
      {
        char *label = strVal(lfirst(aliasp_item));

        *colnames = lappend(*colnames, makeString(pstrdup(label)));
      }

      if (colvars)
      {
        Var *varnode;

        varnode = makeVar(rtindex, varattno, exprType((Node *)te->expr), exprTypmod((Node *)te->expr), exprCollation((Node *)te->expr), sublevels_up);
        varnode->location = location;

        *colvars = lappend(*colvars, varnode);
      }

      aliasp_item = lnext(aliasp_item);
    }
  }
  break;
  case RTE_FUNCTION:
  {
                      
    int atts_done = 0;
    ListCell *lc;

    foreach (lc, rte->functions)
    {
      RangeTblFunction *rtfunc = (RangeTblFunction *)lfirst(lc);
      TypeFuncClass functypclass;
      Oid funcrettype;
      TupleDesc tupdesc;

      functypclass = get_expr_result_type(rtfunc->funcexpr, &funcrettype, &tupdesc);
      if (functypclass == TYPEFUNC_COMPOSITE || functypclass == TYPEFUNC_COMPOSITE_DOMAIN)
      {
                                                          
        Assert(tupdesc);
        expandTupleDesc(tupdesc, rte->eref, rtfunc->funccolcount, atts_done, rtindex, sublevels_up, location, include_dropped, colnames, colvars);
      }
      else if (functypclass == TYPEFUNC_SCALAR)
      {
                                         
        if (colnames)
        {
          *colnames = lappend(*colnames, list_nth(rte->eref->colnames, atts_done));
        }

        if (colvars)
        {
          Var *varnode;

          varnode = makeVar(rtindex, atts_done + 1, funcrettype, -1, exprCollation(rtfunc->funcexpr), sublevels_up);
          varnode->location = location;

          *colvars = lappend(*colvars, varnode);
        }
      }
      else if (functypclass == TYPEFUNC_RECORD)
      {
        if (colnames)
        {
          List *namelist;

                                                         
          namelist = list_copy_tail(rte->eref->colnames, atts_done);
          namelist = list_truncate(namelist, rtfunc->funccolcount);
          *colnames = list_concat(*colnames, namelist);
        }

        if (colvars)
        {
          ListCell *l1;
          ListCell *l2;
          ListCell *l3;
          int attnum = atts_done;

          forthree(l1, rtfunc->funccoltypes, l2, rtfunc->funccoltypmods, l3, rtfunc->funccolcollations)
          {
            Oid attrtype = lfirst_oid(l1);
            int32 attrtypmod = lfirst_int(l2);
            Oid attrcollation = lfirst_oid(l3);
            Var *varnode;

            attnum++;
            varnode = makeVar(rtindex, attnum, attrtype, attrtypmod, attrcollation, sublevels_up);
            varnode->location = location;
            *colvars = lappend(*colvars, varnode);
          }
        }
      }
      else
      {
                                                                 
        elog(ERROR, "function in FROM has unsupported return type");
      }
      atts_done += rtfunc->funccolcount;
    }

                                             
    if (rte->funcordinality)
    {
      if (colnames)
      {
        *colnames = lappend(*colnames, llast(rte->eref->colnames));
      }

      if (colvars)
      {
        Var *varnode = makeVar(rtindex, atts_done + 1, INT8OID, -1, InvalidOid, sublevels_up);

        *colvars = lappend(*colvars, varnode);
      }
    }
  }
  break;
  case RTE_JOIN:
  {
                  
    ListCell *colname;
    ListCell *aliasvar;

    Assert(list_length(rte->eref->colnames) == list_length(rte->joinaliasvars));

    varattno = 0;
    forboth(colname, rte->eref->colnames, aliasvar, rte->joinaliasvars)
    {
      Node *avar = (Node *)lfirst(aliasvar);

      varattno++;

         
                                                          
                                                                 
                                                              
                                                           
                                                                 
                                         
         
      if (avar == NULL)
      {
        if (include_dropped)
        {
          if (colnames)
          {
            *colnames = lappend(*colnames, makeString(pstrdup("")));
          }
          if (colvars)
          {
               
                                                           
                                                          
                                                 
               
            *colvars = lappend(*colvars, makeNullConst(INT4OID, -1, InvalidOid));
          }
        }
        continue;
      }

      if (colnames)
      {
        char *label = strVal(lfirst(colname));

        *colnames = lappend(*colnames, makeString(pstrdup(label)));
      }

      if (colvars)
      {
        Var *varnode;

        varnode = makeVar(rtindex, varattno, exprType(avar), exprTypmod(avar), exprCollation(avar), sublevels_up);
        varnode->location = location;

        *colvars = lappend(*colvars, varnode);
      }
    }
  }
  break;
  case RTE_TABLEFUNC:
  case RTE_VALUES:
  case RTE_CTE:
  case RTE_NAMEDTUPLESTORE:
  {
                                            
    ListCell *aliasp_item = list_head(rte->eref->colnames);
    ListCell *lct;
    ListCell *lcm;
    ListCell *lcc;

    varattno = 0;
    forthree(lct, rte->coltypes, lcm, rte->coltypmods, lcc, rte->colcollations)
    {
      Oid coltype = lfirst_oid(lct);
      int32 coltypmod = lfirst_int(lcm);
      Oid colcoll = lfirst_oid(lcc);

      varattno++;

      if (colnames)
      {
                                                         
        if (OidIsValid(coltype))
        {
          char *label = strVal(lfirst(aliasp_item));

          *colnames = lappend(*colnames, makeString(pstrdup(label)));
        }
        else if (include_dropped)
        {
          *colnames = lappend(*colnames, makeString(pstrdup("")));
        }

        aliasp_item = lnext(aliasp_item);
      }

      if (colvars)
      {
        if (OidIsValid(coltype))
        {
          Var *varnode;

          varnode = makeVar(rtindex, varattno, coltype, coltypmod, colcoll, sublevels_up);
          varnode->location = location;

          *colvars = lappend(*colvars, varnode);
        }
        else if (include_dropped)
        {
             
                                                          
                           
             
          *colvars = lappend(*colvars, makeNullConst(INT4OID, -1, InvalidOid));
        }
      }
    }
  }
  break;
  case RTE_RESULT:
                                                   
    break;
  default:
    elog(ERROR, "unrecognized RTE kind: %d", (int)rte->rtekind);
  }
}

   
                                          
   
static void
expandRelation(Oid relid, Alias *eref, int rtindex, int sublevels_up, int location, bool include_dropped, List **colnames, List **colvars)
{
  Relation rel;

                                                             
  rel = relation_open(relid, AccessShareLock);
  expandTupleDesc(rel->rd_att, eref, rel->rd_att->natts, 0, rtindex, sublevels_up, location, include_dropped, colnames, colvars);
  relation_close(rel, AccessShareLock);
}

   
                                           
   
                                                                               
                                                                           
                                                                           
                                                                            
                                                                       
   
static void
expandTupleDesc(TupleDesc tupdesc, Alias *eref, int count, int offset, int rtindex, int sublevels_up, int location, bool include_dropped, List **colnames, List **colvars)
{
  ListCell *aliascell = list_head(eref->colnames);
  int varattno;

  if (colnames)
  {
    int i;

    for (i = 0; i < offset; i++)
    {
      if (aliascell)
      {
        aliascell = lnext(aliascell);
      }
    }
  }

  Assert(count <= tupdesc->natts);
  for (varattno = 0; varattno < count; varattno++)
  {
    Form_pg_attribute attr = TupleDescAttr(tupdesc, varattno);

    if (attr->attisdropped)
    {
      if (include_dropped)
      {
        if (colnames)
        {
          *colnames = lappend(*colnames, makeString(pstrdup("")));
        }
        if (colvars)
        {
             
                                                                   
                                               
             
          *colvars = lappend(*colvars, makeNullConst(INT4OID, -1, InvalidOid));
        }
      }
      if (aliascell)
      {
        aliascell = lnext(aliascell);
      }
      continue;
    }

    if (colnames)
    {
      char *label;

      if (aliascell)
      {
        label = strVal(lfirst(aliascell));
        aliascell = lnext(aliascell);
      }
      else
      {
                                                               
        label = NameStr(attr->attname);
      }
      *colnames = lappend(*colnames, makeString(pstrdup(label)));
    }

    if (colvars)
    {
      Var *varnode;

      varnode = makeVar(rtindex, varattno + offset + 1, attr->atttypid, attr->atttypmod, attr->attcollation, sublevels_up);
      varnode->location = location;

      *colvars = lappend(*colvars, varnode);
    }
  }
}

   
                    
                                                                  
                                   
   
                                                                           
                                                                  
                                                                    
                                                                 
   
List *
expandRelAttrs(ParseState *pstate, RangeTblEntry *rte, int rtindex, int sublevels_up, int location)
{
  List *names, *vars;
  ListCell *name, *var;
  List *te_list = NIL;

  expandRTE(rte, rtindex, sublevels_up, location, false, &names, &vars);

     
                                                                            
                                                                     
              
     
  rte->requiredPerms |= ACL_SELECT;

  forboth(name, names, var, vars)
  {
    char *label = strVal(lfirst(name));
    Var *varnode = (Var *)lfirst(var);
    TargetEntry *te;

    te = makeTargetEntry((Expr *)varnode, (AttrNumber)pstate->p_next_resno++, label, false);
    te_list = lappend(te_list, te);

                                            
    markVarForSelectPriv(pstate, varnode, rte);
  }

  Assert(name == NULL && var == NULL);                                 

  return te_list;
}

   
                          
                                               
   
                                                                     
                                                                          
                                               
   
                                                                          
                                                             
   
                                                                          
                                                                            
   
char *
get_rte_attribute_name(RangeTblEntry *rte, AttrNumber attnum)
{
  if (attnum == InvalidAttrNumber)
  {
    return "*";
  }

     
                                                      
     
  if (rte->alias && attnum > 0 && attnum <= list_length(rte->alias->colnames))
  {
    return strVal(list_nth(rte->alias->colnames, attnum - 1));
  }

     
                                                                 
                                                                        
                                                                         
                                                
     
  if (rte->rtekind == RTE_RELATION)
  {
    return get_attname(rte->relid, attnum, false);
  }

     
                                                                           
     
  if (attnum > 0 && attnum <= list_length(rte->eref->colnames))
  {
    return strVal(list_nth(rte->eref->colnames, attnum - 1));
  }

                                          
  elog(ERROR, "invalid attnum %d for rangetable entry %s", attnum, rte->eref->aliasname);
  return NULL;                          
}

   
                          
                                                                         
   
void
get_rte_attribute_type(RangeTblEntry *rte, AttrNumber attnum, Oid *vartype, int32 *vartypmod, Oid *varcollid)
{
  switch (rte->rtekind)
  {
  case RTE_RELATION:
  {
                                                              
    HeapTuple tp;
    Form_pg_attribute att_tup;

    tp = SearchSysCache2(ATTNUM, ObjectIdGetDatum(rte->relid), Int16GetDatum(attnum));
    if (!HeapTupleIsValid(tp))                       
    {
      elog(ERROR, "cache lookup failed for attribute %d of relation %u", attnum, rte->relid);
    }
    att_tup = (Form_pg_attribute)GETSTRUCT(tp);

       
                                                                
                         
       
    if (att_tup->attisdropped)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", NameStr(att_tup->attname), get_rel_name(rte->relid))));
    }
    *vartype = att_tup->atttypid;
    *vartypmod = att_tup->atttypmod;
    *varcollid = att_tup->attcollation;
    ReleaseSysCache(tp);
  }
  break;
  case RTE_SUBQUERY:
  {
                                                                
    TargetEntry *te = get_tle_by_resno(rte->subquery->targetList, attnum);

    if (te == NULL || te->resjunk)
    {
      elog(ERROR, "subquery %s does not have attribute %d", rte->eref->aliasname, attnum);
    }
    *vartype = exprType((Node *)te->expr);
    *vartypmod = exprTypmod((Node *)te->expr);
    *varcollid = exprCollation((Node *)te->expr);
  }
  break;
  case RTE_FUNCTION:
  {
                      
    ListCell *lc;
    int atts_done = 0;

                                                             
    foreach (lc, rte->functions)
    {
      RangeTblFunction *rtfunc = (RangeTblFunction *)lfirst(lc);

      if (attnum > atts_done && attnum <= atts_done + rtfunc->funccolcount)
      {
        TypeFuncClass functypclass;
        Oid funcrettype;
        TupleDesc tupdesc;

        attnum -= atts_done;                                
        functypclass = get_expr_result_type(rtfunc->funcexpr, &funcrettype, &tupdesc);

        if (functypclass == TYPEFUNC_COMPOSITE || functypclass == TYPEFUNC_COMPOSITE_DOMAIN)
        {
                                                            
          Form_pg_attribute att_tup;

          Assert(tupdesc);
          Assert(attnum <= tupdesc->natts);
          att_tup = TupleDescAttr(tupdesc, attnum - 1);

             
                                                             
                                        
             
          if (att_tup->attisdropped)
          {
            ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", NameStr(att_tup->attname), rte->eref->aliasname)));
          }
          *vartype = att_tup->atttypid;
          *vartypmod = att_tup->atttypmod;
          *varcollid = att_tup->attcollation;
        }
        else if (functypclass == TYPEFUNC_SCALAR)
        {
                                           
          *vartype = funcrettype;
          *vartypmod = -1;
          *varcollid = exprCollation(rtfunc->funcexpr);
        }
        else if (functypclass == TYPEFUNC_RECORD)
        {
          *vartype = list_nth_oid(rtfunc->funccoltypes, attnum - 1);
          *vartypmod = list_nth_int(rtfunc->funccoltypmods, attnum - 1);
          *varcollid = list_nth_oid(rtfunc->funccolcollations, attnum - 1);
        }
        else
        {
             
                                                            
                  
             
          elog(ERROR, "function in FROM has unsupported return type");
        }
        return;
      }
      atts_done += rtfunc->funccolcount;
    }

                                                                   
    if (rte->funcordinality && attnum == atts_done + 1)
    {
      *vartype = INT8OID;
      *vartypmod = -1;
      *varcollid = InvalidOid;
      return;
    }

                                        
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column %d of relation \"%s\" does not exist", attnum, rte->eref->aliasname)));
  }
  break;
  case RTE_JOIN:
  {
       
                                                                 
       
    Node *aliasvar;

    Assert(attnum > 0 && attnum <= list_length(rte->joinaliasvars));
    aliasvar = (Node *)list_nth(rte->joinaliasvars, attnum - 1);
    Assert(aliasvar != NULL);
    *vartype = exprType(aliasvar);
    *vartypmod = exprTypmod(aliasvar);
    *varcollid = exprCollation(aliasvar);
  }
  break;
  case RTE_TABLEFUNC:
  case RTE_VALUES:
  case RTE_CTE:
  case RTE_NAMEDTUPLESTORE:
  {
       
                                                                 
                        
       
    Assert(attnum > 0 && attnum <= list_length(rte->coltypes));
    *vartype = list_nth_oid(rte->coltypes, attnum - 1);
    *vartypmod = list_nth_int(rte->coltypmods, attnum - 1);
    *varcollid = list_nth_oid(rte->colcollations, attnum - 1);

                                                  
    if (!OidIsValid(*vartype))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column %d of relation \"%s\" does not exist", attnum, rte->eref->aliasname)));
    }
  }
  break;
  case RTE_RESULT:
                                        
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column %d of relation \"%s\" does not exist", attnum, rte->eref->aliasname)));
    break;
  default:
    elog(ERROR, "unrecognized RTE kind: %d", (int)rte->rtekind);
  }
}

   
                                
                                                                 
   
bool
get_rte_attribute_is_dropped(RangeTblEntry *rte, AttrNumber attnum)
{
  bool result;

  switch (rte->rtekind)
  {
  case RTE_RELATION:
  {
       
                                                                
       
    HeapTuple tp;
    Form_pg_attribute att_tup;

    tp = SearchSysCache2(ATTNUM, ObjectIdGetDatum(rte->relid), Int16GetDatum(attnum));
    if (!HeapTupleIsValid(tp))                       
    {
      elog(ERROR, "cache lookup failed for attribute %d of relation %u", attnum, rte->relid);
    }
    att_tup = (Form_pg_attribute)GETSTRUCT(tp);
    result = att_tup->attisdropped;
    ReleaseSysCache(tp);
  }
  break;
  case RTE_SUBQUERY:
  case RTE_TABLEFUNC:
  case RTE_VALUES:
  case RTE_CTE:

       
                                                                       
               
       
    result = false;
    break;
  case RTE_NAMEDTUPLESTORE:
  {
                                                         
    if (attnum <= 0 || attnum > list_length(rte->coltypes))
    {
      elog(ERROR, "invalid varattno %d", attnum);
    }
    result = !OidIsValid((list_nth_oid(rte->coltypes, attnum - 1)));
  }
  break;
  case RTE_JOIN:
  {
       
                                                                   
                                                                
                                                               
                                                                
                                                                   
       
    Var *aliasvar;

    if (attnum <= 0 || attnum > list_length(rte->joinaliasvars))
    {
      elog(ERROR, "invalid varattno %d", attnum);
    }
    aliasvar = (Var *)list_nth(rte->joinaliasvars, attnum - 1);

    result = (aliasvar == NULL);
  }
  break;
  case RTE_FUNCTION:
  {
                      
    ListCell *lc;
    int atts_done = 0;

       
                                                                
                                                                
                                                               
                                                               
                                                      
       
    foreach (lc, rte->functions)
    {
      RangeTblFunction *rtfunc = (RangeTblFunction *)lfirst(lc);

      if (attnum > atts_done && attnum <= atts_done + rtfunc->funccolcount)
      {
        TupleDesc tupdesc;

        tupdesc = get_expr_result_tupdesc(rtfunc->funcexpr, true);
        if (tupdesc)
        {
                                                            
          Form_pg_attribute att_tup;

          Assert(tupdesc);
          Assert(attnum - atts_done <= tupdesc->natts);
          att_tup = TupleDescAttr(tupdesc, attnum - atts_done - 1);
          return att_tup->attisdropped;
        }
                                                          
        return false;
      }
      atts_done += rtfunc->funccolcount;
    }

                                                                   
    if (rte->funcordinality && attnum == atts_done + 1)
    {
      return false;
    }

                                        
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column %d of relation \"%s\" does not exist", attnum, rte->eref->aliasname)));
    result = false;                          
  }
  break;
  case RTE_RESULT:
                                        
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column %d of relation \"%s\" does not exist", attnum, rte->eref->aliasname)));
    result = false;                          
    break;
  default:
    elog(ERROR, "unrecognized RTE kind: %d", (int)rte->rtekind);
    result = false;                          
  }

  return result;
}

   
                                                                   
   
                                                 
   
                                                                       
                                               
   
TargetEntry *
get_tle_by_resno(List *tlist, AttrNumber resno)
{
  ListCell *l;

  foreach (l, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(l);

    if (tle->resno == resno)
    {
      return tle;
    }
  }
  return NULL;
}

   
                                                                              
   
                                                             
   
RowMarkClause *
get_parse_rowmark(Query *qry, Index rtindex)
{
  ListCell *l;

  foreach (l, qry->rowMarks)
  {
    RowMarkClause *rc = (RowMarkClause *)lfirst(l);

    if (rc->rti == rtindex)
    {
      return rc;
    }
  }
  return NULL;
}

   
                                                          
   
                                                                        
   
                                                       
                                                        
                                       
   
int
attnameAttNum(Relation rd, const char *attname, bool sysColOK)
{
  int i;

  for (i = 0; i < RelationGetNumberOfAttributes(rd); i++)
  {
    Form_pg_attribute att = TupleDescAttr(rd->rd_att, i);

    if (namestrcmp(&(att->attname), attname) == 0 && !att->attisdropped)
    {
      return i + 1;
    }
  }

  if (sysColOK)
  {
    if ((i = specialAttNum(attname)) != InvalidAttrNumber)
    {
      return i;
    }
  }

                  
  return InvalidAttrNumber;
}

                   
   
                                                                
                       
   
                                                                           
                                                                     
   
static int
specialAttNum(const char *attname)
{
  const FormData_pg_attribute *sysatt;

  sysatt = SystemAttributeByName(attname);
  if (sysatt != NULL)
  {
    return sysatt->attnum;
  }
  return InvalidAttrNumber;
}

   
                                                     
   
                                                       
                                                         
                                       
   
const NameData *
attnumAttName(Relation rd, int attid)
{
  if (attid <= 0)
  {
    const FormData_pg_attribute *sysatt;

    sysatt = SystemAttributeDefinition(attid);
    return &sysatt->attname;
  }
  if (attid > rd->rd_att->natts)
  {
    elog(ERROR, "invalid attribute number %d", attid);
  }
  return &TupleDescAttr(rd->rd_att, attid - 1)->attname;
}

   
                                                     
   
                                                       
                                                         
                                       
   
Oid
attnumTypeId(Relation rd, int attid)
{
  if (attid <= 0)
  {
    const FormData_pg_attribute *sysatt;

    sysatt = SystemAttributeDefinition(attid);
    return sysatt->atttypid;
  }
  if (attid > rd->rd_att->natts)
  {
    elog(ERROR, "invalid attribute number %d", attid);
  }
  return TupleDescAttr(rd->rd_att, attid - 1)->atttypid;
}

   
                                                          
   
                                                                        
   
Oid
attnumCollationId(Relation rd, int attid)
{
  if (attid <= 0)
  {
                                                           
    return InvalidOid;
  }
  if (attid > rd->rd_att->natts)
  {
    elog(ERROR, "invalid attribute number %d", attid);
  }
  return TupleDescAttr(rd->rd_att, attid - 1)->attcollation;
}

   
                                                  
   
                                                                     
                              
   
void
errorMissingRTE(ParseState *pstate, RangeVar *relation)
{
  RangeTblEntry *rte;
  int sublevels_up;
  const char *badAlias = NULL;

     
                                                                    
                                                                           
                                                         
     
  rte = searchRangeTableForRel(pstate, relation);

     
                                                                           
                                                                             
                                                                         
                                               
     
                                                                      
                                                                       
                                                                  
     
  if (rte && rte->alias && strcmp(rte->eref->aliasname, relation->relname) != 0 && refnameRangeTblEntry(pstate, NULL, rte->eref->aliasname, relation->location, &sublevels_up) == rte)
  {
    badAlias = rte->eref->aliasname;
  }

  if (rte)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("invalid reference to FROM-clause entry for table \"%s\"", relation->relname), (badAlias ? errhint("Perhaps you meant to reference the table alias \"%s\".", badAlias) : errhint("There is an entry for table \"%s\", but it cannot be referenced from this part of the query.", rte->eref->aliasname)), parser_errposition(pstate, relation->location)));
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("missing FROM-clause entry for table \"%s\"", relation->relname), parser_errposition(pstate, relation->location)));
  }
}

   
                                                     
   
                                                                     
                              
   
void
errorMissingColumn(ParseState *pstate, const char *relname, const char *colname, int location)
{
  FuzzyAttrMatchState *state;
  char *closestfirst = NULL;

     
                                                                             
                           
     
                                                                         
                             
     
  state = searchRangeTableForCol(pstate, relname, colname, location);

     
                                                        
     
                                                                             
                                                                           
                                                                           
                                                                     
                                                                           
     
  if (state->rfirst && AttributeNumberIsValid(state->first))
  {
    closestfirst = strVal(list_nth(state->rfirst->eref->colnames, state->first - 1));
  }

  if (!state->rsecond)
  {
       
                                                                          
                                                           
       
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), relname ? errmsg("column %s.%s does not exist", relname, colname) : errmsg("column \"%s\" does not exist", colname), state->rfirst ? closestfirst ? errhint("Perhaps you meant to reference the column \"%s.%s\".", state->rfirst->eref->aliasname, closestfirst) : errhint("There is a column named \"%s\" in table \"%s\", but it cannot be referenced from this part of the query.", colname, state->rfirst->eref->aliasname) : 0, parser_errposition(pstate, location)));
  }
  else
  {
                                                                     
    char *closestsecond;

    closestsecond = strVal(list_nth(state->rsecond->eref->colnames, state->second - 1));

    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), relname ? errmsg("column %s.%s does not exist", relname, colname) : errmsg("column \"%s\" does not exist", colname), errhint("Perhaps you meant to reference the column \"%s.%s\" or the column \"%s.%s\".", state->rfirst->eref->aliasname, closestfirst, state->rsecond->eref->aliasname, closestsecond), parser_errposition(pstate, location)));
  }
}

   
                                                                             
                                                                          
   
bool
isQueryUsingTempRelation(Query *query)
{
  return isQueryUsingTempRelation_walker((Node *)query, NULL);
}

static bool
isQueryUsingTempRelation_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }

  if (IsA(node, Query))
  {
    Query *query = (Query *)node;
    ListCell *rtable;

    foreach (rtable, query->rtable)
    {
      RangeTblEntry *rte = lfirst(rtable);

      if (rte->rtekind == RTE_RELATION)
      {
        Relation rel = table_open(rte->relid, AccessShareLock);
        char relpersistence = rel->rd_rel->relpersistence;

        table_close(rel, AccessShareLock);
        if (relpersistence == RELPERSISTENCE_TEMP)
        {
          return true;
        }
      }
    }

    return query_tree_walker(query, isQueryUsingTempRelation_walker, context, QTW_IGNORE_JOINALIASES);
  }

  return expression_tree_walker(node, isQueryUsingTempRelation_walker, context);
}
