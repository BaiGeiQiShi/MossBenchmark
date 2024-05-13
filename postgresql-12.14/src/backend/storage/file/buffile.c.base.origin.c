/*-------------------------------------------------------------------------
 *
 * buffile.c
 *	  Management of large buffered temporary files.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/storage/file/buffile.c
 *
 * NOTES:
 *
 * BufFiles provide a very incomplete emulation of stdio atop virtual Files
 * (as managed by fd.c).  Currently, we only support the buffered-I/O
 * aspect of stdio: a read or write of the low-level File occurs only
 * when the buffer is filled or emptied.  This is an even bigger win
 * for virtual Files than for ordinary kernel files, since reducing the
 * frequency with which a virtual File is touched reduces "thrashing"
 * of opening/closing file descriptors.
 *
 * Note that BufFile structs are allocated with palloc(), and therefore
 * will go away automatically at query/transaction end.  Since the underlying
 * virtual Files are made with OpenTemporaryFile, all resources for
 * the file are certain to be cleaned up even if processing is aborted
 * by ereport(ERROR).  The data structures required are made in the
 * palloc context that was current when the BufFile was created, and
 * any external resources such as temp files are owned by the ResourceOwner
 * that was current at that time.
 *
 * BufFile also supports temporary files that exceed the OS file size limit
 * (by opening multiple fd.c temporary files).  This is an essential feature
 * for sorts and hashjoins on large amounts of data.
 *
 * BufFile supports temporary files that can be made read-only and shared with
 * other backends, as infrastructure for parallel execution.  Such files need
 * to be created as a member of a SharedFileSet that all participants are
 * attached to.
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "commands/tablespace.h"
#include "executor/instrument.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/fd.h"
#include "storage/buffile.h"
#include "storage/buf_internals.h"
#include "utils/resowner.h"

/*
 * We break BufFiles into gigabyte-sized segments, regardless of RELSEG_SIZE.
 * The reason is that we'd like large BufFiles to be spread across multiple
 * tablespaces when available.
 */
#define MAX_PHYSICAL_FILESIZE 0x40000000
#define BUFFILE_SEG_SIZE (MAX_PHYSICAL_FILESIZE / BLCKSZ)

/*
 * This data structure represents a buffered file that consists of one or
 * more physical files (each accessed through a virtual file descriptor
 * managed by fd.c).
 */
struct BufFile
{
  int numFiles; /* number of physical files in set */
  /* all files except the last have length exactly MAX_PHYSICAL_FILESIZE */
  File *files; /* palloc'd array with numFiles entries */

  bool isInterXact; /* keep open over transactions? */
  bool dirty;       /* does buffer need to be written? */
  bool readOnly;    /* has the file been set to read only? */

  SharedFileSet *fileset; /* space for segment files if shared */
  const char *name;       /* name of this BufFile if shared */

  /*
   * resowner is the ResourceOwner to use for underlying temp files.  (We
   * don't need to remember the memory context we're using explicitly,
   * because after creation we only repalloc our arrays larger.)
   */
  ResourceOwner resowner;

  /*
   * "current pos" is position of start of buffer within the logical file.
   * Position as seen by user of BufFile is (curFile, curOffset + pos).
   */
  int curFile;     /* file index (0..n) part of current pos */
  off_t curOffset; /* offset part of current pos */
  int pos;         /* next read/write position in buffer */
  int nbytes;      /* total # of valid bytes in buffer */
  PGAlignedBlock buffer;
};

static BufFile *
makeBufFileCommon(int nfiles);
static BufFile *
makeBufFile(File firstfile);
static void
extendBufFile(BufFile *file);
static void
BufFileLoadBuffer(BufFile *file);
static void
BufFileDumpBuffer(BufFile *file);
static void
BufFileFlush(BufFile *file);
static File
MakeNewSharedSegment(BufFile *file, int segment);

/*
 * Create BufFile and perform the common initialization.
 */
static BufFile *
makeBufFileCommon(int nfiles)
{












}

/*
 * Create a BufFile given the first underlying physical file.
 * NOTE: caller must set isInterXact if appropriate.
 */
static BufFile *
makeBufFile(File firstfile)
{









}

/*
 * Add another component temp file.
 */
static void
extendBufFile(BufFile *file)
{























}

/*
 * Create a BufFile for a new temporary file (which will expand to become
 * multiple temporary files if more than MAX_PHYSICAL_FILESIZE bytes are
 * written to it).
 *
 * If interXact is true, the temp file will not be automatically deleted
 * at end of transaction.
 *
 * Note: if interXact is true, the caller had better be calling us in a
 * memory context, and with a resource owner, that will survive across
 * transaction boundaries.
 */
BufFile *
BufFileCreateTemp(bool interXact)
{





















}

/*
 * Build the name for a given segment of a given BufFile.
 */
static void
SharedSegmentName(char *name, const char *buffile_name, int segment)
{

}

/*
 * Create a new segment file backing a shared BufFile.
 */
static File
MakeNewSharedSegment(BufFile *buffile, int segment)
{




















}

/*
 * Create a BufFile that can be discovered and opened read-only by other
 * backends that are attached to the same SharedFileSet using the same name.
 *
 * The naming scheme for shared BufFiles is left up to the calling code.  The
 * name will appear as part of one or more filenames on disk, and might
 * provide clues to administrators about which subsystem is generating
 * temporary file data.  Since each SharedFileSet object is backed by one or
 * more uniquely named temporary directory, names don't conflict with
 * unrelated SharedFileSet objects.
 */
BufFile *
BufFileCreateShared(SharedFileSet *fileset, const char *name)
{










}

/*
 * Open a file that was previously created in another backend (or this one)
 * with BufFileCreateShared in the same SharedFileSet using the same name.
 * The backend that created the file must have called BufFileClose() or
 * BufFileExportShared() to make sure that it is ready to be opened by other
 * backends and render it read-only.
 */
BufFile *
BufFileOpenShared(SharedFileSet *fileset, const char *name)
{
















































}

/*
 * Delete a BufFile that was created by BufFileCreateShared in the given
 * SharedFileSet using the given name.
 *
 * It is not necessary to delete files explicitly with this function.  It is
 * provided only as a way to delete files proactively, rather than waiting for
 * the SharedFileSet to be cleaned up.
 *
 * Only one backend should attempt to delete a given name, and should know
 * that it exists and has been exported or closed.
 */
void
BufFileDeleteShared(SharedFileSet *fileset, const char *name)
{


























}

/*
 * BufFileExportShared --- flush and make read-only, in preparation for sharing.
 */
void
BufFileExportShared(BufFile *file)
{








}

/*
 * Close a BufFile
 *
 * Like fclose(), this also implicitly FileCloses the underlying File.
 */
void
BufFileClose(BufFile *file)
{












}

/*
 * BufFileLoadBuffer
 *
 * Load some data into buffer, if possible, starting from curOffset.
 * At call, must have dirty = false, pos and nbytes = 0.
 * On exit, nbytes is number of bytes loaded.
 */
static void
BufFileLoadBuffer(BufFile *file)
{




























}

/*
 * BufFileDumpBuffer
 *
 * Dump buffer contents starting at curOffset.
 * At call, should have dirty = true, nbytes > 0.
 * On exit, dirty is cleared if successful write, and curOffset is advanced.
 */
static void
BufFileDumpBuffer(BufFile *file)
{




































































}

/*
 * BufFileRead
 *
 * Like fread() except we assume 1-byte element size and report I/O errors via
 * ereport().
 */
size_t
BufFileRead(BufFile *file, void *ptr, size_t size)
{




































}

/*
 * BufFileWrite
 *
 * Like fwrite() except we assume 1-byte element size and report errors via
 * ereport().
 */
size_t
BufFileWrite(BufFile *file, void *ptr, size_t size)
{












































}

/*
 * BufFileFlush
 *
 * Like fflush(), except that I/O errors are reported with ereport().
 */
static void
BufFileFlush(BufFile *file)
{






}

/*
 * BufFileSeek
 *
 * Like fseek(), except that target position needs two values in order to
 * work when logical filesize exceeds maximum value representable by off_t.
 * We do not support relative seeks across more than that, however.
 * I/O errors are reported by ereport().
 *
 * Result is 0 if OK, EOF if not.  Logical position is not moved if an
 * impossible seek is attempted.
 */
int
BufFileSeek(BufFile *file, int fileno, off_t offset, int whence)
{




















































































}

void
BufFileTell(BufFile *file, int *fileno, off_t *offset)
{


}

/*
 * BufFileSeekBlock --- block-oriented seek
 *
 * Performs absolute seek to the start of the n'th BLCKSZ-sized block of
 * the file.  Note that users of this interface will fail if their files
 * exceed BLCKSZ * LONG_MAX bytes, but that is quite a lot; we don't work
 * with tables bigger than that, either...
 *
 * Result is 0 if OK, EOF if not.  Logical position is not moved if an
 * impossible seek is attempted.
 */
int
BufFileSeekBlock(BufFile *file, long blknum)
{

}

#ifdef NOT_USED
/*
 * BufFileTellBlock --- block-oriented tell
 *
 * Any fractional part of a block in the current seek position is ignored.
 */
long
BufFileTellBlock(BufFile *file)
{
  long blknum;

  blknum = (file->curOffset + file->pos) / BLCKSZ;
  blknum += file->curFile * BUFFILE_SEG_SIZE;
  return blknum;
}

#endif

/*
 * Return the current shared BufFile size.
 *
 * Counts any holes left behind by BufFileAppend as part of the size.
 * ereport()s on failure.
 */
int64
BufFileSize(BufFile *file)
{












}

/*
 * Append the contents of source file (managed within shared fileset) to
 * end of target file (managed within same shared fileset).
 *
 * Note that operation subsumes ownership of underlying resources from
 * "source".  Caller should never call BufFileClose against source having
 * called here first.  Resource owners for source and target must match,
 * too.
 *
 * This operation works by manipulating lists of segment files, so the
 * file content is always appended at a MAX_PHYSICAL_FILESIZE-aligned
 * boundary, typically creating empty holes before the boundary.  These
 * areas do not contain any interesting data, and cannot be read from by
 * caller.
 *
 * Returns the block number within target where the contents of source
 * begins.  Caller should apply this as an offset when working off block
 * positions that are in terms of the original BufFile space.
 */
long
BufFileAppend(BufFile *target, BufFile *source)
{






















}