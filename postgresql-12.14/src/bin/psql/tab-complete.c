   
                                              
   
                                                                
   
                               
   

                                                                         
                                                                    
                                                                
                                                                   
                                                                       
                                                                     
                                               
   
                                                                    
                                                               
                                                                     
                                                               
                        
   
             
                              
           
   
                                                                 
   
         
                                                                     
                         
                                                                         
   

#include "postgres_fe.h"
#include "tab-complete.h"
#include "input.h"

                                                                           
#ifdef USE_READLINE

#include <ctype.h>

#include "catalog/pg_am_d.h"
#include "catalog/pg_class_d.h"

#include "libpq-fe.h"
#include "pqexpbuffer.h"
#include "common.h"
#include "settings.h"
#include "stringutils.h"

#ifdef HAVE_RL_FILENAME_COMPLETION_FUNCTION
#define filename_completion_function rl_filename_completion_function
#else
                                  
extern char *
filename_completion_function();
#endif

#ifdef HAVE_RL_COMPLETION_MATCHES
#define completion_matches rl_completion_matches
#endif

#define PQmblenBounded(s, e) strnlen(s, PQmblen(s, e))

                           
#define WORD_BREAKS "\t\n@$><=;|&{() "

   
                                                                              
                                                                             
                                                           
   
PQExpBuffer tab_completion_query_buf = NULL;

   
                                                                         
                                                                           
                                                                          
                                                                            
                                                                  
   
                                                                           
                                                                               
                                                                              
                                                             
   
typedef struct VersionedQuery
{
  int min_server_version;
  const char *query;
} VersionedQuery;

   
                                                                          
                                                                            
                                                                           
                                                                      
                                                                        
   
                                                                             
                              
   
typedef struct SchemaQuery
{
     
                                                                         
                                                                            
                                                                             
     
  int min_server_version;

     
                                                                
                                                                         
     
  const char *catname;

     
                                                                             
                                                                             
                                                                            
                                                                          
                 
     
  const char *selcondition;

     
                                                                    
                                                                           
     
  const char *viscondition;

     
                                                                           
                       
     
  const char *namespace;

     
                                                                           
                                                                          
     
  const char *result;

     
                                                                        
                                                           
     
  const char *qualresult;
} SchemaQuery;

                                                                 
                                          
   
static int completion_max_records;

   
                                                                            
                                                                        
   
static const char *completion_charp;                                  
static const char *const *completion_charpp;                                   
static const char *completion_info_charp;                                    
static const char *completion_info_charp2;                                  
static const VersionedQuery *completion_vquery;                               
static const SchemaQuery *completion_squery;                               
static bool completion_case_sensitive;                                            

   
                                                                        
               
                                                                          
                                                   
                                                   
                                                          
                                                                     
                                                                          
                                                 
                                                                             
                                                                               
   
#define COMPLETE_WITH_QUERY(query)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    completion_charp = query;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    matches = completion_matches(text, complete_from_query);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  } while (0)

#define COMPLETE_WITH_VERSIONED_QUERY(query)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    completion_vquery = query;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    matches = completion_matches(text, complete_from_versioned_query);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  } while (0)

#define COMPLETE_WITH_SCHEMA_QUERY(query, addon)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    completion_squery = &(query);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    completion_charp = addon;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    matches = completion_matches(text, complete_from_schema_query);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  } while (0)

#define COMPLETE_WITH_VERSIONED_SCHEMA_QUERY(query, addon)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    completion_squery = query;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    completion_vquery = addon;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    matches = completion_matches(text, complete_from_versioned_schema_query);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  } while (0)

   
                                                                             
                                                   
   
#define COMPLETE_WITH_CONST(cs, con)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    completion_case_sensitive = (cs);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    completion_charp = (con);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    matches = completion_matches(text, complete_from_const);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  } while (0)

#define COMPLETE_WITH_LIST_INT(cs, list)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    completion_case_sensitive = (cs);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    completion_charpp = (list);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    matches = completion_matches(text, complete_from_list);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  } while (0)

#define COMPLETE_WITH_LIST(list) COMPLETE_WITH_LIST_INT(false, list)
#define COMPLETE_WITH_LIST_CS(list) COMPLETE_WITH_LIST_INT(true, list)

#define COMPLETE_WITH(...)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    static const char *const list[] = {__VA_ARGS__, NULL};                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    COMPLETE_WITH_LIST(list);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  } while (0)

#define COMPLETE_WITH_CS(...)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    static const char *const list[] = {__VA_ARGS__, NULL};                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    COMPLETE_WITH_LIST_CS(list);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  } while (0)

#define COMPLETE_WITH_ATTR(relation, addon)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    char *_completion_schema;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    char *_completion_table;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    _completion_schema = strtokx(relation, " \t\n\r", ".", "\"", 0, false, false, pset.encoding);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    (void)strtokx(NULL, " \t\n\r", ".", "\"", 0, false, false, pset.encoding);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    _completion_table = strtokx(NULL, " \t\n\r", ".", "\"", 0, false, false, pset.encoding);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    if (_completion_table == NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      completion_charp = Query_for_list_of_attributes addon;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
      completion_info_charp = relation;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      completion_charp = Query_for_list_of_attributes_with_schema addon;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      completion_info_charp = _completion_table;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      completion_info_charp2 = _completion_schema;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    matches = completion_matches(text, complete_from_query);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  } while (0)

#define COMPLETE_WITH_ENUM_VALUE(type)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    char *_completion_schema;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    char *_completion_type;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    _completion_schema = strtokx(type, " \t\n\r", ".", "\"", 0, false, false, pset.encoding);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    (void)strtokx(NULL, " \t\n\r", ".", "\"", 0, false, false, pset.encoding);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    _completion_type = strtokx(NULL, " \t\n\r", ".", "\"", 0, false, false, pset.encoding);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    if (_completion_type == NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      completion_charp = Query_for_list_of_enum_values;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      completion_info_charp = type;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      completion_charp = Query_for_list_of_enum_values_with_schema;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      completion_info_charp = _completion_type;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      completion_info_charp2 = _completion_schema;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    matches = completion_matches(text, complete_from_query);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  } while (0)

#define COMPLETE_WITH_FUNCTION_ARG(function)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    char *_completion_schema;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    char *_completion_function;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    _completion_schema = strtokx(function, " \t\n\r", ".", "\"", 0, false, false, pset.encoding);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    (void)strtokx(NULL, " \t\n\r", ".", "\"", 0, false, false, pset.encoding);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    _completion_function = strtokx(NULL, " \t\n\r", ".", "\"", 0, false, false, pset.encoding);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    if (_completion_function == NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      completion_charp = Query_for_list_of_arguments;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      completion_info_charp = function;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      completion_charp = Query_for_list_of_arguments_with_schema;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      completion_info_charp = _completion_function;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      completion_info_charp2 = _completion_schema;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    matches = completion_matches(text, complete_from_query);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  } while (0)

   
                                            
   

static const SchemaQuery Query_for_list_of_aggregates[] = {{
                                                               .min_server_version = 110000,
                                                               .catname = "pg_catalog.pg_proc p",
                                                               .selcondition = "p.prokind = 'a'",
                                                               .viscondition = "pg_catalog.pg_function_is_visible(p.oid)",
                                                               .namespace = "p.pronamespace",
                                                               .result = "pg_catalog.quote_ident(p.proname)",
                                                           },
    {
        .catname = "pg_catalog.pg_proc p",
        .selcondition = "p.proisagg",
        .viscondition = "pg_catalog.pg_function_is_visible(p.oid)",
        .namespace = "p.pronamespace",
        .result = "pg_catalog.quote_ident(p.proname)",
    }};

static const SchemaQuery Query_for_list_of_datatypes = {
    .catname = "pg_catalog.pg_type t",
                                                                
    .selcondition = "(t.typrelid = 0 "
                    " OR (SELECT c.relkind = " CppAsString2(RELKIND_COMPOSITE_TYPE) "     FROM pg_catalog.pg_class c WHERE c.oid = t.typrelid)) "
                                                                                    "AND t.typname !~ '^_'",
    .viscondition = "pg_catalog.pg_type_is_visible(t.oid)",
    .namespace = "t.typnamespace",
    .result = "pg_catalog.format_type(t.oid, NULL)",
    .qualresult = "pg_catalog.quote_ident(t.typname)",
};

static const SchemaQuery Query_for_list_of_composite_datatypes = {
    .catname = "pg_catalog.pg_type t",
                                                   
    .selcondition = "(SELECT c.relkind = " CppAsString2(RELKIND_COMPOSITE_TYPE) " FROM pg_catalog.pg_class c WHERE c.oid = t.typrelid) "
                                                                                "AND t.typname !~ '^_'",
    .viscondition = "pg_catalog.pg_type_is_visible(t.oid)",
    .namespace = "t.typnamespace",
    .result = "pg_catalog.format_type(t.oid, NULL)",
    .qualresult = "pg_catalog.quote_ident(t.typname)",
};

static const SchemaQuery Query_for_list_of_domains = {
    .catname = "pg_catalog.pg_type t",
    .selcondition = "t.typtype = 'd'",
    .viscondition = "pg_catalog.pg_type_is_visible(t.oid)",
    .namespace = "t.typnamespace",
    .result = "pg_catalog.quote_ident(t.typname)",
};

                                                                            
static const SchemaQuery Query_for_list_of_functions[] = {{
                                                              .min_server_version = 110000,
                                                              .catname = "pg_catalog.pg_proc p",
                                                              .selcondition = "p.prokind != 'p'",
                                                              .viscondition = "pg_catalog.pg_function_is_visible(p.oid)",
                                                              .namespace = "p.pronamespace",
                                                              .result = "pg_catalog.quote_ident(p.proname)",
                                                          },
    {
        .catname = "pg_catalog.pg_proc p",
        .viscondition = "pg_catalog.pg_function_is_visible(p.oid)",
        .namespace = "p.pronamespace",
        .result = "pg_catalog.quote_ident(p.proname)",
    }};

static const SchemaQuery Query_for_list_of_procedures[] = {{
                                                               .min_server_version = 110000,
                                                               .catname = "pg_catalog.pg_proc p",
                                                               .selcondition = "p.prokind = 'p'",
                                                               .viscondition = "pg_catalog.pg_function_is_visible(p.oid)",
                                                               .namespace = "p.pronamespace",
                                                               .result = "pg_catalog.quote_ident(p.proname)",
                                                           },
    {
                                             
        .catname = NULL,
    }};

static const SchemaQuery Query_for_list_of_routines = {
    .catname = "pg_catalog.pg_proc p",
    .viscondition = "pg_catalog.pg_function_is_visible(p.oid)",
    .namespace = "p.pronamespace",
    .result = "pg_catalog.quote_ident(p.proname)",
};

static const SchemaQuery Query_for_list_of_sequences = {
    .catname = "pg_catalog.pg_class c",
    .selcondition = "c.relkind IN (" CppAsString2(RELKIND_SEQUENCE) ")",
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

static const SchemaQuery Query_for_list_of_foreign_tables = {
    .catname = "pg_catalog.pg_class c",
    .selcondition = "c.relkind IN (" CppAsString2(RELKIND_FOREIGN_TABLE) ")",
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

static const SchemaQuery Query_for_list_of_tables = {
    .catname = "pg_catalog.pg_class c",
    .selcondition = "c.relkind IN (" CppAsString2(RELKIND_RELATION) ", " CppAsString2(RELKIND_PARTITIONED_TABLE) ")",
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

static const SchemaQuery Query_for_list_of_partitioned_tables = {
    .catname = "pg_catalog.pg_class c",
    .selcondition = "c.relkind IN (" CppAsString2(RELKIND_PARTITIONED_TABLE) ")",
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

static const SchemaQuery Query_for_list_of_views = {
    .catname = "pg_catalog.pg_class c",
    .selcondition = "c.relkind IN (" CppAsString2(RELKIND_VIEW) ")",
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

static const SchemaQuery Query_for_list_of_matviews = {
    .catname = "pg_catalog.pg_class c",
    .selcondition = "c.relkind IN (" CppAsString2(RELKIND_MATVIEW) ")",
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

static const SchemaQuery Query_for_list_of_indexes = {
    .catname = "pg_catalog.pg_class c",
    .selcondition = "c.relkind IN (" CppAsString2(RELKIND_INDEX) ", " CppAsString2(RELKIND_PARTITIONED_INDEX) ")",
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

static const SchemaQuery Query_for_list_of_partitioned_indexes = {
    .catname = "pg_catalog.pg_class c",
    .selcondition = "c.relkind = " CppAsString2(RELKIND_PARTITIONED_INDEX),
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

                   
static const SchemaQuery Query_for_list_of_relations = {
    .catname = "pg_catalog.pg_class c",
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

                           
static const SchemaQuery Query_for_list_of_partitioned_relations = {
    .catname = "pg_catalog.pg_class c",
    .selcondition = "c.relkind IN (" CppAsString2(RELKIND_PARTITIONED_TABLE) ", " CppAsString2(RELKIND_PARTITIONED_INDEX) ")",
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

                                                   
static const SchemaQuery Query_for_list_of_updatables = {
    .catname = "pg_catalog.pg_class c",
    .selcondition = "c.relkind IN (" CppAsString2(RELKIND_RELATION) ", " CppAsString2(RELKIND_FOREIGN_TABLE) ", " CppAsString2(RELKIND_VIEW) ", " CppAsString2(RELKIND_PARTITIONED_TABLE) ")",
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

                                 
static const SchemaQuery Query_for_list_of_selectables = {
    .catname = "pg_catalog.pg_class c",
    .selcondition = "c.relkind IN (" CppAsString2(RELKIND_RELATION) ", " CppAsString2(RELKIND_SEQUENCE) ", " CppAsString2(RELKIND_VIEW) ", " CppAsString2(RELKIND_MATVIEW) ", " CppAsString2(RELKIND_FOREIGN_TABLE) ", " CppAsString2(RELKIND_PARTITIONED_TABLE) ")",
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

                                                                              
#define Query_for_list_of_grantables Query_for_list_of_selectables

                                  
static const SchemaQuery Query_for_list_of_analyzables = {
    .catname = "pg_catalog.pg_class c",
    .selcondition = "c.relkind IN (" CppAsString2(RELKIND_RELATION) ", " CppAsString2(RELKIND_PARTITIONED_TABLE) ", " CppAsString2(RELKIND_MATVIEW) ", " CppAsString2(RELKIND_FOREIGN_TABLE) ")",
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

                                         
static const SchemaQuery Query_for_list_of_indexables = {
    .catname = "pg_catalog.pg_class c",
    .selcondition = "c.relkind IN (" CppAsString2(RELKIND_RELATION) ", " CppAsString2(RELKIND_PARTITIONED_TABLE) ", " CppAsString2(RELKIND_MATVIEW) ")",
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

                                 
static const SchemaQuery Query_for_list_of_vacuumables = {
    .catname = "pg_catalog.pg_class c",
    .selcondition = "c.relkind IN (" CppAsString2(RELKIND_RELATION) ", " CppAsString2(RELKIND_MATVIEW) ")",
    .viscondition = "pg_catalog.pg_table_is_visible(c.oid)",
    .namespace = "c.relnamespace",
    .result = "pg_catalog.quote_ident(c.relname)",
};

                                                                                
#define Query_for_list_of_clusterables Query_for_list_of_vacuumables

static const SchemaQuery Query_for_list_of_constraints_with_schema = {
    .catname = "pg_catalog.pg_constraint c",
    .selcondition = "c.conrelid <> 0",
    .viscondition = "true",                                           
    .namespace = "c.connamespace",
    .result = "pg_catalog.quote_ident(c.conname)",
};

static const SchemaQuery Query_for_list_of_statistics = {
    .catname = "pg_catalog.pg_statistic_ext s",
    .viscondition = "pg_catalog.pg_statistics_obj_is_visible(s.oid)",
    .namespace = "s.stxnamespace",
    .result = "pg_catalog.quote_ident(s.stxname)",
};

   
                                                                      
                                                                             
                                                                              
                                                                              
                                                                           
                                                                    
                                                                           
                           
   
                                                                    
                           
   

#define Query_for_list_of_attributes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  "SELECT pg_catalog.quote_ident(attname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "  FROM pg_catalog.pg_attribute a, pg_catalog.pg_class c "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  " WHERE c.oid = a.attrelid "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  "   AND a.attnum > 0 "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  "   AND NOT a.attisdropped "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  "   AND substring(pg_catalog.quote_ident(attname),1,%d)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  "   AND (pg_catalog.quote_ident(relname)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  "        OR '\"' || relname || '\"'='%s') "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "   AND pg_catalog.pg_table_is_visible(c.oid)"

#define Query_for_list_of_attribute_numbers                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "SELECT attnum "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  "  FROM pg_catalog.pg_attribute a, pg_catalog.pg_class c "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  " WHERE c.oid = a.attrelid "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  "   AND a.attnum > 0 "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  "   AND NOT a.attisdropped "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  "   AND substring(attnum::pg_catalog.text,1,%d)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  "   AND (pg_catalog.quote_ident(relname)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  "        OR '\"' || relname || '\"'='%s') "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "   AND pg_catalog.pg_table_is_visible(c.oid)"

#define Query_for_list_of_attributes_with_schema                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  "SELECT pg_catalog.quote_ident(attname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "  FROM pg_catalog.pg_attribute a, pg_catalog.pg_class c, pg_catalog.pg_namespace n "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  " WHERE c.oid = a.attrelid "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  "   AND n.oid = c.relnamespace "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  "   AND a.attnum > 0 "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  "   AND NOT a.attisdropped "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  "   AND substring(pg_catalog.quote_ident(attname),1,%d)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  "   AND (pg_catalog.quote_ident(relname)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  "        OR '\"' || relname || '\"' ='%s') "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  "   AND (pg_catalog.quote_ident(nspname)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  "        OR '\"' || nspname || '\"' ='%s') "

#define Query_for_list_of_enum_values                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  "SELECT pg_catalog.quote_literal(enumlabel) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  "  FROM pg_catalog.pg_enum e, pg_catalog.pg_type t "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  " WHERE t.oid = e.enumtypid "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  "   AND substring(pg_catalog.quote_literal(enumlabel),1,%d)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  "   AND (pg_catalog.quote_ident(typname)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  "        OR '\"' || typname || '\"'='%s') "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "   AND pg_catalog.pg_type_is_visible(t.oid)"

#define Query_for_list_of_enum_values_with_schema                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  "SELECT pg_catalog.quote_literal(enumlabel) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  "  FROM pg_catalog.pg_enum e, pg_catalog.pg_type t, pg_catalog.pg_namespace n "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  " WHERE t.oid = e.enumtypid "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  "   AND n.oid = t.typnamespace "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  "   AND substring(pg_catalog.quote_literal(enumlabel),1,%d)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  "   AND (pg_catalog.quote_ident(typname)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  "        OR '\"' || typname || '\"'='%s') "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "   AND (pg_catalog.quote_ident(nspname)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  "        OR '\"' || nspname || '\"' ='%s') "

#define Query_for_list_of_template_databases                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "SELECT pg_catalog.quote_ident(d.datname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "  FROM pg_catalog.pg_database d "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  " WHERE substring(pg_catalog.quote_ident(d.datname),1,%d)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  "   AND (d.datistemplate OR pg_catalog.pg_has_role(d.datdba, 'USAGE'))"

#define Query_for_list_of_databases                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  "SELECT pg_catalog.quote_ident(datname) FROM pg_catalog.pg_database "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  " WHERE substring(pg_catalog.quote_ident(datname),1,%d)='%s'"

#define Query_for_list_of_tablespaces                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  "SELECT pg_catalog.quote_ident(spcname) FROM pg_catalog.pg_tablespace "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  " WHERE substring(pg_catalog.quote_ident(spcname),1,%d)='%s'"

#define Query_for_list_of_encodings                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  " SELECT DISTINCT pg_catalog.pg_encoding_to_char(conforencoding) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  "   FROM pg_catalog.pg_conversion "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  "  WHERE substring(pg_catalog.pg_encoding_to_char(conforencoding),1,%d)=UPPER('%s')"

#define Query_for_list_of_languages                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  "SELECT pg_catalog.quote_ident(lanname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "  FROM pg_catalog.pg_language "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  " WHERE lanname != 'internal' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  "   AND substring(pg_catalog.quote_ident(lanname),1,%d)='%s'"

#define Query_for_list_of_schemas                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  "SELECT pg_catalog.quote_ident(nspname) FROM pg_catalog.pg_namespace "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  " WHERE substring(pg_catalog.quote_ident(nspname),1,%d)='%s'"

#define Query_for_list_of_alter_system_set_vars                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  "SELECT name FROM "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  " (SELECT pg_catalog.lower(name) AS name FROM pg_catalog.pg_settings "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  "  WHERE context != 'internal' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  "  UNION ALL SELECT 'all') ss "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  " WHERE substring(name,1,%d)='%s'"

#define Query_for_list_of_set_vars                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  "SELECT name FROM "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  " (SELECT pg_catalog.lower(name) AS name FROM pg_catalog.pg_settings "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  "  WHERE context IN ('user', 'superuser') "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "  UNION ALL SELECT 'constraints' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  "  UNION ALL SELECT 'transaction' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  "  UNION ALL SELECT 'session' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  "  UNION ALL SELECT 'role' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  "  UNION ALL SELECT 'tablespace' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  "  UNION ALL SELECT 'all') ss "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  " WHERE substring(name,1,%d)='%s'"

#define Query_for_list_of_show_vars                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  "SELECT name FROM "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  " (SELECT pg_catalog.lower(name) AS name FROM pg_catalog.pg_settings "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  "  UNION ALL SELECT 'session authorization' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  "  UNION ALL SELECT 'all') ss "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  " WHERE substring(name,1,%d)='%s'"

#define Query_for_list_of_roles                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  " SELECT pg_catalog.quote_ident(rolname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "   FROM pg_catalog.pg_roles "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  "  WHERE substring(pg_catalog.quote_ident(rolname),1,%d)='%s'"

#define Query_for_list_of_grant_roles                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  " SELECT pg_catalog.quote_ident(rolname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "   FROM pg_catalog.pg_roles "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  "  WHERE substring(pg_catalog.quote_ident(rolname),1,%d)='%s'"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  " UNION ALL SELECT 'PUBLIC'"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  " UNION ALL SELECT 'CURRENT_USER'"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  " UNION ALL SELECT 'SESSION_USER'"

                                                                           
#define Query_for_index_of_table                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  "SELECT pg_catalog.quote_ident(c2.relname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  "  FROM pg_catalog.pg_class c1, pg_catalog.pg_class c2, pg_catalog.pg_index i"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  " WHERE c1.oid=i.indrelid and i.indexrelid=c2.oid"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  "       and (%d = pg_catalog.length('%s'))"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "       and pg_catalog.quote_ident(c1.relname)='%s'"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  "       and pg_catalog.pg_table_is_visible(c2.oid)"

                                                                           
#define Query_for_constraint_of_table                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  "SELECT pg_catalog.quote_ident(conname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "  FROM pg_catalog.pg_class c1, pg_catalog.pg_constraint con "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  " WHERE c1.oid=conrelid and (%d = pg_catalog.length('%s'))"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "       and pg_catalog.quote_ident(c1.relname)='%s'"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  "       and pg_catalog.pg_table_is_visible(c1.oid)"

#define Query_for_all_table_constraints                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  "SELECT pg_catalog.quote_ident(conname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "  FROM pg_catalog.pg_constraint c "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  " WHERE c.conrelid <> 0 "

                                                                           
#define Query_for_constraint_of_type                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  "SELECT pg_catalog.quote_ident(conname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "  FROM pg_catalog.pg_type t, pg_catalog.pg_constraint con "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  " WHERE t.oid=contypid and (%d = pg_catalog.length('%s'))"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "       and pg_catalog.quote_ident(t.typname)='%s'"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  "       and pg_catalog.pg_type_is_visible(t.oid)"

                                                                           
#define Query_for_list_of_tables_for_constraint                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  "SELECT pg_catalog.quote_ident(relname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "  FROM pg_catalog.pg_class"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  " WHERE (%d = pg_catalog.length('%s'))"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  "   AND oid IN "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  "       (SELECT conrelid FROM pg_catalog.pg_constraint "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  "         WHERE pg_catalog.quote_ident(conname)='%s')"

                                                                           
#define Query_for_rule_of_table                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  "SELECT pg_catalog.quote_ident(rulename) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "  FROM pg_catalog.pg_class c1, pg_catalog.pg_rewrite "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  " WHERE c1.oid=ev_class and (%d = pg_catalog.length('%s'))"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "       and pg_catalog.quote_ident(c1.relname)='%s'"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  "       and pg_catalog.pg_table_is_visible(c1.oid)"

                                                                           
#define Query_for_list_of_tables_for_rule                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  "SELECT pg_catalog.quote_ident(relname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "  FROM pg_catalog.pg_class"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  " WHERE (%d = pg_catalog.length('%s'))"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  "   AND oid IN "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  "       (SELECT ev_class FROM pg_catalog.pg_rewrite "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  "         WHERE pg_catalog.quote_ident(rulename)='%s')"

                                                                           
#define Query_for_trigger_of_table                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  "SELECT pg_catalog.quote_ident(tgname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  "  FROM pg_catalog.pg_class c1, pg_catalog.pg_trigger "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  " WHERE c1.oid=tgrelid and (%d = pg_catalog.length('%s'))"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "       and pg_catalog.quote_ident(c1.relname)='%s'"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  "       and pg_catalog.pg_table_is_visible(c1.oid)"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  "       and not tgisinternal"

                                                                           
#define Query_for_list_of_tables_for_trigger                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "SELECT pg_catalog.quote_ident(relname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "  FROM pg_catalog.pg_class"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  " WHERE (%d = pg_catalog.length('%s'))"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  "   AND oid IN "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  "       (SELECT tgrelid FROM pg_catalog.pg_trigger "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  "         WHERE pg_catalog.quote_ident(tgname)='%s')"

#define Query_for_list_of_ts_configurations                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "SELECT pg_catalog.quote_ident(cfgname) FROM pg_catalog.pg_ts_config "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  " WHERE substring(pg_catalog.quote_ident(cfgname),1,%d)='%s'"

#define Query_for_list_of_ts_dictionaries                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  "SELECT pg_catalog.quote_ident(dictname) FROM pg_catalog.pg_ts_dict "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  " WHERE substring(pg_catalog.quote_ident(dictname),1,%d)='%s'"

#define Query_for_list_of_ts_parsers                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  "SELECT pg_catalog.quote_ident(prsname) FROM pg_catalog.pg_ts_parser "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  " WHERE substring(pg_catalog.quote_ident(prsname),1,%d)='%s'"

#define Query_for_list_of_ts_templates                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  "SELECT pg_catalog.quote_ident(tmplname) FROM pg_catalog.pg_ts_template "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  " WHERE substring(pg_catalog.quote_ident(tmplname),1,%d)='%s'"

#define Query_for_list_of_fdws                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  " SELECT pg_catalog.quote_ident(fdwname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "   FROM pg_catalog.pg_foreign_data_wrapper "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  "  WHERE substring(pg_catalog.quote_ident(fdwname),1,%d)='%s'"

#define Query_for_list_of_servers                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  " SELECT pg_catalog.quote_ident(srvname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "   FROM pg_catalog.pg_foreign_server "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  "  WHERE substring(pg_catalog.quote_ident(srvname),1,%d)='%s'"

#define Query_for_list_of_user_mappings                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  " SELECT pg_catalog.quote_ident(usename) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "   FROM pg_catalog.pg_user_mappings "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  "  WHERE substring(pg_catalog.quote_ident(usename),1,%d)='%s'"

#define Query_for_list_of_access_methods                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  " SELECT pg_catalog.quote_ident(amname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "   FROM pg_catalog.pg_am "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "  WHERE substring(pg_catalog.quote_ident(amname),1,%d)='%s'"

#define Query_for_list_of_index_access_methods                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  " SELECT pg_catalog.quote_ident(amname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "   FROM pg_catalog.pg_am "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "  WHERE substring(pg_catalog.quote_ident(amname),1,%d)='%s' AND "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  "   amtype=" CppAsString2(AMTYPE_INDEX)

#define Query_for_list_of_table_access_methods                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  " SELECT pg_catalog.quote_ident(amname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "   FROM pg_catalog.pg_am "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "  WHERE substring(pg_catalog.quote_ident(amname),1,%d)='%s' AND "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  "   amtype=" CppAsString2(AMTYPE_TABLE)

                                                                           
#define Query_for_list_of_arguments                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  "SELECT pg_catalog.oidvectortypes(proargtypes)||')' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  "  FROM pg_catalog.pg_proc "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  " WHERE (%d = pg_catalog.length('%s'))"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  "   AND (pg_catalog.quote_ident(proname)='%s'"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  "        OR '\"' || proname || '\"'='%s') "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "   AND (pg_catalog.pg_function_is_visible(pg_proc.oid))"

                                                                           
#define Query_for_list_of_arguments_with_schema                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  "SELECT pg_catalog.oidvectortypes(proargtypes)||')' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  "  FROM pg_catalog.pg_proc p, pg_catalog.pg_namespace n "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  " WHERE (%d = pg_catalog.length('%s'))"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  "   AND n.oid = p.pronamespace "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  "   AND (pg_catalog.quote_ident(proname)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  "        OR '\"' || proname || '\"' ='%s') "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  "   AND (pg_catalog.quote_ident(nspname)='%s' "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  "        OR '\"' || nspname || '\"' ='%s') "

#define Query_for_list_of_extensions                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  " SELECT pg_catalog.quote_ident(extname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "   FROM pg_catalog.pg_extension "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  "  WHERE substring(pg_catalog.quote_ident(extname),1,%d)='%s'"

#define Query_for_list_of_available_extensions                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  " SELECT pg_catalog.quote_ident(name) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  "   FROM pg_catalog.pg_available_extensions "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  "  WHERE substring(pg_catalog.quote_ident(name),1,%d)='%s' AND installed_version IS NULL"

                                                                           
#define Query_for_list_of_available_extension_versions                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  " SELECT pg_catalog.quote_ident(version) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "   FROM pg_catalog.pg_available_extension_versions "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  "  WHERE (%d = pg_catalog.length('%s'))"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  "    AND pg_catalog.quote_ident(name)='%s'"

                                                                           
#define Query_for_list_of_available_extension_versions_with_TO                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  " SELECT 'TO ' || pg_catalog.quote_ident(version) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  "   FROM pg_catalog.pg_available_extension_versions "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  "  WHERE (%d = pg_catalog.length('%s'))"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  "    AND pg_catalog.quote_ident(name)='%s'"

#define Query_for_list_of_prepared_statements                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  " SELECT pg_catalog.quote_ident(name) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  "   FROM pg_catalog.pg_prepared_statements "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  "  WHERE substring(pg_catalog.quote_ident(name),1,%d)='%s'"

#define Query_for_list_of_event_triggers                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  " SELECT pg_catalog.quote_ident(evtname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "   FROM pg_catalog.pg_event_trigger "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  "  WHERE substring(pg_catalog.quote_ident(evtname),1,%d)='%s'"

#define Query_for_list_of_tablesample_methods                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  " SELECT pg_catalog.quote_ident(proname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "   FROM pg_catalog.pg_proc "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  "  WHERE prorettype = 'pg_catalog.tsm_handler'::pg_catalog.regtype AND "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  "        proargtypes[0] = 'pg_catalog.internal'::pg_catalog.regtype AND "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "        substring(pg_catalog.quote_ident(proname),1,%d)='%s'"

#define Query_for_list_of_policies                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  " SELECT pg_catalog.quote_ident(polname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "   FROM pg_catalog.pg_policy "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  "  WHERE substring(pg_catalog.quote_ident(polname),1,%d)='%s'"

#define Query_for_list_of_tables_for_policy                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "SELECT pg_catalog.quote_ident(relname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  "  FROM pg_catalog.pg_class"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  " WHERE (%d = pg_catalog.length('%s'))"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  "   AND oid IN "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  "       (SELECT polrelid FROM pg_catalog.pg_policy "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  "         WHERE pg_catalog.quote_ident(polname)='%s')"

#define Query_for_enum                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  " SELECT name FROM ( "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  "   SELECT pg_catalog.quote_ident(pg_catalog.unnest(enumvals)) AS name "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  "     FROM pg_catalog.pg_settings "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  "    WHERE pg_catalog.lower(name)=pg_catalog.lower('%s') "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  "    UNION ALL "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  "   SELECT 'DEFAULT' ) ss "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "  WHERE pg_catalog.substring(name,1,%%d)='%%s'"

                                                                           
#define Query_for_partition_of_table                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  "SELECT pg_catalog.quote_ident(c2.relname) "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  "  FROM pg_catalog.pg_class c1, pg_catalog.pg_class c2, pg_catalog.pg_inherits i"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  " WHERE c1.oid=i.inhparent and i.inhrelid=c2.oid"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  "       and (%d = pg_catalog.length('%s'))"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  "       and pg_catalog.quote_ident(c1.relname)='%s'"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  "       and pg_catalog.pg_table_is_visible(c2.oid)"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  "       and c2.relispartition = 'true'"

   
                                                                       
                                                                         
                                                           
   

static const VersionedQuery Query_for_list_of_publications[] = {{100000, " SELECT pg_catalog.quote_ident(pubname) "
                                                                         "   FROM pg_catalog.pg_publication "
                                                                         "  WHERE substring(pg_catalog.quote_ident(pubname),1,%d)='%s'"},
    {0, NULL}};

static const VersionedQuery Query_for_list_of_subscriptions[] = {{100000, " SELECT pg_catalog.quote_ident(s.subname) "
                                                                          "   FROM pg_catalog.pg_subscription s, pg_catalog.pg_database d "
                                                                          "  WHERE substring(pg_catalog.quote_ident(s.subname),1,%d)='%s' "
                                                                          "    AND d.datname = pg_catalog.current_database() "
                                                                          "    AND s.subdbid = d.oid"},
    {0, NULL}};

   
                                                                              
                                                          
   

typedef struct
{
  const char *name;
  const char *query;                                       
  const VersionedQuery *vquery;                               
  const SchemaQuery *squery;                               
  const bits32 flags;                                            
} pgsql_thing_t;

#define THING_NO_CREATE (1 << 0)                                      
#define THING_NO_DROP (1 << 1)                                      
#define THING_NO_ALTER (1 << 2)                                      
#define THING_NO_SHOW (THING_NO_CREATE | THING_NO_DROP | THING_NO_ALTER)

static const pgsql_thing_t words_after_create[] = {
    {"ACCESS METHOD", NULL, NULL, NULL, THING_NO_ALTER}, {"AGGREGATE", NULL, NULL, Query_for_list_of_aggregates}, {"CAST", NULL, NULL, NULL},                                                
                                                                                                                                                           
    {"COLLATION", "SELECT pg_catalog.quote_ident(collname) FROM pg_catalog.pg_collation WHERE collencoding IN (-1, pg_catalog.pg_char_to_encoding(pg_catalog.getdatabaseencoding())) AND substring(pg_catalog.quote_ident(collname),1,%d)='%s'"},

       
                                                                              
                                   
       
    {"CONFIGURATION", Query_for_list_of_ts_configurations, NULL, NULL, THING_NO_SHOW}, {"CONVERSION", "SELECT pg_catalog.quote_ident(conname) FROM pg_catalog.pg_conversion WHERE substring(pg_catalog.quote_ident(conname),1,%d)='%s'"}, {"DATABASE", Query_for_list_of_databases}, {"DEFAULT PRIVILEGES", NULL, NULL, NULL, THING_NO_CREATE | THING_NO_DROP}, {"DICTIONARY", Query_for_list_of_ts_dictionaries, NULL, NULL, THING_NO_SHOW}, {"DOMAIN", NULL, NULL, &Query_for_list_of_domains}, {"EVENT TRIGGER", NULL, NULL, NULL}, {"EXTENSION", Query_for_list_of_extensions}, {"FOREIGN DATA WRAPPER", NULL, NULL, NULL}, {"FOREIGN TABLE", NULL, NULL, NULL}, {"FUNCTION", NULL, NULL, Query_for_list_of_functions}, {"GROUP", Query_for_list_of_roles}, {"INDEX", NULL, NULL, &Query_for_list_of_indexes}, {"LANGUAGE", Query_for_list_of_languages}, {"LARGE OBJECT", NULL, NULL, NULL, THING_NO_CREATE | THING_NO_DROP}, {"MATERIALIZED VIEW", NULL, NULL, &Query_for_list_of_matviews},
    {"OPERATOR", NULL, NULL, NULL},                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
    {"OWNED", NULL, NULL, NULL, THING_NO_CREATE | THING_NO_ALTER},                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          
    {"PARSER", Query_for_list_of_ts_parsers, NULL, NULL, THING_NO_SHOW}, {"POLICY", NULL, NULL, NULL}, {"PROCEDURE", NULL, NULL, Query_for_list_of_procedures}, {"PUBLICATION", NULL, Query_for_list_of_publications}, {"ROLE", Query_for_list_of_roles}, {"ROUTINE", NULL, NULL, &Query_for_list_of_routines, THING_NO_CREATE}, {"RULE", "SELECT pg_catalog.quote_ident(rulename) FROM pg_catalog.pg_rules WHERE substring(pg_catalog.quote_ident(rulename),1,%d)='%s'"}, {"SCHEMA", Query_for_list_of_schemas}, {"SEQUENCE", NULL, NULL, &Query_for_list_of_sequences}, {"SERVER", Query_for_list_of_servers}, {"STATISTICS", NULL, NULL, &Query_for_list_of_statistics}, {"SUBSCRIPTION", NULL, Query_for_list_of_subscriptions}, {"SYSTEM", NULL, NULL, NULL, THING_NO_CREATE | THING_NO_DROP}, {"TABLE", NULL, NULL, &Query_for_list_of_tables}, {"TABLESPACE", Query_for_list_of_tablespaces}, {"TEMP", NULL, NULL, NULL, THING_NO_DROP | THING_NO_ALTER},                          
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          
    {"TEMPLATE", Query_for_list_of_ts_templates, NULL, NULL, THING_NO_SHOW}, {"TEMPORARY", NULL, NULL, NULL, THING_NO_DROP | THING_NO_ALTER},                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                
    {"TEXT SEARCH", NULL, NULL, NULL}, {"TRANSFORM", NULL, NULL, NULL}, {"TRIGGER", "SELECT pg_catalog.quote_ident(tgname) FROM pg_catalog.pg_trigger WHERE substring(pg_catalog.quote_ident(tgname),1,%d)='%s' AND NOT tgisinternal"}, {"TYPE", NULL, NULL, &Query_for_list_of_datatypes}, {"UNIQUE", NULL, NULL, NULL, THING_NO_DROP | THING_NO_ALTER},                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                
    {"UNLOGGED", NULL, NULL, NULL, THING_NO_DROP | THING_NO_ALTER},                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                
    {"USER", Query_for_list_of_roles " UNION SELECT 'MAPPING FOR'"}, {"USER MAPPING FOR", NULL, NULL, NULL}, {"VIEW", NULL, NULL, &Query_for_list_of_views}, {NULL}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               
};

                                                         
static const char *const table_storage_parameters[] = {"autovacuum_analyze_scale_factor", "autovacuum_analyze_threshold", "autovacuum_enabled", "autovacuum_freeze_max_age", "autovacuum_freeze_min_age", "autovacuum_freeze_table_age", "autovacuum_multixact_freeze_max_age", "autovacuum_multixact_freeze_min_age", "autovacuum_multixact_freeze_table_age", "autovacuum_vacuum_cost_delay", "autovacuum_vacuum_cost_limit", "autovacuum_vacuum_scale_factor", "autovacuum_vacuum_threshold", "fillfactor", "log_autovacuum_min_duration", "parallel_workers", "toast.autovacuum_enabled", "toast.autovacuum_freeze_max_age", "toast.autovacuum_freeze_min_age", "toast.autovacuum_freeze_table_age", "toast.autovacuum_multixact_freeze_max_age", "toast.autovacuum_multixact_freeze_min_age", "toast.autovacuum_multixact_freeze_table_age", "toast.autovacuum_vacuum_cost_delay", "toast.autovacuum_vacuum_cost_limit", "toast.autovacuum_vacuum_scale_factor", "toast.autovacuum_vacuum_threshold",
    "toast.log_autovacuum_min_duration", "toast.vacuum_index_cleanup", "toast.vacuum_truncate", "toast_tuple_target", "user_catalog_table", "vacuum_index_cleanup", "vacuum_truncate", NULL};

                                      
static char **
psql_completion(const char *text, int start, int end);
static char *
create_command_generator(const char *text, int state);
static char *
drop_command_generator(const char *text, int state);
static char *
alter_command_generator(const char *text, int state);
static char *
complete_from_query(const char *text, int state);
static char *
complete_from_versioned_query(const char *text, int state);
static char *
complete_from_schema_query(const char *text, int state);
static char *
complete_from_versioned_schema_query(const char *text, int state);
static char *
_complete_from_query(const char *simple_query, const SchemaQuery *schema_query, const char *text, int state);
static char *
complete_from_list(const char *text, int state);
static char *
complete_from_const(const char *text, int state);
static void
append_variable_names(char ***varnames, int *nvars, int *maxvars, const char *varname, const char *prefix, const char *suffix);
static char **
complete_from_variables(const char *text, const char *prefix, const char *suffix, bool need_value);
static char *
complete_from_files(const char *text, int state);

static char *
pg_strdup_keyword_case(const char *s, const char *ref);
static char *
escape_string(const char *text);
static PGresult *
exec_query(const char *query);

static char **
get_previous_words(int point, char **buffer, int *nwords);

static char *
get_guctype(const char *varname);

#ifdef NOT_USED
static char *
quote_file_name(char *text, int match_type, char *quote_pointer);
static char *
dequote_file_name(char *text, char quote_char);
#endif

   
                                                     
   
void
initialize_readline(void)
{
  rl_readline_name = (char *)pset.progname;
  rl_attempted_completion_function = psql_completion;

  rl_basic_word_break_characters = WORD_BREAKS;

  completion_max_records = 1000;

     
                                                                           
                                  
     
}

   
                                                                          
                                                         
   
                                                               
                                                                              
                                                                        
                                                                            
                                                                     
   
                                                                              
                                                                           
                                          
   
#define MatchAny NULL
#define MatchAnyExcept(pattern) ("!" pattern)

static bool
word_matches(const char *pattern, const char *word, bool case_sensitive)
{
  size_t wordlen;

#define cimatch(s1, s2, n) (case_sensitive ? strncmp(s1, s2, n) == 0 : pg_strncasecmp(s1, s2, n) == 0)

                                      
  if (pattern == NULL)
  {
    return true;
  }

                                                              
  if (*pattern == '!')
  {
    return !word_matches(pattern + 1, word, case_sensitive);
  }

                                                      
  wordlen = strlen(word);
  for (;;)
  {
    const char *star = NULL;
    const char *c;

                                                                    
    c = pattern;
    while (*c != '\0' && *c != '|')
    {
      if (*c == '*')
      {
        star = c;
      }
      c++;
    }
                                
    if (star)
    {
                                
      size_t beforelen = star - pattern, afterlen = c - star - 1;

      if (wordlen >= (beforelen + afterlen) && cimatch(word, pattern, beforelen) && cimatch(word + wordlen - afterlen, star + 1, afterlen))
      {
        return true;
      }
    }
    else
    {
                            
      if (wordlen == (c - pattern) && cimatch(word, pattern, wordlen))
      {
        return true;
      }
    }
                              
    if (*c == '\0')
    {
      break;
    }
                                     
    pattern = c + 1;
  }

  return false;
}

   
                                                                               
                                                   
   
                                                              
                                                                          
   
static bool
TailMatchesImpl(bool case_sensitive, int previous_words_count, char **previous_words, int narg, ...)
{
  va_list args;

  if (previous_words_count < narg)
  {
    return false;
  }

  va_start(args, narg);

  for (int argno = 0; argno < narg; argno++)
  {
    const char *arg = va_arg(args, const char *);

    if (!word_matches(arg, previous_words[narg - argno - 1], case_sensitive))
    {
      va_end(args);
      return false;
    }
  }

  va_end(args);

  return true;
}

   
                                                                       
                                                   
   
static bool
MatchesImpl(bool case_sensitive, int previous_words_count, char **previous_words, int narg, ...)
{
  va_list args;

  if (previous_words_count != narg)
  {
    return false;
  }

  va_start(args, narg);

  for (int argno = 0; argno < narg; argno++)
  {
    const char *arg = va_arg(args, const char *);

    if (!word_matches(arg, previous_words[narg - argno - 1], case_sensitive))
    {
      va_end(args);
      return false;
    }
  }

  va_end(args);

  return true;
}

   
                                                                          
                                                         
   
static bool
HeadMatchesImpl(bool case_sensitive, int previous_words_count, char **previous_words, int narg, ...)
{
  va_list args;

  if (previous_words_count < narg)
  {
    return false;
  }

  va_start(args, narg);

  for (int argno = 0; argno < narg; argno++)
  {
    const char *arg = va_arg(args, const char *);

    if (!word_matches(arg, previous_words[previous_words_count - argno - 1], case_sensitive))
    {
      va_end(args);
      return false;
    }
  }

  va_end(args);

  return true;
}

   
                                               
   
static bool
ends_with(const char *s, char c)
{
  size_t length = strlen(s);

  return (length > 0 && s[length - 1] == c);
}

   
                            
   
                                                                               
                                                                            
                                                                     
                                                                      
   
static char **
psql_completion(const char *text, int start, int end)
{
                                          
  char **matches = NULL;

                                   
  char *words_buffer;

                                                         
  char **previous_words;

                                                    
  int previous_words_count;

     
                                                                         
                                                                            
                                                                            
                                                                        
                                            
     
#define prev_wd (previous_words[0])
#define prev2_wd (previous_words[1])
#define prev3_wd (previous_words[2])
#define prev4_wd (previous_words[3])
#define prev5_wd (previous_words[4])
#define prev6_wd (previous_words[5])
#define prev7_wd (previous_words[6])
#define prev8_wd (previous_words[7])
#define prev9_wd (previous_words[8])

                                                                
#define TailMatches(...) TailMatchesImpl(false, previous_words_count, previous_words, VA_ARGS_NARGS(__VA_ARGS__), __VA_ARGS__)

                                                              
#define TailMatchesCS(...) TailMatchesImpl(true, previous_words_count, previous_words, VA_ARGS_NARGS(__VA_ARGS__), __VA_ARGS__)

                                                                       
#define Matches(...) MatchesImpl(false, previous_words_count, previous_words, VA_ARGS_NARGS(__VA_ARGS__), __VA_ARGS__)

                                                                     
#define MatchesCS(...) MatchesImpl(true, previous_words_count, previous_words, VA_ARGS_NARGS(__VA_ARGS__), __VA_ARGS__)

                                                                
#define HeadMatches(...) HeadMatchesImpl(false, previous_words_count, previous_words, VA_ARGS_NARGS(__VA_ARGS__), __VA_ARGS__)

                                                              
#define HeadMatchesCS(...) HeadMatchesImpl(true, previous_words_count, previous_words, VA_ARGS_NARGS(__VA_ARGS__), __VA_ARGS__)

                                        
  static const char *const sql_commands[] = {"ABORT", "ALTER", "ANALYZE", "BEGIN", "CALL", "CHECKPOINT", "CLOSE", "CLUSTER", "COMMENT", "COMMIT", "COPY", "CREATE", "DEALLOCATE", "DECLARE", "DELETE FROM", "DISCARD", "DO", "DROP", "END", "EXECUTE", "EXPLAIN", "FETCH", "GRANT", "IMPORT", "INSERT", "LISTEN", "LOAD", "LOCK", "MOVE", "NOTIFY", "PREPARE", "REASSIGN", "REFRESH MATERIALIZED VIEW", "REINDEX", "RELEASE", "RESET", "REVOKE", "ROLLBACK", "SAVEPOINT", "SECURITY LABEL", "SELECT", "SET", "SHOW", "START", "TABLE", "TRUNCATE", "UNLISTEN", "UPDATE", "VACUUM", "VALUES", "WITH", NULL};

                                  
  static const char *const backslash_commands[] = {"\\a", "\\connect", "\\conninfo", "\\C", "\\cd", "\\copy", "\\copyright", "\\crosstabview", "\\d", "\\da", "\\dA", "\\db", "\\dc", "\\dC", "\\dd", "\\ddp", "\\dD", "\\des", "\\det", "\\deu", "\\dew", "\\dE", "\\df", "\\dF", "\\dFd", "\\dFp", "\\dFt", "\\dg", "\\di", "\\dl", "\\dL", "\\dm", "\\dn", "\\do", "\\dO", "\\dp", "\\dP", "\\dPi", "\\dPt", "\\drds", "\\dRs", "\\dRp", "\\ds", "\\dS", "\\dt", "\\dT", "\\dv", "\\du", "\\dx", "\\dy", "\\e", "\\echo", "\\ef", "\\elif", "\\else", "\\encoding", "\\endif", "\\errverbose", "\\ev", "\\f", "\\g", "\\gdesc", "\\gexec", "\\gset", "\\gx", "\\h", "\\help", "\\H", "\\i", "\\if", "\\ir", "\\l", "\\lo_import", "\\lo_export", "\\lo_list", "\\lo_unlink", "\\o", "\\p", "\\password", "\\prompt", "\\pset", "\\q", "\\qecho", "\\r", "\\s", "\\set", "\\setenv", "\\sf", "\\sv", "\\t", "\\T", "\\timing", "\\unset", "\\x", "\\w", "\\watch", "\\z", "\\!", "\\?", NULL};

  (void)end;                        

#ifdef HAVE_RL_COMPLETION_APPEND_CHARACTER
  rl_completion_append_character = ' ';
#endif

                           
  completion_charp = NULL;
  completion_charpp = NULL;
  completion_info_charp = NULL;
  completion_info_charp2 = NULL;

     
                                                                           
                                                                            
                                 
     
  previous_words = get_previous_words(start, &words_buffer, &previous_words_count);

                                                                          
  if (text[0] == '\\')
  {
    COMPLETE_WITH_LIST_CS(backslash_commands);
  }

                                                                     
  else if (text[0] == ':' && text[1] != ':')
  {
    if (text[1] == '\'')
    {
      matches = complete_from_variables(text, ":'", "'", true);
    }
    else if (text[1] == '"')
    {
      matches = complete_from_variables(text, ":\"", "\"", true);
    }
    else
    {
      matches = complete_from_variables(text, ":", "", true);
    }
  }

                                                                  
  else if (previous_words_count == 0)
  {
    COMPLETE_WITH_LIST(sql_commands);
  }

              
                                              
  else if (TailMatches("CREATE"))
  {
    matches = completion_matches(text, create_command_generator);
  }

                                                     
                                            
  else if (Matches("DROP"))
  {
    matches = completion_matches(text, drop_command_generator);
  }

             

                   
  else if (Matches("ALTER", "TABLE"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, "UNION SELECT 'ALL IN TABLESPACE'");
  }

                       
  else if (Matches("ALTER"))
  {
    matches = completion_matches(text, alter_command_generator);
  }
                                                                 
  else if (TailMatches("ALL", "IN", "TABLESPACE", MatchAny))
  {
    COMPLETE_WITH("SET TABLESPACE", "OWNED BY");
  }
                                                                          
  else if (TailMatches("ALL", "IN", "TABLESPACE", MatchAny, "OWNED", "BY"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_roles);
  }
                                                                              
  else if (TailMatches("ALL", "IN", "TABLESPACE", MatchAny, "OWNED", "BY", MatchAny))
  {
    COMPLETE_WITH("SET TABLESPACE");
  }
                                                         
  else if (Matches("ALTER", "AGGREGATE|FUNCTION|PROCEDURE|ROUTINE", MatchAny))
  {
    COMPLETE_WITH("(");
  }
                                                               
  else if (Matches("ALTER", "AGGREGATE|FUNCTION|PROCEDURE|ROUTINE", MatchAny, MatchAny))
  {
    if (ends_with(prev_wd, ')'))
    {
      COMPLETE_WITH("OWNER TO", "RENAME TO", "SET SCHEMA");
    }
    else
    {
      COMPLETE_WITH_FUNCTION_ARG(prev2_wd);
    }
  }
                                
  else if (Matches("ALTER", "PUBLICATION", MatchAny))
  {
    COMPLETE_WITH("ADD TABLE", "DROP TABLE", "OWNER TO", "RENAME TO", "SET");
  }
                                    
  else if (Matches("ALTER", "PUBLICATION", MatchAny, "SET"))
  {
    COMPLETE_WITH("(", "TABLE");
  }
                                      
  else if (HeadMatches("ALTER", "PUBLICATION", MatchAny) && TailMatches("SET", "("))
  {
    COMPLETE_WITH("publish");
  }
                                 
  else if (Matches("ALTER", "SUBSCRIPTION", MatchAny))
  {
    COMPLETE_WITH("CONNECTION", "ENABLE", "DISABLE", "OWNER TO", "RENAME TO", "REFRESH PUBLICATION", "SET");
  }
                                                     
  else if (HeadMatches("ALTER", "SUBSCRIPTION", MatchAny) && TailMatches("REFRESH", "PUBLICATION"))
  {
    COMPLETE_WITH("WITH (");
  }
                                                            
  else if (HeadMatches("ALTER", "SUBSCRIPTION", MatchAny) && TailMatches("REFRESH", "PUBLICATION", "WITH", "("))
  {
    COMPLETE_WITH("copy_data");
  }
                                     
  else if (Matches("ALTER", "SUBSCRIPTION", MatchAny, "SET"))
  {
    COMPLETE_WITH("(", "PUBLICATION");
  }
                                       
  else if (HeadMatches("ALTER", "SUBSCRIPTION", MatchAny) && TailMatches("SET", "("))
  {
    COMPLETE_WITH("slot_name", "synchronous_commit");
  }
                                                 
  else if (HeadMatches("ALTER", "SUBSCRIPTION", MatchAny) && TailMatches("SET", "PUBLICATION"))
  {
                                                                          
  }
                                                        
  else if (HeadMatches("ALTER", "SUBSCRIPTION", MatchAny) && TailMatches("SET", "PUBLICATION", MatchAny))
  {
    COMPLETE_WITH("WITH (");
  }
                                                               
  else if (HeadMatches("ALTER", "SUBSCRIPTION", MatchAny) && TailMatches("SET", "PUBLICATION", MatchAny, "WITH", "("))
  {
    COMPLETE_WITH("copy_data", "refresh");
  }
                           
  else if (Matches("ALTER", "SCHEMA", MatchAny))
  {
    COMPLETE_WITH("OWNER TO", "RENAME TO");
  }

                              
  else if (Matches("ALTER", "COLLATION", MatchAny))
  {
    COMPLETE_WITH("OWNER TO", "RENAME TO", "SET SCHEMA");
  }

                               
  else if (Matches("ALTER", "CONVERSION", MatchAny))
  {
    COMPLETE_WITH("OWNER TO", "RENAME TO", "SET SCHEMA");
  }

                             
  else if (Matches("ALTER", "DATABASE", MatchAny))
  {
    COMPLETE_WITH("RESET", "SET", "OWNER TO", "RENAME TO", "IS_TEMPLATE", "ALLOW_CONNECTIONS", "CONNECTION LIMIT");
  }

                                            
  else if (Matches("ALTER", "DATABASE", MatchAny, "SET", "TABLESPACE"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_tablespaces);
  }

                           
  else if (Matches("ALTER", "EVENT", "TRIGGER"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_event_triggers);
  }

                                  
  else if (Matches("ALTER", "EVENT", "TRIGGER", MatchAny))
  {
    COMPLETE_WITH("DISABLE", "ENABLE", "OWNER TO", "RENAME TO");
  }

                                         
  else if (Matches("ALTER", "EVENT", "TRIGGER", MatchAny, "ENABLE"))
  {
    COMPLETE_WITH("REPLICA", "ALWAYS");
  }

                              
  else if (Matches("ALTER", "EXTENSION", MatchAny))
  {
    COMPLETE_WITH("ADD", "DROP", "UPDATE", "SET SCHEMA");
  }

                                     
  else if (Matches("ALTER", "EXTENSION", MatchAny, "UPDATE"))
  {
    completion_info_charp = prev2_wd;
    COMPLETE_WITH_QUERY(Query_for_list_of_available_extension_versions_with_TO);
  }

                                        
  else if (Matches("ALTER", "EXTENSION", MatchAny, "UPDATE", "TO"))
  {
    completion_info_charp = prev3_wd;
    COMPLETE_WITH_QUERY(Query_for_list_of_available_extension_versions);
  }

                     
  else if (Matches("ALTER", "FOREIGN"))
  {
    COMPLETE_WITH("DATA WRAPPER", "TABLE");
  }

                                         
  else if (Matches("ALTER", "FOREIGN", "DATA", "WRAPPER", MatchAny))
  {
    COMPLETE_WITH("HANDLER", "VALIDATOR", "OPTIONS", "OWNER TO", "RENAME TO");
  }

                                  
  else if (Matches("ALTER", "FOREIGN", "TABLE", MatchAny))
  {
    COMPLETE_WITH("ADD", "ALTER", "DISABLE TRIGGER", "DROP", "ENABLE", "INHERIT", "NO INHERIT", "OPTIONS", "OWNER TO", "RENAME", "SET", "VALIDATE CONSTRAINT");
  }

                   
  else if (Matches("ALTER", "INDEX"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_indexes, "UNION SELECT 'ALL IN TABLESPACE'");
  }
                          
  else if (Matches("ALTER", "INDEX", MatchAny))
  {
    COMPLETE_WITH("ALTER COLUMN", "OWNER TO", "RENAME TO", "SET", "RESET", "ATTACH PARTITION");
  }
  else if (Matches("ALTER", "INDEX", MatchAny, "ATTACH"))
  {
    COMPLETE_WITH("PARTITION");
  }
  else if (Matches("ALTER", "INDEX", MatchAny, "ATTACH", "PARTITION"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_indexes, NULL);
  }
                                
  else if (Matches("ALTER", "INDEX", MatchAny, "ALTER"))
  {
    COMPLETE_WITH("COLUMN");
  }
                                       
  else if (Matches("ALTER", "INDEX", MatchAny, "ALTER", "COLUMN"))
  {
    completion_info_charp = prev3_wd;
    COMPLETE_WITH_QUERY(Query_for_list_of_attribute_numbers);
  }
                                                
  else if (Matches("ALTER", "INDEX", MatchAny, "ALTER", "COLUMN", MatchAny))
  {
    COMPLETE_WITH("SET STATISTICS");
  }
                                                    
  else if (Matches("ALTER", "INDEX", MatchAny, "ALTER", "COLUMN", MatchAny, "SET"))
  {
    COMPLETE_WITH("STATISTICS");
  }
                                                               
  else if (Matches("ALTER", "INDEX", MatchAny, "ALTER", "COLUMN", MatchAny, "SET", "STATISTICS"))
  {
                                                                       
  }
                              
  else if (Matches("ALTER", "INDEX", MatchAny, "SET"))
  {
    COMPLETE_WITH("(", "TABLESPACE");
  }
                                
  else if (Matches("ALTER", "INDEX", MatchAny, "RESET"))
  {
    COMPLETE_WITH("(");
  }
                                     
  else if (Matches("ALTER", "INDEX", MatchAny, "RESET", "("))
  {
    COMPLETE_WITH("fillfactor", "vacuum_cleanup_index_scale_factor",            
        "fastupdate", "gin_pending_list_limit",                               
        "buffering",                                                           
        "pages_per_range", "autosummarize"                                     
    );
  }
  else if (Matches("ALTER", "INDEX", MatchAny, "SET", "("))
  {
    COMPLETE_WITH("fillfactor =", "vacuum_cleanup_index_scale_factor =",            
        "fastupdate =", "gin_pending_list_limit =",                               
        "buffering =",                                                             
        "pages_per_range =", "autosummarize ="                                     
    );
  }

                             
  else if (Matches("ALTER", "LANGUAGE", MatchAny))
  {
    COMPLETE_WITH("OWNER TO", "RENAME TO");
  }

                                
  else if (Matches("ALTER", "LARGE", "OBJECT", MatchAny))
  {
    COMPLETE_WITH("OWNER TO");
  }

                               
  else if (Matches("ALTER", "MATERIALIZED", "VIEW"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_matviews, "UNION SELECT 'ALL IN TABLESPACE'");
  }

                              
  else if (Matches("ALTER", "USER|ROLE", MatchAny) && !TailMatches("USER", "MAPPING"))
  {
    COMPLETE_WITH("BYPASSRLS", "CONNECTION LIMIT", "CREATEDB", "CREATEROLE", "ENCRYPTED PASSWORD", "INHERIT", "LOGIN", "NOBYPASSRLS", "NOCREATEDB", "NOCREATEROLE", "NOINHERIT", "NOLOGIN", "NOREPLICATION", "NOSUPERUSER", "PASSWORD", "RENAME TO", "REPLICATION", "RESET", "SET", "SUPERUSER", "VALID UNTIL", "WITH");
  }

                                   
  else if (Matches("ALTER", "USER|ROLE", MatchAny, "WITH"))
  {
                                                                
    COMPLETE_WITH("BYPASSRLS", "CONNECTION LIMIT", "CREATEDB", "CREATEROLE", "ENCRYPTED PASSWORD", "INHERIT", "LOGIN", "NOBYPASSRLS", "NOCREATEDB", "NOCREATEROLE", "NOINHERIT", "NOLOGIN", "NOREPLICATION", "NOSUPERUSER", "PASSWORD", "RENAME TO", "REPLICATION", "RESET", "SET", "SUPERUSER", "VALID UNTIL");
  }

                                
  else if (Matches("ALTER", "DEFAULT", "PRIVILEGES"))
  {
    COMPLETE_WITH("FOR ROLE", "IN SCHEMA");
  }
                                    
  else if (Matches("ALTER", "DEFAULT", "PRIVILEGES", "FOR"))
  {
    COMPLETE_WITH("ROLE");
  }
                                   
  else if (Matches("ALTER", "DEFAULT", "PRIVILEGES", "IN"))
  {
    COMPLETE_WITH("SCHEMA");
  }
                                                  
  else if (Matches("ALTER", "DEFAULT", "PRIVILEGES", "FOR", "ROLE|USER", MatchAny))
  {
    COMPLETE_WITH("GRANT", "REVOKE", "IN SCHEMA");
  }
                                              
  else if (Matches("ALTER", "DEFAULT", "PRIVILEGES", "IN", "SCHEMA", MatchAny))
  {
    COMPLETE_WITH("GRANT", "REVOKE", "FOR ROLE");
  }
                                                  
  else if (Matches("ALTER", "DEFAULT", "PRIVILEGES", "IN", "SCHEMA", MatchAny, "FOR"))
  {
    COMPLETE_WITH("ROLE");
  }
                                                                
                                                                
  else if (Matches("ALTER", "DEFAULT", "PRIVILEGES", "FOR", "ROLE|USER", MatchAny, "IN", "SCHEMA", MatchAny) || Matches("ALTER", "DEFAULT", "PRIVILEGES", "IN", "SCHEMA", MatchAny, "FOR", "ROLE|USER", MatchAny))
  {
    COMPLETE_WITH("GRANT", "REVOKE");
  }
                           
  else if (Matches("ALTER", "DOMAIN", MatchAny))
  {
    COMPLETE_WITH("ADD", "DROP", "OWNER TO", "RENAME", "SET", "VALIDATE CONSTRAINT");
  }
                               
  else if (Matches("ALTER", "DOMAIN", MatchAny, "DROP"))
  {
    COMPLETE_WITH("CONSTRAINT", "DEFAULT", "NOT NULL");
  }
                                                          
  else if (Matches("ALTER", "DOMAIN", MatchAny, "DROP|RENAME|VALIDATE", "CONSTRAINT"))
  {
    completion_info_charp = prev3_wd;
    COMPLETE_WITH_QUERY(Query_for_constraint_of_type);
  }
                                 
  else if (Matches("ALTER", "DOMAIN", MatchAny, "RENAME"))
  {
    COMPLETE_WITH("CONSTRAINT", "TO");
  }
                                                  
  else if (Matches("ALTER", "DOMAIN", MatchAny, "RENAME", "CONSTRAINT", MatchAny))
  {
    COMPLETE_WITH("TO");
  }

                              
  else if (Matches("ALTER", "DOMAIN", MatchAny, "SET"))
  {
    COMPLETE_WITH("DEFAULT", "NOT NULL", "SCHEMA");
  }
                             
  else if (Matches("ALTER", "SEQUENCE", MatchAny))
  {
    COMPLETE_WITH("INCREMENT", "MINVALUE", "MAXVALUE", "RESTART", "NO", "CACHE", "CYCLE", "SET SCHEMA", "OWNED BY", "OWNER TO", "RENAME TO");
  }
                                
  else if (Matches("ALTER", "SEQUENCE", MatchAny, "NO"))
  {
    COMPLETE_WITH("MINVALUE", "MAXVALUE", "CYCLE");
  }
                           
  else if (Matches("ALTER", "SERVER", MatchAny))
  {
    COMPLETE_WITH("VERSION", "OPTIONS", "OWNER TO", "RENAME TO");
  }
                                             
  else if (Matches("ALTER", "SERVER", MatchAny, "VERSION", MatchAny))
  {
    COMPLETE_WITH("OPTIONS");
  }
                                          
  else if (Matches("ALTER", "SYSTEM"))
  {
    COMPLETE_WITH("SET", "RESET");
  }
  else if (Matches("ALTER", "SYSTEM", "SET|RESET"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_alter_system_set_vars);
  }
  else if (Matches("ALTER", "SYSTEM", "SET", MatchAny))
  {
    COMPLETE_WITH("TO");
  }
                         
  else if (Matches("ALTER", "VIEW", MatchAny))
  {
    COMPLETE_WITH("ALTER COLUMN", "OWNER TO", "RENAME TO", "SET SCHEMA");
  }
                                      
  else if (Matches("ALTER", "MATERIALIZED", "VIEW", MatchAny))
  {
    COMPLETE_WITH("ALTER COLUMN", "OWNER TO", "RENAME TO", "SET SCHEMA");
  }

                           
  else if (Matches("ALTER", "POLICY"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_policies);
  }
                              
  else if (Matches("ALTER", "POLICY", MatchAny))
  {
    COMPLETE_WITH("ON");
  }
                                      
  else if (Matches("ALTER", "POLICY", MatchAny, "ON"))
  {
    completion_info_charp = prev2_wd;
    COMPLETE_WITH_QUERY(Query_for_list_of_tables_for_policy);
  }
                                                     
  else if (Matches("ALTER", "POLICY", MatchAny, "ON", MatchAny))
  {
    COMPLETE_WITH("RENAME TO", "TO", "USING (", "WITH CHECK (");
  }
                                                
  else if (Matches("ALTER", "POLICY", MatchAny, "ON", MatchAny, "TO"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_grant_roles);
  }
                                              
  else if (Matches("ALTER", "POLICY", MatchAny, "ON", MatchAny, "USING"))
  {
    COMPLETE_WITH("(");
  }
                                                   
  else if (Matches("ALTER", "POLICY", MatchAny, "ON", MatchAny, "WITH", "CHECK"))
  {
    COMPLETE_WITH("(");
  }

                                 
  else if (Matches("ALTER", "RULE", MatchAny))
  {
    COMPLETE_WITH("ON");
  }

                                                                       
  else if (Matches("ALTER", "RULE", MatchAny, "ON"))
  {
    completion_info_charp = prev2_wd;
    COMPLETE_WITH_QUERY(Query_for_list_of_tables_for_rule);
  }

                                   
  else if (Matches("ALTER", "RULE", MatchAny, "ON", MatchAny))
  {
    COMPLETE_WITH("RENAME TO");
  }

                               
  else if (Matches("ALTER", "STATISTICS", MatchAny))
  {
    COMPLETE_WITH("OWNER TO", "RENAME TO", "SET SCHEMA");
  }

                                    
  else if (Matches("ALTER", "TRIGGER", MatchAny))
  {
    COMPLETE_WITH("ON");
  }

  else if (Matches("ALTER", "TRIGGER", MatchAny, MatchAny))
  {
    completion_info_charp = prev2_wd;
    COMPLETE_WITH_QUERY(Query_for_list_of_tables_for_trigger);
  }

     
                                                                       
     
  else if (Matches("ALTER", "TRIGGER", MatchAny, "ON"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, NULL);
  }

                                      
  else if (Matches("ALTER", "TRIGGER", MatchAny, "ON", MatchAny))
  {
    COMPLETE_WITH("RENAME TO");
  }

     
                                                           
     
  else if (Matches("ALTER", "TABLE", MatchAny))
  {
    COMPLETE_WITH("ADD", "ALTER", "CLUSTER ON", "DISABLE", "DROP", "ENABLE", "INHERIT", "NO INHERIT", "RENAME", "RESET", "OWNER TO", "SET", "VALIDATE CONSTRAINT", "REPLICA IDENTITY", "ATTACH PARTITION", "DETACH PARTITION");
  }
                              
  else if (Matches("ALTER", "TABLE", MatchAny, "ENABLE"))
  {
    COMPLETE_WITH("ALWAYS", "REPLICA", "ROW LEVEL SECURITY", "RULE", "TRIGGER");
  }
  else if (Matches("ALTER", "TABLE", MatchAny, "ENABLE", "REPLICA|ALWAYS"))
  {
    COMPLETE_WITH("RULE", "TRIGGER");
  }
  else if (Matches("ALTER", "TABLE", MatchAny, "ENABLE", "RULE"))
  {
    completion_info_charp = prev3_wd;
    COMPLETE_WITH_QUERY(Query_for_rule_of_table);
  }
  else if (Matches("ALTER", "TABLE", MatchAny, "ENABLE", MatchAny, "RULE"))
  {
    completion_info_charp = prev4_wd;
    COMPLETE_WITH_QUERY(Query_for_rule_of_table);
  }
  else if (Matches("ALTER", "TABLE", MatchAny, "ENABLE", "TRIGGER"))
  {
    completion_info_charp = prev3_wd;
    COMPLETE_WITH_QUERY(Query_for_trigger_of_table);
  }
  else if (Matches("ALTER", "TABLE", MatchAny, "ENABLE", MatchAny, "TRIGGER"))
  {
    completion_info_charp = prev4_wd;
    COMPLETE_WITH_QUERY(Query_for_trigger_of_table);
  }
                               
  else if (Matches("ALTER", "TABLE", MatchAny, "INHERIT"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, "");
  }
                                  
  else if (Matches("ALTER", "TABLE", MatchAny, "NO", "INHERIT"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, "");
  }
                               
  else if (Matches("ALTER", "TABLE", MatchAny, "DISABLE"))
  {
    COMPLETE_WITH("ROW LEVEL SECURITY", "RULE", "TRIGGER");
  }
  else if (Matches("ALTER", "TABLE", MatchAny, "DISABLE", "RULE"))
  {
    completion_info_charp = prev3_wd;
    COMPLETE_WITH_QUERY(Query_for_rule_of_table);
  }
  else if (Matches("ALTER", "TABLE", MatchAny, "DISABLE", "TRIGGER"))
  {
    completion_info_charp = prev3_wd;
    COMPLETE_WITH_QUERY(Query_for_trigger_of_table);
  }

                             
  else if (Matches("ALTER", "TABLE", MatchAny, "ALTER"))
  {
    COMPLETE_WITH_ATTR(prev2_wd, " UNION SELECT 'COLUMN' UNION SELECT 'CONSTRAINT'");
  }

                              
  else if (Matches("ALTER", "TABLE", MatchAny, "RENAME"))
  {
    COMPLETE_WITH_ATTR(prev2_wd, " UNION SELECT 'COLUMN' UNION SELECT 'CONSTRAINT' UNION SELECT 'TO'");
  }
  else if (Matches("ALTER", "TABLE", MatchAny, "ALTER|RENAME", "COLUMN"))
  {
    COMPLETE_WITH_ATTR(prev3_wd, "");
  }

                                  
  else if (Matches("ALTER", "TABLE", MatchAny, "RENAME", MatchAnyExcept("CONSTRAINT|TO")))
  {
    COMPLETE_WITH("TO");
  }

                                                    
  else if (Matches("ALTER", "TABLE", MatchAny, "RENAME", "COLUMN|CONSTRAINT", MatchAnyExcept("TO")))
  {
    COMPLETE_WITH("TO");
  }

                                                                       
  else if (Matches("ALTER", "TABLE", MatchAny, "DROP"))
  {
    COMPLETE_WITH("COLUMN", "CONSTRAINT");
  }
                                                                         
  else if (Matches("ALTER", "TABLE", MatchAny, "DROP", "COLUMN"))
  {
    COMPLETE_WITH_ATTR(prev3_wd, "");
  }

     
                                                                         
                                 
     
  else if (Matches("ALTER", "TABLE", MatchAny, "ALTER|DROP|RENAME|VALIDATE", "CONSTRAINT"))
  {
    completion_info_charp = prev3_wd;
    COMPLETE_WITH_QUERY(Query_for_constraint_of_table);
  }
                                        
  else if (Matches("ALTER", "TABLE", MatchAny, "ALTER", "COLUMN", MatchAny) || Matches("ALTER", "TABLE", MatchAny, "ALTER", MatchAny))
  {
    COMPLETE_WITH("TYPE", "SET", "RESET", "RESTART", "ADD", "DROP");
  }
                                            
  else if (Matches("ALTER", "TABLE", MatchAny, "ALTER", "COLUMN", MatchAny, "SET") || Matches("ALTER", "TABLE", MatchAny, "ALTER", MatchAny, "SET"))
  {
    COMPLETE_WITH("(", "DEFAULT", "NOT NULL", "STATISTICS", "STORAGE");
  }
                                              
  else if (Matches("ALTER", "TABLE", MatchAny, "ALTER", "COLUMN", MatchAny, "SET", "(") || Matches("ALTER", "TABLE", MatchAny, "ALTER", MatchAny, "SET", "("))
  {
    COMPLETE_WITH("n_distinct", "n_distinct_inherited");
  }
                                                    
  else if (Matches("ALTER", "TABLE", MatchAny, "ALTER", "COLUMN", MatchAny, "SET", "STORAGE") || Matches("ALTER", "TABLE", MatchAny, "ALTER", MatchAny, "SET", "STORAGE"))
  {
    COMPLETE_WITH("PLAIN", "EXTERNAL", "EXTENDED", "MAIN");
  }
                                                       
  else if (Matches("ALTER", "TABLE", MatchAny, "ALTER", "COLUMN", MatchAny, "SET", "STATISTICS") || Matches("ALTER", "TABLE", MatchAny, "ALTER", MatchAny, "SET", "STATISTICS"))
  {
                                                                       
  }
                                             
  else if (Matches("ALTER", "TABLE", MatchAny, "ALTER", "COLUMN", MatchAny, "DROP") || Matches("ALTER", "TABLE", MatchAny, "ALTER", MatchAny, "DROP"))
  {
    COMPLETE_WITH("DEFAULT", "IDENTITY", "NOT NULL");
  }
  else if (Matches("ALTER", "TABLE", MatchAny, "CLUSTER"))
  {
    COMPLETE_WITH("ON");
  }
  else if (Matches("ALTER", "TABLE", MatchAny, "CLUSTER", "ON"))
  {
    completion_info_charp = prev3_wd;
    COMPLETE_WITH_QUERY(Query_for_index_of_table);
  }
                                                                            
  else if (Matches("ALTER", "TABLE", MatchAny, "SET"))
  {
    COMPLETE_WITH("(", "LOGGED", "SCHEMA", "TABLESPACE", "UNLOGGED", "WITH", "WITHOUT");
  }

     
                                                                   
                 
     
  else if (Matches("ALTER", "TABLE", MatchAny, "SET", "TABLESPACE"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_tablespaces);
  }
                                                                        
  else if (Matches("ALTER", "TABLE", MatchAny, "SET", "WITHOUT"))
  {
    COMPLETE_WITH("CLUSTER", "OIDS");
  }
                               
  else if (Matches("ALTER", "TABLE", MatchAny, "RESET"))
  {
    COMPLETE_WITH("(");
  }
                                     
  else if (Matches("ALTER", "TABLE", MatchAny, "SET|RESET", "("))
  {
    COMPLETE_WITH_LIST(table_storage_parameters);
  }
  else if (Matches("ALTER", "TABLE", MatchAny, "REPLICA", "IDENTITY", "USING", "INDEX"))
  {
    completion_info_charp = prev5_wd;
    COMPLETE_WITH_QUERY(Query_for_index_of_table);
  }
  else if (Matches("ALTER", "TABLE", MatchAny, "REPLICA", "IDENTITY", "USING"))
  {
    COMPLETE_WITH("INDEX");
  }
  else if (Matches("ALTER", "TABLE", MatchAny, "REPLICA", "IDENTITY"))
  {
    COMPLETE_WITH("FULL", "NOTHING", "DEFAULT", "USING");
  }
  else if (Matches("ALTER", "TABLE", MatchAny, "REPLICA"))
  {
    COMPLETE_WITH("IDENTITY");
  }

     
                                                                      
             
     
  else if (Matches("ALTER", "TABLE", MatchAny, "ATTACH", "PARTITION"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, "");
  }
                                                                    
  else if (TailMatches("ATTACH", "PARTITION", MatchAny))
  {
    COMPLETE_WITH("FOR VALUES", "DEFAULT");
  }
  else if (TailMatches("FOR", "VALUES"))
  {
    COMPLETE_WITH("FROM (", "IN (", "WITH (");
  }

     
                                                                      
                          
     
  else if (Matches("ALTER", "TABLE", MatchAny, "DETACH", "PARTITION"))
  {
    completion_info_charp = prev3_wd;
    COMPLETE_WITH_QUERY(Query_for_partition_of_table);
  }

                                                                   
  else if (Matches("ALTER", "TABLESPACE", MatchAny))
  {
    COMPLETE_WITH("RENAME TO", "OWNER TO", "SET", "RESET");
  }
                                        
  else if (Matches("ALTER", "TABLESPACE", MatchAny, "SET|RESET"))
  {
    COMPLETE_WITH("(");
  }
                                          
  else if (Matches("ALTER", "TABLESPACE", MatchAny, "SET|RESET", "("))
  {
    COMPLETE_WITH("seq_page_cost", "random_page_cost", "effective_io_concurrency");
  }

                         
  else if (Matches("ALTER", "TEXT", "SEARCH"))
  {
    COMPLETE_WITH("CONFIGURATION", "DICTIONARY", "PARSER", "TEMPLATE");
  }
  else if (Matches("ALTER", "TEXT", "SEARCH", "TEMPLATE|PARSER", MatchAny))
  {
    COMPLETE_WITH("RENAME TO", "SET SCHEMA");
  }
  else if (Matches("ALTER", "TEXT", "SEARCH", "DICTIONARY", MatchAny))
  {
    COMPLETE_WITH("OWNER TO", "RENAME TO", "SET SCHEMA");
  }
  else if (Matches("ALTER", "TEXT", "SEARCH", "CONFIGURATION", MatchAny))
  {
    COMPLETE_WITH("ADD MAPPING FOR", "ALTER MAPPING", "DROP MAPPING FOR", "OWNER TO", "RENAME TO", "SET SCHEMA");
  }

                                              
  else if (Matches("ALTER", "TYPE", MatchAny))
  {
    COMPLETE_WITH("ADD ATTRIBUTE", "ADD VALUE", "ALTER ATTRIBUTE", "DROP ATTRIBUTE", "OWNER TO", "RENAME", "SET SCHEMA");
  }
                                                  
  else if (Matches("ALTER", "TYPE", MatchAny, "ADD"))
  {
    COMPLETE_WITH("ATTRIBUTE", "VALUE");
  }
                               
  else if (Matches("ALTER", "TYPE", MatchAny, "RENAME"))
  {
    COMPLETE_WITH("ATTRIBUTE", "TO", "VALUE");
  }
                                                   
  else if (Matches("ALTER", "TYPE", MatchAny, "RENAME", "ATTRIBUTE|VALUE", MatchAny))
  {
    COMPLETE_WITH("TO");
  }

     
                                                                           
                   
     
  else if (Matches("ALTER", "TYPE", MatchAny, "ALTER|DROP|RENAME", "ATTRIBUTE"))
  {
    COMPLETE_WITH_ATTR(prev3_wd, "");
  }
                                        
  else if (Matches("ALTER", "TYPE", MatchAny, "ALTER", "ATTRIBUTE", MatchAny))
  {
    COMPLETE_WITH("TYPE");
  }
                                  
  else if (Matches("ALTER", "GROUP", MatchAny))
  {
    COMPLETE_WITH("ADD USER", "DROP USER", "RENAME TO");
  }
                                                     
  else if (Matches("ALTER", "GROUP", MatchAny, "ADD|DROP"))
  {
    COMPLETE_WITH("USER");
  }
                                                                 
  else if (Matches("ALTER", "GROUP", MatchAny, "ADD|DROP", "USER"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_roles);
  }

     
                                                                           
     
  else if (Matches("ALTER", "TYPE", MatchAny, "RENAME", "VALUE"))
  {
    COMPLETE_WITH_ENUM_VALUE(prev3_wd);
  }

     
                                                                  
                                                       
     
  else if (Matches("ANALYZE"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_analyzables, " UNION SELECT 'VERBOSE'");
  }
  else if (HeadMatches("ANALYZE", "(*") && !HeadMatches("ANALYZE", "(*)"))
  {
       
                                                                       
                                                                          
                                               
       
    if (ends_with(prev_wd, '(') || ends_with(prev_wd, ','))
    {
      COMPLETE_WITH("VERBOSE", "SKIP_LOCKED");
    }
    else if (TailMatches("VERBOSE|SKIP_LOCKED"))
    {
      COMPLETE_WITH("ON", "OFF");
    }
  }
  else if (HeadMatches("ANALYZE") && TailMatches("("))
  {
                                                                       
    COMPLETE_WITH_ATTR(prev2_wd, "");
  }
  else if (HeadMatches("ANALYZE"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_analyzables, NULL);
  }

             
  else if (Matches("BEGIN"))
  {
    COMPLETE_WITH("WORK", "TRANSACTION", "ISOLATION LEVEL", "READ", "DEFERRABLE", "NOT DEFERRABLE");
  }
                  
  else if (Matches("END|ABORT"))
  {
    COMPLETE_WITH("AND", "WORK", "TRANSACTION");
  }
              
  else if (Matches("COMMIT"))
  {
    COMPLETE_WITH("AND", "WORK", "TRANSACTION", "PREPARED");
  }
                         
  else if (Matches("RELEASE"))
  {
    COMPLETE_WITH("SAVEPOINT");
  }
                
  else if (Matches("ROLLBACK"))
  {
    COMPLETE_WITH("AND", "WORK", "TRANSACTION", "TO SAVEPOINT", "PREPARED");
  }
  else if (Matches("ABORT|END|COMMIT|ROLLBACK", "AND"))
  {
    COMPLETE_WITH("CHAIN");
  }
            
  else if (Matches("CALL"))
  {
    COMPLETE_WITH_VERSIONED_SCHEMA_QUERY(Query_for_list_of_procedures, NULL);
  }
  else if (Matches("CALL", MatchAny))
  {
    COMPLETE_WITH("(");
  }
               
  else if (Matches("CLUSTER"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_clusterables, "UNION SELECT 'VERBOSE'");
  }
  else if (Matches("CLUSTER", "VERBOSE"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_clusterables, NULL);
  }
                                                  
  else if (Matches("CLUSTER", MatchAnyExcept("VERBOSE|ON")))
  {
    COMPLETE_WITH("USING");
  }
                                                          
  else if (Matches("CLUSTER", "VERBOSE", MatchAny))
  {
    COMPLETE_WITH("USING");
  }
                                                                  
  else if (Matches("CLUSTER", MatchAny, "USING") || Matches("CLUSTER", "VERBOSE", MatchAny, "USING"))
  {
    completion_info_charp = prev2_wd;
    COMPLETE_WITH_QUERY(Query_for_index_of_table);
  }

               
  else if (Matches("COMMENT"))
  {
    COMPLETE_WITH("ON");
  }
  else if (Matches("COMMENT", "ON"))
  {
    COMPLETE_WITH("ACCESS METHOD", "CAST", "COLLATION", "CONVERSION", "DATABASE", "EVENT TRIGGER", "EXTENSION", "FOREIGN DATA WRAPPER", "FOREIGN TABLE", "SERVER", "INDEX", "LANGUAGE", "POLICY", "PUBLICATION", "RULE", "SCHEMA", "SEQUENCE", "STATISTICS", "SUBSCRIPTION", "TABLE", "TYPE", "VIEW", "MATERIALIZED VIEW", "COLUMN", "AGGREGATE", "FUNCTION", "PROCEDURE", "ROUTINE", "OPERATOR", "TRIGGER", "CONSTRAINT", "DOMAIN", "LARGE OBJECT", "TABLESPACE", "TEXT SEARCH", "ROLE");
  }
  else if (Matches("COMMENT", "ON", "ACCESS", "METHOD"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_access_methods);
  }
  else if (Matches("COMMENT", "ON", "FOREIGN"))
  {
    COMPLETE_WITH("DATA WRAPPER", "TABLE");
  }
  else if (Matches("COMMENT", "ON", "TEXT", "SEARCH"))
  {
    COMPLETE_WITH("CONFIGURATION", "DICTIONARY", "PARSER", "TEMPLATE");
  }
  else if (Matches("COMMENT", "ON", "CONSTRAINT"))
  {
    COMPLETE_WITH_QUERY(Query_for_all_table_constraints);
  }
  else if (Matches("COMMENT", "ON", "CONSTRAINT", MatchAny))
  {
    COMPLETE_WITH("ON");
  }
  else if (Matches("COMMENT", "ON", "CONSTRAINT", MatchAny, "ON"))
  {
    completion_info_charp = prev2_wd;
    COMPLETE_WITH_QUERY(Query_for_list_of_tables_for_constraint);
  }
  else if (Matches("COMMENT", "ON", "MATERIALIZED", "VIEW"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_matviews, NULL);
  }
  else if (Matches("COMMENT", "ON", "EVENT", "TRIGGER"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_event_triggers);
  }
  else if (Matches("COMMENT", "ON", MatchAny, MatchAnyExcept("IS")) || Matches("COMMENT", "ON", MatchAny, MatchAny, MatchAnyExcept("IS")) || Matches("COMMENT", "ON", MatchAny, MatchAny, MatchAny, MatchAnyExcept("IS")))
  {
    COMPLETE_WITH("IS");
  }

            

     
                                                                            
                         
     
  else if (Matches("COPY|\\copy"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, " UNION ALL SELECT '('");
  }
                                                            
  else if (Matches("COPY", "BINARY"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, NULL);
  }
                                                          
  else if (Matches("COPY|\\copy", "("))
  {
    COMPLETE_WITH("SELECT", "TABLE", "VALUES", "INSERT", "UPDATE", "DELETE", "WITH");
  }
                                                                       
  else if (Matches("COPY|\\copy", MatchAny) || Matches("COPY", "BINARY", MatchAny))
  {
    COMPLETE_WITH("FROM", "TO");
  }
                                                                      
  else if (Matches("COPY|\\copy", MatchAny, "FROM|TO") || Matches("COPY", "BINARY", MatchAny, "FROM|TO"))
  {
    completion_charp = "";
    matches = completion_matches(text, complete_from_files);
  }

                                                   
  else if (Matches("COPY|\\copy", MatchAny, "FROM|TO", MatchAny) || Matches("COPY", "BINARY", MatchAny, "FROM|TO", MatchAny))
  {
    COMPLETE_WITH("BINARY", "DELIMITER", "NULL", "CSV", "ENCODING");
  }

                                                       
  else if (Matches("COPY|\\copy", MatchAny, "FROM|TO", MatchAny, "CSV") || Matches("COPY", "BINARY", MatchAny, "FROM|TO", MatchAny, "CSV"))
  {
    COMPLETE_WITH("HEADER", "QUOTE", "ESCAPE", "FORCE QUOTE", "FORCE NOT NULL");
  }

                            
                                              
  else if (Matches("CREATE", "ACCESS", "METHOD", MatchAny))
  {
    COMPLETE_WITH("TYPE");
  }
                                                   
  else if (Matches("CREATE", "ACCESS", "METHOD", MatchAny, "TYPE"))
  {
    COMPLETE_WITH("INDEX", "TABLE");
  }
                                                          
  else if (Matches("CREATE", "ACCESS", "METHOD", MatchAny, "TYPE", MatchAny))
  {
    COMPLETE_WITH("HANDLER");
  }

                       
  else if (Matches("CREATE", "DATABASE", MatchAny))
  {
    COMPLETE_WITH("OWNER", "TEMPLATE", "ENCODING", "TABLESPACE", "IS_TEMPLATE", "ALLOW_CONNECTIONS", "CONNECTION LIMIT", "LC_COLLATE", "LC_CTYPE");
  }

  else if (Matches("CREATE", "DATABASE", MatchAny, "TEMPLATE"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_template_databases);
  }

                        
                                                                      
  else if (Matches("CREATE", "EXTENSION"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_available_extensions);
  }
                               
  else if (Matches("CREATE", "EXTENSION", MatchAny))
  {
    COMPLETE_WITH("WITH SCHEMA", "CASCADE", "VERSION");
  }
                                       
  else if (Matches("CREATE", "EXTENSION", MatchAny, "VERSION"))
  {
    completion_info_charp = prev2_wd;
    COMPLETE_WITH_QUERY(Query_for_list_of_available_extension_versions);
  }

                      
  else if (Matches("CREATE", "FOREIGN"))
  {
    COMPLETE_WITH("DATA WRAPPER", "TABLE");
  }

                                   
  else if (Matches("CREATE", "FOREIGN", "DATA", "WRAPPER", MatchAny))
  {
    COMPLETE_WITH("HANDLER", "VALIDATOR", "OPTIONS");
  }

                                                                            
                                                        
  else if (TailMatches("CREATE", "UNIQUE"))
  {
    COMPLETE_WITH("INDEX");
  }

     
                                                                        
                      
     
  else if (TailMatches("CREATE|UNIQUE", "INDEX"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_indexes, " UNION SELECT 'ON'"
                                                          " UNION SELECT 'CONCURRENTLY'");
  }

     
                                                                          
                                    
     
  else if (TailMatches("INDEX|CONCURRENTLY", MatchAny, "ON") || TailMatches("INDEX|CONCURRENTLY", "ON"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_indexables, NULL);
  }

     
                                                                      
             
     
  else if (TailMatches("CREATE|UNIQUE", "INDEX", "CONCURRENTLY"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_indexes, " UNION SELECT 'ON'");
  }
                                                                   
  else if (TailMatches("CREATE|UNIQUE", "INDEX", MatchAny) || TailMatches("CREATE|UNIQUE", "INDEX", "CONCURRENTLY", MatchAny))
  {
    COMPLETE_WITH("ON");
  }

     
                                                                          
                                 
     
  else if (TailMatches("INDEX", MatchAny, "ON", MatchAny) || TailMatches("INDEX|CONCURRENTLY", "ON", MatchAny))
  {
    COMPLETE_WITH("(", "USING");
  }
  else if (TailMatches("INDEX", MatchAny, "ON", MatchAny, "(") || TailMatches("INDEX|CONCURRENTLY", "ON", MatchAny, "("))
  {
    COMPLETE_WITH_ATTR(prev2_wd, "");
  }
                                
  else if (TailMatches("ON", MatchAny, "USING", MatchAny, "("))
  {
    COMPLETE_WITH_ATTR(prev4_wd, "");
  }
                                           
  else if (TailMatches("INDEX", MatchAny, MatchAny, "ON", MatchAny, "USING") || TailMatches("INDEX", MatchAny, "ON", MatchAny, "USING") || TailMatches("INDEX", "ON", MatchAny, "USING"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_index_access_methods);
  }
  else if (TailMatches("ON", MatchAny, "USING", MatchAny) && !TailMatches("POLICY", MatchAny, MatchAny, MatchAny, MatchAny, MatchAny) && !TailMatches("FOR", MatchAny, MatchAny, MatchAny))
  {
    COMPLETE_WITH("(");
  }

                     
                                          
  else if (Matches("CREATE", "POLICY", MatchAny))
  {
    COMPLETE_WITH("ON");
  }
                                                  
  else if (Matches("CREATE", "POLICY", MatchAny, "ON"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, NULL);
  }
                                                                             
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny))
  {
    COMPLETE_WITH("AS", "FOR", "TO", "USING (", "WITH CHECK (");
  }
                                                                 
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny, "AS"))
  {
    COMPLETE_WITH("PERMISSIVE", "RESTRICTIVE");
  }

     
                                                               
                             
     
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny, "AS", MatchAny))
  {
    COMPLETE_WITH("FOR", "TO", "USING", "WITH CHECK");
  }
                                                                           
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny, "FOR"))
  {
    COMPLETE_WITH("ALL", "SELECT", "INSERT", "UPDATE", "DELETE");
  }
                                                                           
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny, "FOR", "INSERT"))
  {
    COMPLETE_WITH("TO", "WITH CHECK (");
  }
                                                                             
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny, "FOR", "SELECT|DELETE"))
  {
    COMPLETE_WITH("TO", "USING (");
  }
                                                                          
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny, "FOR", "ALL|UPDATE"))
  {
    COMPLETE_WITH("TO", "USING (", "WITH CHECK (");
  }
                                                            
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny, "TO"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_grant_roles);
  }
                                                          
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny, "USING"))
  {
    COMPLETE_WITH("(");
  }

     
                                                                   
                                     
     
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny, "AS", MatchAny, "FOR"))
  {
    COMPLETE_WITH("ALL", "SELECT", "INSERT", "UPDATE", "DELETE");
  }

     
                                                                             
                           
     
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny, "AS", MatchAny, "FOR", "INSERT"))
  {
    COMPLETE_WITH("TO", "WITH CHECK (");
  }

     
                                                                             
                             
     
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny, "AS", MatchAny, "FOR", "SELECT|DELETE"))
  {
    COMPLETE_WITH("TO", "USING (");
  }

     
                                                                   
                                    
     
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny, "AS", MatchAny, "FOR", "ALL|UPDATE"))
  {
    COMPLETE_WITH("TO", "USING (", "WITH CHECK (");
  }

     
                                                                            
             
     
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny, "AS", MatchAny, "TO"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_grant_roles);
  }

     
                                                                         
              
     
  else if (Matches("CREATE", "POLICY", MatchAny, "ON", MatchAny, "AS", MatchAny, "USING"))
  {
    COMPLETE_WITH("(");
  }

                          
  else if (Matches("CREATE", "PUBLICATION", MatchAny))
  {
    COMPLETE_WITH("FOR TABLE", "FOR ALL TABLES", "WITH (");
  }
  else if (Matches("CREATE", "PUBLICATION", MatchAny, "FOR"))
  {
    COMPLETE_WITH("TABLE", "ALL TABLES");
  }
                                                                   
  else if (HeadMatches("CREATE", "PUBLICATION", MatchAny, "FOR", "TABLE"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, NULL);
  }
                                                       
  else if (HeadMatches("CREATE", "PUBLICATION") && TailMatches("WITH", "("))
  {
    COMPLETE_WITH("publish");
  }

                   
                                                 
  else if (Matches("CREATE", "RULE", MatchAny))
  {
    COMPLETE_WITH("AS ON");
  }
                                                 
  else if (Matches("CREATE", "RULE", MatchAny, "AS"))
  {
    COMPLETE_WITH("ON");
  }
                                                                           
  else if (Matches("CREATE", "RULE", MatchAny, "AS", "ON"))
  {
    COMPLETE_WITH("SELECT", "UPDATE", "INSERT", "DELETE");
  }
                                                                
  else if (TailMatches("AS", "ON", "SELECT|UPDATE|INSERT|DELETE"))
  {
    COMPLETE_WITH("TO");
  }
                                                   
  else if (TailMatches("AS", "ON", "SELECT|UPDATE|INSERT|DELETE", "TO"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, NULL);
  }

                                                                               
  else if (TailMatches("CREATE", "SEQUENCE", MatchAny) || TailMatches("CREATE", "TEMP|TEMPORARY", "SEQUENCE", MatchAny))
  {
    COMPLETE_WITH("INCREMENT BY", "MINVALUE", "MAXVALUE", "NO", "CACHE", "CYCLE", "OWNED BY", "START WITH");
  }
  else if (TailMatches("CREATE", "SEQUENCE", MatchAny, "NO") || TailMatches("CREATE", "TEMP|TEMPORARY", "SEQUENCE", MatchAny, "NO"))
  {
    COMPLETE_WITH("MINVALUE", "MAXVALUE", "CYCLE");
  }

                            
  else if (Matches("CREATE", "SERVER", MatchAny))
  {
    COMPLETE_WITH("TYPE", "VERSION", "FOREIGN DATA WRAPPER");
  }

                                
  else if (Matches("CREATE", "STATISTICS", MatchAny))
  {
    COMPLETE_WITH("(", "ON");
  }
  else if (Matches("CREATE", "STATISTICS", MatchAny, "("))
  {
    COMPLETE_WITH("ndistinct", "dependencies", "mcv");
  }
  else if (Matches("CREATE", "STATISTICS", MatchAny, "(*)"))
  {
    COMPLETE_WITH("ON");
  }
  else if (HeadMatches("CREATE", "STATISTICS", MatchAny) && TailMatches("FROM"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, NULL);
  }

                                                                            
                                                                       
  else if (TailMatches("CREATE", "TEMP|TEMPORARY"))
  {
    COMPLETE_WITH("SEQUENCE", "TABLE", "VIEW");
  }
                                                        
  else if (TailMatches("CREATE", "UNLOGGED"))
  {
    COMPLETE_WITH("TABLE", "MATERIALIZED VIEW");
  }
                                                           
  else if (TailMatches("PARTITION", "BY"))
  {
    COMPLETE_WITH("RANGE (", "LIST (", "HASH (");
  }
                                                                         
  else if (TailMatches("PARTITION", "OF"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_partitioned_tables, "");
  }
                                                                    
  else if (TailMatches("PARTITION", "OF", MatchAny))
  {
    COMPLETE_WITH("FOR VALUES", "DEFAULT");
  }
                                                                 
  else if (TailMatches("CREATE", "TABLE", MatchAny) || TailMatches("CREATE", "TEMP|TEMPORARY|UNLOGGED", "TABLE", MatchAny))
  {
    COMPLETE_WITH("(", "OF", "PARTITION OF");
  }
                                                                    
  else if (TailMatches("CREATE", "TABLE", MatchAny, "OF") || TailMatches("CREATE", "TEMP|TEMPORARY|UNLOGGED", "TABLE", MatchAny, "OF"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_composite_datatypes, NULL);
  }
                                                               
  else if (TailMatches("CREATE", "TABLE", MatchAny, "(*)") || TailMatches("CREATE", "UNLOGGED", "TABLE", MatchAny, "(*)"))
  {
    COMPLETE_WITH("INHERITS (", "PARTITION BY", "USING", "TABLESPACE", "WITH (");
  }
  else if (TailMatches("CREATE", "TEMP|TEMPORARY", "TABLE", MatchAny, "(*)"))
  {
    COMPLETE_WITH("INHERITS (", "ON COMMIT", "PARTITION BY", "TABLESPACE", "WITH (");
  }
                                                                   
  else if (TailMatches("CREATE", "TABLE", MatchAny, "(*)", "USING") || TailMatches("CREATE", "TEMP|TEMPORARY|UNLOGGED", "TABLE", MatchAny, "(*)", "USING"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_table_access_methods);
  }
                                                                
  else if (TailMatches("CREATE", "TABLE", MatchAny, "(*)", "WITH", "(") || TailMatches("CREATE", "TEMP|TEMPORARY|UNLOGGED", "TABLE", MatchAny, "(*)", "WITH", "("))
  {
    COMPLETE_WITH_LIST(table_storage_parameters);
  }
                                                    
  else if (TailMatches("CREATE", "TEMP|TEMPORARY", "TABLE", MatchAny, "(*)", "ON", "COMMIT"))
  {
    COMPLETE_WITH("DELETE ROWS", "DROP", "PRESERVE ROWS");
  }

                         
  else if (Matches("CREATE", "TABLESPACE", MatchAny))
  {
    COMPLETE_WITH("OWNER", "LOCATION");
  }
                                                                  
  else if (Matches("CREATE", "TABLESPACE", MatchAny, "OWNER", MatchAny))
  {
    COMPLETE_WITH("LOCATION");
  }

                          
  else if (Matches("CREATE", "TEXT", "SEARCH"))
  {
    COMPLETE_WITH("CONFIGURATION", "DICTIONARY", "PARSER", "TEMPLATE");
  }
  else if (Matches("CREATE", "TEXT", "SEARCH", "CONFIGURATION", MatchAny))
  {
    COMPLETE_WITH("(");
  }

                           
  else if (Matches("CREATE", "SUBSCRIPTION", MatchAny))
  {
    COMPLETE_WITH("CONNECTION");
  }
  else if (Matches("CREATE", "SUBSCRIPTION", MatchAny, "CONNECTION", MatchAny))
  {
    COMPLETE_WITH("PUBLICATION");
  }
  else if (Matches("CREATE", "SUBSCRIPTION", MatchAny, "CONNECTION", MatchAny, "PUBLICATION"))
  {
                                                                          
  }
  else if (HeadMatches("CREATE", "SUBSCRIPTION") && TailMatches("PUBLICATION", MatchAny))
  {
    COMPLETE_WITH("WITH (");
  }
                                                               
  else if (HeadMatches("CREATE", "SUBSCRIPTION") && TailMatches("WITH", "("))
  {
    COMPLETE_WITH("copy_data", "connect", "create_slot", "enabled", "slot_name", "synchronous_commit");
  }

                                                                              
                                                                   
  else if (TailMatches("CREATE", "TRIGGER", MatchAny))
  {
    COMPLETE_WITH("BEFORE", "AFTER", "INSTEAD OF");
  }
                                                                 
  else if (TailMatches("CREATE", "TRIGGER", MatchAny, "BEFORE|AFTER"))
  {
    COMPLETE_WITH("INSERT", "DELETE", "UPDATE", "TRUNCATE");
  }
                                                               
  else if (TailMatches("CREATE", "TRIGGER", MatchAny, "INSTEAD", "OF"))
  {
    COMPLETE_WITH("INSERT", "DELETE", "UPDATE");
  }
                                                                  
  else if (TailMatches("CREATE", "TRIGGER", MatchAny, "BEFORE|AFTER", MatchAny) || TailMatches("CREATE", "TRIGGER", MatchAny, "INSTEAD", "OF", MatchAny))
  {
    COMPLETE_WITH("ON", "OR");
  }

     
                                                                         
                                                                             
                                          
     
  else if (TailMatches("CREATE", "TRIGGER", MatchAny, "BEFORE|AFTER", MatchAny, "ON"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, NULL);
  }
                                                                            
  else if (TailMatches("CREATE", "TRIGGER", MatchAny, "INSTEAD", "OF", MatchAny, "ON"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_views, NULL);
  }
  else if (HeadMatches("CREATE", "TRIGGER") && TailMatches("ON", MatchAny))
  {
    if (pset.sversion >= 110000)
    {
      COMPLETE_WITH("NOT DEFERRABLE", "DEFERRABLE", "INITIALLY", "REFERENCING", "FOR", "WHEN (", "EXECUTE FUNCTION");
    }
    else
    {
      COMPLETE_WITH("NOT DEFERRABLE", "DEFERRABLE", "INITIALLY", "REFERENCING", "FOR", "WHEN (", "EXECUTE PROCEDURE");
    }
  }
  else if (HeadMatches("CREATE", "TRIGGER") && (TailMatches("DEFERRABLE") || TailMatches("INITIALLY", "IMMEDIATE|DEFERRED")))
  {
    if (pset.sversion >= 110000)
    {
      COMPLETE_WITH("REFERENCING", "FOR", "WHEN (", "EXECUTE FUNCTION");
    }
    else
    {
      COMPLETE_WITH("REFERENCING", "FOR", "WHEN (", "EXECUTE PROCEDURE");
    }
  }
  else if (HeadMatches("CREATE", "TRIGGER") && TailMatches("REFERENCING"))
  {
    COMPLETE_WITH("OLD TABLE", "NEW TABLE");
  }
  else if (HeadMatches("CREATE", "TRIGGER") && TailMatches("OLD|NEW", "TABLE"))
  {
    COMPLETE_WITH("AS");
  }
  else if (HeadMatches("CREATE", "TRIGGER") && (TailMatches("REFERENCING", "OLD", "TABLE", "AS", MatchAny) || TailMatches("REFERENCING", "OLD", "TABLE", MatchAny)))
  {
    if (pset.sversion >= 110000)
    {
      COMPLETE_WITH("NEW TABLE", "FOR", "WHEN (", "EXECUTE FUNCTION");
    }
    else
    {
      COMPLETE_WITH("NEW TABLE", "FOR", "WHEN (", "EXECUTE PROCEDURE");
    }
  }
  else if (HeadMatches("CREATE", "TRIGGER") && (TailMatches("REFERENCING", "NEW", "TABLE", "AS", MatchAny) || TailMatches("REFERENCING", "NEW", "TABLE", MatchAny)))
  {
    if (pset.sversion >= 110000)
    {
      COMPLETE_WITH("OLD TABLE", "FOR", "WHEN (", "EXECUTE FUNCTION");
    }
    else
    {
      COMPLETE_WITH("OLD TABLE", "FOR", "WHEN (", "EXECUTE PROCEDURE");
    }
  }
  else if (HeadMatches("CREATE", "TRIGGER") && (TailMatches("REFERENCING", "OLD|NEW", "TABLE", "AS", MatchAny, "OLD|NEW", "TABLE", "AS", MatchAny) || TailMatches("REFERENCING", "OLD|NEW", "TABLE", MatchAny, "OLD|NEW", "TABLE", "AS", MatchAny) || TailMatches("REFERENCING", "OLD|NEW", "TABLE", "AS", MatchAny, "OLD|NEW", "TABLE", MatchAny) || TailMatches("REFERENCING", "OLD|NEW", "TABLE", MatchAny, "OLD|NEW", "TABLE", MatchAny)))
  {
    if (pset.sversion >= 110000)
    {
      COMPLETE_WITH("FOR", "WHEN (", "EXECUTE FUNCTION");
    }
    else
    {
      COMPLETE_WITH("FOR", "WHEN (", "EXECUTE PROCEDURE");
    }
  }
  else if (HeadMatches("CREATE", "TRIGGER") && TailMatches("FOR"))
  {
    COMPLETE_WITH("EACH", "ROW", "STATEMENT");
  }
  else if (HeadMatches("CREATE", "TRIGGER") && TailMatches("FOR", "EACH"))
  {
    COMPLETE_WITH("ROW", "STATEMENT");
  }
  else if (HeadMatches("CREATE", "TRIGGER") && (TailMatches("FOR", "EACH", "ROW|STATEMENT") || TailMatches("FOR", "ROW|STATEMENT")))
  {
    if (pset.sversion >= 110000)
    {
      COMPLETE_WITH("WHEN (", "EXECUTE FUNCTION");
    }
    else
    {
      COMPLETE_WITH("WHEN (", "EXECUTE PROCEDURE");
    }
  }
  else if (HeadMatches("CREATE", "TRIGGER") && TailMatches("WHEN", "(*)"))
  {
    if (pset.sversion >= 110000)
    {
      COMPLETE_WITH("EXECUTE FUNCTION");
    }
    else
    {
      COMPLETE_WITH("EXECUTE PROCEDURE");
    }
  }
                                                                   
  else if (HeadMatches("CREATE", "TRIGGER") && TailMatches("EXECUTE"))
  {
    if (pset.sversion >= 110000)
    {
      COMPLETE_WITH("FUNCTION");
    }
    else
    {
      COMPLETE_WITH("PROCEDURE");
    }
  }
  else if (HeadMatches("CREATE", "TRIGGER") && TailMatches("EXECUTE", "FUNCTION|PROCEDURE"))
  {
    COMPLETE_WITH_VERSIONED_SCHEMA_QUERY(Query_for_list_of_functions, NULL);
  }

                                     
  else if (Matches("CREATE", "ROLE|GROUP|USER", MatchAny) && !TailMatches("USER", "MAPPING"))
  {
    COMPLETE_WITH("ADMIN", "BYPASSRLS", "CONNECTION LIMIT", "CREATEDB", "CREATEROLE", "ENCRYPTED PASSWORD", "IN", "INHERIT", "LOGIN", "NOBYPASSRLS", "NOCREATEDB", "NOCREATEROLE", "NOINHERIT", "NOLOGIN", "NOREPLICATION", "NOSUPERUSER", "PASSWORD", "REPLICATION", "ROLE", "SUPERUSER", "SYSID", "VALID UNTIL", "WITH");
  }

                                          
  else if (Matches("CREATE", "ROLE|GROUP|USER", MatchAny, "WITH"))
  {
                                                                
    COMPLETE_WITH("ADMIN", "BYPASSRLS", "CONNECTION LIMIT", "CREATEDB", "CREATEROLE", "ENCRYPTED PASSWORD", "IN", "INHERIT", "LOGIN", "NOBYPASSRLS", "NOCREATEDB", "NOCREATEROLE", "NOINHERIT", "NOLOGIN", "NOREPLICATION", "NOSUPERUSER", "PASSWORD", "REPLICATION", "ROLE", "SUPERUSER", "SYSID", "VALID UNTIL");
  }

                                                                 
  else if (Matches("CREATE", "ROLE|USER|GROUP", MatchAny, "IN"))
  {
    COMPLETE_WITH("GROUP", "ROLE");
  }

                                                                           
                                           
  else if (TailMatches("CREATE", "VIEW", MatchAny))
  {
    COMPLETE_WITH("AS");
  }
                                                    
  else if (TailMatches("CREATE", "VIEW", MatchAny, "AS"))
  {
    COMPLETE_WITH("SELECT");
  }

                                
  else if (Matches("CREATE", "MATERIALIZED"))
  {
    COMPLETE_WITH("VIEW");
  }
                                                        
  else if (Matches("CREATE", "MATERIALIZED", "VIEW", MatchAny))
  {
    COMPLETE_WITH("AS");
  }
                                                                 
  else if (Matches("CREATE", "MATERIALIZED", "VIEW", MatchAny, "AS"))
  {
    COMPLETE_WITH("SELECT");
  }

                            
  else if (Matches("CREATE", "EVENT"))
  {
    COMPLETE_WITH("TRIGGER");
  }
                                                    
  else if (Matches("CREATE", "EVENT", "TRIGGER", MatchAny))
  {
    COMPLETE_WITH("ON");
  }
                                                               
  else if (Matches("CREATE", "EVENT", "TRIGGER", MatchAny, "ON"))
  {
    COMPLETE_WITH("ddl_command_start", "ddl_command_end", "sql_drop");
  }

     
                                                                             
                                                                           
                  
     
  else if (Matches("CREATE", "EVENT", "TRIGGER", MatchAny, "ON", MatchAny))
  {
    if (pset.sversion >= 110000)
    {
      COMPLETE_WITH("WHEN TAG IN (", "EXECUTE FUNCTION");
    }
    else
    {
      COMPLETE_WITH("WHEN TAG IN (", "EXECUTE PROCEDURE");
    }
  }
  else if (HeadMatches("CREATE", "EVENT", "TRIGGER") && TailMatches("WHEN|AND", MatchAny, "IN", "(*)"))
  {
    if (pset.sversion >= 110000)
    {
      COMPLETE_WITH("EXECUTE FUNCTION");
    }
    else
    {
      COMPLETE_WITH("EXECUTE PROCEDURE");
    }
  }
  else if (HeadMatches("CREATE", "EVENT", "TRIGGER") && TailMatches("EXECUTE", "FUNCTION|PROCEDURE"))
  {
    COMPLETE_WITH_VERSIONED_SCHEMA_QUERY(Query_for_list_of_functions, NULL);
  }

                  
  else if (Matches("DEALLOCATE"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_prepared_statements);
  }

               
  else if (Matches("DECLARE", MatchAny))
  {
    COMPLETE_WITH("BINARY", "INSENSITIVE", "SCROLL", "NO SCROLL", "CURSOR");
  }
  else if (HeadMatches("DECLARE") && TailMatches("CURSOR"))
  {
    COMPLETE_WITH("WITH HOLD", "WITHOUT HOLD", "FOR");
  }

                                                   
                                                                          
  else if (Matches("DELETE"))
  {
    COMPLETE_WITH("FROM");
  }
                                                  
  else if (TailMatches("DELETE", "FROM"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_updatables, NULL);
  }
                                    
  else if (TailMatches("DELETE", "FROM", MatchAny))
  {
    COMPLETE_WITH("USING", "WHERE");
  }
                                                          

               
  else if (Matches("DISCARD"))
  {
    COMPLETE_WITH("ALL", "PLANS", "SEQUENCES", "TEMP");
  }

          
  else if (Matches("DO"))
  {
    COMPLETE_WITH("LANGUAGE");
  }

            
                                                    
  else if (Matches("DROP", "COLLATION|CONVERSION|DOMAIN|EXTENSION|LANGUAGE|PUBLICATION|SCHEMA|SEQUENCE|SERVER|SUBSCRIPTION|STATISTICS|TABLE|TYPE|VIEW", MatchAny) || Matches("DROP", "ACCESS", "METHOD", MatchAny) || (Matches("DROP", "AGGREGATE|FUNCTION|PROCEDURE|ROUTINE", MatchAny, MatchAny) && ends_with(prev_wd, ')')) || Matches("DROP", "EVENT", "TRIGGER", MatchAny) || Matches("DROP", "FOREIGN", "DATA", "WRAPPER", MatchAny) || Matches("DROP", "FOREIGN", "TABLE", MatchAny) || Matches("DROP", "TEXT", "SEARCH", "CONFIGURATION|DICTIONARY|PARSER|TEMPLATE", MatchAny))
  {
    COMPLETE_WITH("CASCADE", "RESTRICT");
  }

                                            
  else if (Matches("DROP", "AGGREGATE|FUNCTION|PROCEDURE|ROUTINE", MatchAny))
  {
    COMPLETE_WITH("(");
  }
  else if (Matches("DROP", "AGGREGATE|FUNCTION|PROCEDURE|ROUTINE", MatchAny, "("))
  {
    COMPLETE_WITH_FUNCTION_ARG(prev2_wd);
  }
  else if (Matches("DROP", "FOREIGN"))
  {
    COMPLETE_WITH("DATA WRAPPER", "TABLE");
  }

                  
  else if (Matches("DROP", "INDEX"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_indexes, " UNION SELECT 'CONCURRENTLY'");
  }
  else if (Matches("DROP", "INDEX", "CONCURRENTLY"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_indexes, NULL);
  }
  else if (Matches("DROP", "INDEX", MatchAny))
  {
    COMPLETE_WITH("CASCADE", "RESTRICT");
  }
  else if (Matches("DROP", "INDEX", "CONCURRENTLY", MatchAny))
  {
    COMPLETE_WITH("CASCADE", "RESTRICT");
  }

                              
  else if (Matches("DROP", "MATERIALIZED"))
  {
    COMPLETE_WITH("VIEW");
  }
  else if (Matches("DROP", "MATERIALIZED", "VIEW"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_matviews, NULL);
  }

                     
  else if (Matches("DROP", "OWNED"))
  {
    COMPLETE_WITH("BY");
  }
  else if (Matches("DROP", "OWNED", "BY"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_roles);
  }

                        
  else if (Matches("DROP", "TEXT", "SEARCH"))
  {
    COMPLETE_WITH("CONFIGURATION", "DICTIONARY", "PARSER", "TEMPLATE");
  }

                    
  else if (Matches("DROP", "TRIGGER", MatchAny))
  {
    COMPLETE_WITH("ON");
  }
  else if (Matches("DROP", "TRIGGER", MatchAny, "ON"))
  {
    completion_info_charp = prev2_wd;
    COMPLETE_WITH_QUERY(Query_for_list_of_tables_for_trigger);
  }
  else if (Matches("DROP", "TRIGGER", MatchAny, "ON", MatchAny))
  {
    COMPLETE_WITH("CASCADE", "RESTRICT");
  }

                          
  else if (Matches("DROP", "ACCESS"))
  {
    COMPLETE_WITH("METHOD");
  }
  else if (Matches("DROP", "ACCESS", "METHOD"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_access_methods);
  }

                          
  else if (Matches("DROP", "EVENT"))
  {
    COMPLETE_WITH("TRIGGER");
  }
  else if (Matches("DROP", "EVENT", "TRIGGER"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_event_triggers);
  }

                           
  else if (Matches("DROP", "POLICY"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_policies);
  }
                             
  else if (Matches("DROP", "POLICY", MatchAny))
  {
    COMPLETE_WITH("ON");
  }
                                     
  else if (Matches("DROP", "POLICY", MatchAny, "ON"))
  {
    completion_info_charp = prev2_wd;
    COMPLETE_WITH_QUERY(Query_for_list_of_tables_for_policy);
  }

                 
  else if (Matches("DROP", "RULE", MatchAny))
  {
    COMPLETE_WITH("ON");
  }
  else if (Matches("DROP", "RULE", MatchAny, "ON"))
  {
    completion_info_charp = prev2_wd;
    COMPLETE_WITH_QUERY(Query_for_list_of_tables_for_rule);
  }
  else if (Matches("DROP", "RULE", MatchAny, "ON", MatchAny))
  {
    COMPLETE_WITH("CASCADE", "RESTRICT");
  }

               
  else if (Matches("EXECUTE"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_prepared_statements);
  }

     
                                              
                                               
     
  else if (Matches("EXPLAIN"))
  {
    COMPLETE_WITH("SELECT", "INSERT", "DELETE", "UPDATE", "DECLARE", "ANALYZE", "VERBOSE");
  }
  else if (HeadMatches("EXPLAIN", "(*") && !HeadMatches("EXPLAIN", "(*)"))
  {
       
                                                                       
                                                                          
                                               
       
    if (ends_with(prev_wd, '(') || ends_with(prev_wd, ','))
    {
      COMPLETE_WITH("ANALYZE", "VERBOSE", "COSTS", "SETTINGS", "BUFFERS", "TIMING", "SUMMARY", "FORMAT");
    }
    else if (TailMatches("ANALYZE|VERBOSE|COSTS|SETTINGS|BUFFERS|TIMING|SUMMARY"))
    {
      COMPLETE_WITH("ON", "OFF");
    }
    else if (TailMatches("FORMAT"))
    {
      COMPLETE_WITH("TEXT", "XML", "JSON", "YAML");
    }
  }
  else if (Matches("EXPLAIN", "ANALYZE"))
  {
    COMPLETE_WITH("SELECT", "INSERT", "DELETE", "UPDATE", "DECLARE", "VERBOSE");
  }
  else if (Matches("EXPLAIN", "(*)") || Matches("EXPLAIN", "VERBOSE") || Matches("EXPLAIN", "ANALYZE", "VERBOSE"))
  {
    COMPLETE_WITH("SELECT", "INSERT", "DELETE", "UPDATE", "DECLARE");
  }

                     
                                                              
  else if (Matches("FETCH|MOVE"))
  {
    COMPLETE_WITH("ABSOLUTE", "BACKWARD", "FORWARD", "RELATIVE");
  }
                                                         
  else if (Matches("FETCH|MOVE", MatchAny))
  {
    COMPLETE_WITH("ALL", "NEXT", "PRIOR");
  }

     
                                                                             
                                                                         
                           
     
  else if (Matches("FETCH|MOVE", MatchAny, MatchAny))
  {
    COMPLETE_WITH("FROM", "IN");
  }

                            
                                                      
  else if (TailMatches("FOREIGN", "DATA", "WRAPPER") && !TailMatches("CREATE", MatchAny, MatchAny, MatchAny))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_fdws);
  }
                                
  else if (TailMatches("FOREIGN", "DATA", "WRAPPER", MatchAny) && HeadMatches("CREATE", "SERVER"))
  {
    COMPLETE_WITH("OPTIONS");
  }

                     
  else if (TailMatches("FOREIGN", "TABLE") && !TailMatches("CREATE", MatchAny, MatchAny))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_foreign_tables, NULL);
  }

                      
  else if (TailMatches("FOREIGN", "SERVER"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_servers);
  }

     
                                                           
                                                  
     
                                                                 
  else if (TailMatches("GRANT|REVOKE"))
  {
       
                                                                       
                                      
       
    if (HeadMatches("ALTER", "DEFAULT", "PRIVILEGES"))
    {
      COMPLETE_WITH("SELECT", "INSERT", "UPDATE", "DELETE", "TRUNCATE", "REFERENCES", "TRIGGER", "EXECUTE", "USAGE", "ALL");
    }
    else
    {
      COMPLETE_WITH_QUERY(Query_for_list_of_roles " UNION SELECT 'SELECT'"
                                                  " UNION SELECT 'INSERT'"
                                                  " UNION SELECT 'UPDATE'"
                                                  " UNION SELECT 'DELETE'"
                                                  " UNION SELECT 'TRUNCATE'"
                                                  " UNION SELECT 'REFERENCES'"
                                                  " UNION SELECT 'TRIGGER'"
                                                  " UNION SELECT 'CREATE'"
                                                  " UNION SELECT 'CONNECT'"
                                                  " UNION SELECT 'TEMPORARY'"
                                                  " UNION SELECT 'EXECUTE'"
                                                  " UNION SELECT 'USAGE'"
                                                  " UNION SELECT 'ALL'");
    }
  }

     
                                                                           
             
     
  else if (TailMatches("GRANT|REVOKE", MatchAny))
  {
    if (TailMatches("SELECT|INSERT|UPDATE|DELETE|TRUNCATE|REFERENCES|TRIGGER|CREATE|CONNECT|TEMPORARY|TEMP|EXECUTE|USAGE|ALL"))
    {
      COMPLETE_WITH("ON");
    }
    else if (TailMatches("GRANT", MatchAny))
    {
      COMPLETE_WITH("TO");
    }
    else
    {
      COMPLETE_WITH("FROM");
    }
  }

     
                                                                          
     
                                                                          
                                                  
     
                                                                             
                                                                    
                
     
  else if (TailMatches("GRANT|REVOKE", MatchAny, "ON"))
  {
       
                                                                          
                          
       
    if (HeadMatches("ALTER", "DEFAULT", "PRIVILEGES"))
    {
      COMPLETE_WITH("TABLES", "SEQUENCES", "FUNCTIONS", "PROCEDURES", "ROUTINES", "TYPES", "SCHEMAS");
    }
    else
    {
      COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_grantables, " UNION SELECT 'ALL FUNCTIONS IN SCHEMA'"
                                                               " UNION SELECT 'ALL PROCEDURES IN SCHEMA'"
                                                               " UNION SELECT 'ALL ROUTINES IN SCHEMA'"
                                                               " UNION SELECT 'ALL SEQUENCES IN SCHEMA'"
                                                               " UNION SELECT 'ALL TABLES IN SCHEMA'"
                                                               " UNION SELECT 'DATABASE'"
                                                               " UNION SELECT 'DOMAIN'"
                                                               " UNION SELECT 'FOREIGN DATA WRAPPER'"
                                                               " UNION SELECT 'FOREIGN SERVER'"
                                                               " UNION SELECT 'FUNCTION'"
                                                               " UNION SELECT 'LANGUAGE'"
                                                               " UNION SELECT 'LARGE OBJECT'"
                                                               " UNION SELECT 'PROCEDURE'"
                                                               " UNION SELECT 'ROUTINE'"
                                                               " UNION SELECT 'SCHEMA'"
                                                               " UNION SELECT 'SEQUENCE'"
                                                               " UNION SELECT 'TABLE'"
                                                               " UNION SELECT 'TABLESPACE'"
                                                               " UNION SELECT 'TYPE'");
    }
  }
  else if (TailMatches("GRANT|REVOKE", MatchAny, "ON", "ALL"))
  {
    COMPLETE_WITH("FUNCTIONS IN SCHEMA", "PROCEDURES IN SCHEMA", "ROUTINES IN SCHEMA", "SEQUENCES IN SCHEMA", "TABLES IN SCHEMA");
  }
  else if (TailMatches("GRANT|REVOKE", MatchAny, "ON", "FOREIGN"))
  {
    COMPLETE_WITH("DATA WRAPPER", "SERVER");
  }

     
                                                                     
                          
     
                                                    
     
  else if (TailMatches("GRANT|REVOKE", MatchAny, "ON", MatchAny))
  {
    if (TailMatches("DATABASE"))
    {
      COMPLETE_WITH_QUERY(Query_for_list_of_databases);
    }
    else if (TailMatches("DOMAIN"))
    {
      COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_domains, NULL);
    }
    else if (TailMatches("FUNCTION"))
    {
      COMPLETE_WITH_VERSIONED_SCHEMA_QUERY(Query_for_list_of_functions, NULL);
    }
    else if (TailMatches("LANGUAGE"))
    {
      COMPLETE_WITH_QUERY(Query_for_list_of_languages);
    }
    else if (TailMatches("PROCEDURE"))
    {
      COMPLETE_WITH_VERSIONED_SCHEMA_QUERY(Query_for_list_of_procedures, NULL);
    }
    else if (TailMatches("ROUTINE"))
    {
      COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_routines, NULL);
    }
    else if (TailMatches("SCHEMA"))
    {
      COMPLETE_WITH_QUERY(Query_for_list_of_schemas);
    }
    else if (TailMatches("SEQUENCE"))
    {
      COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_sequences, NULL);
    }
    else if (TailMatches("TABLE"))
    {
      COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_grantables, NULL);
    }
    else if (TailMatches("TABLESPACE"))
    {
      COMPLETE_WITH_QUERY(Query_for_list_of_tablespaces);
    }
    else if (TailMatches("TYPE"))
    {
      COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_datatypes, NULL);
    }
    else if (TailMatches("GRANT", MatchAny, MatchAny, MatchAny))
    {
      COMPLETE_WITH("TO");
    }
    else
    {
      COMPLETE_WITH("FROM");
    }
  }

     
                                                                
                                    
     
  else if ((HeadMatches("GRANT") && TailMatches("TO")) || (HeadMatches("REVOKE") && TailMatches("FROM")))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_grant_roles);
  }
                                                                       
  else if (HeadMatches("ALTER", "DEFAULT", "PRIVILEGES") && TailMatches("TO|FROM"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_grant_roles);
  }
                                                       
  else if (HeadMatches("GRANT") && TailMatches("ON", MatchAny, MatchAny))
  {
    COMPLETE_WITH("TO");
  }
  else if (HeadMatches("REVOKE") && TailMatches("ON", MatchAny, MatchAny))
  {
    COMPLETE_WITH("FROM");
  }

                                                                   
  else if (TailMatches("GRANT|REVOKE", MatchAny, "ON", "ALL", MatchAny, "IN", "SCHEMA", MatchAny))
  {
    if (TailMatches("GRANT", MatchAny, MatchAny, MatchAny, MatchAny, MatchAny, MatchAny, MatchAny))
    {
      COMPLETE_WITH("TO");
    }
    else
    {
      COMPLETE_WITH("FROM");
    }
  }

                                                                        
  else if (TailMatches("GRANT|REVOKE", MatchAny, "ON", "FOREIGN", "DATA", "WRAPPER", MatchAny))
  {
    if (TailMatches("GRANT", MatchAny, MatchAny, MatchAny, MatchAny, MatchAny, MatchAny))
    {
      COMPLETE_WITH("TO");
    }
    else
    {
      COMPLETE_WITH("FROM");
    }
  }

                                                                  
  else if (TailMatches("GRANT|REVOKE", MatchAny, "ON", "FOREIGN", "SERVER", MatchAny))
  {
    if (TailMatches("GRANT", MatchAny, MatchAny, MatchAny, MatchAny, MatchAny))
    {
      COMPLETE_WITH("TO");
    }
    else
    {
      COMPLETE_WITH("FROM");
    }
  }

                
  else if (TailMatches("FROM", MatchAny, "GROUP"))
  {
    COMPLETE_WITH("BY");
  }

                             
  else if (Matches("IMPORT"))
  {
    COMPLETE_WITH("FOREIGN SCHEMA");
  }
  else if (Matches("IMPORT", "FOREIGN"))
  {
    COMPLETE_WITH("SCHEMA");
  }

                                                   
                                   
  else if (TailMatches("INSERT"))
  {
    COMPLETE_WITH("INTO");
  }
                                             
  else if (TailMatches("INSERT", "INTO"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_updatables, NULL);
  }
                                                             
  else if (TailMatches("INSERT", "INTO", MatchAny, "("))
  {
    COMPLETE_WITH_ATTR(prev2_wd, "");
  }

     
                                                                      
                                                 
     
  else if (TailMatches("INSERT", "INTO", MatchAny))
  {
    COMPLETE_WITH("(", "DEFAULT VALUES", "SELECT", "TABLE", "VALUES", "OVERRIDING");
  }

     
                                                                         
                             
     
  else if (TailMatches("INSERT", "INTO", MatchAny, MatchAny) && ends_with(prev_wd, ')'))
  {
    COMPLETE_WITH("SELECT", "TABLE", "VALUES", "OVERRIDING");
  }

                           
  else if (TailMatches("OVERRIDING"))
  {
    COMPLETE_WITH("SYSTEM VALUE", "USER VALUE");
  }

                                        
  else if (TailMatches("OVERRIDING", MatchAny, "VALUE"))
  {
    COMPLETE_WITH("SELECT", "TABLE", "VALUES");
  }

                                                 
  else if (TailMatches("VALUES") && !TailMatches("DEFAULT", "VALUES"))
  {
    COMPLETE_WITH("(");
  }

            
                                                   
  else if (Matches("LOCK"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, " UNION SELECT 'TABLE'");
  }
  else if (Matches("LOCK", "TABLE"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, "");
  }

                                                                         

                                               
  else if (Matches("LOCK", MatchAnyExcept("TABLE")) || Matches("LOCK", "TABLE", MatchAny))
  {
    COMPLETE_WITH("IN");
  }

                                                         
  else if (Matches("LOCK", MatchAny, "IN") || Matches("LOCK", "TABLE", MatchAny, "IN"))
  {
    COMPLETE_WITH("ACCESS SHARE MODE", "ROW SHARE MODE", "ROW EXCLUSIVE MODE", "SHARE UPDATE EXCLUSIVE MODE", "SHARE MODE", "SHARE ROW EXCLUSIVE MODE", "EXCLUSIVE MODE", "ACCESS EXCLUSIVE MODE");
  }

                                                                          
  else if (Matches("LOCK", MatchAny, "IN", "ACCESS|ROW") || Matches("LOCK", "TABLE", MatchAny, "IN", "ACCESS|ROW"))
  {
    COMPLETE_WITH("EXCLUSIVE MODE", "SHARE MODE");
  }

                                                                     
  else if (Matches("LOCK", MatchAny, "IN", "SHARE") || Matches("LOCK", "TABLE", MatchAny, "IN", "SHARE"))
  {
    COMPLETE_WITH("MODE", "ROW EXCLUSIVE MODE", "UPDATE EXCLUSIVE MODE");
  }

                                                   
  else if (TailMatches("NOTIFY"))
  {
    COMPLETE_WITH_QUERY("SELECT pg_catalog.quote_ident(channel) FROM pg_catalog.pg_listening_channels() AS channel WHERE substring(pg_catalog.quote_ident(channel),1,%d)='%s'");
  }

               
  else if (TailMatches("OPTIONS"))
  {
    COMPLETE_WITH("(");
  }

                                                 
  else if (TailMatches("OWNER", "TO"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_roles);
  }

                
  else if (TailMatches("FROM", MatchAny, "ORDER"))
  {
    COMPLETE_WITH("BY");
  }
  else if (TailMatches("FROM", MatchAny, "ORDER", "BY"))
  {
    COMPLETE_WITH_ATTR(prev3_wd, "");
  }

                     
  else if (Matches("PREPARE", MatchAny, "AS"))
  {
    COMPLETE_WITH("SELECT", "UPDATE", "INSERT", "DELETE FROM");
  }

     
                                                                              
                                                           
     

                                    
  else if (Matches("REASSIGN"))
  {
    COMPLETE_WITH("OWNED BY");
  }
  else if (Matches("REASSIGN", "OWNED"))
  {
    COMPLETE_WITH("BY");
  }
  else if (Matches("REASSIGN", "OWNED", "BY"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_roles);
  }
  else if (Matches("REASSIGN", "OWNED", "BY", MatchAny))
  {
    COMPLETE_WITH("TO");
  }
  else if (Matches("REASSIGN", "OWNED", "BY", MatchAny, "TO"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_roles);
  }

                                 
  else if (Matches("REFRESH"))
  {
    COMPLETE_WITH("MATERIALIZED VIEW");
  }
  else if (Matches("REFRESH", "MATERIALIZED"))
  {
    COMPLETE_WITH("VIEW");
  }
  else if (Matches("REFRESH", "MATERIALIZED", "VIEW"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_matviews, " UNION SELECT 'CONCURRENTLY'");
  }
  else if (Matches("REFRESH", "MATERIALIZED", "VIEW", "CONCURRENTLY"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_matviews, NULL);
  }
  else if (Matches("REFRESH", "MATERIALIZED", "VIEW", MatchAny))
  {
    COMPLETE_WITH("WITH");
  }
  else if (Matches("REFRESH", "MATERIALIZED", "VIEW", "CONCURRENTLY", MatchAny))
  {
    COMPLETE_WITH("WITH");
  }
  else if (Matches("REFRESH", "MATERIALIZED", "VIEW", MatchAny, "WITH"))
  {
    COMPLETE_WITH("NO DATA", "DATA");
  }
  else if (Matches("REFRESH", "MATERIALIZED", "VIEW", "CONCURRENTLY", MatchAny, "WITH"))
  {
    COMPLETE_WITH("NO DATA", "DATA");
  }
  else if (Matches("REFRESH", "MATERIALIZED", "VIEW", MatchAny, "WITH", "NO"))
  {
    COMPLETE_WITH("DATA");
  }
  else if (Matches("REFRESH", "MATERIALIZED", "VIEW", "CONCURRENTLY", MatchAny, "WITH", "NO"))
  {
    COMPLETE_WITH("DATA");
  }

               
  else if (Matches("REINDEX"))
  {
    COMPLETE_WITH("TABLE", "INDEX", "SYSTEM", "SCHEMA", "DATABASE");
  }
  else if (Matches("REINDEX", "TABLE"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_indexables, " UNION SELECT 'CONCURRENTLY'");
  }
  else if (Matches("REINDEX", "INDEX"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_indexes, " UNION SELECT 'CONCURRENTLY'");
  }
  else if (Matches("REINDEX", "SCHEMA"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_schemas " UNION SELECT 'CONCURRENTLY'");
  }
  else if (Matches("REINDEX", "SYSTEM|DATABASE"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_databases " UNION SELECT 'CONCURRENTLY'");
  }
  else if (Matches("REINDEX", "TABLE", "CONCURRENTLY"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_indexables, NULL);
  }
  else if (Matches("REINDEX", "INDEX", "CONCURRENTLY"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_indexes, NULL);
  }
  else if (Matches("REINDEX", "SCHEMA", "CONCURRENTLY"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_schemas);
  }
  else if (Matches("REINDEX", "SYSTEM|DATABASE", "CONCURRENTLY"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_databases);
  }

                      
  else if (Matches("SECURITY"))
  {
    COMPLETE_WITH("LABEL");
  }
  else if (Matches("SECURITY", "LABEL"))
  {
    COMPLETE_WITH("ON", "FOR");
  }
  else if (Matches("SECURITY", "LABEL", "FOR", MatchAny))
  {
    COMPLETE_WITH("ON");
  }
  else if (Matches("SECURITY", "LABEL", "ON") || Matches("SECURITY", "LABEL", "FOR", MatchAny, "ON"))
  {
    COMPLETE_WITH("TABLE", "COLUMN", "AGGREGATE", "DATABASE", "DOMAIN", "EVENT TRIGGER", "FOREIGN TABLE", "FUNCTION", "LARGE OBJECT", "MATERIALIZED VIEW", "LANGUAGE", "PUBLICATION", "PROCEDURE", "ROLE", "ROUTINE", "SCHEMA", "SEQUENCE", "SUBSCRIPTION", "TABLESPACE", "TYPE", "VIEW");
  }
  else if (Matches("SECURITY", "LABEL", "ON", MatchAny, MatchAny))
  {
    COMPLETE_WITH("IS");
  }

              
                  

                        
                                     
  else if (TailMatches("SET|RESET") && !TailMatches("UPDATE", MatchAny, "SET"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_set_vars);
  }
  else if (Matches("SHOW"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_show_vars);
  }
                                  
  else if (Matches("SET", "TRANSACTION"))
  {
    COMPLETE_WITH("SNAPSHOT", "ISOLATION LEVEL", "READ", "DEFERRABLE", "NOT DEFERRABLE");
  }
  else if (Matches("BEGIN|START", "TRANSACTION") || Matches("BEGIN", "WORK") || Matches("BEGIN") || Matches("SET", "SESSION", "CHARACTERISTICS", "AS", "TRANSACTION"))
  {
    COMPLETE_WITH("ISOLATION LEVEL", "READ", "DEFERRABLE", "NOT DEFERRABLE");
  }
  else if (Matches("SET|BEGIN|START", "TRANSACTION|WORK", "NOT") || Matches("BEGIN", "NOT") || Matches("SET", "SESSION", "CHARACTERISTICS", "AS", "TRANSACTION", "NOT"))
  {
    COMPLETE_WITH("DEFERRABLE");
  }
  else if (Matches("SET|BEGIN|START", "TRANSACTION|WORK", "ISOLATION") || Matches("BEGIN", "ISOLATION") || Matches("SET", "SESSION", "CHARACTERISTICS", "AS", "TRANSACTION", "ISOLATION"))
  {
    COMPLETE_WITH("LEVEL");
  }
  else if (Matches("SET|BEGIN|START", "TRANSACTION|WORK", "ISOLATION", "LEVEL") || Matches("BEGIN", "ISOLATION", "LEVEL") || Matches("SET", "SESSION", "CHARACTERISTICS", "AS", "TRANSACTION", "ISOLATION", "LEVEL"))
  {
    COMPLETE_WITH("READ", "REPEATABLE READ", "SERIALIZABLE");
  }
  else if (Matches("SET|BEGIN|START", "TRANSACTION|WORK", "ISOLATION", "LEVEL", "READ") || Matches("BEGIN", "ISOLATION", "LEVEL", "READ") || Matches("SET", "SESSION", "CHARACTERISTICS", "AS", "TRANSACTION", "ISOLATION", "LEVEL", "READ"))
  {
    COMPLETE_WITH("UNCOMMITTED", "COMMITTED");
  }
  else if (Matches("SET|BEGIN|START", "TRANSACTION|WORK", "ISOLATION", "LEVEL", "REPEATABLE") || Matches("BEGIN", "ISOLATION", "LEVEL", "REPEATABLE") || Matches("SET", "SESSION", "CHARACTERISTICS", "AS", "TRANSACTION", "ISOLATION", "LEVEL", "REPEATABLE"))
  {
    COMPLETE_WITH("READ");
  }
  else if (Matches("SET|BEGIN|START", "TRANSACTION|WORK", "READ") || Matches("BEGIN", "READ") || Matches("SET", "SESSION", "CHARACTERISTICS", "AS", "TRANSACTION", "READ"))
  {
    COMPLETE_WITH("ONLY", "WRITE");
  }
                       
  else if (Matches("SET", "CONSTRAINTS"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_constraints_with_schema, "UNION SELECT 'ALL'");
  }
                                                              
  else if (Matches("SET", "CONSTRAINTS", MatchAny))
  {
    COMPLETE_WITH("DEFERRED", "IMMEDIATE");
  }
                         
  else if (Matches("SET", "ROLE"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_roles);
  }
                                                                     
  else if (Matches("SET", "SESSION"))
  {
    COMPLETE_WITH("AUTHORIZATION", "CHARACTERISTICS AS TRANSACTION");
  }
                                                        
  else if (Matches("SET", "SESSION", "AUTHORIZATION"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_roles " UNION SELECT 'DEFAULT'");
  }
                                                 
  else if (Matches("RESET", "SESSION"))
  {
    COMPLETE_WITH("AUTHORIZATION");
  }
                                    
  else if (Matches("SET", MatchAny))
  {
    COMPLETE_WITH("TO");
  }

     
                                                                          
            
     
  else if (HeadMatches("ALTER", "DATABASE|FUNCTION|PROCEDURE|ROLE|ROUTINE|USER") && TailMatches("SET", MatchAny) && !TailMatches("SCHEMA"))
  {
    COMPLETE_WITH("FROM CURRENT", "TO");
  }

     
                                                                           
                               
     
  else if (TailMatches("SET", MatchAny, "TO|=") && !TailMatches("UPDATE", MatchAny, "SET", MatchAny, "TO|="))
  {
                                                
    if (TailMatches("DateStyle", "TO|="))
    {
      COMPLETE_WITH("ISO", "SQL", "Postgres", "German", "YMD", "DMY", "MDY", "US", "European", "NonEuropean", "DEFAULT");
    }
    else if (TailMatches("search_path", "TO|="))
    {
      COMPLETE_WITH_QUERY(Query_for_list_of_schemas " AND nspname not like 'pg\\_toast%%' "
                                                    " AND nspname not like 'pg\\_temp%%' "
                                                    " UNION SELECT 'DEFAULT' ");
    }
    else
    {
                                            
      char *guctype = get_guctype(prev2_wd);

         
                                                                         
                                                                        
                                                              
         
      if (guctype)
      {
        if (strcmp(guctype, "enum") == 0)
        {
          char querybuf[1024];

          snprintf(querybuf, sizeof(querybuf), Query_for_enum, prev2_wd);
          COMPLETE_WITH_QUERY(querybuf);
        }
        else if (strcmp(guctype, "bool") == 0)
        {
          COMPLETE_WITH("on", "off", "true", "false", "yes", "no", "1", "0", "DEFAULT");
        }
        else
        {
          COMPLETE_WITH("DEFAULT");
        }

        free(guctype);
      }
    }
  }

                         
  else if (Matches("START"))
  {
    COMPLETE_WITH("TRANSACTION");
  }

                                                       
  else if (Matches("TABLE"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_selectables, NULL);
  }

                   
  else if (TailMatches("TABLESAMPLE"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_tablesample_methods);
  }
  else if (TailMatches("TABLESAMPLE", MatchAny))
  {
    COMPLETE_WITH("(");
  }

                
  else if (Matches("TRUNCATE"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, NULL);
  }

                
  else if (Matches("UNLISTEN"))
  {
    COMPLETE_WITH_QUERY("SELECT pg_catalog.quote_ident(channel) FROM pg_catalog.pg_listening_channels() AS channel WHERE substring(pg_catalog.quote_ident(channel),1,%d)='%s' UNION SELECT '*'");
  }

                                                   
                                                        
  else if (TailMatches("UPDATE"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_updatables, NULL);
  }
                                          
  else if (TailMatches("UPDATE", MatchAny))
  {
    COMPLETE_WITH("SET");
  }
                                                           
  else if (TailMatches("UPDATE", MatchAny, "SET"))
  {
    COMPLETE_WITH_ATTR(prev2_wd, "");
  }
                                   
  else if (TailMatches("UPDATE", MatchAny, "SET", MatchAny))
  {
    COMPLETE_WITH("=");
  }

                    
  else if (Matches("ALTER|CREATE|DROP", "USER", "MAPPING"))
  {
    COMPLETE_WITH("FOR");
  }
  else if (Matches("CREATE", "USER", "MAPPING", "FOR"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_roles " UNION SELECT 'CURRENT_USER'"
                                                " UNION SELECT 'PUBLIC'"
                                                " UNION SELECT 'USER'");
  }
  else if (Matches("ALTER|DROP", "USER", "MAPPING", "FOR"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_user_mappings);
  }
  else if (Matches("CREATE|ALTER|DROP", "USER", "MAPPING", "FOR", MatchAny))
  {
    COMPLETE_WITH("SERVER");
  }
  else if (Matches("CREATE|ALTER", "USER", "MAPPING", "FOR", MatchAny, "SERVER", MatchAny))
  {
    COMPLETE_WITH("OPTIONS");
  }

     
                                                                 
                                                                                      
     
  else if (Matches("VACUUM"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_vacuumables, " UNION SELECT 'FULL'"
                                                              " UNION SELECT 'FREEZE'"
                                                              " UNION SELECT 'ANALYZE'"
                                                              " UNION SELECT 'VERBOSE'");
  }
  else if (Matches("VACUUM", "FULL"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_vacuumables, " UNION SELECT 'FREEZE'"
                                                              " UNION SELECT 'ANALYZE'"
                                                              " UNION SELECT 'VERBOSE'");
  }
  else if (Matches("VACUUM", "FREEZE") || Matches("VACUUM", "FULL", "FREEZE"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_vacuumables, " UNION SELECT 'VERBOSE'"
                                                              " UNION SELECT 'ANALYZE'");
  }
  else if (Matches("VACUUM", "VERBOSE") || Matches("VACUUM", "FULL|FREEZE", "VERBOSE") || Matches("VACUUM", "FULL", "FREEZE", "VERBOSE"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_vacuumables, " UNION SELECT 'ANALYZE'");
  }
  else if (HeadMatches("VACUUM", "(*") && !HeadMatches("VACUUM", "(*)"))
  {
       
                                                                       
                                                                          
                                               
       
    if (ends_with(prev_wd, '(') || ends_with(prev_wd, ','))
    {
      COMPLETE_WITH("FULL", "FREEZE", "ANALYZE", "VERBOSE", "DISABLE_PAGE_SKIPPING", "SKIP_LOCKED", "INDEX_CLEANUP", "TRUNCATE");
    }
    else if (TailMatches("FULL|FREEZE|ANALYZE|VERBOSE|DISABLE_PAGE_SKIPPING|SKIP_LOCKED|INDEX_CLEANUP|TRUNCATE"))
    {
      COMPLETE_WITH("ON", "OFF");
    }
  }
  else if (HeadMatches("VACUUM") && TailMatches("("))
  {
                                                                      
    COMPLETE_WITH_ATTR(prev2_wd, "");
  }
  else if (HeadMatches("VACUUM"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_vacuumables, NULL);
  }

                        

     
                                                                        
                     
     
  else if (Matches("WITH"))
  {
    COMPLETE_WITH("RECURSIVE");
  }

             
                                                                     
  else if (TailMatches(MatchAny, "WHERE"))
  {
    COMPLETE_WITH_ATTR(prev2_wd, "");
  }

                    
                                
  else if (TailMatches("FROM") && !Matches("COPY|\\copy", MatchAny, "FROM"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_selectables, NULL);
  }

                    
  else if (TailMatches("JOIN"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_selectables, NULL);
  }

                          
                          
  else if (TailMatchesCS("\\?"))
  {
    COMPLETE_WITH_CS("commands", "options", "variables");
  }
  else if (TailMatchesCS("\\connect|\\c"))
  {
    if (!recognized_connection_string(text))
    {
      COMPLETE_WITH_QUERY(Query_for_list_of_databases);
    }
  }
  else if (TailMatchesCS("\\connect|\\c", MatchAny))
  {
    if (!recognized_connection_string(prev_wd))
    {
      COMPLETE_WITH_QUERY(Query_for_list_of_roles);
    }
  }
  else if (TailMatchesCS("\\da*"))
  {
    COMPLETE_WITH_VERSIONED_SCHEMA_QUERY(Query_for_list_of_aggregates, NULL);
  }
  else if (TailMatchesCS("\\dA*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_access_methods);
  }
  else if (TailMatchesCS("\\db*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_tablespaces);
  }
  else if (TailMatchesCS("\\dD*"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_domains, NULL);
  }
  else if (TailMatchesCS("\\des*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_servers);
  }
  else if (TailMatchesCS("\\deu*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_user_mappings);
  }
  else if (TailMatchesCS("\\dew*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_fdws);
  }
  else if (TailMatchesCS("\\df*"))
  {
    COMPLETE_WITH_VERSIONED_SCHEMA_QUERY(Query_for_list_of_functions, NULL);
  }

  else if (TailMatchesCS("\\dFd*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_ts_dictionaries);
  }
  else if (TailMatchesCS("\\dFp*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_ts_parsers);
  }
  else if (TailMatchesCS("\\dFt*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_ts_templates);
  }
                                           
  else if (TailMatchesCS("\\dF*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_ts_configurations);
  }

  else if (TailMatchesCS("\\di*"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_indexes, NULL);
  }
  else if (TailMatchesCS("\\dL*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_languages);
  }
  else if (TailMatchesCS("\\dn*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_schemas);
  }
  else if (TailMatchesCS("\\dp") || TailMatchesCS("\\z"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_grantables, NULL);
  }
  else if (TailMatchesCS("\\dPi*"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_partitioned_indexes, NULL);
  }
  else if (TailMatchesCS("\\dPt*"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_partitioned_tables, NULL);
  }
  else if (TailMatchesCS("\\dP*"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_partitioned_relations, NULL);
  }
  else if (TailMatchesCS("\\ds*"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_sequences, NULL);
  }
  else if (TailMatchesCS("\\dt*"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_tables, NULL);
  }
  else if (TailMatchesCS("\\dT*"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_datatypes, NULL);
  }
  else if (TailMatchesCS("\\du*") || TailMatchesCS("\\dg*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_roles);
  }
  else if (TailMatchesCS("\\dv*"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_views, NULL);
  }
  else if (TailMatchesCS("\\dx*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_extensions);
  }
  else if (TailMatchesCS("\\dm*"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_matviews, NULL);
  }
  else if (TailMatchesCS("\\dE*"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_foreign_tables, NULL);
  }
  else if (TailMatchesCS("\\dy*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_event_triggers);
  }

                                          
  else if (TailMatchesCS("\\d*"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_relations, NULL);
  }

  else if (TailMatchesCS("\\ef"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_routines, NULL);
  }
  else if (TailMatchesCS("\\ev"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_views, NULL);
  }

  else if (TailMatchesCS("\\encoding"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_encodings);
  }
  else if (TailMatchesCS("\\h|\\help"))
  {
    COMPLETE_WITH_LIST(sql_commands);
  }
  else if (TailMatchesCS("\\h|\\help", MatchAny))
  {
    if (TailMatches("DROP"))
    {
      matches = completion_matches(text, drop_command_generator);
    }
    else if (TailMatches("ALTER"))
    {
      matches = completion_matches(text, alter_command_generator);
    }

       
                                                                           
                     
       
  }
  else if (TailMatchesCS("\\h|\\help", MatchAny, MatchAny))
  {
    if (TailMatches("CREATE|DROP", "ACCESS"))
    {
      COMPLETE_WITH("METHOD");
    }
    else if (TailMatches("ALTER", "DEFAULT"))
    {
      COMPLETE_WITH("PRIVILEGES");
    }
    else if (TailMatches("CREATE|ALTER|DROP", "EVENT"))
    {
      COMPLETE_WITH("TRIGGER");
    }
    else if (TailMatches("CREATE|ALTER|DROP", "FOREIGN"))
    {
      COMPLETE_WITH("DATA WRAPPER", "TABLE");
    }
    else if (TailMatches("ALTER", "LARGE"))
    {
      COMPLETE_WITH("OBJECT");
    }
    else if (TailMatches("CREATE|ALTER|DROP", "MATERIALIZED"))
    {
      COMPLETE_WITH("VIEW");
    }
    else if (TailMatches("CREATE|ALTER|DROP", "TEXT"))
    {
      COMPLETE_WITH("SEARCH");
    }
    else if (TailMatches("CREATE|ALTER|DROP", "USER"))
    {
      COMPLETE_WITH("MAPPING FOR");
    }
  }
  else if (TailMatchesCS("\\h|\\help", MatchAny, MatchAny, MatchAny))
  {
    if (TailMatches("CREATE|ALTER|DROP", "FOREIGN", "DATA"))
    {
      COMPLETE_WITH("WRAPPER");
    }
    else if (TailMatches("CREATE|ALTER|DROP", "TEXT", "SEARCH"))
    {
      COMPLETE_WITH("CONFIGURATION", "DICTIONARY", "PARSER", "TEMPLATE");
    }
    else if (TailMatches("CREATE|ALTER|DROP", "USER", "MAPPING"))
    {
      COMPLETE_WITH("FOR");
    }
  }
  else if (TailMatchesCS("\\l*") && !TailMatchesCS("\\lo*"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_databases);
  }
  else if (TailMatchesCS("\\password"))
  {
    COMPLETE_WITH_QUERY(Query_for_list_of_roles);
  }
  else if (TailMatchesCS("\\pset"))
  {
    COMPLETE_WITH_CS("border", "columns", "csv_fieldsep", "expanded", "fieldsep", "fieldsep_zero", "footer", "format", "linestyle", "null", "numericlocale", "pager", "pager_min_lines", "recordsep", "recordsep_zero", "tableattr", "title", "tuples_only", "unicode_border_linestyle", "unicode_column_linestyle", "unicode_header_linestyle");
  }
  else if (TailMatchesCS("\\pset", MatchAny))
  {
    if (TailMatchesCS("format"))
    {
      COMPLETE_WITH_CS("aligned", "asciidoc", "csv", "html", "latex", "latex-longtable", "troff-ms", "unaligned", "wrapped");
    }
    else if (TailMatchesCS("linestyle"))
    {
      COMPLETE_WITH_CS("ascii", "old-ascii", "unicode");
    }
    else if (TailMatchesCS("pager"))
    {
      COMPLETE_WITH_CS("on", "off", "always");
    }
    else if (TailMatchesCS("unicode_border_linestyle|"
                           "unicode_column_linestyle|"
                           "unicode_header_linestyle"))
    {
      COMPLETE_WITH_CS("single", "double");
    }
  }
  else if (TailMatchesCS("\\unset"))
  {
    matches = complete_from_variables(text, "", "", true);
  }
  else if (TailMatchesCS("\\set"))
  {
    matches = complete_from_variables(text, "", "", false);
  }
  else if (TailMatchesCS("\\set", MatchAny))
  {
    if (TailMatchesCS("AUTOCOMMIT|ON_ERROR_STOP|QUIET|"
                      "SINGLELINE|SINGLESTEP"))
    {
      COMPLETE_WITH_CS("on", "off");
    }
    else if (TailMatchesCS("COMP_KEYWORD_CASE"))
    {
      COMPLETE_WITH_CS("lower", "upper", "preserve-lower", "preserve-upper");
    }
    else if (TailMatchesCS("ECHO"))
    {
      COMPLETE_WITH_CS("errors", "queries", "all", "none");
    }
    else if (TailMatchesCS("ECHO_HIDDEN"))
    {
      COMPLETE_WITH_CS("noexec", "off", "on");
    }
    else if (TailMatchesCS("HISTCONTROL"))
    {
      COMPLETE_WITH_CS("ignorespace", "ignoredups", "ignoreboth", "none");
    }
    else if (TailMatchesCS("ON_ERROR_ROLLBACK"))
    {
      COMPLETE_WITH_CS("on", "off", "interactive");
    }
    else if (TailMatchesCS("SHOW_CONTEXT"))
    {
      COMPLETE_WITH_CS("never", "errors", "always");
    }
    else if (TailMatchesCS("VERBOSITY"))
    {
      COMPLETE_WITH_CS("default", "verbose", "terse", "sqlstate");
    }
  }
  else if (TailMatchesCS("\\sf*"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_routines, NULL);
  }
  else if (TailMatchesCS("\\sv*"))
  {
    COMPLETE_WITH_SCHEMA_QUERY(Query_for_list_of_views, NULL);
  }
  else if (TailMatchesCS("\\cd|\\e|\\edit|\\g|\\gx|\\i|\\include|"
                         "\\ir|\\include_relative|\\o|\\out|"
                         "\\s|\\w|\\write|\\lo_import"))
  {
    completion_charp = "\\";
    matches = completion_matches(text, complete_from_files);
  }

     
                                                                             
                                                                            
                   
     
  else
  {
    int i;

    for (i = 0; words_after_create[i].name; i++)
    {
      if (pg_strcasecmp(prev_wd, words_after_create[i].name) == 0)
      {
        if (words_after_create[i].query)
        {
          COMPLETE_WITH_QUERY(words_after_create[i].query);
        }
        else if (words_after_create[i].vquery)
        {
          COMPLETE_WITH_VERSIONED_QUERY(words_after_create[i].vquery);
        }
        else if (words_after_create[i].squery)
        {
          COMPLETE_WITH_VERSIONED_SCHEMA_QUERY(words_after_create[i].squery, NULL);
        }
        break;
      }
    }
  }

     
                                                                             
                                                                             
                                                               
     
  if (matches == NULL)
  {
    COMPLETE_WITH_CONST(true, "");
#ifdef HAVE_RL_COMPLETION_APPEND_CHARACTER
    rl_completion_append_character = '\0';
#endif
  }

                    
  free(previous_words);
  free(words_buffer);

                                        
  return matches;
}

   
                       
   
                                                                            
                                                                             
                              
                                                                           
                                                
                                                                             
                                                                              
                                                                        
                           
   

   
                                                                           
                                                        
   
static char *
create_or_drop_command_generator(const char *text, int state, bits32 excluded)
{
  static int list_index, string_length;
  const char *name;

                                                                       
  if (state == 0)
  {
    list_index = 0;
    string_length = strlen(text);
  }

                                   
  while ((name = words_after_create[list_index++].name))
  {
    if ((pg_strncasecmp(name, text, string_length) == 0) && !(words_after_create[list_index - 1].flags & excluded))
    {
      return pg_strdup_keyword_case(name, text);
    }
  }
                                       
  return NULL;
}

   
                                                                         
                     
   
static char *
create_command_generator(const char *text, int state)
{
  return create_or_drop_command_generator(text, state, THING_NO_CREATE);
}

   
                                                                              
   
static char *
drop_command_generator(const char *text, int state)
{
  return create_or_drop_command_generator(text, state, THING_NO_DROP);
}

   
                                                                                
   
static char *
alter_command_generator(const char *text, int state)
{
  return create_or_drop_command_generator(text, state, THING_NO_ALTER);
}

   
                                                        
                                                   
   

static char *
complete_from_query(const char *text, int state)
{
                                                       
  return _complete_from_query(completion_charp, NULL, text, state);
}

static char *
complete_from_versioned_query(const char *text, int state)
{
  const VersionedQuery *vquery = completion_vquery;

                                      
  while (pset.sversion < vquery->min_server_version)
  {
    vquery++;
  }
                                            
  if (vquery->query == NULL)
  {
    return NULL;
  }

  return _complete_from_query(vquery->query, NULL, text, state);
}

static char *
complete_from_schema_query(const char *text, int state)
{
                                                       
  return _complete_from_query(completion_charp, completion_squery, text, state);
}

static char *
complete_from_versioned_schema_query(const char *text, int state)
{
  const SchemaQuery *squery = completion_squery;
  const VersionedQuery *vquery = completion_vquery;

                                      
  while (pset.sversion < squery->min_server_version)
  {
    squery++;
  }
                                            
  if (squery->catname == NULL)
  {
    return NULL;
  }

                                            
  if (vquery)
  {
    while (pset.sversion < vquery->min_server_version)
    {
      vquery++;
    }
    if (vquery->query == NULL)
    {
      return NULL;
    }
  }

  return _complete_from_query(vquery ? vquery->query : NULL, squery, text, state);
}

   
                                                                             
                                                                          
                                                          
   
                                      
   
                                                                              
                                                                            
                                                                             
                                                                
                           
   
                                                                            
                                                                   
                              
                                                                     
   
                                                                         
                                                                            
   
                                                                       
                                                            
   
                                                        
   
                                                
   
static char *
_complete_from_query(const char *simple_query, const SchemaQuery *schema_query, const char *text, int state)
{
  static int list_index, byte_length;
  static PGresult *result = NULL;

     
                                                                           
                                
     
  if (state == 0)
  {
    PQExpBufferData query_buffer;
    char *e_text;
    char *e_info_charp;
    char *e_info_charp2;
    const char *pstr = text;
    int char_length = 0;

    list_index = 0;
    byte_length = strlen(text);

       
                                                                        
                 
       
    while (*pstr)
    {
      char_length++;
      pstr += PQmblenBounded(pstr, pset.encoding);
    }

                               
    PQclear(result);
    result = NULL;

                                                          
    e_text = escape_string(text);

    if (completion_info_charp)
    {
      e_info_charp = escape_string(completion_info_charp);
    }
    else
    {
      e_info_charp = NULL;
    }

    if (completion_info_charp2)
    {
      e_info_charp2 = escape_string(completion_info_charp2);
    }
    else
    {
      e_info_charp2 = NULL;
    }

    initPQExpBuffer(&query_buffer);

    if (schema_query)
    {
                                                        
      const char *qualresult = schema_query->qualresult;

      if (qualresult == NULL)
      {
        qualresult = schema_query->result;
      }

                                                           
      appendPQExpBuffer(&query_buffer, "SELECT %s FROM %s WHERE ", schema_query->result, schema_query->catname);
      if (schema_query->selcondition)
      {
        appendPQExpBuffer(&query_buffer, "%s AND ", schema_query->selcondition);
      }
      appendPQExpBuffer(&query_buffer, "substring(%s,1,%d)='%s'", schema_query->result, char_length, e_text);
      appendPQExpBuffer(&query_buffer, " AND %s", schema_query->viscondition);

         
                                                                       
                                                                   
                                                                         
                                                                  
         
      if (strcmp(schema_query->catname, "pg_catalog.pg_class c") == 0 && strncmp(text, "pg_", 3) != 0)
      {
        appendPQExpBufferStr(&query_buffer, " AND c.relnamespace <> (SELECT oid FROM"
                                            " pg_catalog.pg_namespace WHERE nspname = 'pg_catalog')");
      }

         
                                                                      
                                                 
         
      appendPQExpBuffer(&query_buffer,
          "\nUNION\n"
          "SELECT pg_catalog.quote_ident(n.nspname) || '.' "
          "FROM pg_catalog.pg_namespace n "
          "WHERE substring(pg_catalog.quote_ident(n.nspname) || '.',1,%d)='%s'",
          char_length, e_text);
      appendPQExpBuffer(&query_buffer,
          " AND (SELECT pg_catalog.count(*)"
          " FROM pg_catalog.pg_namespace"
          " WHERE substring(pg_catalog.quote_ident(nspname) || '.',1,%d) ="
          " substring('%s',1,pg_catalog.length(pg_catalog.quote_ident(nspname))+1)) > 1",
          char_length, e_text);

         
                                                                       
                                               
         
      appendPQExpBuffer(&query_buffer,
          "\nUNION\n"
          "SELECT pg_catalog.quote_ident(n.nspname) || '.' || %s "
          "FROM %s, pg_catalog.pg_namespace n "
          "WHERE %s = n.oid AND ",
          qualresult, schema_query->catname, schema_query->namespace);
      if (schema_query->selcondition)
      {
        appendPQExpBuffer(&query_buffer, "%s AND ", schema_query->selcondition);
      }
      appendPQExpBuffer(&query_buffer, "substring(pg_catalog.quote_ident(n.nspname) || '.' || %s,1,%d)='%s'", qualresult, char_length, e_text);

         
                                                                    
                            
         
      appendPQExpBuffer(&query_buffer,
          " AND substring(pg_catalog.quote_ident(n.nspname) || '.',1,%d) ="
          " substring('%s',1,pg_catalog.length(pg_catalog.quote_ident(n.nspname))+1)",
          char_length, e_text);
      appendPQExpBuffer(&query_buffer,
          " AND (SELECT pg_catalog.count(*)"
          " FROM pg_catalog.pg_namespace"
          " WHERE substring(pg_catalog.quote_ident(nspname) || '.',1,%d) ="
          " substring('%s',1,pg_catalog.length(pg_catalog.quote_ident(nspname))+1)) = 1",
          char_length, e_text);

                                                  
      if (simple_query)
      {
        appendPQExpBuffer(&query_buffer, "\n%s", simple_query);
      }
    }
    else
    {
      Assert(simple_query);
                                                          
      appendPQExpBuffer(&query_buffer, simple_query, char_length, e_text, e_info_charp, e_info_charp, e_info_charp2, e_info_charp2);
    }

                                                   
    appendPQExpBuffer(&query_buffer, "\nLIMIT %d", completion_max_records);

    result = exec_query(query_buffer.data);

    termPQExpBuffer(&query_buffer);
    free(e_text);
    if (e_info_charp)
    {
      free(e_info_charp);
    }
    if (e_info_charp2)
    {
      free(e_info_charp2);
    }
  }

                                   
  if (result && PQresultStatus(result) == PGRES_TUPLES_OK)
  {
    const char *item;

    while (list_index < PQntuples(result) && (item = PQgetvalue(result, list_index++, 0)))
    {
      if (pg_strncasecmp(text, item, byte_length) == 0)
      {
        return pg_strdup(item);
      }
    }
  }

                                                                 
  PQclear(result);
  result = NULL;
  return NULL;
}

   
                                                                               
                                                                               
                                              
   
static char *
complete_from_list(const char *text, int state)
{
  static int string_length, list_index, matches;
  static bool casesensitive;
  const char *item;

                           
  Assert(completion_charpp != NULL);

                      
  if (state == 0)
  {
    list_index = 0;
    string_length = strlen(text);
    casesensitive = completion_case_sensitive;
    matches = 0;
  }

  while ((item = completion_charpp[list_index++]))
  {
                                      
    if (casesensitive && strncmp(text, item, string_length) == 0)
    {
      matches++;
      return pg_strdup(item);
    }

                                                                        
    if (!casesensitive && pg_strncasecmp(text, item, string_length) == 0)
    {
      if (completion_case_sensitive)
      {
        return pg_strdup(item);
      }
      else
      {

           
                                                                 
                                                 
           
        return pg_strdup_keyword_case(item, text);
      }
    }
  }

     
                                                                             
                                          
     
  if (casesensitive && matches == 0)
  {
    casesensitive = false;
    list_index = 0;
    state++;
    return complete_from_list(text, state);
  }

                                        
  return NULL;
}

   
                                                                            
                                                                
                                           
   
                                                                         
                                                                            
                                                
   
                                                                        
                                                                       
                                                                       
                                                                        
                                                                      
                                                                     
                                                                      
                                                  
   
static char *
complete_from_const(const char *text, int state)
{
  Assert(completion_charp != NULL);
  if (state == 0)
  {
    if (completion_case_sensitive)
    {
      return pg_strdup(completion_charp);
    }
    else
    {

         
                                                                      
                                        
         
      return pg_strdup_keyword_case(completion_charp, text);
    }
  }
  else
  {
    return NULL;
  }
}

   
                                                                     
                             
   
static void
append_variable_names(char ***varnames, int *nvars, int *maxvars, const char *varname, const char *prefix, const char *suffix)
{
  if (*nvars >= *maxvars)
  {
    *maxvars *= 2;
    *varnames = (char **)pg_realloc(*varnames, ((*maxvars) + 1) * sizeof(char *));
  }

  (*varnames)[(*nvars)++] = psprintf("%s%s%s", prefix, varname, suffix);
}

   
                                                                       
                                                                        
                                                                    
                                                                     
                                                                 
   
static char **
complete_from_variables(const char *text, const char *prefix, const char *suffix, bool need_value)
{
  char **matches;
  char **varnames;
  int nvars = 0;
  int maxvars = 100;
  int i;
  struct _variable *ptr;

  varnames = (char **)pg_malloc((maxvars + 1) * sizeof(char *));

  for (ptr = pset.vars->next; ptr; ptr = ptr->next)
  {
    if (need_value && !(ptr->value))
    {
      continue;
    }
    append_variable_names(&varnames, &nvars, &maxvars, ptr->name, prefix, suffix);
  }

  varnames[nvars] = NULL;
  COMPLETE_WITH_LIST_CS((const char *const *)varnames);

  for (i = 0; i < nvars; i++)
  {
    free(varnames[i]);
  }
  free(varnames);

  return matches;
}

   
                                                                              
                                                                             
                                          
   
static char *
complete_from_files(const char *text, int state)
{
  static const char *unquoted_text;
  char *unquoted_match;
  char *ret = NULL;

  if (state == 0)
  {
                                                   
    unquoted_text = strtokx(text, "", NULL, "'", *completion_charp, false, true, pset.encoding);
                                                        
    if (!unquoted_text)
    {
      Assert(*text == '\0');
      unquoted_text = text;
    }
  }

  unquoted_match = filename_completion_function(unquoted_text, state);
  if (unquoted_match)
  {
       
                                                                       
                                                                           
                                                                         
                                                                          
                                                  
       
    ret = quote_if_needed(unquoted_match, " \t\r\n\"`", '\'', *completion_charp, pset.encoding);
    if (ret)
    {
      free(unquoted_match);
    }
    else
    {
      ret = unquoted_match;
    }
  }

  return ret;
}

                      

   
                                                                
                                                                              
   
static char *
pg_strdup_keyword_case(const char *s, const char *ref)
{
  char *ret, *p;
  unsigned char first = ref[0];

  ret = pg_strdup(s);

  if (pset.comp_case == PSQL_COMP_CASE_LOWER || ((pset.comp_case == PSQL_COMP_CASE_PRESERVE_LOWER || pset.comp_case == PSQL_COMP_CASE_PRESERVE_UPPER) && islower(first)) || (pset.comp_case == PSQL_COMP_CASE_PRESERVE_LOWER && !isalpha(first)))
  {
    for (p = ret; *p; p++)
    {
      *p = pg_tolower((unsigned char)*p);
    }
  }
  else
  {
    for (p = ret; *p; p++)
    {
      *p = pg_toupper((unsigned char)*p);
    }
  }

  return ret;
}

   
                                                              
   
                                       
   
static char *
escape_string(const char *text)
{
  size_t text_length;
  char *result;

  text_length = strlen(text);

  result = pg_malloc(text_length * 2 + 1);
  PQescapeStringConn(pset.db, result, text, text_length, NULL);

  return result;
}

   
                                                                              
                                         
   
static PGresult *
exec_query(const char *query)
{
  PGresult *result;

  if (query == NULL || !pset.db || PQstatus(pset.db) != CONNECTION_OK)
  {
    return NULL;
  }

  result = PQexec(pset.db, query);

  if (PQresultStatus(result) != PGRES_TUPLES_OK)
  {
#ifdef NOT_USED
    pg_log_error("tab completion query failed: %s\nQuery was:\n%s", PQerrorMessage(pset.db), query);
#endif
    PQclear(result);
    result = NULL;
  }

  return result;
}

   
                                       
   
                                                                               
                                                                               
                                                                           
                 
   
                                                                              
                                                               
   
static char **
get_previous_words(int point, char **buffer, int *nwords)
{
  char **previous_words;
  char *buf;
  char *outptr;
  int words_found = 0;
  int i;

     
                                                                             
                                                                            
                                         
     
  if (tab_completion_query_buf && tab_completion_query_buf->len > 0)
  {
    i = tab_completion_query_buf->len;
    buf = pg_malloc(point + i + 2);
    memcpy(buf, tab_completion_query_buf->data, i);
    buf[i++] = '\n';
    memcpy(buf + i, rl_line_buffer, point);
    i += point;
    buf[i] = '\0';
                                                               
    point = i;
  }
  else
  {
    buf = rl_line_buffer;
  }

     
                                                                           
                                                                
                                                                             
                                                                         
                                              
     
  previous_words = (char **)pg_malloc(point * sizeof(char *));
  *buffer = outptr = (char *)pg_malloc(point * 2);

     
                                                                           
                                                                            
                                       
     
  for (i = point - 1; i >= 0; i--)
  {
    if (strchr(WORD_BREAKS, buf[i]))
    {
      break;
    }
  }
  point = i;

     
                                                                          
                                                                    
                                      
     
  while (point >= 0)
  {
    int start, end;
    bool inquotes = false;
    int parentheses = 0;

                                                                     
    end = -1;
    for (i = point; i >= 0; i--)
    {
      if (!isspace((unsigned char)buf[i]))
      {
        end = i;
        break;
      }
    }
                                     
    if (end < 0)
    {
      break;
    }

       
                                                                          
                                                                          
                                                                        
                    
       
    for (start = end; start > 0; start--)
    {
      if (buf[start] == '"')
      {
        inquotes = !inquotes;
      }
      if (!inquotes)
      {
        if (buf[start] == ')')
        {
          parentheses++;
        }
        else if (buf[start] == '(')
        {
          if (--parentheses <= 0)
          {
            break;
          }
        }
        else if (parentheses == 0 && strchr(WORD_BREAKS, buf[start - 1]))
        {
          break;
        }
      }
    }

                                                           
    previous_words[words_found++] = outptr;
    i = end - start + 1;
    memcpy(outptr, &buf[start], i);
    outptr += i;
    *outptr++ = '\0';

                            
    point = start - 1;
  }

                                                             
  if (buf != rl_line_buffer)
  {
    free(buf);
  }

  *nwords = words_found;
  return previous_words;
}

   
                                                               
   
                                                                           
                                         
   
static char *
get_guctype(const char *varname)
{
  PQExpBufferData query_buffer;
  char *e_varname;
  PGresult *result;
  char *guctype = NULL;

  e_varname = escape_string(varname);

  initPQExpBuffer(&query_buffer);
  appendPQExpBuffer(&query_buffer,
      "SELECT vartype FROM pg_catalog.pg_settings "
      "WHERE pg_catalog.lower(name) = pg_catalog.lower('%s')",
      e_varname);

  result = exec_query(query_buffer.data);
  termPQExpBuffer(&query_buffer);
  free(e_varname);

  if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0)
  {
    guctype = pg_strdup(PQgetvalue(result, 0, 0));
  }

  PQclear(result);

  return guctype;
}

#ifdef NOT_USED

   
                                                                     
                                                                   
                                                
   
static char *
quote_file_name(char *text, int match_type, char *quote_pointer)
{
  char *s;
  size_t length;

  (void)quote_pointer;               

  length = strlen(text) + (match_type == SINGLE_MATCH ? 3 : 2);
  s = pg_malloc(length);
  s[0] = '\'';
  strcpy(s + 1, text);
  if (match_type == SINGLE_MATCH)
  {
    s[length - 2] = '\'';
  }
  s[length - 1] = '\0';
  return s;
}

static char *
dequote_file_name(char *text, char quote_char)
{
  char *s;
  size_t length;

  if (!quote_char)
  {
    return pg_strdup(text);
  }

  length = strlen(text);
  s = pg_malloc(length - 2 + 1);
  strlcpy(s, text + 1, length - 2 + 1);

  return s;
}
#endif               

#endif                   
