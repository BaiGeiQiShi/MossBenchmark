                                                                            
   
             
                                                                    
   
                                                                
   
                                                                            
   

#include "postgres_fe.h"

#include <sys/stat.h>
#include <unistd.h>

#include "datapagemap.h"
#include "filemap.h"
#include "pg_rewind.h"

#include "common/string.h"
#include "catalog/pg_tablespace_d.h"
#include "storage/fd.h"

filemap_t *filemap = NULL;

static bool
isRelDataFile(const char *path);
static char *
datasegpath(RelFileNode rnode, ForkNumber forknum, BlockNumber segno);
static int
path_cmp(const void *a, const void *b);
static int
final_filemap_cmp(const void *a, const void *b);
static void
filemap_list_to_array(filemap_t *map);
static bool
check_file_excluded(const char *path, bool is_source);

   
                                                                        
                                                                       
                                                                       
                                    
   
struct exclude_list_item
{
  const char *name;
  bool match_prefix;
};

   
                                                                            
                                                                  
   
                                                                             
                                                                            
                                                                         
                               
   
static const char *excludeDirContents[] = {
       
                                                                             
                                                                         
                      
       
    "pg_stat_tmp",                                 

       
                                                                           
                                                                              
                                        
       
    "pg_replslot",

                                                                  
    "pg_dynshmem",                                 

                                                            
    "pg_notify",

       
                                                                               
                                              
       
    "pg_serial",

                                                                            
    "pg_snapshots",

                                                            
    "pg_subtrans",

                     
    NULL};

   
                                                                        
                          
   
static const struct exclude_list_item excludeFiles[] = {
                                        
    {"postgresql.auto.conf.tmp", false},                                      

                                              
    {"current_logfiles.tmp", false},               
                                                                    

                                                              
    {"pg_internal.init", true},                                        

       
                                                                         
                                                                               
                                                                          
       
    {"backup_label", false},                                     
    {"tablespace_map", false},                                

    {"postmaster.pid", false}, {"postmaster.opts", false},

                     
    {NULL, false}};

   
                                                                   
   
void
filemap_create(void)
{
  filemap_t *map;

  map = pg_malloc(sizeof(filemap_t));
  map->first = map->last = NULL;
  map->nlist = 0;
  map->array = NULL;
  map->narray = 0;

  Assert(filemap == NULL);
  filemap = map;
}

   
                                             
   
                                                                           
                                                                        
                                                      
   
void
process_source_file(const char *path, file_type_t type, size_t newsize, const char *link_target)
{
  bool exists;
  char localpath[MAXPGPATH];
  struct stat statbuf;
  filemap_t *map = filemap;
  file_action_t action = FILE_ACTION_NONE;
  size_t oldsize = 0;
  file_entry_t *entry;

  Assert(map->array == NULL);

     
                                                                           
                                           
     
  if (check_file_excluded(path, true))
  {
    return;
  }

     
                                                                           
                                                                        
                                                        
     
  if (strcmp(path, "pg_wal") == 0 && type == FILE_TYPE_SYMLINK)
  {
    type = FILE_TYPE_DIRECTORY;
  }

     
                                                                            
                                                                             
              
     
  if (strstr(path, "/" PG_TEMP_FILE_PREFIX) != NULL)
  {
    return;
  }
  if (strstr(path, "/" PG_TEMP_FILES_DIR "/") != NULL)
  {
    return;
  }

     
                                                                      
                  
     
  if (type != FILE_TYPE_REGULAR && isRelDataFile(path))
  {
    pg_fatal("data file \"%s\" in source is not a regular file", path);
  }

  snprintf(localpath, sizeof(localpath), "%s/%s", datadir_target, path);

                                                                 
  if (lstat(localpath, &statbuf) < 0)
  {
    if (errno != ENOENT)
    {
      pg_fatal("could not stat file \"%s\": %m", localpath);
    }

    exists = false;
  }
  else
  {
    exists = true;
  }

  switch (type)
  {
  case FILE_TYPE_DIRECTORY:
    if (exists && !S_ISDIR(statbuf.st_mode) && strcmp(path, "pg_wal") != 0)
    {
                                                                    
      pg_fatal("\"%s\" is not a directory", localpath);
    }

    if (!exists)
    {
      action = FILE_ACTION_CREATE;
    }
    else
    {
      action = FILE_ACTION_NONE;
    }
    oldsize = 0;
    break;

  case FILE_TYPE_SYMLINK:
    if (exists &&
#ifndef WIN32
        !S_ISLNK(statbuf.st_mode)
#else
        !pgwin32_is_junction(localpath)
#endif
    )
    {
         
                                                            
                   
         
      pg_fatal("\"%s\" is not a symbolic link", localpath);
    }

    if (!exists)
    {
      action = FILE_ACTION_CREATE;
    }
    else
    {
      action = FILE_ACTION_NONE;
    }
    oldsize = 0;
    break;

  case FILE_TYPE_REGULAR:
    if (exists && !S_ISREG(statbuf.st_mode))
    {
      pg_fatal("\"%s\" is not a regular file", localpath);
    }

    if (!exists || !isRelDataFile(path))
    {
         
                                                             
                                                                    
                     
         
                                                                  
                                      
         
      if (pg_str_endswith(path, "PG_VERSION"))
      {
        action = FILE_ACTION_NONE;
        oldsize = statbuf.st_size;
      }
      else
      {
        action = FILE_ACTION_COPY;
        oldsize = 0;
      }
    }
    else
    {
         
                                               
         
                                                                  
                                                              
                                                                    
                                              
         
                                                                  
                                                                
                                                                  
                                                                     
                                                                
                                                                  
                                                                
                                                                  
                                   
         
                                                                     
                                                                  
                                                                     
                                                                  
                   
         
      oldsize = statbuf.st_size;
      if (oldsize < newsize)
      {
        action = FILE_ACTION_COPY_TAIL;
      }
      else if (oldsize > newsize)
      {
        action = FILE_ACTION_TRUNCATE;
      }
      else
      {
        action = FILE_ACTION_NONE;
      }
    }
    break;
  }

                                        
  entry = pg_malloc(sizeof(file_entry_t));
  entry->path = pg_strdup(path);
  entry->type = type;
  entry->action = action;
  entry->oldsize = oldsize;
  entry->newsize = newsize;
  entry->link_target = link_target ? pg_strdup(link_target) : NULL;
  entry->next = NULL;
  entry->pagemap.bitmap = NULL;
  entry->pagemap.bitmapsize = 0;
  entry->isrelfile = isRelDataFile(path);

  if (map->last)
  {
    map->last->next = entry;
    map->last = entry;
  }
  else
  {
    map->first = map->last = entry;
  }
  map->nlist++;
}

   
                                             
   
                                                                             
                                                                           
             
   
void
process_target_file(const char *path, file_type_t type, size_t oldsize, const char *link_target)
{
  bool exists;
  file_entry_t key;
  file_entry_t *key_ptr;
  filemap_t *map = filemap;
  file_entry_t *entry;

     
                                                                            
                                                                             
                                                              
     

  if (map->array == NULL)
  {
                                                
    if (map->nlist == 0)
    {
                             
      pg_fatal("source file list is empty");
    }

    filemap_list_to_array(map);

    Assert(map->array != NULL);

    qsort(map->array, map->narray, sizeof(file_entry_t *), path_cmp);
  }

     
                                                                            
     
  if (strcmp(path, "pg_wal") == 0 && type == FILE_TYPE_SYMLINK)
  {
    type = FILE_TYPE_DIRECTORY;
  }

  key.path = (char *)path;
  key_ptr = &key;
  exists = (bsearch(&key_ptr, map->array, map->narray, sizeof(file_entry_t *), path_cmp) != NULL);

                                                                          
  if (!exists)
  {
    entry = pg_malloc(sizeof(file_entry_t));
    entry->path = pg_strdup(path);
    entry->type = type;
    entry->action = FILE_ACTION_REMOVE;
    entry->oldsize = oldsize;
    entry->newsize = 0;
    entry->link_target = link_target ? pg_strdup(link_target) : NULL;
    entry->next = NULL;
    entry->pagemap.bitmap = NULL;
    entry->pagemap.bitmapsize = 0;
    entry->isrelfile = isRelDataFile(path);

    if (map->last == NULL)
    {
      map->first = entry;
    }
    else
    {
      map->last->next = entry;
    }
    map->last = entry;
    map->nlist++;
  }
  else
  {
       
                                                                       
                              
       
  }
}

   
                                                                            
                                                                          
                                              
   
void
process_block_change(ForkNumber forknum, RelFileNode rnode, BlockNumber blkno)
{
  char *path;
  file_entry_t key;
  file_entry_t *key_ptr;
  file_entry_t *entry;
  BlockNumber blkno_inseg;
  int segno;
  filemap_t *map = filemap;
  file_entry_t **e;

  Assert(map->array);

  segno = blkno / RELSEG_SIZE;
  blkno_inseg = blkno % RELSEG_SIZE;

  path = datasegpath(rnode, forknum, segno);

  key.path = (char *)path;
  key_ptr = &key;

  e = bsearch(&key_ptr, map->array, map->narray, sizeof(file_entry_t *), path_cmp);
  if (e)
  {
    entry = *e;
  }
  else
  {
    entry = NULL;
  }
  pfree(path);

  if (entry)
  {
    Assert(entry->isrelfile);

    switch (entry->action)
    {
    case FILE_ACTION_NONE:
    case FILE_ACTION_TRUNCATE:
                                                                   
      if ((blkno_inseg + 1) * BLCKSZ <= entry->newsize)
      {
        datapagemap_add(&entry->pagemap, blkno_inseg);
      }
      break;

    case FILE_ACTION_COPY_TAIL:

         
                                                                  
                               
         
      if ((blkno_inseg + 1) * BLCKSZ <= entry->oldsize)
      {
        datapagemap_add(&entry->pagemap, blkno_inseg);
      }
      break;

    case FILE_ACTION_COPY:
    case FILE_ACTION_REMOVE:
      break;

    case FILE_ACTION_CREATE:
      pg_fatal("unexpected page modification for directory or symbolic link \"%s\"", entry->path);
    }
  }
  else
  {
       
                                                                          
                                                                         
                                                                     
                         
       
  }
}

   
                                                             
   
static bool
check_file_excluded(const char *path, bool is_source)
{
  char localpath[MAXPGPATH];
  int excludeIdx;
  const char *filename;

                                 
  for (excludeIdx = 0; excludeFiles[excludeIdx].name != NULL; excludeIdx++)
  {
    int cmplen = strlen(excludeFiles[excludeIdx].name);

    filename = last_dir_separator(path);
    if (filename == NULL)
    {
      filename = path;
    }
    else
    {
      filename++;
    }

    if (!excludeFiles[excludeIdx].match_prefix)
    {
      cmplen++;
    }
    if (strncmp(filename, excludeFiles[excludeIdx].name, cmplen) == 0)
    {
      if (is_source)
      {
        pg_log_debug("entry \"%s\" excluded from source file list", path);
      }
      else
      {
        pg_log_debug("entry \"%s\" excluded from target file list", path);
      }
      return true;
    }
  }

     
                                                                           
                                        
     
  for (excludeIdx = 0; excludeDirContents[excludeIdx] != NULL; excludeIdx++)
  {
    snprintf(localpath, sizeof(localpath), "%s/", excludeDirContents[excludeIdx]);
    if (strstr(path, localpath) == path)
    {
      if (is_source)
      {
        pg_log_debug("entry \"%s\" excluded from source file list", path);
      }
      else
      {
        pg_log_debug("entry \"%s\" excluded from target file list", path);
      }
      return true;
    }
  }

  return false;
}

   
                                                                       
               
   
static void
filemap_list_to_array(filemap_t *map)
{
  int narray;
  file_entry_t *entry, *next;

  map->array = (file_entry_t **)pg_realloc(map->array, (map->nlist + map->narray) * sizeof(file_entry_t *));

  narray = map->narray;
  for (entry = map->first; entry != NULL; entry = next)
  {
    map->array[narray++] = entry;
    next = entry->next;
    entry->next = NULL;
  }
  Assert(narray == map->nlist + map->narray);
  map->narray = narray;
  map->nlist = 0;
  map->first = map->last = NULL;
}

void
filemap_finalize(void)
{
  filemap_t *map = filemap;

  filemap_list_to_array(map);
  qsort(map->array, map->narray, sizeof(file_entry_t *), final_filemap_cmp);
}

static const char *
action_to_str(file_action_t action)
{
  switch (action)
  {
  case FILE_ACTION_NONE:
    return "NONE";
  case FILE_ACTION_COPY:
    return "COPY";
  case FILE_ACTION_TRUNCATE:
    return "TRUNCATE";
  case FILE_ACTION_COPY_TAIL:
    return "COPY_TAIL";
  case FILE_ACTION_CREATE:
    return "CREATE";
  case FILE_ACTION_REMOVE:
    return "REMOVE";

  default:
    return "unknown";
  }
}

   
                                                     
   
void
calculate_totals(void)
{
  file_entry_t *entry;
  int i;
  filemap_t *map = filemap;

  map->total_size = 0;
  map->fetch_size = 0;

  for (i = 0; i < map->narray; i++)
  {
    entry = map->array[i];

    if (entry->type != FILE_TYPE_REGULAR)
    {
      continue;
    }

    map->total_size += entry->newsize;

    if (entry->action == FILE_ACTION_COPY)
    {
      map->fetch_size += entry->newsize;
      continue;
    }

    if (entry->action == FILE_ACTION_COPY_TAIL)
    {
      map->fetch_size += (entry->newsize - entry->oldsize);
    }

    if (entry->pagemap.bitmapsize > 0)
    {
      datapagemap_iterator_t *iter;
      BlockNumber blk;

      iter = datapagemap_iterate(&entry->pagemap);
      while (datapagemap_next(iter, &blk))
      {
        map->fetch_size += BLCKSZ;
      }

      pg_free(iter);
    }
  }
}

void
print_filemap(void)
{
  filemap_t *map = filemap;
  file_entry_t *entry;
  int i;

  for (i = 0; i < map->narray; i++)
  {
    entry = map->array[i];
    if (entry->action != FILE_ACTION_NONE || entry->pagemap.bitmapsize > 0)
    {
      pg_log_debug("%s (%s)", entry->path, action_to_str(entry->action));

      if (entry->pagemap.bitmapsize > 0)
      {
        datapagemap_print(&entry->pagemap);
      }
    }
  }
  fflush(stdout);
}

   
                                           
   
                                                                          
                                                                            
                                                                              
                      
   
static bool
isRelDataFile(const char *path)
{
  RelFileNode rnode;
  unsigned int segNo;
  int nmatch;
  bool matched;

         
                                                                     
     
             
                       
     
                    
                                            
     
                                              
                                                                 
                          
     
                                                                  
     
                            
     
         
     
  rnode.spcNode = InvalidOid;
  rnode.dbNode = InvalidOid;
  rnode.relNode = InvalidOid;
  segNo = 0;
  matched = false;

  nmatch = sscanf(path, "global/%u.%u", &rnode.relNode, &segNo);
  if (nmatch == 1 || nmatch == 2)
  {
    rnode.spcNode = GLOBALTABLESPACE_OID;
    rnode.dbNode = 0;
    matched = true;
  }
  else
  {
    nmatch = sscanf(path, "base/%u/%u.%u", &rnode.dbNode, &rnode.relNode, &segNo);
    if (nmatch == 2 || nmatch == 3)
    {
      rnode.spcNode = DEFAULTTABLESPACE_OID;
      matched = true;
    }
    else
    {
      nmatch = sscanf(path, "pg_tblspc/%u/" TABLESPACE_VERSION_DIRECTORY "/%u/%u.%u", &rnode.spcNode, &rnode.dbNode, &rnode.relNode, &segNo);
      if (nmatch == 3 || nmatch == 4)
      {
        matched = true;
      }
    }
  }

     
                                                                          
                                                                        
                                                                  
                                                 
     
  if (matched)
  {
    char *check_path = datasegpath(rnode, MAIN_FORKNUM, segNo);

    if (strcmp(check_path, path) != 0)
    {
      matched = false;
    }

    pfree(check_path);
  }

  return matched;
}

   
                                                                        
   
                                 
   
static char *
datasegpath(RelFileNode rnode, ForkNumber forknum, BlockNumber segno)
{
  char *path;
  char *segpath;

  path = relpathperm(rnode, forknum);
  if (segno > 0)
  {
    segpath = psprintf("%s.%u", path, segno);
    pfree(path);
    return segpath;
  }
  else
  {
    return path;
  }
}

static int
path_cmp(const void *a, const void *b)
{
  file_entry_t *fa = *((file_entry_t **)a);
  file_entry_t *fb = *((file_entry_t **)b);

  return strcmp(fa->path, fb->path);
}

   
                                                                         
                                                                          
                                                                          
                                                                          
                                                                            
                                                                         
                                                                        
                                                                       
   
static int
final_filemap_cmp(const void *a, const void *b)
{
  file_entry_t *fa = *((file_entry_t **)a);
  file_entry_t *fb = *((file_entry_t **)b);

  if (fa->action > fb->action)
  {
    return 1;
  }
  if (fa->action < fb->action)
  {
    return -1;
  }

  if (fa->action == FILE_ACTION_REMOVE)
  {
    return strcmp(fb->path, fa->path);
  }
  else
  {
    return strcmp(fa->path, fb->path);
  }
}
