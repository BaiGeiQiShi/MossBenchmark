                                                                            
   
           
                                    
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include <sys/stat.h>

#ifdef HAVE_DLOPEN
#include <dlfcn.h>

   
                                                                       
                                                 
   
#ifndef USE_STDBOOL
#ifdef bool
#undef bool
#endif
#endif
#endif                  

#include "fmgr.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "storage/shmem.h"
#include "utils/hsearch.h"

                                                                    
typedef void (*PG_init_t)(void);
typedef void (*PG_fini_t)(void);

                                              
typedef struct
{
  char varName[NAMEDATALEN];                               
  void *varValue;
} rendezvousHashEntry;

   
                                                               
   

typedef struct df_files
{
  struct df_files *next;                
  dev_t device;                                 
#ifndef WIN32                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
                                    
  ino_t inode;                                     
#endif
  void *handle;                                                            
  char filename[FLEXIBLE_ARRAY_MEMBER];                            
} DynamicFileList;

static DynamicFileList *file_list = NULL;
static DynamicFileList *file_tail = NULL;

                                                                            
#ifndef WIN32
#define SAME_INODE(A, B) ((A).st_ino == (B).inode && (A).st_dev == (B).device)
#else
#define SAME_INODE(A, B) false
#endif

char *Dynamic_library_path;

static void *
internal_load_library(const char *libname);
static void
incompatible_module_error(const char *libname, const Pg_magic_struct *module_magic_data) pg_attribute_noreturn();
static void
internal_unload_library(const char *libname);
static bool
file_exists(const char *name);
static char *
expand_dynamic_library_name(const char *name);
static void
check_restricted_library_name(const char *name);
static char *
substitute_libpath_macro(const char *name);
static char *
find_in_dynamic_libpath(const char *basename);

                                                               
static const Pg_magic_struct magic_data = PG_MODULE_MAGIC_DATA;

   
                                                                         
                         
   
                                                                              
                                                                           
                                                        
   
                                                                       
                                                                  
                                                                            
                                                       
   
PGFunction
load_external_function(const char *filename, const char *funcname, bool signalNotFound, void **filehandle)
{
  char *fullname;
  void *lib_handle;
  PGFunction retval;

                                                                      
  fullname = expand_dynamic_library_name(filename);

                                                      
  lib_handle = internal_load_library(fullname);

                                        
  if (filehandle)
  {
    *filehandle = lib_handle;
  }

                                                
  retval = (PGFunction)dlsym(lib_handle, funcname);

  if (retval == NULL && signalNotFound)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not find function \"%s\" in file \"%s\"", funcname, fullname)));
  }

  pfree(fullname);
  return retval;
}

   
                                                                      
                                                                  
                         
   
                                                                    
                                                
   
void
load_file(const char *filename, bool restricted)
{
  char *fullname;

                                               
  if (restricted)
  {
    check_restricted_library_name(filename);
  }

                                                                      
  fullname = expand_dynamic_library_name(filename);

                                              
  internal_unload_library(fullname);

                               
  (void)internal_load_library(fullname);

  pfree(fullname);
}

   
                                                           
                                          
   
PGFunction
lookup_external_function(void *filehandle, const char *funcname)
{
  return (PGFunction)dlsym(filehandle, funcname);
}

   
                                                                      
                                                   
   
                                                                       
   
static void *
internal_load_library(const char *libname)
{
  DynamicFileList *file_scanner;
  PGModuleMagicFunction magic_func;
  char *load_error;
  struct stat stat_buf;
  PG_init_t PG_init;

     
                                                                       
     
  for (file_scanner = file_list; file_scanner != NULL && strcmp(libname, file_scanner->filename) != 0; file_scanner = file_scanner->next)
    ;

  if (file_scanner == NULL)
  {
       
                                                                    
       
    if (stat(libname, &stat_buf) == -1)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not access file \"%s\": %m", libname)));
    }

    for (file_scanner = file_list; file_scanner != NULL && !SAME_INODE(stat_buf, *file_scanner); file_scanner = file_scanner->next)
      ;
  }

  if (file_scanner == NULL)
  {
       
                            
       
    file_scanner = (DynamicFileList *)malloc(offsetof(DynamicFileList, filename) + strlen(libname) + 1);
    if (file_scanner == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
    }

    MemSet(file_scanner, 0, offsetof(DynamicFileList, filename));
    strcpy(file_scanner->filename, libname);
    file_scanner->device = stat_buf.st_dev;
#ifndef WIN32
    file_scanner->inode = stat_buf.st_ino;
#endif
    file_scanner->next = NULL;

    file_scanner->handle = dlopen(file_scanner->filename, RTLD_NOW | RTLD_GLOBAL);
    if (file_scanner->handle == NULL)
    {
      load_error = dlerror();
      free((char *)file_scanner);
                                                                  
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not load library \"%s\": %s", libname, load_error)));
    }

                                                             
    magic_func = (PGModuleMagicFunction)dlsym(file_scanner->handle, PG_MAGIC_FUNCTION_NAME_STRING);
    if (magic_func)
    {
      const Pg_magic_struct *magic_data_ptr = (*magic_func)();

      if (magic_data_ptr->len != magic_data.len || memcmp(magic_data_ptr, &magic_data, magic_data.len) != 0)
      {
                                                      
        Pg_magic_struct module_magic_data = *magic_data_ptr;

                                  
        dlclose(file_scanner->handle);
        free((char *)file_scanner);

                                      
        incompatible_module_error(libname, &module_magic_data);
      }
    }
    else
    {
                                
      dlclose(file_scanner->handle);
      free((char *)file_scanner);
                    
      ereport(ERROR, (errmsg("incompatible library \"%s\": missing magic block", libname), errhint("Extension libraries are required to use the PG_MODULE_MAGIC macro.")));
    }

       
                                                          
       
    PG_init = (PG_init_t)dlsym(file_scanner->handle, "_PG_init");
    if (PG_init)
    {
      (*PG_init)();
    }

                                 
    if (file_list == NULL)
    {
      file_list = file_scanner;
    }
    else
    {
      file_tail->next = file_scanner;
    }
    file_tail = file_scanner;
  }

  return file_scanner->handle;
}

   
                                                            
   
static void
incompatible_module_error(const char *libname, const Pg_magic_struct *module_magic_data)
{
  StringInfoData details;

     
                                                                             
                                                     
     
  if (magic_data.version != module_magic_data->version)
  {
    char library_version[32];

    if (module_magic_data->version >= 1000)
    {
      snprintf(library_version, sizeof(library_version), "%d", module_magic_data->version / 100);
    }
    else
    {
      snprintf(library_version, sizeof(library_version), "%d.%d", module_magic_data->version / 100, module_magic_data->version % 100);
    }
    ereport(ERROR, (errmsg("incompatible library \"%s\": version mismatch", libname), errdetail("Server is version %d, library is version %s.", magic_data.version / 100, library_version)));
  }

     
                                                    
     
                                                                            
                   
     
  initStringInfo(&details);

  if (module_magic_data->funcmaxargs != magic_data.funcmaxargs)
  {
    if (details.len)
    {
      appendStringInfoChar(&details, '\n');
    }
    appendStringInfo(&details, _("Server has FUNC_MAX_ARGS = %d, library has %d."), magic_data.funcmaxargs, module_magic_data->funcmaxargs);
  }
  if (module_magic_data->indexmaxkeys != magic_data.indexmaxkeys)
  {
    if (details.len)
    {
      appendStringInfoChar(&details, '\n');
    }
    appendStringInfo(&details, _("Server has INDEX_MAX_KEYS = %d, library has %d."), magic_data.indexmaxkeys, module_magic_data->indexmaxkeys);
  }
  if (module_magic_data->namedatalen != magic_data.namedatalen)
  {
    if (details.len)
    {
      appendStringInfoChar(&details, '\n');
    }
    appendStringInfo(&details, _("Server has NAMEDATALEN = %d, library has %d."), magic_data.namedatalen, module_magic_data->namedatalen);
  }
  if (module_magic_data->float4byval != magic_data.float4byval)
  {
    if (details.len)
    {
      appendStringInfoChar(&details, '\n');
    }
    appendStringInfo(&details, _("Server has FLOAT4PASSBYVAL = %s, library has %s."), magic_data.float4byval ? "true" : "false", module_magic_data->float4byval ? "true" : "false");
  }
  if (module_magic_data->float8byval != magic_data.float8byval)
  {
    if (details.len)
    {
      appendStringInfoChar(&details, '\n');
    }
    appendStringInfo(&details, _("Server has FLOAT8PASSBYVAL = %s, library has %s."), magic_data.float8byval ? "true" : "false", module_magic_data->float8byval ? "true" : "false");
  }

  if (details.len == 0)
  {
    appendStringInfoString(&details, _("Magic block has unexpected length or padding difference."));
  }

  ereport(ERROR, (errmsg("incompatible library \"%s\": magic block mismatch", libname), errdetail_internal("%s", details.data)));
}

   
                                                                    
   
                                                                       
   
                                                                                
                                                                          
                                                                               
                                                                           
                                    
   
static void
internal_unload_library(const char *libname)
{
#ifdef NOT_USED
  DynamicFileList *file_scanner, *prv, *nxt;
  struct stat stat_buf;
  PG_fini_t PG_fini;

     
                                                                         
                                                                            
                                             
     
  if (stat(libname, &stat_buf) == -1)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not access file \"%s\": %m", libname)));
  }

     
                                                                             
                                                                        
     
  prv = NULL;
  for (file_scanner = file_list; file_scanner != NULL; file_scanner = nxt)
  {
    nxt = file_scanner->next;
    if (strcmp(libname, file_scanner->filename) == 0 || SAME_INODE(stat_buf, *file_scanner))
    {
      if (prv)
      {
        prv->next = nxt;
      }
      else
      {
        file_list = nxt;
      }

         
                                                            
         
      PG_fini = (PG_fini_t)dlsym(file_scanner->handle, "_PG_fini");
      if (PG_fini)
      {
        (*PG_fini)();
      }

      clear_external_function_hash(file_scanner->handle);
      dlclose(file_scanner->handle);
      free((char *)file_scanner);
                               
    }
    else
    {
      prv = file_scanner;
    }
  }
#endif               
}

static bool
file_exists(const char *name)
{
  struct stat st;

  AssertArg(name != NULL);

  if (stat(name, &st) == 0)
  {
    return S_ISDIR(st.st_mode) ? false : true;
  }
  else if (!(errno == ENOENT || errno == ENOTDIR || errno == EACCES))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not access file \"%s\": %m", name)));
  }

  return false;
}

                           
#ifndef DLSUFFIX
#error "DLSUFFIX must be defined to compile this file."
#endif

   
                                                                    
                                                                   
                                                                   
                                                                    
                                                            
   
                                               
   
static char *
expand_dynamic_library_name(const char *name)
{
  bool have_slash;
  char *new;
  char *full;

  AssertArg(name);

  have_slash = (first_dir_separator(name) != NULL);

  if (!have_slash)
  {
    full = find_in_dynamic_libpath(name);
    if (full)
    {
      return full;
    }
  }
  else
  {
    full = substitute_libpath_macro(name);
    if (file_exists(full))
    {
      return full;
    }
    pfree(full);
  }

  new = psprintf("%s%s", name, DLSUFFIX);

  if (!have_slash)
  {
    full = find_in_dynamic_libpath(new);
    pfree(new);
    if (full)
    {
      return full;
    }
  }
  else
  {
    full = substitute_libpath_macro(new);
    pfree(new);
    if (file_exists(full))
    {
      return full;
    }
    pfree(full);
  }

     
                                                                          
                                                           
     
  return pstrdup(name);
}

   
                                                                           
                                                                      
                                              
   
static void
check_restricted_library_name(const char *name)
{
  if (strncmp(name, "$libdir/plugins/", 16) != 0 || first_dir_separator(name + 16) != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("access to library \"%s\" is not allowed", name)));
  }
}

   
                                                            
                                      
   
static char *
substitute_libpath_macro(const char *name)
{
  const char *sep_ptr;

  AssertArg(name != NULL);

                                                                       
  if (name[0] != '$')
  {
    return pstrdup(name);
  }

  if ((sep_ptr = first_dir_separator(name)) == NULL)
  {
    sep_ptr = name + strlen(name);
  }

  if (strlen("$libdir") != sep_ptr - name || strncmp(name, "$libdir", strlen("$libdir")) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("invalid macro name in dynamic library path: %s", name)));
  }

  return psprintf("%s%s", pkglib_path, sep_ptr);
}

   
                                                                     
                                                                        
                                                                      
                
   
static char *
find_in_dynamic_libpath(const char *basename)
{
  const char *p;
  size_t baselen;

  AssertArg(basename != NULL);
  AssertArg(first_dir_separator(basename) == NULL);
  AssertState(Dynamic_library_path != NULL);

  p = Dynamic_library_path;
  if (strlen(p) == 0)
  {
    return NULL;
  }

  baselen = strlen(basename);

  for (;;)
  {
    size_t len;
    char *piece;
    char *mangled;
    char *full;

    piece = first_path_var_separator(p);
    if (piece == p)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("zero-length component in parameter \"dynamic_library_path\"")));
    }

    if (piece == NULL)
    {
      len = strlen(p);
    }
    else
    {
      len = piece - p;
    }

    piece = palloc(len + 1);
    strlcpy(piece, p, len + 1);

    mangled = substitute_libpath_macro(piece);
    pfree(piece);

    canonicalize_path(mangled);

                             
    if (!is_absolute_path(mangled))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("component in parameter \"dynamic_library_path\" is not an absolute path")));
    }

    full = palloc(strlen(mangled) + 1 + baselen + 1);
    sprintf(full, "%s/%s", mangled, basename);
    pfree(mangled);

    elog(DEBUG3, "find_in_dynamic_libpath: trying \"%s\"", full);

    if (file_exists(full))
    {
      return full;
    }

    pfree(full);

    if (p[len] == '\0')
    {
      break;
    }
    else
    {
      p += len + 1;
    }
  }

  return NULL;
}

   
                                                               
                                                   
   
                                                                
                                                           
                                                                        
                                                                     
                                                                     
                                
   
                                                             
                                                                     
                                                       
   
void **
find_rendezvous_variable(const char *varName)
{
  static HTAB *rendezvousHash = NULL;

  rendezvousHashEntry *hentry;
  bool found;

                                                                        
  if (rendezvousHash == NULL)
  {
    HASHCTL ctl;

    MemSet(&ctl, 0, sizeof(ctl));
    ctl.keysize = NAMEDATALEN;
    ctl.entrysize = sizeof(rendezvousHashEntry);
    rendezvousHash = hash_create("Rendezvous variable hash", 16, &ctl, HASH_ELEM);
  }

                                                           
  hentry = (rendezvousHashEntry *)hash_search(rendezvousHash, varName, HASH_ENTER, &found);

                                        
  if (!found)
  {
    hentry->varValue = NULL;
  }

  return &hentry->varValue;
}

   
                                                                          
                   
   
Size
EstimateLibraryStateSpace(void)
{
  DynamicFileList *file_scanner;
  Size size = 1;

  for (file_scanner = file_list; file_scanner != NULL; file_scanner = file_scanner->next)
  {
    size = add_size(size, strlen(file_scanner->filename) + 1);
  }

  return size;
}

   
                                                                        
   
void
SerializeLibraryState(Size maxsize, char *start_address)
{
  DynamicFileList *file_scanner;

  for (file_scanner = file_list; file_scanner != NULL; file_scanner = file_scanner->next)
  {
    Size len;

    len = strlcpy(start_address, file_scanner->filename, maxsize) + 1;
    Assert(len < maxsize);
    maxsize -= len;
    start_address += len;
  }
  start_address[0] = '\0';
}

   
                                                          
   
void
RestoreLibraryState(char *start_address)
{
  while (*start_address != '\0')
  {
    internal_load_library(start_address);
    start_address += strlen(start_address) + 1;
  }
}
