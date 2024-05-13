/* -------------------------------------------------------------------------
 *
 * seclabel.c
 *	  routines to support security label feature.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * -------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/relation.h"
#include "access/table.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/pg_seclabel.h"
#include "catalog/pg_shseclabel.h"
#include "commands/seclabel.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/rel.h"

typedef struct
{
  const char *provider_name;
  check_object_relabel_type hook;
} LabelProvider;

static List *label_provider_list = NIL;

/*
 * ExecSecLabelStmt --
 *
 * Apply a security label to a database object.
 *
 * Returns the ObjectAddress of the object to which the policy was applied.
 */
ObjectAddress
ExecSecLabelStmt(SecLabelStmt *stmt)
{























































































}

/*
 * GetSharedSecurityLabel returns the security label for a shared object for
 * a given provider, or NULL if there is no such label.
 */
static char *
GetSharedSecurityLabel(const ObjectAddress *object, const char *provider)
{






























}

/*
 * GetSecurityLabel returns the security label for a shared or database object
 * for a given provider, or NULL if there is no such label.
 */
char *
GetSecurityLabel(const ObjectAddress *object, const char *provider)
{






































}

/*
 * SetSharedSecurityLabel is a helper function of SetSecurityLabel to
 * handle shared database objects.
 */
static void
SetSharedSecurityLabel(const ObjectAddress *object, const char *provider, const char *label)
{


























































}

/*
 * SetSecurityLabel attempts to set the security label for the specified
 * provider on the specified object to the given value.  NULL means that any
 * existing label should be deleted.
 */
void
SetSecurityLabel(const ObjectAddress *object, const char *provider, const char *label)
{




































































}

/*
 * DeleteSharedSecurityLabel is a helper function of DeleteSecurityLabel
 * to handle shared database objects.
 */
void
DeleteSharedSecurityLabel(Oid objectId, Oid classId)
{


















}

/*
 * DeleteSecurityLabel removes all security labels for an object (and any
 * sub-objects, if applicable).
 */
void
DeleteSecurityLabel(const ObjectAddress *object)
{




































}

void
register_label_provider(const char *provider_name, check_object_relabel_type hook)
{









}