                                                                            
   
              
                                                     
   
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   

#include "postgres.h"

#include <ctype.h>

#include "access/htup_details.h"
#include "access/parallel.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/pg_authid.h"
#include "commands/variable.h"
#include "miscadmin.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "utils/snapmgr.h"
#include "utils/timestamp.h"
#include "utils/varlena.h"
#include "mb/pg_wchar.h"

   
             
   

   
                                                 
   
bool
check_datestyle(char **newval, void **extra, GucSource source)
{
  int newDateStyle = DateStyle;
  int newDateOrder = DateOrder;
  bool have_style = false;
  bool have_order = false;
  bool ok = true;
  char *rawstring;
  int *myextra;
  char *result;
  List *elemlist;
  ListCell *l;

                                        
  rawstring = pstrdup(*newval);

                                             
  if (!SplitIdentifierString(rawstring, ',', &elemlist))
  {
                              
    GUC_check_errdetail("List syntax is invalid.");
    pfree(rawstring);
    list_free(elemlist);
    return false;
  }

  foreach (l, elemlist)
  {
    char *tok = (char *)lfirst(l);

                                                                    

    if (pg_strcasecmp(tok, "ISO") == 0)
    {
      if (have_style && newDateStyle != USE_ISO_DATES)
      {
        ok = false;                         
      }
      newDateStyle = USE_ISO_DATES;
      have_style = true;
    }
    else if (pg_strcasecmp(tok, "SQL") == 0)
    {
      if (have_style && newDateStyle != USE_SQL_DATES)
      {
        ok = false;                         
      }
      newDateStyle = USE_SQL_DATES;
      have_style = true;
    }
    else if (pg_strncasecmp(tok, "POSTGRES", 8) == 0)
    {
      if (have_style && newDateStyle != USE_POSTGRES_DATES)
      {
        ok = false;                         
      }
      newDateStyle = USE_POSTGRES_DATES;
      have_style = true;
    }
    else if (pg_strcasecmp(tok, "GERMAN") == 0)
    {
      if (have_style && newDateStyle != USE_GERMAN_DATES)
      {
        ok = false;                         
      }
      newDateStyle = USE_GERMAN_DATES;
      have_style = true;
                                                              
      if (!have_order)
      {
        newDateOrder = DATEORDER_DMY;
      }
    }
    else if (pg_strcasecmp(tok, "YMD") == 0)
    {
      if (have_order && newDateOrder != DATEORDER_YMD)
      {
        ok = false;                         
      }
      newDateOrder = DATEORDER_YMD;
      have_order = true;
    }
    else if (pg_strcasecmp(tok, "DMY") == 0 || pg_strncasecmp(tok, "EURO", 4) == 0)
    {
      if (have_order && newDateOrder != DATEORDER_DMY)
      {
        ok = false;                         
      }
      newDateOrder = DATEORDER_DMY;
      have_order = true;
    }
    else if (pg_strcasecmp(tok, "MDY") == 0 || pg_strcasecmp(tok, "US") == 0 || pg_strncasecmp(tok, "NONEURO", 7) == 0)
    {
      if (have_order && newDateOrder != DATEORDER_MDY)
      {
        ok = false;                         
      }
      newDateOrder = DATEORDER_MDY;
      have_order = true;
    }
    else if (pg_strcasecmp(tok, "DEFAULT") == 0)
    {
         
                                                                      
                                                             
         
                                                                       
                                                   
         
      char *subval;
      void *subextra = NULL;

      subval = strdup(GetConfigOptionResetString("datestyle"));
      if (!subval)
      {
        ok = false;
        break;
      }
      if (!check_datestyle(&subval, &subextra, source))
      {
        free(subval);
        ok = false;
        break;
      }
      myextra = (int *)subextra;
      if (!have_style)
      {
        newDateStyle = myextra[0];
      }
      if (!have_order)
      {
        newDateOrder = myextra[1];
      }
      free(subval);
      free(subextra);
    }
    else
    {
      GUC_check_errdetail("Unrecognized key word: \"%s\".", tok);
      pfree(rawstring);
      list_free(elemlist);
      return false;
    }
  }

  pfree(rawstring);
  list_free(elemlist);

  if (!ok)
  {
    GUC_check_errdetail("Conflicting \"datestyle\" specifications.");
    return false;
  }

     
                                                                     
     
  result = (char *)malloc(32);
  if (!result)
  {
    return false;
  }

  switch (newDateStyle)
  {
  case USE_ISO_DATES:
    strcpy(result, "ISO");
    break;
  case USE_SQL_DATES:
    strcpy(result, "SQL");
    break;
  case USE_GERMAN_DATES:
    strcpy(result, "German");
    break;
  default:
    strcpy(result, "Postgres");
    break;
  }
  switch (newDateOrder)
  {
  case DATEORDER_YMD:
    strcat(result, ", YMD");
    break;
  case DATEORDER_DMY:
    strcat(result, ", DMY");
    break;
  default:
    strcat(result, ", MDY");
    break;
  }

  free(*newval);
  *newval = result;

     
                                                                  
     
  myextra = (int *)malloc(2 * sizeof(int));
  if (!myextra)
  {
    return false;
  }
  myextra[0] = newDateStyle;
  myextra[1] = newDateOrder;
  *extra = (void *)myextra;

  return true;
}

   
                                                   
   
void
assign_datestyle(const char *newval, void *extra)
{
  int *myextra = (int *)extra;

  DateStyle = myextra[0];
  DateOrder = myextra[1];
}

   
            
   

   
                                               
   
bool
check_timezone(char **newval, void **extra, GucSource source)
{
  pg_tz *new_tz;
  long gmtoffset;
  char *endptr;
  double hours;

  if (pg_strncasecmp(*newval, "interval", 8) == 0)
  {
       
                                                                     
                                                        
       
    const char *valueptr = *newval;
    char *val;
    Interval *interval;

    valueptr += 8;
    while (isspace((unsigned char)*valueptr))
    {
      valueptr++;
    }
    if (*valueptr++ != '\'')
    {
      return false;
    }
    val = pstrdup(valueptr);
                                         
    endptr = strchr(val, '\'');
    if (!endptr || endptr[1] != '\0')
    {
      pfree(val);
      return false;
    }
    *endptr = '\0';

       
                                                                       
                                                                       
                                                                       
                                                                     
       
    interval = DatumGetIntervalP(DirectFunctionCall3(interval_in, CStringGetDatum(val), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1)));

    pfree(val);
    if (interval->month != 0)
    {
      GUC_check_errdetail("Cannot specify months in time zone interval.");
      pfree(interval);
      return false;
    }
    if (interval->day != 0)
    {
      GUC_check_errdetail("Cannot specify days in time zone interval.");
      pfree(interval);
      return false;
    }

                                                         
    gmtoffset = -(interval->time / USECS_PER_SEC);
    new_tz = pg_tzset_offset(gmtoffset);

    pfree(interval);
  }
  else
  {
       
                                                                  
       
    hours = strtod(*newval, &endptr);
    if (endptr != *newval && *endptr == '\0')
    {
                                                           
      gmtoffset = -hours * SECS_PER_HOUR;
      new_tz = pg_tzset_offset(gmtoffset);
    }
    else
    {
         
                                                                     
         
      new_tz = pg_tzset(*newval);

      if (!new_tz)
      {
                                                                  
        return false;
      }

      if (!pg_tz_acceptable(new_tz))
      {
        GUC_check_errmsg("time zone \"%s\" appears to use leap seconds", *newval);
        GUC_check_errdetail("PostgreSQL does not support leap seconds.");
        return false;
      }
    }
  }

                                                                            
  if (!new_tz)
  {
    GUC_check_errdetail("UTC timezone offset is out of range.");
    return false;
  }

     
                                               
     
  *extra = malloc(sizeof(pg_tz *));
  if (!*extra)
  {
    return false;
  }
  *((pg_tz **)*extra) = new_tz;

  return true;
}

   
                                                 
   
void
assign_timezone(const char *newval, void *extra)
{
  session_timezone = *((pg_tz **)extra);
}

   
                                             
   
const char *
show_timezone(void)
{
  const char *tzn;

                                             
  tzn = pg_get_timezone_name(session_timezone);

  if (tzn != NULL)
  {
    return tzn;
  }

  return "unknown";
}

   
                
   
                                                                              
                                                                          
                    
   

   
                                                       
   
bool
check_log_timezone(char **newval, void **extra, GucSource source)
{
  pg_tz *new_tz;

     
                                                       
     
  new_tz = pg_tzset(*newval);

  if (!new_tz)
  {
                                                              
    return false;
  }

  if (!pg_tz_acceptable(new_tz))
  {
    GUC_check_errmsg("time zone \"%s\" appears to use leap seconds", *newval);
    GUC_check_errdetail("PostgreSQL does not support leap seconds.");
    return false;
  }

     
                                                   
     
  *extra = malloc(sizeof(pg_tz *));
  if (!*extra)
  {
    return false;
  }
  *((pg_tz **)*extra) = new_tz;

  return true;
}

   
                                                         
   
void
assign_log_timezone(const char *newval, void *extra)
{
  log_timezone = *((pg_tz **)extra);
}

   
                                                     
   
const char *
show_log_timezone(void)
{
  const char *tzn;

                                             
  tzn = pg_get_timezone_name(log_timezone);

  if (tzn != NULL)
  {
    return tzn;
  }

  return "unknown";
}

   
                                                            
   
                                                                            
                                                                        
                                                                               
                                                                              
           
   
                                                                          
                                                                            
                                                                     
                                                                           
                                                                         
                
   
bool
check_transaction_read_only(bool *newval, void **extra, GucSource source)
{
  if (*newval == false && XactReadOnly && IsTransactionState() && !InitializingParallelWorker)
  {
                                                       
    if (IsSubTransaction())
    {
      GUC_check_errcode(ERRCODE_ACTIVE_SQL_TRANSACTION);
      GUC_check_errmsg("cannot set transaction read-write mode inside a read-only transaction");
      return false;
    }
                                                                         
    if (FirstSnapshotSet)
    {
      GUC_check_errcode(ERRCODE_ACTIVE_SQL_TRANSACTION);
      GUC_check_errmsg("transaction read-write mode must be set before any query");
      return false;
    }
                                                             
    if (RecoveryInProgress())
    {
      GUC_check_errcode(ERRCODE_FEATURE_NOT_SUPPORTED);
      GUC_check_errmsg("cannot set transaction read-write mode during recovery");
      return false;
    }
  }

  return true;
}

   
                                   
   
                                                                           
                                                                        
   
                                                                            
   
bool
check_XactIsoLevel(int *newval, void **extra, GucSource source)
{
  int newXactIsoLevel = *newval;

  if (newXactIsoLevel != XactIsoLevel && IsTransactionState())
  {
    if (FirstSnapshotSet)
    {
      GUC_check_errcode(ERRCODE_ACTIVE_SQL_TRANSACTION);
      GUC_check_errmsg("SET TRANSACTION ISOLATION LEVEL must be called before any query");
      return false;
    }
                                                                      
    if (IsSubTransaction())
    {
      GUC_check_errcode(ERRCODE_ACTIVE_SQL_TRANSACTION);
      GUC_check_errmsg("SET TRANSACTION ISOLATION LEVEL must not be called in a subtransaction");
      return false;
    }
                                                                      
    if (newXactIsoLevel == XACT_SERIALIZABLE && RecoveryInProgress())
    {
      GUC_check_errcode(ERRCODE_FEATURE_NOT_SUPPORTED);
      GUC_check_errmsg("cannot use serializable mode in a hot standby");
      GUC_check_errhint("You can use REPEATABLE READ instead.");
      return false;
    }
  }

  return true;
}

   
                                    
   

bool
check_transaction_deferrable(bool *newval, void **extra, GucSource source)
{
  if (IsSubTransaction())
  {
    GUC_check_errcode(ERRCODE_ACTIVE_SQL_TRANSACTION);
    GUC_check_errmsg("SET TRANSACTION [NOT] DEFERRABLE cannot be called within a subtransaction");
    return false;
  }
  if (FirstSnapshotSet)
  {
    GUC_check_errcode(ERRCODE_ACTIVE_SQL_TRANSACTION);
    GUC_check_errmsg("SET TRANSACTION [NOT] DEFERRABLE must be called before any query");
    return false;
  }

  return true;
}

   
                      
   
                                                                      
                                                                          
                                                                            
                                        
   

bool
check_random_seed(double *newval, void **extra, GucSource source)
{
  *extra = malloc(sizeof(int));
  if (!*extra)
  {
    return false;
  }
                                                                    
  *((int *)*extra) = (source >= PGC_S_INTERACTIVE);

  return true;
}

void
assign_random_seed(double newval, void *extra)
{
                                                                      
  if (*((int *)extra))
  {
    DirectFunctionCall1(setseed, Float8GetDatum(newval));
  }
  *((int *)extra) = 0;
}

const char *
show_random_seed(void)
{
  return "unavailable";
}

   
                       
   

bool
check_client_encoding(char **newval, void **extra, GucSource source)
{
  int encoding;
  const char *canonical_name;

                                    
  encoding = pg_valid_client_encoding(*newval);
  if (encoding < 0)
  {
    return false;
  }

                                                         
  canonical_name = pg_encoding_to_char(encoding);

     
                                                                            
                                                                         
                                                                           
                                                                     
                                                                             
                                                                           
                                                                             
                                                  
     
                                                                    
                                                                          
                                                                             
     
  if (PrepareClientEncoding(encoding) < 0)
  {
    if (IsTransactionState())
    {
                                                        
      GUC_check_errcode(ERRCODE_FEATURE_NOT_SUPPORTED);
      GUC_check_errdetail("Conversion between %s and %s is not supported.", canonical_name, GetDatabaseEncodingName());
    }
    else
    {
                                      
      GUC_check_errdetail("Cannot change \"client_encoding\" now.");
    }
    return false;
  }

     
                                                                          
                                                           
     
                                                                            
                                                                           
                                                                            
                                                                            
                                                          
     
  if (strcmp(*newval, canonical_name) != 0 && strcmp(*newval, "UNICODE") != 0)
  {
    free(*newval);
    *newval = strdup(canonical_name);
    if (!*newval)
    {
      return false;
    }
  }

     
                                                                          
     
  *extra = malloc(sizeof(int));
  if (!*extra)
  {
    return false;
  }
  *((int *)*extra) = encoding;

  return true;
}

void
assign_client_encoding(const char *newval, void *extra)
{
  int encoding = *((int *)extra);

     
                                                                            
                                            
     
  if (IsParallelWorker())
  {
       
                                                                      
                                                                        
                                                                         
       
    if (InitializingParallelWorker)
    {
      return;
    }

       
                                                                           
                                                                          
                                                                   
       
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot change client_encoding during a parallel operation")));
  }

                                                                    
  if (SetClientEncoding(encoding) < 0)
  {
    elog(LOG, "SetClientEncoding(%d) failed", encoding);
  }
}

   
                             
   

typedef struct
{
                                                                         
  Oid roleid;
  bool is_superuser;
} role_auth_extra;

bool
check_session_authorization(char **newval, void **extra, GucSource source)
{
  HeapTuple roleTup;
  Form_pg_authid roleform;
  Oid roleid;
  bool is_superuser;
  role_auth_extra *myextra;

                                                   
  if (*newval == NULL)
  {
    return true;
  }

  if (!IsTransactionState())
  {
       
                                                                      
                                                                           
                                                                    
       
    return false;
  }

                            
  roleTup = SearchSysCache1(AUTHNAME, PointerGetDatum(*newval));
  if (!HeapTupleIsValid(roleTup))
  {
       
                                                                    
                                                                     
       
    if (source == PGC_S_TEST)
    {
      ereport(NOTICE, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("role \"%s\" does not exist", *newval)));
      return true;
    }
    GUC_check_errmsg("role \"%s\" does not exist", *newval);
    return false;
  }

  roleform = (Form_pg_authid)GETSTRUCT(roleTup);
  roleid = roleform->oid;
  is_superuser = roleform->rolsuper;

  ReleaseSysCache(roleTup);

                                                                     
  myextra = (role_auth_extra *)malloc(sizeof(role_auth_extra));
  if (!myextra)
  {
    return false;
  }
  myextra->roleid = roleid;
  myextra->is_superuser = is_superuser;
  *extra = (void *)myextra;

  return true;
}

void
assign_session_authorization(const char *newval, void *extra)
{
  role_auth_extra *myextra = (role_auth_extra *)extra;

                                                   
  if (!myextra)
  {
    return;
  }

  SetSessionAuthorization(myextra->roleid, myextra->is_superuser);
}

   
            
   
                                                                           
                                                                       
                              
   
extern char *role_string;               

bool
check_role(char **newval, void **extra, GucSource source)
{
  HeapTuple roleTup;
  Oid roleid;
  bool is_superuser;
  role_auth_extra *myextra;
  Form_pg_authid roleform;

  if (strcmp(*newval, "none") == 0)
  {
                               
    roleid = InvalidOid;
    is_superuser = false;
  }
  else
  {
    if (!IsTransactionState())
    {
         
                                                                        
                                                                        
                                                          
         
      return false;
    }

       
                                                                    
                                                                        
                              
       

                              
    roleTup = SearchSysCache1(AUTHNAME, PointerGetDatum(*newval));
    if (!HeapTupleIsValid(roleTup))
    {
      if (source == PGC_S_TEST)
      {
        ereport(NOTICE, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("role \"%s\" does not exist", *newval)));
        return true;
      }
      GUC_check_errmsg("role \"%s\" does not exist", *newval);
      return false;
    }

    roleform = (Form_pg_authid)GETSTRUCT(roleTup);
    roleid = roleform->oid;
    is_superuser = roleform->rolsuper;

    ReleaseSysCache(roleTup);

       
                                                                         
                                                                          
                       
       
    if (!InitializingParallelWorker && !is_member_of_role(GetSessionUserId(), roleid))
    {
      if (source == PGC_S_TEST)
      {
        ereport(NOTICE, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission will be denied to set role \"%s\"", *newval)));
        return true;
      }
      GUC_check_errcode(ERRCODE_INSUFFICIENT_PRIVILEGE);
      GUC_check_errmsg("permission denied to set role \"%s\"", *newval);
      return false;
    }
  }

                                                    
  myextra = (role_auth_extra *)malloc(sizeof(role_auth_extra));
  if (!myextra)
  {
    return false;
  }
  myextra->roleid = roleid;
  myextra->is_superuser = is_superuser;
  *extra = (void *)myextra;

  return true;
}

void
assign_role(const char *newval, void *extra)
{
  role_auth_extra *myextra = (role_auth_extra *)extra;

  SetCurrentRoleId(myextra->roleid, myextra->is_superuser);
}

const char *
show_role(void)
{
     
                                                                        
                                                                          
                                                                           
                                                                         
                              
     
  if (!OidIsValid(GetCurrentRoleId()))
  {
    return "none";
  }

                                                
  return role_string ? role_string : "none";
}
