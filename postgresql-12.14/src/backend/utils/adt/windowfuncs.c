                                                                            
   
                 
                                                    
   
                                                                         
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "utils/builtins.h"
#include "windowapi.h"

   
                               
   
typedef struct rank_context
{
  int64 rank;                   
} rank_context;

   
                             
   
typedef struct
{
  int32 ntile;                               
  int64 rows_per_bucket;                                   
  int64 boundary;                                                   
  int64 remainder;                                        
} ntile_context;

static bool
rank_up(WindowObject winobj);
static Datum
leadlag_common(FunctionCallInfo fcinfo, bool forward, bool withoffset, bool withdefault);

   
                                         
   
static bool
rank_up(WindowObject winobj)
{
  bool up = false;                            
  int64 curpos = WinGetCurrentPosition(winobj);
  rank_context *context;

  context = (rank_context *)WinGetPartitionLocalMemory(winobj, sizeof(rank_context));

  if (context->rank == 0)
  {
                                                   
    Assert(curpos == 0);
    context->rank = 1;
  }
  else
  {
    Assert(curpos > 0);
                                                               
    if (!WinRowsArePeers(winobj, curpos - 1, curpos))
    {
      up = true;
    }
  }

                                                                     
  WinSetMarkPosition(winobj, curpos);

  return up;
}

   
              
                                                              
   
Datum
window_row_number(PG_FUNCTION_ARGS)
{
  WindowObject winobj = PG_WINDOW_OBJECT();
  int64 curpos = WinGetCurrentPosition(winobj);

  WinSetMarkPosition(winobj, curpos);
  PG_RETURN_INT64(curpos + 1);
}

   
        
                                         
                                                  
   
Datum
window_rank(PG_FUNCTION_ARGS)
{
  WindowObject winobj = PG_WINDOW_OBJECT();
  rank_context *context;
  bool up;

  up = rank_up(winobj);
  context = (rank_context *)WinGetPartitionLocalMemory(winobj, sizeof(rank_context));
  if (up)
  {
    context->rank = WinGetCurrentPosition(winobj) + 1;
  }

  PG_RETURN_INT64(context->rank);
}

   
              
                                                
   
Datum
window_dense_rank(PG_FUNCTION_ARGS)
{
  WindowObject winobj = PG_WINDOW_OBJECT();
  rank_context *context;
  bool up;

  up = rank_up(winobj);
  context = (rank_context *)WinGetPartitionLocalMemory(winobj, sizeof(rank_context));
  if (up)
  {
    context->rank++;
  }

  PG_RETURN_INT64(context->rank);
}

   
                
                                              
                                                                            
                                                      
   
Datum
window_percent_rank(PG_FUNCTION_ARGS)
{
  WindowObject winobj = PG_WINDOW_OBJECT();
  rank_context *context;
  bool up;
  int64 totalrows = WinGetPartitionRowCount(winobj);

  Assert(totalrows > 0);

  up = rank_up(winobj);
  context = (rank_context *)WinGetPartitionLocalMemory(winobj, sizeof(rank_context));
  if (up)
  {
    context->rank = WinGetCurrentPosition(winobj) + 1;
  }

                                                     
  if (totalrows <= 1)
  {
    PG_RETURN_FLOAT8(0.0);
  }

  PG_RETURN_FLOAT8((float8)(context->rank - 1) / (float8)(totalrows - 1));
}

   
             
                                              
                                                                              
                                                                           
   
Datum
window_cume_dist(PG_FUNCTION_ARGS)
{
  WindowObject winobj = PG_WINDOW_OBJECT();
  rank_context *context;
  bool up;
  int64 totalrows = WinGetPartitionRowCount(winobj);

  Assert(totalrows > 0);

  up = rank_up(winobj);
  context = (rank_context *)WinGetPartitionLocalMemory(winobj, sizeof(rank_context));
  if (up || context->rank == 1)
  {
       
                                                                         
                                                                 
       
    int64 row;

    context->rank = WinGetCurrentPosition(winobj) + 1;

       
                              
       
    for (row = context->rank; row < totalrows; row++)
    {
      if (!WinRowsArePeers(winobj, row - 1, row))
      {
        break;
      }
      context->rank++;
    }
  }

  PG_RETURN_FLOAT8((float8)context->rank / (float8)totalrows);
}

   
         
                                                       
                                        
   
Datum
window_ntile(PG_FUNCTION_ARGS)
{
  WindowObject winobj = PG_WINDOW_OBJECT();
  ntile_context *context;

  context = (ntile_context *)WinGetPartitionLocalMemory(winobj, sizeof(ntile_context));

  if (context->ntile == 0)
  {
                    
    int64 total;
    int32 nbuckets;
    bool isnull;

    total = WinGetPartitionRowCount(winobj);
    nbuckets = DatumGetInt32(WinGetFuncArgCurrent(winobj, 0, &isnull));

       
                                                                      
              
       
    if (isnull)
    {
      PG_RETURN_NULL();
    }

       
                                                                  
                                      
       
    if (nbuckets <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_NTILE), errmsg("argument of ntile must be greater than zero")));
    }

    context->ntile = 1;
    context->rows_per_bucket = 0;
    context->boundary = total / nbuckets;
    if (context->boundary <= 0)
    {
      context->boundary = 1;
    }
    else
    {
         
                                                                    
                  
         
      context->remainder = total % nbuckets;
      if (context->remainder != 0)
      {
        context->boundary++;
      }
    }
  }

  context->rows_per_bucket++;
  if (context->boundary < context->rows_per_bucket)
  {
                  
    if (context->remainder != 0 && context->ntile == context->remainder)
    {
      context->remainder = 0;
      context->boundary -= 1;
    }
    context->ntile += 1;
    context->rows_per_bucket = 1;
  }

  PG_RETURN_INT32(context->ntile);
}

   
                  
                                        
                                                              
                                                           
                                                           
   
static Datum
leadlag_common(FunctionCallInfo fcinfo, bool forward, bool withoffset, bool withdefault)
{
  WindowObject winobj = PG_WINDOW_OBJECT();
  int32 offset;
  bool const_offset;
  Datum result;
  bool isnull;
  bool isout;

  if (withoffset)
  {
    offset = DatumGetInt32(WinGetFuncArgCurrent(winobj, 1, &isnull));
    if (isnull)
    {
      PG_RETURN_NULL();
    }
    const_offset = get_fn_expr_arg_stable(fcinfo->flinfo, 1);
  }
  else
  {
    offset = 1;
    const_offset = true;
  }

  result = WinGetFuncArgInPartition(winobj, 0, (forward ? offset : -offset), WINDOW_SEEK_CURRENT, const_offset, &isnull, &isout);

  if (isout)
  {
       
                                                                   
                                            
       
    if (withdefault)
    {
      result = WinGetFuncArgCurrent(winobj, 2, &isnull);
    }
  }

  if (isnull)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_DATUM(result);
}

   
       
                                                        
                                                  
             
   
Datum
window_lag(PG_FUNCTION_ARGS)
{
  return leadlag_common(fcinfo, false, false, false);
}

   
                   
                                                             
                                                   
             
   
Datum
window_lag_with_offset(PG_FUNCTION_ARGS)
{
  return leadlag_common(fcinfo, false, true, false);
}

   
                               
                                                     
                          
   
Datum
window_lag_with_offset_and_default(PG_FUNCTION_ARGS)
{
  return leadlag_common(fcinfo, false, true, true);
}

   
        
                                                        
                                                 
             
   
Datum
window_lead(PG_FUNCTION_ARGS)
{
  return leadlag_common(fcinfo, true, false, false);
}

   
                    
                                                             
                                                            
             
   
Datum
window_lead_with_offset(PG_FUNCTION_ARGS)
{
  return leadlag_common(fcinfo, true, true, false);
}

   
                                
                                                      
                          
   
Datum
window_lead_with_offset_and_default(PG_FUNCTION_ARGS)
{
  return leadlag_common(fcinfo, true, true, true);
}

   
               
                                                            
                           
   
Datum
window_first_value(PG_FUNCTION_ARGS)
{
  WindowObject winobj = PG_WINDOW_OBJECT();
  Datum result;
  bool isnull;

  result = WinGetFuncArgInFrame(winobj, 0, 0, WINDOW_SEEK_HEAD, true, &isnull, NULL);
  if (isnull)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_DATUM(result);
}

   
              
                                                           
                           
   
Datum
window_last_value(PG_FUNCTION_ARGS)
{
  WindowObject winobj = PG_WINDOW_OBJECT();
  Datum result;
  bool isnull;

  result = WinGetFuncArgInFrame(winobj, 0, 0, WINDOW_SEEK_TAIL, true, &isnull, NULL);
  if (isnull)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_DATUM(result);
}

   
             
                                                                   
                                      
   
Datum
window_nth_value(PG_FUNCTION_ARGS)
{
  WindowObject winobj = PG_WINDOW_OBJECT();
  bool const_offset;
  Datum result;
  bool isnull;
  int32 nth;

  nth = DatumGetInt32(WinGetFuncArgCurrent(winobj, 1, &isnull));
  if (isnull)
  {
    PG_RETURN_NULL();
  }
  const_offset = get_fn_expr_arg_stable(fcinfo->flinfo, 1);

  if (nth <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_NTH_VALUE), errmsg("argument of nth_value must be greater than zero")));
  }

  result = WinGetFuncArgInFrame(winobj, 0, nth - 1, WINDOW_SEEK_HEAD, const_offset, &isnull, NULL);
  if (isnull)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_DATUM(result);
}
