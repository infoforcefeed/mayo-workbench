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

#define DEFAULT_DB_HOST "localhost"
#define DEFAULT_DB_PORT "38080"

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

	char db_host[256] = {0};
	char db_port[64] = {0};

	/* Defaults: */
	strncpy(db_host, DEFAULT_DB_HOST, sizeof(db_host));
	strncpy(db_port, DEFAULT_DB_PORT, sizeof(db_port));

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
		} else if (strncmp(cur_arg, "-h", strlen("-h")) == 0) {
			if ((i + 1) < argc) {
				const char *host = argv[++i];
				strncpy(db_host, host, sizeof(db_host));
			} else {
				log_msg(LOG_ERR, "Not enough arguments to -h.");
				return -1;
			}
		} else if (strncmp(cur_arg, "-p", strlen("-p")) == 0) {
			if ((i + 1) < argc) {
				const char *port = argv[++i];
				strncpy(db_port, port, sizeof(db_port));
			} else {
				log_msg(LOG_ERR, "Not enough arguments to -p.");
				return -1;
			}
		}
	}

	log_msg(LOG_INFO, "Using DB host: http://%s:%s.", db_host, db_port);

	int rc = 0;
	if ((rc = http_serve(main_sock_fd, num_threads)) != 0) {
		term(SIGTERM);
		log_msg(LOG_ERR, "Could not start HTTP service.");
		return rc;
	}
	return 0;
}
