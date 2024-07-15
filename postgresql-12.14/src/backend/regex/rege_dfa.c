   
                
                                        
   
                                                                 
   
                                                                            
                                                                            
                                                                          
                       
   
                                                                        
                                                                
                                                                          
                                                        
   
                                                                           
                                                             
   
                                                                              
                                                                            
                                                                           
                                                                          
                                                                       
                                                                               
                                                                            
                                                                           
                                                                          
                                              
   
                                
   
   

   
                                               
   
                                                                          
                                                      
   
static chr *
longest(struct vars *v, struct dfa *d, chr *start,                                   
    chr *stop,                                                                           
    int *hitstopp)                                                                              
{
  chr *cp;
  chr *realstop = (stop == v->stop) ? stop : stop + 1;
  color co;
  struct sset *css;
  struct sset *ss;
  chr *post;
  int i;
  struct colormap *cm = d->cm;

                                                 
  if (hitstopp != NULL)
  {
    *hitstopp = 0;
  }

                  
  css = initialize(v, d, start);
  if (css == NULL)
  {
    return NULL;
  }
  cp = start;

               
  FDEBUG(("+++ startup +++\n"));
  if (cp == v->start)
  {
    co = d->cnfa->bos[(v->eflags & REG_NOTBOL) ? 0 : 1];
    FDEBUG(("color %ld\n", (long)co));
  }
  else
  {
    co = GETCOLOR(cm, *(cp - 1));
    FDEBUG(("char %c, color %ld\n", (char)*(cp - 1), (long)co));
  }
  css = miss(v, d, css, co, cp, start);
  if (css == NULL)
  {
    return NULL;
  }
  css->lastseen = cp;

     
                                                                            
                                                                       
                                               
     
#ifdef REG_DEBUG
  if (v->eflags & REG_FTRACE)
  {
    while (cp < realstop)
    {
      FDEBUG(("+++ at c%d +++\n", (int)(css - d->ssets)));
      co = GETCOLOR(cm, *cp);
      FDEBUG(("char %c, color %ld\n", (char)*cp, (long)co));
      ss = css->outs[co];
      if (ss == NULL)
      {
        ss = miss(v, d, css, co, cp + 1, start);
        if (ss == NULL)
        {
          break;                     
        }
      }
      cp++;
      ss->lastseen = cp;
      css = ss;
    }
  }
  else
#endif
  {
    while (cp < realstop)
    {
      co = GETCOLOR(cm, *cp);
      ss = css->outs[co];
      if (ss == NULL)
      {
        ss = miss(v, d, css, co, cp + 1, start);
        if (ss == NULL)
        {
          break;                     
        }
      }
      cp++;
      ss->lastseen = cp;
      css = ss;
    }
  }

  if (ISERR())
  {
    return NULL;
  }

                
  FDEBUG(("+++ shutdown at c%d +++\n", (int)(css - d->ssets)));
  if (cp == v->stop && stop == v->stop)
  {
    if (hitstopp != NULL)
    {
      *hitstopp = 1;
    }
    co = d->cnfa->eos[(v->eflags & REG_NOTEOL) ? 0 : 1];
    FDEBUG(("color %ld\n", (long)co));
    ss = miss(v, d, css, co, cp, start);
    if (ISERR())
    {
      return NULL;
    }
                                            
    if (ss != NULL && (ss->flags & POSTSTATE))
    {
      return cp;
    }
    else if (ss != NULL)
    {
      ss->lastseen = cp;                 
    }
  }

                               
  post = d->lastpost;
  for (ss = d->ssets, i = d->nssused; i > 0; ss++, i--)
  {
    if ((ss->flags & POSTSTATE) && post != ss->lastseen && (post == NULL || post < ss->lastseen))
    {
      post = ss->lastseen;
    }
  }
  if (post != NULL)                
  {
    return post - 1;
  }

  return NULL;
}

   
                                                 
   
                                                                          
                                                      
   
static chr *
shortest(struct vars *v, struct dfa *d, chr *start,                                   
    chr *min,                                                                            
    chr *max,                                                                             
    chr **coldp,                                                                                   
    int *hitstopp)                                                                               
{
  chr *cp;
  chr *realmin = (min == v->stop) ? min : min + 1;
  chr *realmax = (max == v->stop) ? max : max + 1;
  color co;
  struct sset *css;
  struct sset *ss;
  struct colormap *cm = d->cm;

                                                 
  if (coldp != NULL)
  {
    *coldp = NULL;
  }
  if (hitstopp != NULL)
  {
    *hitstopp = 0;
  }

                  
  css = initialize(v, d, start);
  if (css == NULL)
  {
    return NULL;
  }
  cp = start;

               
  FDEBUG(("--- startup ---\n"));
  if (cp == v->start)
  {
    co = d->cnfa->bos[(v->eflags & REG_NOTBOL) ? 0 : 1];
    FDEBUG(("color %ld\n", (long)co));
  }
  else
  {
    co = GETCOLOR(cm, *(cp - 1));
    FDEBUG(("char %c, color %ld\n", (char)*(cp - 1), (long)co));
  }
  css = miss(v, d, css, co, cp, start);
  if (css == NULL)
  {
    return NULL;
  }
  css->lastseen = cp;
  ss = css;

     
                                                                            
                                                                       
                                               
     
#ifdef REG_DEBUG
  if (v->eflags & REG_FTRACE)
  {
    while (cp < realmax)
    {
      FDEBUG(("--- at c%d ---\n", (int)(css - d->ssets)));
      co = GETCOLOR(cm, *cp);
      FDEBUG(("char %c, color %ld\n", (char)*cp, (long)co));
      ss = css->outs[co];
      if (ss == NULL)
      {
        ss = miss(v, d, css, co, cp + 1, start);
        if (ss == NULL)
        {
          break;                     
        }
      }
      cp++;
      ss->lastseen = cp;
      css = ss;
      if ((ss->flags & POSTSTATE) && cp >= realmin)
      {
        break;                     
      }
    }
  }
  else
#endif
  {
    while (cp < realmax)
    {
      co = GETCOLOR(cm, *cp);
      ss = css->outs[co];
      if (ss == NULL)
      {
        ss = miss(v, d, css, co, cp + 1, start);
        if (ss == NULL)
        {
          break;                     
        }
      }
      cp++;
      ss->lastseen = cp;
      css = ss;
      if ((ss->flags & POSTSTATE) && cp >= realmin)
      {
        break;                     
      }
    }
  }

  if (ss == NULL)
  {
    return NULL;
  }

  if (coldp != NULL)                                                
  {
    *coldp = lastcold(v, d);
  }

  if ((ss->flags & POSTSTATE) && cp > min)
  {
    assert(cp >= realmin);
    cp--;
  }
  else if (cp == v->stop && max == v->stop)
  {
    co = d->cnfa->eos[(v->eflags & REG_NOTEOL) ? 0 : 1];
    FDEBUG(("color %ld\n", (long)co));
    ss = miss(v, d, css, co, cp, start);
                                       
    if ((ss == NULL || !(ss->flags & POSTSTATE)) && hitstopp != NULL)
    {
      *hitstopp = 1;
    }
  }

  if (ss == NULL || !(ss->flags & POSTSTATE))
  {
    return NULL;
  }

  return cp;
}

   
                                            
   
                                                                          
                                                                        
                                                                          
                                                                     
                                                                           
                               
   
                                          
                                                   
   
static int
matchuntil(struct vars *v, struct dfa *d, chr *probe,                                           
    struct sset **lastcss,                                                            
    chr **lastcp)                                                                     
{
  chr *cp = *lastcp;
  color co;
  struct sset *css = *lastcss;
  struct sset *ss;
  struct colormap *cm = d->cm;

                                                        
  if (cp == NULL || cp > probe)
  {
    cp = v->start;
    css = initialize(v, d, cp);
    if (css == NULL)
    {
      return 0;
    }

    FDEBUG((">>> startup >>>\n"));
    co = d->cnfa->bos[(v->eflags & REG_NOTBOL) ? 0 : 1];
    FDEBUG(("color %ld\n", (long)co));

    css = miss(v, d, css, co, cp, v->start);
    if (css == NULL)
    {
      return 0;
    }
    css->lastseen = cp;
  }
  else if (css == NULL)
  {
                                                                      
    return 0;
  }
  ss = css;

     
                                                                            
                                                                       
                                               
     
#ifdef REG_DEBUG
  if (v->eflags & REG_FTRACE)
  {
    while (cp < probe)
    {
      FDEBUG((">>> at c%d >>>\n", (int)(css - d->ssets)));
      co = GETCOLOR(cm, *cp);
      FDEBUG(("char %c, color %ld\n", (char)*cp, (long)co));
      ss = css->outs[co];
      if (ss == NULL)
      {
        ss = miss(v, d, css, co, cp + 1, v->start);
        if (ss == NULL)
        {
          break;                     
        }
      }
      cp++;
      ss->lastseen = cp;
      css = ss;
    }
  }
  else
#endif
  {
    while (cp < probe)
    {
      co = GETCOLOR(cm, *cp);
      ss = css->outs[co];
      if (ss == NULL)
      {
        ss = miss(v, d, css, co, cp + 1, v->start);
        if (ss == NULL)
        {
          break;                     
        }
      }
      cp++;
      ss->lastseen = cp;
      css = ss;
    }
  }

  *lastcss = ss;
  *lastcp = cp;

  if (ss == NULL)
  {
    return 0;                                          
  }

                                                                          
  if (cp < v->stop)
  {
    FDEBUG((">>> at c%d >>>\n", (int)(css - d->ssets)));
    co = GETCOLOR(cm, *cp);
    FDEBUG(("char %c, color %ld\n", (char)*cp, (long)co));
    ss = css->outs[co];
    if (ss == NULL)
    {
      ss = miss(v, d, css, co, cp + 1, v->start);
    }
  }
  else
  {
    assert(cp == v->stop);
    co = d->cnfa->eos[(v->eflags & REG_NOTEOL) ? 0 : 1];
    FDEBUG(("color %ld\n", (long)co));
    ss = miss(v, d, css, co, cp, v->start);
  }

  if (ss == NULL || !(ss->flags & POSTSTATE))
  {
    return 0;
  }

  return 1;
}

   
                                                                      
   
static chr *                        
lastcold(struct vars *v, struct dfa *d)
{
  struct sset *ss;
  chr *nopr;
  int i;

  nopr = d->lastnopr;
  if (nopr == NULL)
  {
    nopr = v->start;
  }
  for (ss = d->ssets, i = d->nssused; i > 0; ss++, i--)
  {
    if ((ss->flags & NOPROGRESS) && nopr < ss->lastseen)
    {
      nopr = ss->lastseen;
    }
  }
  return nopr;
}

   
                               
   
static struct dfa *
newdfa(struct vars *v, struct cnfa *cnfa, struct colormap *cm, struct smalldfa *sml)                                      
{
  struct dfa *d;
  size_t nss = cnfa->nstates * 2;
  int wordsper = (cnfa->nstates + UBITS - 1) / UBITS;
  struct smalldfa *smallwas = sml;

  assert(cnfa != NULL && cnfa->nstates != 0);

  if (nss <= FEWSTATES && cnfa->ncolors <= FEWCOLORS)
  {
    assert(wordsper == 1);
    if (sml == NULL)
    {
      sml = (struct smalldfa *)MALLOC(sizeof(struct smalldfa));
      if (sml == NULL)
      {
        ERR(REG_ESPACE);
        return NULL;
      }
    }
    d = &sml->dfa;
    d->ssets = sml->ssets;
    d->statesarea = sml->statesarea;
    d->work = &d->statesarea[nss];
    d->outsarea = sml->outsarea;
    d->incarea = sml->incarea;
    d->cptsmalloced = 0;
    d->mallocarea = (smallwas == NULL) ? (char *)sml : NULL;
  }
  else
  {
    d = (struct dfa *)MALLOC(sizeof(struct dfa));
    if (d == NULL)
    {
      ERR(REG_ESPACE);
      return NULL;
    }
    d->ssets = (struct sset *)MALLOC(nss * sizeof(struct sset));
    d->statesarea = (unsigned *)MALLOC((nss + WORK) * wordsper * sizeof(unsigned));
    d->work = &d->statesarea[nss * wordsper];
    d->outsarea = (struct sset **)MALLOC(nss * cnfa->ncolors * sizeof(struct sset *));
    d->incarea = (struct arcp *)MALLOC(nss * cnfa->ncolors * sizeof(struct arcp));
    d->cptsmalloced = 1;
    d->mallocarea = (char *)d;
    if (d->ssets == NULL || d->statesarea == NULL || d->outsarea == NULL || d->incarea == NULL)
    {
      freedfa(d);
      ERR(REG_ESPACE);
      return NULL;
    }
  }

  d->nssets = (v->eflags & REG_SMALL) ? 7 : nss;
  d->nssused = 0;
  d->nstates = cnfa->nstates;
  d->ncolors = cnfa->ncolors;
  d->wordsper = wordsper;
  d->cnfa = cnfa;
  d->cm = cm;
  d->lastpost = NULL;
  d->lastnopr = NULL;
  d->search = d->ssets;

                                                       

  return d;
}

   
                        
   
static void
freedfa(struct dfa *d)
{
  if (d->cptsmalloced)
  {
    if (d->ssets != NULL)
    {
      FREE(d->ssets);
    }
    if (d->statesarea != NULL)
    {
      FREE(d->statesarea);
    }
    if (d->outsarea != NULL)
    {
      FREE(d->outsarea);
    }
    if (d->incarea != NULL)
    {
      FREE(d->incarea);
    }
  }

  if (d->mallocarea != NULL)
  {
    FREE(d->mallocarea);
  }
}

   
                                                
   
                                                               
   
static unsigned
hash(unsigned *uv, int n)
{
  int i;
  unsigned h;

  h = 0;
  for (i = 0; i < n; i++)
  {
    h ^= uv[i];
  }
  return h;
}

   
                                                                          
   
static struct sset *
initialize(struct vars *v, struct dfa *d, chr *start)
{
  struct sset *ss;
  int i;

                                    
  if (d->nssused > 0 && (d->ssets[0].flags & STARTER))
  {
    ss = &d->ssets[0];
  }
  else
  {                            
    ss = getvacant(v, d, start, start);
    if (ss == NULL)
    {
      return NULL;
    }
    for (i = 0; i < d->wordsper; i++)
    {
      ss->states[i] = 0;
    }
    BSET(ss->states, d->cnfa->pre);
    ss->hash = HASH(ss->states, d->wordsper);
    assert(d->cnfa->pre != d->cnfa->post);
    ss->flags = STARTER | LOCKED | NOPROGRESS;
                                   
  }

  for (i = 0; i < d->nssused; i++)
  {
    d->ssets[i].lastseen = NULL;
  }
  ss->lastseen = start;                                 
  d->lastpost = NULL;
  d->lastnopr = NULL;
  return ss;
}

   
                                       
   
                                                                                
                                                                             
                                                                               
                                                           
   
                                                                         
                                                                         
                                                          
                                                      
   
static struct sset *
miss(struct vars *v, struct dfa *d, struct sset *css, color co, chr *cp,               
    chr *start)                                                                                             
{
  struct cnfa *cnfa = d->cnfa;
  int i;
  unsigned h;
  struct carc *ca;
  struct sset *p;
  int ispost;
  int noprogress;
  int gotstate;
  int dolacons;
  int sawlacons;

                                                                        
  if (css->outs[co] != NULL)
  {
    FDEBUG(("hit\n"));
    return css->outs[co];
  }
  FDEBUG(("miss\n"));

     
                                                                       
                                                                    
     
  if (CANCEL_REQUESTED(v->re))
  {
    ERR(REG_CANCEL);
    return NULL;
  }

     
                                                                             
                                                                            
                                                                   
     
  for (i = 0; i < d->wordsper; i++)
  {
    d->work[i] = 0;                                           
  }
  ispost = 0;
  noprogress = 1;
  gotstate = 0;
  for (i = 0; i < d->nstates; i++)
  {
    if (ISBSET(css->states, i))
    {
      for (ca = cnfa->states[i]; ca->co != COLORLESS; ca++)
      {
        if (ca->co == co)
        {
          BSET(d->work, ca->to);
          gotstate = 1;
          if (ca->to == cnfa->post)
          {
            ispost = 1;
          }
          if (!(cnfa->stflags[ca->to] & CNFA_NOPROGRESS))
          {
            noprogress = 0;
          }
          FDEBUG(("%d -> %d\n", i, ca->to));
        }
      }
    }
  }
  if (!gotstate)
  {
    return NULL;                                           
  }
  dolacons = (cnfa->flags & HASLACONS);
  sawlacons = 0;
                                                                          
  while (dolacons)
  {
    dolacons = 0;
    for (i = 0; i < d->nstates; i++)
    {
      if (ISBSET(d->work, i))
      {
        for (ca = cnfa->states[i]; ca->co != COLORLESS; ca++)
        {
          if (ca->co < cnfa->ncolors)
          {
            continue;                      
          }
          if (ISBSET(d->work, ca->to))
          {
            continue;                                  
          }
          sawlacons = 1;                                    
          if (!lacon(v, cnfa, cp, ca->co))
          {
            if (ISERR())
            {
              return NULL;
            }
            continue;                                    
          }
          if (ISERR())
          {
            return NULL;
          }
          BSET(d->work, ca->to);
          dolacons = 1;
          if (ca->to == cnfa->post)
          {
            ispost = 1;
          }
          if (!(cnfa->stflags[ca->to] & CNFA_NOPROGRESS))
          {
            noprogress = 0;
          }
          FDEBUG(("%d :> %d\n", i, ca->to));
        }
      }
    }
  }
  h = HASH(d->work, d->wordsper);

                                              
  for (p = d->ssets, i = d->nssused; i > 0; p++, i--)
  {
    if (HIT(h, d->work, p, d->wordsper))
    {
      FDEBUG(("cached c%d\n", (int)(p - d->ssets)));
      break;                     
    }
  }
  if (i == 0)
  {                                   
    p = getvacant(v, d, cp, start);
    if (p == NULL)
    {
      return NULL;
    }
    assert(p != css);
    for (i = 0; i < d->wordsper; i++)
    {
      p->states[i] = d->work[i];
    }
    p->hash = h;
    p->flags = (ispost) ? POSTSTATE : 0;
    if (noprogress)
    {
      p->flags |= NOPROGRESS;
    }
                                             
  }

     
                                                                            
                                                                           
                                                                             
                                                                             
                                               
     
  if (!sawlacons)
  {
    FDEBUG(("c%d[%d]->c%d\n", (int)(css - d->ssets), co, (int)(p - d->ssets)));
    css->outs[co] = p;
    css->inchain[co] = p->ins;
    p->ins.ss = css;
    p->ins.co = co;
  }
  return p;
}

   
                                                    
   
static int                                                                       
lacon(struct vars *v, struct cnfa *pcnfa,                  
    chr *cp, color co)                                                              
{
  int n;
  struct subre *sub;
  struct dfa *d;
  chr *end;
  int satisfied;

                                                                     
  if (STACK_TOO_DEEP(v->re))
  {
    ERR(REG_ETOOBIG);
    return 0;
  }

  n = co - pcnfa->ncolors;
  assert(n > 0 && n < v->g->nlacons && v->g->lacons != NULL);
  FDEBUG(("=== testing lacon %d\n", n));
  sub = &v->g->lacons[n];
  d = getladfa(v, n);
  if (d == NULL)
  {
    return 0;
  }
  if (LATYPE_IS_AHEAD(sub->subno))
  {
                                                                          
    end = shortest(v, d, cp, cp, v->stop, (chr **)NULL, (int *)NULL);
    satisfied = LATYPE_IS_POS(sub->subno) ? (end != NULL) : (end == NULL);
  }
  else
  {
       
                                                                       
                                                                          
                                                                         
                                                                           
                                                                       
                      
       
    satisfied = matchuntil(v, d, cp, &v->lblastcss[n], &v->lblastcp[n]);
    if (!LATYPE_IS_POS(sub->subno))
    {
      satisfied = !satisfied;
    }
  }
  FDEBUG(("=== lacon %d satisfied %d\n", n, satisfied));
  return satisfied;
}

   
                                      
   
                                                                          
                                                                  
   
static struct sset *
getvacant(struct vars *v, struct dfa *d, chr *cp, chr *start)
{
  int i;
  struct sset *ss;
  struct sset *p;
  struct arcp ap;
  color co;

  ss = pickss(v, d, cp, start);
  if (ss == NULL)
  {
    return NULL;
  }
  assert(!(ss->flags & LOCKED));

                                                             
  ap = ss->ins;
  while ((p = ap.ss) != NULL)
  {
    co = ap.co;
    FDEBUG(("zapping c%d's %ld outarc\n", (int)(p - d->ssets), (long)co));
    p->outs[co] = NULL;
    ap = p->inchain[co];
    p->inchain[co].ss = NULL;               
  }
  ss->ins.ss = NULL;

                                                                        
  for (i = 0; i < d->ncolors; i++)
  {
    p = ss->outs[i];
    assert(p != ss);                           
    if (p == NULL)
    {
      continue;                    
    }
    FDEBUG(("del outarc %d from c%d's in chn\n", i, (int)(p - d->ssets)));
    if (p->ins.ss == ss && p->ins.co == i)
    {
      p->ins = ss->inchain[i];
    }
    else
    {
      struct arcp lastap = {NULL, 0};

      assert(p->ins.ss != NULL);
      for (ap = p->ins; ap.ss != NULL && !(ap.ss == ss && ap.co == i); ap = ap.ss->inchain[ap.co])
      {
        lastap = ap;
      }
      assert(ap.ss != NULL);
      lastap.ss->inchain[lastap.co] = ss->inchain[i];
    }
    ss->outs[i] = NULL;
    ss->inchain[i].ss = NULL;
  }

                                                                
  if ((ss->flags & POSTSTATE) && ss->lastseen != d->lastpost && (d->lastpost == NULL || d->lastpost < ss->lastseen))
  {
    d->lastpost = ss->lastseen;
  }

                                        
  if ((ss->flags & NOPROGRESS) && ss->lastseen != d->lastnopr && (d->lastnopr == NULL || d->lastnopr < ss->lastseen))
  {
    d->lastnopr = ss->lastseen;
  }

  return ss;
}

   
                                              
   
static struct sset *
pickss(struct vars *v, struct dfa *d, chr *cp, chr *start)
{
  int i;
  struct sset *ss;
  struct sset *end;
  chr *ancient;

                                                 
  if (d->nssused < d->nssets)
  {
    i = d->nssused;
    d->nssused++;
    ss = &d->ssets[i];
    FDEBUG(("new c%d\n", i));
                        
    ss->states = &d->statesarea[i * d->wordsper];
    ss->flags = 0;
    ss->ins.ss = NULL;
    ss->ins.co = WHITE;                         
    ss->outs = &d->outsarea[i * d->ncolors];
    ss->inchain = &d->incarea[i * d->ncolors];
    for (i = 0; i < d->ncolors; i++)
    {
      ss->outs[i] = NULL;
      ss->inchain[i].ss = NULL;
    }
    return ss;
  }

                                             
  if (cp - start > d->nssets * 2 / 3)                                
  {
    ancient = cp - d->nssets * 2 / 3;
  }
  else
  {
    ancient = start;
  }
  for (ss = d->search, end = &d->ssets[d->nssets]; ss < end; ss++)
  {
    if ((ss->lastseen == NULL || ss->lastseen < ancient) && !(ss->flags & LOCKED))
    {
      d->search = ss + 1;
      FDEBUG(("replacing c%d\n", (int)(ss - d->ssets)));
      return ss;
    }
  }
  for (ss = d->ssets, end = d->search; ss < end; ss++)
  {
    if ((ss->lastseen == NULL || ss->lastseen < ancient) && !(ss->flags & LOCKED))
    {
      d->search = ss + 1;
      FDEBUG(("replacing c%d\n", (int)(ss - d->ssets)));
      return ss;
    }
  }

                                                          
  FDEBUG(("cannot find victim to replace!\n"));
  ERR(REG_ASSERT);
  return NULL;
}
