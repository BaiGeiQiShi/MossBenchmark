                                                                            
   
                                                        
   
                                                                          
                                    
   
   
                                                                         
                                                                        
   
                               
   
                                                                            
   
#include "postgres_fe.h"

#include <ctype.h>

#include "fe_utils/string_utils.h"

#include "common/keywords.h"

static PQExpBuffer
defaultGetLocalPQExpBuffer(void);

                                   
int quote_all_identifiers = 0;
PQExpBuffer (*getLocalPQExpBuffer)(void) = defaultGetLocalPQExpBuffer;

   
                                                                               
                                             
   
                                                                         
                                                                         
                     
   
static PQExpBuffer
defaultGetLocalPQExpBuffer(void)
{
  static PQExpBuffer id_return = NULL;

  if (id_return)                          
  {
                                         
    resetPQExpBuffer(id_return);
  }
  else
  {
                    
    id_return = createPQExpBuffer();
  }

  return id_return;
}

   
                                                                      
   
                                                                          
                                                     
   
const char *
fmtId(const char *rawid)
{
  PQExpBuffer id_return = getLocalPQExpBuffer();

  const char *cp;
  bool need_quotes = false;

     
                                                                           
                        
     
  if (quote_all_identifiers)
  {
    need_quotes = true;
  }
                                                    
  else if (!((rawid[0] >= 'a' && rawid[0] <= 'z') || rawid[0] == '_'))
  {
    need_quotes = true;
  }
  else
  {
                                           
    for (cp = rawid; *cp; cp++)
    {
      if (!((*cp >= 'a' && *cp <= 'z') || (*cp >= '0' && *cp <= '9') || (*cp == '_')))
      {
        need_quotes = true;
        break;
      }
    }
  }

  if (!need_quotes)
  {
       
                                                                         
                                                                          
                                                                        
       
                                                                       
                                                                  
       
    int kwnum = ScanKeywordLookup(rawid, &ScanKeywords);

    if (kwnum >= 0 && ScanKeywordCategories[kwnum] != UNRESERVED_KEYWORD)
    {
      need_quotes = true;
    }
  }

  if (!need_quotes)
  {
                           
    appendPQExpBufferStr(id_return, rawid);
  }
  else
  {
    appendPQExpBufferChar(id_return, '"');
    for (cp = rawid; *cp; cp++)
    {
         
                                                                    
                                                            
                                                          
         
      if (*cp == '"')
      {
        appendPQExpBufferChar(id_return, '"');
      }
      appendPQExpBufferChar(id_return, *cp);
    }
    appendPQExpBufferChar(id_return, '"');
  }

  return id_return->data;
}

   
                                                                               
   
                                                    
   
                                                                        
                                                              
   
const char *
fmtQualifiedId(const char *schema, const char *id)
{
  PQExpBuffer id_return;
  PQExpBuffer lcl_pqexp = createPQExpBuffer();

                                                        
  if (schema && *schema)
  {
    appendPQExpBuffer(lcl_pqexp, "%s.", fmtId(schema));
  }
  appendPQExpBufferStr(lcl_pqexp, fmtId(id));

  id_return = getLocalPQExpBuffer();

  appendPQExpBufferStr(id_return, lcl_pqexp->data);
  destroyPQExpBuffer(lcl_pqexp);

  return id_return->data;
}

   
                                                                          
                                                                      
                                                                        
   
                                                                       
                                               
   
                                                          
   
char *
formatPGVersionNumber(int version_number, bool include_minor, char *buf, size_t buflen)
{
  if (version_number >= 100000)
  {
                            
    if (include_minor)
    {
      snprintf(buf, buflen, "%d.%d", version_number / 10000, version_number % 10000);
    }
    else
    {
      snprintf(buf, buflen, "%d", version_number / 10000);
    }
  }
  else
  {
                              
    if (include_minor)
    {
      snprintf(buf, buflen, "%d.%d.%d", version_number / 10000, (version_number / 100) % 100, version_number % 100);
    }
    else
    {
      snprintf(buf, buflen, "%d.%d", version_number / 10000, (version_number / 100) % 100);
    }
  }
  return buf;
}

   
                                                                    
                                                                  
                                         
   
                                                                     
                                                                     
                                                          
                                               
   
void
appendStringLiteral(PQExpBuffer buf, const char *str, int encoding, bool std_strings)
{
  size_t length = strlen(str);
  const char *source = str;
  char *target;

  if (!enlargePQExpBuffer(buf, 2 * length + 2))
  {
    return;
  }

  target = buf->data + buf->len;
  *target++ = '\'';

  while (*source != '\0')
  {
    char c = *source;
    int len;
    int i;

                                   
    if (!IS_HIGHBIT_SET(c))
    {
                                   
      if (SQL_STR_DOUBLE(c, !std_strings))
      {
        *target++ = c;
      }
                              
      *target++ = c;
      source++;
      continue;
    }

                                                     
    len = PQmblen(source, encoding);

                            
    for (i = 0; i < len; i++)
    {
      if (*source == '\0')
      {
        break;
      }
      *target++ = *source++;
    }

       
                                                                   
                                                                        
                                                                        
                                                                   
                                                                          
                                     
       
    if (i < len)
    {
      char *stop = buf->data + buf->maxlen - 2;

      for (; i < len; i++)
      {
        if (target >= stop)
        {
          break;
        }
        *target++ = ' ';
      }
      break;
    }
  }

                                                      
  *target++ = '\'';
  *target = '\0';

  buf->len = target - buf->data;
}

   
                                                                    
                                                                        
                                      
   
void
appendStringLiteralConn(PQExpBuffer buf, const char *str, PGconn *conn)
{
  size_t length = strlen(str);

     
                                                                         
                                           
     
  if (strchr(str, '\\') != NULL && PQserverVersion(conn) >= 80100)
  {
                                                     
    if (buf->len > 0 && buf->data[buf->len - 1] != ' ')
    {
      appendPQExpBufferChar(buf, ' ');
    }
    appendPQExpBufferChar(buf, ESCAPE_STRING_SYNTAX);
    appendStringLiteral(buf, str, PQclientEncoding(conn), false);
    return;
  }
                     

  if (!enlargePQExpBuffer(buf, 2 * length + 2))
  {
    return;
  }
  appendPQExpBufferChar(buf, '\'');
  buf->len += PQescapeStringConn(conn, buf->data + buf->len, str, length, NULL);
  appendPQExpBufferChar(buf, '\'');
}

   
                                                                      
                                                                    
                                                                      
   
                                                                   
                                                                     
                    
   
void
appendStringLiteralDQ(PQExpBuffer buf, const char *str, const char *dqprefix)
{
  static const char suffixes[] = "_XXXXXXX";
  int nextchar = 0;
  PQExpBuffer delimBuf = createPQExpBuffer();

                                           
  appendPQExpBufferChar(delimBuf, '$');
  if (dqprefix)
  {
    appendPQExpBufferStr(delimBuf, dqprefix);
  }

     
                                                                           
                                                                            
                                                                    
     
  while (strstr(str, delimBuf->data) != NULL)
  {
    appendPQExpBufferChar(delimBuf, suffixes[nextchar++]);
    nextchar %= sizeof(suffixes) - 1;
  }

                      
  appendPQExpBufferChar(delimBuf, '$');

                                    
  appendPQExpBufferStr(buf, delimBuf->data);
  appendPQExpBufferStr(buf, str);
  appendPQExpBufferStr(buf, delimBuf->data);

  destroyPQExpBuffer(delimBuf);
}

   
                                                                           
                                                               
                                        
   
                                                                         
                                                      
   
void
appendByteaLiteral(PQExpBuffer buf, const unsigned char *str, size_t length, bool std_strings)
{
  const unsigned char *source = str;
  char *target;

  static const char hextbl[] = "0123456789abcdef";

     
                                                                           
                                                                           
                                                                        
                                        
     
  if (!enlargePQExpBuffer(buf, 2 * length + 5))
  {
    return;
  }

  target = buf->data + buf->len;
  *target++ = '\'';
  if (!std_strings)
  {
    *target++ = '\\';
  }
  *target++ = '\\';
  *target++ = 'x';

  while (length-- > 0)
  {
    unsigned char c = *source++;

    *target++ = hextbl[(c >> 4) & 0xF];
    *target++ = hextbl[c & 0xF];
  }

                                                      
  *target++ = '\'';
  *target = '\0';

  buf->len = target - buf->data;
}

   
                                                                           
                                                                      
   
                                                                               
                                                                              
                                                                            
                                                                           
                                          
   
                                                                            
                                                                          
                                    
   
void
appendShellString(PQExpBuffer buf, const char *str)
{
  if (!appendShellStringNoError(buf, str))
  {
    fprintf(stderr,
        _("shell command argument contains a newline or carriage return: "
          "\"%s\"\n"),
        str);
    exit(EXIT_FAILURE);
  }
}

bool
appendShellStringNoError(PQExpBuffer buf, const char *str)
{
#ifdef WIN32
  int backslash_run_length = 0;
#endif
  bool ok = true;
  const char *p;

     
                                                                           
                                    
     
  if (*str != '\0' && strspn(str, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQR"
                                  "STUVWXYZ0123456789-_./:") == strlen(str))
  {
    appendPQExpBufferStr(buf, str);
    return ok;
  }

#ifndef WIN32
  appendPQExpBufferChar(buf, '\'');
  for (p = str; *p; p++)
  {
    if (*p == '\n' || *p == '\r')
    {
      ok = false;
      continue;
    }

    if (*p == '\'')
    {
      appendPQExpBufferStr(buf, "'\"'\"'");
    }
    else
    {
      appendPQExpBufferChar(buf, *p);
    }
  }
  appendPQExpBufferChar(buf, '\'');
#else             

     
                                                                           
                                                                          
                                                                            
                                                                           
                                                            
     
                                                                            
                                                                           
                                                 
     
  appendPQExpBufferStr(buf, "^\"");
  for (p = str; *p; p++)
  {
    if (*p == '\n' || *p == '\r')
    {
      ok = false;
      continue;
    }

                                                                         
    if (*p == '"')
    {
      while (backslash_run_length)
      {
        appendPQExpBufferStr(buf, "^\\");
        backslash_run_length--;
      }
      appendPQExpBufferStr(buf, "^\\");
    }
    else if (*p == '\\')
    {
      backslash_run_length++;
    }
    else
    {
      backslash_run_length = 0;
    }

       
                                                                    
                                                                
       
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')))
    {
      appendPQExpBufferChar(buf, '^');
    }
    appendPQExpBufferChar(buf, *p);
  }

     
                                                                             
                                                            
     
  while (backslash_run_length)
  {
    appendPQExpBufferStr(buf, "^\\");
    backslash_run_length--;
  }
  appendPQExpBufferStr(buf, "^\"");
#endif            

  return ok;
}

   
                                                                            
                                                                               
   
void
appendConnStrVal(PQExpBuffer buf, const char *str)
{
  const char *s;
  bool needquotes;

     
                                                                           
                                                                 
     
  needquotes = true;
  for (s = str; *s; s++)
  {
    if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_' || *s == '.'))
    {
      needquotes = true;
      break;
    }
    needquotes = false;
  }

  if (needquotes)
  {
    appendPQExpBufferChar(buf, '\'');
    while (*str)
    {
                                                   
      if (*str == '\'' || *str == '\\')
      {
        appendPQExpBufferChar(buf, '\\');
      }

      appendPQExpBufferChar(buf, *str);
      str++;
    }
    appendPQExpBufferChar(buf, '\'');
  }
  else
  {
    appendPQExpBufferStr(buf, str);
  }
}

   
                                                                           
                                                  
   
void
appendPsqlMetaConnect(PQExpBuffer buf, const char *dbname)
{
  const char *s;
  bool complex;

     
                                                                             
                                                                          
                                                        
     
  complex = false;

  for (s = dbname; *s; s++)
  {
    if (*s == '\n' || *s == '\r')
    {
      fprintf(stderr, _("database name contains a newline or carriage return: \"%s\"\n"), dbname);
      exit(EXIT_FAILURE);
    }

    if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_' || *s == '.'))
    {
      complex = true;
    }
  }

  appendPQExpBufferStr(buf, "\\connect ");
  if (complex)
  {
    PQExpBufferData connstr;

    initPQExpBuffer(&connstr);
    appendPQExpBuffer(&connstr, "dbname=");
    appendConnStrVal(&connstr, dbname);

    appendPQExpBuffer(buf, "-reuse-previous=on ");

       
                                                                      
                                                                      
                                                                         
                              
       
    appendPQExpBufferStr(buf, fmtId(connstr.data));

    termPQExpBuffer(&connstr);
  }
  else
  {
    appendPQExpBufferStr(buf, fmtId(dbname));
  }
  appendPQExpBufferChar(buf, '\n');
}

   
                                                                         
                          
   
                                                                        
                                                                     
                                    
   
                                                                             
   
bool
parsePGArray(const char *atext, char ***itemarray, int *nitems)
{
  int inputlen;
  char **items;
  char *strings;
  int curitem;

     
                                                                         
                                                                             
                                                                   
     
                                                                        
                                                                           
                                                                      
                                                                   
     
  *itemarray = NULL;
  *nitems = 0;
  inputlen = strlen(atext);
  if (inputlen < 2 || atext[0] != '{' || atext[inputlen - 1] != '}')
  {
    return false;                
  }
  items = (char **)malloc(inputlen * (sizeof(char *) + sizeof(char)));
  if (items == NULL)
  {
    return false;                    
  }
  *itemarray = items;
  strings = (char *)(items + inputlen);

  atext++;                               
  curitem = 0;
  while (*atext != '}')
  {
    if (*atext == '\0')
    {
      return false;                              
    }
    items[curitem] = strings;
    while (*atext != '}' && *atext != ',')
    {
      if (*atext == '\0')
      {
        return false;                              
      }
      if (*atext != '"')
      {
        *strings++ = *atext++;                         
      }
      else
      {
                                      
        atext++;
        while (*atext != '"')
        {
          if (*atext == '\0')
          {
            return false;                              
          }
          if (*atext == '\\')
          {
            atext++;
            if (*atext == '\0')
            {
              return false;                              
            }
          }
          *strings++ = *atext++;                       
        }
        atext++;
      }
    }
    *strings++ = '\0';
    if (*atext == ',')
    {
      atext++;
    }
    curitem++;
  }
  if (atext[1] != '\0')
  {
    return false;                                  
  }
  *nitems = curitem;
  return true;
}

   
                                                                
   
                                                                             
   
                                                                            
                                                                       
   
                                                                              
                         
   
bool
appendReloptionsArray(PQExpBuffer buffer, const char *reloptions, const char *prefix, int encoding, bool std_strings)
{
  char **options;
  int noptions;
  int i;

  if (!parsePGArray(reloptions, &options, &noptions))
  {
    if (options)
    {
      free(options);
    }
    return false;
  }

  for (i = 0; i < noptions; i++)
  {
    char *option = options[i];
    char *name;
    char *separator;
    char *value;

       
                                                                          
                                                              
       
    name = option;
    separator = strchr(option, '=');
    if (separator)
    {
      *separator = '\0';
      value = separator + 1;
    }
    else
    {
      value = "";
    }

    if (i > 0)
    {
      appendPQExpBufferStr(buffer, ", ");
    }
    appendPQExpBuffer(buffer, "%s%s=", prefix, fmtId(name));

       
                                                                       
                                                                        
                                                                          
                                                                          
                                                                        
                    
       
    if (strcmp(fmtId(value), value) == 0)
    {
      appendPQExpBufferStr(buffer, value);
    }
    else
    {
      appendStringLiteral(buffer, value, encoding, std_strings);
    }
  }

  if (options)
  {
    free(options);
  }

  return true;
}

   
                         
   
                                                                         
                                                                         
                                                                       
                         
   
                                                                          
                          
                                                                             
                                                                             
                                    
                                                                      
                                                                    
                                                                             
                             
                                                                            
                                                                               
                                                                           
                                                                         
   
                                                                               
                                                     
   
bool
processSQLNamePattern(PGconn *conn, PQExpBuffer buf, const char *pattern, bool have_where, bool force_escape, const char *schemavar, const char *namevar, const char *altnamevar, const char *visibilityrule)
{
  PQExpBufferData schemabuf;
  PQExpBufferData namebuf;
  int encoding = PQclientEncoding(conn);
  bool inquotes;
  const char *cp;
  int i;
  bool added_clause = false;

#define WHEREAND() (appendPQExpBufferStr(buf, have_where ? "  AND " : "WHERE "), have_where = true, added_clause = true)

  if (pattern == NULL)
  {
                                              
    if (visibilityrule)
    {
      WHEREAND();
      appendPQExpBuffer(buf, "%s\n", visibilityrule);
    }
    return added_clause;
  }

  initPQExpBuffer(&schemabuf);
  initPQExpBuffer(&namebuf);

     
                                                                             
                                                                        
     
                                                                           
                                                                             
                                                                         
                                                  
     
                                                                           
                                                                        
                                            
     
  appendPQExpBufferStr(&namebuf, "^(");

  inquotes = false;
  cp = pattern;

  while (*cp)
  {
    char ch = *cp;

    if (ch == '"')
    {
      if (inquotes && cp[1] == '"')
      {
                                                   
        appendPQExpBufferChar(&namebuf, '"');
        cp++;
      }
      else
      {
        inquotes = !inquotes;
      }
      cp++;
    }
    else if (!inquotes && isupper((unsigned char)ch))
    {
      appendPQExpBufferChar(&namebuf, pg_tolower((unsigned char)ch));
      cp++;
    }
    else if (!inquotes && ch == '*')
    {
      appendPQExpBufferStr(&namebuf, ".*");
      cp++;
    }
    else if (!inquotes && ch == '?')
    {
      appendPQExpBufferChar(&namebuf, '.');
      cp++;
    }
    else if (!inquotes && ch == '.')
    {
                                                                       
      resetPQExpBuffer(&schemabuf);
      appendPQExpBufferStr(&schemabuf, namebuf.data);
      resetPQExpBuffer(&namebuf);
      appendPQExpBufferStr(&namebuf, "^(");
      cp++;
    }
    else if (ch == '$')
    {
         
                                                                    
                                                                      
                                                                       
                                                                      
                                               
         
      appendPQExpBufferStr(&namebuf, "\\$");
      cp++;
    }
    else
    {
         
                                                      
         
                                                                        
                                                                   
                                                                        
                                                                       
                                                           
         
      if ((inquotes || force_escape) && strchr("|*+?()[]{}.^$\\", ch))
      {
        appendPQExpBufferChar(&namebuf, '\\');
      }
      i = PQmblen(cp, encoding);
      while (i-- && *cp)
      {
        appendPQExpBufferChar(&namebuf, *cp);
        cp++;
      }
    }
  }

     
                                                                  
                                                                            
                                  
     
                                                                             
                                                                             
                                                                             
                                                                          
                                                                          
     
  if (namebuf.len > 2)
  {
                                                             

    appendPQExpBufferStr(&namebuf, ")$");
                                     
    if (strcmp(namebuf.data, "^(.*)$") != 0)
    {
      WHEREAND();
      if (altnamevar)
      {
        appendPQExpBuffer(buf, "(%s OPERATOR(pg_catalog.~) ", namevar);
        appendStringLiteralConn(buf, namebuf.data, conn);
        if (PQserverVersion(conn) >= 120000)
        {
          appendPQExpBufferStr(buf, " COLLATE pg_catalog.default");
        }
        appendPQExpBuffer(buf, "\n        OR %s OPERATOR(pg_catalog.~) ", altnamevar);
        appendStringLiteralConn(buf, namebuf.data, conn);
        if (PQserverVersion(conn) >= 120000)
        {
          appendPQExpBufferStr(buf, " COLLATE pg_catalog.default");
        }
        appendPQExpBufferStr(buf, ")\n");
      }
      else
      {
        appendPQExpBuffer(buf, "%s OPERATOR(pg_catalog.~) ", namevar);
        appendStringLiteralConn(buf, namebuf.data, conn);
        if (PQserverVersion(conn) >= 120000)
        {
          appendPQExpBufferStr(buf, " COLLATE pg_catalog.default");
        }
        appendPQExpBufferChar(buf, '\n');
      }
    }
  }

  if (schemabuf.len > 2)
  {
                                                              

    appendPQExpBufferStr(&schemabuf, ")$");
                                     
    if (strcmp(schemabuf.data, "^(.*)$") != 0 && schemavar)
    {
      WHEREAND();
      appendPQExpBuffer(buf, "%s OPERATOR(pg_catalog.~) ", schemavar);
      appendStringLiteralConn(buf, schemabuf.data, conn);
      if (PQserverVersion(conn) >= 120000)
      {
        appendPQExpBufferStr(buf, " COLLATE pg_catalog.default");
      }
      appendPQExpBufferChar(buf, '\n');
    }
  }
  else
  {
                                                                 
    if (visibilityrule)
    {
      WHEREAND();
      appendPQExpBuffer(buf, "%s\n", visibilityrule);
    }
  }

  termPQExpBuffer(&schemabuf);
  termPQExpBuffer(&namebuf);

  return added_clause;
#undef WHEREAND
}
