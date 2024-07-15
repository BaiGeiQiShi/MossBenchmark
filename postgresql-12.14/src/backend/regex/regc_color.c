   
                           
                                        
   
                                                                 
   
                                                                            
                                                                            
                                                                          
                       
   
                                                                        
                                                                
                                                                          
                                                        
   
                                                                           
                                                             
   
                                                                              
                                                                            
                                                                           
                                                                          
                                                                       
                                                                               
                                                                            
                                                                           
                                                                          
                                              
   
                                  
   
   
                                                                           
                                                                       
   

#define CISERR() VISERR(cm->v)
#define CERR(e) VERR(cm->v, (e))

   
                                
   
static void
initcm(struct vars *v, struct colormap *cm)
{
  struct colordesc *cd;

  cm->magic = CMMAGIC;
  cm->v = v;

  cm->ncds = NINLINECDS;
  cm->cd = cm->cdspace;
  cm->max = 0;
  cm->free = 0;

  cd = cm->cd;                    
  cd->nschrs = MAX_SIMPLE_CHR - CHR_MIN + 1;
  cd->nuchrs = 1;
  cd->sub = NOSUB;
  cd->arcs = NULL;
  cd->firstchr = CHR_MIN;
  cd->flags = 0;

  cm->locolormap = (color *)MALLOC((MAX_SIMPLE_CHR - CHR_MIN + 1) * sizeof(color));
  if (cm->locolormap == NULL)
  {
    CERR(REG_ESPACE);
    cm->cmranges = NULL;                                    
    cm->hicolormap = NULL;
    return;
  }
                                               
  memset(cm->locolormap, WHITE, (MAX_SIMPLE_CHR - CHR_MIN + 1) * sizeof(color));

  memset(cm->classbits, 0, sizeof(cm->classbits));
  cm->numcmranges = 0;
  cm->cmranges = NULL;
  cm->maxarrayrows = 4;                                   
  cm->hiarrayrows = 1;                                              
  cm->hiarraycols = 1;
  cm->hicolormap = (color *)MALLOC(cm->maxarrayrows * sizeof(color));
  if (cm->hicolormap == NULL)
  {
    CERR(REG_ESPACE);
    return;
  }
                                                          
  cm->hicolormap[0] = WHITE;
}

   
                                                            
   
static void
freecm(struct colormap *cm)
{
  cm->magic = 0;
  if (cm->cd != cm->cdspace)
  {
    FREE(cm->cd);
  }
  if (cm->locolormap != NULL)
  {
    FREE(cm->locolormap);
  }
  if (cm->cmranges != NULL)
  {
    FREE(cm->cmranges);
  }
  if (cm->hicolormap != NULL)
  {
    FREE(cm->hicolormap);
  }
}

   
                                             
   
color
pg_reg_getcolor(struct colormap *cm, chr c)
{
  int rownum, colnum, low, high;

                                                     
  assert(c > MAX_SIMPLE_CHR);

     
                                                                             
                    
     
  rownum = 0;                                      
  low = 0;
  high = cm->numcmranges;
  while (low < high)
  {
    int middle = low + (high - low) / 2;
    const colormaprange *cmr = &cm->cmranges[middle];

    if (c < cmr->cmin)
    {
      high = middle;
    }
    else if (c > cmr->cmax)
    {
      low = middle + 1;
    }
    else
    {
      rownum = cmr->rownum;                    
      break;
    }
  }

     
                                                                 
     
  if (cm->hiarraycols > 1)
  {
    colnum = cclass_column_index(cm, c);
    return cm->hicolormap[rownum * cm->hiarraycols + colnum];
  }
  else
  {
                                           
    return cm->hicolormap[rownum];
  }
}

   
                                                 
   
static color
maxcolor(struct colormap *cm)
{
  if (CISERR())
  {
    return COLORLESS;
  }

  return (color)cm->max;
}

   
                                                          
                                        
   
static color                          
newcolor(struct colormap *cm)
{
  struct colordesc *cd;
  size_t n;

  if (CISERR())
  {
    return COLORLESS;
  }

  if (cm->free != 0)
  {
    assert(cm->free > 0);
    assert((size_t)cm->free < cm->ncds);
    cd = &cm->cd[cm->free];
    assert(UNUSEDCOLOR(cd));
    assert(cd->arcs == NULL);
    cm->free = cd->sub;
  }
  else if (cm->max < cm->ncds - 1)
  {
    cm->max++;
    cd = &cm->cd[cm->max];
  }
  else
  {
                                  
    struct colordesc *newCd;

    if (cm->max == MAX_COLOR)
    {
      CERR(REG_ECOLORS);
      return COLORLESS;                      
    }

    n = cm->ncds * 2;
    if (n > MAX_COLOR + 1)
    {
      n = MAX_COLOR + 1;
    }
    if (cm->cd == cm->cdspace)
    {
      newCd = (struct colordesc *)MALLOC(n * sizeof(struct colordesc));
      if (newCd != NULL)
      {
        memcpy(VS(newCd), VS(cm->cdspace), cm->ncds * sizeof(struct colordesc));
      }
    }
    else
    {
      newCd = (struct colordesc *)REALLOC(cm->cd, n * sizeof(struct colordesc));
    }
    if (newCd == NULL)
    {
      CERR(REG_ESPACE);
      return COLORLESS;
    }
    cm->cd = newCd;
    cm->ncds = n;
    assert(cm->max < cm->ncds - 1);
    cm->max++;
    cd = &cm->cd[cm->max];
  }

  cd->nschrs = 0;
  cd->nuchrs = 0;
  cd->sub = NOSUB;
  cd->arcs = NULL;
  cd->firstchr = CHR_MIN;                                  
  cd->flags = 0;

  return (color)(cd - cm->cd);
}

   
                                                            
   
static void
freecolor(struct colormap *cm, color co)
{
  struct colordesc *cd = &cm->cd[co];
  color pco, nco;                        

  assert(co >= 0);
  if (co == WHITE)
  {
    return;
  }

  assert(cd->arcs == NULL);
  assert(cd->sub == NOSUB);
  assert(cd->nschrs == 0);
  assert(cd->nuchrs == 0);
  cd->flags = FREECOL;

  if ((size_t)co == cm->max)
  {
    while (cm->max > WHITE && UNUSEDCOLOR(&cm->cd[cm->max]))
    {
      cm->max--;
    }
    assert(cm->free >= 0);
    while ((size_t)cm->free > cm->max)
    {
      cm->free = cm->cd[cm->free].sub;
    }
    if (cm->free > 0)
    {
      assert(cm->free < cm->max);
      pco = cm->free;
      nco = cm->cd[pco].sub;
      while (nco > 0)
      {
        if ((size_t)nco > cm->max)
        {
                                             
          nco = cm->cd[nco].sub;
          cm->cd[pco].sub = nco;
        }
        else
        {
          assert(nco < cm->max);
          pco = nco;
          nco = cm->cd[pco].sub;
        }
      }
    }
  }
  else
  {
    cd->sub = cm->free;
    cm->free = (color)(cd - cm->cd);
  }
}

   
                                                                      
   
static color
pseudocolor(struct colormap *cm)
{
  color co;
  struct colordesc *cd;

  co = newcolor(cm);
  if (CISERR())
  {
    return COLORLESS;
  }
  cd = &cm->cd[co];
  cd->nschrs = 0;
  cd->nuchrs = 1;                                     
  cd->sub = NOSUB;
  cd->arcs = NULL;
  cd->firstchr = CHR_MIN;
  cd->flags = PSEUDO;
  return co;
}

   
                                                                 
   
                                                             
   
static color
subcolor(struct colormap *cm, chr c)
{
  color co;                          
  color sco;                   

  assert(c <= MAX_SIMPLE_CHR);

  co = cm->locolormap[c - CHR_MIN];
  sco = newsub(cm, co);
  if (CISERR())
  {
    return COLORLESS;
  }
  assert(sco != COLORLESS);

  if (co == sco)                                  
  {
    return co;                        
  }
  cm->cd[co].nschrs--;
  if (cm->cd[sco].nschrs == 0)
  {
    cm->cd[sco].firstchr = c;
  }
  cm->cd[sco].nschrs++;
  cm->locolormap[c - CHR_MIN] = sco;
  return sco;
}

   
                                                                              
   
                                                                          
                                                                          
   
static color
subcolorhi(struct colormap *cm, color *pco)
{
  color co;                              
  color sco;                   

  co = *pco;
  sco = newsub(cm, co);
  if (CISERR())
  {
    return COLORLESS;
  }
  assert(sco != COLORLESS);

  if (co == sco)                                  
  {
    return co;                        
  }
  cm->cd[co].nuchrs--;
  cm->cd[sco].nuchrs++;
  *pco = sco;
  return sco;
}

   
                                                               
   
static color
newsub(struct colormap *cm, color co)
{
  color sco;                   

  sco = cm->cd[co].sub;
  if (sco == NOSUB)
  {                                 
                                                                      
    if ((cm->cd[co].nschrs + cm->cd[co].nuchrs) == 1)
    {
      return co;
    }
    sco = newcolor(cm);                           
    if (sco == COLORLESS)
    {
      assert(CISERR());
      return COLORLESS;
    }
    cm->cd[co].sub = sco;
    cm->cd[sco].sub = sco;                                   
  }
  assert(sco != NOSUB);

  return sco;
}

   
                                                                           
   
                                                               
   
static int
newhicolorrow(struct colormap *cm, int oldrow)
{
  int newrow = cm->hiarrayrows;
  color *newrowptr;
  int i;

                                                                   
  if (newrow >= cm->maxarrayrows)
  {
    color *newarray;

    if (cm->maxarrayrows >= INT_MAX / (cm->hiarraycols * 2))
    {
      CERR(REG_ESPACE);
      return 0;
    }
    newarray = (color *)REALLOC(cm->hicolormap, cm->maxarrayrows * 2 * cm->hiarraycols * sizeof(color));
    if (newarray == NULL)
    {
      CERR(REG_ESPACE);
      return 0;
    }
    cm->hicolormap = newarray;
    cm->maxarrayrows *= 2;
  }
  cm->hiarrayrows++;

                         
  newrowptr = &cm->hicolormap[newrow * cm->hiarraycols];
  memcpy(newrowptr, &cm->hicolormap[oldrow * cm->hiarraycols], cm->hiarraycols * sizeof(color));

                                                                       
  for (i = 0; i < cm->hiarraycols; i++)
  {
    cm->cd[newrowptr[i]].nuchrs++;
  }

  return newrow;
}

   
                                                                     
   
                                                                          
   
static void
newhicolorcols(struct colormap *cm)
{
  color *newarray;
  int r, c;

  if (cm->hiarraycols >= INT_MAX / (cm->maxarrayrows * 2))
  {
    CERR(REG_ESPACE);
    return;
  }
  newarray = (color *)REALLOC(cm->hicolormap, cm->maxarrayrows * cm->hiarraycols * 2 * sizeof(color));
  if (newarray == NULL)
  {
    CERR(REG_ESPACE);
    return;
  }
  cm->hicolormap = newarray;

                                                                        
                                                                      
  for (r = cm->hiarrayrows - 1; r >= 0; r--)
  {
    color *oldrowptr = &newarray[r * cm->hiarraycols];
    color *newrowptr = &newarray[r * cm->hiarraycols * 2];
    color *newrowptr2 = newrowptr + cm->hiarraycols;

    for (c = 0; c < cm->hiarraycols; c++)
    {
      color co = oldrowptr[c];

      newrowptr[c] = newrowptr2[c] = co;
      cm->cd[co].nuchrs++;
    }
  }

  cm->hiarraycols *= 2;
}

   
                                                                       
   
                                                                  
                                                      
   
                                                                   
                                                                    
                                                                      
                                                      
   
static void
subcolorcvec(struct vars *v, struct cvec *cv, struct state *lp, struct state *rp)
{
  struct colormap *cm = v->cm;
  color lastsubcolor = COLORLESS;
  chr ch, from, to;
  const chr *p;
  int i;

                           
  for (p = cv->chrs, i = cv->nchrs; i > 0; p++, i--)
  {
    ch = *p;
    subcoloronechr(v, ch, lp, rp, &lastsubcolor);
    NOERR();
  }

                      
  for (p = cv->ranges, i = cv->nranges; i > 0; p += 2, i--)
  {
    from = *p;
    to = *(p + 1);
    if (from <= MAX_SIMPLE_CHR)
    {
                                                
      chr lim = (to <= MAX_SIMPLE_CHR) ? to : MAX_SIMPLE_CHR;

      while (from <= lim)
      {
        color sco = subcolor(cm, from);

        NOERR();
        if (sco != lastsubcolor)
        {
          newarc(v->nfa, PLAIN, sco, lp, rp);
          NOERR();
          lastsubcolor = sco;
        }
        from++;
      }
    }
                                                                     
    if (from < to)
    {
      subcoloronerange(v, from, to, lp, rp, &lastsubcolor);
    }
    else if (from == to)
    {
      subcoloronechr(v, from, lp, rp, &lastsubcolor);
    }
    NOERR();
  }

                                   
  if (cv->cclasscode >= 0)
  {
    int classbit;
    color *pco;
    int r, c;

                                                                           
    if (cm->classbits[cv->cclasscode] == 0)
    {
      cm->classbits[cv->cclasscode] = cm->hiarraycols;
      newhicolorcols(cm);
      NOERR();
    }
                                                                         
    classbit = cm->classbits[cv->cclasscode];
    pco = cm->hicolormap;
    for (r = 0; r < cm->hiarrayrows; r++)
    {
      for (c = 0; c < cm->hiarraycols; c++)
      {
        if (c & classbit)
        {
          color sco = subcolorhi(cm, pco);

          NOERR();
                                     
          if (sco != lastsubcolor)
          {
            newarc(v->nfa, PLAIN, sco, lp, rp);
            NOERR();
            lastsubcolor = sco;
          }
        }
        pco++;
      }
    }
  }
}

   
                                                               
   
                                                                             
                                                                             
                                                     
   
static void
subcoloronechr(struct vars *v, chr ch, struct state *lp, struct state *rp, color *lastsubcolor)
{
  struct colormap *cm = v->cm;
  colormaprange *newranges;
  int numnewranges;
  colormaprange *oldrange;
  int oldrangen;
  int newrow;

                                   
  if (ch <= MAX_SIMPLE_CHR)
  {
    color sco = subcolor(cm, ch);

    NOERR();
    if (sco != *lastsubcolor)
    {
      newarc(v->nfa, PLAIN, sco, lp, rp);
      *lastsubcolor = sco;
    }
    return;
  }

     
                                                                             
                                                            
     
  newranges = (colormaprange *)MALLOC((cm->numcmranges + 2) * sizeof(colormaprange));
  if (newranges == NULL)
  {
    CERR(REG_ESPACE);
    return;
  }
  numnewranges = 0;

                                          
  for (oldrange = cm->cmranges, oldrangen = 0; oldrangen < cm->numcmranges; oldrange++, oldrangen++)
  {
    if (oldrange->cmax >= ch)
    {
      break;
    }
    newranges[numnewranges++] = *oldrange;
  }

                                              
  if (oldrangen >= cm->numcmranges || oldrange->cmin > ch)
  {
                                                                   
    newranges[numnewranges].cmin = ch;
    newranges[numnewranges].cmax = ch;
                                                              
    newranges[numnewranges].rownum = newrow = newhicolorrow(cm, 0);
    numnewranges++;
  }
  else if (oldrange->cmin == oldrange->cmax)
  {
                                                              
    newranges[numnewranges++] = *oldrange;
    newrow = oldrange->rownum;
                                                  
    oldrange++, oldrangen++;
  }
  else
  {
                                                               
    if (ch > oldrange->cmin)
    {
                                                
      newranges[numnewranges].cmin = oldrange->cmin;
      newranges[numnewranges].cmax = ch - 1;
      newranges[numnewranges].rownum = oldrange->rownum;
      numnewranges++;
    }
                                                                   
    newranges[numnewranges].cmin = ch;
    newranges[numnewranges].cmax = ch;
    newranges[numnewranges].rownum = newrow = newhicolorrow(cm, oldrange->rownum);
    numnewranges++;
    if (ch < oldrange->cmax)
    {
                                               
      newranges[numnewranges].cmin = ch + 1;
      newranges[numnewranges].cmax = oldrange->cmax;
                                                                       
      newranges[numnewranges].rownum = (ch > oldrange->cmin) ? newhicolorrow(cm, oldrange->rownum) : oldrange->rownum;
      numnewranges++;
    }
                                                  
    oldrange++, oldrangen++;
  }

                                                         
  subcoloronerow(v, newrow, lp, rp, lastsubcolor);

                                         
  for (; oldrangen < cm->numcmranges; oldrange++, oldrangen++)
  {
    newranges[numnewranges++] = *oldrange;
  }

                                                       
  assert(numnewranges <= (cm->numcmranges + 2));

                                                          
  if (cm->cmranges != NULL)
  {
    FREE(cm->cmranges);
  }
  cm->cmranges = newranges;
  cm->numcmranges = numnewranges;
}

   
                                                              
   
static void
subcoloronerange(struct vars *v, chr from, chr to, struct state *lp, struct state *rp, color *lastsubcolor)
{
  struct colormap *cm = v->cm;
  colormaprange *newranges;
  int numnewranges;
  colormaprange *oldrange;
  int oldrangen;
  int newrow;

                                                       
  assert(from > MAX_SIMPLE_CHR);
  assert(from < to);

     
                                                                             
                                                                       
     
  newranges = (colormaprange *)MALLOC((cm->numcmranges * 2 + 1) * sizeof(colormaprange));
  if (newranges == NULL)
  {
    CERR(REG_ESPACE);
    return;
  }
  numnewranges = 0;

                                          
  for (oldrange = cm->cmranges, oldrangen = 0; oldrangen < cm->numcmranges; oldrange++, oldrangen++)
  {
    if (oldrange->cmax >= from)
    {
      break;
    }
    newranges[numnewranges++] = *oldrange;
  }

     
                                                                          
                                                                          
                            
     
  while (oldrangen < cm->numcmranges && oldrange->cmin <= to)
  {
    if (from < oldrange->cmin)
    {
                                                                        
      newranges[numnewranges].cmin = from;
      newranges[numnewranges].cmax = oldrange->cmin - 1;
                                                                
      newranges[numnewranges].rownum = newrow = newhicolorrow(cm, 0);
      numnewranges++;
                                                             
      subcoloronerow(v, newrow, lp, rp, lastsubcolor);
                                                                      
      from = oldrange->cmin;
    }

    if (from <= oldrange->cmin && to >= oldrange->cmax)
    {
                                                                    
      newranges[numnewranges++] = *oldrange;
      newrow = oldrange->rownum;
      from = oldrange->cmax + 1;
    }
    else
    {
                                                             
      if (from > oldrange->cmin)
      {
                                                        
        newranges[numnewranges].cmin = oldrange->cmin;
        newranges[numnewranges].cmax = from - 1;
        newranges[numnewranges].rownum = oldrange->rownum;
        numnewranges++;
      }
                                                                  
      newranges[numnewranges].cmin = from;
      newranges[numnewranges].cmax = (to < oldrange->cmax) ? to : oldrange->cmax;
      newranges[numnewranges].rownum = newrow = newhicolorrow(cm, oldrange->rownum);
      numnewranges++;
      if (to < oldrange->cmax)
      {
                                                       
        newranges[numnewranges].cmin = to + 1;
        newranges[numnewranges].cmax = oldrange->cmax;
                                                                         
        newranges[numnewranges].rownum = (from > oldrange->cmin) ? newhicolorrow(cm, oldrange->rownum) : oldrange->rownum;
        numnewranges++;
      }
      from = oldrange->cmax + 1;
    }
                                                           
    subcoloronerow(v, newrow, lp, rp, lastsubcolor);
                                                  
    oldrange++, oldrangen++;
  }

  if (from <= to)
  {
                                                                      
    newranges[numnewranges].cmin = from;
    newranges[numnewranges].cmax = to;
                                                              
    newranges[numnewranges].rownum = newrow = newhicolorrow(cm, 0);
    numnewranges++;
                                                           
    subcoloronerow(v, newrow, lp, rp, lastsubcolor);
  }

                                         
  for (; oldrangen < cm->numcmranges; oldrange++, oldrangen++)
  {
    newranges[numnewranges++] = *oldrange;
  }

                                                       
  assert(numnewranges <= (cm->numcmranges * 2 + 1));

                                                          
  if (cm->cmranges != NULL)
  {
    FREE(cm->cmranges);
  }
  cm->cmranges = newranges;
  cm->numcmranges = numnewranges;
}

   
                                                                                
   
static void
subcoloronerow(struct vars *v, int rownum, struct state *lp, struct state *rp, color *lastsubcolor)
{
  struct colormap *cm = v->cm;
  color *pco;
  int i;

                                                             
  pco = &cm->hicolormap[rownum * cm->hiarraycols];
  for (i = 0; i < cm->hiarraycols; pco++, i++)
  {
    color sco = subcolorhi(cm, pco);

    NOERR();
                                
    if (sco != *lastsubcolor)
    {
      newarc(v->nfa, PLAIN, sco, lp, rp);
      NOERR();
      *lastsubcolor = sco;
    }
  }
}

   
                                               
   
static void
okcolors(struct nfa *nfa, struct colormap *cm)
{
  struct colordesc *cd;
  struct colordesc *end = CDEND(cm);
  struct colordesc *scd;
  struct arc *a;
  color co;
  color sco;

  for (cd = cm->cd, co = 0; cd < end; cd++, co++)
  {
    sco = cd->sub;
    if (UNUSEDCOLOR(cd) || sco == NOSUB)
    {
                                              
    }
    else if (sco == co)
    {
                                                
    }
    else if (cd->nschrs == 0 && cd->nuchrs == 0)
    {
                                                           
      cd->sub = NOSUB;
      scd = &cm->cd[sco];
      assert(scd->nschrs > 0 || scd->nuchrs > 0);
      assert(scd->sub == sco);
      scd->sub = NOSUB;
      while ((a = cd->arcs) != NULL)
      {
        assert(a->co == co);
        uncolorchain(cm, a);
        a->co = sco;
        colorchain(cm, a);
      }
      freecolor(cm, co);
    }
    else
    {
                                                          
      cd->sub = NOSUB;
      scd = &cm->cd[sco];
      assert(scd->nschrs > 0 || scd->nuchrs > 0);
      assert(scd->sub == sco);
      scd->sub = NOSUB;
      for (a = cd->arcs; a != NULL; a = a->colorchain)
      {
        assert(a->co == co);
        newarc(nfa, a->type, sco, a->from, a->to);
      }
    }
  }
}

   
                                                             
   
static void
colorchain(struct colormap *cm, struct arc *a)
{
  struct colordesc *cd = &cm->cd[a->co];

  if (cd->arcs != NULL)
  {
    cd->arcs->colorchainRev = a;
  }
  a->colorchain = cd->arcs;
  a->colorchainRev = NULL;
  cd->arcs = a;
}

   
                                                                    
   
static void
uncolorchain(struct colormap *cm, struct arc *a)
{
  struct colordesc *cd = &cm->cd[a->co];
  struct arc *aa = a->colorchainRev;

  if (aa == NULL)
  {
    assert(cd->arcs == a);
    cd->arcs = a->colorchain;
  }
  else
  {
    assert(aa->colorchain == a);
    aa->colorchain = a->colorchain;
  }
  if (a->colorchain != NULL)
  {
    a->colorchain->colorchainRev = aa;
  }
  a->colorchain = NULL;               
  a->colorchainRev = NULL;
}

   
                                                                            
   
static void
rainbow(struct nfa *nfa, struct colormap *cm, int type, color but,                                 
    struct state *from, struct state *to)
{
  struct colordesc *cd;
  struct colordesc *end = CDEND(cm);
  color co;

  for (cd = cm->cd, co = 0; cd < end && !CISERR(); cd++, co++)
  {
    if (!UNUSEDCOLOR(cd) && cd->sub != co && co != but && !(cd->flags & PSEUDO))
    {
      newarc(nfa, type, co, from, to);
    }
  }
}

   
                                                      
   
                                                                 
   
static void
colorcomplement(struct nfa *nfa, struct colormap *cm, int type, struct state *of,                                              
    struct state *from, struct state *to)
{
  struct colordesc *cd;
  struct colordesc *end = CDEND(cm);
  color co;

  assert(of != from);
  for (cd = cm->cd, co = 0; cd < end && !CISERR(); cd++, co++)
  {
    if (!UNUSEDCOLOR(cd) && !(cd->flags & PSEUDO))
    {
      if (findarc(of, PLAIN, co) == NULL)
      {
        newarc(nfa, type, co, from, to);
      }
    }
  }
}

#ifdef REG_DEBUG

   
                                 
   
static void
dumpcolors(struct colormap *cm, FILE *f)
{
  struct colordesc *cd;
  struct colordesc *end;
  color co;
  chr c;

  fprintf(f, "max %ld\n", (long)cm->max);
  end = CDEND(cm);
  for (cd = cm->cd + 1, co = 1; cd < end; cd++, co++)             
  {
    if (!UNUSEDCOLOR(cd))
    {
      assert(cd->nschrs > 0 || cd->nuchrs > 0);
      if (cd->flags & PSEUDO)
      {
        fprintf(f, "#%2ld(ps): ", (long)co);
      }
      else
      {
        fprintf(f, "#%2ld(%2d): ", (long)co, cd->nschrs + cd->nuchrs);
      }

         
                                                                        
         
      for (c = CHR_MIN; c <= MAX_SIMPLE_CHR; c++)
      {
        if (GETCOLOR(cm, c) == co)
        {
          dumpchr(c, f);
        }
      }
      fprintf(f, "\n");
    }
  }
                                                                  
  if (cm->hiarrayrows > 1 || cm->hiarraycols > 1)
  {
    int r, c;
    const color *rowptr;

    fprintf(f, "other:\t");
    for (c = 0; c < cm->hiarraycols; c++)
    {
      fprintf(f, "\t%ld", (long)cm->hicolormap[c]);
    }
    fprintf(f, "\n");
    for (r = 0; r < cm->numcmranges; r++)
    {
      dumpchr(cm->cmranges[r].cmin, f);
      fprintf(f, "..");
      dumpchr(cm->cmranges[r].cmax, f);
      fprintf(f, ":");
      rowptr = &cm->hicolormap[cm->cmranges[r].rownum * cm->hiarraycols];
      for (c = 0; c < cm->hiarraycols; c++)
      {
        fprintf(f, "\t%ld", (long)rowptr[c]);
      }
      fprintf(f, "\n");
    }
  }
}

   
                         
   
                                                             
   
static void
dumpchr(chr c, FILE *f)
{
  if (c == '\\')
  {
    fprintf(f, "\\\\");
  }
  else if (c > ' ' && c <= '~')
  {
    putc((char)c, f);
  }
  else
  {
    fprintf(f, "\\u%04lx", (long)c);
  }
}

#endif                
