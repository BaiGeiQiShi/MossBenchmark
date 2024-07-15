   
                     
   
                               
   

#include "postgres.h"

#include "mb/pg_wchar.h"
#include "utils/memutils.h"

#include "plpython.h"

#include "plpy_util.h"

#include "plpy_elog.h"

   
                                                                      
                                                                     
           
   
PyObject *
PLyUnicode_Bytes(PyObject *unicode)
{
  PyObject *bytes, *rv;
  char *utf8string, *encoded;

                                                          
  bytes = PyUnicode_AsUTF8String(unicode);
  if (bytes == NULL)
  {
    PLy_elog(ERROR, "could not convert Python Unicode object to bytes");
  }

  utf8string = PyBytes_AsString(bytes);
  if (utf8string == NULL)
  {
    Py_DECREF(bytes);
    PLy_elog(ERROR, "could not extract bytes from encoded string");
  }

     
                                                   
     
                                                                           
                                                                          
                                                                          
                                                    
     
  if (GetDatabaseEncoding() != PG_UTF8)
  {
    PG_TRY();
    {
      encoded = pg_any_to_server(utf8string, strlen(utf8string), PG_UTF8);
    }
    PG_CATCH();
    {
      Py_DECREF(bytes);
      PG_RE_THROW();
    }
    PG_END_TRY();
  }
  else
  {
    encoded = utf8string;
  }

                                                            
  rv = PyBytes_FromStringAndSize(encoded, strlen(encoded));

                                                         
  if (utf8string != encoded)
  {
    pfree(encoded);
  }

  Py_DECREF(bytes);
  return rv;
}

   
                                                                      
                                                               
                                       
   
                                                                    
                                                                      
                                                                     
                                                              
                                                   
   
char *
PLyUnicode_AsString(PyObject *unicode)
{
  PyObject *o = PLyUnicode_Bytes(unicode);
  char *rv = pstrdup(PyBytes_AsString(o));

  Py_XDECREF(o);
  return rv;
}

#if PY_MAJOR_VERSION >= 3
   
                                                                    
                                                                 
   
PyObject *
PLyUnicode_FromStringAndSize(const char *s, Py_ssize_t size)
{
  char *utf8string;
  PyObject *o;

  utf8string = pg_server_to_any(s, size, PG_UTF8);

  if (utf8string == s)
  {
    o = PyUnicode_FromStringAndSize(s, size);
  }
  else
  {
    o = PyUnicode_FromString(utf8string);
    pfree(utf8string);
  }

  return o;
}

PyObject *
PLyUnicode_FromString(const char *s)
{
  return PLyUnicode_FromStringAndSize(s, strlen(s));
}

#endif                            
