                                                                            
   
                  
                                                                       
   
                                                                       
                                                                          
                                                                       
                                                                      
                                                                      
                                                                    
                                                                     
                                                                        
                                                                           
                                       
   
                                     
                              
                                                           
                                              
                                                            
                                                                           
                                                                        
                               
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "access/htup_details.h"
#include "access/stratnum.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_type.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/pg_locale.h"
#include "utils/selfuncs.h"
#include "utils/varlena.h"

typedef enum
{
  Pattern_Type_Like,
  Pattern_Type_Like_IC,
  Pattern_Type_Regex,
  Pattern_Type_Regex_IC,
  Pattern_Type_Prefix
} Pattern_Type;

typedef enum
{
  Pattern_Prefix_None,
  Pattern_Prefix_Partial,
  Pattern_Prefix_Exact
} Pattern_Prefix_Status;

static Node *
like_regex_support(Node *rawreq, Pattern_Type ptype);
static List *
match_pattern_prefix(Node *leftop, Node *rightop, Pattern_Type ptype, Oid expr_coll, Oid opfamily, Oid indexcollation);
static double
patternsel_common(PlannerInfo *root, Oid oprid, Oid opfuncid, List *args, int varRelid, Oid collation, Pattern_Type ptype, bool negate);
static Pattern_Prefix_Status
pattern_fixed_prefix(Const *patt, Pattern_Type ptype, Oid collation, Const **prefix, Selectivity *rest_selec);
static Selectivity
prefix_selectivity(PlannerInfo *root, VariableStatData *vardata, Oid vartype, Oid opfamily, Oid collation, Const *prefixcon);
static Selectivity
like_selectivity(const char *patt, int pattlen, bool case_insensitive);
static Selectivity
regex_selectivity(const char *patt, int pattlen, bool case_insensitive, int fixed_prefix_len);
static int
pattern_char_isalpha(char c, bool is_multibyte, pg_locale_t locale, bool locale_is_c);
static Const *
make_greater_string(const Const *str_const, FmgrInfo *ltproc, Oid collation);
static Datum
string_to_datum(const char *str, Oid datatype);
static Const *
string_to_const(const char *str, Oid datatype);
static Const *
string_to_bytea_const(const char *str, size_t str_len);

   
                                                                    
   
Datum
textlike_support(PG_FUNCTION_ARGS)
{
  Node *rawreq = (Node *)PG_GETARG_POINTER(0);

  PG_RETURN_POINTER(like_regex_support(rawreq, Pattern_Type_Like));
}

Datum
texticlike_support(PG_FUNCTION_ARGS)
{
  Node *rawreq = (Node *)PG_GETARG_POINTER(0);

  PG_RETURN_POINTER(like_regex_support(rawreq, Pattern_Type_Like_IC));
}

Datum
textregexeq_support(PG_FUNCTION_ARGS)
{
  Node *rawreq = (Node *)PG_GETARG_POINTER(0);

  PG_RETURN_POINTER(like_regex_support(rawreq, Pattern_Type_Regex));
}

Datum
texticregexeq_support(PG_FUNCTION_ARGS)
{
  Node *rawreq = (Node *)PG_GETARG_POINTER(0);

  PG_RETURN_POINTER(like_regex_support(rawreq, Pattern_Type_Regex_IC));
}

                               
static Node *
like_regex_support(Node *rawreq, Pattern_Type ptype)
{
  Node *ret = NULL;

  if (IsA(rawreq, SupportRequestSelectivity))
  {
       
                                                                           
                                                    
       
    SupportRequestSelectivity *req = (SupportRequestSelectivity *)rawreq;
    Selectivity s1;

    if (req->is_join)
    {
         
                                                                 
                                                                
         
      s1 = DEFAULT_MATCH_SEL;
    }
    else
    {
                                                                      
      s1 = patternsel_common(req->root, InvalidOid, req->funcid, req->args, req->varRelid, req->inputcollid, ptype, false);
    }
    req->selectivity = s1;
    ret = (Node *)req;
  }
  else if (IsA(rawreq, SupportRequestIndexCondition))
  {
                                                                   
    SupportRequestIndexCondition *req = (SupportRequestIndexCondition *)rawreq;

       
                                                                          
                                                                         
             
       
    if (req->indexarg != 0)
    {
      return NULL;
    }

    if (is_opclause(req->node))
    {
      OpExpr *clause = (OpExpr *)req->node;

      Assert(list_length(clause->args) == 2);
      ret = (Node *)match_pattern_prefix((Node *)linitial(clause->args), (Node *)lsecond(clause->args), ptype, clause->inputcollid, req->opfamily, req->indexcollation);
    }
    else if (is_funcclause(req->node))                  
    {
      FuncExpr *clause = (FuncExpr *)req->node;

      Assert(list_length(clause->args) == 2);
      ret = (Node *)match_pattern_prefix((Node *)linitial(clause->args), (Node *)lsecond(clause->args), ptype, clause->inputcollid, req->opfamily, req->indexcollation);
    }
  }

  return ret;
}

   
                        
                                                                
   
static List *
match_pattern_prefix(Node *leftop, Node *rightop, Pattern_Type ptype, Oid expr_coll, Oid opfamily, Oid indexcollation)
{
  List *result;
  Const *patt;
  Const *prefix;
  Pattern_Prefix_Status pstatus;
  Oid ldatatype;
  Oid rdatatype;
  Oid oproid;
  Expr *expr;
  FmgrInfo ltproc;
  Const *greaterstr;

     
                                                                     
     
                                                                            
                                                                           
                           
     
  if (!IsA(rightop, Const) || ((Const *)rightop)->constisnull)
  {
    return NIL;
  }
  patt = (Const *)rightop;

     
                                                                         
                                                                           
                                                                  
                                                                         
                                                                             
                                                                          
                                                                  
                                                              
                                                         
     
                                                                             
     
  if (expr_coll && !get_collation_isdeterministic(expr_coll))
  {
    return NIL;
  }

     
                                                     
     
  pstatus = pattern_fixed_prefix(patt, ptype, expr_coll, &prefix, NULL);

                               
  if (pstatus == Pattern_Prefix_None)
  {
    return NIL;
  }

     
                                                                          
                                                                         
                                                                     
     
                                                                             
                                                                          
                    
     
                                                                         
                                                                         
                                       
     
                                                                           
                                                                          
                                                                       
                                                                         
                                                                          
     
                                                                            
                                  
     
  switch (opfamily)
  {
  case TEXT_BTREE_FAM_OID:
    if (!(pstatus == Pattern_Prefix_Exact || lc_collate_is_c(indexcollation)))
    {
      return NIL;
    }
    rdatatype = TEXTOID;
    break;

  case TEXT_PATTERN_BTREE_FAM_OID:
  case TEXT_SPGIST_FAM_OID:
    rdatatype = TEXTOID;
    break;

  case BPCHAR_BTREE_FAM_OID:
    if (!(pstatus == Pattern_Prefix_Exact || lc_collate_is_c(indexcollation)))
    {
      return NIL;
    }
    rdatatype = BPCHAROID;
    break;

  case BPCHAR_PATTERN_BTREE_FAM_OID:
    rdatatype = BPCHAROID;
    break;

  case BYTEA_BTREE_FAM_OID:
    rdatatype = BYTEAOID;
    break;

  default:
    return NIL;
  }

                                              
  ldatatype = exprType(leftop);

     
                                                                            
                                                                           
                                                                            
                                                                      
                 
     
  if (prefix->consttype != rdatatype)
  {
    Assert(prefix->consttype == TEXTOID && rdatatype == BPCHAROID);
    prefix->consttype = rdatatype;
  }

     
                                                                    
     
                                                                             
                                                                     
     
  if (pstatus == Pattern_Prefix_Exact)
  {
    oproid = get_opfamily_member(opfamily, ldatatype, rdatatype, BTEqualStrategyNumber);
    if (oproid == InvalidOid)
    {
      return NIL;
    }
    expr = make_opclause(oproid, BOOLOID, false, (Expr *)leftop, (Expr *)prefix, InvalidOid, indexcollation);
    result = list_make1(expr);
    return result;
  }

     
                                                                  
     
                                      
     
  oproid = get_opfamily_member(opfamily, ldatatype, rdatatype, BTGreaterEqualStrategyNumber);
  if (oproid == InvalidOid)
  {
    return NIL;
  }
  expr = make_opclause(oproid, BOOLOID, false, (Expr *)leftop, (Expr *)prefix, InvalidOid, indexcollation);
  result = list_make1(expr);

            
                                                                  
                                                                         
                                                                      
                                                                       
                                       
            
     
  oproid = get_opfamily_member(opfamily, ldatatype, rdatatype, BTLessStrategyNumber);
  if (oproid == InvalidOid)
  {
    return result;
  }
  fmgr_info(get_opcode(oproid), &ltproc);
  greaterstr = make_greater_string(prefix, &ltproc, indexcollation);
  if (greaterstr)
  {
    expr = make_opclause(oproid, BOOLOID, false, (Expr *)leftop, (Expr *)greaterstr, InvalidOid, indexcollation);
    result = lappend(result, expr);
  }

  return result;
}

   
                                                                               
   
                                                                            
                                                                           
                                                                            
                                                                         
                                                     
   
                                                                             
                             
   
static double
patternsel_common(PlannerInfo *root, Oid oprid, Oid opfuncid, List *args, int varRelid, Oid collation, Pattern_Type ptype, bool negate)
{
  VariableStatData vardata;
  Node *other;
  bool varonleft;
  Datum constval;
  Oid consttype;
  Oid vartype;
  Oid opfamily;
  Pattern_Prefix_Status pstatus;
  Const *patt;
  Const *prefix = NULL;
  Selectivity rest_selec = 0;
  double nullfrac = 0.0;
  double result;

     
                                                                        
                                                 
     
  if (negate)
  {
    result = 1.0 - DEFAULT_MATCH_SEL;
  }
  else
  {
    result = DEFAULT_MATCH_SEL;
  }

     
                                                                         
                       
     
  if (!get_restriction_variable(root, args, varRelid, &vardata, &other, &varonleft))
  {
    return result;
  }
  if (!varonleft || !IsA(other, Const))
  {
    ReleaseVariableStats(vardata);
    return result;
  }

     
                                                                             
                                                                          
     
  if (((Const *)other)->constisnull)
  {
    ReleaseVariableStats(vardata);
    return 0.0;
  }
  constval = ((Const *)other)->constvalue;
  consttype = ((Const *)other)->consttype;

     
                                                                             
                                                                 
                                                                        
                               
     
  if (consttype != TEXTOID && consttype != BYTEAOID)
  {
    ReleaseVariableStats(vardata);
    return result;
  }

     
                                                                        
                                                                     
                                                                         
                                                                          
     
                                                                           
                                                                             
                                                                            
                                 
     
  vartype = vardata.vartype;

  switch (vartype)
  {
  case TEXTOID:
  case NAMEOID:
    opfamily = TEXT_BTREE_FAM_OID;
    break;
  case BPCHAROID:
    opfamily = BPCHAR_BTREE_FAM_OID;
    break;
  case BYTEAOID:
    opfamily = BYTEA_BTREE_FAM_OID;
    break;
  default:
    ReleaseVariableStats(vardata);
    return result;
  }

     
                                      
     
  if (HeapTupleIsValid(vardata.statsTuple))
  {
    Form_pg_statistic stats;

    stats = (Form_pg_statistic)GETSTRUCT(vardata.statsTuple);
    nullfrac = stats->stanullfrac;
  }

     
                                                                        
                                                                          
                                                                        
                                                                           
                                                                             
                                                                        
                                                              
     
  patt = (Const *)other;
  pstatus = pattern_fixed_prefix(patt, ptype, collation, &prefix, &rest_selec);

     
                                                                 
     
  if (prefix && prefix->consttype != vartype)
  {
    char *prefixstr;

    switch (prefix->consttype)
    {
    case TEXTOID:
      prefixstr = TextDatumGetCString(prefix->constvalue);
      break;
    case BYTEAOID:
      prefixstr = DatumGetCString(DirectFunctionCall1(byteaout, prefix->constvalue));
      break;
    default:
      elog(ERROR, "unrecognized consttype: %u", prefix->consttype);
      ReleaseVariableStats(vardata);
      return result;
    }
    prefix = string_to_const(prefixstr, vartype);
    pfree(prefixstr);
  }

  if (pstatus == Pattern_Prefix_Exact)
  {
       
                                                                    
       
    Oid eqopr = get_opfamily_member(opfamily, vartype, vartype, BTEqualStrategyNumber);

    if (eqopr == InvalidOid)
    {
      elog(ERROR, "no = operator for opfamily %u", opfamily);
    }
    result = var_eq_const_ext(&vardata, eqopr, collation, prefix->constvalue, false, true, false);
  }
  else
  {
       
                                                                 
                                                                     
                                                                          
                                                                
                                                                  
                                                                        
                                                               
                    
       
                                                                        
                                                                         
                                                                      
                                                                        
                                                            
       
    Selectivity selec;
    int hist_size;
    FmgrInfo opproc;
    double mcv_selec, sumcommon;

                                                             
    if (!OidIsValid(opfuncid))
    {
      opfuncid = get_opcode(oprid);
    }
    fmgr_info(opfuncid, &opproc);

    selec = histogram_selectivity_ext(&vardata, &opproc, collation, constval, true, 10, 1, &hist_size);

                                                               
    if (hist_size < 100)
    {
      Selectivity heursel;
      Selectivity prefixsel;

      if (pstatus == Pattern_Prefix_Partial)
      {
        prefixsel = prefix_selectivity(root, &vardata, vartype, opfamily, collation, prefix);
      }
      else
      {
        prefixsel = 1.0;
      }
      heursel = prefixsel * rest_selec;

      if (selec < 0)                                       
      {
        selec = heursel;
      }
      else
      {
           
                                                              
                                                                       
                                                         
           
        double hist_weight = hist_size / 100.0;

        selec = selec * hist_weight + heursel * (1.0 - hist_weight);
      }
    }

                                                                        
    if (selec < 0.0001)
    {
      selec = 0.0001;
    }
    else if (selec > 0.9999)
    {
      selec = 0.9999;
    }

       
                                                                           
                                                                        
                                                                           
                                   
       
    mcv_selec = mcv_selectivity_ext(&vardata, &opproc, collation, constval, true, &sumcommon);

       
                                                                      
                                                                         
                              
       
    selec *= 1.0 - nullfrac - sumcommon;
    selec += mcv_selec;
    result = selec;
  }

                                                           
  if (negate)
  {
    result = 1.0 - result - nullfrac;
  }

                                                   
  CLAMP_PROBABILITY(result);

  if (prefix)
  {
    pfree(DatumGetPointer(prefix->constvalue));
    pfree(prefix);
  }

  ReleaseVariableStats(vardata);

  return result;
}

   
                                                                               
   
static double
patternsel(PG_FUNCTION_ARGS, Pattern_Type ptype, bool negate)
{
  PlannerInfo *root = (PlannerInfo *)PG_GETARG_POINTER(0);
  Oid operator= PG_GETARG_OID(1);
  List *args = (List *)PG_GETARG_POINTER(2);
  int varRelid = PG_GETARG_INT32(3);
  Oid collation = PG_GET_COLLATION();

     
                                                                          
                                                 
     
  if (negate)
  {
    operator= get_negator(operator);
    if (!OidIsValid(operator))
    {
      elog(ERROR, "patternsel called for operator without a negator");
    }
  }

  return patternsel_common(root, operator, InvalidOid, args, varRelid, collation, ptype, negate);
}

   
                                                                   
   
Datum
regexeqsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternsel(fcinfo, Pattern_Type_Regex, false));
}

   
                                                                
   
Datum
icregexeqsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternsel(fcinfo, Pattern_Type_Regex_IC, false));
}

   
                                                   
   
Datum
likesel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternsel(fcinfo, Pattern_Type_Like, false));
}

   
                                                 
   
Datum
prefixsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternsel(fcinfo, Pattern_Type_Prefix, false));
}

   
   
                                                      
   
Datum
iclikesel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternsel(fcinfo, Pattern_Type_Like_IC, false));
}

   
                                                                       
   
Datum
regexnesel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternsel(fcinfo, Pattern_Type_Regex, true));
}

   
                                                                    
   
Datum
icregexnesel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternsel(fcinfo, Pattern_Type_Regex_IC, true));
}

   
                                                       
   
Datum
nlikesel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternsel(fcinfo, Pattern_Type_Like, true));
}

   
                                                          
   
Datum
icnlikesel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternsel(fcinfo, Pattern_Type_Like_IC, true));
}

   
                                                                      
   
static double
patternjoinsel(PG_FUNCTION_ARGS, Pattern_Type ptype, bool negate)
{
                                    
  return negate ? (1.0 - DEFAULT_MATCH_SEL) : DEFAULT_MATCH_SEL;
}

   
                                                                           
   
Datum
regexeqjoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternjoinsel(fcinfo, Pattern_Type_Regex, false));
}

   
                                                                         
   
Datum
icregexeqjoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternjoinsel(fcinfo, Pattern_Type_Regex_IC, false));
}

   
                                                            
   
Datum
likejoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternjoinsel(fcinfo, Pattern_Type_Like, false));
}

   
                                                          
   
Datum
prefixjoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternjoinsel(fcinfo, Pattern_Type_Prefix, false));
}

   
                                                               
   
Datum
iclikejoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternjoinsel(fcinfo, Pattern_Type_Like_IC, false));
}

   
                                                          
   
Datum
regexnejoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternjoinsel(fcinfo, Pattern_Type_Regex, true));
}

   
                                                                             
   
Datum
icregexnejoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternjoinsel(fcinfo, Pattern_Type_Regex_IC, true));
}

   
                                                                
   
Datum
nlikejoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternjoinsel(fcinfo, Pattern_Type_Like, true));
}

   
                                                                   
   
Datum
icnlikejoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(patternjoinsel(fcinfo, Pattern_Type_Like_IC, true));
}

                                                                            
   
                              
   
                                                                           
                                                                      
                                                                  
                                                                        
                                                                          
                                                                          
                                       
   
                                                                            
   

   
                                                    
   
                                                                             
                                                         
                                                                       
                                                                           
                                                                            
   
                                                                     
                                   
   

static Pattern_Prefix_Status
like_fixed_prefix(Const *patt_const, bool case_insensitive, Oid collation, Const **prefix_const, Selectivity *rest_selec)
{
  char *match;
  char *patt;
  int pattlen;
  Oid typeid = patt_const->consttype;
  int pos, match_pos;
  bool is_multibyte = (pg_database_encoding_max_length() > 1);
  pg_locale_t locale = 0;
  bool locale_is_c = false;

                                                  
  Assert(typeid == BYTEAOID || typeid == TEXTOID);

  if (case_insensitive)
  {
    if (typeid == BYTEAOID)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("case insensitive matching not supported on type bytea")));
    }

                                                  
    if (lc_ctype_is_c(collation))
    {
      locale_is_c = true;
    }
    else if (collation != DEFAULT_COLLATION_OID)
    {
      if (!OidIsValid(collation))
      {
           
                                                                    
                                                                   
           
        ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("could not determine which collation to use for ILIKE"), errhint("Use the COLLATE clause to set the collation explicitly.")));
      }
      locale = pg_newlocale_from_collation(collation);
    }
  }

  if (typeid != BYTEAOID)
  {
    patt = TextDatumGetCString(patt_const->constvalue);
    pattlen = strlen(patt);
  }
  else
  {
    bytea *bstr = DatumGetByteaPP(patt_const->constvalue);

    pattlen = VARSIZE_ANY_EXHDR(bstr);
    patt = (char *)palloc(pattlen);
    memcpy(patt, VARDATA_ANY(bstr), pattlen);
    Assert((Pointer)bstr == DatumGetPointer(patt_const->constvalue));
  }

  match = palloc(pattlen + 1);
  match_pos = 0;
  for (pos = 0; pos < pattlen; pos++)
  {
                                                 
    if (patt[pos] == '%' || patt[pos] == '_')
    {
      break;
    }

                                              
    if (patt[pos] == '\\')
    {
      pos++;
      if (pos >= pattlen)
      {
        break;
      }
    }

                                                                  
    if (case_insensitive && pattern_char_isalpha(patt[pos], is_multibyte, locale, locale_is_c))
    {
      break;
    }

    match[match_pos++] = patt[pos];
  }

  match[match_pos] = '\0';

  if (typeid != BYTEAOID)
  {
    *prefix_const = string_to_const(match, typeid);
  }
  else
  {
    *prefix_const = string_to_bytea_const(match, match_pos);
  }

  if (rest_selec != NULL)
  {
    *rest_selec = like_selectivity(&patt[pos], pattlen - pos, case_insensitive);
  }

  pfree(patt);
  pfree(match);

                                                    
  if (pos == pattlen)
  {
    return Pattern_Prefix_Exact;                                       
  }

  if (match_pos > 0)
  {
    return Pattern_Prefix_Partial;
  }

  return Pattern_Prefix_None;
}

static Pattern_Prefix_Status
regex_fixed_prefix(Const *patt_const, bool case_insensitive, Oid collation, Const **prefix_const, Selectivity *rest_selec)
{
  Oid typeid = patt_const->consttype;
  char *prefix;
  bool exact;

     
                                                                           
                                                                            
                                                              
     
  if (typeid == BYTEAOID)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("regular-expression matching not supported on type bytea")));
  }

                                                              
  prefix = regexp_fixed_prefix(DatumGetTextPP(patt_const->constvalue), case_insensitive, collation, &exact);

  if (prefix == NULL)
  {
    *prefix_const = NULL;

    if (rest_selec != NULL)
    {
      char *patt = TextDatumGetCString(patt_const->constvalue);

      *rest_selec = regex_selectivity(patt, strlen(patt), case_insensitive, 0);
      pfree(patt);
    }

    return Pattern_Prefix_None;
  }

  *prefix_const = string_to_const(prefix, typeid);

  if (rest_selec != NULL)
  {
    if (exact)
    {
                                                             
      *rest_selec = 1.0;
    }
    else
    {
      char *patt = TextDatumGetCString(patt_const->constvalue);

      *rest_selec = regex_selectivity(patt, strlen(patt), case_insensitive, strlen(prefix));
      pfree(patt);
    }
  }

  pfree(prefix);

  if (exact)
  {
    return Pattern_Prefix_Exact;                                    
  }
  else
  {
    return Pattern_Prefix_Partial;
  }
}

static Pattern_Prefix_Status
pattern_fixed_prefix(Const *patt, Pattern_Type ptype, Oid collation, Const **prefix, Selectivity *rest_selec)
{
  Pattern_Prefix_Status result;

  switch (ptype)
  {
  case Pattern_Type_Like:
    result = like_fixed_prefix(patt, false, collation, prefix, rest_selec);
    break;
  case Pattern_Type_Like_IC:
    result = like_fixed_prefix(patt, true, collation, prefix, rest_selec);
    break;
  case Pattern_Type_Regex:
    result = regex_fixed_prefix(patt, false, collation, prefix, rest_selec);
    break;
  case Pattern_Type_Regex_IC:
    result = regex_fixed_prefix(patt, true, collation, prefix, rest_selec);
    break;
  case Pattern_Type_Prefix:
                                       
    result = Pattern_Prefix_Partial;
    *rest_selec = 1.0;          
    *prefix = makeConst(patt->consttype, patt->consttypmod, patt->constcollid, patt->constlen, datumCopy(patt->constvalue, patt->constbyval, patt->constlen), patt->constisnull, patt->constbyval);
    break;
  default:
    elog(ERROR, "unrecognized ptype: %d", (int)ptype);
    result = Pattern_Prefix_None;                          
    break;
  }
  return result;
}

   
                                                                   
   
                                                                          
                                                                   
   
                                                                         
                                                                         
                                            
   
                                                                             
                                                                       
             
   
                                                                             
                                                                             
                                                                            
                                                     
   
static Selectivity
prefix_selectivity(PlannerInfo *root, VariableStatData *vardata, Oid vartype, Oid opfamily, Oid collation, Const *prefixcon)
{
  Selectivity prefixsel;
  Oid cmpopr;
  FmgrInfo opproc;
  Const *greaterstrcon;
  Selectivity eq_sel;

  cmpopr = get_opfamily_member(opfamily, vartype, vartype, BTGreaterEqualStrategyNumber);
  if (cmpopr == InvalidOid)
  {
    elog(ERROR, "no >= operator for opfamily %u", opfamily);
  }
  fmgr_info(get_opcode(cmpopr), &opproc);

  prefixsel = ineq_histogram_selectivity_ext(root, vardata, &opproc, true, true, collation, prefixcon->constvalue, prefixcon->consttype);

  if (prefixsel < 0.0)
  {
                                                                        
    return DEFAULT_MATCH_SEL;
  }

     
                                                                             
     
  cmpopr = get_opfamily_member(opfamily, vartype, vartype, BTLessStrategyNumber);
  if (cmpopr == InvalidOid)
  {
    elog(ERROR, "no < operator for opfamily %u", opfamily);
  }
  fmgr_info(get_opcode(cmpopr), &opproc);
  greaterstrcon = make_greater_string(prefixcon, &opproc, collation);
  if (greaterstrcon)
  {
    Selectivity topsel;

    topsel = ineq_histogram_selectivity_ext(root, vardata, &opproc, false, false, collation, greaterstrcon->constvalue, greaterstrcon->consttype);

                                                                         
    Assert(topsel >= 0.0);

       
                                                                        
                                                                         
                                                                         
                                   
       
    prefixsel = topsel + prefixsel - 1.0;
  }

     
                                                                           
                                                                             
                                                                        
                                                                        
                                                                      
                                        
     
                                                                         
                                                                     
                                                                            
                                                                      
     
  cmpopr = get_opfamily_member(opfamily, vartype, vartype, BTEqualStrategyNumber);
  if (cmpopr == InvalidOid)
  {
    elog(ERROR, "no = operator for opfamily %u", opfamily);
  }
  eq_sel = var_eq_const_ext(vardata, cmpopr, collation, prefixcon->constvalue, false, true, false);

  prefixsel = Max(prefixsel, eq_sel);

  return prefixsel;
}

   
                                                                
                                                                             
                                                                    
   
                                                                           
                                                                 
                                                               
   

#define FIXED_CHAR_SEL 0.20                
#define CHAR_RANGE_SEL 0.25
#define ANY_CHAR_SEL 0.9                                                
#define FULL_WILDCARD_SEL 5.0
#define PARTIAL_WILDCARD_SEL 2.0

static Selectivity
like_selectivity(const char *patt, int pattlen, bool case_insensitive)
{
  Selectivity sel = 1.0;
  int pos;

                                                                         
  for (pos = 0; pos < pattlen; pos++)
  {
    if (patt[pos] != '%' && patt[pos] != '_')
    {
      break;
    }
  }

  for (; pos < pattlen; pos++)
  {
                                                 
    if (patt[pos] == '%')
    {
      sel *= FULL_WILDCARD_SEL;
    }
    else if (patt[pos] == '_')
    {
      sel *= ANY_CHAR_SEL;
    }
    else if (patt[pos] == '\\')
    {
                                               
      pos++;
      if (pos >= pattlen)
      {
        break;
      }
      sel *= FIXED_CHAR_SEL;
    }
    else
    {
      sel *= FIXED_CHAR_SEL;
    }
  }
                                               
  if (sel > 1.0)
  {
    sel = 1.0;
  }
  return sel;
}

static Selectivity
regex_selectivity_sub(const char *patt, int pattlen, bool case_insensitive)
{
  Selectivity sel = 1.0;
  int paren_depth = 0;
  int paren_pos = 0;                                        
  int pos;

                                                                          
  check_stack_depth();

  for (pos = 0; pos < pattlen; pos++)
  {
    if (patt[pos] == '(')
    {
      if (paren_depth == 0)
      {
        paren_pos = pos;                                           
      }
      paren_depth++;
    }
    else if (patt[pos] == ')' && paren_depth > 0)
    {
      paren_depth--;
      if (paren_depth == 0)
      {
        sel *= regex_selectivity_sub(patt + (paren_pos + 1), pos - (paren_pos + 1), case_insensitive);
      }
    }
    else if (patt[pos] == '|' && paren_depth == 0)
    {
         
                                                                       
                                                         
         
      sel += regex_selectivity_sub(patt + (pos + 1), pattlen - (pos + 1), case_insensitive);
      break;                                       
    }
    else if (patt[pos] == '[')
    {
      bool negclass = false;

      if (patt[++pos] == '^')
      {
        negclass = true;
        pos++;
      }
      if (patt[pos] == ']')                                           
      {
        pos++;
      }
      while (pos < pattlen && patt[pos] != ']')
      {
        pos++;
      }
      if (paren_depth == 0)
      {
        sel *= (negclass ? (1.0 - CHAR_RANGE_SEL) : CHAR_RANGE_SEL);
      }
    }
    else if (patt[pos] == '.')
    {
      if (paren_depth == 0)
      {
        sel *= ANY_CHAR_SEL;
      }
    }
    else if (patt[pos] == '*' || patt[pos] == '?' || patt[pos] == '+')
    {
                                                    
      if (paren_depth == 0)
      {
        sel *= PARTIAL_WILDCARD_SEL;
      }
    }
    else if (patt[pos] == '{')
    {
      while (pos < pattlen && patt[pos] != '}')
      {
        pos++;
      }
      if (paren_depth == 0)
      {
        sel *= PARTIAL_WILDCARD_SEL;
      }
    }
    else if (patt[pos] == '\\')
    {
                                               
      pos++;
      if (pos >= pattlen)
      {
        break;
      }
      if (paren_depth == 0)
      {
        sel *= FIXED_CHAR_SEL;
      }
    }
    else
    {
      if (paren_depth == 0)
      {
        sel *= FIXED_CHAR_SEL;
      }
    }
  }
                                               
  if (sel > 1.0)
  {
    sel = 1.0;
  }
  return sel;
}

static Selectivity
regex_selectivity(const char *patt, int pattlen, bool case_insensitive, int fixed_prefix_len)
{
  Selectivity sel;

                                                                           
  if (pattlen > 0 && patt[pattlen - 1] == '$' && (pattlen == 1 || patt[pattlen - 2] != '\\'))
  {
                        
    sel = regex_selectivity_sub(patt, pattlen - 1, case_insensitive);
  }
  else
  {
                       
    sel = regex_selectivity_sub(patt, pattlen, case_insensitive);
    sel *= FULL_WILDCARD_SEL;
  }

     
                                                                         
                                                                        
                                                                      
     
  if (fixed_prefix_len > 0)
  {
    double prefixsel = pow(FIXED_CHAR_SEL, fixed_prefix_len);

    if (prefixsel > 0.0)
    {
      sel /= prefixsel;
    }
  }

                                       
  CLAMP_PROBABILITY(sel);
  return sel;
}

   
                                                                        
   
                                                                              
                                                                             
                                                                            
                                                         
   
static int
pattern_char_isalpha(char c, bool is_multibyte, pg_locale_t locale, bool locale_is_c)
{
  if (locale_is_c)
  {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
  }
  else if (is_multibyte && IS_HIGHBIT_SET(c))
  {
    return true;
  }
  else if (locale && locale->provider == COLLPROVIDER_ICU)
  {
    return IS_HIGHBIT_SET(c) || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
  }
#ifdef HAVE_LOCALE_T
  else if (locale && locale->provider == COLLPROVIDER_LIBC)
  {
    return isalpha_l((unsigned char)c, locale->info.lt);
  }
#endif
  else
  {
    return isalpha((unsigned char)c);
  }
}

   
                                                                          
                                                       
   
static bool
byte_increment(unsigned char *ptr, int len)
{
  if (*ptr >= 255)
  {
    return false;
  }
  (*ptr)++;
  return true;
}

   
                                                                 
                                                                      
                                                  
   
                                                                           
                                                             
   
                                                                      
                                                                         
                                                                       
                                                                              
                                                                             
                                                                            
                                                                            
                                                                              
                                                          
   
                                                                            
                                                                             
                                                                             
                                                                  
   
                                                                           
                                                                         
                                                                             
                                                                    
                                                                          
                                                        
   
                                                                     
                                                                     
   
                                                                             
                                                                               
                                                                             
                                                                           
                                                                          
                                                                           
                                                                  
   
static Const *
make_greater_string(const Const *str_const, FmgrInfo *ltproc, Oid collation)
{
  Oid datatype = str_const->consttype;
  char *workstr;
  int len;
  Datum cmpstr;
  char *cmptxt = NULL;
  mbcharacter_incrementer charinc;

     
                                                                            
                                                                             
                                                                          
                                                             
     
  if (datatype == BYTEAOID)
  {
    bytea *bstr = DatumGetByteaPP(str_const->constvalue);

    len = VARSIZE_ANY_EXHDR(bstr);
    workstr = (char *)palloc(len);
    memcpy(workstr, VARDATA_ANY(bstr), len);
    Assert((Pointer)bstr == DatumGetPointer(str_const->constvalue));
    cmpstr = str_const->constvalue;
  }
  else
  {
    if (datatype == NAMEOID)
    {
      workstr = DatumGetCString(DirectFunctionCall1(nameout, str_const->constvalue));
    }
    else
    {
      workstr = TextDatumGetCString(str_const->constvalue);
    }
    len = strlen(workstr);
    if (lc_collate_is_c(collation) || len == 0)
    {
      cmpstr = str_const->constvalue;
    }
    else
    {
                                                              
      static char suffixchar = 0;
      static Oid suffixcollation = 0;

      if (!suffixchar || suffixcollation != collation)
      {
        char *best;

        best = "Z";
        if (varstr_cmp(best, 1, "z", 1, collation) < 0)
        {
          best = "z";
        }
        if (varstr_cmp(best, 1, "y", 1, collation) < 0)
        {
          best = "y";
        }
        if (varstr_cmp(best, 1, "9", 1, collation) < 0)
        {
          best = "9";
        }
        suffixchar = *best;
        suffixcollation = collation;
      }

                                              
      if (datatype == NAMEOID)
      {
        cmptxt = palloc(len + 2);
        memcpy(cmptxt, workstr, len);
        cmptxt[len] = suffixchar;
        cmptxt[len + 1] = '\0';
        cmpstr = PointerGetDatum(cmptxt);
      }
      else
      {
        cmptxt = palloc(VARHDRSZ + len + 1);
        SET_VARSIZE(cmptxt, VARHDRSZ + len + 1);
        memcpy(VARDATA(cmptxt), workstr, len);
        *(VARDATA(cmptxt) + len) = suffixchar;
        cmpstr = PointerGetDatum(cmptxt);
      }
    }
  }

                                                         
  if (datatype == BYTEAOID)
  {
    charinc = byte_increment;
  }
  else
  {
    charinc = pg_database_encoding_character_incrementer();
  }

                      
  while (len > 0)
  {
    int charlen;
    unsigned char *lastchar;

                                                                       
    if (datatype == BYTEAOID)
    {
      charlen = 1;
    }
    else
    {
      charlen = len - pg_mbcliplen(workstr, len, len - 1);
    }
    lastchar = (unsigned char *)(workstr + len - charlen);

       
                                                                          
                                                       
       
                                                                         
                                                                          
                                                                      
       
    while (charinc(lastchar, charlen))
    {
      Const *workstr_const;

      if (datatype == BYTEAOID)
      {
        workstr_const = string_to_bytea_const(workstr, len);
      }
      else
      {
        workstr_const = string_to_const(workstr, datatype);
      }

      if (DatumGetBool(FunctionCall2Coll(ltproc, collation, cmpstr, workstr_const->constvalue)))
      {
                                                           
        if (cmptxt)
        {
          pfree(cmptxt);
        }
        pfree(workstr);
        return workstr_const;
      }

                                                         
      pfree(DatumGetPointer(workstr_const->constvalue));
      pfree(workstr_const);
    }

       
                                                                   
                               
       
    len -= charlen;
    workstr[len] = '\0';
  }

                 
  if (cmptxt)
  {
    pfree(cmptxt);
  }
  pfree(workstr);

  return NULL;
}

   
                                                             
                                                                
                                                         
   
static Datum
string_to_datum(const char *str, Oid datatype)
{
  Assert(str != NULL);

     
                                                                          
                                         
     
  if (datatype == NAMEOID)
  {
    return DirectFunctionCall1(namein, CStringGetDatum(str));
  }
  else if (datatype == BYTEAOID)
  {
    return DirectFunctionCall1(byteain, CStringGetDatum(str));
  }
  else
  {
    return CStringGetTextDatum(str);
  }
}

   
                                                                  
   
static Const *
string_to_const(const char *str, Oid datatype)
{
  Datum conval = string_to_datum(str, datatype);
  Oid collation;
  int constlen;

     
                                                                           
                                                          
     
  switch (datatype)
  {
  case TEXTOID:
  case VARCHAROID:
  case BPCHAROID:
    collation = DEFAULT_COLLATION_OID;
    constlen = -1;
    break;

  case NAMEOID:
    collation = C_COLLATION_OID;
    constlen = NAMEDATALEN;
    break;

  case BYTEAOID:
    collation = InvalidOid;
    constlen = -1;
    break;

  default:
    elog(ERROR, "unexpected datatype in string_to_const: %u", datatype);
    return NULL;
  }

  return makeConst(datatype, -1, collation, constlen, conval, false, false);
}

   
                                                                            
   
static Const *
string_to_bytea_const(const char *str, size_t str_len)
{
  bytea *bstr = palloc(VARHDRSZ + str_len);
  Datum conval;

  memcpy(VARDATA(bstr), str, str_len);
  SET_VARSIZE(bstr, VARHDRSZ + str_len);
  conval = PointerGetDatum(bstr);

  return makeConst(BYTEAOID, -1, InvalidOid, -1, conval, false, false);
}
