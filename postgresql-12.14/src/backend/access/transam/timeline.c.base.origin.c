/*-------------------------------------------------------------------------
 *
 * timeline.c
 *		Functions for reading and writing timeline history files.
 *
 * A timeline history file lists the timeline changes of the timeline, in
 * a simple text format. They are archived along with the WAL segments.
 *
 * The files are named like "<tli>.history". For example, if the database
 * starts up and switches to timeline 5, the timeline history file would be
 * called "00000005.history".
 *
 * Each line in the file represents a timeline switch:
 *
 * <parentTLI> <switchpoint> <reason>
 *
 *	parentTLI	ID of the parent timeline
 *	switchpoint XLogRecPtr of the WAL location where the switch happened
 *	reason		human-readable explanation of why the timeline was
 *changed
 *
 * The fields are separated by tabs. Lines beginning with # are comments, and
 * are ignored. Empty lines are also ignored.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/access/transam/timeline.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>

#include "access/timeline.h"
#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "access/xlogdefs.h"
#include "pgstat.h"
#include "storage/fd.h"

/*
 * Copies all timeline history files with id's between 'begin' and 'end'
 * from archive to pg_wal.
 */
void
restoreTimeLineHistoryFiles(TimeLineID begin, TimeLineID end)
{

















}

/*
 * Try to read a timeline's history file.
 *
 * If successful, return the list of component TLIs (the given TLI followed by
 * its ancestor TLIs).  If we can't find the history file, assume that the
 * timeline has no parents, and return a list of just the specified timeline
 * ID.
 */
List *
readTimeLineHistory(TimeLineID targetTLI)
{















































































































































}

/*
 * Probe whether a timeline history file exists for the given timeline ID
 */
bool
existsTimeLineHistory(TimeLineID probeTLI)
{


































}

/*
 * Find the newest existing timeline, assuming that startTLI exists.
 *
 * Note: while this is somewhat heuristic, it does positively guarantee
 * that (result + 1) is not a known timeline, and therefore it should
 * be safe to assign that ID to a new timeline.
 */
TimeLineID
findNewestTimeLine(TimeLineID startTLI)
{























}

/*
 * Create a new timeline history file.
 *
 *	newTLI: ID of the new timeline
 *	parentTLI: ID of its immediate parent
 *	switchpoint: WAL location where the system switched to the new timeline
 *	reason: human-readable explanation of why the timeline was switched
 *
 * Currently this is only used at the end recovery, and so there are no locking
 * considerations.  But we should be just as tense as XLogFileInit to avoid
 * emplacing a bogus file.
 */
void
writeTimeLineHistory(TimeLineID newTLI, TimeLineID parentTLI, XLogRecPtr switchpoint, char *reason)
{

















































































































































}

/*
 * Writes a history file for given timeline and contents.
 *
 * Currently this is only used in the walreceiver process, and so there are
 * no locking considerations.  But we should be just as tense as XLogFileInit
 * to avoid emplacing a bogus file.
 */
void
writeTimeLineHistoryFile(TimeLineID tli, char *content, int size)
{

























































}

/*
 * Returns true if 'expectedTLEs' contains a timeline with id 'tli'
 */
bool
tliInHistory(TimeLineID tli, List *expectedTLEs)
{











}

/*
 * Returns the ID of the timeline in use at a particular point in time, in
 * the given timeline history.
 */
TimeLineID
tliOfPointInHistory(XLogRecPtr ptr, List *history)
{
















}

/*
 * Returns the point in history where we branched off the given timeline,
 * and the timeline we branched to (*nextTLI). Returns InvalidXLogRecPtr if
 * the timeline is current, ie. we have not branched off from it, and throws
 * an error if the timeline is not part of this server's history.
 */
XLogRecPtr
tliSwitchPoint(TimeLineID tli, List *history, TimeLineID *nextTLI)
{






















}