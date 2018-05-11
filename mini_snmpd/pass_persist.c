#include "pass_persist.h"
#include "mini_snmpd.h"
#include "popen2.h"
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <unistd.h>

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
	/* Ignore broken pipe signals.
	   The handler will be restarted if reads or writes fail.  */
	signal(SIGPIPE, SIG_IGN);

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

value_t results[16];
value_t *result_it = results;
value_t *result_end = results + sizeof(results) / sizeof(*results);

static value_t *handle_pass_persist(pp_handler* handler, const char* method, const oid_t *oid)
{
	int mib_value_alloc(data_t *data, int type);
	int set_oid_encoded_length(oid_t *oid);
	int mib_set_value(data_t *data, int type, const void *dataval);

	value_t *result = result_it++;
	if (result_it == result_end)
		result_it = results;

	if (result->data.buffer == NULL)
		mib_value_alloc(&result->data, BER_TYPE_INTEGER);

	#define maybe_retry_due_to_pipe_fail do {				\
		if (ferror(handler->to) || ferror(handler->from))	\
			if (errno == EPIPE) {							\
				close(handler->pid); handler->pid = 0;		\
				goto retry;									\
			}												\
	} while (0)

	int retries = 0;

retry:

	if (++retries > 5)
		return NULL;

	if (handler->pid == 0)
		handler->pid = popen2(handler->command, &handler->to, &handler->from);

	// Post request to co-process.
	fprintf(handler->to, "%s\n%s\n", method, oid_ntoa(oid));
	fflush(handler->to); maybe_retry_due_to_pipe_fail;

	// Handle response.
	char buf[2048];

	// OID, "NONE" or 'error-string':
	fgets(buf, 2048, handler->from); maybe_retry_due_to_pipe_fail;
	if (buf[0] != '.')
	{
		if (strncmp("NONE", buf, 4) == 0)
			return NULL;
		// TODO: handle error strings
		return NULL;
	}
	memcpy(&result->oid, oid_aton(buf), sizeof result->oid);
	set_oid_encoded_length(&result->oid);

	// Type:
	int type = BER_TYPE_NO_SUCH_OBJECT;
	fgets(buf, 2048, handler->from); maybe_retry_due_to_pipe_fail;
	if (strncmp("string", buf, 6) == 0)
		type = BER_TYPE_OCTET_STRING;
	else if (strncmp("integer", buf, 7) == 0)
		type = BER_TYPE_INTEGER;
	else if (strncmp("unsigned", buf, 8) == 0 || strncmp("gauge", buf, 5) == 0)
		type = BER_TYPE_GAUGE;
	else if (strncmp("counter", buf, 7) == 0)
		type = BER_TYPE_COUNTER;

	// Value:
	fgets(buf, 2048, handler->from); maybe_retry_due_to_pipe_fail;
	switch (type)
	{
	case BER_TYPE_OCTET_STRING:
		strtok(buf, "\n");
		mib_set_value(&result->data, type, buf);
		break;

	case BER_TYPE_INTEGER:
	case BER_TYPE_COUNTER:
	case BER_TYPE_GAUGE:
		mib_set_value(&result->data, type, (void*)atoi(buf));
		break;

	default:
		return NULL;
	}

	return result;
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

		return handle_pass_persist(p, method, oid);
	}
	return NULL;
}

/* vim: ts=4 sts=4 sw=4 nowrap noexpandtab
 */
