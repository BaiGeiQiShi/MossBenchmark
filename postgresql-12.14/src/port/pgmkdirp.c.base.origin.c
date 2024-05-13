/*
 * This is adapted from FreeBSD's src/bin/mkdir/mkdir.c, which bears
 * the following copyright notice:
 *
 * Copyright (c) 1983, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *	  may be used to endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "c.h"

#include <sys/stat.h>

/*
 * pg_mkdir_p --- create a directory and, if necessary, parent directories
 *
 * This is equivalent to "mkdir -p" except we don't complain if the target
 * directory already exists.
 *
 * We assume the path is in canonical form, i.e., uses / as the separator.
 *
 * omode is the file permissions bits for the target directory.  Note that any
 * parent directories that have to be created get permissions according to the
 * prevailing umask, but with u+wx forced on to ensure we can create there.
 * (We declare omode as int, not mode_t, to minimize dependencies for port.h.)
 *
 * Returns 0 on success, -1 (with errno set) on failure.
 *
 * Note that on failure, the path arg has been modified to show the particular
 * directory level we had problems with.
 */
int
pg_mkdir_p(char *path, int omode)
{





































































































}