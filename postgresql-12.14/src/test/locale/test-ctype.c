   
                                
   

   
 
                
 
                                              
                                                 
                                    
 
                                                                             
                                                                             
                              
 
                                                  
                                                                            
                                                
 
  

#include <stdio.h>
#include <locale.h>
#include <ctype.h>

char *
flag(int b);
void
describe_char(int c);

#undef LONG_FLAG

char *
flag(int b)
{
#ifdef LONG_FLAG
  return b ? "yes" : "no";
#else
  return b ? "+" : " ";
#endif
}

void
describe_char(int c)
{
  unsigned char cp = c, up = toupper(c), lo = tolower(c);

  if (!isprint(cp))
  {
    cp = ' ';
  }
  if (!isprint(up))
  {
    up = ' ';
  }
  if (!isprint(lo))
  {
    lo = ' ';
  }

  printf("chr#%-4d%2c%6s%6s%6s%6s%6s%6s%6s%6s%6s%6s%6s%4c%4c\n", c, cp, flag(isalnum(c)), flag(isalpha(c)), flag(iscntrl(c)), flag(isdigit(c)), flag(islower(c)), flag(isgraph(c)), flag(isprint(c)), flag(ispunct(c)), flag(isspace(c)), flag(isupper(c)), flag(isxdigit(c)), lo, up);
}

int
main()
{
  short c;
  char *cur_locale;

  cur_locale = setlocale(LC_ALL, "");
  if (cur_locale)
  {
    fprintf(stderr, "Successfully set locale to \"%s\"\n", cur_locale);
  }
  else
  {
    fprintf(stderr, "Cannot setup locale. Either your libc does not provide\nlocale support, or your locale data is corrupt, or you have not set\nLANG or LC_CTYPE environment variable to proper value. Program aborted.\n");
    return 1;
  }

  printf("char#  char alnum alpha cntrl digit lower graph print punct space upper xdigit lo up\n");
  for (c = 0; c <= 255; c++)
  {
    describe_char(c);
  }

  return 0;
}
