   
                                                                
                
                                
   
   
                                               
#ifdef FRONTEND
#include "postgres_fe.h"
#else
#include "postgres.h"
#endif

#include "mb/pg_wchar.h"

   
                                                                      
              
   
                                                                           
                                                                              
                         
   
                                                                        
                                                                       
             
   
                                                                            
                                                                             
                                                                         
                                                                          
                                                                              
                                     
   
                                                                            
                                                                               
                                                                        
                                                                          
                                                       
   

   
             
   
static int
pg_ascii2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
  int cnt = 0;

  while (len > 0 && *from)
  {
    *to++ = *from++;
    len--;
    cnt++;
  }
  *to = 0;
  return cnt;
}

static int
pg_ascii_mblen(const unsigned char *s)
{
  return 1;
}

static int
pg_ascii_dsplen(const unsigned char *s)
{
  if (*s == '\0')
  {
    return 0;
  }
  if (*s < 0x20 || *s == 0x7f)
  {
    return -1;
  }

  return 1;
}

   
       
   
static int
pg_euc2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
  int cnt = 0;

  while (len > 0 && *from)
  {
    if (*from == SS2 && len >= 2)                                  
                                              
    {
      from++;
      *to = (SS2 << 8) | *from++;
      len -= 2;
    }
    else if (*from == SS3 && len >= 3)                       
    {
      from++;
      *to = (SS3 << 16) | (*from++ << 8);
      *to |= *from++;
      len -= 3;
    }
    else if (IS_HIGHBIT_SET(*from) && len >= 2)                       
    {
      *to = *from++ << 8;
      *to |= *from++;
      len -= 2;
    }
    else                    
    {
      *to = *from++;
      len--;
    }
    to++;
    cnt++;
  }
  *to = 0;
  return cnt;
}

static inline int
pg_euc_mblen(const unsigned char *s)
{
  int len;

  if (*s == SS2)
  {
    len = 2;
  }
  else if (*s == SS3)
  {
    len = 3;
  }
  else if (IS_HIGHBIT_SET(*s))
  {
    len = 2;
  }
  else
  {
    len = 1;
  }
  return len;
}

static inline int
pg_euc_dsplen(const unsigned char *s)
{
  int len;

  if (*s == SS2)
  {
    len = 2;
  }
  else if (*s == SS3)
  {
    len = 2;
  }
  else if (IS_HIGHBIT_SET(*s))
  {
    len = 2;
  }
  else
  {
    len = pg_ascii_dsplen(s);
  }
  return len;
}

   
          
   
static int
pg_eucjp2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
  return pg_euc2wchar_with_len(from, to, len);
}

static int
pg_eucjp_mblen(const unsigned char *s)
{
  return pg_euc_mblen(s);
}

static int
pg_eucjp_dsplen(const unsigned char *s)
{
  int len;

  if (*s == SS2)
  {
    len = 1;
  }
  else if (*s == SS3)
  {
    len = 2;
  }
  else if (IS_HIGHBIT_SET(*s))
  {
    len = 2;
  }
  else
  {
    len = pg_ascii_dsplen(s);
  }
  return len;
}

   
          
   
static int
pg_euckr2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
  return pg_euc2wchar_with_len(from, to, len);
}

static int
pg_euckr_mblen(const unsigned char *s)
{
  return pg_euc_mblen(s);
}

static int
pg_euckr_dsplen(const unsigned char *s)
{
  return pg_euc_dsplen(s);
}

   
          
   
   
static int
pg_euccn2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
  int cnt = 0;

  while (len > 0 && *from)
  {
    if (*from == SS2 && len >= 3)                           
    {
      from++;
      *to = (SS2 << 16) | (*from++ << 8);
      *to |= *from++;
      len -= 3;
    }
    else if (*from == SS3 && len >= 3)                            
    {
      from++;
      *to = (SS3 << 16) | (*from++ << 8);
      *to |= *from++;
      len -= 3;
    }
    else if (IS_HIGHBIT_SET(*from) && len >= 2)                 
    {
      *to = *from++ << 8;
      *to |= *from++;
      len -= 2;
    }
    else
    {
      *to = *from++;
      len--;
    }
    to++;
    cnt++;
  }
  *to = 0;
  return cnt;
}

static int
pg_euccn_mblen(const unsigned char *s)
{
  int len;

  if (IS_HIGHBIT_SET(*s))
  {
    len = 2;
  }
  else
  {
    len = 1;
  }
  return len;
}

static int
pg_euccn_dsplen(const unsigned char *s)
{
  int len;

  if (IS_HIGHBIT_SET(*s))
  {
    len = 2;
  }
  else
  {
    len = pg_ascii_dsplen(s);
  }
  return len;
}

   
          
   
   
static int
pg_euctw2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
  int cnt = 0;

  while (len > 0 && *from)
  {
    if (*from == SS2 && len >= 4)                 
    {
      from++;
      *to = (((uint32)SS2) << 24) | (*from++ << 16);
      *to |= *from++ << 8;
      *to |= *from++;
      len -= 4;
    }
    else if (*from == SS3 && len >= 3)                           
    {
      from++;
      *to = (SS3 << 16) | (*from++ << 8);
      *to |= *from++;
      len -= 3;
    }
    else if (IS_HIGHBIT_SET(*from) && len >= 2)                 
    {
      *to = *from++ << 8;
      *to |= *from++;
      len -= 2;
    }
    else
    {
      *to = *from++;
      len--;
    }
    to++;
    cnt++;
  }
  *to = 0;
  return cnt;
}

static int
pg_euctw_mblen(const unsigned char *s)
{
  int len;

  if (*s == SS2)
  {
    len = 4;
  }
  else if (*s == SS3)
  {
    len = 3;
  }
  else if (IS_HIGHBIT_SET(*s))
  {
    len = 2;
  }
  else
  {
    len = 1;
  }
  return len;
}

static int
pg_euctw_dsplen(const unsigned char *s)
{
  int len;

  if (*s == SS2)
  {
    len = 2;
  }
  else if (*s == SS3)
  {
    len = 2;
  }
  else if (IS_HIGHBIT_SET(*s))
  {
    len = 2;
  }
  else
  {
    len = pg_ascii_dsplen(s);
  }
  return len;
}

   
                                       
                                                                          
                        
                                           
   
static int
pg_wchar2euc_with_len(const pg_wchar *from, unsigned char *to, int len)
{
  int cnt = 0;

  while (len > 0 && *from)
  {
    unsigned char c;

    if ((c = (*from >> 24)))
    {
      *to++ = c;
      *to++ = (*from >> 16) & 0xff;
      *to++ = (*from >> 8) & 0xff;
      *to++ = *from & 0xff;
      cnt += 4;
    }
    else if ((c = (*from >> 16)))
    {
      *to++ = c;
      *to++ = (*from >> 8) & 0xff;
      *to++ = *from & 0xff;
      cnt += 3;
    }
    else if ((c = (*from >> 8)))
    {
      *to++ = c;
      *to++ = *from & 0xff;
      cnt += 2;
    }
    else
    {
      *to++ = *from;
      cnt++;
    }
    from++;
    len--;
  }
  *to = 0;
  return cnt;
}

   
         
   
static int
pg_johab_mblen(const unsigned char *s)
{
  return pg_euc_mblen(s);
}

static int
pg_johab_dsplen(const unsigned char *s)
{
  return pg_euc_dsplen(s);
}

   
                                           
                                                                          
                        
                                           
   
static int
pg_utf2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
  int cnt = 0;
  uint32 c1, c2, c3, c4;

  while (len > 0 && *from)
  {
    if ((*from & 0x80) == 0)
    {
      *to = *from++;
      len--;
    }
    else if ((*from & 0xe0) == 0xc0)
    {
      if (len < 2)
      {
        break;                                    
      }
      c1 = *from++ & 0x1f;
      c2 = *from++ & 0x3f;
      *to = (c1 << 6) | c2;
      len -= 2;
    }
    else if ((*from & 0xf0) == 0xe0)
    {
      if (len < 3)
      {
        break;                                    
      }
      c1 = *from++ & 0x0f;
      c2 = *from++ & 0x3f;
      c3 = *from++ & 0x3f;
      *to = (c1 << 12) | (c2 << 6) | c3;
      len -= 3;
    }
    else if ((*from & 0xf8) == 0xf0)
    {
      if (len < 4)
      {
        break;                                    
      }
      c1 = *from++ & 0x07;
      c2 = *from++ & 0x3f;
      c3 = *from++ & 0x3f;
      c4 = *from++ & 0x3f;
      *to = (c1 << 18) | (c2 << 12) | (c3 << 6) | c4;
      len -= 4;
    }
    else
    {
                                                                   
      *to = *from++;
      len--;
    }
    to++;
    cnt++;
  }
  *to = 0;
  return cnt;
}

   
                                                                       
                    
   
unsigned char *
unicode_to_utf8(pg_wchar c, unsigned char *utf8string)
{
  if (c <= 0x7F)
  {
    utf8string[0] = c;
  }
  else if (c <= 0x7FF)
  {
    utf8string[0] = 0xC0 | ((c >> 6) & 0x1F);
    utf8string[1] = 0x80 | (c & 0x3F);
  }
  else if (c <= 0xFFFF)
  {
    utf8string[0] = 0xE0 | ((c >> 12) & 0x0F);
    utf8string[1] = 0x80 | ((c >> 6) & 0x3F);
    utf8string[2] = 0x80 | (c & 0x3F);
  }
  else
  {
    utf8string[0] = 0xF0 | ((c >> 18) & 0x07);
    utf8string[1] = 0x80 | ((c >> 12) & 0x3F);
    utf8string[2] = 0x80 | ((c >> 6) & 0x3F);
    utf8string[3] = 0x80 | (c & 0x3F);
  }

  return utf8string;
}

   
                                              
                                                
                        
                                           
   
static int
pg_wchar2utf_with_len(const pg_wchar *from, unsigned char *to, int len)
{
  int cnt = 0;

  while (len > 0 && *from)
  {
    int char_len;

    unicode_to_utf8(*from, to);
    char_len = pg_utf_mblen(to);
    cnt += char_len;
    to += char_len;
    from++;
    len--;
  }
  *to = 0;
  return cnt;
}

   
                                                              
   
                                                                        
                                                                    
                                                                         
                                              
   
                                                                              
                                                       
   
int
pg_utf_mblen(const unsigned char *s)
{
  int len;

  if ((*s & 0x80) == 0)
  {
    len = 1;
  }
  else if ((*s & 0xe0) == 0xc0)
  {
    len = 2;
  }
  else if ((*s & 0xf0) == 0xe0)
  {
    len = 3;
  }
  else if ((*s & 0xf8) == 0xf0)
  {
    len = 4;
  }
#ifdef NOT_USED
  else if ((*s & 0xfc) == 0xf8)
  {
    len = 5;
  }
  else if ((*s & 0xfe) == 0xfc)
  {
    len = 6;
  }
#endif
  else
  {
    len = 1;
  }
  return len;
}

   
                                                                       
                                                                    
                                             
   
                                              
   
                             
   
                                                                        
   

struct mbinterval
{
  unsigned short first;
  unsigned short last;
};

                                                            
static int
mbbisearch(pg_wchar ucs, const struct mbinterval *table, int max)
{
  int min = 0;
  int mid;

  if (ucs < table[0].first || ucs > table[max].last)
  {
    return 0;
  }
  while (max >= min)
  {
    mid = (min + max) / 2;
    if (ucs > table[mid].last)
    {
      min = mid + 1;
    }
    else if (ucs < table[mid].first)
    {
      max = mid - 1;
    }
    else
    {
      return 1;
    }
  }

  return 0;
}

                                                                   
                         
   
                                                            
   
                                                                    
                 
   
                                                               
                                                           
                       
   
                                                                        
                                                                      
   
                                                                      
                              
   
                                                                   
                                                           
                                         
   
                                                         
                                                                
                                    
   
                                                                   
                 
   

static int
ucs_wcwidth(pg_wchar ucs)
{
                                                                          
  static const struct mbinterval combining[] = {
      {0x0300, 0x036F},
      {0x0483, 0x0489},
      {0x0591, 0x05BD},
      {0x05BF, 0x05BF},
      {0x05C1, 0x05C2},
      {0x05C4, 0x05C5},
      {0x05C7, 0x05C7},
      {0x0610, 0x061A},
      {0x064B, 0x065F},
      {0x0670, 0x0670},
      {0x06D6, 0x06DC},
      {0x06DF, 0x06E4},
      {0x06E7, 0x06E8},
      {0x06EA, 0x06ED},
      {0x0711, 0x0711},
      {0x0730, 0x074A},
      {0x07A6, 0x07B0},
      {0x07EB, 0x07F3},
      {0x07FD, 0x07FD},
      {0x0816, 0x0819},
      {0x081B, 0x0823},
      {0x0825, 0x0827},
      {0x0829, 0x082D},
      {0x0859, 0x085B},
      {0x08D3, 0x08E1},
      {0x08E3, 0x0902},
      {0x093A, 0x093A},
      {0x093C, 0x093C},
      {0x0941, 0x0948},
      {0x094D, 0x094D},
      {0x0951, 0x0957},
      {0x0962, 0x0963},
      {0x0981, 0x0981},
      {0x09BC, 0x09BC},
      {0x09C1, 0x09C4},
      {0x09CD, 0x09CD},
      {0x09E2, 0x09E3},
      {0x09FE, 0x0A02},
      {0x0A3C, 0x0A3C},
      {0x0A41, 0x0A51},
      {0x0A70, 0x0A71},
      {0x0A75, 0x0A75},
      {0x0A81, 0x0A82},
      {0x0ABC, 0x0ABC},
      {0x0AC1, 0x0AC8},
      {0x0ACD, 0x0ACD},
      {0x0AE2, 0x0AE3},
      {0x0AFA, 0x0B01},
      {0x0B3C, 0x0B3C},
      {0x0B3F, 0x0B3F},
      {0x0B41, 0x0B44},
      {0x0B4D, 0x0B56},
      {0x0B62, 0x0B63},
      {0x0B82, 0x0B82},
      {0x0BC0, 0x0BC0},
      {0x0BCD, 0x0BCD},
      {0x0C00, 0x0C00},
      {0x0C04, 0x0C04},
      {0x0C3E, 0x0C40},
      {0x0C46, 0x0C56},
      {0x0C62, 0x0C63},
      {0x0C81, 0x0C81},
      {0x0CBC, 0x0CBC},
      {0x0CBF, 0x0CBF},
      {0x0CC6, 0x0CC6},
      {0x0CCC, 0x0CCD},
      {0x0CE2, 0x0CE3},
      {0x0D00, 0x0D01},
      {0x0D3B, 0x0D3C},
      {0x0D41, 0x0D44},
      {0x0D4D, 0x0D4D},
      {0x0D62, 0x0D63},
      {0x0DCA, 0x0DCA},
      {0x0DD2, 0x0DD6},
      {0x0E31, 0x0E31},
      {0x0E34, 0x0E3A},
      {0x0E47, 0x0E4E},
      {0x0EB1, 0x0EB1},
      {0x0EB4, 0x0EBC},
      {0x0EC8, 0x0ECD},
      {0x0F18, 0x0F19},
      {0x0F35, 0x0F35},
      {0x0F37, 0x0F37},
      {0x0F39, 0x0F39},
      {0x0F71, 0x0F7E},
      {0x0F80, 0x0F84},
      {0x0F86, 0x0F87},
      {0x0F8D, 0x0FBC},
      {0x0FC6, 0x0FC6},
      {0x102D, 0x1030},
      {0x1032, 0x1037},
      {0x1039, 0x103A},
      {0x103D, 0x103E},
      {0x1058, 0x1059},
      {0x105E, 0x1060},
      {0x1071, 0x1074},
      {0x1082, 0x1082},
      {0x1085, 0x1086},
      {0x108D, 0x108D},
      {0x109D, 0x109D},
      {0x135D, 0x135F},
      {0x1712, 0x1714},
      {0x1732, 0x1734},
      {0x1752, 0x1753},
      {0x1772, 0x1773},
      {0x17B4, 0x17B5},
      {0x17B7, 0x17BD},
      {0x17C6, 0x17C6},
      {0x17C9, 0x17D3},
      {0x17DD, 0x17DD},
      {0x180B, 0x180D},
      {0x1885, 0x1886},
      {0x18A9, 0x18A9},
      {0x1920, 0x1922},
      {0x1927, 0x1928},
      {0x1932, 0x1932},
      {0x1939, 0x193B},
      {0x1A17, 0x1A18},
      {0x1A1B, 0x1A1B},
      {0x1A56, 0x1A56},
      {0x1A58, 0x1A60},
      {0x1A62, 0x1A62},
      {0x1A65, 0x1A6C},
      {0x1A73, 0x1A7F},
      {0x1AB0, 0x1B03},
      {0x1B34, 0x1B34},
      {0x1B36, 0x1B3A},
      {0x1B3C, 0x1B3C},
      {0x1B42, 0x1B42},
      {0x1B6B, 0x1B73},
      {0x1B80, 0x1B81},
      {0x1BA2, 0x1BA5},
      {0x1BA8, 0x1BA9},
      {0x1BAB, 0x1BAD},
      {0x1BE6, 0x1BE6},
      {0x1BE8, 0x1BE9},
      {0x1BED, 0x1BED},
      {0x1BEF, 0x1BF1},
      {0x1C2C, 0x1C33},
      {0x1C36, 0x1C37},
      {0x1CD0, 0x1CD2},
      {0x1CD4, 0x1CE0},
      {0x1CE2, 0x1CE8},
      {0x1CED, 0x1CED},
      {0x1CF4, 0x1CF4},
      {0x1CF8, 0x1CF9},
      {0x1DC0, 0x1DFF},
      {0x20D0, 0x20F0},
      {0x2CEF, 0x2CF1},
      {0x2D7F, 0x2D7F},
      {0x2DE0, 0x2DFF},
      {0x302A, 0x302D},
      {0x3099, 0x309A},
      {0xA66F, 0xA672},
      {0xA674, 0xA67D},
      {0xA69E, 0xA69F},
      {0xA6F0, 0xA6F1},
      {0xA802, 0xA802},
      {0xA806, 0xA806},
      {0xA80B, 0xA80B},
      {0xA825, 0xA826},
      {0xA8C4, 0xA8C5},
      {0xA8E0, 0xA8F1},
      {0xA8FF, 0xA8FF},
      {0xA926, 0xA92D},
      {0xA947, 0xA951},
      {0xA980, 0xA982},
      {0xA9B3, 0xA9B3},
      {0xA9B6, 0xA9B9},
      {0xA9BC, 0xA9BD},
      {0xA9E5, 0xA9E5},
      {0xAA29, 0xAA2E},
      {0xAA31, 0xAA32},
      {0xAA35, 0xAA36},
      {0xAA43, 0xAA43},
      {0xAA4C, 0xAA4C},
      {0xAA7C, 0xAA7C},
      {0xAAB0, 0xAAB0},
      {0xAAB2, 0xAAB4},
      {0xAAB7, 0xAAB8},
      {0xAABE, 0xAABF},
      {0xAAC1, 0xAAC1},
      {0xAAEC, 0xAAED},
      {0xAAF6, 0xAAF6},
      {0xABE5, 0xABE5},
      {0xABE8, 0xABE8},
      {0xABED, 0xABED},
      {0xFB1E, 0xFB1E},
      {0xFE00, 0xFE0F},
      {0xFE20, 0xFE2F},
  };

                                         
  if (ucs == 0)
  {
    return 0;
  }

  if (ucs < 0x20 || (ucs >= 0x7f && ucs < 0xa0) || ucs > 0x0010ffff)
  {
    return -1;
  }

                                                        
  if (mbbisearch(ucs, combining, sizeof(combining) / sizeof(struct mbinterval) - 1))
  {
    return 0;
  }

     
                                                                          
     

  return 1 + (ucs >= 0x1100 && (ucs <= 0x115f ||                                                                                                       
                                   (ucs >= 0x2e80 && ucs <= 0xa4cf && (ucs & ~0x0011) != 0x300a && ucs != 0x303f) ||                 
                                   (ucs >= 0xac00 && ucs <= 0xd7a3) ||                                                                     
                                   (ucs >= 0xf900 && ucs <= 0xfaff) ||                                                                    
                                                                                                                                     
                                   (ucs >= 0xfe30 && ucs <= 0xfe6f) ||                                                                            
                                   (ucs >= 0xff00 && ucs <= 0xff5f) ||                                                                    
                                   (ucs >= 0xffe0 && ucs <= 0xffe6) || (ucs >= 0x20000 && ucs <= 0x2ffff)));
}

   
                                                      
                                                             
   
                                                               
   
pg_wchar
utf8_to_unicode(const unsigned char *c)
{
  if ((*c & 0x80) == 0)
  {
    return (pg_wchar)c[0];
  }
  else if ((*c & 0xe0) == 0xc0)
  {
    return (pg_wchar)(((c[0] & 0x1f) << 6) | (c[1] & 0x3f));
  }
  else if ((*c & 0xf0) == 0xe0)
  {
    return (pg_wchar)(((c[0] & 0x0f) << 12) | ((c[1] & 0x3f) << 6) | (c[2] & 0x3f));
  }
  else if ((*c & 0xf8) == 0xf0)
  {
    return (pg_wchar)(((c[0] & 0x07) << 18) | ((c[1] & 0x3f) << 12) | ((c[2] & 0x3f) << 6) | (c[3] & 0x3f));
  }
  else
  {
                                            
    return 0xffffffff;
  }
}

static int
pg_utf_dsplen(const unsigned char *s)
{
  return ucs_wcwidth(utf8_to_unicode(s));
}

   
                                          
                                                
                        
                                           
   
static int
pg_mule2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
  int cnt = 0;

  while (len > 0 && *from)
  {
    if (IS_LC1(*from) && len >= 2)
    {
      *to = *from++ << 16;
      *to |= *from++;
      len -= 2;
    }
    else if (IS_LCPRV1(*from) && len >= 3)
    {
      from++;
      *to = *from++ << 16;
      *to |= *from++;
      len -= 3;
    }
    else if (IS_LC2(*from) && len >= 3)
    {
      *to = *from++ << 16;
      *to |= *from++ << 8;
      *to |= *from++;
      len -= 3;
    }
    else if (IS_LCPRV2(*from) && len >= 4)
    {
      from++;
      *to = *from++ << 16;
      *to |= *from++ << 8;
      *to |= *from++;
      len -= 4;
    }
    else
    {                   
      *to = (unsigned char)*from++;
      len--;
    }
    to++;
    cnt++;
  }
  *to = 0;
  return cnt;
}

   
                                          
                                                
                        
                                           
   
static int
pg_wchar2mule_with_len(const pg_wchar *from, unsigned char *to, int len)
{
  int cnt = 0;

  while (len > 0 && *from)
  {
    unsigned char lb;

    lb = (*from >> 16) & 0xff;
    if (IS_LC1(lb))
    {
      *to++ = lb;
      *to++ = *from & 0xff;
      cnt += 2;
    }
    else if (IS_LC2(lb))
    {
      *to++ = lb;
      *to++ = (*from >> 8) & 0xff;
      *to++ = *from & 0xff;
      cnt += 3;
    }
    else if (IS_LCPRV1_A_RANGE(lb))
    {
      *to++ = LCPRV1_A;
      *to++ = lb;
      *to++ = *from & 0xff;
      cnt += 3;
    }
    else if (IS_LCPRV1_B_RANGE(lb))
    {
      *to++ = LCPRV1_B;
      *to++ = lb;
      *to++ = *from & 0xff;
      cnt += 3;
    }
    else if (IS_LCPRV2_A_RANGE(lb))
    {
      *to++ = LCPRV2_A;
      *to++ = lb;
      *to++ = (*from >> 8) & 0xff;
      *to++ = *from & 0xff;
      cnt += 4;
    }
    else if (IS_LCPRV2_B_RANGE(lb))
    {
      *to++ = LCPRV2_B;
      *to++ = lb;
      *to++ = (*from >> 8) & 0xff;
      *to++ = *from & 0xff;
      cnt += 4;
    }
    else
    {
      *to++ = *from & 0xff;
      cnt += 1;
    }
    from++;
    len--;
  }
  *to = 0;
  return cnt;
}

int
pg_mule_mblen(const unsigned char *s)
{
  int len;

  if (IS_LC1(*s))
  {
    len = 2;
  }
  else if (IS_LCPRV1(*s))
  {
    len = 3;
  }
  else if (IS_LC2(*s))
  {
    len = 3;
  }
  else if (IS_LCPRV2(*s))
  {
    len = 4;
  }
  else
  {
    len = 1;                   
  }
  return len;
}

static int
pg_mule_dsplen(const unsigned char *s)
{
  int len;

     
                                                                             
                                                                          
                                             
     

  if (IS_LC1(*s))
  {
    len = 1;
  }
  else if (IS_LCPRV1(*s))
  {
    len = 1;
  }
  else if (IS_LC2(*s))
  {
    len = 2;
  }
  else if (IS_LCPRV2(*s))
  {
    len = 2;
  }
  else
  {
    len = 1;                   
  }

  return len;
}

   
             
   
static int
pg_latin12wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{
  int cnt = 0;

  while (len > 0 && *from)
  {
    *to++ = *from++;
    len--;
    cnt++;
  }
  *to = 0;
  return cnt;
}

   
                                                                          
              
                                                
                        
                                           
   
static int
pg_wchar2single_with_len(const pg_wchar *from, unsigned char *to, int len)
{
  int cnt = 0;

  while (len > 0 && *from)
  {
    *to++ = *from++;
    len--;
    cnt++;
  }
  *to = 0;
  return cnt;
}

static int
pg_latin1_mblen(const unsigned char *s)
{
  return 1;
}

static int
pg_latin1_dsplen(const unsigned char *s)
{
  return pg_ascii_dsplen(s);
}

   
        
   
static int
pg_sjis_mblen(const unsigned char *s)
{
  int len;

  if (*s >= 0xa1 && *s <= 0xdf)
  {
    len = 1;                   
  }
  else if (IS_HIGHBIT_SET(*s))
  {
    len = 2;             
  }
  else
  {
    len = 1;                      
  }
  return len;
}

static int
pg_sjis_dsplen(const unsigned char *s)
{
  int len;

  if (*s >= 0xa1 && *s <= 0xdf)
  {
    len = 1;                   
  }
  else if (IS_HIGHBIT_SET(*s))
  {
    len = 2;             
  }
  else
  {
    len = pg_ascii_dsplen(s);                      
  }
  return len;
}

   
        
   
static int
pg_big5_mblen(const unsigned char *s)
{
  int len;

  if (IS_HIGHBIT_SET(*s))
  {
    len = 2;             
  }
  else
  {
    len = 1;                      
  }
  return len;
}

static int
pg_big5_dsplen(const unsigned char *s)
{
  int len;

  if (IS_HIGHBIT_SET(*s))
  {
    len = 2;             
  }
  else
  {
    len = pg_ascii_dsplen(s);                      
  }
  return len;
}

   
       
   
static int
pg_gbk_mblen(const unsigned char *s)
{
  int len;

  if (IS_HIGHBIT_SET(*s))
  {
    len = 2;             
  }
  else
  {
    len = 1;                      
  }
  return len;
}

static int
pg_gbk_dsplen(const unsigned char *s)
{
  int len;

  if (IS_HIGHBIT_SET(*s))
  {
    len = 2;             
  }
  else
  {
    len = pg_ascii_dsplen(s);                      
  }
  return len;
}

   
       
   
static int
pg_uhc_mblen(const unsigned char *s)
{
  int len;

  if (IS_HIGHBIT_SET(*s))
  {
    len = 2;             
  }
  else
  {
    len = 1;                      
  }
  return len;
}

static int
pg_uhc_dsplen(const unsigned char *s)
{
  int len;

  if (IS_HIGHBIT_SET(*s))
  {
    len = 2;             
  }
  else
  {
    len = pg_ascii_dsplen(s);                      
  }
  return len;
}

   
           
                                                                    
   

   
                                                                             
                                                                        
                                                                             
                                                                         
                                                                          
                                                                             
                                                                    
               
   
static int
pg_gb18030_mblen(const unsigned char *s)
{
  int len;

  if (!IS_HIGHBIT_SET(*s))
  {
    len = 1;            
  }
  else if (*(s + 1) >= 0x30 && *(s + 1) <= 0x39)
  {
    len = 4;
  }
  else
  {
    len = 2;
  }
  return len;
}

static int
pg_gb18030_dsplen(const unsigned char *s)
{
  int len;

  if (IS_HIGHBIT_SET(*s))
  {
    len = 2;
  }
  else
  {
    len = pg_ascii_dsplen(s);            
  }
  return len;
}

   
                                                                      
                                 
   
                                                                        
                                                                         
                                                                       
              
   
                                                                            
                                                                                
   
                                                                   
                                   
                                                                      
   

static int
pg_ascii_verifier(const unsigned char *s, int len)
{
  return 1;
}

#define IS_EUC_RANGE_VALID(c) ((c) >= 0xa1 && (c) <= 0xfe)

static int
pg_eucjp_verifier(const unsigned char *s, int len)
{
  int l;
  unsigned char c1, c2;

  c1 = *s++;

  switch (c1)
  {
  case SS2:                 
    l = 2;
    if (l > len)
    {
      return -1;
    }
    c2 = *s++;
    if (c2 < 0xa1 || c2 > 0xdf)
    {
      return -1;
    }
    break;

  case SS3:                 
    l = 3;
    if (l > len)
    {
      return -1;
    }
    c2 = *s++;
    if (!IS_EUC_RANGE_VALID(c2))
    {
      return -1;
    }
    c2 = *s++;
    if (!IS_EUC_RANGE_VALID(c2))
    {
      return -1;
    }
    break;

  default:
    if (IS_HIGHBIT_SET(c1))                  
    {
      l = 2;
      if (l > len)
      {
        return -1;
      }
      if (!IS_EUC_RANGE_VALID(c1))
      {
        return -1;
      }
      c2 = *s++;
      if (!IS_EUC_RANGE_VALID(c2))
      {
        return -1;
      }
    }
    else
                       
    {
      l = 1;
    }
    break;
  }

  return l;
}

static int
pg_euckr_verifier(const unsigned char *s, int len)
{
  int l;
  unsigned char c1, c2;

  c1 = *s++;

  if (IS_HIGHBIT_SET(c1))
  {
    l = 2;
    if (l > len)
    {
      return -1;
    }
    if (!IS_EUC_RANGE_VALID(c1))
    {
      return -1;
    }
    c2 = *s++;
    if (!IS_EUC_RANGE_VALID(c2))
    {
      return -1;
    }
  }
  else
                     
  {
    l = 1;
  }

  return l;
}

                                                      
#define pg_euccn_verifier pg_euckr_verifier

static int
pg_euctw_verifier(const unsigned char *s, int len)
{
  int l;
  unsigned char c1, c2;

  c1 = *s++;

  switch (c1)
  {
  case SS2:                          
    l = 4;
    if (l > len)
    {
      return -1;
    }
    c2 = *s++;
    if (c2 < 0xa1 || c2 > 0xa7)
    {
      return -1;
    }
    c2 = *s++;
    if (!IS_EUC_RANGE_VALID(c2))
    {
      return -1;
    }
    c2 = *s++;
    if (!IS_EUC_RANGE_VALID(c2))
    {
      return -1;
    }
    break;

  case SS3:             
    return -1;

  default:
    if (IS_HIGHBIT_SET(c1))                        
    {
      l = 2;
      if (l > len)
      {
        return -1;
      }
                                         
      c2 = *s++;
      if (!IS_EUC_RANGE_VALID(c2))
      {
        return -1;
      }
    }
    else
                       
    {
      l = 1;
    }
    break;
  }
  return l;
}

static int
pg_johab_verifier(const unsigned char *s, int len)
{
  int l, mbl;
  unsigned char c;

  l = mbl = pg_johab_mblen(s);

  if (len < l)
  {
    return -1;
  }

  if (!IS_HIGHBIT_SET(*s))
  {
    return mbl;
  }

  while (--l > 0)
  {
    c = *++s;
    if (!IS_EUC_RANGE_VALID(c))
    {
      return -1;
    }
  }
  return mbl;
}

static int
pg_mule_verifier(const unsigned char *s, int len)
{
  int l, mbl;
  unsigned char c;

  l = mbl = pg_mule_mblen(s);

  if (len < l)
  {
    return -1;
  }

  while (--l > 0)
  {
    c = *++s;
    if (!IS_HIGHBIT_SET(c))
    {
      return -1;
    }
  }
  return mbl;
}

static int
pg_latin1_verifier(const unsigned char *s, int len)
{
  return 1;
}

static int
pg_sjis_verifier(const unsigned char *s, int len)
{
  int l, mbl;
  unsigned char c1, c2;

  l = mbl = pg_sjis_mblen(s);

  if (len < l)
  {
    return -1;
  }

  if (l == 1)                                        
  {
    return mbl;
  }

  c1 = *s++;
  c2 = *s;
  if (!ISSJISHEAD(c1) || !ISSJISTAIL(c2))
  {
    return -1;
  }
  return mbl;
}

static int
pg_big5_verifier(const unsigned char *s, int len)
{
  int l, mbl;

  l = mbl = pg_big5_mblen(s);

  if (len < l)
  {
    return -1;
  }

  while (--l > 0)
  {
    if (*++s == '\0')
    {
      return -1;
    }
  }

  return mbl;
}

static int
pg_gbk_verifier(const unsigned char *s, int len)
{
  int l, mbl;

  l = mbl = pg_gbk_mblen(s);

  if (len < l)
  {
    return -1;
  }

  while (--l > 0)
  {
    if (*++s == '\0')
    {
      return -1;
    }
  }

  return mbl;
}

static int
pg_uhc_verifier(const unsigned char *s, int len)
{
  int l, mbl;

  l = mbl = pg_uhc_mblen(s);

  if (len < l)
  {
    return -1;
  }

  while (--l > 0)
  {
    if (*++s == '\0')
    {
      return -1;
    }
  }

  return mbl;
}

static int
pg_gb18030_verifier(const unsigned char *s, int len)
{
  int l;

  if (!IS_HIGHBIT_SET(*s))
  {
    l = 1;            
  }
  else if (len >= 4 && *(s + 1) >= 0x30 && *(s + 1) <= 0x39)
  {
                                                    
    if (*s >= 0x81 && *s <= 0xfe && *(s + 2) >= 0x81 && *(s + 2) <= 0xfe && *(s + 3) >= 0x30 && *(s + 3) <= 0x39)
    {
      l = 4;
    }
    else
    {
      l = -1;
    }
  }
  else if (len >= 2 && *s >= 0x81 && *s <= 0xfe)
  {
                                    
    if ((*(s + 1) >= 0x40 && *(s + 1) <= 0x7e) || (*(s + 1) >= 0x80 && *(s + 1) <= 0xfe))
    {
      l = 2;
    }
    else
    {
      l = -1;
    }
  }
  else
  {
    l = -1;
  }
  return l;
}

static int
pg_utf8_verifier(const unsigned char *s, int len)
{
  int l = pg_utf_mblen(s);

  if (len < l)
  {
    return -1;
  }

  if (!pg_utf8_islegal(s, l))
  {
    return -1;
  }

  return l;
}

   
                                                          
   
                                                                       
                                                                        
                                                                       
                                                                         
                                                                     
                                                                         
                                                     
   
                                                                      
                                                                            
   
bool
pg_utf8_islegal(const unsigned char *source, int length)
{
  unsigned char a;

  switch (length)
  {
  default:
                                        
    return false;
  case 4:
    a = source[3];
    if (a < 0x80 || a > 0xBF)
    {
      return false;
    }
                   
  case 3:
    a = source[2];
    if (a < 0x80 || a > 0xBF)
    {
      return false;
    }
                   
  case 2:
    a = source[1];
    switch (*source)
    {
    case 0xE0:
      if (a < 0xA0 || a > 0xBF)
      {
        return false;
      }
      break;
    case 0xED:
      if (a < 0x80 || a > 0x9F)
      {
        return false;
      }
      break;
    case 0xF0:
      if (a < 0x90 || a > 0xBF)
      {
        return false;
      }
      break;
    case 0xF4:
      if (a < 0x80 || a > 0x8F)
      {
        return false;
      }
      break;
    default:
      if (a < 0x80 || a > 0xBF)
      {
        return false;
      }
      break;
    }
                   
  case 1:
    a = *source;
    if (a >= 0x80 && a < 0xC2)
    {
      return false;
    }
    if (a > 0xF4)
    {
      return false;
    }
    break;
  }
  return true;
}

#ifndef FRONTEND

   
                                           
   
                                                                             
                                                                          
                                                                        
                                                                             
                                                                            
                                               
   
static bool
pg_generic_charinc(unsigned char *charptr, int len)
{
  unsigned char *lastbyte = charptr + len - 1;
  mbverifier mbverify;

                                                           
  mbverify = pg_wchar_table[GetDatabaseEncoding()].mbverify;

  while (*lastbyte < (unsigned char)255)
  {
    (*lastbyte)++;
    if ((*mbverify)(charptr, len) == len)
    {
      return true;
    }
  }

  return false;
}

   
                                         
   
                                                                        
   
                                                                              
                                                                             
                                                                              
                                                                               
                                                                        
                                                       
   
                                                                            
                                                                           
   
static bool
pg_utf8_increment(unsigned char *charptr, int length)
{
  unsigned char a;
  unsigned char limit;

  switch (length)
  {
  default:
                                        
    return false;
  case 4:
    a = charptr[3];
    if (a < 0xBF)
    {
      charptr[3]++;
      break;
    }
                   
  case 3:
    a = charptr[2];
    if (a < 0xBF)
    {
      charptr[2]++;
      break;
    }
                   
  case 2:
    a = charptr[1];
    switch (*charptr)
    {
    case 0xED:
      limit = 0x9F;
      break;
    case 0xF4:
      limit = 0x8F;
      break;
    default:
      limit = 0xBF;
      break;
    }
    if (a < limit)
    {
      charptr[1]++;
      break;
    }
                   
  case 1:
    a = *charptr;
    if (a == 0x7F || a == 0xDF || a == 0xEF || a == 0xF4)
    {
      return false;
    }
    charptr[0]++;
    break;
  }

  return true;
}

   
                                          
   
                                                                          
                                                                           
                                                                           
                                                          
   
                                                                            
                                                                           
                                                                  
   
                                                                        
                                                                             
                                                                    
                                                               
   
                                                                   
                           
   
static bool
pg_eucjp_increment(unsigned char *charptr, int length)
{
  unsigned char c1, c2;
  int i;

  c1 = *charptr;

  switch (c1)
  {
  case SS2:                 
    if (length != 2)
    {
      return false;
    }

    c2 = charptr[1];

    if (c2 >= 0xdf)
    {
      charptr[0] = charptr[1] = 0xa1;
    }
    else if (c2 < 0xa1)
    {
      charptr[1] = 0xa1;
    }
    else
    {
      charptr[1]++;
    }
    break;

  case SS3:                 
    if (length != 3)
    {
      return false;
    }

    for (i = 2; i > 0; i--)
    {
      c2 = charptr[i];
      if (c2 < 0xa1)
      {
        charptr[i] = 0xa1;
        return true;
      }
      else if (c2 < 0xfe)
      {
        charptr[i]++;
        return true;
      }
    }

                                   
    return false;

  default:
    if (IS_HIGHBIT_SET(c1))                  
    {
      if (length != 2)
      {
        return false;
      }

      for (i = 1; i >= 0; i--)
      {
        c2 = charptr[i];
        if (c2 < 0xa1)
        {
          charptr[i] = 0xa1;
          return true;
        }
        else if (c2 < 0xfe)
        {
          charptr[i]++;
          return true;
        }
      }

                                     
      return false;
    }
    else
    {                         
      if (c1 > 0x7e)
      {
        return false;
      }
      (*charptr)++;
    }
    break;
  }

  return true;
}
#endif                

   
                                                                      
                       
                                                                          
                                                                      
   
const pg_wchar_tbl pg_wchar_table[] = {
    {pg_ascii2wchar_with_len, pg_wchar2single_with_len, pg_ascii_mblen, pg_ascii_dsplen, pg_ascii_verifier, 1},                       
    {pg_eucjp2wchar_with_len, pg_wchar2euc_with_len, pg_eucjp_mblen, pg_eucjp_dsplen, pg_eucjp_verifier, 3},                       
    {pg_euccn2wchar_with_len, pg_wchar2euc_with_len, pg_euccn_mblen, pg_euccn_dsplen, pg_euccn_verifier, 2},                       
    {pg_euckr2wchar_with_len, pg_wchar2euc_with_len, pg_euckr_mblen, pg_euckr_dsplen, pg_euckr_verifier, 3},                       
    {pg_euctw2wchar_with_len, pg_wchar2euc_with_len, pg_euctw_mblen, pg_euctw_dsplen, pg_euctw_verifier, 4},                       
    {pg_eucjp2wchar_with_len, pg_wchar2euc_with_len, pg_eucjp_mblen, pg_eucjp_dsplen, pg_eucjp_verifier, 3},                             
    {pg_utf2wchar_with_len, pg_wchar2utf_with_len, pg_utf_mblen, pg_utf_dsplen, pg_utf8_verifier, 4},                            
    {pg_mule2wchar_with_len, pg_wchar2mule_with_len, pg_mule_mblen, pg_mule_dsplen, pg_mule_verifier, 4},                                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},               
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},                 
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1},               
    {0, 0, pg_sjis_mblen, pg_sjis_dsplen, pg_sjis_verifier, 2},                                                                  
    {0, 0, pg_big5_mblen, pg_big5_dsplen, pg_big5_verifier, 2},                                                                  
    {0, 0, pg_gbk_mblen, pg_gbk_dsplen, pg_gbk_verifier, 2},                                                                    
    {0, 0, pg_uhc_mblen, pg_uhc_dsplen, pg_uhc_verifier, 2},                                                                    
    {0, 0, pg_gb18030_mblen, pg_gb18030_dsplen, pg_gb18030_verifier, 4},                                                            
    {0, 0, pg_johab_mblen, pg_johab_dsplen, pg_johab_verifier, 3},                                                                
    {0, 0, pg_sjis_mblen, pg_sjis_dsplen, pg_sjis_verifier, 2}                                                                             
};

                                                              
int
pg_mic_mblen(const unsigned char *mbstr)
{
  return pg_mule_mblen(mbstr);
}

   
                                                     
   
int
pg_encoding_mblen(int encoding, const char *mbstr)
{
  return (PG_VALID_ENCODING(encoding) ? pg_wchar_table[encoding].mblen((const unsigned char *)mbstr) : pg_wchar_table[PG_SQL_ASCII].mblen((const unsigned char *)mbstr));
}

   
                                                        
   
int
pg_encoding_dsplen(int encoding, const char *mbstr)
{
  return (PG_VALID_ENCODING(encoding) ? pg_wchar_table[encoding].dsplen((const unsigned char *)mbstr) : pg_wchar_table[PG_SQL_ASCII].dsplen((const unsigned char *)mbstr));
}

   
                                                             
                                                                       
                                      
   
int
pg_encoding_verifymb(int encoding, const char *mbstr, int len)
{
  return (PG_VALID_ENCODING(encoding) ? pg_wchar_table[encoding].mbverify((const unsigned char *)mbstr, len) : pg_wchar_table[PG_SQL_ASCII].mbverify((const unsigned char *)mbstr, len));
}

   
                                            
   
int
pg_encoding_max_length(int encoding)
{
  Assert(PG_VALID_ENCODING(encoding));

  return pg_wchar_table[encoding].maxmblen;
}

#ifndef FRONTEND

   
                                                                 
   
int
pg_database_encoding_max_length(void)
{
  return pg_wchar_table[GetDatabaseEncoding()].maxmblen;
}

   
                                                                           
   
mbcharacter_incrementer
pg_database_encoding_character_incrementer(void)
{
     
                                                                             
                               
     
  switch (GetDatabaseEncoding())
  {
  case PG_UTF8:
    return pg_utf8_increment;

  case PG_EUC_JP:
    return pg_eucjp_increment;

  default:
    return pg_generic_charinc;
  }
}

   
                                                                       
                                                            
   
bool
pg_verifymbstr(const char *mbstr, int len, bool noError)
{
  return pg_verify_mbstr_len(GetDatabaseEncoding(), mbstr, len, noError) >= 0;
}

   
                                                                         
             
   
bool
pg_verify_mbstr(int encoding, const char *mbstr, int len, bool noError)
{
  return pg_verify_mbstr_len(encoding, mbstr, len, noError) >= 0;
}

   
                                                                         
             
   
                                                                
                     
   
                                                   
                                                    
                                                                 
   
int
pg_verify_mbstr_len(int encoding, const char *mbstr, int len, bool noError)
{
  mbverifier mbverify;
  int mb_len;

  Assert(PG_VALID_ENCODING(encoding));

     
                                                               
     
  if (pg_encoding_max_length(encoding) <= 1)
  {
    const char *nullpos = memchr(mbstr, 0, len);

    if (nullpos == NULL)
    {
      return len;
    }
    if (noError)
    {
      return -1;
    }
    report_invalid_encoding(encoding, nullpos, 1);
  }

                                        
  mbverify = pg_wchar_table[encoding].mbverify;

  mb_len = 0;

  while (len > 0)
  {
    int l;

                                               
    if (!IS_HIGHBIT_SET(*mbstr))
    {
      if (*mbstr != '\0')
      {
        mb_len++;
        mbstr++;
        len--;
        continue;
      }
      if (noError)
      {
        return -1;
      }
      report_invalid_encoding(encoding, mbstr, len);
    }

    l = (*mbverify)((const unsigned char *)mbstr, len);

    if (l < 0)
    {
      if (noError)
      {
        return -1;
      }
      report_invalid_encoding(encoding, mbstr, len);
    }

    mbstr += l;
    len -= l;
    mb_len++;
  }
  return mb_len;
}

   
                                                                            
   
                                                                            
                                                    
   
                                                                        
                                                                          
                                                                     
   
void
check_encoding_conversion_args(int src_encoding, int dest_encoding, int len, int expected_src_encoding, int expected_dest_encoding)
{
  if (!PG_VALID_ENCODING(src_encoding))
  {
    elog(ERROR, "invalid source encoding ID: %d", src_encoding);
  }
  if (src_encoding != expected_src_encoding && expected_src_encoding >= 0)
  {
    elog(ERROR, "expected source encoding \"%s\", but got \"%s\"", pg_enc2name_tbl[expected_src_encoding].name, pg_enc2name_tbl[src_encoding].name);
  }
  if (!PG_VALID_ENCODING(dest_encoding))
  {
    elog(ERROR, "invalid destination encoding ID: %d", dest_encoding);
  }
  if (dest_encoding != expected_dest_encoding && expected_dest_encoding >= 0)
  {
    elog(ERROR, "expected destination encoding \"%s\", but got \"%s\"", pg_enc2name_tbl[expected_dest_encoding].name, pg_enc2name_tbl[dest_encoding].name);
  }
  if (len < 0)
  {
    elog(ERROR, "encoding conversion length must not be negative");
  }
}

   
                                                                       
   
                                                                     
                                                                       
   
void
report_invalid_encoding(int encoding, const char *mbstr, int len)
{
  int l = pg_encoding_mblen(encoding, mbstr);
  char buf[8 * 5 + 1];
  char *p = buf;
  int j, jlimit;

  jlimit = Min(l, len);
  jlimit = Min(jlimit, 8);                             

  for (j = 0; j < jlimit; j++)
  {
    p += sprintf(p, "0x%02x", (unsigned char)mbstr[j]);
    if (j < jlimit - 1)
    {
      p += sprintf(p, " ");
    }
  }

  ereport(ERROR, (errcode(ERRCODE_CHARACTER_NOT_IN_REPERTOIRE), errmsg("invalid byte sequence for encoding \"%s\": %s", pg_enc2name_tbl[encoding].name, buf)));
}

   
                                                                       
   
                                                                     
                                                                       
   
void
report_untranslatable_char(int src_encoding, int dest_encoding, const char *mbstr, int len)
{
  int l = pg_encoding_mblen(src_encoding, mbstr);
  char buf[8 * 5 + 1];
  char *p = buf;
  int j, jlimit;

  jlimit = Min(l, len);
  jlimit = Min(jlimit, 8);                             

  for (j = 0; j < jlimit; j++)
  {
    p += sprintf(p, "0x%02x", (unsigned char)mbstr[j]);
    if (j < jlimit - 1)
    {
      p += sprintf(p, " ");
    }
  }

  ereport(ERROR, (errcode(ERRCODE_UNTRANSLATABLE_CHARACTER), errmsg("character with byte sequence %s in encoding \"%s\" has no equivalent in encoding \"%s\"", buf, pg_enc2name_tbl[src_encoding].name, pg_enc2name_tbl[dest_encoding].name)));
}

#endif                
