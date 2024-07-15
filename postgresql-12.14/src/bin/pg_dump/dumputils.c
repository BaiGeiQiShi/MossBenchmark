                                                                            
   
                                    
   
                                                                          
   
   
                                                                         
                                                                        
   
                               
   
                                                                            
   
#include "postgres_fe.h"

#include <ctype.h>

#include "dumputils.h"
#include "fe_utils/string_utils.h"

static bool
parseAclItem(const char *item, const char *type, const char *name, const char *subname, int remoteVersion, PQExpBuffer grantee, PQExpBuffer grantor, PQExpBuffer privs, PQExpBuffer privswgo);
static char *
copyAclUserName(PQExpBuffer output, char *input);
static void
AddAcl(PQExpBuffer aclbuf, const char *keyword, const char *subname);

   
                                                
   
                                                                              
                                                                       
                                                                          
                                                                   
                                                                                  
                                                   
                                                  
                                                                   
                                                                          
                                                      
                                                                       
                                      
   
                                                                  
                                                                          
   
                                                                              
                                                      
   
                                                                             
                                               
   
bool
buildACLCommands(const char *name, const char *subname, const char *nspname, const char *type, const char *acls, const char *racls, const char *owner, const char *prefix, int remoteVersion, PQExpBuffer sql)
{
  bool ok = true;
  char **aclitems = NULL;
  char **raclitems = NULL;
  int naclitems = 0;
  int nraclitems = 0;
  int i;
  PQExpBuffer grantee, grantor, privs, privswgo;
  PQExpBuffer firstsql, secondsql;
  bool found_owner_privs = false;

  if (strlen(acls) == 0 && strlen(racls) == 0)
  {
    return true;                                     
  }

                                             
  if (owner && *owner == '\0')
  {
    owner = NULL;
  }

  if (strlen(acls) != 0)
  {
    if (!parsePGArray(acls, &aclitems, &naclitems))
    {
      if (aclitems)
      {
        free(aclitems);
      }
      return false;
    }
  }

  if (strlen(racls) != 0)
  {
    if (!parsePGArray(racls, &raclitems, &nraclitems))
    {
      if (aclitems)
      {
        free(aclitems);
      }
      if (raclitems)
      {
        free(raclitems);
      }
      return false;
    }
  }

  grantee = createPQExpBuffer();
  grantor = createPQExpBuffer();
  privs = createPQExpBuffer();
  privswgo = createPQExpBuffer();

     
                                                                       
     
                                                                           
                                                                          
                                                                            
                                                                           
                                                                            
                 
     
                                                                          
                                                                             
                                                                           
                                                                           
                                                                       
                                                                       
                                            
     
  firstsql = createPQExpBuffer();
  secondsql = createPQExpBuffer();

     
                                                                             
                                                                             
                                                                            
                                
     
                                                                            
                         
     
                                                                     
                                                                           
                                                                          
                                                                             
                                                                        
                                                                        
                                                                           
                                                                       
                                                                   
                                                                         
                    
     
  if (remoteVersion < 90600)
  {
    Assert(nraclitems == 0);

    appendPQExpBuffer(firstsql, "%sREVOKE ALL", prefix);
    if (subname)
    {
      appendPQExpBuffer(firstsql, "(%s)", subname);
    }
    appendPQExpBuffer(firstsql, " ON %s ", type);
    if (nspname && *nspname)
    {
      appendPQExpBuffer(firstsql, "%s.", fmtId(nspname));
    }
    appendPQExpBuffer(firstsql, "%s FROM PUBLIC;\n", name);
  }
  else
  {
                                          
    for (i = 0; i < nraclitems; i++)
    {
      if (!parseAclItem(raclitems[i], type, name, subname, remoteVersion, grantee, grantor, privs, NULL))
      {
        ok = false;
        break;
      }

      if (privs->len > 0)
      {
        appendPQExpBuffer(firstsql, "%sREVOKE %s ON %s ", prefix, privs->data, type);
        if (nspname && *nspname)
        {
          appendPQExpBuffer(firstsql, "%s.", fmtId(nspname));
        }
        appendPQExpBuffer(firstsql, "%s FROM ", name);
        if (grantee->len == 0)
        {
          appendPQExpBufferStr(firstsql, "PUBLIC;\n");
        }
        else if (strncmp(grantee->data, "group ", strlen("group ")) == 0)
        {
          appendPQExpBuffer(firstsql, "GROUP %s;\n", fmtId(grantee->data + strlen("group ")));
        }
        else
        {
          appendPQExpBuffer(firstsql, "%s;\n", fmtId(grantee->data));
        }
      }
    }
  }

     
                                                                           
                                                                             
                                                                        
                                                                             
                                                                  
     
  if (remoteVersion < 80200 && strcmp(type, "DATABASE") == 0)
  {
                                                       
    appendPQExpBuffer(firstsql, "%sGRANT CONNECT ON %s %s TO PUBLIC;\n", prefix, type, name);
  }

                                 
  for (i = 0; i < naclitems; i++)
  {
    if (!parseAclItem(aclitems[i], type, name, subname, remoteVersion, grantee, grantor, privs, privswgo))
    {
      ok = false;
      break;
    }

    if (grantor->len == 0 && owner)
    {
      printfPQExpBuffer(grantor, "%s", owner);
    }

    if (privs->len > 0 || privswgo->len > 0)
    {
         
                                                                      
                                                                       
                                                                        
                                                                    
                                                       
         
      if (remoteVersion < 90600 && owner && strcmp(grantee->data, owner) == 0 && strcmp(grantor->data, owner) == 0)
      {
        found_owner_privs = true;

           
                                                                  
                         
           
        if (strcmp(privswgo->data, "ALL") != 0)
        {
          appendPQExpBuffer(firstsql, "%sREVOKE ALL", prefix);
          if (subname)
          {
            appendPQExpBuffer(firstsql, "(%s)", subname);
          }
          appendPQExpBuffer(firstsql, " ON %s ", type);
          if (nspname && *nspname)
          {
            appendPQExpBuffer(firstsql, "%s.", fmtId(nspname));
          }
          appendPQExpBuffer(firstsql, "%s FROM %s;\n", name, fmtId(grantee->data));
          if (privs->len > 0)
          {
            appendPQExpBuffer(firstsql, "%sGRANT %s ON %s ", prefix, privs->data, type);
            if (nspname && *nspname)
            {
              appendPQExpBuffer(firstsql, "%s.", fmtId(nspname));
            }
            appendPQExpBuffer(firstsql, "%s TO %s;\n", name, fmtId(grantee->data));
          }
          if (privswgo->len > 0)
          {
            appendPQExpBuffer(firstsql, "%sGRANT %s ON %s ", prefix, privswgo->data, type);
            if (nspname && *nspname)
            {
              appendPQExpBuffer(firstsql, "%s.", fmtId(nspname));
            }
            appendPQExpBuffer(firstsql, "%s TO %s WITH GRANT OPTION;\n", name, fmtId(grantee->data));
          }
        }
      }
      else
      {
           
                                                                   
                                        
           
                                                                  
                                                                       
                                                                      
                                                                    
                   
           
        if (grantor->len > 0 && (!owner || strcmp(owner, grantor->data) != 0))
        {
          appendPQExpBuffer(secondsql, "SET SESSION AUTHORIZATION %s;\n", fmtId(grantor->data));
        }

        if (privs->len > 0)
        {
          appendPQExpBuffer(secondsql, "%sGRANT %s ON %s ", prefix, privs->data, type);
          if (nspname && *nspname)
          {
            appendPQExpBuffer(secondsql, "%s.", fmtId(nspname));
          }
          appendPQExpBuffer(secondsql, "%s TO ", name);
          if (grantee->len == 0)
          {
            appendPQExpBufferStr(secondsql, "PUBLIC;\n");
          }
          else if (strncmp(grantee->data, "group ", strlen("group ")) == 0)
          {
            appendPQExpBuffer(secondsql, "GROUP %s;\n", fmtId(grantee->data + strlen("group ")));
          }
          else
          {
            appendPQExpBuffer(secondsql, "%s;\n", fmtId(grantee->data));
          }
        }
        if (privswgo->len > 0)
        {
          appendPQExpBuffer(secondsql, "%sGRANT %s ON %s ", prefix, privswgo->data, type);
          if (nspname && *nspname)
          {
            appendPQExpBuffer(secondsql, "%s.", fmtId(nspname));
          }
          appendPQExpBuffer(secondsql, "%s TO ", name);
          if (grantee->len == 0)
          {
            appendPQExpBufferStr(secondsql, "PUBLIC");
          }
          else if (strncmp(grantee->data, "group ", strlen("group ")) == 0)
          {
            appendPQExpBuffer(secondsql, "GROUP %s", fmtId(grantee->data + strlen("group ")));
          }
          else
          {
            appendPQExpBufferStr(secondsql, fmtId(grantee->data));
          }
          appendPQExpBufferStr(secondsql, " WITH GRANT OPTION;\n");
        }

        if (grantor->len > 0 && (!owner || strcmp(owner, grantor->data) != 0))
        {
          appendPQExpBufferStr(secondsql, "RESET SESSION AUTHORIZATION;\n");
        }
      }
    }
  }

     
                                                                            
                                
     
                                                                        
     
  if (remoteVersion < 90600 && !found_owner_privs && owner)
  {
    appendPQExpBuffer(firstsql, "%sREVOKE ALL", prefix);
    if (subname)
    {
      appendPQExpBuffer(firstsql, "(%s)", subname);
    }
    appendPQExpBuffer(firstsql, " ON %s ", type);
    if (nspname && *nspname)
    {
      appendPQExpBuffer(firstsql, "%s.", fmtId(nspname));
    }
    appendPQExpBuffer(firstsql, "%s FROM %s;\n", name, fmtId(owner));
  }

  destroyPQExpBuffer(grantee);
  destroyPQExpBuffer(grantor);
  destroyPQExpBuffer(privs);
  destroyPQExpBuffer(privswgo);

  appendPQExpBuffer(sql, "%s%s", firstsql->data, secondsql->data);
  destroyPQExpBuffer(firstsql);
  destroyPQExpBuffer(secondsql);

  if (aclitems)
  {
    free(aclitems);
  }

  if (raclitems)
  {
    free(raclitems);
  }

  return ok;
}

   
                                                                              
   
                                                  
                                                               
                                                  
                                                                      
                                      
   
                                                                  
                                                                          
   
bool
buildDefaultACLCommands(const char *type, const char *nspname, const char *acls, const char *racls, const char *initacls, const char *initracls, const char *owner, int remoteVersion, PQExpBuffer sql)
{
  PQExpBuffer prefix;

  prefix = createPQExpBuffer();

     
                                                                           
                                                                            
                                                                        
                                            
     
  appendPQExpBuffer(prefix, "ALTER DEFAULT PRIVILEGES FOR ROLE %s ", fmtId(owner));
  if (nspname)
  {
    appendPQExpBuffer(prefix, "IN SCHEMA %s ", fmtId(nspname));
  }

  if (strlen(initacls) != 0 || strlen(initracls) != 0)
  {
    appendPQExpBuffer(sql, "SELECT pg_catalog.binary_upgrade_set_record_init_privs(true);\n");
    if (!buildACLCommands("", NULL, NULL, type, initacls, initracls, owner, prefix->data, remoteVersion, sql))
    {
      destroyPQExpBuffer(prefix);
      return false;
    }
    appendPQExpBuffer(sql, "SELECT pg_catalog.binary_upgrade_set_record_init_privs(false);\n");
  }

  if (!buildACLCommands("", NULL, NULL, type, acls, racls, owner, prefix->data, remoteVersion, sql))
  {
    destroyPQExpBuffer(prefix);
    return false;
  }

  destroyPQExpBuffer(prefix);

  return true;
}

   
                                                              
                                    
      
                                           
                                                           
   
                                                                              
                                                             
   
                                                                          
                                                                             
                                                                           
                                                                               
                                                                               
                                                                            
                                                                               
                             
   
                                                                       
                                                      
   
static bool
parseAclItem(const char *item, const char *type, const char *name, const char *subname, int remoteVersion, PQExpBuffer grantee, PQExpBuffer grantor, PQExpBuffer privs, PQExpBuffer privswgo)
{
  char *buf;
  bool all_with_go = true;
  bool all_without_go = true;
  char *eqpos;
  char *slpos;
  char *pos;

  buf = pg_strdup(item);

                                            
  eqpos = copyAclUserName(grantee, buf);
  if (*eqpos != '=')
  {
    pg_free(buf);
    return false;
  }

                                     
  slpos = strchr(eqpos + 1, '/');
  if (slpos)
  {
    *slpos++ = '\0';
    slpos = copyAclUserName(grantor, slpos);
    if (*slpos != '\0')
    {
      pg_free(buf);
      return false;
    }
  }
  else
  {
    pg_free(buf);
    return false;
  }

                       
#define CONVERT_PRIV(code, keywd)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if ((pos = strchr(eqpos + 1, code)))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      if (*(pos + 1) == '*' && privswgo != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
        AddAcl(privswgo, keywd, subname);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
        all_without_go = false;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
      {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
        AddAcl(privs, keywd, subname);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
        all_with_go = false;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
      }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      all_with_go = all_without_go = false;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  } while (0)

  resetPQExpBuffer(privs);
  resetPQExpBuffer(privswgo);

  if (strcmp(type, "TABLE") == 0 || strcmp(type, "SEQUENCE") == 0 || strcmp(type, "TABLES") == 0 || strcmp(type, "SEQUENCES") == 0)
  {
    CONVERT_PRIV('r', "SELECT");

    if (strcmp(type, "SEQUENCE") == 0 || strcmp(type, "SEQUENCES") == 0)
    {
                         
      CONVERT_PRIV('U', "USAGE");
    }
    else
    {
                      
      CONVERT_PRIV('a', "INSERT");
      CONVERT_PRIV('x', "REFERENCES");
                                              
      if (subname == NULL)
      {
        CONVERT_PRIV('d', "DELETE");
        CONVERT_PRIV('t', "TRIGGER");
        if (remoteVersion >= 80400)
        {
          CONVERT_PRIV('D', "TRUNCATE");
        }
      }
    }

                
    CONVERT_PRIV('w', "UPDATE");
  }
  else if (strcmp(type, "FUNCTION") == 0 || strcmp(type, "FUNCTIONS") == 0)
  {
    CONVERT_PRIV('X', "EXECUTE");
  }
  else if (strcmp(type, "PROCEDURE") == 0 || strcmp(type, "PROCEDURES") == 0)
  {
    CONVERT_PRIV('X', "EXECUTE");
  }
  else if (strcmp(type, "LANGUAGE") == 0)
  {
    CONVERT_PRIV('U', "USAGE");
  }
  else if (strcmp(type, "SCHEMA") == 0 || strcmp(type, "SCHEMAS") == 0)
  {
    CONVERT_PRIV('C', "CREATE");
    CONVERT_PRIV('U', "USAGE");
  }
  else if (strcmp(type, "DATABASE") == 0)
  {
    CONVERT_PRIV('C', "CREATE");
    CONVERT_PRIV('c', "CONNECT");
    CONVERT_PRIV('T', "TEMPORARY");
  }
  else if (strcmp(type, "TABLESPACE") == 0)
  {
    CONVERT_PRIV('C', "CREATE");
  }
  else if (strcmp(type, "TYPE") == 0 || strcmp(type, "TYPES") == 0)
  {
    CONVERT_PRIV('U', "USAGE");
  }
  else if (strcmp(type, "FOREIGN DATA WRAPPER") == 0)
  {
    CONVERT_PRIV('U', "USAGE");
  }
  else if (strcmp(type, "FOREIGN SERVER") == 0)
  {
    CONVERT_PRIV('U', "USAGE");
  }
  else if (strcmp(type, "FOREIGN TABLE") == 0)
  {
    CONVERT_PRIV('r', "SELECT");
  }
  else if (strcmp(type, "LARGE OBJECT") == 0)
  {
    CONVERT_PRIV('r', "SELECT");
    CONVERT_PRIV('w', "UPDATE");
  }
  else
  {
    abort();
  }

#undef CONVERT_PRIV

  if (all_with_go)
  {
    resetPQExpBuffer(privs);
    printfPQExpBuffer(privswgo, "ALL");
    if (subname)
    {
      appendPQExpBuffer(privswgo, "(%s)", subname);
    }
  }
  else if (all_without_go)
  {
    resetPQExpBuffer(privswgo);
    printfPQExpBuffer(privs, "ALL");
    if (subname)
    {
      appendPQExpBuffer(privs, "(%s)", subname);
    }
  }

  pg_free(buf);

  return true;
}

   
                                                                            
                                                                        
                                                                 
   
static char *
copyAclUserName(PQExpBuffer output, char *input)
{
  resetPQExpBuffer(output);

  while (*input && *input != '=')
  {
       
                                                                        
       
    if (*input != '"')
    {
      appendPQExpBufferChar(output, *input++);
    }
    else
    {
                                             
      input++;
                                                        
      while (!(*input == '"' && *(input + 1) != '"'))
      {
        if (*input == '\0')
        {
          return input;                               
        }

           
                                                                       
                                                 
           
        if (*input == '"' && *(input + 1) == '"')
        {
          input++;
        }
        appendPQExpBufferChar(output, *input++);
      }
      input++;
    }
  }
  return input;
}

   
                                                                            
   
static void
AddAcl(PQExpBuffer aclbuf, const char *keyword, const char *subname)
{
  if (aclbuf->len > 0)
  {
    appendPQExpBufferChar(aclbuf, ',');
  }
  appendPQExpBufferStr(aclbuf, keyword);
  if (subname)
  {
    appendPQExpBuffer(aclbuf, "(%s)", subname);
  }
}

   
                        
   
                                                                  
                                                                    
                                                                
                                                                       
                                                                          
   
void
buildShSecLabelQuery(PGconn *conn, const char *catalog_name, Oid objectId, PQExpBuffer sql)
{
  appendPQExpBuffer(sql,
      "SELECT provider, label FROM pg_catalog.pg_shseclabel "
      "WHERE classoid = 'pg_catalog.%s'::pg_catalog.regclass "
      "AND objoid = '%u'",
      catalog_name, objectId);
}

   
                   
   
                                                                           
                                                                   
                                                                            
                                  
   
void
emitShSecLabels(PGconn *conn, PGresult *res, PQExpBuffer buffer, const char *objtype, const char *objname)
{
  int i;

  for (i = 0; i < PQntuples(res); i++)
  {
    char *provider = PQgetvalue(res, i, 0);
    char *label = PQgetvalue(res, i, 1);

                                                       
    appendPQExpBuffer(buffer, "SECURITY LABEL FOR %s ON %s", fmtId(provider), objtype);
    appendPQExpBuffer(buffer, " %s IS ", fmtId(objname));
    appendStringLiteralConn(buffer, label, conn);
    appendPQExpBufferStr(buffer, ";\n");
  }
}

   
                   
   
                                                                     
                                                                            
                                                                            
                        
   
                                                                               
                                                                              
                                                                                
                                                                    
   
void
buildACLQueries(PQExpBuffer acl_subquery, PQExpBuffer racl_subquery, PQExpBuffer init_acl_subquery, PQExpBuffer init_racl_subquery, const char *acl_column, const char *acl_owner, const char *obj_kind, bool binary_upgrade)
{
     
                                                                      
                                                                           
                         
     
                                                                           
                                                                         
                                                                         
                                                                     
                                                                   
                  
     
                                                                             
                                                                             
                                                                      
                                                  
     
                                                                             
                                                                          
                                                                          
                                              
     
                                                                          
                                                                             
                                    
     
                                                                             
                                                                             
                                                                         
                                            
     
  printfPQExpBuffer(acl_subquery,
      "(SELECT pg_catalog.array_agg(acl ORDER BY row_n) FROM "
      "(SELECT acl, row_n FROM "
      "pg_catalog.unnest(coalesce(%s,pg_catalog.acldefault(%s,%s))) "
      "WITH ORDINALITY AS perm(acl,row_n) "
      "WHERE NOT EXISTS ( "
      "SELECT 1 FROM "
      "pg_catalog.unnest(coalesce(pip.initprivs,pg_catalog.acldefault(%s,%s))) "
      "AS init(init_acl) WHERE acl = init_acl)) as foo)",
      acl_column, obj_kind, acl_owner, obj_kind, acl_owner);

  printfPQExpBuffer(racl_subquery,
      "(SELECT pg_catalog.array_agg(acl ORDER BY row_n) FROM "
      "(SELECT acl, row_n FROM "
      "pg_catalog.unnest(coalesce(pip.initprivs,pg_catalog.acldefault(%s,%s))) "
      "WITH ORDINALITY AS initp(acl,row_n) "
      "WHERE NOT EXISTS ( "
      "SELECT 1 FROM "
      "pg_catalog.unnest(coalesce(%s,pg_catalog.acldefault(%s,%s))) "
      "AS permp(orig_acl) WHERE acl = orig_acl)) as foo)",
      obj_kind, acl_owner, acl_column, obj_kind, acl_owner);

     
                                                                          
                                                                             
                                                                            
                                                                         
                                                                           
                       
     
                                                                          
                                                                             
                                                                           
                                                                            
                                                      
     
  if (binary_upgrade)
  {
    printfPQExpBuffer(init_acl_subquery,
        "CASE WHEN privtype = 'e' THEN "
        "(SELECT pg_catalog.array_agg(acl ORDER BY row_n) FROM "
        "(SELECT acl, row_n FROM pg_catalog.unnest(pip.initprivs) "
        "WITH ORDINALITY AS initp(acl,row_n) "
        "WHERE NOT EXISTS ( "
        "SELECT 1 FROM "
        "pg_catalog.unnest(pg_catalog.acldefault(%s,%s)) "
        "AS privm(orig_acl) WHERE acl = orig_acl)) as foo) END",
        obj_kind, acl_owner);

    printfPQExpBuffer(init_racl_subquery,
        "CASE WHEN privtype = 'e' THEN "
        "(SELECT pg_catalog.array_agg(acl) FROM "
        "(SELECT acl, row_n FROM "
        "pg_catalog.unnest(pg_catalog.acldefault(%s,%s)) "
        "WITH ORDINALITY AS privp(acl,row_n) "
        "WHERE NOT EXISTS ( "
        "SELECT 1 FROM pg_catalog.unnest(pip.initprivs) "
        "AS initp(init_acl) WHERE acl = init_acl)) as foo) END",
        obj_kind, acl_owner);
  }
  else
  {
    printfPQExpBuffer(init_acl_subquery, "NULL");
    printfPQExpBuffer(init_racl_subquery, "NULL");
  }
}

   
                                                                    
   
                                                                               
                                                                            
                                                                              
                                                                             
                                                                             
                                                                    
   
bool
variable_is_guc_list_quote(const char *name)
{
  if (pg_strcasecmp(name, "temp_tablespaces") == 0 || pg_strcasecmp(name, "session_preload_libraries") == 0 || pg_strcasecmp(name, "shared_preload_libraries") == 0 || pg_strcasecmp(name, "local_preload_libraries") == 0 || pg_strcasecmp(name, "search_path") == 0)
  {
    return true;
  }
  else
  {
    return false;
  }
}

   
                                                                        
   
                                                                             
                                                                              
                                                           
   
           
                                                                      
                                                            
                                                                     
                                                                   
                     
            
                                                                       
                                                              
                             
   
                                                                         
   
bool
SplitGUCList(char *rawstring, char separator, char ***namelist)
{
  char *nextp = rawstring;
  bool done = false;
  char **nextptr;

     
                                                                 
                                                                          
                      
     
  *namelist = nextptr = (char **)pg_malloc((strlen(rawstring) / 2 + 2) * sizeof(char *));
  *nextptr = NULL;

  while (isspace((unsigned char)*nextp))
  {
    nextp++;                              
  }

  if (*nextp == '\0')
  {
    return true;                         
  }

                                                                    
  do
  {
    char *curname;
    char *endp;

    if (*nextp == '"')
    {
                                                      
      curname = nextp + 1;
      for (;;)
      {
        endp = strchr(nextp + 1, '"');
        if (endp == NULL)
        {
          return false;                        
        }
        if (endp[1] != '"')
        {
          break;                               
        }
                                                                     
        memmove(endp, endp + 1, strlen(endp));
        nextp = endp;
      }
                                                    
      nextp = endp + 1;
    }
    else
    {
                                                                
      curname = nextp;
      while (*nextp && *nextp != separator && !isspace((unsigned char)*nextp))
      {
        nextp++;
      }
      endp = nextp;
      if (curname == nextp)
      {
        return false;                                      
      }
    }

    while (isspace((unsigned char)*nextp))
    {
      nextp++;                               
    }

    if (*nextp == separator)
    {
      nextp++;
      while (isspace((unsigned char)*nextp))
      {
        nextp++;                                       
      }
                                                         
    }
    else if (*nextp == '\0')
    {
      done = true;
    }
    else
    {
      return false;                     
    }

                                                     
    *endp = '\0';

       
                                                                  
       
    *nextptr++ = curname;

                                                    
  } while (!done);

  *nextptr = NULL;
  return true;
}

   
                                                                       
   
                                                                        
                                                   
   
                                                                           
                                                                           
                                         
                                                                      
   
void
makeAlterConfigCommand(PGconn *conn, const char *configitem, const char *type, const char *name, const char *type2, const char *name2, PQExpBuffer buf)
{
  char *mine;
  char *pos;

                                                                            
  mine = pg_strdup(configitem);
  pos = strchr(mine, '=');
  if (pos == NULL)
  {
    pg_free(mine);
    return;
  }
  *pos++ = '\0';

                                                                
  appendPQExpBuffer(buf, "ALTER %s %s ", type, fmtId(name));
  if (type2 != NULL && name2 != NULL)
  {
    appendPQExpBuffer(buf, "IN %s %s ", type2, fmtId(name2));
  }
  appendPQExpBuffer(buf, "SET %s TO ", fmtId(mine));

     
                                                                           
                                                                         
                                                                          
                                                                          
                                                                             
                                                                          
                                                                           
     
                                                                       
                                                       
                                                                          
                                                 
     
  if (variable_is_guc_list_quote(mine))
  {
    char **namelist;
    char **nameptr;

                                               
                                    
    if (SplitGUCList(pos, ',', &namelist))
    {
      for (nameptr = namelist; *nameptr; nameptr++)
      {
        if (nameptr != namelist)
        {
          appendPQExpBufferStr(buf, ", ");
        }
        appendStringLiteralConn(buf, *nameptr, conn);
      }
    }
    pg_free(namelist);
  }
  else
  {
    appendStringLiteralConn(buf, pos, conn);
  }

  appendPQExpBufferStr(buf, ";\n");

  pg_free(mine);
}
