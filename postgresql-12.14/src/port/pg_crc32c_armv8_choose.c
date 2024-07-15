                                                                            
   
                            
                                                               
   
                                                                        
                                                                       
                                                                         
                   
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <setjmp.h>
#include <signal.h>

#include "port/pg_crc32c.h"

static sigjmp_buf illegal_instruction_jump;

   
                                                                          
                                                                
   
static void
illegal_instruction_handler(SIGNAL_ARGS)
{
  siglongjmp(illegal_instruction_jump, 1);
}

static bool
pg_crc32c_armv8_available(void)
{
  uint64 data = 42;
  int result;

     
                                                                           
                                                    
     
  pqsignal(SIGILL, illegal_instruction_handler);
  if (sigsetjmp(illegal_instruction_jump, 1) == 0)
  {
                                                                         
    result = (pg_comp_crc32c_armv8(0, &data, sizeof(data)) == pg_comp_crc32c_sb8(0, &data, sizeof(data)));
  }
  else
  {
                                
    result = -1;
  }
  pqsignal(SIGILL, SIG_DFL);

#ifndef FRONTEND
                                                     
  if (result == 0)
  {
    elog(ERROR, "crc32 hardware and software results disagree");
  }

  elog(DEBUG1, "using armv8 crc32 hardware = %d", (result > 0));
#endif

  return (result > 0);
}

   
                                                                        
                                                                              
   
static pg_crc32c
pg_comp_crc32c_choose(pg_crc32c crc, const void *data, size_t len)
{
  if (pg_crc32c_armv8_available())
  {
    pg_comp_crc32c = pg_comp_crc32c_armv8;
  }
  else
  {
    pg_comp_crc32c = pg_comp_crc32c_sb8;
  }

  return pg_comp_crc32c(crc, data, len);
}

pg_crc32c (*pg_comp_crc32c)(pg_crc32c crc, const void *data, size_t len) = pg_comp_crc32c_choose;
