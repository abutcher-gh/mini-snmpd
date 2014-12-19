#include "popen2.h"
#include <stdlib.h>
#include <unistd.h>

pid_t popen2(const char* command, FILE** in, FILE** out)
{
	int p_stdin[2], p_stdout[2];
	pid_t pid;

	if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
		return -1;

	pid = fork();

	if (pid < 0)
		return pid;

	if (pid == 0)
	{
		dup2(p_stdin[0], STDIN_FILENO);
		dup2(p_stdout[1], STDOUT_FILENO);

		//close unused descriptors on child process.
		close(p_stdin[0]);
		close(p_stdin[1]);
		close(p_stdout[0]);
		close(p_stdout[1]);

		execl("/bin/sh", "sh", "-c", command, NULL);
		exit(1);
	}

	// close unused descriptors on parent process.
	close(p_stdin[0]);
	close(p_stdout[1]);

	if (in)
		*in = fdopen(p_stdin[1], "w");
	else
		close(p_stdin[1]);

	if (out)
		*out = fdopen(p_stdout[0], "r");
	else
		close(p_stdout[0]);

	return pid;
}

/* vim: ts=4 sts=4 sw=4 nowrap
 */
