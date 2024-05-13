/*-------------------------------------------------------------------------
 * help_config.c
 *
 * Displays available options under grand unified configuration scheme
 *
 * Options whose flag bits are set to GUC_NO_SHOW_ALL, GUC_NOT_IN_SAMPLE,
 * or GUC_DISALLOW_IN_FILE are not displayed, unless the user specifically
 * requests that variable by name
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/utils/misc/help_config.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>
#include <unistd.h>

#include "utils/guc_tables.h"
#include "utils/help_config.h"

/*
 * This union allows us to mix the numerous different types of structs
 * that we are organizing.
 */
typedef union
{
  struct config_generic generic;
  struct config_bool _bool;
  struct config_real real;
  struct config_int integer;
  struct config_string string;
  struct config_enum _enum;
} mixedStruct;

static void
printMixedStruct(mixedStruct *structToPrint);
static bool
displayStruct(mixedStruct *structToDisplay);

void
GucInfoMain(void)
{




















}

/*
 * This function will return true if the struct passed to it
 * should be displayed to the user.
 */
static bool
displayStruct(mixedStruct *structToDisplay)
{

}

/*
 * This function prints out the generic struct passed to it. It will print out
 * a different format, depending on what the user wants to see.
 */
static void
printMixedStruct(mixedStruct *structToPrint)
{































}