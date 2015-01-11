// vim: noet ts=4 sw=4
#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <regex.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "db.h"
#include "http.h"
#include "logging.h"
#include "server.h"
#include "grengine.h"
#include "greshunkel.h"


const static char LNAV[] =
"<nav class=\"lnav grd\">"
"	<div class=\"row\">"
"		<div class=\"col-6\">"
"			<h4>OlegDB</h4>"
"			<span class=\"lnav-section\">Mayo Workbench</span>"
"			<ul>"
"				<li><a href=\"/\">Overview</a></li>"
"				<li><a href=\"#\">Data</a></li>"
"			</ul>"
"			<span class=\"lnav-section\">OlegDB</span>"
"			<ul>"
"				<li><a href=\"https://olegdb.org/docs/0.1.5/en/documentation.html\">Documentation</a></li>"
"			</ul>"
"		</div>"
"	</div>"
"	<p class=\"copyright\">&copy;2015 Quinlan Pfiffer</p>"
"</nav>";

/* Various handlers for our routes: */
static int static_handler(const http_request *request, http_response *response, const void *e) {
	/* Remove the leading slash: */
	const char *file_path = request->resource + sizeof(char);
	return mmap_file(file_path, response);
}

static int index_handler(const http_request *request, http_response *response, const void *e) {
	int rc = mmap_file("./templates/index.html", response);
	if (rc != 200)
		return rc;
	// 1. Render the mmap()'d file with greshunkel
	const char *mmapd_region = (char *)response->out;
	const size_t original_size = response->outsize;

	/* Render that shit */
	size_t new_size = 0;
	greshunkel_ctext *ctext = gshkl_init_context();
	const db_conn *conn = (db_conn *)e;
	gshkl_add_string(ctext, "LNAV", LNAV);
	gshkl_add_string(ctext, "DB_NAME", conn->db_name);
	//gshkl_add_int(ctext, "webm_count", webm_count());
	//gshkl_add_int(ctext, "alias_count", webm_alias_count());

	char *rendered = gshkl_render(ctext, mmapd_region, original_size, &new_size);
	gshkl_free_context(ctext);

	/* Clean up the stuff we're no longer using. */
	munmap(response->out, original_size);
	free(response->extra_data);

	/* Make sure the response is kept up to date: */
	response->outsize = new_size;
	response->out = (unsigned char *)rendered;
	return 200;
}

static int favicon_handler(const http_request *request, http_response *response, const void *e) {
	strncpy(response->mimetype, "image/x-icon", sizeof(response->mimetype));
	return mmap_file("./static/favicon.ico", response);
}

/* All other routes: */
static const route all_routes[] = {
	{"GET", "^/favicon.ico$", 0, &favicon_handler, &mmap_cleanup},
	{"GET", "^/static/[a-zA-Z0-9/_-]*\\.[a-zA-Z]*$", 0, &static_handler, &mmap_cleanup},
	{"GET", "^/$", 0, &index_handler, &heap_cleanup},
};

struct acceptor_arg {
	const int sock;
	const db_conn *conn;
};

static void *acceptor(void *arg) {
	const struct acceptor_arg *args = (struct acceptor_arg *)arg;
	const int main_sock_fd = args->sock;
	const db_conn *conn = args->conn;

	while(1) {
		struct sockaddr_storage their_addr = {0};
		socklen_t sin_size = sizeof(their_addr);

		int new_fd = accept(main_sock_fd, (struct sockaddr *)&their_addr, &sin_size);

		if (new_fd == -1) {
			log_msg(LOG_ERR, "Could not accept new connection.");
			return NULL;
		} else {
			respond(new_fd, all_routes, sizeof(all_routes)/sizeof(all_routes[0]), conn);
			close(new_fd);
		}
	}
	return NULL;
}

int http_serve(int main_sock_fd, const int num_threads, const db_conn *conn) {
	/* Our acceptor pool: */
	pthread_t workers[num_threads];

	int rc = -1;
	main_sock_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (main_sock_fd <= 0) {
		log_msg(LOG_ERR, "Could not create main socket.");
		goto error;
	}

	int opt = 1;
	setsockopt(main_sock_fd, SOL_SOCKET, SO_REUSEADDR, (void*) &opt, sizeof(opt));

	const int port = 8666;
	struct sockaddr_in hints = {0};
	hints.sin_family		 = AF_INET;
	hints.sin_port			 = htons(port);
	hints.sin_addr.s_addr	 = htonl(INADDR_ANY);

	rc = bind(main_sock_fd, (struct sockaddr *)&hints, sizeof(hints));
	if (rc < 0) {
		log_msg(LOG_ERR, "Could not bind main socket.");
		goto error;
	}

	rc = listen(main_sock_fd, 0);
	if (rc < 0) {
		log_msg(LOG_ERR, "Could not listen on main socket.");
		goto error;
	}
	log_msg(LOG_FUN, "Listening on http://localhost:%i/", port);

	struct acceptor_arg arg = {
		.sock = main_sock_fd,
		.conn = conn
	};

	int i;
	for (i = 0; i < num_threads; i++) {
		if (pthread_create(&workers[i], NULL, acceptor, &arg) != 0) {
			goto error;
		}
		log_msg(LOG_INFO, "Thread %i started.", i);
	}

	for (i = 0; i < num_threads; i++) {
		pthread_join(workers[i], NULL);
		log_msg(LOG_INFO, "Thread %i stopped.", i);
	}


	close(main_sock_fd);
	return 0;

error:
	perror("Socket error");
	close(main_sock_fd);
	return rc;
}

