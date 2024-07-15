   
                    
                                        
   
                                                                 
   
                                                                            
                                                                            
                                                                          
                       
   
                                                                        
                                                                
                                                                          
                                                        
   
                                                                           
                                                             
   
                                                                              
                                                                            
                                                                           
                                                                          
                                                                       
                                                                               
                                                                            
                                                                           
                                                                          
                                              
   
                                
   
   

                                    
#define ATEOS() (v->now >= v->stop)
#define HAVE(n) (v->stop - v->now >= (n))
#define NEXT1(c) (!ATEOS() && *v->now == CHR(c))
#define NEXT2(a, b) (HAVE(2) && *v->now == CHR(a) && *(v->now + 1) == CHR(b))
#define NEXT3(a, b, c) (HAVE(3) && *v->now == CHR(a) && *(v->now + 1) == CHR(b) && *(v->now + 2) == CHR(c))
#define SET(c) (v->nexttype = (c))
#define SETV(c, n) (v->nexttype = (c), v->nextvalue = (n))
#define RET(c) return (SET(c), 1)
#define RETV(c, n) return (SETV(c, n), 1)
#define FAILW(e) return (ERR(e), 0)                        
#define LASTTYPE(t) (v->lasttype == (t))

                      
#define L_ERE 1                         
#define L_BRE 2                     
#define L_Q 3                    
#define L_EBND 4                     
#define L_BBND 5                 
#define L_BRACK 6               
#define L_CEL 7                          
#define L_ECL 8                          
#define L_CCL 9                        
#define INTOCON(c) (v->lexcon = (c))
#define INCON(con) (v->lexcon == (con))

                                             
#define ENDOF(array) ((array) + sizeof(array) / sizeof(chr))

   
                                                         
   
static void
lexstart(struct vars *v)
{
  prefixes(v);                                     
  NOERR();

  if (v->cflags & REG_QUOTE)
  {
    assert(!(v->cflags & (REG_ADVANCED | REG_EXPANDED | REG_NEWLINE)));
    INTOCON(L_Q);
  }
  else if (v->cflags & REG_EXTENDED)
  {
    assert(!(v->cflags & REG_QUOTE));
    INTOCON(L_ERE);
  }
  else
  {
    assert(!(v->cflags & (REG_QUOTE | REG_ADVF)));
    INTOCON(L_BRE);
  }

  v->nexttype = EMPTY;                                    
  next(v);                                         
}

   
                                                 
   
static void
prefixes(struct vars *v)
{
                                                    
  if (v->cflags & REG_QUOTE)
  {
    return;
  }

                                         
  if (HAVE(4) && NEXT3('*', '*', '*'))
  {
    switch (*(v->now + 3))
    {
    case CHR('?'):                                      
      ERR(REG_BADPAT);
      return;                         
      break;
    case CHR('='):                                      
      NOTE(REG_UNONPOSIX);
      v->cflags |= REG_QUOTE;
      v->cflags &= ~(REG_ADVANCED | REG_EXPANDED | REG_NEWLINE);
      v->now += 4;
      return;                                        
      break;
    case CHR(':'):                            
      NOTE(REG_UNONPOSIX);
      v->cflags |= REG_ADVANCED;
      v->now += 4;
      break;
    default:                                     
      ERR(REG_BADRPT);
      return;
      break;
    }
  }

                                                
  if ((v->cflags & REG_ADVANCED) != REG_ADVANCED)
  {
    return;
  }

                                    
  if (HAVE(3) && NEXT2('(', '?') && iscalpha(*(v->now + 2)))
  {
    NOTE(REG_UNONPOSIX);
    v->now += 2;
    for (; !ATEOS() && iscalpha(*v->now); v->now++)
    {
      switch (*v->now)
      {
      case CHR('b'):                        
        v->cflags &= ~(REG_ADVANCED | REG_QUOTE);
        break;
      case CHR('c'):                     
        v->cflags &= ~REG_ICASE;
        break;
      case CHR('e'):                 
        v->cflags |= REG_EXTENDED;
        v->cflags &= ~(REG_ADVF | REG_QUOTE);
        break;
      case CHR('i'):                       
        v->cflags |= REG_ICASE;
        break;
      case CHR('m'):                            
      case CHR('n'):                          
        v->cflags |= REG_NEWLINE;
        break;
      case CHR('p'):                             
        v->cflags |= REG_NLSTOP;
        v->cflags &= ~REG_NLANCH;
        break;
      case CHR('q'):                     
        v->cflags |= REG_QUOTE;
        v->cflags &= ~REG_ADVANCED;
        break;
      case CHR('s'):                               
        v->cflags &= ~REG_NEWLINE;
        break;
      case CHR('t'):                   
        v->cflags &= ~REG_EXPANDED;
        break;
      case CHR('w'):                                 
        v->cflags &= ~REG_NLSTOP;
        v->cflags |= REG_NLANCH;
        break;
      case CHR('x'):                      
        v->cflags |= REG_EXPANDED;
        break;
      default:
        ERR(REG_BADOPT);
        return;
      }
    }
    if (!NEXT1(')'))
    {
      ERR(REG_BADOPT);
      return;
    }
    v->now++;
    if (v->cflags & REG_QUOTE)
    {
      v->cflags &= ~(REG_EXPANDED | REG_NEWLINE);
    }
  }
}

   
                                                                            
   
                                                                     
                                                                        
   
static void
lexnest(struct vars *v, const chr *beginp,                             
    const chr *endp)                                                          
{
  assert(v->savenow == NULL);                                
  v->savenow = v->now;
  v->savestop = v->stop;
  v->now = beginp;
  v->stop = endp;
}

   
                                                                   
   
static const chr backd[] = {        
    CHR('['), CHR('['), CHR(':'), CHR('d'), CHR('i'), CHR('g'), CHR('i'), CHR('t'), CHR(':'), CHR(']'), CHR(']')};
static const chr backD[] = {        
    CHR('['), CHR('^'), CHR('['), CHR(':'), CHR('d'), CHR('i'), CHR('g'), CHR('i'), CHR('t'), CHR(':'), CHR(']'), CHR(']')};
static const chr brbackd[] = {                        
    CHR('['), CHR(':'), CHR('d'), CHR('i'), CHR('g'), CHR('i'), CHR('t'), CHR(':'), CHR(']')};
static const chr backs[] = {        
    CHR('['), CHR('['), CHR(':'), CHR('s'), CHR('p'), CHR('a'), CHR('c'), CHR('e'), CHR(':'), CHR(']'), CHR(']')};
static const chr backS[] = {        
    CHR('['), CHR('^'), CHR('['), CHR(':'), CHR('s'), CHR('p'), CHR('a'), CHR('c'), CHR('e'), CHR(':'), CHR(']'), CHR(']')};
static const chr brbacks[] = {                        
    CHR('['), CHR(':'), CHR('s'), CHR('p'), CHR('a'), CHR('c'), CHR('e'), CHR(':'), CHR(']')};
static const chr backw[] = {        
    CHR('['), CHR('['), CHR(':'), CHR('a'), CHR('l'), CHR('n'), CHR('u'), CHR('m'), CHR(':'), CHR(']'), CHR('_'), CHR(']')};
static const chr backW[] = {        
    CHR('['), CHR('^'), CHR('['), CHR(':'), CHR('a'), CHR('l'), CHR('n'), CHR('u'), CHR('m'), CHR(':'), CHR(']'), CHR('_'), CHR(']')};
static const chr brbackw[] = {                        
    CHR('['), CHR(':'), CHR('a'), CHR('l'), CHR('n'), CHR('u'), CHR('m'), CHR(':'), CHR(']'), CHR('_')};

   
                                                                  
                                                                        
   
static void
lexword(struct vars *v)
{
  lexnest(v, backw, ENDOF(backw));
}

   
                         
   
static int                          
next(struct vars *v)
{
  chr c;

next_restart:                                       

                                                     
  if (ISERR())
  {
    return 0;                                        
  }

                                     
  v->lasttype = v->nexttype;

                   
  if (v->nexttype == EMPTY && (v->cflags & REG_BOSONLY))
  {
                                      
    RETV(SBEGIN, 0);                 
  }

                                                                
  if (v->savenow != NULL && ATEOS())
  {
    v->now = v->savenow;
    v->stop = v->savestop;
    v->savenow = v->savestop = NULL;
  }

                                                                   
  if (v->cflags & REG_EXPANDED)
  {
    switch (v->lexcon)
    {
    case L_ERE:
    case L_BRE:
    case L_EBND:
    case L_BBND:
      skip(v);
      break;
    }
  }

                                        
  if (ATEOS())
  {
    switch (v->lexcon)
    {
    case L_ERE:
    case L_BRE:
    case L_Q:
      RET(EOS);
      break;
    case L_EBND:
    case L_BBND:
      FAILW(REG_EBRACE);
      break;
    case L_BRACK:
    case L_CEL:
    case L_ECL:
    case L_CCL:
      FAILW(REG_EBRACK);
      break;
    }
    assert(NOTREACHED);
  }

                                              
  c = *v->now++;

                                                            
  switch (v->lexcon)
  {
  case L_BRE:                                     
    return brenext(v, c);
    break;
  case L_ERE:                
    break;
  case L_Q:                               
    RETV(PLAIN, c);
    break;
  case L_BBND:                               
  case L_EBND:
    switch (c)
    {
    case CHR('0'):
    case CHR('1'):
    case CHR('2'):
    case CHR('3'):
    case CHR('4'):
    case CHR('5'):
    case CHR('6'):
    case CHR('7'):
    case CHR('8'):
    case CHR('9'):
      RETV(DIGIT, (chr)DIGITVAL(c));
      break;
    case CHR(','):
      RET(',');
      break;
    case CHR('}'):                            
      if (INCON(L_EBND))
      {
        INTOCON(L_ERE);
        if ((v->cflags & REG_ADVF) && NEXT1('?'))
        {
          v->now++;
          NOTE(REG_UNONPOSIX);
          RETV('}', 0);
        }
        RETV('}', 1);
      }
      else
      {
        FAILW(REG_BADBR);
      }
      break;
    case CHR('\\'):                             
      if (INCON(L_BBND) && NEXT1('}'))
      {
        v->now++;
        INTOCON(L_BRE);
        RETV('}', 1);
      }
      else
      {
        FAILW(REG_BADBR);
      }
      break;
    default:
      FAILW(REG_BADBR);
      break;
    }
    assert(NOTREACHED);
    break;
  case L_BRACK:                                
    switch (c)
    {
    case CHR(']'):
      if (LASTTYPE('['))
      {
        RETV(PLAIN, c);
      }
      else
      {
        INTOCON((v->cflags & REG_EXTENDED) ? L_ERE : L_BRE);
        RET(']');
      }
      break;
    case CHR('\\'):
      NOTE(REG_UBBS);
      if (!(v->cflags & REG_ADVF))
      {
        RETV(PLAIN, c);
      }
      NOTE(REG_UNONPOSIX);
      if (ATEOS())
      {
        FAILW(REG_EESCAPE);
      }
      (DISCARD) lexescape(v);
      switch (v->nexttype)
      {                                
      case PLAIN:
        return 1;
        break;
      case CCLASS:
        switch (v->nextvalue)
        {
        case 'd':
          lexnest(v, brbackd, ENDOF(brbackd));
          break;
        case 's':
          lexnest(v, brbacks, ENDOF(brbacks));
          break;
        case 'w':
          lexnest(v, brbackw, ENDOF(brbackw));
          break;
        default:
          FAILW(REG_EESCAPE);
          break;
        }
                                                 
        v->nexttype = v->lasttype;
        return next(v);
        break;
      }
                                             
      FAILW(REG_EESCAPE);
      break;
    case CHR('-'):
      if (LASTTYPE('[') || NEXT1(']'))
      {
        RETV(PLAIN, c);
      }
      else
      {
        RETV(RANGE, c);
      }
      break;
    case CHR('['):
      if (ATEOS())
      {
        FAILW(REG_EBRACK);
      }
      switch (*v->now++)
      {
      case CHR('.'):
        INTOCON(L_CEL);
                                                   
        RET(COLLEL);
        break;
      case CHR('='):
        INTOCON(L_ECL);
        NOTE(REG_ULOCALE);
        RET(ECLASS);
        break;
      case CHR(':'):
        INTOCON(L_CCL);
        NOTE(REG_ULOCALE);
        RET(CCLASS);
        break;
      default:           
        v->now--;
        RETV(PLAIN, c);
        break;
      }
      assert(NOTREACHED);
      break;
    default:
      RETV(PLAIN, c);
      break;
    }
    assert(NOTREACHED);
    break;
  case L_CEL:                                  
    if (c == CHR('.') && NEXT1(']'))
    {
      v->now++;
      INTOCON(L_BRACK);
      RETV(END, '.');
    }
    else
    {
      RETV(PLAIN, c);
    }
    break;
  case L_ECL:                                
    if (c == CHR('=') && NEXT1(']'))
    {
      v->now++;
      INTOCON(L_BRACK);
      RETV(END, '=');
    }
    else
    {
      RETV(PLAIN, c);
    }
    break;
  case L_CCL:                              
    if (c == CHR(':') && NEXT1(']'))
    {
      v->now++;
      INTOCON(L_BRACK);
      RETV(END, ':');
    }
    else
    {
      RETV(PLAIN, c);
    }
    break;
  default:
    assert(NOTREACHED);
    break;
  }

                                                       
  assert(INCON(L_ERE));

                                                       
  switch (c)
  {
  case CHR('|'):
    RET('|');
    break;
  case CHR('*'):
    if ((v->cflags & REG_ADVF) && NEXT1('?'))
    {
      v->now++;
      NOTE(REG_UNONPOSIX);
      RETV('*', 0);
    }
    RETV('*', 1);
    break;
  case CHR('+'):
    if ((v->cflags & REG_ADVF) && NEXT1('?'))
    {
      v->now++;
      NOTE(REG_UNONPOSIX);
      RETV('+', 0);
    }
    RETV('+', 1);
    break;
  case CHR('?'):
    if ((v->cflags & REG_ADVF) && NEXT1('?'))
    {
      v->now++;
      NOTE(REG_UNONPOSIX);
      RETV('?', 0);
    }
    RETV('?', 1);
    break;
  case CHR('{'):                                      
    if (v->cflags & REG_EXPANDED)
    {
      skip(v);
    }
    if (ATEOS() || !iscdigit(*v->now))
    {
      NOTE(REG_UBRACES);
      NOTE(REG_UUNSPEC);
      RETV(PLAIN, c);
    }
    else
    {
      NOTE(REG_UBOUNDS);
      INTOCON(L_EBND);
      RET('{');
    }
    assert(NOTREACHED);
    break;
  case CHR('('):                                         
    if ((v->cflags & REG_ADVF) && NEXT1('?'))
    {
      NOTE(REG_UNONPOSIX);
      v->now++;
      if (ATEOS())
      {
        FAILW(REG_BADRPT);
      }
      switch (*v->now++)
      {
      case CHR(':'):                          
        RETV('(', 0);
        break;
      case CHR('#'):              
        while (!ATEOS() && *v->now != CHR(')'))
        {
          v->now++;
        }
        if (!ATEOS())
        {
          v->now++;
        }
        assert(v->nexttype == v->lasttype);
        goto next_restart;
      case CHR('='):                         
        NOTE(REG_ULOOKAROUND);
        RETV(LACON, LATYPE_AHEAD_POS);
        break;
      case CHR('!'):                         
        NOTE(REG_ULOOKAROUND);
        RETV(LACON, LATYPE_AHEAD_NEG);
        break;
      case CHR('<'):
        if (ATEOS())
        {
          FAILW(REG_BADRPT);
        }
        switch (*v->now++)
        {
        case CHR('='):                          
          NOTE(REG_ULOOKAROUND);
          RETV(LACON, LATYPE_BEHIND_POS);
          break;
        case CHR('!'):                          
          NOTE(REG_ULOOKAROUND);
          RETV(LACON, LATYPE_BEHIND_NEG);
          break;
        default:
          FAILW(REG_BADRPT);
          break;
        }
        assert(NOTREACHED);
        break;
      default:
        FAILW(REG_BADRPT);
        break;
      }
      assert(NOTREACHED);
    }
    if (v->cflags & REG_NOSUB)
    {
      RETV('(', 0);                               
    }
    else
    {
      RETV('(', 1);
    }
    break;
  case CHR(')'):
    if (LASTTYPE('('))
    {
      NOTE(REG_UUNSPEC);
    }
    RETV(')', c);
    break;
  case CHR('['):                                          
    if (HAVE(6) && *(v->now + 0) == CHR('[') && *(v->now + 1) == CHR(':') && (*(v->now + 2) == CHR('<') || *(v->now + 2) == CHR('>')) && *(v->now + 3) == CHR(':') && *(v->now + 4) == CHR(']') && *(v->now + 5) == CHR(']'))
    {
      c = *(v->now + 2);
      v->now += 6;
      NOTE(REG_UNONPOSIX);
      RET((c == CHR('<')) ? '<' : '>');
    }
    INTOCON(L_BRACK);
    if (NEXT1('^'))
    {
      v->now++;
      RETV('[', 0);
    }
    RETV('[', 1);
    break;
  case CHR('.'):
    RET('.');
    break;
  case CHR('^'):
    RET('^');
    break;
  case CHR('$'):
    RET('$');
    break;
  case CHR('\\'):                                            
    if (ATEOS())
    {
      FAILW(REG_EESCAPE);
    }
    break;
  default:                         
    RETV(PLAIN, c);
    break;
  }

                                                           
  assert(!ATEOS());
  if (!(v->cflags & REG_ADVF))
  {                                         
    if (iscalnum(*v->now))
    {
      NOTE(REG_UBSALNUM);
      NOTE(REG_UUNSPEC);
    }
    RETV(PLAIN, *v->now++);
  }
  (DISCARD) lexescape(v);
  if (ISERR())
  {
    FAILW(REG_EESCAPE);
  }
  if (v->nexttype == CCLASS)
  {                             
    switch (v->nextvalue)
    {
    case 'd':
      lexnest(v, backd, ENDOF(backd));
      break;
    case 'D':
      lexnest(v, backD, ENDOF(backD));
      break;
    case 's':
      lexnest(v, backs, ENDOF(backs));
      break;
    case 'S':
      lexnest(v, backS, ENDOF(backS));
      break;
    case 'w':
      lexnest(v, backw, ENDOF(backw));
      break;
    case 'W':
      lexnest(v, backW, ENDOF(backW));
      break;
    default:
      assert(NOTREACHED);
      FAILW(REG_ASSERT);
      break;
    }
                                             
    v->nexttype = v->lasttype;
    return next(v);
  }
                                                      
  return !ISERR();
}

   
                                                                       
                                                          
   
static int                                                 
lexescape(struct vars *v)
{
  chr c;
  static const chr alert[] = {CHR('a'), CHR('l'), CHR('e'), CHR('r'), CHR('t')};
  static const chr esc[] = {CHR('E'), CHR('S'), CHR('C')};
  const chr *save;

  assert(v->cflags & REG_ADVF);

  assert(!ATEOS());
  c = *v->now++;
  if (!iscalnum(c))
  {
    RETV(PLAIN, c);
  }

  NOTE(REG_UNONPOSIX);
  switch (c)
  {
  case CHR('a'):
    RETV(PLAIN, chrnamed(v, alert, ENDOF(alert), CHR('\007')));
    break;
  case CHR('A'):
    RETV(SBEGIN, 0);
    break;
  case CHR('b'):
    RETV(PLAIN, CHR('\b'));
    break;
  case CHR('B'):
    RETV(PLAIN, CHR('\\'));
    break;
  case CHR('c'):
    NOTE(REG_UUNPORT);
    if (ATEOS())
    {
      FAILW(REG_EESCAPE);
    }
    RETV(PLAIN, (chr)(*v->now++ & 037));
    break;
  case CHR('d'):
    NOTE(REG_ULOCALE);
    RETV(CCLASS, 'd');
    break;
  case CHR('D'):
    NOTE(REG_ULOCALE);
    RETV(CCLASS, 'D');
    break;
  case CHR('e'):
    NOTE(REG_UUNPORT);
    RETV(PLAIN, chrnamed(v, esc, ENDOF(esc), CHR('\033')));
    break;
  case CHR('f'):
    RETV(PLAIN, CHR('\f'));
    break;
  case CHR('m'):
    RET('<');
    break;
  case CHR('M'):
    RET('>');
    break;
  case CHR('n'):
    RETV(PLAIN, CHR('\n'));
    break;
  case CHR('r'):
    RETV(PLAIN, CHR('\r'));
    break;
  case CHR('s'):
    NOTE(REG_ULOCALE);
    RETV(CCLASS, 's');
    break;
  case CHR('S'):
    NOTE(REG_ULOCALE);
    RETV(CCLASS, 'S');
    break;
  case CHR('t'):
    RETV(PLAIN, CHR('\t'));
    break;
  case CHR('u'):
    c = lexdigits(v, 16, 4, 4);
    if (ISERR() || !CHR_IS_IN_RANGE(c))
    {
      FAILW(REG_EESCAPE);
    }
    RETV(PLAIN, c);
    break;
  case CHR('U'):
    c = lexdigits(v, 16, 8, 8);
    if (ISERR() || !CHR_IS_IN_RANGE(c))
    {
      FAILW(REG_EESCAPE);
    }
    RETV(PLAIN, c);
    break;
  case CHR('v'):
    RETV(PLAIN, CHR('\v'));
    break;
  case CHR('w'):
    NOTE(REG_ULOCALE);
    RETV(CCLASS, 'w');
    break;
  case CHR('W'):
    NOTE(REG_ULOCALE);
    RETV(CCLASS, 'W');
    break;
  case CHR('x'):
    NOTE(REG_UUNPORT);
    c = lexdigits(v, 16, 1, 255);                                 
    if (ISERR() || !CHR_IS_IN_RANGE(c))
    {
      FAILW(REG_EESCAPE);
    }
    RETV(PLAIN, c);
    break;
  case CHR('y'):
    NOTE(REG_ULOCALE);
    RETV(WBDRY, 0);
    break;
  case CHR('Y'):
    NOTE(REG_ULOCALE);
    RETV(NWBDRY, 0);
    break;
  case CHR('Z'):
    RETV(SEND, 0);
    break;
  case CHR('1'):
  case CHR('2'):
  case CHR('3'):
  case CHR('4'):
  case CHR('5'):
  case CHR('6'):
  case CHR('7'):
  case CHR('8'):
  case CHR('9'):
    save = v->now;
    v->now--;                                               
    c = lexdigits(v, 10, 1, 255);                                 
    if (ISERR())
    {
      FAILW(REG_EESCAPE);
    }
                                                           
    if (v->now == save || ((int)c > 0 && (int)c <= v->nsubexp))
    {
      NOTE(REG_UBACKREF);
      RETV(BACKREF, c);
    }
                                                             
    v->now = save;
                                            
                     
  case CHR('0'):
    NOTE(REG_UUNPORT);
    v->now--;                           
    c = lexdigits(v, 8, 1, 3);
    if (ISERR())
    {
      FAILW(REG_EESCAPE);
    }
    if (c > 0xff)
    {
                                                          
      v->now--;
      c >>= 3;
    }
    RETV(PLAIN, c);
    break;
  default:
    assert(iscalpha(c));
    FAILW(REG_EESCAPE);                                
    break;
  }
  assert(NOTREACHED);
}

   
                                                    
   
                                                                             
                                                    
   
static chr                                          
lexdigits(struct vars *v, int base, int minlen, int maxlen)
{
  uchr n;                                             
  int len;
  chr c;
  int d;
  const uchr ub = (uchr)base;

  n = 0;
  for (len = 0; len < maxlen && !ATEOS(); len++)
  {
    c = *v->now++;
    switch (c)
    {
    case CHR('0'):
    case CHR('1'):
    case CHR('2'):
    case CHR('3'):
    case CHR('4'):
    case CHR('5'):
    case CHR('6'):
    case CHR('7'):
    case CHR('8'):
    case CHR('9'):
      d = DIGITVAL(c);
      break;
    case CHR('a'):
    case CHR('A'):
      d = 10;
      break;
    case CHR('b'):
    case CHR('B'):
      d = 11;
      break;
    case CHR('c'):
    case CHR('C'):
      d = 12;
      break;
    case CHR('d'):
    case CHR('D'):
      d = 13;
      break;
    case CHR('e'):
    case CHR('E'):
      d = 14;
      break;
    case CHR('f'):
    case CHR('F'):
      d = 15;
      break;
    default:
      v->now--;                               
      d = -1;
      break;
    }

    if (d >= base)
    {                            
      v->now--;
      d = -1;
    }
    if (d < 0)
    {
      break;                     
    }
    n = n * ub + (uchr)d;
  }
  if (len < minlen)
  {
    ERR(REG_EESCAPE);
  }

  return (chr)n;
}

   
                                
   
                                                                        
                                      
   
static int                          
brenext(struct vars *v, chr c)
{
  switch (c)
  {
  case CHR('*'):
    if (LASTTYPE(EMPTY) || LASTTYPE('(') || LASTTYPE('^'))
    {
      RETV(PLAIN, c);
    }
    RETV('*', 1);
    break;
  case CHR('['):
    if (HAVE(6) && *(v->now + 0) == CHR('[') && *(v->now + 1) == CHR(':') && (*(v->now + 2) == CHR('<') || *(v->now + 2) == CHR('>')) && *(v->now + 3) == CHR(':') && *(v->now + 4) == CHR(']') && *(v->now + 5) == CHR(']'))
    {
      c = *(v->now + 2);
      v->now += 6;
      NOTE(REG_UNONPOSIX);
      RET((c == CHR('<')) ? '<' : '>');
    }
    INTOCON(L_BRACK);
    if (NEXT1('^'))
    {
      v->now++;
      RETV('[', 0);
    }
    RETV('[', 1);
    break;
  case CHR('.'):
    RET('.');
    break;
  case CHR('^'):
    if (LASTTYPE(EMPTY))
    {
      RET('^');
    }
    if (LASTTYPE('('))
    {
      NOTE(REG_UUNSPEC);
      RET('^');
    }
    RETV(PLAIN, c);
    break;
  case CHR('$'):
    if (v->cflags & REG_EXPANDED)
    {
      skip(v);
    }
    if (ATEOS())
    {
      RET('$');
    }
    if (NEXT2('\\', ')'))
    {
      NOTE(REG_UUNSPEC);
      RET('$');
    }
    RETV(PLAIN, c);
    break;
  case CHR('\\'):
    break;                
  default:
    RETV(PLAIN, c);
    break;
  }

  assert(c == CHR('\\'));

  if (ATEOS())
  {
    FAILW(REG_EESCAPE);
  }

  c = *v->now++;
  switch (c)
  {
  case CHR('{'):
    INTOCON(L_BBND);
    NOTE(REG_UBOUNDS);
    RET('{');
    break;
  case CHR('('):
    RETV('(', 1);
    break;
  case CHR(')'):
    RETV(')', c);
    break;
  case CHR('<'):
    NOTE(REG_UNONPOSIX);
    RET('<');
    break;
  case CHR('>'):
    NOTE(REG_UNONPOSIX);
    RET('>');
    break;
  case CHR('1'):
  case CHR('2'):
  case CHR('3'):
  case CHR('4'):
  case CHR('5'):
  case CHR('6'):
  case CHR('7'):
  case CHR('8'):
  case CHR('9'):
    NOTE(REG_UBACKREF);
    RETV(BACKREF, (chr)DIGITVAL(c));
    break;
  default:
    if (iscalnum(c))
    {
      NOTE(REG_UBSALNUM);
      NOTE(REG_UUNSPEC);
    }
    RETV(PLAIN, c);
    break;
  }

  assert(NOTREACHED);
  return 0;
}

   
                                                         
   
static void
skip(struct vars *v)
{
  const chr *start = v->now;

  assert(v->cflags & REG_EXPANDED);

  for (;;)
  {
    while (!ATEOS() && iscspace(*v->now))
    {
      v->now++;
    }
    if (ATEOS() || *v->now != CHR('#'))
    {
      break;                     
    }
    assert(NEXT1('#'));
    while (!ATEOS() && *v->now != CHR('\n'))
    {
      v->now++;
    }
                                                                
  }

  if (v->now != start)
  {
    NOTE(REG_UNONPOSIX);
  }
}

   
                                          
   
                                                      
   
static chr
newline(void)
{
  return CHR('\n');
}

   
                                                                
   
                                                                         
                               
   
static chr
chrnamed(struct vars *v, const chr *startp,                    
    const chr *endp,                                                   
    chr lastresort)                                                                  
{
  chr c;
  int errsave;
  int e;
  struct cvec *cv;

  errsave = v->err;
  v->err = 0;
  c = element(v, startp, endp);
  e = v->err;
  v->err = errsave;

  if (e != 0)
  {
    return lastresort;
  }

  cv = range(v, c, c, 0);
  if (cv->nchrs == 0)
  {
    return lastresort;
  }
  return cv->chrs[0];
}
