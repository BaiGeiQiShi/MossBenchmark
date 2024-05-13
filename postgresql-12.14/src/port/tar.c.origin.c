#include "c.h"
#include "pgtar.h"
#include <sys/stat.h>

/*
 * Print a numeric field in a tar header.  The field starts at *s and is of
 * length len; val is the value to be written.
 *
 * Per POSIX, the way to write a number is in octal with leading zeroes and
 * one trailing space (or NUL, but we use space) at the end of the specified
 * field width.
 *
 * However, the given value may not fit in the available space in octal form.
 * If that's true, we use the GNU extension of writing \200 followed by the
 * number in base-256 form (ie, stored in binary MSB-first).  (Note: here we
 * support only non-negative numbers, so we don't worry about the GNU rules
 * for handling negative numbers.)
 */
void
print_tar_number(char *s, int len, uint64 val)
{




















}

/*
 * Read a numeric field in a tar header.  The field starts at *s and is of
 * length len.
 *
 * The POSIX-approved format for a number is octal, ending with a space or
 * NUL.  However, for values that don't fit, we recognize the GNU extension
 * of \200 followed by the number in base-256 form (ie, stored in binary
 * MSB-first).  (Note: here we support only non-negative numbers, so we don't
 * worry about the GNU rules for handling negative numbers.)
 */
uint64
read_tar_number(const char *s, int len)
{






















}

/*
 * Calculate the tar checksum for a header. The header is assumed to always
 * be 512 bytes, per the tar standard.
 */
int
tarChecksum(char *header)
{















}

/*
 * Fill in the buffer pointed to by h with a tar format header. This buffer
 * must always have space for 512 characters, which is a requirement of
 * the tar format.
 */
enum tarError
tarCreateHeader(char *h, const char *filename, const char *linktarget, pgoff_t size, mode_t mode, uid_t uid, gid_t gid, time_t mtime)
{

































































































}
