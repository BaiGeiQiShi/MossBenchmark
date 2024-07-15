   
                                                                            
   
                                                                          
   

                                                           

    
                            
                                                                      
   
                                                                      
                                                                      
            
                                                                     
                                                                   
                                                                        
                                                                         
                                                                          
                                                                           
                                                                           
                                                
   
                                                                           
                                                                         
                                                                              
                                                                            
                                                                              
                                                                           
                                                                         
                                                                              
                                                                             
                                                                          
                
   

   
                                                           
                                  
                                                          
   
                                                                               
                                                                               
   
                                                                            
                                                                      
                                                                             
                                                                            
                                                                         
                                          
   

static void
swapfunc(SortTuple *a, SortTuple *b, size_t n)
{
  do
  {
    SortTuple t = *a;
    *a++ = *b;
    *b++ = t;
  } while (--n > 0);
}

#define swap(a, b)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    SortTuple t = *(a);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    *(a) = *(b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    *(b) = t;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  } while (0)

#define vecswap(a, b, n)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  if ((n) > 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  swapfunc(a, b, n)

static SortTuple *
med3_tuple(SortTuple *a, SortTuple *b, SortTuple *c, SortTupleComparator cmp_tuple, Tuplesortstate *state)
{
  return cmp_tuple(a, b, state) < 0 ? (cmp_tuple(b, c, state) < 0 ? b : (cmp_tuple(a, c, state) < 0 ? c : a)) : (cmp_tuple(b, c, state) > 0 ? b : (cmp_tuple(a, c, state) < 0 ? a : c));
}

static void
qsort_tuple(SortTuple *a, size_t n, SortTupleComparator cmp_tuple, Tuplesortstate *state)
{
  SortTuple *pa, *pb, *pc, *pd, *pl, *pm, *pn;
  size_t d1, d2;
  int r, presorted;

loop:
  CHECK_FOR_INTERRUPTS();
  if (n < 7)
  {
    for (pm = a + 1; pm < a + n; pm++)
    {
      for (pl = pm; pl > a && cmp_tuple(pl - 1, pl, state) > 0; pl--)
      {
        swap(pl, pl - 1);
      }
    }
    return;
  }
  presorted = 1;
  for (pm = a + 1; pm < a + n; pm++)
  {
    CHECK_FOR_INTERRUPTS();
    if (cmp_tuple(pm - 1, pm, state) > 0)
    {
      presorted = 0;
      break;
    }
  }
  if (presorted)
  {
    return;
  }
  pm = a + (n / 2);
  if (n > 7)
  {
    pl = a;
    pn = a + (n - 1);
    if (n > 40)
    {
      size_t d = (n / 8);

      pl = med3_tuple(pl, pl + d, pl + 2 * d, cmp_tuple, state);
      pm = med3_tuple(pm - d, pm, pm + d, cmp_tuple, state);
      pn = med3_tuple(pn - 2 * d, pn - d, pn, cmp_tuple, state);
    }
    pm = med3_tuple(pl, pm, pn, cmp_tuple, state);
  }
  swap(a, pm);
  pa = pb = a + 1;
  pc = pd = a + (n - 1);
  for (;;)
  {
    while (pb <= pc && (r = cmp_tuple(pb, a, state)) <= 0)
    {
      if (r == 0)
      {
        swap(pa, pb);
        pa++;
      }
      pb++;
      CHECK_FOR_INTERRUPTS();
    }
    while (pb <= pc && (r = cmp_tuple(pc, a, state)) >= 0)
    {
      if (r == 0)
      {
        swap(pc, pd);
        pd--;
      }
      pc--;
      CHECK_FOR_INTERRUPTS();
    }
    if (pb > pc)
    {
      break;
    }
    swap(pb, pc);
    pb++;
    pc--;
  }
  pn = a + n;
  d1 = Min(pa - a, pb - pa);
  vecswap(a, pb - d1, d1);
  d1 = Min(pd - pc, pn - pd - 1);
  vecswap(pb, pn - d1, d1);
  d1 = pb - pa;
  d2 = pd - pc;
  if (d1 <= d2)
  {
                                                                    
    if (d1 > 1)
    {
      qsort_tuple(a, d1, cmp_tuple, state);
    }
    if (d2 > 1)
    {
                                                           
                                                       
      a = pn - d2;
      n = d2;
      goto loop;
    }
  }
  else
  {
                                                                    
    if (d2 > 1)
    {
      qsort_tuple(pn - d2, d2, cmp_tuple, state);
    }
    if (d1 > 1)
    {
                                                           
                                                 
      n = d1;
      goto loop;
    }
  }
}

#define cmp_ssup(a, b, ssup) ApplySortComparator((a)->datum1, (a)->isnull1, (b)->datum1, (b)->isnull1, ssup)

static SortTuple *
med3_ssup(SortTuple *a, SortTuple *b, SortTuple *c, SortSupport ssup)
{
  return cmp_ssup(a, b, ssup) < 0 ? (cmp_ssup(b, c, ssup) < 0 ? b : (cmp_ssup(a, c, ssup) < 0 ? c : a)) : (cmp_ssup(b, c, ssup) > 0 ? b : (cmp_ssup(a, c, ssup) < 0 ? a : c));
}

static void
qsort_ssup(SortTuple *a, size_t n, SortSupport ssup)
{
  SortTuple *pa, *pb, *pc, *pd, *pl, *pm, *pn;
  size_t d1, d2;
  int r, presorted;

loop:
  CHECK_FOR_INTERRUPTS();
  if (n < 7)
  {
    for (pm = a + 1; pm < a + n; pm++)
    {
      for (pl = pm; pl > a && cmp_ssup(pl - 1, pl, ssup) > 0; pl--)
      {
        swap(pl, pl - 1);
      }
    }
    return;
  }
  presorted = 1;
  for (pm = a + 1; pm < a + n; pm++)
  {
    CHECK_FOR_INTERRUPTS();
    if (cmp_ssup(pm - 1, pm, ssup) > 0)
    {
      presorted = 0;
      break;
    }
  }
  if (presorted)
  {
    return;
  }
  pm = a + (n / 2);
  if (n > 7)
  {
    pl = a;
    pn = a + (n - 1);
    if (n > 40)
    {
      size_t d = (n / 8);

      pl = med3_ssup(pl, pl + d, pl + 2 * d, ssup);
      pm = med3_ssup(pm - d, pm, pm + d, ssup);
      pn = med3_ssup(pn - 2 * d, pn - d, pn, ssup);
    }
    pm = med3_ssup(pl, pm, pn, ssup);
  }
  swap(a, pm);
  pa = pb = a + 1;
  pc = pd = a + (n - 1);
  for (;;)
  {
    while (pb <= pc && (r = cmp_ssup(pb, a, ssup)) <= 0)
    {
      if (r == 0)
      {
        swap(pa, pb);
        pa++;
      }
      pb++;
      CHECK_FOR_INTERRUPTS();
    }
    while (pb <= pc && (r = cmp_ssup(pc, a, ssup)) >= 0)
    {
      if (r == 0)
      {
        swap(pc, pd);
        pd--;
      }
      pc--;
      CHECK_FOR_INTERRUPTS();
    }
    if (pb > pc)
    {
      break;
    }
    swap(pb, pc);
    pb++;
    pc--;
  }
  pn = a + n;
  d1 = Min(pa - a, pb - pa);
  vecswap(a, pb - d1, d1);
  d1 = Min(pd - pc, pn - pd - 1);
  vecswap(pb, pn - d1, d1);
  d1 = pb - pa;
  d2 = pd - pc;
  if (d1 <= d2)
  {
                                                                    
    if (d1 > 1)
    {
      qsort_ssup(a, d1, ssup);
    }
    if (d2 > 1)
    {
                                                           
                                          
      a = pn - d2;
      n = d2;
      goto loop;
    }
  }
  else
  {
                                                                    
    if (d2 > 1)
    {
      qsort_ssup(pn - d2, d2, ssup);
    }
    if (d1 > 1)
    {
                                                           
                                    
      n = d1;
      goto loop;
    }
  }
}
