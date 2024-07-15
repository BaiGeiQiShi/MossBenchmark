   
                    
   
                                                       
                                        
   
                                                
   
                                                                    
                                                                          
                                                                          
                                                                
                     
   
                                                                         
                                                                             
                                                                            
                                                                           
                                                                       
                                                                      
                                                                         
                                                                            
               
   
                                                                        
                                                                       
                                                                      
                                                                     
                               
   
                                                                      
                                                                             
                                                                          
                                                                          
                                                                            
                  
   
                                                                       
                                                                       
                                                                       
                                                                       
                                                                          
                                                                          
                                                                       
                                                                      
                                                                     
                                                                        
                                    
   
                                   
   

                                

static const struct cname
{
  const char *name;
  const char code;
} cnames[] =

    {{"NUL", '\0'}, {"SOH", '\001'}, {"STX", '\002'}, {"ETX", '\003'}, {"EOT", '\004'}, {"ENQ", '\005'}, {"ACK", '\006'}, {"BEL", '\007'}, {"alert", '\007'}, {"BS", '\010'}, {"backspace", '\b'}, {"HT", '\011'}, {"tab", '\t'}, {"LF", '\012'}, {"newline", '\n'}, {"VT", '\013'}, {"vertical-tab", '\v'}, {"FF", '\014'}, {"form-feed", '\f'}, {"CR", '\015'}, {"carriage-return", '\r'}, {"SO", '\016'}, {"SI", '\017'}, {"DLE", '\020'}, {"DC1", '\021'}, {"DC2", '\022'}, {"DC3", '\023'}, {"DC4", '\024'}, {"NAK", '\025'}, {"SYN", '\026'}, {"ETB", '\027'}, {"CAN", '\030'}, {"EM", '\031'}, {"SUB", '\032'}, {"ESC", '\033'}, {"IS4", '\034'}, {"FS", '\034'}, {"IS3", '\035'}, {"GS", '\035'}, {"IS2", '\036'}, {"RS", '\036'}, {"IS1", '\037'}, {"US", '\037'}, {"space", ' '}, {"exclamation-mark", '!'}, {"quotation-mark", '"'}, {"number-sign", '#'}, {"dollar-sign", '$'}, {"percent-sign", '%'}, {"ampersand", '&'}, {"apostrophe", '\''}, {"left-parenthesis", '('}, {"right-parenthesis", ')'}, {"asterisk", '*'},
        {"plus-sign", '+'}, {"comma", ','}, {"hyphen", '-'}, {"hyphen-minus", '-'}, {"period", '.'}, {"full-stop", '.'}, {"slash", '/'}, {"solidus", '/'}, {"zero", '0'}, {"one", '1'}, {"two", '2'}, {"three", '3'}, {"four", '4'}, {"five", '5'}, {"six", '6'}, {"seven", '7'}, {"eight", '8'}, {"nine", '9'}, {"colon", ':'}, {"semicolon", ';'}, {"less-than-sign", '<'}, {"equals-sign", '='}, {"greater-than-sign", '>'}, {"question-mark", '?'}, {"commercial-at", '@'}, {"left-square-bracket", '['}, {"backslash", '\\'}, {"reverse-solidus", '\\'}, {"right-square-bracket", ']'}, {"circumflex", '^'}, {"circumflex-accent", '^'}, {"underscore", '_'}, {"low-line", '_'}, {"grave-accent", '`'}, {"left-brace", '{'}, {"left-curly-bracket", '{'}, {"vertical-line", '|'}, {"right-brace", '}'}, {"right-curly-bracket", '}'}, {"tilde", '~'}, {"DEL", '\177'}, {NULL, 0}};

   
                                                                
   
static const char *const classNames[NUM_CCLASSES + 1] = {"alnum", "alpha", "ascii", "blank", "cntrl", "digit", "graph", "lower", "print", "punct", "space", "upper", "xdigit", NULL};

enum classes
{
  CC_ALNUM,
  CC_ALPHA,
  CC_ASCII,
  CC_BLANK,
  CC_CNTRL,
  CC_DIGIT,
  CC_GRAPH,
  CC_LOWER,
  CC_PRINT,
  CC_PUNCT,
  CC_SPACE,
  CC_UPPER,
  CC_XDIGIT
};

   
                                                                             
                                                                             
                                                                        
                                                                        
                                                                         
                                                                    
   

   
                                               
   
static chr
element(struct vars *v,              
    const chr *startp,                               
    const chr *endp)                                      
{
  const struct cname *cn;
  size_t len;

                                                    
  assert(startp < endp);
  len = endp - startp;
  if (len == 1)
  {
    return *startp;
  }

  NOTE(REG_ULOCALE);

                    
  for (cn = cnames; cn->name != NULL; cn++)
  {
    if (strlen(cn->name) == len && pg_char_and_wchar_strncmp(cn->name, startp, len) == 0)
    {
      break;                     
    }
  }
  if (cn->name != NULL)
  {
    return CHR(cn->code);
  }

                        
  ERR(REG_ECOLLATE);
  return 0;
}

   
                                                             
   
static struct cvec *
range(struct vars *v,              
    chr a,                             
    chr b,                                          
    int cases)                               
{
  int nchrs;
  struct cvec *cv;
  chr c, cc;

  if (a != b && !before(a, b))
  {
    ERR(REG_ERANGE);
    return NULL;
  }

  if (!cases)
  {                   
    cv = getcvec(v, 0, 1);
    NOERRN();
    addrange(cv, a, b);
    return cv;
  }

     
                                                                             
                                                                           
                                                                          
                                    
     
                                                                            
                                                                          
                            
     
  nchrs = b - a + 1;
  if (nchrs <= 0 || nchrs > 100000)
  {
    nchrs = 100000;
  }

  cv = getcvec(v, nchrs, 1);
  NOERRN();
  addrange(cv, a, b);

  for (c = a; c <= b; c++)
  {
    cc = pg_wc_tolower(c);
    if (cc != c && (before(cc, a) || before(b, cc)))
    {
      if (cv->nchrs >= cv->chrspace)
      {
        ERR(REG_ETOOBIG);
        return NULL;
      }
      addchr(cv, cc);
    }
    cc = pg_wc_toupper(c);
    if (cc != c && (before(cc, a) || before(b, cc)))
    {
      if (cv->nchrs >= cv->chrspace)
      {
        ERR(REG_ETOOBIG);
        return NULL;
      }
      addchr(cv, cc);
    }
    if (CANCEL_REQUESTED(v->re))
    {
      ERR(REG_CANCEL);
      return NULL;
    }
  }

  return cv;
}

   
                                                                   
   
static int                
before(chr x, chr y)
{
  if (x < y)
  {
    return 1;
  }
  return 0;
}

   
                                                 
                                              
   
static struct cvec *
eclass(struct vars *v,              
    chr c,                                                   
                                               
    int cases)                         
{
  struct cvec *cv;

                                                
  if ((v->cflags & REG_FAKE) && c == 'x')
  {
    cv = getcvec(v, 4, 0);
    addchr(cv, CHR('x'));
    addchr(cv, CHR('y'));
    if (cases)
    {
      addchr(cv, CHR('X'));
      addchr(cv, CHR('Y'));
    }
    return cv;
  }

                       
  if (cases)
  {
    return allcases(v, c);
  }
  cv = getcvec(v, 1, 0);
  assert(cv != NULL);
  addchr(cv, c);
  return cv;
}

   
                                              
   
                                                      
   
                                                                             
                                                                        
                                                                              
   
static struct cvec *
cclass(struct vars *v,              
    const chr *startp,                            
    const chr *endp,                                      
    int cases)                                
{
  size_t len;
  struct cvec *cv = NULL;
  const char *const *namePtr;
  int i, index;

     
                                                         
     
  len = endp - startp;
  index = -1;
  for (namePtr = classNames, i = 0; *namePtr != NULL; namePtr++, i++)
  {
    if (strlen(*namePtr) == len && pg_char_and_wchar_strncmp(*namePtr, startp, len) == 0)
    {
      index = i;
      break;
    }
  }
  if (index == -1)
  {
    ERR(REG_ECTYPE);
    return NULL;
  }

     
                                                                      
     

  if (cases && ((enum classes)index == CC_LOWER || (enum classes)index == CC_UPPER))
  {
    index = (int)CC_ALPHA;
  }

     
                                                                           
                                                                   
                                                                         
                                                                      
                                            
     
                                                                   
     

  switch ((enum classes)index)
  {
  case CC_PRINT:
    cv = pg_ctype_get_cache(pg_wc_isprint, index);
    break;
  case CC_ALNUM:
    cv = pg_ctype_get_cache(pg_wc_isalnum, index);
    break;
  case CC_ALPHA:
    cv = pg_ctype_get_cache(pg_wc_isalpha, index);
    break;
  case CC_ASCII:
                            
    cv = getcvec(v, 0, 1);
    if (cv)
    {
      addrange(cv, 0, 0x7f);
    }
    break;
  case CC_BLANK:
                            
    cv = getcvec(v, 2, 0);
    addchr(cv, '\t');
    addchr(cv, ' ');
    break;
  case CC_CNTRL:
                            
    cv = getcvec(v, 0, 2);
    addrange(cv, 0x0, 0x1f);
    addrange(cv, 0x7f, 0x9f);
    break;
  case CC_DIGIT:
    cv = pg_ctype_get_cache(pg_wc_isdigit, index);
    break;
  case CC_PUNCT:
    cv = pg_ctype_get_cache(pg_wc_ispunct, index);
    break;
  case CC_XDIGIT:

       
                                                                     
                                                                     
                                   
       
    cv = getcvec(v, 0, 3);
    if (cv)
    {
      addrange(cv, '0', '9');
      addrange(cv, 'a', 'f');
      addrange(cv, 'A', 'F');
    }
    break;
  case CC_SPACE:
    cv = pg_ctype_get_cache(pg_wc_isspace, index);
    break;
  case CC_LOWER:
    cv = pg_ctype_get_cache(pg_wc_islower, index);
    break;
  case CC_UPPER:
    cv = pg_ctype_get_cache(pg_wc_isupper, index);
    break;
  case CC_GRAPH:
    cv = pg_ctype_get_cache(pg_wc_isgraph, index);
    break;
  }

                                                             
  if (cv == NULL)
  {
    ERR(REG_ESPACE);
  }
  return cv;
}

   
                                                                            
   
static int
cclass_column_index(struct colormap *cm, chr c)
{
  int colnum = 0;

                                                              
  assert(c > MAX_SIMPLE_CHR);

     
                                                                        
                                                    
     
  if (cm->classbits[CC_PRINT] && pg_wc_isprint(c))
  {
    colnum |= cm->classbits[CC_PRINT];
  }
  if (cm->classbits[CC_ALNUM] && pg_wc_isalnum(c))
  {
    colnum |= cm->classbits[CC_ALNUM];
  }
  if (cm->classbits[CC_ALPHA] && pg_wc_isalpha(c))
  {
    colnum |= cm->classbits[CC_ALPHA];
  }
  assert(cm->classbits[CC_ASCII] == 0);
  assert(cm->classbits[CC_BLANK] == 0);
  assert(cm->classbits[CC_CNTRL] == 0);
  if (cm->classbits[CC_DIGIT] && pg_wc_isdigit(c))
  {
    colnum |= cm->classbits[CC_DIGIT];
  }
  if (cm->classbits[CC_PUNCT] && pg_wc_ispunct(c))
  {
    colnum |= cm->classbits[CC_PUNCT];
  }
  assert(cm->classbits[CC_XDIGIT] == 0);
  if (cm->classbits[CC_SPACE] && pg_wc_isspace(c))
  {
    colnum |= cm->classbits[CC_SPACE];
  }
  if (cm->classbits[CC_LOWER] && pg_wc_islower(c))
  {
    colnum |= cm->classbits[CC_LOWER];
  }
  if (cm->classbits[CC_UPPER] && pg_wc_isupper(c))
  {
    colnum |= cm->classbits[CC_UPPER];
  }
  if (cm->classbits[CC_GRAPH] && pg_wc_isgraph(c))
  {
    colnum |= cm->classbits[CC_GRAPH];
  }

  return colnum;
}

   
                                                                                
   
                                                                           
                                     
   
static struct cvec *
allcases(struct vars *v,              
    chr c)                                                    
{
  struct cvec *cv;
  chr lc, uc;

  lc = pg_wc_tolower(c);
  uc = pg_wc_toupper(c);

  cv = getcvec(v, 2, 0);
  addchr(cv, lc);
  if (lc != uc)
  {
    addchr(cv, uc);
  }
  return cv;
}

   
                               
   
                                                           
                                                                       
                                                                     
                          
   
static int                                                            
cmp(const chr *x, const chr *y,                         
    size_t len)                                                 
{
  return memcmp(VS(x), VS(y), len * sizeof(chr));
}

   
                                                    
   
                                                                     
                                                                       
                                                                     
                          
   
static int                                                                
casecmp(const chr *x, const chr *y,                         
    size_t len)                                                     
{
  for (; len > 0; len--, x++, y++)
  {
    if ((*x != *y) && (pg_wc_tolower(*x) != pg_wc_tolower(*y)))
    {
      return 1;
    }
  }
  return 0;
}
