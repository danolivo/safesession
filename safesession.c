/*-------------------------------------------------------------------------
 *
 * safesession is a PostgreSQL extension which allows to set a session
 * read only: no INSERT,UPDATE,DELETE and no DDL can be run.
 *
 * Activated by LOAD 'safesession' — from that point on, every
 * transaction in the session is forced into read-only mode.
 *
 * This program is open source, licensed under the PostgreSQL license.
 * For license terms, see the LICENSE file.
 *
 * Copyright (c) 2020, Pierre Forstmann.
 *
 *-------------------------------------------------------------------------
*/
#include "postgres.h"
#include "access/xact.h"
#include "executor/executor.h"
#include "tcop/utility.h"
#include "utils/guc.h"

PG_MODULE_MAGIC;

/* Saved hook values in case of unload */
static ExecutorStart_hook_type prev_executor_start_hook = NULL;
static ProcessUtility_hook_type prev_process_utility_hook = NULL;

/*---- Function declarations ----*/

void		_PG_init(void);

static void ss_exec(QueryDesc *queryDesc, int eflags);
static void ss_utility(PlannedStmt *pstmt, const char *queryString,
					   bool readOnlyTree,
					   ProcessUtilityContext context,
					   ParamListInfo params,
					   QueryEnvironment *queryEnv,
					   DestReceiver *dest, QueryCompletion *qc);

/*
 * Module load callback.
 */
void
_PG_init(void)
{
	prev_executor_start_hook = ExecutorStart_hook;
	ExecutorStart_hook = ss_exec;
	prev_process_utility_hook = ProcessUtility_hook;
	ProcessUtility_hook = ss_utility;
}

/*
 * Set transaction_read_only for the current transaction via the GUC
 * machinery.  Using GUC_ACTION_LOCAL means the value is automatically
 * reverted at transaction end — no manual restore is needed.
 *
 * Once set, the user cannot revert to read-write mode within the same
 * transaction: check_transaction_read_only() in variable.c rejects the
 * read-only -> read-write transition after the first snapshot is taken.
 */
static void
ss_set_xact_readonly(void)
{
	if (XactReadOnly)
		return;

	set_config_option("transaction_read_only", "on",
					  PGC_USERSET, PGC_S_SESSION,
					  GUC_ACTION_LOCAL, true, 0, false);
}

/*
 * ExecutorStart hook.
 *
 * Set transaction_read_only = on via GUC machinery.  The downstream
 * standard_ExecutorStart will then call ExecCheckXactReadOnly(), which
 * enforces all the read-only checks: DML blocking, temp table exemptions,
 * modifying CTE detection, etc.
 */
static void
ss_exec(QueryDesc *queryDesc, int eflags)
{
	ss_set_xact_readonly();

	if (prev_executor_start_hook)
		(*prev_executor_start_hook)(queryDesc, eflags);
	else
		standard_ExecutorStart(queryDesc, eflags);
}

/*
 * ProcessUtility hook.
 *
 * Set transaction_read_only = on via GUC machinery.  The downstream
 * standard_ProcessUtility uses ClassifyUtilityCommandAsReadOnly() +
 * PreventCommandIfReadOnly() to block DDL and other write commands.
 */
static void
ss_utility(PlannedStmt *pstmt, const char *queryString,
		   bool readOnlyTree,
		   ProcessUtilityContext context,
		   ParamListInfo params,
		   QueryEnvironment *queryEnv,
		   DestReceiver *dest, QueryCompletion *qc)
{
	ss_set_xact_readonly();

	if (prev_process_utility_hook)
		(*prev_process_utility_hook)(pstmt, queryString, readOnlyTree,
									 context, params, queryEnv,
									 dest, qc);
	else
		standard_ProcessUtility(pstmt, queryString, readOnlyTree,
								context, params, queryEnv,
								dest, qc);
}
