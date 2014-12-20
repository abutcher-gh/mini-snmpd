#include "pass_persist.h"
#include "mini_snmpd.h"
#include "popen2.h"
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>

#define MAX_NR_HANDLERS 8

typedef struct pp_handler
{
	struct pp_handler* next;

	oid_t prefix;
	const char* command;

	int pid;
	FILE* to;
	FILE* from;
}
pp_handler;
static pp_handler pass_persist_handlers[1];

void install_pass_persist_handler(const char* oid_prefix, const char* command)
{
	pp_handler* p;
	for (p = pass_persist_handlers; p && p->command; p = p->next)
	{
		if (p->next == NULL)
		{
			p->next = (pp_handler*) calloc(1, sizeof(pp_handler));
			p = p->next;
			break;
		}
	}
	p->command = strdup(command);
	memcpy(&p->prefix, oid_aton(oid_prefix), sizeof p->prefix);
}

value_t *maybe_handle_pass_persist(const char* method, const oid_t *oid)
{
	pp_handler* p;
	for (p = pass_persist_handlers; p && p->command; p = p->next)
	{
		// Check tail-to-head for prefix OID; this eliminates many
		// loops over the same ordinals.
		int prefix_len = p->prefix.subid_list_length;
		if (prefix_len > oid->subid_list_length)
			continue;
		while (prefix_len-- > 0)
			if (oid->subid_list[prefix_len] != p->prefix.subid_list[prefix_len])
				break;
		if (prefix_len >= 0)
			continue;

		char* s = strdup(oid_ntoa(oid));
		fprintf(stderr, "%s matches PP handler %s...\n", s, oid_ntoa(&p->prefix));
		free(s);
	}
	return NULL;
}

/* vim: ts=4 sts=4 sw=4 nowrap noexpandtab
 */
