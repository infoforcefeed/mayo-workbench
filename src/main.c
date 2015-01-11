// vim: noet ts=4 sw=4
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "db.h"
#include "http.h"
#include "logging.h"
#include "server.h"
#include "stack.h"

#define DEBUG 0

int main_sock_fd = 0;

void term(int signum) {
	close(main_sock_fd);
	exit(1);
}

int main(int argc, char *argv[]) {
	signal(SIGTERM, term);
	signal(SIGINT, term);
	signal(SIGKILL, term);
	signal(SIGCHLD, SIG_IGN);

	int num_threads = DEFAULT_NUM_THREADS;
	int i;
	for (i = 1; i < argc; i++) {
		const char *cur_arg = argv[i];
		if (strncmp(cur_arg, "-t", strlen("-t")) == 0) {
			if ((i + 1) < argc) {
				num_threads = strtol(argv[++i], NULL, 10);
				if (num_threads <= 0) {
					log_msg(LOG_ERR, "Thread count must be at least 1.");
					return -1;
				}
			} else {
				log_msg(LOG_ERR, "Not enough arguments to -t.");
				return -1;
			}
		}
	}

	int rc = 0;
	if ((rc = http_serve(main_sock_fd, num_threads)) != 0) {
		term(SIGTERM);
		log_msg(LOG_ERR, "Could not start HTTP service.");
		return rc;
	}
	return 0;
}
