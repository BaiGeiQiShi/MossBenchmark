                                                                            
   
             
                             
   
                                                                         
                                                                        
   
   
                  
                        
   
                                                                            
   
#include "c.h"

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

   
                 
   
                                                                         
                                                                  
   
                                                                           
                                                
                                             
                                                                           
   
                                                                               
                         
   
void
simple_prompt(const char *prompt, char *destination, size_t destlen, bool echo)
{
  int length;
  FILE *termin, *termout;

#if defined(HAVE_TERMIOS_H)
  struct termios t_orig, t;
#elif defined(WIN32)
  HANDLE t = NULL;
  DWORD t_orig = 0;
#endif

#ifdef WIN32

     
                                                                           
                                                                             
                                                                             
                                                                      
                                                                            
                                                                            
                                                                             
                                                                        
                                                                          
                                                                  
     
                                                                             
                                             
     
                                                                          
                                                             
     
  termin = fopen("CONIN$", "w+");
  termout = fopen("CONOUT$", "w+");
#else

     
                                                                           
                                      
     
  termin = fopen("/dev/tty", "r");
  termout = fopen("/dev/tty", "w");
#endif
  if (!termin ||
      !termout
#ifdef WIN32

         
                                                                                
                                                                                 
                                                                            
                                                                                 
         
      || (getenv("OSTYPE") && strcmp(getenv("OSTYPE"), "msys") == 0)
#endif
  )
  {
    if (termin)
    {
      fclose(termin);
    }
    if (termout)
    {
      fclose(termout);
    }
    termin = stdin;
    termout = stderr;
  }

  if (!echo)
  {
#if defined(HAVE_TERMIOS_H)
                                              
    tcgetattr(fileno(termin), &t);
    t_orig = t;
    t.c_lflag &= ~ECHO;
    tcsetattr(fileno(termin), TCSAFLUSH, &t);
#elif defined(WIN32)
                                                 
    t = (HANDLE)_get_osfhandle(_fileno(termin));

                                          
    GetConsoleMode(t, &t_orig);

                             
    SetConsoleMode(t, ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
#endif
  }

  if (prompt)
  {
    fputs(_(prompt), termout);
    fflush(termout);
  }

  if (fgets(destination, destlen, termin) == NULL)
  {
    destination[0] = '\0';
  }

  length = strlen(destination);
  if (length > 0 && destination[length - 1] != '\n')
  {
                              
    char buf[128];
    int buflen;

    do
    {
      if (fgets(buf, sizeof(buf), termin) == NULL)
      {
        break;
      }
      buflen = strlen(buf);
    } while (buflen > 0 && buf[buflen - 1] != '\n');
  }

  if (length > 0 && destination[length - 1] == '\n')
  {
                                 
    destination[length - 1] = '\0';
  }

  if (!echo)
  {
                                                      
#if defined(HAVE_TERMIOS_H)
    tcsetattr(fileno(termin), TCSAFLUSH, &t_orig);
    fputs("\n", termout);
    fflush(termout);
#elif defined(WIN32)
    SetConsoleMode(t, t_orig);
    fputs("\n", termout);
    fflush(termout);
#endif
  }

  if (termin != stdin)
  {
    fclose(termin);
    fclose(termout);
  }
}
