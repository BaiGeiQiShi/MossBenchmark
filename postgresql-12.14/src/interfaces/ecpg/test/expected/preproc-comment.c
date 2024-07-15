                                         
                                                       
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
                                      
#define ECPGdebug(X, Y) ECPGdebug((X) + 100, (Y))

#line 1 "comment.pgc"
#include <stdlib.h>

#line 1 "regression.h"

#line 3 "comment.pgc"

                          int i;
                              ;

                                                                              
                                                                              
                                                                              

int
main(void)
{
  ECPGdebug(1, stderr);

  {
    ECPGconnect(__LINE__, 0, "ecpg1_regression", NULL, NULL, NULL, 0);
  }
#line 17 "comment.pgc"

  {
    ECPGdisconnect(__LINE__, "CURRENT");
  }
#line 19 "comment.pgc"

  exit(0);
}
