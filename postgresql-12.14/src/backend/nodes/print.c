                                                                            
   
           
                                                        
   
                                                                         
                                                                        
   
   
                  
                               
   
           
                                 
                                            
   
                                                                            
   

#include "postgres.h"

#include "access/printtup.h"
#include "lib/stringinfo.h"
#include "nodes/nodeFuncs.h"
#include "nodes/pathnodes.h"
#include "nodes/print.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"

   
         
                                      
   
void
print(const void *obj)
{
  char *s;
  char *f;

  s = nodeToString(obj);
  f = format_node_dump(s);
  pfree(s);
  printf("%s\n", f);
  fflush(stdout);
  pfree(f);
}

   
          
                                             
   
void
pprint(const void *obj)
{
  char *s;
  char *f;

  s = nodeToString(obj);
  f = pretty_format_node_dump(s);
  pfree(s);
  printf("%s\n", f);
  fflush(stdout);
  pfree(f);
}

   
                     
                                                            
   
void
elog_node_display(int lev, const char *title, const void *obj, bool pretty)
{
  char *s;
  char *f;

  s = nodeToString(obj);
  if (pretty)
  {
    f = pretty_format_node_dump(s);
  }
  else
  {
    f = format_node_dump(s);
  }
  pfree(s);
  ereport(lev, (errmsg_internal("%s:", title), errdetail_internal("%s", f)));
  pfree(f);
}

   
                                                           
   
                                    
   
                                                   
   
char *
format_node_dump(const char *dump)
{
#define LINELEN 78
  char line[LINELEN + 1];
  StringInfoData str;
  int i;
  int j;
  int k;

  initStringInfo(&str);
  i = 0;
  for (;;)
  {
    for (j = 0; j < LINELEN && dump[i] != '\0'; i++, j++)
    {
      line[j] = dump[i];
    }
    if (dump[i] == '\0')
    {
      break;
    }
    if (dump[i] == ' ')
    {
                                         
      i++;
    }
    else
    {
      for (k = j - 1; k > 0; k--)
      {
        if (line[k] == ' ')
        {
          break;
        }
      }
      if (k > 0)
      {
                                                   
        i -= (j - k - 1);
        j = k;
      }
    }
    line[j] = '\0';
    appendStringInfo(&str, "%s\n", line);
  }
  if (j > 0)
  {
    line[j] = '\0';
    appendStringInfo(&str, "%s\n", line);
  }
  return str.data;
#undef LINELEN
}

   
                                                           
   
                                    
   
                                               
   
char *
pretty_format_node_dump(const char *dump)
{
#define INDENTSTOP 3
#define MAXINDENT 60
#define LINELEN 78
  char line[LINELEN + 1];
  StringInfoData str;
  int indentLev;
  int indentDist;
  int i;
  int j;

  initStringInfo(&str);
  indentLev = 0;                            
  indentDist = 0;                               
  i = 0;
  for (;;)
  {
    for (j = 0; j < indentDist; j++)
    {
      line[j] = ' ';
    }
    for (; j < LINELEN && dump[i] != '\0'; i++, j++)
    {
      line[j] = dump[i];
      switch (line[j])
      {
      case '}':
        if (j != indentDist)
        {
                                       
          line[j] = '\0';
          appendStringInfo(&str, "%s\n", line);
        }
                                       
        line[indentDist] = '}';
        line[indentDist + 1] = '\0';
        appendStringInfo(&str, "%s\n", line);
                     
        if (indentLev > 0)
        {
          indentLev--;
          indentDist = Min(indentLev * INDENTSTOP, MAXINDENT);
        }
        j = indentDist - 1;
                                                            
                                              
        while (dump[i + 1] == ' ')
        {
          i++;
        }
        break;
      case ')':
                                                                
        if (dump[i + 1] != ')')
        {
          line[j + 1] = '\0';
          appendStringInfo(&str, "%s\n", line);
          j = indentDist - 1;
          while (dump[i + 1] == ' ')
          {
            i++;
          }
        }
        break;
      case '{':
                                       
        if (j != indentDist)
        {
          line[j] = '\0';
          appendStringInfo(&str, "%s\n", line);
        }
                    
        indentLev++;
        indentDist = Min(indentLev * INDENTSTOP, MAXINDENT);
        for (j = 0; j < indentDist; j++)
        {
          line[j] = ' ';
        }
        line[j] = dump[i];
        break;
      case ':':
                                       
        if (j != indentDist)
        {
          line[j] = '\0';
          appendStringInfo(&str, "%s\n", line);
        }
        j = indentDist;
        line[j] = dump[i];
        break;
      }
    }
    line[j] = '\0';
    if (dump[i] == '\0')
    {
      break;
    }
    appendStringInfo(&str, "%s\n", line);
  }
  if (j > 0)
  {
    appendStringInfo(&str, "%s\n", line);
  }
  return str.data;
#undef INDENTSTOP
#undef MAXINDENT
#undef LINELEN
}

   
            
                                   
   
void
print_rt(const List *rtable)
{
  const ListCell *l;
  int i = 1;

  printf("resno\trefname  \trelid\tinFromCl\n");
  printf("-----\t---------\t-----\t--------\n");
  foreach (l, rtable)
  {
    RangeTblEntry *rte = lfirst(l);

    switch (rte->rtekind)
    {
    case RTE_RELATION:
      printf("%d\t%s\t%u\t%c", i, rte->eref->aliasname, rte->relid, rte->relkind);
      break;
    case RTE_SUBQUERY:
      printf("%d\t%s\t[subquery]", i, rte->eref->aliasname);
      break;
    case RTE_JOIN:
      printf("%d\t%s\t[join]", i, rte->eref->aliasname);
      break;
    case RTE_FUNCTION:
      printf("%d\t%s\t[rangefunction]", i, rte->eref->aliasname);
      break;
    case RTE_TABLEFUNC:
      printf("%d\t%s\t[table function]", i, rte->eref->aliasname);
      break;
    case RTE_VALUES:
      printf("%d\t%s\t[values list]", i, rte->eref->aliasname);
      break;
    case RTE_CTE:
      printf("%d\t%s\t[cte]", i, rte->eref->aliasname);
      break;
    case RTE_NAMEDTUPLESTORE:
      printf("%d\t%s\t[tuplestore]", i, rte->eref->aliasname);
      break;
    case RTE_RESULT:
      printf("%d\t%s\t[result]", i, rte->eref->aliasname);
      break;
    default:
      printf("%d\t%s\t[unknown rtekind]", i, rte->eref->aliasname);
    }

    printf("\t%s\t%s\n", (rte->inh ? "inh" : ""), (rte->inFromCl ? "inFromCl" : ""));
    i++;
  }
}

   
              
                         
   
void
print_expr(const Node *expr, const List *rtable)
{
  if (expr == NULL)
  {
    printf("<>");
    return;
  }

  if (IsA(expr, Var))
  {
    const Var *var = (const Var *)expr;
    char *relname, *attname;

    switch (var->varno)
    {
    case INNER_VAR:
      relname = "INNER";
      attname = "?";
      break;
    case OUTER_VAR:
      relname = "OUTER";
      attname = "?";
      break;
    case INDEX_VAR:
      relname = "INDEX";
      attname = "?";
      break;
    default:
    {
      RangeTblEntry *rte;

      Assert(var->varno > 0 && (int)var->varno <= list_length(rtable));
      rte = rt_fetch(var->varno, rtable);
      relname = rte->eref->aliasname;
      attname = get_rte_attribute_name(rte, var->varattno);
    }
    break;
    }
    printf("%s.%s", relname, attname);
  }
  else if (IsA(expr, Const))
  {
    const Const *c = (const Const *)expr;
    Oid typoutput;
    bool typIsVarlena;
    char *outputstr;

    if (c->constisnull)
    {
      printf("NULL");
      return;
    }

    getTypeOutputInfo(c->consttype, &typoutput, &typIsVarlena);

    outputstr = OidOutputFunctionCall(typoutput, c->constvalue);
    printf("%s", outputstr);
    pfree(outputstr);
  }
  else if (IsA(expr, OpExpr))
  {
    const OpExpr *e = (const OpExpr *)expr;
    char *opname;

    opname = get_opname(e->opno);
    if (list_length(e->args) > 1)
    {
      print_expr(get_leftop((const Expr *)e), rtable);
      printf(" %s ", ((opname != NULL) ? opname : "(invalid operator)"));
      print_expr(get_rightop((const Expr *)e), rtable);
    }
    else
    {
                                                       
      printf("%s ", ((opname != NULL) ? opname : "(invalid operator)"));
      print_expr(get_leftop((const Expr *)e), rtable);
    }
  }
  else if (IsA(expr, FuncExpr))
  {
    const FuncExpr *e = (const FuncExpr *)expr;
    char *funcname;
    ListCell *l;

    funcname = get_func_name(e->funcid);
    printf("%s(", ((funcname != NULL) ? funcname : "(invalid function)"));
    foreach (l, e->args)
    {
      print_expr(lfirst(l), rtable);
      if (lnext(l))
      {
        printf(",");
      }
    }
    printf(")");
  }
  else
  {
    printf("unknown expr");
  }
}

   
                    
                               
   
void
print_pathkeys(const List *pathkeys, const List *rtable)
{
  const ListCell *i;

  printf("(");
  foreach (i, pathkeys)
  {
    PathKey *pathkey = (PathKey *)lfirst(i);
    EquivalenceClass *eclass;
    ListCell *k;
    bool first = true;

    eclass = pathkey->pk_eclass;
                                                    
    while (eclass->ec_merged)
    {
      eclass = eclass->ec_merged;
    }

    printf("(");
    foreach (k, eclass->ec_members)
    {
      EquivalenceMember *mem = (EquivalenceMember *)lfirst(k);

      if (first)
      {
        first = false;
      }
      else
      {
        printf(", ");
      }
      print_expr((Node *)mem->em_expr, rtable);
    }
    printf(")");
    if (lnext(i))
    {
      printf(", ");
    }
  }
  printf(")\n");
}

   
            
                                             
   
void
print_tl(const List *tlist, const List *rtable)
{
  const ListCell *tl;

  printf("(\n");
  foreach (tl, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(tl);

    printf("\t%d %s\t", tle->resno, tle->resname ? tle->resname : "<null>");
    if (tle->ressortgroupref != 0)
    {
      printf("(%u):\t", tle->ressortgroupref);
    }
    else
    {
      printf("    :\t");
    }
    print_expr((Node *)tle->expr, rtable);
    printf("\n");
  }
  printf(")\n");
}

   
              
                                                       
   
void
print_slot(TupleTableSlot *slot)
{
  if (TupIsNull(slot))
  {
    printf("tuple is null.\n");
    return;
  }
  if (!slot->tts_tupleDescriptor)
  {
    printf("no tuple descriptor.\n");
    return;
  }

  debugtup(slot, NULL);
}
