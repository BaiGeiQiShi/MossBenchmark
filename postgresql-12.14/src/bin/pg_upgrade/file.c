   
          
   
                          
   
                                                                
                             
   

#include "postgres_fe.h"

#include "access/visibilitymap.h"
#include "common/file_perm.h"
#include "pg_upgrade.h"
#include "storage/bufpage.h"
#include "storage/checksum.h"
#include "storage/checksum_impl.h"

#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_COPYFILE_H
#include <copyfile.h>
#endif
#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/fs.h>
#endif

#ifdef WIN32
static int
win32_pghardlink(const char *src, const char *dst);
#endif

   
               
   
                                                    
   
                                                                              
   
void
cloneFile(const char *src, const char *dst, const char *schemaName, const char *relName)
{
#if defined(HAVE_COPYFILE) && defined(COPYFILE_CLONE_FORCE)
  if (copyfile(src, dst, NULL, COPYFILE_CLONE_FORCE) < 0)
  {
    pg_fatal("error while cloning relation \"%s.%s\" (\"%s\" to \"%s\"): %s\n", schemaName, relName, src, dst, strerror(errno));
  }
#elif defined(__linux__) && defined(FICLONE)
  int src_fd;
  int dest_fd;

  if ((src_fd = open(src, O_RDONLY | PG_BINARY, 0)) < 0)
  {
    pg_fatal("error while cloning relation \"%s.%s\": could not open file \"%s\": %s\n", schemaName, relName, src, strerror(errno));
  }

  if ((dest_fd = open(dst, O_RDWR | O_CREAT | O_EXCL | PG_BINARY, pg_file_create_mode)) < 0)
  {
    pg_fatal("error while cloning relation \"%s.%s\": could not create file \"%s\": %s\n", schemaName, relName, dst, strerror(errno));
  }

  if (ioctl(dest_fd, FICLONE, src_fd) < 0)
  {
    int save_errno = errno;

    unlink(dst);
    pg_fatal("error while cloning relation \"%s.%s\" (\"%s\" to \"%s\"): %s\n", schemaName, relName, src, dst, strerror(save_errno));
  }

  close(src_fd);
  close(dest_fd);
#endif
}

   
              
   
                                           
                                                                              
   
void
copyFile(const char *src, const char *dst, const char *schemaName, const char *relName)
{
#ifndef WIN32
  int src_fd;
  int dest_fd;
  char *buffer;

  if ((src_fd = open(src, O_RDONLY | PG_BINARY, 0)) < 0)
  {
    pg_fatal("error while copying relation \"%s.%s\": could not open file \"%s\": %s\n", schemaName, relName, src, strerror(errno));
  }

  if ((dest_fd = open(dst, O_RDWR | O_CREAT | O_EXCL | PG_BINARY, pg_file_create_mode)) < 0)
  {
    pg_fatal("error while copying relation \"%s.%s\": could not create file \"%s\": %s\n", schemaName, relName, dst, strerror(errno));
  }

                                                       
#define COPY_BUF_SIZE (50 * BLCKSZ)

  buffer = (char *)pg_malloc(COPY_BUF_SIZE);

                                                                      
  while (true)
  {
    ssize_t nbytes = read(src_fd, buffer, COPY_BUF_SIZE);

    if (nbytes < 0)
    {
      pg_fatal("error while copying relation \"%s.%s\": could not read file \"%s\": %s\n", schemaName, relName, src, strerror(errno));
    }

    if (nbytes == 0)
    {
      break;
    }

    errno = 0;
    if (write(dest_fd, buffer, nbytes) != nbytes)
    {
                                                                      
      if (errno == 0)
      {
        errno = ENOSPC;
      }
      pg_fatal("error while copying relation \"%s.%s\": could not write file \"%s\": %s\n", schemaName, relName, dst, strerror(errno));
    }
  }

  pg_free(buffer);
  close(src_fd);
  close(dest_fd);

#else            

  if (CopyFile(src, dst, true) == 0)
  {
    _dosmaperr(GetLastError());
    pg_fatal("error while copying relation \"%s.%s\" (\"%s\" to \"%s\"): %s\n", schemaName, relName, src, dst, strerror(errno));
  }

#endif            
}

   
              
   
                                               
                                                                              
   
void
linkFile(const char *src, const char *dst, const char *schemaName, const char *relName)
{
  if (pg_link_file(src, dst) < 0)
  {
    pg_fatal("error while creating link for relation \"%s.%s\" (\"%s\" to \"%s\"): %s\n", schemaName, relName, src, dst, strerror(errno));
  }
}

   
                          
   
                                                             
                                                                              
   
                                                                         
                                                                       
                                                                          
                                                                         
                                                                       
                                                                        
                                                                      
                                                                     
                                                                              
   
void
rewriteVisibilityMap(const char *fromfile, const char *tofile, const char *schemaName, const char *relName)
{
  int src_fd;
  int dst_fd;
  PGAlignedBlock buffer;
  PGAlignedBlock new_vmbuf;
  ssize_t totalBytesRead = 0;
  ssize_t src_filesize;
  int rewriteVmBytesPerPage;
  BlockNumber new_blkno = 0;
  struct stat statbuf;

                                                       
  rewriteVmBytesPerPage = (BLCKSZ - SizeOfPageHeaderData) / 2;

  if ((src_fd = open(fromfile, O_RDONLY | PG_BINARY, 0)) < 0)
  {
    pg_fatal("error while copying relation \"%s.%s\": could not open file \"%s\": %s\n", schemaName, relName, fromfile, strerror(errno));
  }

  if (fstat(src_fd, &statbuf) != 0)
  {
    pg_fatal("error while copying relation \"%s.%s\": could not stat file \"%s\": %s\n", schemaName, relName, fromfile, strerror(errno));
  }

  if ((dst_fd = open(tofile, O_RDWR | O_CREAT | O_EXCL | PG_BINARY, pg_file_create_mode)) < 0)
  {
    pg_fatal("error while copying relation \"%s.%s\": could not create file \"%s\": %s\n", schemaName, relName, tofile, strerror(errno));
  }

                          
  src_filesize = statbuf.st_size;

     
                                                                          
                                                                          
                                                                      
                                                                    
     
  while (totalBytesRead < src_filesize)
  {
    ssize_t bytesRead;
    char *old_cur;
    char *old_break;
    char *old_blkend;
    PageHeaderData pageheader;
    bool old_lastblk;

    if ((bytesRead = read(src_fd, buffer.data, BLCKSZ)) != BLCKSZ)
    {
      if (bytesRead < 0)
      {
        pg_fatal("error while copying relation \"%s.%s\": could not read file \"%s\": %s\n", schemaName, relName, fromfile, strerror(errno));
      }
      else
      {
        pg_fatal("error while copying relation \"%s.%s\": partial page found in file \"%s\"\n", schemaName, relName, fromfile);
      }
    }

    totalBytesRead += BLCKSZ;
    old_lastblk = (totalBytesRead == src_filesize);

                                   
    memcpy(&pageheader, buffer.data, SizeOfPageHeaderData);

       
                                                                       
                                                                           
                                                                           
                                                              
       
    old_cur = buffer.data + SizeOfPageHeaderData;
    old_blkend = buffer.data + bytesRead;
    old_break = old_cur + rewriteVmBytesPerPage;

    while (old_break <= old_blkend)
    {
      char *new_cur;
      bool empty = true;
      bool old_lastpart;

                                                   
      memcpy(new_vmbuf.data, &pageheader, SizeOfPageHeaderData);

                                                         
      old_lastpart = old_lastblk && (old_break == old_blkend);

      new_cur = new_vmbuf.data + SizeOfPageHeaderData;

                                                                         
      while (old_cur < old_break)
      {
        uint8 byte = *(uint8 *)old_cur;
        uint16 new_vmbits = 0;
        int i;

                                                                    
        for (i = 0; i < BITS_PER_BYTE; i++)
        {
          if (byte & (1 << i))
          {
            empty = false;
            new_vmbits |= VISIBILITYMAP_ALL_VISIBLE << (BITS_PER_HEAPBLOCK * i);
          }
        }

                                                              
        new_cur[0] = (char)(new_vmbits & 0xFF);
        new_cur[1] = (char)(new_vmbits >> 8);

        old_cur++;
        new_cur += BITS_PER_HEAPBLOCK;
      }

                                                                       
      if (old_lastpart && empty)
      {
        break;
      }

                                                                
      if (new_cluster.controldata.data_checksum_version != 0)
      {
        ((PageHeader)new_vmbuf.data)->pd_checksum = pg_checksum_page(new_vmbuf.data, new_blkno);
      }

      errno = 0;
      if (write(dst_fd, new_vmbuf.data, BLCKSZ) != BLCKSZ)
      {
                                                                        
        if (errno == 0)
        {
          errno = ENOSPC;
        }
        pg_fatal("error while copying relation \"%s.%s\": could not write file \"%s\": %s\n", schemaName, relName, tofile, strerror(errno));
      }

                                     
      old_break += rewriteVmBytesPerPage;
      new_blkno++;
    }
  }

                
  close(dst_fd);
  close(src_fd);
}

void
check_file_clone(void)
{
  char existing_file[MAXPGPATH];
  char new_link_file[MAXPGPATH];

  snprintf(existing_file, sizeof(existing_file), "%s/PG_VERSION", old_cluster.pgdata);
  snprintf(new_link_file, sizeof(new_link_file), "%s/PG_VERSION.clonetest", new_cluster.pgdata);
  unlink(new_link_file);                 

#if defined(HAVE_COPYFILE) && defined(COPYFILE_CLONE_FORCE)
  if (copyfile(existing_file, new_link_file, NULL, COPYFILE_CLONE_FORCE) < 0)
  {
    pg_fatal("could not clone file between old and new data directories: %s\n", strerror(errno));
  }
#elif defined(__linux__) && defined(FICLONE)
  {
    int src_fd;
    int dest_fd;

    if ((src_fd = open(existing_file, O_RDONLY | PG_BINARY, 0)) < 0)
    {
      pg_fatal("could not open file \"%s\": %s\n", existing_file, strerror(errno));
    }

    if ((dest_fd = open(new_link_file, O_RDWR | O_CREAT | O_EXCL | PG_BINARY, pg_file_create_mode)) < 0)
    {
      pg_fatal("could not create file \"%s\": %s\n", new_link_file, strerror(errno));
    }

    if (ioctl(dest_fd, FICLONE, src_fd) < 0)
    {
      pg_fatal("could not clone file between old and new data directories: %s\n", strerror(errno));
    }

    close(src_fd);
    close(dest_fd);
  }
#else
  pg_fatal("file cloning not supported on this platform\n");
#endif

  unlink(new_link_file);
}

void
check_hard_link(void)
{
  char existing_file[MAXPGPATH];
  char new_link_file[MAXPGPATH];

  snprintf(existing_file, sizeof(existing_file), "%s/PG_VERSION", old_cluster.pgdata);
  snprintf(new_link_file, sizeof(new_link_file), "%s/PG_VERSION.linktest", new_cluster.pgdata);
  unlink(new_link_file);                 

  if (pg_link_file(existing_file, new_link_file) < 0)
  {
    pg_fatal("could not create hard link between old and new data directories: %s\n"
             "In link mode the old and new data directories must be on the same file system.\n",
        strerror(errno));
  }

  unlink(new_link_file);
}

#ifdef WIN32
                                                 
static int
win32_pghardlink(const char *src, const char *dst)
{
     
                                              
                                                                  
     
  if (CreateHardLinkA(dst, src, NULL) == 0)
  {
    _dosmaperr(GetLastError());
    return -1;
  }
  else
  {
    return 0;
  }
}
#endif
