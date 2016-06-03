/*
 * sym_script.c
 *
 * Function to execute a shell script.
 */

#include "private.h"
#include "lub/string.h"
#include "konf/buf.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

/*--------------------------------------------------------- */
CLISH_PLUGIN_SYM(clish_script)
{
	clish_shell_t *this = clish_context__get_shell(clish_context);
	const clish_action_t *action = clish_context__get_action(clish_context);
	const char *shebang = NULL;
	pid_t cpid = -1;
	int res;
	char fifo_name[PATH_MAX];
	FILE *rpipe, *wpipe;
	char *command = NULL;

	/* Signal vars */
	struct sigaction sig_old_int;
	struct sigaction sig_old_quit;
	struct sigaction sig_new;
	sigset_t sig_set;

	assert(this);
	if (!script) /* Nothing to do */
		return 0;

	/* Find out shebang */
	if (action)
		shebang = clish_action__get_shebang(action);
	if (!shebang)
		shebang = clish_shell__get_default_shebang(this);
	assert(shebang);

#ifdef DEBUG
	fprintf(stderr, "SHEBANG: #!%s\n", shebang);
	fprintf(stderr, "SCRIPT: %s\n", script);
#endif /* DEBUG */

	/* Get FIFO */
	if (! clish_shell_mkfifo(this, fifo_name, sizeof(fifo_name))) {
		fprintf(stderr, "Error: Can't create temporary FIFO.\n"
			"Error: The ACTION will be not executed.\n");
		return -1;
	}

	/* Create process to write to FIFO */
	cpid = fork();
	if (cpid == -1) {
		fprintf(stderr, "Error: Can't fork the write process.\n"
			"Error: The ACTION will be not executed.\n");
		clish_shell_rmfifo(this, fifo_name);
		return -1;
	}

	/* Child: write to FIFO */
	if (cpid == 0) {
		wpipe = fopen(fifo_name, "w");
		if (!wpipe)
			_exit(-1);
		fwrite(script, strlen(script), 1, wpipe);
		fclose(wpipe);
		_exit(0);
	}

	/* Parent */
	/* Prepare command */
	lub_string_cat(&command, shebang);
	lub_string_cat(&command, " ");
	lub_string_cat(&command, fifo_name);

	/* If the stdout of script is needed */
	if (out) {
		konf_buf_t *buf;

		/* Ignore SIGINT and SIGQUIT */
		sigemptyset(&sig_set);
		sig_new.sa_flags = 0;
		sig_new.sa_mask = sig_set;
		sig_new.sa_handler = SIG_IGN;
		sigaction(SIGINT, &sig_new, &sig_old_int);
		sigaction(SIGQUIT, &sig_new, &sig_old_quit);

		/* Execute shebang with FIFO as argument */
		rpipe = popen(command, "r");
		if (!rpipe) {
			fprintf(stderr, "Error: Can't fork the script.\n"
				"Error: The ACTION will be not executed.\n");
			kill(cpid, SIGTERM);
			waitpid(cpid, NULL, 0);

			/* Restore SIGINT and SIGQUIT */
			sigaction(SIGINT, &sig_old_int, NULL);
			sigaction(SIGQUIT, &sig_old_quit, NULL);

			lub_string_free(command);
			clish_shell_rmfifo(this, fifo_name);
			return -1;
		}
		/* Read the result of script execution */
		buf = konf_buf_new(fileno(rpipe));
		while (konf_buf_read(buf) > 0);
		*out = konf_buf__dup_line(buf);
		konf_buf_delete(buf);
		/* Wait for the writing process */
		kill(cpid, SIGTERM);
		waitpid(cpid, NULL, 0);
		/* Wait for script */
		res = pclose(rpipe);

		/* Restore SIGINT and SIGQUIT */
		sigaction(SIGINT, &sig_old_int, NULL);
		sigaction(SIGQUIT, &sig_old_quit, NULL);
	} else {
		res = system(command);
		/* Wait for the writing process */
		kill(cpid, SIGTERM);
		waitpid(cpid, NULL, 0);
	}

	/* Clean up */
	lub_string_free(command);
	clish_shell_rmfifo(this, fifo_name);

#ifdef DEBUG
	fprintf(stderr, "RETCODE: %d\n", WEXITSTATUS(res));
#endif /* DEBUG */
	return WEXITSTATUS(res);
}

/*--------------------------------------------------------- */
