                                                                            
   
            
                                  
   
                                                                         
                                                                        
   
                                                                
                          
   
                  
                       
   
                                                                            
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

                                                 
#if defined(WIN32) || defined(__CYGWIN__)
#undef rename
#undef unlink
#endif

#include <unistd.h>
#include <sys/stat.h>

#if defined(WIN32) || defined(__CYGWIN__)
#ifndef __CYGWIN__
#include <winioctl.h>
#else
#include <windows.h>
#include <w32api/winioctl.h>
#endif
#endif

#if defined(WIN32) || defined(__CYGWIN__)

   
            
   
int
pgrename(const char *from, const char *to)
{
  int loops = 0;

     
                                                                          
                                                                           
                                                                        
                                                                          
                                  
     
#if defined(WIN32) && !defined(__CYGWIN__)
  while (!MoveFileEx(from, to, MOVEFILE_REPLACE_EXISTING))
#else
  while (rename(from, to) < 0)
#endif
  {
#if defined(WIN32) && !defined(__CYGWIN__)
    DWORD err = GetLastError();

    _dosmaperr(err);

       
                                                                          
                                                                    
                                                                    
                                                                     
                                                                       
                                                                      
       
    if (err != ERROR_ACCESS_DENIED && err != ERROR_SHARING_VIOLATION && err != ERROR_LOCK_VIOLATION)
    {
      return -1;
    }
#else
    if (errno != EACCES)
    {
      return -1;
    }
#endif

    if (++loops > 100)                            
    {
      return -1;
    }
    pg_usleep(100000);         
  }
  return 0;
}

   
            
   
int
pgunlink(const char *path)
{
  int loops = 0;

     
                                                                          
                                                                           
                                                                        
                                                                          
                                  
     
  while (unlink(path))
  {
    if (errno != EACCES)
    {
      return -1;
    }
    if (++loops > 100)                            
    {
      return -1;
    }
    pg_usleep(100000);         
  }
  return 0;
}

                                                                   
#define rename(from, to) pgrename(from, to)
#define unlink(path) pgunlink(path)
#endif                                            

#if defined(WIN32) && !defined(__CYGWIN__)                                  

   
                      
   
                                                                                        
                                       
                                                                                   
   
typedef struct
{
  DWORD ReparseTag;
  WORD ReparseDataLength;
  WORD Reserved;
                                 
  WORD SubstituteNameOffset;
  WORD SubstituteNameLength;
  WORD PrintNameOffset;
  WORD PrintNameLength;
  WCHAR PathBuffer[FLEXIBLE_ARRAY_MEMBER];
} REPARSE_JUNCTION_DATA_BUFFER;

#define REPARSE_JUNCTION_DATA_BUFFER_HEADER_SIZE FIELD_OFFSET(REPARSE_JUNCTION_DATA_BUFFER, SubstituteNameOffset)

   
                                          
   
                                                                           
   
int
pgsymlink(const char *oldpath, const char *newpath)
{
  HANDLE dirhandle;
  DWORD len;
  char buffer[MAX_PATH * sizeof(WCHAR) + offsetof(REPARSE_JUNCTION_DATA_BUFFER, PathBuffer)];
  char nativeTarget[MAX_PATH];
  char *p = nativeTarget;
  REPARSE_JUNCTION_DATA_BUFFER *reparseBuf = (REPARSE_JUNCTION_DATA_BUFFER *)buffer;

  CreateDirectory(newpath, 0);
  dirhandle = CreateFile(newpath, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, 0);

  if (dirhandle == INVALID_HANDLE_VALUE)
  {
    return -1;
  }

                                                       
  if (memcmp("\\??\\", oldpath, 4) != 0)
  {
    snprintf(nativeTarget, sizeof(nativeTarget), "\\??\\%s", oldpath);
  }
  else
  {
    strlcpy(nativeTarget, oldpath, sizeof(nativeTarget));
  }

  while ((p = strchr(p, '/')) != NULL)
  {
    *p++ = '\\';
  }

  len = strlen(nativeTarget) * sizeof(WCHAR);
  reparseBuf->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
  reparseBuf->ReparseDataLength = len + 12;
  reparseBuf->Reserved = 0;
  reparseBuf->SubstituteNameOffset = 0;
  reparseBuf->SubstituteNameLength = len;
  reparseBuf->PrintNameOffset = len + sizeof(WCHAR);
  reparseBuf->PrintNameLength = 0;
  MultiByteToWideChar(CP_ACP, 0, nativeTarget, -1, reparseBuf->PathBuffer, MAX_PATH);

     
                                                                            
                               
     
  if (!DeviceIoControl(dirhandle, CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 41, METHOD_BUFFERED, FILE_ANY_ACCESS), reparseBuf, reparseBuf->ReparseDataLength + REPARSE_JUNCTION_DATA_BUFFER_HEADER_SIZE, 0, 0, &len, 0))
  {
    LPSTR msg;

    errno = 0;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (LPSTR)&msg, 0, NULL);
#ifndef FRONTEND
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not set junction for \"%s\": %s", nativeTarget, msg)));
#else
    fprintf(stderr, _("could not set junction for \"%s\": %s\n"), nativeTarget, msg);
#endif
    LocalFree(msg);

    CloseHandle(dirhandle);
    RemoveDirectory(newpath);
    return -1;
  }

  CloseHandle(dirhandle);

  return 0;
}

   
                                           
   
int
pgreadlink(const char *path, char *buf, size_t size)
{
  DWORD attr;
  HANDLE h;
  char buffer[MAX_PATH * sizeof(WCHAR) + offsetof(REPARSE_JUNCTION_DATA_BUFFER, PathBuffer)];
  REPARSE_JUNCTION_DATA_BUFFER *reparseBuf = (REPARSE_JUNCTION_DATA_BUFFER *)buffer;
  DWORD len;
  int r;

  attr = GetFileAttributes(path);
  if (attr == INVALID_FILE_ATTRIBUTES)
  {
    _dosmaperr(GetLastError());
    return -1;
  }
  if ((attr & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
  {
    errno = EINVAL;
    return -1;
  }

  h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (h == INVALID_HANDLE_VALUE)
  {
    _dosmaperr(GetLastError());
    return -1;
  }

  if (!DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, NULL, 0, (LPVOID)reparseBuf, sizeof(buffer), &len, NULL))
  {
    LPSTR msg;

    errno = 0;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (LPSTR)&msg, 0, NULL);
#ifndef FRONTEND
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not get junction for \"%s\": %s", path, msg)));
#else
    fprintf(stderr, _("could not get junction for \"%s\": %s\n"), path, msg);
#endif
    LocalFree(msg);
    CloseHandle(h);
    errno = EINVAL;
    return -1;
  }
  CloseHandle(h);

                                                
  if (reparseBuf->ReparseTag != IO_REPARSE_TAG_MOUNT_POINT)
  {
    errno = EINVAL;
    return -1;
  }

  r = WideCharToMultiByte(CP_ACP, 0, reparseBuf->PathBuffer, -1, buf, size, NULL, NULL);

  if (r <= 0)
  {
    errno = EINVAL;
    return -1;
  }

     
                                                                            
                      
     
  if (r > 4 && strncmp(buf, "\\??\\", 4) == 0)
  {
    memmove(buf, buf + 4, strlen(buf + 4) + 1);
    r -= 4;
  }
  return r;
}

   
                                                               
                                                
   
bool
pgwin32_is_junction(const char *path)
{
  DWORD attr = GetFileAttributes(path);

  if (attr == INVALID_FILE_ATTRIBUTES)
  {
    _dosmaperr(GetLastError());
    return false;
  }
  return ((attr & FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT);
}
#endif                                             

#if defined(WIN32) && !defined(__CYGWIN__)

#undef stat

   
                                                                        
                                                                        
                         
   
int
pgwin32_safestat(const char *path, struct stat *buf)
{
  int r;
  WIN32_FILE_ATTRIBUTE_DATA attr;

  r = stat(path, buf);
  if (r < 0)
  {
    if (GetLastError() == ERROR_DELETE_PENDING)
    {
         
                                                                         
                                                                         
                                                                       
                                                                     
                        
         
      errno = ENOENT;
      return -1;
    }

    return r;
  }

  if (!GetFileAttributesEx(path, GetFileExInfoStandard, &attr))
  {
    _dosmaperr(GetLastError());
    return -1;
  }

     
                                                                             
                
     
  buf->st_size = attr.nFileSizeLow;

  return 0;
}

#endif
