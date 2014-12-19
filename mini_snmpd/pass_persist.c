#include "pass_persist.h"
#include "mini_snmpd.h"
#include "popen2.h"
#include <signal.h>

void install_pass_persist_handler(const char* oid, const char* command)
{
}

value_t *maybe_handle_pass_persist(const char* method, const oid_t *oid)
{
	return NULL;
}

/* vim: ts=4 sts=4 sw=4 nowrap noexpandtab
 */
