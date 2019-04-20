// vim: noet ts=4 sw=4
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <38-moths/38-moths.h>

#include "common_defs.h"
#include "server.h"

#define DEBUG 0

#define DEFAULT_DB_HOST "localhost"
#define DEFAULT_DB_PORT "38080"

int main_sock_fd = 0;
struct db_conn conn = {
	.host = {0},
	.port = {0},
	.db_name = {0}
};

void term(int signum) {
	close(main_sock_fd);
	exit(1);
}

/* All other routes: */
static const m38_route all_routes[] = {
	{"GET", "favicon_handler", "^/favicon.ico$", 0, &favicon_handler, &m38_mmap_cleanup},
	{"GET", "static_handler", "^/static/[a-zA-Z0-9/_-]*\\.[a-zA-Z]*$", 0, &static_handler, &m38_mmap_cleanup},
	{"GET", "data_handler", "^/data$", 0, &data_handler, &m38_heap_cleanup},
	{"POST", "data_handler_filter", "^/data/filter/([a-zA-Z0-9\\/\\_\\-\\:\\@\\.]*)$", 1, &data_handler_filter, &m38_heap_cleanup},
	{"POST", "datum_handler_save", "^/datum/save/([a-zA-Z0-9\\/\\_\\-\\:\\@\\.]*)$", 1, &datum_handler_save, &m38_heap_cleanup},
	{"POST", "datum_handler_delete", "^/datum/delete/([a-zA-Z0-9\\/\\_\\-\\:\\@\\.]*)$", 1, &datum_handler_delete, &m38_heap_cleanup},
	{"GET", "datum_handler", "^/datum/([a-zA-Z0-9\\/\\_\\-\\:\\@\\.]*)$", 1, &datum_handler, &m38_heap_cleanup},
	{"GET", "root_handler", "^/$", 0, &index_handler, &m38_heap_cleanup},
};

int main(int argc, char *argv[]) {
	signal(SIGTERM, term);
	signal(SIGINT, term);
	signal(SIGKILL, term);
	signal(SIGCHLD, SIG_IGN);

	int num_threads = 2;

	/* Defaults: */
	strncpy(conn.host, DEFAULT_DB_HOST, sizeof(conn.host));
	strncpy(conn.port, DEFAULT_DB_PORT, sizeof(conn.port));

	int i;
	for (i = 1; i < argc; i++) {
		const char *cur_arg = argv[i];
		if (strncmp(cur_arg, "-t", strlen("-t")) == 0) {
			if ((i + 1) < argc) {
				num_threads = strtol(argv[++i], NULL, 10);
				if (num_threads <= 0) {
					m38_log_msg(LOG_ERR, "Thread count must be at least 1.");
					return -1;
				}
			} else {
				m38_log_msg(LOG_ERR, "Not enough arguments to -t.");
				return -1;
			}
		} else if (strncmp(cur_arg, "-h", strlen("-h")) == 0) {
			if ((i + 1) < argc) {
				const char *host = argv[++i];
				strncpy(conn.host, host, sizeof(conn.host));
			} else {
				m38_log_msg(LOG_ERR, "Not enough arguments to -h.");
				return -1;
			}
		} else if (strncmp(cur_arg, "-p", strlen("-p")) == 0) {
			if ((i + 1) < argc) {
				const char *port = argv[++i];
				strncpy(conn.port, port, sizeof(conn.port));
			} else {
				m38_log_msg(LOG_ERR, "Not enough arguments to -p.");
				return -1;
			}
		} else if (strncmp(cur_arg, "-n", strlen("-n")) == 0) {
			if ((i + 1) < argc) {
				const char *name = argv[++i];
				strncpy(conn.db_name, name, sizeof(conn.db_name));
			} else {
				m38_log_msg(LOG_ERR, "Not enough arguments to -n.");
				return -1;
			}
		}
	}

	if (!strnlen(conn.db_name, sizeof(conn.db_name))) {
		m38_log_msg(LOG_ERR, "A database name is required (for now). Please specify one with -n.");
		exit(1);
	}

	m38_log_msg(LOG_INFO, "Connecting to db '%s' on host: http://%s:%s.", conn.db_name, conn.host, conn.port);

	int rc = 0;
	const size_t route_len = sizeof(all_routes)/sizeof(all_routes[0]);
	if ((rc = m38_http_serve(&main_sock_fd, 8086, num_threads, all_routes, route_len)) != 0) {
		term(SIGTERM);
		m38_log_msg(LOG_ERR, "Could not start HTTP service.");
		return rc;
	}
	return 0;
}
