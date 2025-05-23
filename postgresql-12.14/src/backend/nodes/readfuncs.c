                                                                            
   
               
                                               
   
                                                                         
                                                                        
   
   
                  
                                   
   
         
                                                                    
                                                                    
                                                                     
                                              
   
                                                                       
                                                                        
                                                                        
                                                                           
                                                           
                                                                         
                                                                             
                                                                        
                                      
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "fmgr.h"
#include "miscadmin.h"
#include "nodes/extensible.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "nodes/readfuncs.h"
#include "utils/builtins.h"

   
                                                                       
                                                                            
                                                                          
            
   

                                                      

                                     
#define READ_LOCALS_NO_FIELDS(nodeTypeName) nodeTypeName *local_node = makeNode(nodeTypeName)

                                                           
#define READ_TEMP_LOCALS()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  const char *token;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  int length

                            
#define READ_LOCALS(nodeTypeName)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  READ_LOCALS_NO_FIELDS(nodeTypeName);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  READ_TEMP_LOCALS()

                                                               
#define READ_INT_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  local_node->fldname = atoi(token)

                                                                        
#define READ_UINT_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  local_node->fldname = atoui(token)

                                                                           
#define READ_UINT64_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  local_node->fldname = pg_strtouint64(token, NULL, 10)

                                                                    
#define READ_LONG_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  local_node->fldname = atol(token)

                                                                             
#define READ_OID_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  local_node->fldname = atooid(token)

                                                 
#define READ_CHAR_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  local_node->fldname = (length == 0) ? '\0' : (token[0] == '\\' ? token[1] : token[0])

                                                                       
#define READ_ENUM_FIELD(fldname, enumtype)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  local_node->fldname = (enumtype)atoi(token)

                        
#define READ_FLOAT_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  local_node->fldname = atof(token)

                          
#define READ_BOOL_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  local_node->fldname = strtobool(token)

                                   
#define READ_STRING_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  local_node->fldname = nullable_string(token, length)

                                                                     
#ifdef WRITE_READ_PARSE_PLAN_TREES
#define READ_LOCATION_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  local_node->fldname = restore_location_fields ? atoi(token) : -1
#else
#define READ_LOCATION_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  (void)token;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  local_node->fldname = -1                                
#endif

                       
#define READ_NODE_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  (void)token;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  local_node->fldname = nodeRead(NULL, 0)

                            
#define READ_BITMAPSET_FIELD(fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  (void)token;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  local_node->fldname = _readBitmapset()

                                    
#define READ_ATTRNUMBER_ARRAY(fldname, len)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  local_node->fldname = readAttrNumberCols(len)

                       
#define READ_OID_ARRAY(fldname, len)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  local_node->fldname = readOidCols(len)

                       
#define READ_INT_ARRAY(fldname, len)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  local_node->fldname = readIntCols(len)

                       
#define READ_BOOL_ARRAY(fldname, len)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  token = pg_strtok(&length);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  local_node->fldname = readBoolCols(len)

                  
#define READ_DONE() return local_node

   
                                                                       
                                                                      
                                                                      
                                                 
   
#define atoui(x) ((unsigned int)strtoul((x), NULL, 10))

#define strtobool(x) ((*(x) == 't') ? true : false)

#define nullable_string(token, length) ((length) == 0 ? NULL : debackslash(token, length))

   
                  
   
static Bitmapset *
_readBitmapset(void)
{
  Bitmapset *result = NULL;

  READ_TEMP_LOCALS();

  token = pg_strtok(&length);
  if (token == NULL)
  {
    elog(ERROR, "incomplete Bitmapset structure");
  }
  if (length != 1 || token[0] != '(')
  {
    elog(ERROR, "unrecognized token: \"%.*s\"", length, token);
  }

  token = pg_strtok(&length);
  if (token == NULL)
  {
    elog(ERROR, "incomplete Bitmapset structure");
  }
  if (length != 1 || token[0] != 'b')
  {
    elog(ERROR, "unrecognized token: \"%.*s\"", length, token);
  }

  for (;;)
  {
    int val;
    char *endptr;

    token = pg_strtok(&length);
    if (token == NULL)
    {
      elog(ERROR, "unterminated Bitmapset structure");
    }
    if (length == 1 && token[0] == ')')
    {
      break;
    }
    val = (int)strtol(token, &endptr, 10);
    if (endptr != token + length)
    {
      elog(ERROR, "unrecognized integer: \"%.*s\"", length, token);
    }
    result = bms_add_member(result, val);
  }

  return result;
}

   
                                                       
   
Bitmapset *
readBitmapset(void)
{
  return _readBitmapset();
}

   
              
   
static Query *
_readQuery(void)
{
  READ_LOCALS(Query);

  READ_ENUM_FIELD(commandType, CmdType);
  READ_ENUM_FIELD(querySource, QuerySource);
  local_node->queryId = UINT64CONST(0);                                 
  READ_BOOL_FIELD(canSetTag);
  READ_NODE_FIELD(utilityStmt);
  READ_INT_FIELD(resultRelation);
  READ_BOOL_FIELD(hasAggs);
  READ_BOOL_FIELD(hasWindowFuncs);
  READ_BOOL_FIELD(hasTargetSRFs);
  READ_BOOL_FIELD(hasSubLinks);
  READ_BOOL_FIELD(hasDistinctOn);
  READ_BOOL_FIELD(hasRecursive);
  READ_BOOL_FIELD(hasModifyingCTE);
  READ_BOOL_FIELD(hasForUpdate);
  READ_BOOL_FIELD(hasRowSecurity);
  READ_NODE_FIELD(cteList);
  READ_NODE_FIELD(rtable);
  READ_NODE_FIELD(jointree);
  READ_NODE_FIELD(targetList);
  READ_ENUM_FIELD(override, OverridingKind);
  READ_NODE_FIELD(onConflict);
  READ_NODE_FIELD(returningList);
  READ_NODE_FIELD(groupClause);
  READ_NODE_FIELD(groupingSets);
  READ_NODE_FIELD(havingQual);
  READ_NODE_FIELD(windowClause);
  READ_NODE_FIELD(distinctClause);
  READ_NODE_FIELD(sortClause);
  READ_NODE_FIELD(limitOffset);
  READ_NODE_FIELD(limitCount);
  READ_NODE_FIELD(rowMarks);
  READ_NODE_FIELD(setOperations);
  READ_NODE_FIELD(constraintDeps);
  READ_NODE_FIELD(withCheckOptions);
  READ_LOCATION_FIELD(stmt_location);
  READ_LOCATION_FIELD(stmt_len);

  READ_DONE();
}

   
                   
   
static NotifyStmt *
_readNotifyStmt(void)
{
  READ_LOCALS(NotifyStmt);

  READ_STRING_FIELD(conditionname);
  READ_STRING_FIELD(payload);

  READ_DONE();
}

   
                          
   
static DeclareCursorStmt *
_readDeclareCursorStmt(void)
{
  READ_LOCALS(DeclareCursorStmt);

  READ_STRING_FIELD(portalname);
  READ_INT_FIELD(options);
  READ_NODE_FIELD(query);

  READ_DONE();
}

   
                        
   
static WithCheckOption *
_readWithCheckOption(void)
{
  READ_LOCALS(WithCheckOption);

  READ_ENUM_FIELD(kind, WCOKind);
  READ_STRING_FIELD(relname);
  READ_STRING_FIELD(polname);
  READ_NODE_FIELD(qual);
  READ_BOOL_FIELD(cascaded);

  READ_DONE();
}

   
                        
   
static SortGroupClause *
_readSortGroupClause(void)
{
  READ_LOCALS(SortGroupClause);

  READ_UINT_FIELD(tleSortGroupRef);
  READ_OID_FIELD(eqop);
  READ_OID_FIELD(sortop);
  READ_BOOL_FIELD(nulls_first);
  READ_BOOL_FIELD(hashable);

  READ_DONE();
}

   
                    
   
static GroupingSet *
_readGroupingSet(void)
{
  READ_LOCALS(GroupingSet);

  READ_ENUM_FIELD(kind, GroupingSetKind);
  READ_NODE_FIELD(content);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                     
   
static WindowClause *
_readWindowClause(void)
{
  READ_LOCALS(WindowClause);

  READ_STRING_FIELD(name);
  READ_STRING_FIELD(refname);
  READ_NODE_FIELD(partitionClause);
  READ_NODE_FIELD(orderClause);
  READ_INT_FIELD(frameOptions);
  READ_NODE_FIELD(startOffset);
  READ_NODE_FIELD(endOffset);
  READ_OID_FIELD(startInRangeFunc);
  READ_OID_FIELD(endInRangeFunc);
  READ_OID_FIELD(inRangeColl);
  READ_BOOL_FIELD(inRangeAsc);
  READ_BOOL_FIELD(inRangeNullsFirst);
  READ_UINT_FIELD(winref);
  READ_BOOL_FIELD(copiedOrder);

  READ_DONE();
}

   
                      
   
static RowMarkClause *
_readRowMarkClause(void)
{
  READ_LOCALS(RowMarkClause);

  READ_UINT_FIELD(rti);
  READ_ENUM_FIELD(strength, LockClauseStrength);
  READ_ENUM_FIELD(waitPolicy, LockWaitPolicy);
  READ_BOOL_FIELD(pushedDown);

  READ_DONE();
}

   
                        
   
static CommonTableExpr *
_readCommonTableExpr(void)
{
  READ_LOCALS(CommonTableExpr);

  READ_STRING_FIELD(ctename);
  READ_NODE_FIELD(aliascolnames);
  READ_ENUM_FIELD(ctematerialized, CTEMaterialize);
  READ_NODE_FIELD(ctequery);
  READ_LOCATION_FIELD(location);
  READ_BOOL_FIELD(cterecursive);
  READ_INT_FIELD(cterefcount);
  READ_NODE_FIELD(ctecolnames);
  READ_NODE_FIELD(ctecoltypes);
  READ_NODE_FIELD(ctecoltypmods);
  READ_NODE_FIELD(ctecolcollations);

  READ_DONE();
}

   
                         
   
static SetOperationStmt *
_readSetOperationStmt(void)
{
  READ_LOCALS(SetOperationStmt);

  READ_ENUM_FIELD(op, SetOperation);
  READ_BOOL_FIELD(all);
  READ_NODE_FIELD(larg);
  READ_NODE_FIELD(rarg);
  READ_NODE_FIELD(colTypes);
  READ_NODE_FIELD(colTypmods);
  READ_NODE_FIELD(colCollations);
  READ_NODE_FIELD(groupClauses);

  READ_DONE();
}

   
                           
   

static Alias *
_readAlias(void)
{
  READ_LOCALS(Alias);

  READ_STRING_FIELD(aliasname);
  READ_NODE_FIELD(colnames);

  READ_DONE();
}

static RangeVar *
_readRangeVar(void)
{
  READ_LOCALS(RangeVar);

  local_node->catalogname = NULL;                                           

  READ_STRING_FIELD(schemaname);
  READ_STRING_FIELD(relname);
  READ_BOOL_FIELD(inh);
  READ_CHAR_FIELD(relpersistence);
  READ_NODE_FIELD(alias);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                  
   
static TableFunc *
_readTableFunc(void)
{
  READ_LOCALS(TableFunc);

  READ_NODE_FIELD(ns_uris);
  READ_NODE_FIELD(ns_names);
  READ_NODE_FIELD(docexpr);
  READ_NODE_FIELD(rowexpr);
  READ_NODE_FIELD(colnames);
  READ_NODE_FIELD(coltypes);
  READ_NODE_FIELD(coltypmods);
  READ_NODE_FIELD(colcollations);
  READ_NODE_FIELD(colexprs);
  READ_NODE_FIELD(coldefexprs);
  READ_BITMAPSET_FIELD(notnulls);
  READ_INT_FIELD(ordinalitycol);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

static IntoClause *
_readIntoClause(void)
{
  READ_LOCALS(IntoClause);

  READ_NODE_FIELD(rel);
  READ_NODE_FIELD(colNames);
  READ_STRING_FIELD(accessMethod);
  READ_NODE_FIELD(options);
  READ_ENUM_FIELD(onCommit, OnCommitAction);
  READ_STRING_FIELD(tableSpaceName);
  READ_NODE_FIELD(viewQuery);
  READ_BOOL_FIELD(skipData);

  READ_DONE();
}

   
            
   
static Var *
_readVar(void)
{
  READ_LOCALS(Var);

  READ_UINT_FIELD(varno);
  READ_INT_FIELD(varattno);
  READ_OID_FIELD(vartype);
  READ_INT_FIELD(vartypmod);
  READ_OID_FIELD(varcollid);
  READ_UINT_FIELD(varlevelsup);
  READ_UINT_FIELD(varnoold);
  READ_INT_FIELD(varoattno);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
              
   
static Const *
_readConst(void)
{
  READ_LOCALS(Const);

  READ_OID_FIELD(consttype);
  READ_INT_FIELD(consttypmod);
  READ_OID_FIELD(constcollid);
  READ_INT_FIELD(constlen);
  READ_BOOL_FIELD(constbyval);
  READ_BOOL_FIELD(constisnull);
  READ_LOCATION_FIELD(location);

  token = pg_strtok(&length);                       
  if (local_node->constisnull)
  {
    token = pg_strtok(&length);                
  }
  else
  {
    local_node->constvalue = readDatum(local_node->constbyval);
  }

  READ_DONE();
}

   
              
   
static Param *
_readParam(void)
{
  READ_LOCALS(Param);

  READ_ENUM_FIELD(paramkind, ParamKind);
  READ_INT_FIELD(paramid);
  READ_OID_FIELD(paramtype);
  READ_INT_FIELD(paramtypmod);
  READ_OID_FIELD(paramcollid);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
               
   
static Aggref *
_readAggref(void)
{
  READ_LOCALS(Aggref);

  READ_OID_FIELD(aggfnoid);
  READ_OID_FIELD(aggtype);
  READ_OID_FIELD(aggcollid);
  READ_OID_FIELD(inputcollid);
  READ_OID_FIELD(aggtranstype);
  READ_NODE_FIELD(aggargtypes);
  READ_NODE_FIELD(aggdirectargs);
  READ_NODE_FIELD(args);
  READ_NODE_FIELD(aggorder);
  READ_NODE_FIELD(aggdistinct);
  READ_NODE_FIELD(aggfilter);
  READ_BOOL_FIELD(aggstar);
  READ_BOOL_FIELD(aggvariadic);
  READ_CHAR_FIELD(aggkind);
  READ_UINT_FIELD(agglevelsup);
  READ_ENUM_FIELD(aggsplit, AggSplit);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                     
   
static GroupingFunc *
_readGroupingFunc(void)
{
  READ_LOCALS(GroupingFunc);

  READ_NODE_FIELD(args);
  READ_NODE_FIELD(refs);
  READ_NODE_FIELD(cols);
  READ_UINT_FIELD(agglevelsup);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                   
   
static WindowFunc *
_readWindowFunc(void)
{
  READ_LOCALS(WindowFunc);

  READ_OID_FIELD(winfnoid);
  READ_OID_FIELD(wintype);
  READ_OID_FIELD(wincollid);
  READ_OID_FIELD(inputcollid);
  READ_NODE_FIELD(args);
  READ_NODE_FIELD(aggfilter);
  READ_UINT_FIELD(winref);
  READ_BOOL_FIELD(winstar);
  READ_BOOL_FIELD(winagg);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                        
   
static SubscriptingRef *
_readSubscriptingRef(void)
{
  READ_LOCALS(SubscriptingRef);

  READ_OID_FIELD(refcontainertype);
  READ_OID_FIELD(refelemtype);
  READ_INT_FIELD(reftypmod);
  READ_OID_FIELD(refcollid);
  READ_NODE_FIELD(refupperindexpr);
  READ_NODE_FIELD(reflowerindexpr);
  READ_NODE_FIELD(refexpr);
  READ_NODE_FIELD(refassgnexpr);

  READ_DONE();
}

   
                 
   
static FuncExpr *
_readFuncExpr(void)
{
  READ_LOCALS(FuncExpr);

  READ_OID_FIELD(funcid);
  READ_OID_FIELD(funcresulttype);
  READ_BOOL_FIELD(funcretset);
  READ_BOOL_FIELD(funcvariadic);
  READ_ENUM_FIELD(funcformat, CoercionForm);
  READ_OID_FIELD(funccollid);
  READ_OID_FIELD(inputcollid);
  READ_NODE_FIELD(args);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                     
   
static NamedArgExpr *
_readNamedArgExpr(void)
{
  READ_LOCALS(NamedArgExpr);

  READ_NODE_FIELD(arg);
  READ_STRING_FIELD(name);
  READ_INT_FIELD(argnumber);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
               
   
static OpExpr *
_readOpExpr(void)
{
  READ_LOCALS(OpExpr);

  READ_OID_FIELD(opno);
  READ_OID_FIELD(opfuncid);
  READ_OID_FIELD(opresulttype);
  READ_BOOL_FIELD(opretset);
  READ_OID_FIELD(opcollid);
  READ_OID_FIELD(inputcollid);
  READ_NODE_FIELD(args);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                     
   
static DistinctExpr *
_readDistinctExpr(void)
{
  READ_LOCALS(DistinctExpr);

  READ_OID_FIELD(opno);
  READ_OID_FIELD(opfuncid);
  READ_OID_FIELD(opresulttype);
  READ_BOOL_FIELD(opretset);
  READ_OID_FIELD(opcollid);
  READ_OID_FIELD(inputcollid);
  READ_NODE_FIELD(args);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                   
   
static NullIfExpr *
_readNullIfExpr(void)
{
  READ_LOCALS(NullIfExpr);

  READ_OID_FIELD(opno);
  READ_OID_FIELD(opfuncid);
  READ_OID_FIELD(opresulttype);
  READ_BOOL_FIELD(opretset);
  READ_OID_FIELD(opcollid);
  READ_OID_FIELD(inputcollid);
  READ_NODE_FIELD(args);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                          
   
static ScalarArrayOpExpr *
_readScalarArrayOpExpr(void)
{
  READ_LOCALS(ScalarArrayOpExpr);

  READ_OID_FIELD(opno);
  READ_OID_FIELD(opfuncid);
  READ_BOOL_FIELD(useOr);
  READ_OID_FIELD(inputcollid);
  READ_NODE_FIELD(args);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                 
   
static BoolExpr *
_readBoolExpr(void)
{
  READ_LOCALS(BoolExpr);

                                          
  token = pg_strtok(&length);                   
  token = pg_strtok(&length);                      
  if (strncmp(token, "and", 3) == 0)
  {
    local_node->boolop = AND_EXPR;
  }
  else if (strncmp(token, "or", 2) == 0)
  {
    local_node->boolop = OR_EXPR;
  }
  else if (strncmp(token, "not", 3) == 0)
  {
    local_node->boolop = NOT_EXPR;
  }
  else
  {
    elog(ERROR, "unrecognized boolop \"%.*s\"", length, token);
  }

  READ_NODE_FIELD(args);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                
   
static SubLink *
_readSubLink(void)
{
  READ_LOCALS(SubLink);

  READ_ENUM_FIELD(subLinkType, SubLinkType);
  READ_INT_FIELD(subLinkId);
  READ_NODE_FIELD(testexpr);
  READ_NODE_FIELD(operName);
  READ_NODE_FIELD(subselect);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                                                                       
   

   
                    
   
static FieldSelect *
_readFieldSelect(void)
{
  READ_LOCALS(FieldSelect);

  READ_NODE_FIELD(arg);
  READ_INT_FIELD(fieldnum);
  READ_OID_FIELD(resulttype);
  READ_INT_FIELD(resulttypmod);
  READ_OID_FIELD(resultcollid);

  READ_DONE();
}

   
                   
   
static FieldStore *
_readFieldStore(void)
{
  READ_LOCALS(FieldStore);

  READ_NODE_FIELD(arg);
  READ_NODE_FIELD(newvals);
  READ_NODE_FIELD(fieldnums);
  READ_OID_FIELD(resulttype);

  READ_DONE();
}

   
                    
   
static RelabelType *
_readRelabelType(void)
{
  READ_LOCALS(RelabelType);

  READ_NODE_FIELD(arg);
  READ_OID_FIELD(resulttype);
  READ_INT_FIELD(resulttypmod);
  READ_OID_FIELD(resultcollid);
  READ_ENUM_FIELD(relabelformat, CoercionForm);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                    
   
static CoerceViaIO *
_readCoerceViaIO(void)
{
  READ_LOCALS(CoerceViaIO);

  READ_NODE_FIELD(arg);
  READ_OID_FIELD(resulttype);
  READ_OID_FIELD(resultcollid);
  READ_ENUM_FIELD(coerceformat, CoercionForm);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                        
   
static ArrayCoerceExpr *
_readArrayCoerceExpr(void)
{
  READ_LOCALS(ArrayCoerceExpr);

  READ_NODE_FIELD(arg);
  READ_NODE_FIELD(elemexpr);
  READ_OID_FIELD(resulttype);
  READ_INT_FIELD(resulttypmod);
  READ_OID_FIELD(resultcollid);
  READ_ENUM_FIELD(coerceformat, CoercionForm);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                           
   
static ConvertRowtypeExpr *
_readConvertRowtypeExpr(void)
{
  READ_LOCALS(ConvertRowtypeExpr);

  READ_NODE_FIELD(arg);
  READ_OID_FIELD(resulttype);
  READ_ENUM_FIELD(convertformat, CoercionForm);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                    
   
static CollateExpr *
_readCollateExpr(void)
{
  READ_LOCALS(CollateExpr);

  READ_NODE_FIELD(arg);
  READ_OID_FIELD(collOid);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                 
   
static CaseExpr *
_readCaseExpr(void)
{
  READ_LOCALS(CaseExpr);

  READ_OID_FIELD(casetype);
  READ_OID_FIELD(casecollid);
  READ_NODE_FIELD(arg);
  READ_NODE_FIELD(args);
  READ_NODE_FIELD(defresult);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                 
   
static CaseWhen *
_readCaseWhen(void)
{
  READ_LOCALS(CaseWhen);

  READ_NODE_FIELD(expr);
  READ_NODE_FIELD(result);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                     
   
static CaseTestExpr *
_readCaseTestExpr(void)
{
  READ_LOCALS(CaseTestExpr);

  READ_OID_FIELD(typeId);
  READ_INT_FIELD(typeMod);
  READ_OID_FIELD(collation);

  READ_DONE();
}

   
                  
   
static ArrayExpr *
_readArrayExpr(void)
{
  READ_LOCALS(ArrayExpr);

  READ_OID_FIELD(array_typeid);
  READ_OID_FIELD(array_collid);
  READ_OID_FIELD(element_typeid);
  READ_NODE_FIELD(elements);
  READ_BOOL_FIELD(multidims);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                
   
static RowExpr *
_readRowExpr(void)
{
  READ_LOCALS(RowExpr);

  READ_NODE_FIELD(args);
  READ_OID_FIELD(row_typeid);
  READ_ENUM_FIELD(row_format, CoercionForm);
  READ_NODE_FIELD(colnames);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                       
   
static RowCompareExpr *
_readRowCompareExpr(void)
{
  READ_LOCALS(RowCompareExpr);

  READ_ENUM_FIELD(rctype, RowCompareType);
  READ_NODE_FIELD(opnos);
  READ_NODE_FIELD(opfamilies);
  READ_NODE_FIELD(inputcollids);
  READ_NODE_FIELD(largs);
  READ_NODE_FIELD(rargs);

  READ_DONE();
}

   
                     
   
static CoalesceExpr *
_readCoalesceExpr(void)
{
  READ_LOCALS(CoalesceExpr);

  READ_OID_FIELD(coalescetype);
  READ_OID_FIELD(coalescecollid);
  READ_NODE_FIELD(args);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                   
   
static MinMaxExpr *
_readMinMaxExpr(void)
{
  READ_LOCALS(MinMaxExpr);

  READ_OID_FIELD(minmaxtype);
  READ_OID_FIELD(minmaxcollid);
  READ_OID_FIELD(inputcollid);
  READ_ENUM_FIELD(op, MinMaxOp);
  READ_NODE_FIELD(args);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                         
   
static SQLValueFunction *
_readSQLValueFunction(void)
{
  READ_LOCALS(SQLValueFunction);

  READ_ENUM_FIELD(op, SQLValueFunctionOp);
  READ_OID_FIELD(type);
  READ_INT_FIELD(typmod);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                
   
static XmlExpr *
_readXmlExpr(void)
{
  READ_LOCALS(XmlExpr);

  READ_ENUM_FIELD(op, XmlExprOp);
  READ_STRING_FIELD(name);
  READ_NODE_FIELD(named_args);
  READ_NODE_FIELD(arg_names);
  READ_NODE_FIELD(args);
  READ_ENUM_FIELD(xmloption, XmlOptionType);
  READ_OID_FIELD(type);
  READ_INT_FIELD(typmod);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                 
   
static NullTest *
_readNullTest(void)
{
  READ_LOCALS(NullTest);

  READ_NODE_FIELD(arg);
  READ_ENUM_FIELD(nulltesttype, NullTestType);
  READ_BOOL_FIELD(argisrow);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                    
   
static BooleanTest *
_readBooleanTest(void)
{
  READ_LOCALS(BooleanTest);

  READ_NODE_FIELD(arg);
  READ_ENUM_FIELD(booltesttype, BoolTestType);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                       
   
static CoerceToDomain *
_readCoerceToDomain(void)
{
  READ_LOCALS(CoerceToDomain);

  READ_NODE_FIELD(arg);
  READ_OID_FIELD(resulttype);
  READ_INT_FIELD(resulttypmod);
  READ_OID_FIELD(resultcollid);
  READ_ENUM_FIELD(coercionformat, CoercionForm);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                            
   
static CoerceToDomainValue *
_readCoerceToDomainValue(void)
{
  READ_LOCALS(CoerceToDomainValue);

  READ_OID_FIELD(typeId);
  READ_INT_FIELD(typeMod);
  READ_OID_FIELD(collation);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                     
   
static SetToDefault *
_readSetToDefault(void)
{
  READ_LOCALS(SetToDefault);

  READ_OID_FIELD(typeId);
  READ_INT_FIELD(typeMod);
  READ_OID_FIELD(collation);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                      
   
static CurrentOfExpr *
_readCurrentOfExpr(void)
{
  READ_LOCALS(CurrentOfExpr);

  READ_UINT_FIELD(cvarno);
  READ_STRING_FIELD(cursor_name);
  READ_INT_FIELD(cursor_param);

  READ_DONE();
}

   
                      
   
static NextValueExpr *
_readNextValueExpr(void)
{
  READ_LOCALS(NextValueExpr);

  READ_OID_FIELD(seqid);
  READ_OID_FIELD(typeId);

  READ_DONE();
}

   
                      
   
static InferenceElem *
_readInferenceElem(void)
{
  READ_LOCALS(InferenceElem);

  READ_NODE_FIELD(expr);
  READ_OID_FIELD(infercollid);
  READ_OID_FIELD(inferopclass);

  READ_DONE();
}

   
                    
   
static TargetEntry *
_readTargetEntry(void)
{
  READ_LOCALS(TargetEntry);

  READ_NODE_FIELD(expr);
  READ_INT_FIELD(resno);
  READ_STRING_FIELD(resname);
  READ_UINT_FIELD(ressortgroupref);
  READ_OID_FIELD(resorigtbl);
  READ_INT_FIELD(resorigcol);
  READ_BOOL_FIELD(resjunk);

  READ_DONE();
}

   
                    
   
static RangeTblRef *
_readRangeTblRef(void)
{
  READ_LOCALS(RangeTblRef);

  READ_INT_FIELD(rtindex);

  READ_DONE();
}

   
                 
   
static JoinExpr *
_readJoinExpr(void)
{
  READ_LOCALS(JoinExpr);

  READ_ENUM_FIELD(jointype, JoinType);
  READ_BOOL_FIELD(isNatural);
  READ_NODE_FIELD(larg);
  READ_NODE_FIELD(rarg);
  READ_NODE_FIELD(usingClause);
  READ_NODE_FIELD(quals);
  READ_NODE_FIELD(alias);
  READ_INT_FIELD(rtindex);

  READ_DONE();
}

   
                 
   
static FromExpr *
_readFromExpr(void)
{
  READ_LOCALS(FromExpr);

  READ_NODE_FIELD(fromlist);
  READ_NODE_FIELD(quals);

  READ_DONE();
}

   
                       
   
static OnConflictExpr *
_readOnConflictExpr(void)
{
  READ_LOCALS(OnConflictExpr);

  READ_ENUM_FIELD(action, OnConflictAction);
  READ_NODE_FIELD(arbiterElems);
  READ_NODE_FIELD(arbiterWhere);
  READ_OID_FIELD(constraint);
  READ_NODE_FIELD(onConflictSet);
  READ_NODE_FIELD(onConflictWhere);
  READ_INT_FIELD(exclRelIndex);
  READ_NODE_FIELD(exclRelTlist);

  READ_DONE();
}

   
                            
   

   
                      
   
static RangeTblEntry *
_readRangeTblEntry(void)
{
  READ_LOCALS(RangeTblEntry);

                                                        
  READ_NODE_FIELD(alias);
  READ_NODE_FIELD(eref);
  READ_ENUM_FIELD(rtekind, RTEKind);

  switch (local_node->rtekind)
  {
  case RTE_RELATION:
    READ_OID_FIELD(relid);
    READ_CHAR_FIELD(relkind);
    READ_INT_FIELD(rellockmode);
    READ_NODE_FIELD(tablesample);
    break;
  case RTE_SUBQUERY:
    READ_NODE_FIELD(subquery);
    READ_BOOL_FIELD(security_barrier);
    break;
  case RTE_JOIN:
    READ_ENUM_FIELD(jointype, JoinType);
    READ_NODE_FIELD(joinaliasvars);
    break;
  case RTE_FUNCTION:
    READ_NODE_FIELD(functions);
    READ_BOOL_FIELD(funcordinality);
    break;
  case RTE_TABLEFUNC:
    READ_NODE_FIELD(tablefunc);
                                                                  
    if (local_node->tablefunc)
    {
      TableFunc *tf = local_node->tablefunc;

      local_node->coltypes = tf->coltypes;
      local_node->coltypmods = tf->coltypmods;
      local_node->colcollations = tf->colcollations;
    }
    break;
  case RTE_VALUES:
    READ_NODE_FIELD(values_lists);
    READ_NODE_FIELD(coltypes);
    READ_NODE_FIELD(coltypmods);
    READ_NODE_FIELD(colcollations);
    break;
  case RTE_CTE:
    READ_STRING_FIELD(ctename);
    READ_UINT_FIELD(ctelevelsup);
    READ_BOOL_FIELD(self_reference);
    READ_NODE_FIELD(coltypes);
    READ_NODE_FIELD(coltypmods);
    READ_NODE_FIELD(colcollations);
    break;
  case RTE_NAMEDTUPLESTORE:
    READ_STRING_FIELD(enrname);
    READ_FLOAT_FIELD(enrtuples);
    READ_OID_FIELD(relid);
    READ_NODE_FIELD(coltypes);
    READ_NODE_FIELD(coltypmods);
    READ_NODE_FIELD(colcollations);
    break;
  case RTE_RESULT:
                         
    break;
  default:
    elog(ERROR, "unrecognized RTE kind: %d", (int)local_node->rtekind);
    break;
  }

  READ_BOOL_FIELD(lateral);
  READ_BOOL_FIELD(inh);
  READ_BOOL_FIELD(inFromCl);
  READ_UINT_FIELD(requiredPerms);
  READ_OID_FIELD(checkAsUser);
  READ_BITMAPSET_FIELD(selectedCols);
  READ_BITMAPSET_FIELD(insertedCols);
  READ_BITMAPSET_FIELD(updatedCols);
  READ_BITMAPSET_FIELD(extraUpdatedCols);
  READ_NODE_FIELD(securityQuals);

  READ_DONE();
}

   
                         
   
static RangeTblFunction *
_readRangeTblFunction(void)
{
  READ_LOCALS(RangeTblFunction);

  READ_NODE_FIELD(funcexpr);
  READ_INT_FIELD(funccolcount);
  READ_NODE_FIELD(funccolnames);
  READ_NODE_FIELD(funccoltypes);
  READ_NODE_FIELD(funccoltypmods);
  READ_NODE_FIELD(funccolcollations);
  READ_BITMAPSET_FIELD(funcparams);

  READ_DONE();
}

   
                          
   
static TableSampleClause *
_readTableSampleClause(void)
{
  READ_LOCALS(TableSampleClause);

  READ_OID_FIELD(tsmhandler);
  READ_NODE_FIELD(args);
  READ_NODE_FIELD(repeatable);

  READ_DONE();
}

   
                
   
static DefElem *
_readDefElem(void)
{
  READ_LOCALS(DefElem);

  READ_STRING_FIELD(defnamespace);
  READ_STRING_FIELD(defname);
  READ_NODE_FIELD(arg);
  READ_ENUM_FIELD(defaction, DefElemAction);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                           
   

   
                    
   
static PlannedStmt *
_readPlannedStmt(void)
{
  READ_LOCALS(PlannedStmt);

  READ_ENUM_FIELD(commandType, CmdType);
  READ_UINT64_FIELD(queryId);
  READ_BOOL_FIELD(hasReturning);
  READ_BOOL_FIELD(hasModifyingCTE);
  READ_BOOL_FIELD(canSetTag);
  READ_BOOL_FIELD(transientPlan);
  READ_BOOL_FIELD(dependsOnRole);
  READ_BOOL_FIELD(parallelModeNeeded);
  READ_INT_FIELD(jitFlags);
  READ_NODE_FIELD(planTree);
  READ_NODE_FIELD(rtable);
  READ_NODE_FIELD(resultRelations);
  READ_NODE_FIELD(rootResultRelations);
  READ_NODE_FIELD(subplans);
  READ_BITMAPSET_FIELD(rewindPlanIDs);
  READ_NODE_FIELD(rowMarks);
  READ_NODE_FIELD(relationOids);
  READ_NODE_FIELD(invalItems);
  READ_NODE_FIELD(paramExecTypes);
  READ_NODE_FIELD(utilityStmt);
  READ_LOCATION_FIELD(stmt_location);
  READ_LOCATION_FIELD(stmt_len);

  READ_DONE();
}

   
                  
                                                              
   
static void
ReadCommonPlan(Plan *local_node)
{
  READ_TEMP_LOCALS();

  READ_FLOAT_FIELD(startup_cost);
  READ_FLOAT_FIELD(total_cost);
  READ_FLOAT_FIELD(plan_rows);
  READ_INT_FIELD(plan_width);
  READ_BOOL_FIELD(parallel_aware);
  READ_BOOL_FIELD(parallel_safe);
  READ_INT_FIELD(plan_node_id);
  READ_NODE_FIELD(targetlist);
  READ_NODE_FIELD(qual);
  READ_NODE_FIELD(lefttree);
  READ_NODE_FIELD(righttree);
  READ_NODE_FIELD(initPlan);
  READ_BITMAPSET_FIELD(extParam);
  READ_BITMAPSET_FIELD(allParam);
}

   
             
   
static Plan *
_readPlan(void)
{
  READ_LOCALS_NO_FIELDS(Plan);

  ReadCommonPlan(local_node);

  READ_DONE();
}

   
               
   
static Result *
_readResult(void)
{
  READ_LOCALS(Result);

  ReadCommonPlan(&local_node->plan);

  READ_NODE_FIELD(resconstantqual);

  READ_DONE();
}

   
                   
   
static ProjectSet *
_readProjectSet(void)
{
  READ_LOCALS_NO_FIELDS(ProjectSet);

  ReadCommonPlan(&local_node->plan);

  READ_DONE();
}

   
                    
   
static ModifyTable *
_readModifyTable(void)
{
  READ_LOCALS(ModifyTable);

  ReadCommonPlan(&local_node->plan);

  READ_ENUM_FIELD(operation, CmdType);
  READ_BOOL_FIELD(canSetTag);
  READ_UINT_FIELD(nominalRelation);
  READ_UINT_FIELD(rootRelation);
  READ_BOOL_FIELD(partColsUpdated);
  READ_NODE_FIELD(resultRelations);
  READ_INT_FIELD(resultRelIndex);
  READ_INT_FIELD(rootResultRelIndex);
  READ_NODE_FIELD(plans);
  READ_NODE_FIELD(withCheckOptionLists);
  READ_NODE_FIELD(returningLists);
  READ_NODE_FIELD(fdwPrivLists);
  READ_BITMAPSET_FIELD(fdwDirectModifyPlans);
  READ_NODE_FIELD(rowMarks);
  READ_INT_FIELD(epqParam);
  READ_ENUM_FIELD(onConflictAction, OnConflictAction);
  READ_NODE_FIELD(arbiterIndexes);
  READ_NODE_FIELD(onConflictSet);
  READ_NODE_FIELD(onConflictWhere);
  READ_UINT_FIELD(exclRelRTI);
  READ_NODE_FIELD(exclRelTlist);

  READ_DONE();
}

   
               
   
static Append *
_readAppend(void)
{
  READ_LOCALS(Append);

  ReadCommonPlan(&local_node->plan);

  READ_NODE_FIELD(appendplans);
  READ_INT_FIELD(first_partial_plan);
  READ_NODE_FIELD(part_prune_info);

  READ_DONE();
}

   
                    
   
static MergeAppend *
_readMergeAppend(void)
{
  READ_LOCALS(MergeAppend);

  ReadCommonPlan(&local_node->plan);

  READ_NODE_FIELD(mergeplans);
  READ_INT_FIELD(numCols);
  READ_ATTRNUMBER_ARRAY(sortColIdx, local_node->numCols);
  READ_OID_ARRAY(sortOperators, local_node->numCols);
  READ_OID_ARRAY(collations, local_node->numCols);
  READ_BOOL_ARRAY(nullsFirst, local_node->numCols);
  READ_NODE_FIELD(part_prune_info);

  READ_DONE();
}

   
                       
   
static RecursiveUnion *
_readRecursiveUnion(void)
{
  READ_LOCALS(RecursiveUnion);

  ReadCommonPlan(&local_node->plan);

  READ_INT_FIELD(wtParam);
  READ_INT_FIELD(numCols);
  READ_ATTRNUMBER_ARRAY(dupColIdx, local_node->numCols);
  READ_OID_ARRAY(dupOperators, local_node->numCols);
  READ_OID_ARRAY(dupCollations, local_node->numCols);
  READ_LONG_FIELD(numGroups);

  READ_DONE();
}

   
                  
   
static BitmapAnd *
_readBitmapAnd(void)
{
  READ_LOCALS(BitmapAnd);

  ReadCommonPlan(&local_node->plan);

  READ_NODE_FIELD(bitmapplans);

  READ_DONE();
}

   
                 
   
static BitmapOr *
_readBitmapOr(void)
{
  READ_LOCALS(BitmapOr);

  ReadCommonPlan(&local_node->plan);

  READ_BOOL_FIELD(isshared);
  READ_NODE_FIELD(bitmapplans);

  READ_DONE();
}

   
                  
                                                              
   
static void
ReadCommonScan(Scan *local_node)
{
  READ_TEMP_LOCALS();

  ReadCommonPlan(&local_node->plan);

  READ_UINT_FIELD(scanrelid);
}

   
             
   
static Scan *
_readScan(void)
{
  READ_LOCALS_NO_FIELDS(Scan);

  ReadCommonScan(local_node);

  READ_DONE();
}

   
                
   
static SeqScan *
_readSeqScan(void)
{
  READ_LOCALS_NO_FIELDS(SeqScan);

  ReadCommonScan(local_node);

  READ_DONE();
}

   
                   
   
static SampleScan *
_readSampleScan(void)
{
  READ_LOCALS(SampleScan);

  ReadCommonScan(&local_node->scan);

  READ_NODE_FIELD(tablesample);

  READ_DONE();
}

   
                  
   
static IndexScan *
_readIndexScan(void)
{
  READ_LOCALS(IndexScan);

  ReadCommonScan(&local_node->scan);

  READ_OID_FIELD(indexid);
  READ_NODE_FIELD(indexqual);
  READ_NODE_FIELD(indexqualorig);
  READ_NODE_FIELD(indexorderby);
  READ_NODE_FIELD(indexorderbyorig);
  READ_NODE_FIELD(indexorderbyops);
  READ_ENUM_FIELD(indexorderdir, ScanDirection);

  READ_DONE();
}

   
                      
   
static IndexOnlyScan *
_readIndexOnlyScan(void)
{
  READ_LOCALS(IndexOnlyScan);

  ReadCommonScan(&local_node->scan);

  READ_OID_FIELD(indexid);
  READ_NODE_FIELD(indexqual);
  READ_NODE_FIELD(recheckqual);
  READ_NODE_FIELD(indexorderby);
  READ_NODE_FIELD(indextlist);
  READ_ENUM_FIELD(indexorderdir, ScanDirection);

  READ_DONE();
}

   
                        
   
static BitmapIndexScan *
_readBitmapIndexScan(void)
{
  READ_LOCALS(BitmapIndexScan);

  ReadCommonScan(&local_node->scan);

  READ_OID_FIELD(indexid);
  READ_BOOL_FIELD(isshared);
  READ_NODE_FIELD(indexqual);
  READ_NODE_FIELD(indexqualorig);

  READ_DONE();
}

   
                       
   
static BitmapHeapScan *
_readBitmapHeapScan(void)
{
  READ_LOCALS(BitmapHeapScan);

  ReadCommonScan(&local_node->scan);

  READ_NODE_FIELD(bitmapqualorig);

  READ_DONE();
}

   
                
   
static TidScan *
_readTidScan(void)
{
  READ_LOCALS(TidScan);

  ReadCommonScan(&local_node->scan);

  READ_NODE_FIELD(tidquals);

  READ_DONE();
}

   
                     
   
static SubqueryScan *
_readSubqueryScan(void)
{
  READ_LOCALS(SubqueryScan);

  ReadCommonScan(&local_node->scan);

  READ_NODE_FIELD(subplan);

  READ_DONE();
}

   
                     
   
static FunctionScan *
_readFunctionScan(void)
{
  READ_LOCALS(FunctionScan);

  ReadCommonScan(&local_node->scan);

  READ_NODE_FIELD(functions);
  READ_BOOL_FIELD(funcordinality);

  READ_DONE();
}

   
                   
   
static ValuesScan *
_readValuesScan(void)
{
  READ_LOCALS(ValuesScan);

  ReadCommonScan(&local_node->scan);

  READ_NODE_FIELD(values_lists);

  READ_DONE();
}

   
                      
   
static TableFuncScan *
_readTableFuncScan(void)
{
  READ_LOCALS(TableFuncScan);

  ReadCommonScan(&local_node->scan);

  READ_NODE_FIELD(tablefunc);

  READ_DONE();
}

   
                
   
static CteScan *
_readCteScan(void)
{
  READ_LOCALS(CteScan);

  ReadCommonScan(&local_node->scan);

  READ_INT_FIELD(ctePlanId);
  READ_INT_FIELD(cteParam);

  READ_DONE();
}

   
                            
   
static NamedTuplestoreScan *
_readNamedTuplestoreScan(void)
{
  READ_LOCALS(NamedTuplestoreScan);

  ReadCommonScan(&local_node->scan);

  READ_STRING_FIELD(enrname);

  READ_DONE();
}

   
                      
   
static WorkTableScan *
_readWorkTableScan(void)
{
  READ_LOCALS(WorkTableScan);

  ReadCommonScan(&local_node->scan);

  READ_INT_FIELD(wtParam);

  READ_DONE();
}

   
                    
   
static ForeignScan *
_readForeignScan(void)
{
  READ_LOCALS(ForeignScan);

  ReadCommonScan(&local_node->scan);

  READ_ENUM_FIELD(operation, CmdType);
  READ_OID_FIELD(fs_server);
  READ_NODE_FIELD(fdw_exprs);
  READ_NODE_FIELD(fdw_private);
  READ_NODE_FIELD(fdw_scan_tlist);
  READ_NODE_FIELD(fdw_recheck_quals);
  READ_BITMAPSET_FIELD(fs_relids);
  READ_BOOL_FIELD(fsSystemCol);

  READ_DONE();
}

   
                   
   
static CustomScan *
_readCustomScan(void)
{
  READ_LOCALS(CustomScan);
  char *custom_name;
  const CustomScanMethods *methods;

  ReadCommonScan(&local_node->scan);

  READ_UINT_FIELD(flags);
  READ_NODE_FIELD(custom_plans);
  READ_NODE_FIELD(custom_exprs);
  READ_NODE_FIELD(custom_private);
  READ_NODE_FIELD(custom_scan_tlist);
  READ_BITMAPSET_FIELD(custom_relids);

                                              
  token = pg_strtok(&length);                    
  token = pg_strtok(&length);                 
  custom_name = nullable_string(token, length);
  methods = GetCustomScanMethods(custom_name, false);
  local_node->methods = methods;

  READ_DONE();
}

   
                  
                                                              
   
static void
ReadCommonJoin(Join *local_node)
{
  READ_TEMP_LOCALS();

  ReadCommonPlan(&local_node->plan);

  READ_ENUM_FIELD(jointype, JoinType);
  READ_BOOL_FIELD(inner_unique);
  READ_NODE_FIELD(joinqual);
}

   
             
   
static Join *
_readJoin(void)
{
  READ_LOCALS_NO_FIELDS(Join);

  ReadCommonJoin(local_node);

  READ_DONE();
}

   
                 
   
static NestLoop *
_readNestLoop(void)
{
  READ_LOCALS(NestLoop);

  ReadCommonJoin(&local_node->join);

  READ_NODE_FIELD(nestParams);

  READ_DONE();
}

   
                  
   
static MergeJoin *
_readMergeJoin(void)
{
  int numCols;

  READ_LOCALS(MergeJoin);

  ReadCommonJoin(&local_node->join);

  READ_BOOL_FIELD(skip_mark_restore);
  READ_NODE_FIELD(mergeclauses);

  numCols = list_length(local_node->mergeclauses);

  READ_OID_ARRAY(mergeFamilies, numCols);
  READ_OID_ARRAY(mergeCollations, numCols);
  READ_INT_ARRAY(mergeStrategies, numCols);
  READ_BOOL_ARRAY(mergeNullsFirst, numCols);

  READ_DONE();
}

   
                 
   
static HashJoin *
_readHashJoin(void)
{
  READ_LOCALS(HashJoin);

  ReadCommonJoin(&local_node->join);

  READ_NODE_FIELD(hashclauses);
  READ_NODE_FIELD(hashoperators);
  READ_NODE_FIELD(hashcollations);
  READ_NODE_FIELD(hashkeys);

  READ_DONE();
}

   
                 
   
static Material *
_readMaterial(void)
{
  READ_LOCALS_NO_FIELDS(Material);

  ReadCommonPlan(&local_node->plan);

  READ_DONE();
}

   
             
   
static Sort *
_readSort(void)
{
  READ_LOCALS(Sort);

  ReadCommonPlan(&local_node->plan);

  READ_INT_FIELD(numCols);
  READ_ATTRNUMBER_ARRAY(sortColIdx, local_node->numCols);
  READ_OID_ARRAY(sortOperators, local_node->numCols);
  READ_OID_ARRAY(collations, local_node->numCols);
  READ_BOOL_ARRAY(nullsFirst, local_node->numCols);

  READ_DONE();
}

   
              
   
static Group *
_readGroup(void)
{
  READ_LOCALS(Group);

  ReadCommonPlan(&local_node->plan);

  READ_INT_FIELD(numCols);
  READ_ATTRNUMBER_ARRAY(grpColIdx, local_node->numCols);
  READ_OID_ARRAY(grpOperators, local_node->numCols);
  READ_OID_ARRAY(grpCollations, local_node->numCols);

  READ_DONE();
}

   
            
   
static Agg *
_readAgg(void)
{
  READ_LOCALS(Agg);

  ReadCommonPlan(&local_node->plan);

  READ_ENUM_FIELD(aggstrategy, AggStrategy);
  READ_ENUM_FIELD(aggsplit, AggSplit);
  READ_INT_FIELD(numCols);
  READ_ATTRNUMBER_ARRAY(grpColIdx, local_node->numCols);
  READ_OID_ARRAY(grpOperators, local_node->numCols);
  READ_OID_ARRAY(grpCollations, local_node->numCols);
  READ_LONG_FIELD(numGroups);
  READ_BITMAPSET_FIELD(aggParams);
  READ_NODE_FIELD(groupingSets);
  READ_NODE_FIELD(chain);

  READ_DONE();
}

   
                  
   
static WindowAgg *
_readWindowAgg(void)
{
  READ_LOCALS(WindowAgg);

  ReadCommonPlan(&local_node->plan);

  READ_UINT_FIELD(winref);
  READ_INT_FIELD(partNumCols);
  READ_ATTRNUMBER_ARRAY(partColIdx, local_node->partNumCols);
  READ_OID_ARRAY(partOperators, local_node->partNumCols);
  READ_OID_ARRAY(partCollations, local_node->partNumCols);
  READ_INT_FIELD(ordNumCols);
  READ_ATTRNUMBER_ARRAY(ordColIdx, local_node->ordNumCols);
  READ_OID_ARRAY(ordOperators, local_node->ordNumCols);
  READ_OID_ARRAY(ordCollations, local_node->ordNumCols);
  READ_INT_FIELD(frameOptions);
  READ_NODE_FIELD(startOffset);
  READ_NODE_FIELD(endOffset);
  READ_OID_FIELD(startInRangeFunc);
  READ_OID_FIELD(endInRangeFunc);
  READ_OID_FIELD(inRangeColl);
  READ_BOOL_FIELD(inRangeAsc);
  READ_BOOL_FIELD(inRangeNullsFirst);

  READ_DONE();
}

   
               
   
static Unique *
_readUnique(void)
{
  READ_LOCALS(Unique);

  ReadCommonPlan(&local_node->plan);

  READ_INT_FIELD(numCols);
  READ_ATTRNUMBER_ARRAY(uniqColIdx, local_node->numCols);
  READ_OID_ARRAY(uniqOperators, local_node->numCols);
  READ_OID_ARRAY(uniqCollations, local_node->numCols);

  READ_DONE();
}

   
               
   
static Gather *
_readGather(void)
{
  READ_LOCALS(Gather);

  ReadCommonPlan(&local_node->plan);

  READ_INT_FIELD(num_workers);
  READ_INT_FIELD(rescan_param);
  READ_BOOL_FIELD(single_copy);
  READ_BOOL_FIELD(invisible);
  READ_BITMAPSET_FIELD(initParam);

  READ_DONE();
}

   
                    
   
static GatherMerge *
_readGatherMerge(void)
{
  READ_LOCALS(GatherMerge);

  ReadCommonPlan(&local_node->plan);

  READ_INT_FIELD(num_workers);
  READ_INT_FIELD(rescan_param);
  READ_INT_FIELD(numCols);
  READ_ATTRNUMBER_ARRAY(sortColIdx, local_node->numCols);
  READ_OID_ARRAY(sortOperators, local_node->numCols);
  READ_OID_ARRAY(collations, local_node->numCols);
  READ_BOOL_ARRAY(nullsFirst, local_node->numCols);
  READ_BITMAPSET_FIELD(initParam);

  READ_DONE();
}

   
             
   
static Hash *
_readHash(void)
{
  READ_LOCALS(Hash);

  ReadCommonPlan(&local_node->plan);

  READ_NODE_FIELD(hashkeys);
  READ_OID_FIELD(skewTable);
  READ_INT_FIELD(skewColumn);
  READ_BOOL_FIELD(skewInherit);
  READ_FLOAT_FIELD(rows_total);

  READ_DONE();
}

   
              
   
static SetOp *
_readSetOp(void)
{
  READ_LOCALS(SetOp);

  ReadCommonPlan(&local_node->plan);

  READ_ENUM_FIELD(cmd, SetOpCmd);
  READ_ENUM_FIELD(strategy, SetOpStrategy);
  READ_INT_FIELD(numCols);
  READ_ATTRNUMBER_ARRAY(dupColIdx, local_node->numCols);
  READ_OID_ARRAY(dupOperators, local_node->numCols);
  READ_OID_ARRAY(dupCollations, local_node->numCols);
  READ_INT_FIELD(flagColIdx);
  READ_INT_FIELD(firstFlag);
  READ_LONG_FIELD(numGroups);

  READ_DONE();
}

   
                 
   
static LockRows *
_readLockRows(void)
{
  READ_LOCALS(LockRows);

  ReadCommonPlan(&local_node->plan);

  READ_NODE_FIELD(rowMarks);
  READ_INT_FIELD(epqParam);

  READ_DONE();
}

   
              
   
static Limit *
_readLimit(void)
{
  READ_LOCALS(Limit);

  ReadCommonPlan(&local_node->plan);

  READ_NODE_FIELD(limitOffset);
  READ_NODE_FIELD(limitCount);

  READ_DONE();
}

   
                      
   
static NestLoopParam *
_readNestLoopParam(void)
{
  READ_LOCALS(NestLoopParam);

  READ_INT_FIELD(paramno);
  READ_NODE_FIELD(paramval);

  READ_DONE();
}

   
                    
   
static PlanRowMark *
_readPlanRowMark(void)
{
  READ_LOCALS(PlanRowMark);

  READ_UINT_FIELD(rti);
  READ_UINT_FIELD(prti);
  READ_UINT_FIELD(rowmarkId);
  READ_ENUM_FIELD(markType, RowMarkType);
  READ_INT_FIELD(allMarkTypes);
  READ_ENUM_FIELD(strength, LockClauseStrength);
  READ_ENUM_FIELD(waitPolicy, LockWaitPolicy);
  READ_BOOL_FIELD(isParent);

  READ_DONE();
}

static PartitionPruneInfo *
_readPartitionPruneInfo(void)
{
  READ_LOCALS(PartitionPruneInfo);

  READ_NODE_FIELD(prune_infos);
  READ_BITMAPSET_FIELD(other_subplans);

  READ_DONE();
}

static PartitionedRelPruneInfo *
_readPartitionedRelPruneInfo(void)
{
  READ_LOCALS(PartitionedRelPruneInfo);

  READ_UINT_FIELD(rtindex);
  READ_BITMAPSET_FIELD(present_parts);
  READ_INT_FIELD(nparts);
  READ_INT_ARRAY(subplan_map, local_node->nparts);
  READ_INT_ARRAY(subpart_map, local_node->nparts);
  READ_OID_ARRAY(relid_map, local_node->nparts);
  READ_NODE_FIELD(initial_pruning_steps);
  READ_NODE_FIELD(exec_pruning_steps);
  READ_BITMAPSET_FIELD(execparamids);

  READ_DONE();
}

static PartitionPruneStepOp *
_readPartitionPruneStepOp(void)
{
  READ_LOCALS(PartitionPruneStepOp);

  READ_INT_FIELD(step.step_id);
  READ_INT_FIELD(opstrategy);
  READ_NODE_FIELD(exprs);
  READ_NODE_FIELD(cmpfns);
  READ_BITMAPSET_FIELD(nullkeys);

  READ_DONE();
}

static PartitionPruneStepCombine *
_readPartitionPruneStepCombine(void)
{
  READ_LOCALS(PartitionPruneStepCombine);

  READ_INT_FIELD(step.step_id);
  READ_ENUM_FIELD(combineOp, PartitionPruneCombineOp);
  READ_NODE_FIELD(source_stepids);

  READ_DONE();
}

   
                      
   
static PlanInvalItem *
_readPlanInvalItem(void)
{
  READ_LOCALS(PlanInvalItem);

  READ_INT_FIELD(cacheId);
  READ_UINT_FIELD(hashValue);

  READ_DONE();
}

   
                
   
static SubPlan *
_readSubPlan(void)
{
  READ_LOCALS(SubPlan);

  READ_ENUM_FIELD(subLinkType, SubLinkType);
  READ_NODE_FIELD(testexpr);
  READ_NODE_FIELD(paramIds);
  READ_INT_FIELD(plan_id);
  READ_STRING_FIELD(plan_name);
  READ_OID_FIELD(firstColType);
  READ_INT_FIELD(firstColTypmod);
  READ_OID_FIELD(firstColCollation);
  READ_BOOL_FIELD(useHashTable);
  READ_BOOL_FIELD(unknownEqFalse);
  READ_BOOL_FIELD(parallel_safe);
  READ_NODE_FIELD(setParam);
  READ_NODE_FIELD(parParam);
  READ_NODE_FIELD(args);
  READ_FLOAT_FIELD(startup_cost);
  READ_FLOAT_FIELD(per_call_cost);
  READ_INT_FIELD(subLinkId);

  READ_DONE();
}

   
                           
   
static AlternativeSubPlan *
_readAlternativeSubPlan(void)
{
  READ_LOCALS(AlternativeSubPlan);

  READ_NODE_FIELD(subplans);

  READ_DONE();
}

   
                       
   
static ExtensibleNode *
_readExtensibleNode(void)
{
  const ExtensibleNodeMethods *methods;
  ExtensibleNode *local_node;
  const char *extnodename;

  READ_TEMP_LOCALS();

  token = pg_strtok(&length);                        
  token = pg_strtok(&length);                      

  extnodename = nullable_string(token, length);
  if (!extnodename)
  {
    elog(ERROR, "extnodename has to be supplied");
  }
  methods = GetExtensibleNodeMethods(extnodename, false);

  local_node = (ExtensibleNode *)newNode(methods->node_size, T_ExtensibleNode);
  local_node->extnodename = extnodename;

                                      
  methods->nodeRead(local_node);

  READ_DONE();
}

   
                           
   
static PartitionBoundSpec *
_readPartitionBoundSpec(void)
{
  READ_LOCALS(PartitionBoundSpec);

  READ_CHAR_FIELD(strategy);
  READ_BOOL_FIELD(is_default);
  READ_INT_FIELD(modulus);
  READ_INT_FIELD(remainder);
  READ_NODE_FIELD(listdatums);
  READ_NODE_FIELD(lowerdatums);
  READ_NODE_FIELD(upperdatums);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                            
   
static PartitionRangeDatum *
_readPartitionRangeDatum(void)
{
  READ_LOCALS(PartitionRangeDatum);

  READ_ENUM_FIELD(kind, PartitionRangeDatumKind);
  READ_NODE_FIELD(value);
  READ_LOCATION_FIELD(location);

  READ_DONE();
}

   
                   
   
                                                                              
                                
   
                                                                         
   
Node *
parseNodeString(void)
{
  void *return_value;

  READ_TEMP_LOCALS();

                                                                      
  check_stack_depth();

  token = pg_strtok(&length);

#define MATCH(tokname, namelen) (length == namelen && memcmp(token, tokname, namelen) == 0)

  if (MATCH("QUERY", 5))
  {
    return_value = _readQuery();
  }
  else if (MATCH("WITHCHECKOPTION", 15))
  {
    return_value = _readWithCheckOption();
  }
  else if (MATCH("SORTGROUPCLAUSE", 15))
  {
    return_value = _readSortGroupClause();
  }
  else if (MATCH("GROUPINGSET", 11))
  {
    return_value = _readGroupingSet();
  }
  else if (MATCH("WINDOWCLAUSE", 12))
  {
    return_value = _readWindowClause();
  }
  else if (MATCH("ROWMARKCLAUSE", 13))
  {
    return_value = _readRowMarkClause();
  }
  else if (MATCH("COMMONTABLEEXPR", 15))
  {
    return_value = _readCommonTableExpr();
  }
  else if (MATCH("SETOPERATIONSTMT", 16))
  {
    return_value = _readSetOperationStmt();
  }
  else if (MATCH("ALIAS", 5))
  {
    return_value = _readAlias();
  }
  else if (MATCH("RANGEVAR", 8))
  {
    return_value = _readRangeVar();
  }
  else if (MATCH("INTOCLAUSE", 10))
  {
    return_value = _readIntoClause();
  }
  else if (MATCH("TABLEFUNC", 9))
  {
    return_value = _readTableFunc();
  }
  else if (MATCH("VAR", 3))
  {
    return_value = _readVar();
  }
  else if (MATCH("CONST", 5))
  {
    return_value = _readConst();
  }
  else if (MATCH("PARAM", 5))
  {
    return_value = _readParam();
  }
  else if (MATCH("AGGREF", 6))
  {
    return_value = _readAggref();
  }
  else if (MATCH("GROUPINGFUNC", 12))
  {
    return_value = _readGroupingFunc();
  }
  else if (MATCH("WINDOWFUNC", 10))
  {
    return_value = _readWindowFunc();
  }
  else if (MATCH("SUBSCRIPTINGREF", 15))
  {
    return_value = _readSubscriptingRef();
  }
  else if (MATCH("FUNCEXPR", 8))
  {
    return_value = _readFuncExpr();
  }
  else if (MATCH("NAMEDARGEXPR", 12))
  {
    return_value = _readNamedArgExpr();
  }
  else if (MATCH("OPEXPR", 6))
  {
    return_value = _readOpExpr();
  }
  else if (MATCH("DISTINCTEXPR", 12))
  {
    return_value = _readDistinctExpr();
  }
  else if (MATCH("NULLIFEXPR", 10))
  {
    return_value = _readNullIfExpr();
  }
  else if (MATCH("SCALARARRAYOPEXPR", 17))
  {
    return_value = _readScalarArrayOpExpr();
  }
  else if (MATCH("BOOLEXPR", 8))
  {
    return_value = _readBoolExpr();
  }
  else if (MATCH("SUBLINK", 7))
  {
    return_value = _readSubLink();
  }
  else if (MATCH("FIELDSELECT", 11))
  {
    return_value = _readFieldSelect();
  }
  else if (MATCH("FIELDSTORE", 10))
  {
    return_value = _readFieldStore();
  }
  else if (MATCH("RELABELTYPE", 11))
  {
    return_value = _readRelabelType();
  }
  else if (MATCH("COERCEVIAIO", 11))
  {
    return_value = _readCoerceViaIO();
  }
  else if (MATCH("ARRAYCOERCEEXPR", 15))
  {
    return_value = _readArrayCoerceExpr();
  }
  else if (MATCH("CONVERTROWTYPEEXPR", 18))
  {
    return_value = _readConvertRowtypeExpr();
  }
  else if (MATCH("COLLATE", 7))
  {
    return_value = _readCollateExpr();
  }
  else if (MATCH("CASE", 4))
  {
    return_value = _readCaseExpr();
  }
  else if (MATCH("WHEN", 4))
  {
    return_value = _readCaseWhen();
  }
  else if (MATCH("CASETESTEXPR", 12))
  {
    return_value = _readCaseTestExpr();
  }
  else if (MATCH("ARRAY", 5))
  {
    return_value = _readArrayExpr();
  }
  else if (MATCH("ROW", 3))
  {
    return_value = _readRowExpr();
  }
  else if (MATCH("ROWCOMPARE", 10))
  {
    return_value = _readRowCompareExpr();
  }
  else if (MATCH("COALESCE", 8))
  {
    return_value = _readCoalesceExpr();
  }
  else if (MATCH("MINMAX", 6))
  {
    return_value = _readMinMaxExpr();
  }
  else if (MATCH("SQLVALUEFUNCTION", 16))
  {
    return_value = _readSQLValueFunction();
  }
  else if (MATCH("XMLEXPR", 7))
  {
    return_value = _readXmlExpr();
  }
  else if (MATCH("NULLTEST", 8))
  {
    return_value = _readNullTest();
  }
  else if (MATCH("BOOLEANTEST", 11))
  {
    return_value = _readBooleanTest();
  }
  else if (MATCH("COERCETODOMAIN", 14))
  {
    return_value = _readCoerceToDomain();
  }
  else if (MATCH("COERCETODOMAINVALUE", 19))
  {
    return_value = _readCoerceToDomainValue();
  }
  else if (MATCH("SETTODEFAULT", 12))
  {
    return_value = _readSetToDefault();
  }
  else if (MATCH("CURRENTOFEXPR", 13))
  {
    return_value = _readCurrentOfExpr();
  }
  else if (MATCH("NEXTVALUEEXPR", 13))
  {
    return_value = _readNextValueExpr();
  }
  else if (MATCH("INFERENCEELEM", 13))
  {
    return_value = _readInferenceElem();
  }
  else if (MATCH("TARGETENTRY", 11))
  {
    return_value = _readTargetEntry();
  }
  else if (MATCH("RANGETBLREF", 11))
  {
    return_value = _readRangeTblRef();
  }
  else if (MATCH("JOINEXPR", 8))
  {
    return_value = _readJoinExpr();
  }
  else if (MATCH("FROMEXPR", 8))
  {
    return_value = _readFromExpr();
  }
  else if (MATCH("ONCONFLICTEXPR", 14))
  {
    return_value = _readOnConflictExpr();
  }
  else if (MATCH("RTE", 3))
  {
    return_value = _readRangeTblEntry();
  }
  else if (MATCH("RANGETBLFUNCTION", 16))
  {
    return_value = _readRangeTblFunction();
  }
  else if (MATCH("TABLESAMPLECLAUSE", 17))
  {
    return_value = _readTableSampleClause();
  }
  else if (MATCH("NOTIFY", 6))
  {
    return_value = _readNotifyStmt();
  }
  else if (MATCH("DEFELEM", 7))
  {
    return_value = _readDefElem();
  }
  else if (MATCH("DECLARECURSOR", 13))
  {
    return_value = _readDeclareCursorStmt();
  }
  else if (MATCH("PLANNEDSTMT", 11))
  {
    return_value = _readPlannedStmt();
  }
  else if (MATCH("PLAN", 4))
  {
    return_value = _readPlan();
  }
  else if (MATCH("RESULT", 6))
  {
    return_value = _readResult();
  }
  else if (MATCH("PROJECTSET", 10))
  {
    return_value = _readProjectSet();
  }
  else if (MATCH("MODIFYTABLE", 11))
  {
    return_value = _readModifyTable();
  }
  else if (MATCH("APPEND", 6))
  {
    return_value = _readAppend();
  }
  else if (MATCH("MERGEAPPEND", 11))
  {
    return_value = _readMergeAppend();
  }
  else if (MATCH("RECURSIVEUNION", 14))
  {
    return_value = _readRecursiveUnion();
  }
  else if (MATCH("BITMAPAND", 9))
  {
    return_value = _readBitmapAnd();
  }
  else if (MATCH("BITMAPOR", 8))
  {
    return_value = _readBitmapOr();
  }
  else if (MATCH("SCAN", 4))
  {
    return_value = _readScan();
  }
  else if (MATCH("SEQSCAN", 7))
  {
    return_value = _readSeqScan();
  }
  else if (MATCH("SAMPLESCAN", 10))
  {
    return_value = _readSampleScan();
  }
  else if (MATCH("INDEXSCAN", 9))
  {
    return_value = _readIndexScan();
  }
  else if (MATCH("INDEXONLYSCAN", 13))
  {
    return_value = _readIndexOnlyScan();
  }
  else if (MATCH("BITMAPINDEXSCAN", 15))
  {
    return_value = _readBitmapIndexScan();
  }
  else if (MATCH("BITMAPHEAPSCAN", 14))
  {
    return_value = _readBitmapHeapScan();
  }
  else if (MATCH("TIDSCAN", 7))
  {
    return_value = _readTidScan();
  }
  else if (MATCH("SUBQUERYSCAN", 12))
  {
    return_value = _readSubqueryScan();
  }
  else if (MATCH("FUNCTIONSCAN", 12))
  {
    return_value = _readFunctionScan();
  }
  else if (MATCH("VALUESSCAN", 10))
  {
    return_value = _readValuesScan();
  }
  else if (MATCH("TABLEFUNCSCAN", 13))
  {
    return_value = _readTableFuncScan();
  }
  else if (MATCH("CTESCAN", 7))
  {
    return_value = _readCteScan();
  }
  else if (MATCH("NAMEDTUPLESTORESCAN", 19))
  {
    return_value = _readNamedTuplestoreScan();
  }
  else if (MATCH("WORKTABLESCAN", 13))
  {
    return_value = _readWorkTableScan();
  }
  else if (MATCH("FOREIGNSCAN", 11))
  {
    return_value = _readForeignScan();
  }
  else if (MATCH("CUSTOMSCAN", 10))
  {
    return_value = _readCustomScan();
  }
  else if (MATCH("JOIN", 4))
  {
    return_value = _readJoin();
  }
  else if (MATCH("NESTLOOP", 8))
  {
    return_value = _readNestLoop();
  }
  else if (MATCH("MERGEJOIN", 9))
  {
    return_value = _readMergeJoin();
  }
  else if (MATCH("HASHJOIN", 8))
  {
    return_value = _readHashJoin();
  }
  else if (MATCH("MATERIAL", 8))
  {
    return_value = _readMaterial();
  }
  else if (MATCH("SORT", 4))
  {
    return_value = _readSort();
  }
  else if (MATCH("GROUP", 5))
  {
    return_value = _readGroup();
  }
  else if (MATCH("AGG", 3))
  {
    return_value = _readAgg();
  }
  else if (MATCH("WINDOWAGG", 9))
  {
    return_value = _readWindowAgg();
  }
  else if (MATCH("UNIQUE", 6))
  {
    return_value = _readUnique();
  }
  else if (MATCH("GATHER", 6))
  {
    return_value = _readGather();
  }
  else if (MATCH("GATHERMERGE", 11))
  {
    return_value = _readGatherMerge();
  }
  else if (MATCH("HASH", 4))
  {
    return_value = _readHash();
  }
  else if (MATCH("SETOP", 5))
  {
    return_value = _readSetOp();
  }
  else if (MATCH("LOCKROWS", 8))
  {
    return_value = _readLockRows();
  }
  else if (MATCH("LIMIT", 5))
  {
    return_value = _readLimit();
  }
  else if (MATCH("NESTLOOPPARAM", 13))
  {
    return_value = _readNestLoopParam();
  }
  else if (MATCH("PLANROWMARK", 11))
  {
    return_value = _readPlanRowMark();
  }
  else if (MATCH("PARTITIONPRUNEINFO", 18))
  {
    return_value = _readPartitionPruneInfo();
  }
  else if (MATCH("PARTITIONEDRELPRUNEINFO", 23))
  {
    return_value = _readPartitionedRelPruneInfo();
  }
  else if (MATCH("PARTITIONPRUNESTEPOP", 20))
  {
    return_value = _readPartitionPruneStepOp();
  }
  else if (MATCH("PARTITIONPRUNESTEPCOMBINE", 25))
  {
    return_value = _readPartitionPruneStepCombine();
  }
  else if (MATCH("PLANINVALITEM", 13))
  {
    return_value = _readPlanInvalItem();
  }
  else if (MATCH("SUBPLAN", 7))
  {
    return_value = _readSubPlan();
  }
  else if (MATCH("ALTERNATIVESUBPLAN", 18))
  {
    return_value = _readAlternativeSubPlan();
  }
  else if (MATCH("EXTENSIBLENODE", 14))
  {
    return_value = _readExtensibleNode();
  }
  else if (MATCH("PARTITIONBOUNDSPEC", 18))
  {
    return_value = _readPartitionBoundSpec();
  }
  else if (MATCH("PARTITIONRANGEDATUM", 19))
  {
    return_value = _readPartitionRangeDatum();
  }
  else
  {
    elog(ERROR, "badly formatted node string \"%.32s\"...", token);
    return_value = NULL;                          
  }

  return (Node *)return_value;
}

   
             
   
                                                                         
                                                                          
                            
   
Datum
readDatum(bool typbyval)
{
  Size length, i;
  int tokenLength;
  const char *token;
  Datum res;
  char *s;

     
                                         
     
  token = pg_strtok(&tokenLength);
  length = atoui(token);

  token = pg_strtok(&tokenLength);                   
  if (token == NULL || token[0] != '[')
  {
    elog(ERROR, "expected \"[\" to start datum, but got \"%s\"; length = %zu", token ? token : "[NULL]", length);
  }

  if (typbyval)
  {
    if (length > (Size)sizeof(Datum))
    {
      elog(ERROR, "byval datum but length = %zu", length);
    }
    res = (Datum)0;
    s = (char *)(&res);
    for (i = 0; i < (Size)sizeof(Datum); i++)
    {
      token = pg_strtok(&tokenLength);
      s[i] = (char)atoi(token);
    }
  }
  else if (length <= 0)
  {
    res = (Datum)NULL;
  }
  else
  {
    s = (char *)palloc(length);
    for (i = 0; i < length; i++)
    {
      token = pg_strtok(&tokenLength);
      s[i] = (char)atoi(token);
    }
    res = PointerGetDatum(s);
  }

  token = pg_strtok(&tokenLength);                   
  if (token == NULL || token[0] != ']')
  {
    elog(ERROR, "expected \"]\" to end datum, but got \"%s\"; length = %zu", token ? token : "[NULL]", length);
  }

  return res;
}

   
                      
   
AttrNumber *
readAttrNumberCols(int numCols)
{
  int tokenLength, i;
  const char *token;
  AttrNumber *attr_vals;

  if (numCols <= 0)
  {
    return NULL;
  }

  attr_vals = (AttrNumber *)palloc(numCols * sizeof(AttrNumber));
  for (i = 0; i < numCols; i++)
  {
    token = pg_strtok(&tokenLength);
    attr_vals[i] = atoi(token);
  }

  return attr_vals;
}

   
               
   
Oid *
readOidCols(int numCols)
{
  int tokenLength, i;
  const char *token;
  Oid *oid_vals;

  if (numCols <= 0)
  {
    return NULL;
  }

  oid_vals = (Oid *)palloc(numCols * sizeof(Oid));
  for (i = 0; i < numCols; i++)
  {
    token = pg_strtok(&tokenLength);
    oid_vals[i] = atooid(token);
  }

  return oid_vals;
}

   
               
   
int *
readIntCols(int numCols)
{
  int tokenLength, i;
  const char *token;
  int *int_vals;

  if (numCols <= 0)
  {
    return NULL;
  }

  int_vals = (int *)palloc(numCols * sizeof(int));
  for (i = 0; i < numCols; i++)
  {
    token = pg_strtok(&tokenLength);
    int_vals[i] = atoi(token);
  }

  return int_vals;
}

   
                
   
bool *
readBoolCols(int numCols)
{
  int tokenLength, i;
  const char *token;
  bool *bool_vals;

  if (numCols <= 0)
  {
    return NULL;
  }

  bool_vals = (bool *)palloc(numCols * sizeof(bool));
  for (i = 0; i < numCols; i++)
  {
    token = pg_strtok(&tokenLength);
    bool_vals[i] = strtobool(token);
  }

  return bool_vals;
}
