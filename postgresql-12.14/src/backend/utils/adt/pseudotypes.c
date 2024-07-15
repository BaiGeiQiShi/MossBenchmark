                                                                            
   
                 
                                            
   
                                                                       
                                                                        
                                                                       
                                                                       
                                                                       
                  
   
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "libpq/pqformat.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/rangetypes.h"

   
                                                        
   
                                                                            
   
Datum
cstring_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);

  PG_RETURN_CSTRING(pstrdup(str));
}

   
                                                          
   
                                                                        
                              
   
Datum
cstring_out(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);

  PG_RETURN_CSTRING(pstrdup(str));
}

   
                                                                 
   
Datum
cstring_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  char *str;
  int nbytes;

  str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);
  PG_RETURN_CSTRING(str);
}

   
                                                                  
   
Datum
cstring_send(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendtext(&buf, str, strlen(str));
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                                                          
   
Datum
anyarray_in(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", "anyarray")));

  PG_RETURN_VOID();                          
}

   
                                                            
   
                                                                 
   
Datum
anyarray_out(PG_FUNCTION_ARGS)
{
  return array_out(fcinfo);
}

   
                                                                   
   
                                                                     
                                                                  
                                                   
   
Datum
anyarray_recv(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", "anyarray")));

  PG_RETURN_VOID();                          
}

   
                                                                    
   
                                                                  
   
Datum
anyarray_send(PG_FUNCTION_ARGS)
{
  return array_send(fcinfo);
}

   
                                                        
   
Datum
anyenum_in(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", "anyenum")));

  PG_RETURN_VOID();                          
}

   
                                                          
   
                                                                
   
Datum
anyenum_out(PG_FUNCTION_ARGS)
{
  return enum_out(fcinfo);
}

   
                                                          
   
Datum
anyrange_in(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", "anyrange")));

  PG_RETURN_VOID();                          
}

   
                                                            
   
                                                                 
   
Datum
anyrange_out(PG_FUNCTION_ARGS)
{
  return range_out(fcinfo);
}

   
                                                  
   
                                                                          
                                                                        
                         
   
Datum
void_in(PG_FUNCTION_ARGS)
{
  PG_RETURN_VOID();                                              
}

   
                                                    
   
                                                                      
   
Datum
void_out(PG_FUNCTION_ARGS)
{
  PG_RETURN_CSTRING(pstrdup(""));
}

   
                                                          
   
                                                                        
                                                                     
   
Datum
void_recv(PG_FUNCTION_ARGS)
{
  PG_RETURN_VOID();
}

   
                                                           
   
                                                                     
                                         
   
Datum
void_send(PG_FUNCTION_ARGS)
{
  StringInfoData buf;

                            
  pq_begintypsend(&buf);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                                                                          
   
Datum
shell_in(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of a shell type")));

  PG_RETURN_VOID();                          
}

   
                                                  
   
Datum
shell_out(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot display a value of a shell type")));

  PG_RETURN_VOID();                          
}

   
                                                           
   
                                                                             
                                                                           
                                                                         
   
Datum
pg_node_tree_in(PG_FUNCTION_ARGS)
{
     
                                                                             
                                                                 
     
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", "pg_node_tree")));

  PG_RETURN_VOID();                          
}

   
                                                             
   
                                                                         
   
Datum
pg_node_tree_out(PG_FUNCTION_ARGS)
{
  return textout(fcinfo);
}

   
                                                                    
   
Datum
pg_node_tree_recv(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", "pg_node_tree")));

  PG_RETURN_VOID();                          
}

   
                                                                     
   
Datum
pg_node_tree_send(PG_FUNCTION_ARGS)
{
  return textsend(fcinfo);
}

   
                                                              
   
                                                                              
                                 
   
Datum
pg_ddl_command_in(PG_FUNCTION_ARGS)
{
     
                                             
     
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", "pg_ddl_command")));

  PG_RETURN_VOID();                          
}

   
                                                                 
   
                                                                     
   
Datum
pg_ddl_command_out(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot output a value of type %s", "pg_ddl_command")));

  PG_RETURN_VOID();
}

   
                                                                        
   
Datum
pg_ddl_command_recv(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", "pg_ddl_command")));

  PG_RETURN_VOID();
}

   
                                                                         
   
Datum
pg_ddl_command_send(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot output a value of type %s", "pg_ddl_command")));

  PG_RETURN_VOID();
}

   
                                                                             
                              
   
#define PSEUDOTYPE_DUMMY_IO_FUNCS(typname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  Datum typname##_in(PG_FUNCTION_ARGS)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", #typname)));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    PG_RETURN_VOID();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  Datum typname##_out(PG_FUNCTION_ARGS)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot display a value of type %s", #typname)));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    PG_RETURN_VOID();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  extern int no_such_variable

PSEUDOTYPE_DUMMY_IO_FUNCS(any);
PSEUDOTYPE_DUMMY_IO_FUNCS(trigger);
PSEUDOTYPE_DUMMY_IO_FUNCS(event_trigger);
PSEUDOTYPE_DUMMY_IO_FUNCS(language_handler);
PSEUDOTYPE_DUMMY_IO_FUNCS(fdw_handler);
PSEUDOTYPE_DUMMY_IO_FUNCS(index_am_handler);
PSEUDOTYPE_DUMMY_IO_FUNCS(tsm_handler);
PSEUDOTYPE_DUMMY_IO_FUNCS(internal);
PSEUDOTYPE_DUMMY_IO_FUNCS(opaque);
PSEUDOTYPE_DUMMY_IO_FUNCS(anyelement);
PSEUDOTYPE_DUMMY_IO_FUNCS(anynonarray);
PSEUDOTYPE_DUMMY_IO_FUNCS(table_am_handler);
