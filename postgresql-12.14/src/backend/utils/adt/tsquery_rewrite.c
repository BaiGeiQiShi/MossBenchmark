                                                                            
   
                     
                                          
   
                                                                         
   
   
                  
                                             
   
                                                                            
   

#include "postgres.h"

#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "miscadmin.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"

   
                                                                
                                                                          
                                                                         
                                       
   
                                                                       
                                         
                                                    
   
static QTNode *
findeq(QTNode *node, QTNode *ex, QTNode *subs, bool *isfind)
{
                                                                   
  if ((node->sign & ex->sign) != ex->sign || node->valnode->type != ex->valnode->type)
  {
    return node;
  }

                                          
  if (node->flags & QTN_NOCHANGE)
  {
    return node;
  }

  if (node->valnode->type == QI_OPR)
  {
                                
    if (node->valnode->qoperator.oper != ex->valnode->qoperator.oper)
    {
      return node;
    }

    if (node->nchild == ex->nchild)
    {
         
                                                                    
                                                                   
         
      if (QTNEq(node, ex))
      {
                                                                   
        QTNFree(node);
        if (subs)
        {
          node = QTNCopy(subs);
          node->flags |= QTN_NOCHANGE;
        }
        else
        {
          node = NULL;
        }
        *isfind = true;
      }
    }
    else if (node->nchild > ex->nchild && ex->nchild > 0)
    {
         
                                                                         
                                                                       
                                                                         
                                                                         
                                                                       
                             
         
                                                                        
                                                                  
         
      bool *matched;
      int nmatched;
      int i, j;

                                             
      Assert(node->valnode->qoperator.oper == OP_AND || node->valnode->qoperator.oper == OP_OR);

                                                                
      matched = (bool *)palloc0(node->nchild * sizeof(bool));
      nmatched = 0;
      i = j = 0;
      while (i < node->nchild && j < ex->nchild)
      {
        int cmp = QTNodeCompare(node->child[i], ex->child[j]);

        if (cmp == 0)
        {
                      
          matched[i] = true;
          nmatched++;
          i++, j++;
        }
        else if (cmp < 0)
        {
                                                      
          i++;
        }
        else
        {
                                                                     
          break;
        }
      }

      if (nmatched == ex->nchild)
      {
                                                       
        j = 0;
        for (i = 0; i < node->nchild; i++)
        {
          if (matched[i])
          {
            QTNFree(node->child[i]);
          }
          else
          {
            node->child[j++] = node->child[i];
          }
        }

                                               
        if (subs)
        {
          subs = QTNCopy(subs);
          subs->flags |= QTN_NOCHANGE;
          node->child[j++] = subs;
        }

        node->nchild = j;

           
                                                                      
                                                                      
                                                  
           

           
                                                                       
                                                                      
                                                                      
                                                                   
                                                                    
                                                                       
                                                                     
           
        QTNSort(node);

        *isfind = true;
      }

      pfree(matched);
    }
  }
  else
  {
    Assert(node->valnode->type == QI_VAL);

    if (node->valnode->qoperand.valcrc != ex->valnode->qoperand.valcrc)
    {
      return node;
    }
    else if (QTNEq(node, ex))
    {
      QTNFree(node);
      if (subs)
      {
        node = QTNCopy(subs);
        node->flags |= QTN_NOCHANGE;
      }
      else
      {
        node = NULL;
      }
      *isfind = true;
    }
  }

  return node;
}

   
                                                                         
                                                                          
                
   
                                                            
                                                              
   
                 
       \
         
       \
          
   
static QTNode *
dofindsubquery(QTNode *root, QTNode *ex, QTNode *subs, bool *isfind)
{
                                                                           
  check_stack_depth();

                                                                       
  CHECK_FOR_INTERRUPTS();

                                
  root = findeq(root, ex, subs, isfind);

                                                               
  if (root && (root->flags & QTN_NOCHANGE) == 0 && root->valnode->type == QI_OPR)
  {
    int i, j = 0;

       
                                                                       
             
       
    for (i = 0; i < root->nchild; i++)
    {
      root->child[j] = dofindsubquery(root->child[i], ex, subs, isfind);
      if (root->child[j])
      {
        j++;
      }
    }

    root->nchild = j;

       
                                                                           
                      
       
    if (root->nchild == 0)
    {
      QTNFree(root);
      root = NULL;
    }
    else if (root->nchild == 1 && root->valnode->qoperator.oper != OP_NOT)
    {
      QTNode *nroot = root->child[0];

      pfree(root);
      root = nroot;
    }
  }

  return root;
}

   
                                                                  
   
                                                                               
   
                                                                      
                                
   
QTNode *
findsubquery(QTNode *root, QTNode *ex, QTNode *subs, bool *isfind)
{
  bool DidFind = false;

  root = dofindsubquery(root, ex, subs, &DidFind);

  if (isfind)
  {
    *isfind = DidFind;
  }

  return root;
}

Datum
tsquery_rewrite_query(PG_FUNCTION_ARGS)
{
  TSQuery query = PG_GETARG_TSQUERY_COPY(0);
  text *in = PG_GETARG_TEXT_PP(1);
  TSQuery rewrited = query;
  MemoryContext outercontext = CurrentMemoryContext;
  MemoryContext oldcontext;
  QTNode *tree;
  char *buf;
  SPIPlanPtr plan;
  Portal portal;
  bool isnull;

  if (query->size == 0)
  {
    PG_FREE_IF_COPY(in, 1);
    PG_RETURN_POINTER(rewrited);
  }

  tree = QT2QTN(GETQUERY(query), GETOPERAND(query));
  QTNTernary(tree);
  QTNSort(tree);

  buf = text_to_cstring(in);

  SPI_connect();

  if ((plan = SPI_prepare(buf, 0, NULL)) == NULL)
  {
    elog(ERROR, "SPI_prepare(\"%s\") failed", buf);
  }

  if ((portal = SPI_cursor_open(NULL, plan, NULL, NULL, true)) == NULL)
  {
    elog(ERROR, "SPI_cursor_open(\"%s\") failed", buf);
  }

  SPI_cursor_fetch(portal, true, 100);

  if (SPI_tuptable == NULL || SPI_tuptable->tupdesc->natts != 2 || SPI_gettypeid(SPI_tuptable->tupdesc, 1) != TSQUERYOID || SPI_gettypeid(SPI_tuptable->tupdesc, 2) != TSQUERYOID)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("ts_rewrite query must return two tsquery columns")));
  }

  while (SPI_processed > 0 && tree)
  {
    uint64 i;

    for (i = 0; i < SPI_processed && tree; i++)
    {
      Datum qdata = SPI_getbinval(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1, &isnull);
      Datum sdata;

      if (isnull)
      {
        continue;
      }

      sdata = SPI_getbinval(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 2, &isnull);

      if (!isnull)
      {
        TSQuery qtex = DatumGetTSQuery(qdata);
        TSQuery qtsubs = DatumGetTSQuery(sdata);
        QTNode *qex, *qsubs = NULL;

        if (qtex->size == 0)
        {
          if (qtex != (TSQuery)DatumGetPointer(qdata))
          {
            pfree(qtex);
          }
          if (qtsubs != (TSQuery)DatumGetPointer(sdata))
          {
            pfree(qtsubs);
          }
          continue;
        }

        qex = QT2QTN(GETQUERY(qtex), GETOPERAND(qtex));

        QTNTernary(qex);
        QTNSort(qex);

        if (qtsubs->size)
        {
          qsubs = QT2QTN(GETQUERY(qtsubs), GETOPERAND(qtsubs));
        }

        oldcontext = MemoryContextSwitchTo(outercontext);
        tree = findsubquery(tree, qex, qsubs, NULL);
        MemoryContextSwitchTo(oldcontext);

        QTNFree(qex);
        if (qtex != (TSQuery)DatumGetPointer(qdata))
        {
          pfree(qtex);
        }
        QTNFree(qsubs);
        if (qtsubs != (TSQuery)DatumGetPointer(sdata))
        {
          pfree(qtsubs);
        }

        if (tree)
        {
                                               
          QTNClearFlags(tree, QTN_NOCHANGE);
          QTNTernary(tree);
          QTNSort(tree);
        }
      }
    }

    SPI_freetuptable(SPI_tuptable);
    SPI_cursor_fetch(portal, true, 100);
  }

  SPI_freetuptable(SPI_tuptable);
  SPI_cursor_close(portal);
  SPI_freeplan(plan);
  SPI_finish();

  if (tree)
  {
    QTNBinary(tree);
    rewrited = QTN2QT(tree);
    QTNFree(tree);
    PG_FREE_IF_COPY(query, 0);
  }
  else
  {
    SET_VARSIZE(rewrited, HDRSIZETQ);
    rewrited->size = 0;
  }

  pfree(buf);
  PG_FREE_IF_COPY(in, 1);
  PG_RETURN_POINTER(rewrited);
}

Datum
tsquery_rewrite(PG_FUNCTION_ARGS)
{
  TSQuery query = PG_GETARG_TSQUERY_COPY(0);
  TSQuery ex = PG_GETARG_TSQUERY(1);
  TSQuery subst = PG_GETARG_TSQUERY(2);
  TSQuery rewrited = query;
  QTNode *tree, *qex, *subs = NULL;

  if (query->size == 0 || ex->size == 0)
  {
    PG_FREE_IF_COPY(ex, 1);
    PG_FREE_IF_COPY(subst, 2);
    PG_RETURN_POINTER(rewrited);
  }

  tree = QT2QTN(GETQUERY(query), GETOPERAND(query));
  QTNTernary(tree);
  QTNSort(tree);

  qex = QT2QTN(GETQUERY(ex), GETOPERAND(ex));
  QTNTernary(qex);
  QTNSort(qex);

  if (subst->size)
  {
    subs = QT2QTN(GETQUERY(subst), GETOPERAND(subst));
  }

  tree = findsubquery(tree, qex, subs, NULL);

  QTNFree(qex);
  QTNFree(subs);

  if (!tree)
  {
    SET_VARSIZE(rewrited, HDRSIZETQ);
    rewrited->size = 0;
    PG_FREE_IF_COPY(ex, 1);
    PG_FREE_IF_COPY(subst, 2);
    PG_RETURN_POINTER(rewrited);
  }
  else
  {
    QTNBinary(tree);
    rewrited = QTN2QT(tree);
    QTNFree(tree);
  }

  PG_FREE_IF_COPY(query, 0);
  PG_FREE_IF_COPY(ex, 1);
  PG_FREE_IF_COPY(subst, 2);
  PG_RETURN_POINTER(rewrited);
}
