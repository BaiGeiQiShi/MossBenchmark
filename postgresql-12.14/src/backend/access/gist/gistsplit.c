                                                                            
   
               
                                           
   
                                                                                
                                                                                
                                                                            
                                                                              
                                                                          
                                                                              
                                                                        
                                     
   
                                                     
   
   
                                                                         
                                                                        
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "access/gist_private.h"
#include "utils/rel.h"

typedef struct
{
  OffsetNumber *entries;
  int len;
  Datum *attr;
  bool *isnull;
  bool *dontcare;
} GistSplitUnion;

   
                                                                        
                                                                            
                    
   
static void
gistunionsubkeyvec(GISTSTATE *giststate, IndexTuple *itvec, GistSplitUnion *gsvp)
{
  IndexTuple *cleanedItVec;
  int i, cleanedLen = 0;

  cleanedItVec = (IndexTuple *)palloc(sizeof(IndexTuple) * gsvp->len);

  for (i = 0; i < gsvp->len; i++)
  {
    if (gsvp->dontcare && gsvp->dontcare[gsvp->entries[i]])
    {
      continue;
    }

    cleanedItVec[cleanedLen++] = itvec[gsvp->entries[i] - 1];
  }

  gistMakeUnionItVec(giststate, cleanedItVec, cleanedLen, gsvp->attr, gsvp->isnull);

  pfree(cleanedItVec);
}

   
                                                                        
                                                               
   
                                                                              
                                                                            
                                                                           
                                                                        
   
static void
gistunionsubkey(GISTSTATE *giststate, IndexTuple *itvec, GistSplitVector *spl)
{
  GistSplitUnion gsvp;

  gsvp.dontcare = spl->spl_dontcare;

  gsvp.entries = spl->splitVector.spl_left;
  gsvp.len = spl->splitVector.spl_nleft;
  gsvp.attr = spl->spl_lattr;
  gsvp.isnull = spl->spl_lisnull;

  gistunionsubkeyvec(giststate, itvec, &gsvp);

  gsvp.entries = spl->splitVector.spl_right;
  gsvp.len = spl->splitVector.spl_nright;
  gsvp.attr = spl->spl_rattr;
  gsvp.isnull = spl->spl_risnull;

  gistunionsubkeyvec(giststate, itvec, &gsvp);
}

   
                                                                           
                                                                      
              
   
                                                                      
                                                                           
              
   
                                        
   
static int
findDontCares(Relation r, GISTSTATE *giststate, GISTENTRY *valvec, GistSplitVector *spl, int attno)
{
  int i;
  GISTENTRY entry;
  int NumDontCare = 0;

     
                                                                           
                                           
     
                                                                             
                     
     
  gistentryinit(entry, spl->splitVector.spl_rdatum, r, NULL, (OffsetNumber)0, false);
  for (i = 0; i < spl->splitVector.spl_nleft; i++)
  {
    int j = spl->splitVector.spl_left[i];
    float penalty = gistpenalty(giststate, attno, &entry, false, &valvec[j], false);

    if (penalty == 0.0)
    {
      spl->spl_dontcare[j] = true;
      NumDontCare++;
    }
  }

                                                
  gistentryinit(entry, spl->splitVector.spl_ldatum, r, NULL, (OffsetNumber)0, false);
  for (i = 0; i < spl->splitVector.spl_nright; i++)
  {
    int j = spl->splitVector.spl_right[i];
    float penalty = gistpenalty(giststate, attno, &entry, false, &valvec[j], false);

    if (penalty == 0.0)
    {
      spl->spl_dontcare[j] = true;
      NumDontCare++;
    }
  }

  return NumDontCare;
}

   
                                                                            
                                                                             
           
   
static void
removeDontCares(OffsetNumber *a, int *len, const bool *dontcare)
{
  int origlen, newlen, i;
  OffsetNumber *curwpos;

  origlen = newlen = *len;
  curwpos = a;
  for (i = 0; i < origlen; i++)
  {
    OffsetNumber ai = a[i];

    if (dontcare[ai] == false)
    {
                                 
      *curwpos = ai;
      curwpos++;
    }
    else
    {
      newlen--;
    }
  }

  *len = newlen;
}

   
                                                                             
                                                                          
                                                                               
             
   
static void
placeOne(Relation r, GISTSTATE *giststate, GistSplitVector *v, IndexTuple itup, OffsetNumber off, int attno)
{
  GISTENTRY identry[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];
  bool toLeft = true;

  gistDeCompressAtt(giststate, r, itup, NULL, (OffsetNumber)0, identry, isnull);

  for (; attno < giststate->nonLeafTupdesc->natts; attno++)
  {
    float lpenalty, rpenalty;
    GISTENTRY entry;

    gistentryinit(entry, v->spl_lattr[attno], r, NULL, 0, false);
    lpenalty = gistpenalty(giststate, attno, &entry, v->spl_lisnull[attno], identry + attno, isnull[attno]);
    gistentryinit(entry, v->spl_rattr[attno], r, NULL, 0, false);
    rpenalty = gistpenalty(giststate, attno, &entry, v->spl_risnull[attno], identry + attno, isnull[attno]);

    if (lpenalty != rpenalty)
    {
      if (lpenalty > rpenalty)
      {
        toLeft = false;
      }
      break;
    }
  }

  if (toLeft)
  {
    v->splitVector.spl_left[v->splitVector.spl_nleft++] = off;
  }
  else
  {
    v->splitVector.spl_right[v->splitVector.spl_nright++] = off;
  }
}

#define SWAPVAR(s, d, t)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (t) = (s);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    (s) = (d);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    (d) = (t);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

   
                                                                         
                                                                            
          
   
                                                                           
                                                                              
                                                
   
                                                                         
                                                                         
                                  
   
static void
supportSecondarySplit(Relation r, GISTSTATE *giststate, int attno, GIST_SPLITVEC *sv, Datum oldL, Datum oldR)
{
  bool leaveOnLeft = true, tmpBool;
  GISTENTRY entryL, entryR, entrySL, entrySR;

  gistentryinit(entryL, oldL, r, NULL, 0, false);
  gistentryinit(entryR, oldR, r, NULL, 0, false);
  gistentryinit(entrySL, sv->spl_ldatum, r, NULL, 0, false);
  gistentryinit(entrySR, sv->spl_rdatum, r, NULL, 0, false);

  if (sv->spl_ldatum_exists && sv->spl_rdatum_exists)
  {
    float penalty1, penalty2;

    penalty1 = gistpenalty(giststate, attno, &entryL, false, &entrySL, false) + gistpenalty(giststate, attno, &entryR, false, &entrySR, false);
    penalty2 = gistpenalty(giststate, attno, &entryL, false, &entrySR, false) + gistpenalty(giststate, attno, &entryR, false, &entrySL, false);

    if (penalty1 > penalty2)
    {
      leaveOnLeft = false;
    }
  }
  else
  {
    GISTENTRY *entry1 = (sv->spl_ldatum_exists) ? &entryL : &entryR;
    float penalty1, penalty2;

       
                                                                          
                                                                          
                                                                       
                                                                       
                                                                      
                                                                          
                                                                          
                                                                    
       
    penalty1 = gistpenalty(giststate, attno, entry1, false, &entrySL, false);
    penalty2 = gistpenalty(giststate, attno, entry1, false, &entrySR, false);

    if (penalty1 < penalty2)
    {
      leaveOnLeft = (sv->spl_ldatum_exists) ? true : false;
    }
    else
    {
      leaveOnLeft = (sv->spl_rdatum_exists) ? true : false;
    }
  }

  if (leaveOnLeft == false)
  {
       
                           
       
    OffsetNumber *off, noff;
    Datum datum;

    SWAPVAR(sv->spl_left, sv->spl_right, off);
    SWAPVAR(sv->spl_nleft, sv->spl_nright, noff);
    SWAPVAR(sv->spl_ldatum, sv->spl_rdatum, datum);
    gistentryinit(entrySL, sv->spl_ldatum, r, NULL, 0, false);
    gistentryinit(entrySR, sv->spl_rdatum, r, NULL, 0, false);
  }

  if (sv->spl_ldatum_exists)
  {
    gistMakeUnionKey(giststate, attno, &entryL, false, &entrySL, false, &sv->spl_ldatum, &tmpBool);
  }

  if (sv->spl_rdatum_exists)
  {
    gistMakeUnionKey(giststate, attno, &entryR, false, &entrySR, false, &sv->spl_rdatum, &tmpBool);
  }

  sv->spl_ldatum_exists = sv->spl_rdatum_exists = false;
}

   
                                                          
                                                                          
                                                                      
   
static void
genericPickSplit(GISTSTATE *giststate, GistEntryVector *entryvec, GIST_SPLITVEC *v, int attno)
{
  OffsetNumber i, maxoff;
  int nbytes;
  GistEntryVector *evec;

  maxoff = entryvec->n - 1;

  nbytes = (maxoff + 2) * sizeof(OffsetNumber);

  v->spl_left = (OffsetNumber *)palloc(nbytes);
  v->spl_right = (OffsetNumber *)palloc(nbytes);
  v->spl_nleft = v->spl_nright = 0;

  for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
  {
    if (i <= (maxoff - FirstOffsetNumber + 1) / 2)
    {
      v->spl_left[v->spl_nleft] = i;
      v->spl_nleft++;
    }
    else
    {
      v->spl_right[v->spl_nright] = i;
      v->spl_nright++;
    }
  }

     
                                     
     
  evec = palloc(sizeof(GISTENTRY) * entryvec->n + GEVHDRSZ);

  evec->n = v->spl_nleft;
  memcpy(evec->vector, entryvec->vector + FirstOffsetNumber, sizeof(GISTENTRY) * evec->n);
  v->spl_ldatum = FunctionCall2Coll(&giststate->unionFn[attno], giststate->supportCollation[attno], PointerGetDatum(evec), PointerGetDatum(&nbytes));

  evec->n = v->spl_nright;
  memcpy(evec->vector, entryvec->vector + FirstOffsetNumber + v->spl_nleft, sizeof(GISTENTRY) * evec->n);
  v->spl_rdatum = FunctionCall2Coll(&giststate->unionFn[attno], giststate->supportCollation[attno], PointerGetDatum(evec), PointerGetDatum(&nbytes));
}

   
                                                                     
                
   
                                                                           
                                                                        
   
                                                                      
                                                                     
                                                                  
   
                                                                           
                                                                        
                                                                             
                                                                         
                                                                            
   
                                                                  
   
static bool
gistUserPicksplit(Relation r, GistEntryVector *entryvec, int attno, GistSplitVector *v, IndexTuple *itup, int len, GISTSTATE *giststate)
{
  GIST_SPLITVEC *sv = &v->splitVector;

     
                                                                          
                                                                   
     
  sv->spl_ldatum_exists = (v->spl_lisnull[attno]) ? false : true;
  sv->spl_rdatum_exists = (v->spl_risnull[attno]) ? false : true;
  sv->spl_ldatum = v->spl_lattr[attno];
  sv->spl_rdatum = v->spl_rattr[attno];

     
                                                                           
                                                                
     
  FunctionCall2Coll(&giststate->picksplitFn[attno], giststate->supportCollation[attno], PointerGetDatum(entryvec), PointerGetDatum(sv));

  if (sv->spl_nleft == 0 || sv->spl_nright == 0)
  {
       
                                                                          
                                                        
       
    ereport(DEBUG1, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("picksplit method for column %d of index \"%s\" failed", attno + 1, RelationGetRelationName(r)), errhint("The index is not optimal. To optimize it, contact a developer, or try to use the column as the second one in the CREATE INDEX command.")));

       
                                                                   
                                                              
       
    sv->spl_ldatum_exists = (v->spl_lisnull[attno]) ? false : true;
    sv->spl_rdatum_exists = (v->spl_risnull[attno]) ? false : true;
    sv->spl_ldatum = v->spl_lattr[attno];
    sv->spl_rdatum = v->spl_rattr[attno];

                            
    genericPickSplit(giststate, entryvec, sv, attno);
  }
  else
  {
                                                       
    if (sv->spl_left[sv->spl_nleft - 1] == InvalidOffsetNumber)
    {
      sv->spl_left[sv->spl_nleft - 1] = (OffsetNumber)(entryvec->n - 1);
    }
    if (sv->spl_right[sv->spl_nright - 1] == InvalidOffsetNumber)
    {
      sv->spl_right[sv->spl_nright - 1] = (OffsetNumber)(entryvec->n - 1);
    }
  }

                                                                   
  if (sv->spl_ldatum_exists || sv->spl_rdatum_exists)
  {
    supportSecondarySplit(r, giststate, attno, sv, v->spl_lattr[attno], v->spl_rattr[attno]);
  }

                                                                
  v->spl_lattr[attno] = sv->spl_ldatum;
  v->spl_rattr[attno] = sv->spl_rdatum;
  v->spl_lisnull[attno] = false;
  v->spl_risnull[attno] = false;

     
                                                                             
                    
     
  v->spl_dontcare = NULL;

  if (attno + 1 < giststate->nonLeafTupdesc->natts)
  {
    int NumDontCare;

       
                                                                         
                                                                   
                                      
       
    if (gistKeyIsEQ(giststate, attno, sv->spl_ldatum, sv->spl_rdatum))
    {
      return true;
    }

       
                                                                          
                                                   
       
    v->spl_dontcare = (bool *)palloc0(sizeof(bool) * (entryvec->n + 1));

    NumDontCare = findDontCares(r, giststate, entryvec->vector, v, attno);

    if (NumDontCare > 0)
    {
         
                                                             
         
      removeDontCares(sv->spl_left, &sv->spl_nleft, v->spl_dontcare);
      removeDontCares(sv->spl_right, &sv->spl_nright, v->spl_dontcare);

         
                                                                     
                                                                      
                                                                    
                                                                       
                                                                   
                                                                         
                                                                       
                                                                 
                                                                     
                                                                         
                                                                     
                                               
         
      if (sv->spl_nleft == 0 || sv->spl_nright == 0)
      {
        v->spl_dontcare = NULL;
        return true;
      }

         
                                                                       
                                                                     
                                                                         
                                                            
                                                       
         
      gistunionsubkey(giststate, itup, v);

      if (NumDontCare == 1)
      {
           
                                                                   
                                                                      
                                                        
                                                                   
                                                  
           
        OffsetNumber toMove;

                         
        for (toMove = FirstOffsetNumber; toMove < entryvec->n; toMove++)
        {
          if (v->spl_dontcare[toMove])
          {
            break;
          }
        }
        Assert(toMove < entryvec->n);

                                               
        placeOne(r, giststate, v, itup[toMove - 1], toMove, attno + 1);

           
                                                                     
                                                                  
                                                                     
           
      }
      else
      {
        return true;
      }
    }
  }

  return false;
}

   
                             
   
static void
gistSplitHalf(GIST_SPLITVEC *v, int len)
{
  int i;

  v->spl_nright = v->spl_nleft = 0;
  v->spl_left = (OffsetNumber *)palloc(len * sizeof(OffsetNumber));
  v->spl_right = (OffsetNumber *)palloc(len * sizeof(OffsetNumber));
  for (i = 1; i <= len; i++)
  {
    if (i < len / 2)
    {
      v->spl_right[v->spl_nright++] = i;
    }
    else
    {
      v->spl_left[v->spl_nleft++] = i;
    }
  }

                                                              
}

   
                                                                 
   
                     
                          
                                              
                                                                   
                                          
                                    
                                                      
   
                                                                           
                                                                         
                                                                           
                                                                 
                                                                            
                                               
   
                                                                         
                                                  
   
void
gistSplitByKey(Relation r, Page page, IndexTuple *itup, int len, GISTSTATE *giststate, GistSplitVector *v, int attno)
{
  GistEntryVector *entryvec;
  OffsetNumber *offNullTuples;
  int nOffNullTuples = 0;
  int i;

                                                                   
                                                              
  entryvec = palloc(GEVHDRSZ + (len + 1) * sizeof(GISTENTRY));
  entryvec->n = len + 1;
  offNullTuples = (OffsetNumber *)palloc(len * sizeof(OffsetNumber));

  for (i = 1; i <= len; i++)
  {
    Datum datum;
    bool IsNull;

    datum = index_getattr(itup[i - 1], attno + 1, giststate->leafTupdesc, &IsNull);
    gistdentryinit(giststate, attno, &(entryvec->vector[i]), datum, r, page, i, false, IsNull);
    if (IsNull)
    {
      offNullTuples[nOffNullTuples++] = i;
    }
  }

  if (nOffNullTuples == len)
  {
       
                                                                        
                                                                          
                           
       
    v->spl_risnull[attno] = v->spl_lisnull[attno] = true;

    if (attno + 1 < giststate->nonLeafTupdesc->natts)
    {
      gistSplitByKey(r, page, itup, len, giststate, v, attno + 1);
    }
    else
    {
      gistSplitHalf(&v->splitVector, len);
    }
  }
  else if (nOffNullTuples > 0)
  {
    int j = 0;

       
                                                                         
                                                  
       
    v->splitVector.spl_right = offNullTuples;
    v->splitVector.spl_nright = nOffNullTuples;
    v->spl_risnull[attno] = true;

    v->splitVector.spl_left = (OffsetNumber *)palloc(len * sizeof(OffsetNumber));
    v->splitVector.spl_nleft = 0;
    for (i = 1; i <= len; i++)
    {
      if (j < v->splitVector.spl_nright && offNullTuples[j] == i)
      {
        j++;
      }
      else
      {
        v->splitVector.spl_left[v->splitVector.spl_nleft++] = i;
      }
    }

                                                                         
    if (attno == 0 && giststate->nonLeafTupdesc->natts == 1)
    {
      v->spl_dontcare = NULL;
      gistunionsubkey(giststate, itup, v);
    }
  }
  else
  {
       
                                                                     
       
    if (gistUserPicksplit(r, entryvec, attno, v, itup, len, giststate))
    {
         
                                                               
                                                                       
         
      Assert(attno + 1 < giststate->nonLeafTupdesc->natts);

      if (v->spl_dontcare == NULL)
      {
           
                                                                       
                                                        
           
        gistSplitByKey(r, page, itup, len, giststate, v, attno + 1);
      }
      else
      {
           
                                                                    
                                                                      
           
        IndexTuple *newitup = (IndexTuple *)palloc(len * sizeof(IndexTuple));
        OffsetNumber *map = (OffsetNumber *)palloc(len * sizeof(OffsetNumber));
        int newlen = 0;
        GIST_SPLITVEC backupSplit;

        for (i = 0; i < len; i++)
        {
          if (v->spl_dontcare[i + 1])
          {
            newitup[newlen] = itup[i];
            map[newlen] = i + 1;
            newlen++;
          }
        }

        Assert(newlen > 0);

           
                                                                     
                                                         
           
        backupSplit = v->splitVector;
        backupSplit.spl_left = (OffsetNumber *)palloc(sizeof(OffsetNumber) * len);
        memcpy(backupSplit.spl_left, v->splitVector.spl_left, sizeof(OffsetNumber) * v->splitVector.spl_nleft);
        backupSplit.spl_right = (OffsetNumber *)palloc(sizeof(OffsetNumber) * len);
        memcpy(backupSplit.spl_right, v->splitVector.spl_right, sizeof(OffsetNumber) * v->splitVector.spl_nright);

                                                                   
        gistSplitByKey(r, page, newitup, newlen, giststate, v, attno + 1);

                                                                 
        for (i = 0; i < v->splitVector.spl_nleft; i++)
        {
          backupSplit.spl_left[backupSplit.spl_nleft++] = map[v->splitVector.spl_left[i] - 1];
        }
        for (i = 0; i < v->splitVector.spl_nright; i++)
        {
          backupSplit.spl_right[backupSplit.spl_nright++] = map[v->splitVector.spl_right[i] - 1];
        }

        v->splitVector = backupSplit;
      }
    }
  }

     
                                                                        
                                                                            
                                                                       
                                                                            
                                                                            
                                                                            
     
                                                                            
                                                                       
                                                                           
                                                                        
             
     
  if (attno == 0 && giststate->nonLeafTupdesc->natts > 1)
  {
    v->spl_dontcare = NULL;
    gistunionsubkey(giststate, itup, v);
  }
}
