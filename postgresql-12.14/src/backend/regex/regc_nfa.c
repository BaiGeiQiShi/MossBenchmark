   
                  
                                        
   
                                                                 
   
                                                                            
                                                                            
                                                                          
                       
   
                                                                        
                                                                
                                                                          
                                                        
   
                                                                           
                                                             
   
                                                                              
                                                                            
                                                                           
                                                                          
                                                                       
                                                                               
                                                                            
                                                                           
                                                                          
                                              
   
                                
   
   
                                                          
                                                                       
                     
   

#define NISERR() VISERR(nfa->v)
#define NERR(e) VERR(nfa->v, (e))

   
                          
   
static struct nfa *                                                                   
newnfa(struct vars *v, struct colormap *cm, struct nfa *parent)                          
{
  struct nfa *nfa;

  nfa = (struct nfa *)MALLOC(sizeof(struct nfa));
  if (nfa == NULL)
  {
    ERR(REG_ESPACE);
    return NULL;
  }

  nfa->states = NULL;
  nfa->slast = NULL;
  nfa->free = NULL;
  nfa->nstates = 0;
  nfa->cm = cm;
  nfa->v = v;
  nfa->bos[0] = nfa->bos[1] = COLORLESS;
  nfa->eos[0] = nfa->eos[1] = COLORLESS;
  nfa->parent = parent;                                                        
  nfa->post = newfstate(nfa, '@');               
  nfa->pre = newfstate(nfa, '>');                

  nfa->init = newstate(nfa);                               
  nfa->final = newstate(nfa);
  if (ISERR())
  {
    freenfa(nfa);
    return NULL;
  }
  rainbow(nfa, nfa->cm, PLAIN, COLORLESS, nfa->pre, nfa->init);
  newarc(nfa, '^', 1, nfa->pre, nfa->init);
  newarc(nfa, '^', 0, nfa->pre, nfa->init);
  rainbow(nfa, nfa->cm, PLAIN, COLORLESS, nfa->final, nfa->post);
  newarc(nfa, '$', 1, nfa->final, nfa->post);
  newarc(nfa, '$', 0, nfa->final, nfa->post);

  if (ISERR())
  {
    freenfa(nfa);
    return NULL;
  }
  return nfa;
}

   
                                
   
static void
freenfa(struct nfa *nfa)
{
  struct state *s;

  while ((s = nfa->states) != NULL)
  {
    s->nins = s->nouts = 0;                             
    freestate(nfa, s);
  }
  while ((s = nfa->free) != NULL)
  {
    nfa->free = s->next;
    destroystate(nfa, s);
  }

  nfa->slast = NULL;
  nfa->nstates = -1;
  nfa->pre = NULL;
  nfa->post = NULL;
  FREE(nfa);
}

   
                                                          
   
static struct state *                    
newstate(struct nfa *nfa)
{
  struct state *s;

     
                                                                      
                                                                            
                   
     
  if (CANCEL_REQUESTED(nfa->v->re))
  {
    NERR(REG_CANCEL);
    return NULL;
  }

  if (nfa->free != NULL)
  {
    s = nfa->free;
    nfa->free = s->next;
  }
  else
  {
    if (nfa->v->spaceused >= REG_MAX_COMPILE_SPACE)
    {
      NERR(REG_ETOOBIG);
      return NULL;
    }
    s = (struct state *)MALLOC(sizeof(struct state));
    if (s == NULL)
    {
      NERR(REG_ESPACE);
      return NULL;
    }
    nfa->v->spaceused += sizeof(struct state);
    s->oas.next = NULL;
    s->free = NULL;
    s->noas = 0;
  }

  assert(nfa->nstates >= 0);
  s->no = nfa->nstates++;
  s->flag = 0;
  if (nfa->states == NULL)
  {
    nfa->states = s;
  }
  s->nins = 0;
  s->ins = NULL;
  s->nouts = 0;
  s->outs = NULL;
  s->tmp = NULL;
  s->next = NULL;
  if (nfa->slast != NULL)
  {
    assert(nfa->slast->next == NULL);
    nfa->slast->next = s;
  }
  s->prev = nfa->slast;
  nfa->slast = s;
  return s;
}

   
                                                                 
   
static struct state *                    
newfstate(struct nfa *nfa, int flag)
{
  struct state *s;

  s = newstate(nfa);
  if (s != NULL)
  {
    s->flag = (char)flag;
  }
  return s;
}

   
                                                               
   
static void
dropstate(struct nfa *nfa, struct state *s)
{
  struct arc *a;

  while ((a = s->ins) != NULL)
  {
    freearc(nfa, a);
  }
  while ((a = s->outs) != NULL)
  {
    freearc(nfa, a);
  }
  freestate(nfa, s);
}

   
                                                              
   
static void
freestate(struct nfa *nfa, struct state *s)
{
  assert(s != NULL);
  assert(s->nins == 0 && s->nouts == 0);

  s->no = FREESTATE;
  s->flag = 0;
  if (s->next != NULL)
  {
    s->next->prev = s->prev;
  }
  else
  {
    assert(s == nfa->slast);
    nfa->slast = s->prev;
  }
  if (s->prev != NULL)
  {
    s->prev->next = s->next;
  }
  else
  {
    assert(s == nfa->states);
    nfa->states = s->next;
  }
  s->prev = NULL;
  s->next = nfa->free;                                               
  nfa->free = s;
}

   
                                                           
   
static void
destroystate(struct nfa *nfa, struct state *s)
{
  struct arcbatch *ab;
  struct arcbatch *abnext;

  assert(s->no == FREESTATE);
  for (ab = s->oas.next; ab != NULL; ab = abnext)
  {
    abnext = ab->next;
    FREE(ab);
    nfa->v->spaceused -= sizeof(struct arcbatch);
  }
  s->ins = NULL;
  s->outs = NULL;
  s->next = NULL;
  FREE(s);
  nfa->v->spaceused -= sizeof(struct state);
}

   
                                           
   
                                                                         
                                        
   
static void
newarc(struct nfa *nfa, int t, color co, struct state *from, struct state *to)
{
  struct arc *a;

  assert(from != NULL && to != NULL);

     
                                                                      
                                                                            
                   
     
  if (CANCEL_REQUESTED(nfa->v->re))
  {
    NERR(REG_CANCEL);
    return;
  }

                                                                 
  if (from->nouts <= to->nins)
  {
    for (a = from->outs; a != NULL; a = a->outchain)
    {
      if (a->to == to && a->co == co && a->type == t)
      {
        return;
      }
    }
  }
  else
  {
    for (a = to->ins; a != NULL; a = a->inchain)
    {
      if (a->from == from && a->co == co && a->type == t)
      {
        return;
      }
    }
  }

                                 
  createarc(nfa, t, co, from, to);
}

   
                                              
   
                                                                               
                                            
   
static void
createarc(struct nfa *nfa, int t, color co, struct state *from, struct state *to)
{
  struct arc *a;

                                                             
  a = allocarc(nfa, from);
  if (NISERR())
  {
    return;
  }
  assert(a != NULL);

  a->type = t;
  a->co = co;
  a->to = to;
  a->from = from;

     
                                                                        
                                                                            
                                                                  
     
  a->inchain = to->ins;
  a->inchainRev = NULL;
  if (to->ins)
  {
    to->ins->inchainRev = a;
  }
  to->ins = a;
  a->outchain = from->outs;
  a->outchainRev = NULL;
  if (from->outs)
  {
    from->outs->outchainRev = a;
  }
  from->outs = a;

  from->nouts++;
  to->nins++;

  if (COLORED(a) && nfa->parent == NULL)
  {
    colorchain(nfa->cm, a);
  }
}

   
                                                    
   
static struct arc *                       
allocarc(struct nfa *nfa, struct state *s)
{
  struct arc *a;

                
  if (s->free == NULL && s->noas < ABSIZE)
  {
    a = &s->oas.a[s->noas];
    s->noas++;
    return a;
  }

                                 
  if (s->free == NULL)
  {
    struct arcbatch *newAb;
    int i;

    if (nfa->v->spaceused >= REG_MAX_COMPILE_SPACE)
    {
      NERR(REG_ETOOBIG);
      return NULL;
    }
    newAb = (struct arcbatch *)MALLOC(sizeof(struct arcbatch));
    if (newAb == NULL)
    {
      NERR(REG_ESPACE);
      return NULL;
    }
    nfa->v->spaceused += sizeof(struct arcbatch);
    newAb->next = s->oas.next;
    s->oas.next = newAb;

    for (i = 0; i < ABSIZE; i++)
    {
      newAb->a[i].type = 0;
      newAb->a[i].freechain = &newAb->a[i + 1];
    }
    newAb->a[ABSIZE - 1].freechain = NULL;
    s->free = &newAb->a[0];
  }
  assert(s->free != NULL);

  a = s->free;
  s->free = a->freechain;
  return a;
}

   
                         
   
static void
freearc(struct nfa *nfa, struct arc *victim)
{
  struct state *from = victim->from;
  struct state *to = victim->to;
  struct arc *predecessor;

  assert(victim->type != 0);

                                            
  if (COLORED(victim) && nfa->parent == NULL)
  {
    uncolorchain(nfa->cm, victim);
  }

                                      
  assert(from != NULL);
  predecessor = victim->outchainRev;
  if (predecessor == NULL)
  {
    assert(from->outs == victim);
    from->outs = victim->outchain;
  }
  else
  {
    assert(predecessor->outchain == victim);
    predecessor->outchain = victim->outchain;
  }
  if (victim->outchain != NULL)
  {
    assert(victim->outchain->outchainRev == victim);
    victim->outchain->outchainRev = predecessor;
  }
  from->nouts--;

                                     
  assert(to != NULL);
  predecessor = victim->inchainRev;
  if (predecessor == NULL)
  {
    assert(to->ins == victim);
    to->ins = victim->inchain;
  }
  else
  {
    assert(predecessor->inchain == victim);
    predecessor->inchain = victim->inchain;
  }
  if (victim->inchain != NULL)
  {
    assert(victim->inchain->inchainRev == victim);
    victim->inchain->inchainRev = predecessor;
  }
  to->nins--;

                                                    
  victim->type = 0;
  victim->from = NULL;                     
  victim->to = NULL;
  victim->inchain = NULL;
  victim->inchainRev = NULL;
  victim->outchain = NULL;
  victim->outchainRev = NULL;
  victim->freechain = from->free;
  from->free = victim;
}

   
                                                              
   
                                                                          
   
                                                                             
                                       
   
static void
changearctarget(struct arc *a, struct state *newto)
{
  struct state *oldto = a->to;
  struct arc *predecessor;

  assert(oldto != newto);

                                         
  assert(oldto != NULL);
  predecessor = a->inchainRev;
  if (predecessor == NULL)
  {
    assert(oldto->ins == a);
    oldto->ins = a->inchain;
  }
  else
  {
    assert(predecessor->inchain == a);
    predecessor->inchain = a->inchain;
  }
  if (a->inchain != NULL)
  {
    assert(a->inchain->inchainRev == a);
    a->inchain->inchainRev = predecessor;
  }
  oldto->nins--;

  a->to = newto;

                                           
  a->inchain = newto->ins;
  a->inchainRev = NULL;
  if (newto->ins)
  {
    newto->ins->inchainRev = a;
  }
  newto->ins = a;
  newto->nins++;
}

   
                                                         
   
static int
hasnonemptyout(struct state *s)
{
  struct arc *a;

  for (a = s->outs; a != NULL; a = a->outchain)
  {
    if (a->type != EMPTY)
    {
      return 1;
    }
  }
  return 0;
}

   
                                                                           
                                                             
   
static struct arc *
findarc(struct state *s, int type, color co)
{
  struct arc *a;

  for (a = s->outs; a != NULL; a = a->outchain)
  {
    if (a->type == type && a->co == co)
    {
      return a;
    }
  }
  return NULL;
}

   
                                                                          
   
static void
cparc(struct nfa *nfa, struct arc *oa, struct state *from, struct state *to)
{
  newarc(nfa, oa->type, oa->co, from, to);
}

   
                                                            
   
static void
sortins(struct nfa *nfa, struct state *s)
{
  struct arc **sortarray;
  struct arc *a;
  int n = s->nins;
  int i;

  if (n <= 1)
  {
    return;                    
  }
                                         
  sortarray = (struct arc **)MALLOC(n * sizeof(struct arc *));
  if (sortarray == NULL)
  {
    NERR(REG_ESPACE);
    return;
  }
  i = 0;
  for (a = s->ins; a != NULL; a = a->inchain)
  {
    sortarray[i++] = a;
  }
  assert(i == n);
                          
  qsort(sortarray, n, sizeof(struct arc *), sortins_cmp);
                                         
                                                                           
  a = sortarray[0];
  s->ins = a;
  a->inchain = sortarray[1];
  a->inchainRev = NULL;
  for (i = 1; i < n - 1; i++)
  {
    a = sortarray[i];
    a->inchain = sortarray[i + 1];
    a->inchainRev = sortarray[i - 1];
  }
  a = sortarray[i];
  a->inchain = NULL;
  a->inchainRev = sortarray[i - 1];
  FREE(sortarray);
}

static int
sortins_cmp(const void *a, const void *b)
{
  const struct arc *aa = *((const struct arc *const *)a);
  const struct arc *bb = *((const struct arc *const *)b);

                                                                             
  if (aa->from->no < bb->from->no)
  {
    return -1;
  }
  if (aa->from->no > bb->from->no)
  {
    return 1;
  }
  if (aa->co < bb->co)
  {
    return -1;
  }
  if (aa->co > bb->co)
  {
    return 1;
  }
  if (aa->type < bb->type)
  {
    return -1;
  }
  if (aa->type > bb->type)
  {
    return 1;
  }
  return 0;
}

   
                                                            
   
static void
sortouts(struct nfa *nfa, struct state *s)
{
  struct arc **sortarray;
  struct arc *a;
  int n = s->nouts;
  int i;

  if (n <= 1)
  {
    return;                    
  }
                                         
  sortarray = (struct arc **)MALLOC(n * sizeof(struct arc *));
  if (sortarray == NULL)
  {
    NERR(REG_ESPACE);
    return;
  }
  i = 0;
  for (a = s->outs; a != NULL; a = a->outchain)
  {
    sortarray[i++] = a;
  }
  assert(i == n);
                          
  qsort(sortarray, n, sizeof(struct arc *), sortouts_cmp);
                                         
                                                                           
  a = sortarray[0];
  s->outs = a;
  a->outchain = sortarray[1];
  a->outchainRev = NULL;
  for (i = 1; i < n - 1; i++)
  {
    a = sortarray[i];
    a->outchain = sortarray[i + 1];
    a->outchainRev = sortarray[i - 1];
  }
  a = sortarray[i];
  a->outchain = NULL;
  a->outchainRev = sortarray[i - 1];
  FREE(sortarray);
}

static int
sortouts_cmp(const void *a, const void *b)
{
  const struct arc *aa = *((const struct arc *const *)a);
  const struct arc *bb = *((const struct arc *const *)b);

                                                                             
  if (aa->to->no < bb->to->no)
  {
    return -1;
  }
  if (aa->to->no > bb->to->no)
  {
    return 1;
  }
  if (aa->co < bb->co)
  {
    return -1;
  }
  if (aa->co > bb->co)
  {
    return 1;
  }
  if (aa->type < bb->type)
  {
    return -1;
  }
  if (aa->type > bb->type)
  {
    return 1;
  }
  return 0;
}

   
                                                                       
                                                                       
                                                                        
                                                                       
                                                                        
                                 
   
#define BULK_ARC_OP_USE_SORT(nsrcarcs, ndestarcs) ((nsrcarcs) < 4 ? 0 : ((nsrcarcs) > 32 || (ndestarcs) > 32))

   
                                                          
   
                                                                  
                                                                    
                                                                     
                                                      
   
                                                                          
                                                                           
                                                                        
   
static void
moveins(struct nfa *nfa, struct state *oldState, struct state *newState)
{
  assert(oldState != newState);

  if (!BULK_ARC_OP_USE_SORT(oldState->nins, newState->nins))
  {
                                                            
    struct arc *a;

    while ((a = oldState->ins) != NULL)
    {
      cparc(nfa, a, a->from, newState);
      freearc(nfa, a);
    }
  }
  else
  {
       
                                                                          
                                                                           
                                                            
       
    struct arc *oa;
    struct arc *na;

       
                                                                           
                     
       
    if (CANCEL_REQUESTED(nfa->v->re))
    {
      NERR(REG_CANCEL);
      return;
    }

    sortins(nfa, oldState);
    sortins(nfa, newState);
    if (NISERR())
    {
      return;                                
    }
    oa = oldState->ins;
    na = newState->ins;
    while (oa != NULL && na != NULL)
    {
      struct arc *a = oa;

      switch (sortins_cmp(&oa, &na))
      {
      case -1:
                                                         
        oa = oa->inchain;

           
                                                                   
                                               
           
        changearctarget(a, newState);
        break;
      case 0:
                                          
        oa = oa->inchain;
        na = na->inchain;
                                                      
        freearc(nfa, a);
        break;
      case +1:
                                                          
        na = na->inchain;
        break;
      default:
        assert(NOTREACHED);
      }
    }
    while (oa != NULL)
    {
                                                       
      struct arc *a = oa;

      oa = oa->inchain;
      changearctarget(a, newState);
    }
  }

  assert(oldState->nins == 0);
  assert(oldState->ins == NULL);
}

   
                                                      
   
static void
copyins(struct nfa *nfa, struct state *oldState, struct state *newState)
{
  assert(oldState != newState);

  if (!BULK_ARC_OP_USE_SORT(oldState->nins, newState->nins))
  {
                                                            
    struct arc *a;

    for (a = oldState->ins; a != NULL; a = a->inchain)
    {
      cparc(nfa, a, a->from, newState);
    }
  }
  else
  {
       
                                                                         
                                                                        
                                                                
       
    struct arc *oa;
    struct arc *na;

       
                                                                           
                     
       
    if (CANCEL_REQUESTED(nfa->v->re))
    {
      NERR(REG_CANCEL);
      return;
    }

    sortins(nfa, oldState);
    sortins(nfa, newState);
    if (NISERR())
    {
      return;                                
    }
    oa = oldState->ins;
    na = newState->ins;
    while (oa != NULL && na != NULL)
    {
      struct arc *a = oa;

      switch (sortins_cmp(&oa, &na))
      {
      case -1:
                                                         
        oa = oa->inchain;
        createarc(nfa, a->type, a->co, a->from, newState);
        break;
      case 0:
                                          
        oa = oa->inchain;
        na = na->inchain;
        break;
      case +1:
                                                          
        na = na->inchain;
        break;
      default:
        assert(NOTREACHED);
      }
    }
    while (oa != NULL)
    {
                                                       
      struct arc *a = oa;

      oa = oa->inchain;
      createarc(nfa, a->type, a->co, a->from, newState);
    }
  }
}

   
                                                  
   
                                                                          
                                                                            
   
static void
mergeins(struct nfa *nfa, struct state *s, struct arc **arcarray, int arccount)
{
  struct arc *na;
  int i;
  int j;

  if (arccount <= 0)
  {
    return;
  }

     
                                                                         
                   
     
  if (CANCEL_REQUESTED(nfa->v->re))
  {
    NERR(REG_CANCEL);
    return;
  }

                                                         
  sortins(nfa, s);
  if (NISERR())
  {
    return;                                
  }

  qsort(arcarray, arccount, sizeof(struct arc *), sortins_cmp);

     
                                                                           
                                                                          
     
  j = 0;
  for (i = 1; i < arccount; i++)
  {
    switch (sortins_cmp(&arcarray[j], &arcarray[i]))
    {
    case -1:
                   
      arcarray[++j] = arcarray[i];
      break;
    case 0:
               
      break;
    default:
                   
      assert(NOTREACHED);
    }
  }
  arccount = j + 1;

     
                                                                         
                                                                            
                               
     
  i = 0;
  na = s->ins;
  while (i < arccount && na != NULL)
  {
    struct arc *a = arcarray[i];

    switch (sortins_cmp(&a, &na))
    {
    case -1:
                                               
      createarc(nfa, a->type, a->co, a->from, s);
      i++;
      break;
    case 0:
                                        
      i++;
      na = na->inchain;
      break;
    case +1:
                                                           
      na = na->inchain;
      break;
    default:
      assert(NOTREACHED);
    }
  }
  while (i < arccount)
  {
                                             
    struct arc *a = arcarray[i];

    createarc(nfa, a->type, a->co, a->from, s);
    i++;
  }
}

   
                                                            
   
static void
moveouts(struct nfa *nfa, struct state *oldState, struct state *newState)
{
  assert(oldState != newState);

  if (!BULK_ARC_OP_USE_SORT(oldState->nouts, newState->nouts))
  {
                                                            
    struct arc *a;

    while ((a = oldState->outs) != NULL)
    {
      cparc(nfa, a, newState, a->to);
      freearc(nfa, a);
    }
  }
  else
  {
       
                                                                         
                                                                        
                                                                
       
    struct arc *oa;
    struct arc *na;

       
                                                                           
                     
       
    if (CANCEL_REQUESTED(nfa->v->re))
    {
      NERR(REG_CANCEL);
      return;
    }

    sortouts(nfa, oldState);
    sortouts(nfa, newState);
    if (NISERR())
    {
      return;                                
    }
    oa = oldState->outs;
    na = newState->outs;
    while (oa != NULL && na != NULL)
    {
      struct arc *a = oa;

      switch (sortouts_cmp(&oa, &na))
      {
      case -1:
                                                         
        oa = oa->outchain;
        createarc(nfa, a->type, a->co, newState, a->to);
        freearc(nfa, a);
        break;
      case 0:
                                          
        oa = oa->outchain;
        na = na->outchain;
                                                      
        freearc(nfa, a);
        break;
      case +1:
                                                          
        na = na->outchain;
        break;
      default:
        assert(NOTREACHED);
      }
    }
    while (oa != NULL)
    {
                                                       
      struct arc *a = oa;

      oa = oa->outchain;
      createarc(nfa, a->type, a->co, newState, a->to);
      freearc(nfa, a);
    }
  }

  assert(oldState->nouts == 0);
  assert(oldState->outs == NULL);
}

   
                                                        
   
static void
copyouts(struct nfa *nfa, struct state *oldState, struct state *newState)
{
  assert(oldState != newState);

  if (!BULK_ARC_OP_USE_SORT(oldState->nouts, newState->nouts))
  {
                                                            
    struct arc *a;

    for (a = oldState->outs; a != NULL; a = a->outchain)
    {
      cparc(nfa, a, newState, a->to);
    }
  }
  else
  {
       
                                                                         
                                                                        
                                                                
       
    struct arc *oa;
    struct arc *na;

       
                                                                           
                     
       
    if (CANCEL_REQUESTED(nfa->v->re))
    {
      NERR(REG_CANCEL);
      return;
    }

    sortouts(nfa, oldState);
    sortouts(nfa, newState);
    if (NISERR())
    {
      return;                                
    }
    oa = oldState->outs;
    na = newState->outs;
    while (oa != NULL && na != NULL)
    {
      struct arc *a = oa;

      switch (sortouts_cmp(&oa, &na))
      {
      case -1:
                                                         
        oa = oa->outchain;
        createarc(nfa, a->type, a->co, newState, a->to);
        break;
      case 0:
                                          
        oa = oa->outchain;
        na = na->outchain;
        break;
      case +1:
                                                          
        na = na->outchain;
        break;
      default:
        assert(NOTREACHED);
      }
    }
    while (oa != NULL)
    {
                                                       
      struct arc *a = oa;

      oa = oa->outchain;
      createarc(nfa, a->type, a->co, newState, a->to);
    }
  }
}

   
                                                                              
   
static void
cloneouts(struct nfa *nfa, struct state *old, struct state *from, struct state *to, int type)
{
  struct arc *a;

  assert(old != from);

  for (a = old->outs; a != NULL; a = a->outchain)
  {
    newarc(nfa, type, a->co, from, to);
  }
}

   
                                                                   
   
                                                                        
                                   
   
static void
delsub(struct nfa *nfa, struct state *lp,                                    
    struct state *rp)                                                      
{
  assert(lp != rp);

  rp->tmp = rp;               

  deltraverse(nfa, lp, lp);
  if (NISERR())
  {
    return;                                           
  }
  assert(lp->nouts == 0 && rp->nins == 0);                             
  assert(lp->no != FREESTATE && rp->no != FREESTATE);              

  rp->tmp = NULL;                 
  lp->tmp = NULL;                                       
}

   
                                               
                                                                     
   
static void
deltraverse(struct nfa *nfa, struct state *leftend, struct state *s)
{
  struct arc *a;
  struct state *to;

                                                                     
  if (STACK_TOO_DEEP(nfa->v->re))
  {
    NERR(REG_ETOOBIG);
    return;
  }

  if (s->nouts == 0)
  {
    return;                    
  }
  if (s->tmp != NULL)
  {
    return;                          
  }

  s->tmp = s;                          

  while ((a = s->outs) != NULL)
  {
    to = a->to;
    deltraverse(nfa, leftend, to);
    if (NISERR())
    {
      return;                                           
    }
    assert(to->nouts == 0 || to->tmp != NULL);
    freearc(nfa, a);
    if (to->nins == 0 && to->tmp == NULL)
    {
      assert(to->nouts == 0);
      freestate(nfa, to);
    }
  }

  assert(s->no != FREESTATE);                                 
  assert(s == leftend || s->nins != 0);                          
  assert(s->nouts == 0);                                         

  s->tmp = NULL;                      
}

   
                              
   
                                                                           
                                                                          
                                          
   
static void
dupnfa(struct nfa *nfa, struct state *start,                                        
    struct state *stop,                                             
    struct state *from,                                                         
    struct state *to)                                     
{
  if (start == stop)
  {
    newarc(nfa, EMPTY, 0, from, to);
    return;
  }

  stop->tmp = to;
  duptraverse(nfa, start, from);
                                                      

  stop->tmp = NULL;
  cleartraverse(nfa, start);
}

   
                                           
   
static void
duptraverse(struct nfa *nfa, struct state *s, struct state *stmp)                             
{
  struct arc *a;

                                                                     
  if (STACK_TOO_DEEP(nfa->v->re))
  {
    NERR(REG_ETOOBIG);
    return;
  }

  if (s->tmp != NULL)
  {
    return;                   
  }

  s->tmp = (stmp == NULL) ? newstate(nfa) : stmp;
  if (s->tmp == NULL)
  {
    assert(NISERR());
    return;
  }

  for (a = s->outs; a != NULL && !NISERR(); a = a->outchain)
  {
    duptraverse(nfa, a->to, (struct state *)NULL);
    if (NISERR())
    {
      break;
    }
    assert(a->to->tmp != NULL);
    cparc(nfa, a, s->tmp, a->to->tmp);
  }
}

   
                                                                            
   
static void
cleartraverse(struct nfa *nfa, struct state *s)
{
  struct arc *a;

                                                                     
  if (STACK_TOO_DEEP(nfa->v->re))
  {
    NERR(REG_ETOOBIG);
    return;
  }

  if (s->tmp == NULL)
  {
    return;
  }
  s->tmp = NULL;

  for (a = s->outs; a != NULL; a = a->outchain)
  {
    cleartraverse(nfa, a->to);
  }
}

   
                                                                             
   
                                                                              
                                                                             
                                                   
   
                                                                             
                                                                               
                         
   
                                                                              
                                                                              
                                                                              
                                                                               
   
static struct state *
single_color_transition(struct state *s1, struct state *s2)
{
  struct arc *a;

                                        
  if (s1->nouts == 1 && s1->outs->type == EMPTY)
  {
    s1 = s1->outs->to;
  }
                                           
  if (s2->nins == 1 && s2->ins->type == EMPTY)
  {
    s2 = s2->ins->from;
  }
                                                                          
  if (s1 == s2)
  {
    return NULL;
  }
                                           
  if (s1->outs == NULL)
  {
    return NULL;
  }
                                                 
  for (a = s1->outs; a != NULL; a = a->outchain)
  {
    if (a->type != PLAIN || a->to != s2)
    {
      return NULL;
    }
  }
                                                              
  return s1;
}

   
                                                     
   
static void
specialcolors(struct nfa *nfa)
{
                                           
  if (nfa->parent == NULL)
  {
    nfa->bos[0] = pseudocolor(nfa->cm);
    nfa->bos[1] = pseudocolor(nfa->cm);
    nfa->eos[0] = pseudocolor(nfa->cm);
    nfa->eos[1] = pseudocolor(nfa->cm);
  }
  else
  {
    assert(nfa->parent->bos[0] != COLORLESS);
    nfa->bos[0] = nfa->parent->bos[0];
    assert(nfa->parent->bos[1] != COLORLESS);
    nfa->bos[1] = nfa->parent->bos[1];
    assert(nfa->parent->eos[0] != COLORLESS);
    nfa->eos[0] = nfa->parent->eos[0];
    assert(nfa->parent->eos[1] != COLORLESS);
    nfa->eos[1] = nfa->parent->eos[1];
  }
}

   
                              
   
                                                                           
                                                                            
                                                                            
                                                                           
                                                                      
                                                                           
                                                                          
                                                                         
                                                                             
                                                                               
                                                    
   
static long                                          
optimize(struct nfa *nfa, FILE *f)                                  
{
#ifdef REG_DEBUG
  int verbose = (f != NULL) ? 1 : 0;

  if (verbose)
  {
    fprintf(f, "\ninitial cleanup:\n");
  }
#endif
  cleanup(nfa);                             
#ifdef REG_DEBUG
  if (verbose)
  {
    dumpnfa(nfa, f);
  }
  if (verbose)
  {
    fprintf(f, "\nempties:\n");
  }
#endif
  fixempties(nfa, f);                            
#ifdef REG_DEBUG
  if (verbose)
  {
    fprintf(f, "\nconstraints:\n");
  }
#endif
  fixconstraintloops(nfa, f);                                  
  pullback(nfa, f);                                               
  pushfwd(nfa, f);                                              
#ifdef REG_DEBUG
  if (verbose)
  {
    fprintf(f, "\nfinal cleanup:\n");
  }
#endif
  cleanup(nfa);                    
#ifdef REG_DEBUG
  if (verbose)
  {
    dumpnfa(nfa, f);
  }
#endif
  return analyze(nfa);                   
}

   
                                                               
   
static void
pullback(struct nfa *nfa, FILE *f)                                  
{
  struct state *s;
  struct state *nexts;
  struct arc *a;
  struct arc *nexta;
  struct state *intermediates;
  int progress;

                                             
  do
  {
    progress = 0;
    for (s = nfa->states; s != NULL && !NISERR(); s = nexts)
    {
      nexts = s->next;
      intermediates = NULL;
      for (a = s->outs; a != NULL && !NISERR(); a = nexta)
      {
        nexta = a->outchain;
        if (a->type == '^' || a->type == BEHIND)
        {
          if (pull(nfa, a, &intermediates))
          {
            progress = 1;
          }
        }
      }
                                                                
      while (intermediates != NULL)
      {
        struct state *ns = intermediates->tmp;

        intermediates->tmp = NULL;
        intermediates = ns;
      }
                                              
      if ((s->nins == 0 || s->nouts == 0) && !s->flag)
      {
        dropstate(nfa, s);
      }
    }
    if (progress && f != NULL)
    {
      dumpnfa(nfa, f);
    }
  } while (progress && !NISERR());
  if (NISERR())
  {
    return;
  }

     
                                                                          
                                                                             
                                                                          
                                            
     
  for (a = nfa->pre->outs; a != NULL; a = nexta)
  {
    nexta = a->outchain;
    if (a->type == '^')
    {
      assert(a->co == 0 || a->co == 1);
      newarc(nfa, PLAIN, nfa->bos[a->co], a->from, a->to);
      freearc(nfa, a);
    }
  }
}

   
                                                                
   
                                                                        
                                                                     
   
                                                                              
                                                                              
                                                                              
                                                                             
                          
   
                                                                           
                                                                             
                                                                              
                                                                             
                              
   
static int
pull(struct nfa *nfa, struct arc *con, struct state **intermediates)
{
  struct state *from = con->from;
  struct state *to = con->to;
  struct arc *a;
  struct arc *nexta;
  struct state *s;

  assert(from != to);                                             
  if (from->flag)                                       
  {
    return 0;
  }
  if (from->nins == 0)
  {                  
    freearc(nfa, con);
    return 1;
  }

     
                                                                            
                                                                          
                                      
     
  if (from->nouts > 1)
  {
    s = newstate(nfa);
    if (NISERR())
    {
      return 0;
    }
    copyins(nfa, from, s);                        
    cparc(nfa, con, s, to);                          
    freearc(nfa, con);
    if (NISERR())
    {
      return 0;
    }
    from = s;
    con = from->outs;
  }
  assert(from->nouts == 1);

                                                             
  for (a = from->ins; a != NULL && !NISERR(); a = nexta)
  {
    nexta = a->inchain;
    switch (combine(con, a))
    {
    case INCOMPATIBLE:                      
      freearc(nfa, a);
      break;
    case SATISFIED:                       
      break;
    case COMPATIBLE:                                      
                                                                  
      for (s = *intermediates; s != NULL; s = s->tmp)
      {
        assert(s->nins > 0 && s->nouts > 0);
        if (s->ins->from == a->from && s->outs->to == to)
        {
          break;
        }
      }
      if (s == NULL)
      {
        s = newstate(nfa);
        if (NISERR())
        {
          return 0;
        }
        s->tmp = *intermediates;
        *intermediates = s;
      }
      cparc(nfa, con, a->from, s);
      cparc(nfa, a, s, to);
      freearc(nfa, a);
      break;
    default:
      assert(NOTREACHED);
      break;
    }
  }

                                                            
  moveins(nfa, from, to);
  freearc(nfa, con);
                                                                            
  return 1;
}

   
                                                                
   
static void
pushfwd(struct nfa *nfa, FILE *f)                                  
{
  struct state *s;
  struct state *nexts;
  struct arc *a;
  struct arc *nexta;
  struct state *intermediates;
  int progress;

                                             
  do
  {
    progress = 0;
    for (s = nfa->states; s != NULL && !NISERR(); s = nexts)
    {
      nexts = s->next;
      intermediates = NULL;
      for (a = s->ins; a != NULL && !NISERR(); a = nexta)
      {
        nexta = a->inchain;
        if (a->type == '$' || a->type == AHEAD)
        {
          if (push(nfa, a, &intermediates))
          {
            progress = 1;
          }
        }
      }
                                                                
      while (intermediates != NULL)
      {
        struct state *ns = intermediates->tmp;

        intermediates->tmp = NULL;
        intermediates = ns;
      }
                                              
      if ((s->nins == 0 || s->nouts == 0) && !s->flag)
      {
        dropstate(nfa, s);
      }
    }
    if (progress && f != NULL)
    {
      dumpnfa(nfa, f);
    }
  } while (progress && !NISERR());
  if (NISERR())
  {
    return;
  }

     
                                                                         
                                                                             
                                                                         
                                            
     
  for (a = nfa->post->ins; a != NULL; a = nexta)
  {
    nexta = a->inchain;
    if (a->type == '$')
    {
      assert(a->co == 0 || a->co == 1);
      newarc(nfa, PLAIN, nfa->eos[a->co], a->from, a->to);
      freearc(nfa, a);
    }
  }
}

   
                                                                       
   
                                                                             
                                                                    
   
                                                                              
                                                                           
                                                                             
                                                                            
                          
   
                                                                           
                                                                              
                                                                              
                                                                           
                              
   
static int
push(struct nfa *nfa, struct arc *con, struct state **intermediates)
{
  struct state *from = con->from;
  struct state *to = con->to;
  struct arc *a;
  struct arc *nexta;
  struct state *s;

  assert(to != from);                                             
  if (to->flag)                                          
  {
    return 0;
  }
  if (to->nouts == 0)
  {               
    freearc(nfa, con);
    return 1;
  }

     
                                                                         
                                                                          
                                      
     
  if (to->nins > 1)
  {
    s = newstate(nfa);
    if (NISERR())
    {
      return 0;
    }
    copyouts(nfa, to, s);                            
    cparc(nfa, con, from, s);                          
    freearc(nfa, con);
    if (NISERR())
    {
      return 0;
    }
    to = s;
    con = to->ins;
  }
  assert(to->nins == 1);

                                                            
  for (a = to->outs; a != NULL && !NISERR(); a = nexta)
  {
    nexta = a->outchain;
    switch (combine(con, a))
    {
    case INCOMPATIBLE:                      
      freearc(nfa, a);
      break;
    case SATISFIED:                       
      break;
    case COMPATIBLE:                                      
                                                                  
      for (s = *intermediates; s != NULL; s = s->tmp)
      {
        assert(s->nins > 0 && s->nouts > 0);
        if (s->ins->from == from && s->outs->to == a->to)
        {
          break;
        }
      }
      if (s == NULL)
      {
        s = newstate(nfa);
        if (NISERR())
        {
          return 0;
        }
        s->tmp = *intermediates;
        *intermediates = s;
      }
      cparc(nfa, con, s, a->to);
      cparc(nfa, a, from, s);
      freearc(nfa, a);
      break;
    default:
      assert(NOTREACHED);
      break;
    }
  }

                                                             
  moveouts(nfa, to, from);
  freearc(nfa, con);
                                                                         
  return 1;
}

   
                                                       
   
                                       
                                             
                                                          
   
static int
combine(struct arc *con, struct arc *a)
{
#define CA(ct, at) (((ct) << CHAR_BIT) | (at))

  switch (CA(con->type, a->type))
  {
  case CA('^', PLAIN):                                      
  case CA('$', PLAIN):
    return INCOMPATIBLE;
    break;
  case CA(AHEAD, PLAIN):                                    
  case CA(BEHIND, PLAIN):
    if (con->co == a->co)
    {
      return SATISFIED;
    }
    return INCOMPATIBLE;
    break;
  case CA('^', '^'):                                     
  case CA('$', '$'):
  case CA(AHEAD, AHEAD):
  case CA(BEHIND, BEHIND):
    if (con->co == a->co)                       
    {
      return SATISFIED;
    }
    return INCOMPATIBLE;
    break;
  case CA('^', BEHIND):                                        
  case CA(BEHIND, '^'):
  case CA('$', AHEAD):
  case CA(AHEAD, '$'):
    return INCOMPATIBLE;
    break;
  case CA('^', '$'):                                     
  case CA('^', AHEAD):
  case CA(BEHIND, '$'):
  case CA(BEHIND, AHEAD):
  case CA('$', '^'):
  case CA('$', BEHIND):
  case CA(AHEAD, '^'):
  case CA(AHEAD, BEHIND):
  case CA('^', LACON):
  case CA(BEHIND, LACON):
  case CA('$', LACON):
  case CA(AHEAD, LACON):
    return COMPATIBLE;
    break;
  }
  assert(NOTREACHED);
  return INCOMPATIBLE;                                     
}

   
                                      
   
static void
fixempties(struct nfa *nfa, FILE *f)                                  
{
  struct state *s;
  struct state *s2;
  struct state *nexts;
  struct arc *a;
  struct arc *nexta;
  int totalinarcs;
  struct arc **inarcsorig;
  struct arc **arcarray;
  int arccount;
  int prevnins;
  int nskip;

     
                                                                        
                                                                      
                                                                            
     
  for (s = nfa->states; s != NULL && !NISERR(); s = nexts)
  {
    nexts = s->next;
    if (s->flag || s->nouts != 1)
    {
      continue;
    }
    a = s->outs;
    assert(a != NULL && a->outchain == NULL);
    if (a->type != EMPTY)
    {
      continue;
    }
    if (s != a->to)
    {
      moveins(nfa, s, a->to);
    }
    dropstate(nfa, s);
  }

     
                                                                            
                              
     
  for (s = nfa->states; s != NULL && !NISERR(); s = nexts)
  {
    nexts = s->next;
                                                                      
    assert(s->tmp == NULL);
    if (s->flag || s->nins != 1)
    {
      continue;
    }
    a = s->ins;
    assert(a != NULL && a->inchain == NULL);
    if (a->type != EMPTY)
    {
      continue;
    }
    if (s != a->from)
    {
      moveouts(nfa, s, a->from);
    }
    dropstate(nfa, s);
  }

  if (NISERR())
  {
    return;
  }

     
                                                                          
                                                                             
                                                  
     
                                                                           
                                                                       
                                                                          
                                                                        
                                                                      
                                                                    
     
                                                                            
                                                                         
                                                                          
                                                                             
                                                                            
                                                                        
                                                                          
                                                                        
                                                                   
                                                                            
                                                                             
                                                                            
                                                                            
                                                                             
     
                                                                            
                                                                           
                                                                           
                                                                           
                                                                          
                                                                           
     
                                                                          
                                                                        
                                                                            
                                                                             
                                                                          
                                                                         
            
     
                                                                            
                                                                            
                                                                             
                                                                          
                                                                           
                                                                        
                                     
     

                                                  
                                                                           
  inarcsorig = (struct arc **)MALLOC(nfa->nstates * sizeof(struct arc *));
  if (inarcsorig == NULL)
  {
    NERR(REG_ESPACE);
    return;
  }
  totalinarcs = 0;
  for (s = nfa->states; s != NULL; s = s->next)
  {
    inarcsorig[s->no] = s->ins;
    totalinarcs += s->nins;
  }

     
                                                                       
                                                                   
                                                                           
                                                          
     
  arcarray = (struct arc **)MALLOC(totalinarcs * sizeof(struct arc *));
  if (arcarray == NULL)
  {
    NERR(REG_ESPACE);
    FREE(inarcsorig);
    return;
  }

                                          
  for (s = nfa->states; s != NULL && !NISERR(); s = s->next)
  {
                                                                        
    if (!s->flag && !hasnonemptyout(s))
    {
      continue;
    }

                                                                      
    arccount = 0;
    for (s2 = emptyreachable(nfa, s, s, inarcsorig); s2 != s; s2 = nexts)
    {
                                                                      
      for (a = inarcsorig[s2->no]; a != NULL; a = a->inchain)
      {
        if (a->type != EMPTY)
        {
          arcarray[arccount++] = a;
        }
      }

                                                
      nexts = s2->tmp;
      s2->tmp = NULL;
    }
    s->tmp = NULL;
    assert(arccount <= totalinarcs);

                                                          
    prevnins = s->nins;

                                                  
    mergeins(nfa, s, arcarray, arccount);

                                                           
    nskip = s->nins - prevnins;
    a = s->ins;
    while (nskip-- > 0)
    {
      a = a->inchain;
    }
    inarcsorig[s->no] = a;
  }

  FREE(arcarray);
  FREE(inarcsorig);

  if (NISERR())
  {
    return;
  }

     
                                                                      
     
  for (s = nfa->states; s != NULL; s = s->next)
  {
    for (a = s->outs; a != NULL; a = nexta)
    {
      nexta = a->outchain;
      if (a->type == EMPTY)
      {
        freearc(nfa, a);
      }
    }
  }

     
                                                                           
                                                                             
                                                                           
     
  for (s = nfa->states; s != NULL; s = nexts)
  {
    nexts = s->next;
    if ((s->nins == 0 || s->nouts == 0) && !s->flag)
    {
      dropstate(nfa, s);
    }
  }

  if (f != NULL)
  {
    dumpnfa(nfa, f);
  }
}

   
                                                                               
   
                                                                            
                                                                          
                                                          
   
                                                                              
                                                                              
                                 
   
                                                                          
                                                                           
                                                                
   
static struct state *
emptyreachable(struct nfa *nfa, struct state *s, struct state *lastfound, struct arc **inarcsorig)
{
  struct arc *a;

                                                                     
  if (STACK_TOO_DEEP(nfa->v->re))
  {
    NERR(REG_ETOOBIG);
    return lastfound;
  }

  s->tmp = lastfound;
  lastfound = s;
  for (a = inarcsorig[s->no]; a != NULL; a = a->inchain)
  {
    if (a->type == EMPTY && a->from->tmp == NULL)
    {
      lastfound = emptyreachable(nfa, a->from, lastfound, inarcsorig);
    }
  }
  return lastfound;
}

   
                                                                   
   
static inline int
isconstraintarc(struct arc *a)
{
  switch (a->type)
  {
  case '^':
  case '$':
  case BEHIND:
  case AHEAD:
  case LACON:
    return 1;
  }
  return 0;
}

   
                                                            
   
static int
hasconstraintout(struct state *s)
{
  struct arc *a;

  for (a = s->outs; a != NULL; a = a->outchain)
  {
    if (isconstraintarc(a))
    {
      return 1;
    }
  }
  return 0;
}

   
                                                                         
   
                                                                         
                                                                         
                                                                           
                                    
   
static void
fixconstraintloops(struct nfa *nfa, FILE *f)                                  
{
  struct state *s;
  struct state *nexts;
  struct arc *a;
  struct arc *nexta;
  int hasconstraints;

     
                                                                           
                                                                          
                                                                           
                                                                  
     
  hasconstraints = 0;
  for (s = nfa->states; s != NULL && !NISERR(); s = nexts)
  {
    nexts = s->next;
                                                                      
    assert(s->tmp == NULL);
    for (a = s->outs; a != NULL && !NISERR(); a = nexta)
    {
      nexta = a->outchain;
      if (isconstraintarc(a))
      {
        if (a->to == s)
        {
          freearc(nfa, a);
        }
        else
        {
          hasconstraints = 1;
        }
      }
    }
                                                              
    if (s->nouts == 0 && !s->flag)
    {
      dropstate(nfa, s);
    }
  }

                                                     
  if (NISERR() || !hasconstraints)
  {
    return;
  }

     
                                                                   
                                                                         
                                                                             
                                                                        
                                                                     
     
restart:
  for (s = nfa->states; s != NULL && !NISERR(); s = s->next)
  {
    if (findconstraintloop(nfa, s))
    {
      goto restart;
    }
  }

  if (NISERR())
  {
    return;
  }

     
                                                                           
                                                                             
                                                                           
     
                                                                            
                                                                           
                   
     
  for (s = nfa->states; s != NULL; s = nexts)
  {
    nexts = s->next;
    s->tmp = NULL;
    if ((s->nins == 0 || s->nouts == 0) && !s->flag)
    {
      dropstate(nfa, s);
    }
  }

  if (f != NULL)
  {
    dumpnfa(nfa, f);
  }
}

   
                                                                   
   
                                                                      
                                 
   
                                                                         
                                                                          
                                                                             
                                                                             
                                                               
   
                                                                           
                                                               
   
                                                                               
                                                                             
                                                                   
   
static int
findconstraintloop(struct nfa *nfa, struct state *s)
{
  struct arc *a;

                                                                     
  if (STACK_TOO_DEEP(nfa->v->re))
  {
    NERR(REG_ETOOBIG);
    return 1;                                     
  }

  if (s->tmp != NULL)
  {
                                       
    if (s->tmp == s)
    {
      return 0;
    }
                                  
    breakconstraintloop(nfa, s);
                                                                    
    return 1;
  }
  for (a = s->outs; a != NULL; a = a->outchain)
  {
    if (isconstraintarc(a))
    {
      struct state *sto = a->to;

      assert(sto != s);
      s->tmp = sto;
      if (findconstraintloop(nfa, sto))
      {
        return 1;
      }
    }
  }

     
                                                                            
                                                                       
     
  s->tmp = s;
  return 0;
}

   
                                                         
   
                                                                         
                                                                           
                                           
   
                                                                           
                                                                            
                                                                               
                                                                        
                                                                            
                                                                              
                                                                              
                                                                            
                                                                               
                                                                            
                                                                            
                                                                               
                                                                          
                                                                              
                                                                             
                                                                        
                                                                            
                                                                           
                                                                        
                                                                            
                                                                        
                                                                            
                                       
   
                                                                       
                                                                               
                                                                        
                                                                             
                                                                          
                                                                               
                                                                              
                            
   
                                                                             
                                                                               
                                                                             
                                                                               
                                                                              
   
static void
breakconstraintloop(struct nfa *nfa, struct state *sinitial)
{
  struct state *s;
  struct state *shead;
  struct state *stail;
  struct state *sclone;
  struct state *nexts;
  struct arc *refarc;
  struct arc *a;
  struct arc *nexta;

     
                                                               
                                                                        
                                                                            
                                                                    
     
  refarc = NULL;
  s = sinitial;
  do
  {
    nexts = s->tmp;
    assert(nexts != s);                                           
    if (refarc == NULL)
    {
      int narcs = 0;

      for (a = s->outs; a != NULL; a = a->outchain)
      {
        if (a->to == nexts && isconstraintarc(a))
        {
          refarc = a;
          narcs++;
        }
      }
      assert(narcs > 0);
      if (narcs > 1)
      {
        refarc = NULL;                                             
      }
    }
    s = nexts;
  } while (s != sinitial);

  if (refarc)
  {
                             
    shead = refarc->from;
    stail = refarc->to;
    assert(stail == shead->tmp);
  }
  else
  {
                                                         
    shead = sinitial;
    stail = sinitial->tmp;
  }

     
                                                                       
                                                                            
                                          
     
  for (s = nfa->states; s != NULL; s = s->next)
  {
    s->tmp = NULL;
  }

     
                                                 
     
  sclone = newstate(nfa);
  if (sclone == NULL)
  {
    assert(NISERR());
    return;
  }

  clonesuccessorstates(nfa, stail, sclone, shead, refarc, NULL, NULL, nfa->nstates);

  if (NISERR())
  {
    return;
  }

     
                                                                         
                                                                         
                                                        
     
  if (sclone->nouts == 0)
  {
    freestate(nfa, sclone);
    sclone = NULL;
  }

     
                                                                             
                                            
     
  for (a = shead->outs; a != NULL; a = nexta)
  {
    nexta = a->outchain;
    if (a->to == stail && isconstraintarc(a))
    {
      if (sclone)
      {
        cparc(nfa, a, shead, sclone);
      }
      freearc(nfa, a);
      if (NISERR())
      {
        break;
      }
    }
  }
}

   
                                                                           
   
                                                                          
                                                                     
   
                                                                              
                                                                        
                                                                             
                                                                 
   
                                                                              
                                                                              
                                                                            
                                                                               
                                                                        
                                                                              
                                                                              
                                                                         
                                                                              
               
   
                                                                        
                                                                              
                                                                           
                                                                           
                          
   
                                                                              
                                                                            
                                                                            
                                                                              
                                                                               
                                                                               
                     
   
static void
clonesuccessorstates(struct nfa *nfa, struct state *ssource, struct state *sclone, struct state *spredecessor, struct arc *refarc, char *curdonemap, char *outerdonemap, int nstates)
{
  char *donemap;
  struct arc *a;

                                                                     
  if (STACK_TOO_DEEP(nfa->v->re))
  {
    NERR(REG_ETOOBIG);
    return;
  }

                                                              
  donemap = curdonemap;
  if (donemap == NULL)
  {
    donemap = (char *)MALLOC(nstates * sizeof(char));
    if (donemap == NULL)
    {
      NERR(REG_ESPACE);
      return;
    }

    if (outerdonemap != NULL)
    {
         
                                                                     
                                                                      
                                                                     
                                                        
         
      memcpy(donemap, outerdonemap, nstates * sizeof(char));
    }
    else
    {
                                                               
      memset(donemap, 0, nstates * sizeof(char));
      assert(spredecessor->no < nstates);
      donemap[spredecessor->no] = 1;
    }
  }

                                              
  assert(ssource->no < nstates);
  assert(donemap[ssource->no] == 0);
  donemap[ssource->no] = 1;

     
                                                                        
                                                                             
                                                                            
                                                                            
                                                                            
                                                                          
                                                                            
                                                                         
                                                                           
                                                    
     
                                                                           
                                                                            
                                                                          
                                                                            
                                                                
     
  for (a = ssource->outs; a != NULL && !NISERR(); a = a->outchain)
  {
    struct state *sto = a->to;

       
                                                                           
                                                                   
                                                                           
                                                                           
                      
       
    if (isconstraintarc(a) && hasconstraintout(sto))
    {
      struct state *prevclone;
      int canmerge;
      struct arc *a2;

         
                                                                         
                                                                   
         
      assert(sto->no < nstates);
      if (donemap[sto->no] != 0)
      {
        continue;
      }

         
                                                                    
                       
         
      prevclone = NULL;
      for (a2 = sclone->outs; a2 != NULL; a2 = a2->outchain)
      {
        if (a2->to->tmp == sto)
        {
          prevclone = a2->to;
          break;
        }
      }

         
                                                                       
                                                                         
                                                                     
                                        
         
      if (refarc && a->type == refarc->type && a->co == refarc->co)
      {
        canmerge = 1;
      }
      else
      {
        struct state *s;

        canmerge = 0;
        for (s = sclone; s->ins; s = s->ins->from)
        {
          if (s->nins == 1 && a->type == s->ins->type && a->co == s->ins->co)
          {
            canmerge = 1;
            break;
          }
        }
      }

      if (canmerge)
      {
           
                                                                    
                                                                     
                                                                      
                                                              
           
        if (prevclone)
        {
          dropstate(nfa, prevclone);                            
        }

                                                        
        clonesuccessorstates(nfa, sto, sclone, spredecessor, refarc, donemap, outerdonemap, nstates);
                                                            
        assert(NISERR() || donemap[sto->no] == 1);
      }
      else if (prevclone)
      {
           
                                                                     
                                   
           
        cparc(nfa, a, sclone, prevclone);
      }
      else
      {
           
                                                          
           
        struct state *stoclone;

        stoclone = newstate(nfa);
        if (stoclone == NULL)
        {
          assert(NISERR());
          break;
        }
                                                
        stoclone->tmp = sto;
                                                  
        cparc(nfa, a, sclone, stoclone);
      }
    }
    else
    {
         
                                                                         
                                                      
         
      cparc(nfa, a, sclone, sto);
    }
  }

     
                                                                             
                                                                      
                                                                           
                                                                            
                                       
     
  if (curdonemap == NULL)
  {
    for (a = sclone->outs; a != NULL && !NISERR(); a = a->outchain)
    {
      struct state *stoclone = a->to;
      struct state *sto = stoclone->tmp;

      if (sto != NULL)
      {
        stoclone->tmp = NULL;
        clonesuccessorstates(nfa, sto, stoclone, spredecessor, refarc, NULL, donemap, nstates);
      }
    }

                                                                 
    FREE(donemap);
  }
}

   
                                              
   
static void
cleanup(struct nfa *nfa)
{
  struct state *s;
  struct state *nexts;
  int n;

  if (NISERR())
  {
    return;
  }

                                                
                                                                   
  markreachable(nfa, nfa->pre, (struct state *)NULL, nfa->pre);
  markcanreach(nfa, nfa->post, nfa->pre, nfa->post);
  for (s = nfa->states; s != NULL && !NISERR(); s = nexts)
  {
    nexts = s->next;
    if (s->tmp != nfa->post && !s->flag)
    {
      dropstate(nfa, s);
    }
  }
  assert(NISERR() || nfa->post->nins == 0 || nfa->post->tmp == nfa->post);
  cleartraverse(nfa, nfa->pre);
  assert(NISERR() || nfa->post->nins == 0 || nfa->post->tmp == NULL);
                                                                 

                                 
  n = 0;
  for (s = nfa->states; s != NULL; s = s->next)
  {
    s->no = n++;
  }
  nfa->nstates = n;
}

   
                                                         
   
static void
markreachable(struct nfa *nfa, struct state *s, struct state *okay,                                          
    struct state *mark)                                                                         
{
  struct arc *a;

                                                                     
  if (STACK_TOO_DEEP(nfa->v->re))
  {
    NERR(REG_ETOOBIG);
    return;
  }

  if (s->tmp != okay)
  {
    return;
  }
  s->tmp = mark;

  for (a = s->outs; a != NULL; a = a->outchain)
  {
    markreachable(nfa, a->to, okay, mark);
  }
}

   
                                                                   
   
static void
markcanreach(struct nfa *nfa, struct state *s, struct state *okay,                                          
    struct state *mark)                                                                        
{
  struct arc *a;

                                                                     
  if (STACK_TOO_DEEP(nfa->v->re))
  {
    NERR(REG_ETOOBIG);
    return;
  }

  if (s->tmp != okay)
  {
    return;
  }
  s->tmp = mark;

  for (a = s->ins; a != NULL; a = a->inchain)
  {
    markcanreach(nfa, a->from, okay, mark);
  }
}

   
                                                                       
   
static long                                 
analyze(struct nfa *nfa)
{
  struct arc *a;
  struct arc *aa;

  if (NISERR())
  {
    return 0;
  }

  if (nfa->pre->outs == NULL)
  {
    return REG_UIMPOSSIBLE;
  }
  for (a = nfa->pre->outs; a != NULL; a = a->outchain)
  {
    for (aa = a->to->outs; aa != NULL; aa = aa->outchain)
    {
      if (aa->to == nfa->post)
      {
        return REG_UEMPTYMATCH;
      }
    }
  }
  return 0;
}

   
                                                            
   
static void
compact(struct nfa *nfa, struct cnfa *cnfa)
{
  struct state *s;
  struct arc *a;
  size_t nstates;
  size_t narcs;
  struct carc *ca;
  struct carc *first;

  assert(!NISERR());

  nstates = 0;
  narcs = 0;
  for (s = nfa->states; s != NULL; s = s->next)
  {
    nstates++;
    narcs += s->nouts + 1;                                   
  }

  cnfa->stflags = (char *)MALLOC(nstates * sizeof(char));
  cnfa->states = (struct carc **)MALLOC(nstates * sizeof(struct carc *));
  cnfa->arcs = (struct carc *)MALLOC(narcs * sizeof(struct carc));
  if (cnfa->stflags == NULL || cnfa->states == NULL || cnfa->arcs == NULL)
  {
    if (cnfa->stflags != NULL)
    {
      FREE(cnfa->stflags);
    }
    if (cnfa->states != NULL)
    {
      FREE(cnfa->states);
    }
    if (cnfa->arcs != NULL)
    {
      FREE(cnfa->arcs);
    }
    NERR(REG_ESPACE);
    return;
  }
  cnfa->nstates = nstates;
  cnfa->pre = nfa->pre->no;
  cnfa->post = nfa->post->no;
  cnfa->bos[0] = nfa->bos[0];
  cnfa->bos[1] = nfa->bos[1];
  cnfa->eos[0] = nfa->eos[0];
  cnfa->eos[1] = nfa->eos[1];
  cnfa->ncolors = maxcolor(nfa->cm) + 1;
  cnfa->flags = 0;

  ca = cnfa->arcs;
  for (s = nfa->states; s != NULL; s = s->next)
  {
    assert((size_t)s->no < nstates);
    cnfa->stflags[s->no] = 0;
    cnfa->states[s->no] = ca;
    first = ca;
    for (a = s->outs; a != NULL; a = a->outchain)
    {
      switch (a->type)
      {
      case PLAIN:
        ca->co = a->co;
        ca->to = a->to->no;
        ca++;
        break;
      case LACON:
        assert(s->no != cnfa->pre);
        ca->co = (color)(cnfa->ncolors + a->co);
        ca->to = a->to->no;
        ca++;
        cnfa->flags |= HASLACONS;
        break;
      default:
        NERR(REG_ASSERT);
        break;
      }
    }
    carcsort(first, ca - first);
    ca->co = COLORLESS;
    ca->to = 0;
    ca++;
  }
  assert(ca == &cnfa->arcs[narcs]);
  assert(cnfa->nstates != 0);

                               
  for (a = nfa->pre->outs; a != NULL; a = a->outchain)
  {
    cnfa->stflags[a->to->no] = CNFA_NOPROGRESS;
  }
  cnfa->stflags[nfa->pre->no] = CNFA_NOPROGRESS;
}

   
                                               
   
static void
carcsort(struct carc *first, size_t n)
{
  if (n > 1)
  {
    qsort(first, n, sizeof(struct carc), carc_cmp);
  }
}

static int
carc_cmp(const void *a, const void *b)
{
  const struct carc *aa = (const struct carc *)a;
  const struct carc *bb = (const struct carc *)b;

  if (aa->co < bb->co)
  {
    return -1;
  }
  if (aa->co > bb->co)
  {
    return +1;
  }
  if (aa->to < bb->to)
  {
    return -1;
  }
  if (aa->to > bb->to)
  {
    return +1;
  }
  return 0;
}

   
                                   
   
static void
freecnfa(struct cnfa *cnfa)
{
  assert(cnfa->nstates != 0);                        
  cnfa->nstates = 0;
  FREE(cnfa->stflags);
  FREE(cnfa->states);
  FREE(cnfa->arcs);
}

   
                                                
   
static void
dumpnfa(struct nfa *nfa, FILE *f)
{
#ifdef REG_DEBUG
  struct state *s;
  int nstates = 0;
  int narcs = 0;

  fprintf(f, "pre %d, post %d", nfa->pre->no, nfa->post->no);
  if (nfa->bos[0] != COLORLESS)
  {
    fprintf(f, ", bos [%ld]", (long)nfa->bos[0]);
  }
  if (nfa->bos[1] != COLORLESS)
  {
    fprintf(f, ", bol [%ld]", (long)nfa->bos[1]);
  }
  if (nfa->eos[0] != COLORLESS)
  {
    fprintf(f, ", eos [%ld]", (long)nfa->eos[0]);
  }
  if (nfa->eos[1] != COLORLESS)
  {
    fprintf(f, ", eol [%ld]", (long)nfa->eos[1]);
  }
  fprintf(f, "\n");
  for (s = nfa->states; s != NULL; s = s->next)
  {
    dumpstate(s, f);
    nstates++;
    narcs += s->nouts;
  }
  fprintf(f, "total of %d states, %d arcs\n", nstates, narcs);
  if (nfa->parent == NULL)
  {
    dumpcolors(nfa->cm, f);
  }
  fflush(f);
#endif
}

#ifdef REG_DEBUG                              

   
                                                        
   
static void
dumpstate(struct state *s, FILE *f)
{
  struct arc *a;

  fprintf(f, "%d%s%c", s->no, (s->tmp != NULL) ? "T" : "", (s->flag) ? s->flag : '.');
  if (s->prev != NULL && s->prev->next != s)
  {
    fprintf(f, "\tstate chain bad\n");
  }
  if (s->nouts == 0)
  {
    fprintf(f, "\tno out arcs\n");
  }
  else
  {
    dumparcs(s, f);
  }
  fflush(f);
  for (a = s->ins; a != NULL; a = a->inchain)
  {
    if (a->to != s)
    {
      fprintf(f, "\tlink from %d to %d on %d's in-chain\n", a->from->no, a->to->no, s->no);
    }
  }
}

   
                                                   
   
static void
dumparcs(struct state *s, FILE *f)
{
  int pos;
  struct arc *a;

                                                     
  a = s->outs;
  assert(a != NULL);
  while (a->outchain != NULL)
  {
    a = a->outchain;
  }
  pos = 1;
  do
  {
    dumparc(a, s, f);
    if (pos == 5)
    {
      fprintf(f, "\n");
      pos = 1;
    }
    else
    {
      pos++;
    }
    a = a->outchainRev;
  } while (a != NULL);
  if (pos != 1)
  {
    fprintf(f, "\n");
  }
}

   
                                                                       
   
static void
dumparc(struct arc *a, struct state *s, FILE *f)
{
  struct arc *aa;
  struct arcbatch *ab;

  fprintf(f, "\t");
  switch (a->type)
  {
  case PLAIN:
    fprintf(f, "[%ld]", (long)a->co);
    break;
  case AHEAD:
    fprintf(f, ">%ld>", (long)a->co);
    break;
  case BEHIND:
    fprintf(f, "<%ld<", (long)a->co);
    break;
  case LACON:
    fprintf(f, ":%ld:", (long)a->co);
    break;
  case '^':
  case '$':
    fprintf(f, "%c%d", a->type, (int)a->co);
    break;
  case EMPTY:
    break;
  default:
    fprintf(f, "0x%x/0%lo", a->type, (long)a->co);
    break;
  }
  if (a->from != s)
  {
    fprintf(f, "?%d?", a->from->no);
  }
  for (ab = &a->from->oas; ab != NULL; ab = ab->next)
  {
    for (aa = &ab->a[0]; aa < &ab->a[ABSIZE]; aa++)
    {
      if (aa == a)
      {
        break;                     
      }
    }
    if (aa < &ab->a[ABSIZE])                      
    {
      break;                     
    }
  }
  if (ab == NULL)
  {
    fprintf(f, "?!?");                             
  }
  fprintf(f, "->");
  if (a->to == NULL)
  {
    fprintf(f, "NULL");
    return;
  }
  fprintf(f, "%d", a->to->no);
  for (aa = a->to->ins; aa != NULL; aa = aa->inchain)
  {
    if (aa == a)
    {
      break;                     
    }
  }
  if (aa == NULL)
  {
    fprintf(f, "?!?");                            
  }
}
#endif                

   
                                                          
   
#ifdef REG_DEBUG
static void
dumpcnfa(struct cnfa *cnfa, FILE *f)
{
  int st;

  fprintf(f, "pre %d, post %d", cnfa->pre, cnfa->post);
  if (cnfa->bos[0] != COLORLESS)
  {
    fprintf(f, ", bos [%ld]", (long)cnfa->bos[0]);
  }
  if (cnfa->bos[1] != COLORLESS)
  {
    fprintf(f, ", bol [%ld]", (long)cnfa->bos[1]);
  }
  if (cnfa->eos[0] != COLORLESS)
  {
    fprintf(f, ", eos [%ld]", (long)cnfa->eos[0]);
  }
  if (cnfa->eos[1] != COLORLESS)
  {
    fprintf(f, ", eol [%ld]", (long)cnfa->eos[1]);
  }
  if (cnfa->flags & HASLACONS)
  {
    fprintf(f, ", haslacons");
  }
  fprintf(f, "\n");
  for (st = 0; st < cnfa->nstates; st++)
  {
    dumpcstate(st, cnfa, f);
  }
  fflush(f);
}
#endif

#ifdef REG_DEBUG                               

   
                                                                  
   
static void
dumpcstate(int st, struct cnfa *cnfa, FILE *f)
{
  struct carc *ca;
  int pos;

  fprintf(f, "%d%s", st, (cnfa->stflags[st] & CNFA_NOPROGRESS) ? ":" : ".");
  pos = 1;
  for (ca = cnfa->states[st]; ca->co != COLORLESS; ca++)
  {
    if (ca->co < cnfa->ncolors)
    {
      fprintf(f, "\t[%ld]->%d", (long)ca->co, ca->to);
    }
    else
    {
      fprintf(f, "\t:%ld:->%d", (long)(ca->co - cnfa->ncolors), ca->to);
    }
    if (pos == 5)
    {
      fprintf(f, "\n");
      pos = 1;
    }
    else
    {
      pos++;
    }
  }
  if (ca == cnfa->states[st] || pos != 1)
  {
    fprintf(f, "\n");
  }
  fflush(f);
}

#endif                
