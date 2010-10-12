/*
 * konf.c
 *
 *
 * The client to communicate to konfd configuration daemon.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "konf/net.h"
#include "konf/query.h"
#include "konf/buf.h"
#include "lub/string.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif
#define MAXMSG 1024

#ifndef VERSION
#define VERSION 1.2.2
#endif
#define QUOTE(t) #t
#define version(v) printf("%s\n", QUOTE(v))

static void help(int status, const char *argv0);

static const char *escape_chars = "\"\\'";

/*--------------------------------------------------------- */
int main(int argc, char **argv)
{
	int res = -1;
	konf_client_t *client = NULL;
	konf_buf_t *buf = NULL;
	char *line = NULL;
	char *str = NULL;
	const char *socket_path = KONFD_SOCKET_PATH;
	unsigned i = 0;

	static const char *shortopts = "hvs:";
/*	static const struct option longopts[] = {
		{"help",	0, NULL, 'h'},
		{"version",	0, NULL, 'v'},
		{"socket",	1, NULL, 's'},
		{NULL,		0, NULL, 0}
	};
*/
	/* Parse command line options */
	optind = 0;
	while(1) {
		int opt;
/*		opt = getopt_long(argc, argv, shortopts, longopts, NULL); */
		opt = getopt(argc, argv, shortopts);
		if (-1 == opt)
			break;
		switch (opt) {
		case 's':
			socket_path = optarg;
			break;
		case 'h':
			help(0, argv[0]);
			exit(0);
			break;
		case 'v':
			version(VERSION);
			exit(0);
			break;
		default:
			help(-1, argv[0]);
			exit(-1);
			break;
		}
	}

	/* Get request line from the args */
	for (i = optind; i < argc; i++) {
		char *space = NULL;
		if (NULL != line)
			lub_string_cat(&line, " ");
		space = strchr(argv[i], ' ');
		if (space)
			lub_string_cat(&line, "\"");
		str = lub_string_encode(argv[i], escape_chars);
		lub_string_cat(&line, str);
		lub_string_free(str);
		if (space)
			lub_string_cat(&line, "\"");
	}
	if (!line) {
		help(-1, argv[0]);
		goto err;
	}
#ifdef DEBUG
	printf("REQUEST: %s\n", line);
#endif

	if (!(client = konf_client_new(socket_path))) {
		fprintf(stderr, "Can't create internal data structures.\n");
		goto err;
	}

	if (konf_client_connect(client) < 0) {
		fprintf(stderr, "Can't connect to %s socket.\n", socket_path);
		goto err;
	}

	if (konf_client_send(client, line) < 0) {
		fprintf(stderr, "Can't connect to %s socket.\n", socket_path);
		goto err;
	}

	if (konf_client_recv_answer(client, &buf) < 0) {
		fprintf(stderr, "The error while request to the konfd daemon.\n");
	}

	if (buf) {
		konf_buf_lseek(buf, 0);
		while ((str = konf_buf_preparse(buf))) {
			if (strlen(str) == 0) {
				lub_string_free(str);
				break;
			}
			fprintf(stdout, "%s\n", str);
			lub_string_free(str);
		}
		konf_buf_delete(buf);
	}

	res = 0;
err:
	lub_string_free(line);
	konf_client_free(client);

	return res;
}

/*--------------------------------------------------------- */
/* Print help message */
static void help(int status, const char *argv0)
{
	const char *name = NULL;

	if (!argv0)
		return;

	/* Find the basename */
	name = strrchr(argv0, '/');
	if (name)
		name++;
	else
		name = argv0;

	if (status != 0) {
		fprintf(stderr, "Try `%s -h' for more information.\n",
			name);
	} else {
		printf("Usage: %s [options] -- <command for konfd daemon>\n", name);
		printf("Utility for communication to the konfd "
			"configuration daemon.\n");
		printf("Options:\n");
		printf("\t-v, --version\tPrint utility version.\n");
		printf("\t-h, --help\tPrint this help.\n");
		printf("\t-s <path>, --socket=<path>\tSpecify listen socket "
			"of the konfd daemon.\n");
	}
}
