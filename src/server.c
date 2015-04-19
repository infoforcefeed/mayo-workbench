// vim: noet ts=4 sw=4
#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
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

#include <38-moths/38-moths.h>
#include <oleg-http/oleg-http.h>

#include "server.h"
#include "common_defs.h"

const static char LNAV[] =
"<nav class=\"lnav grd\">\n"
"	<div class=\"row\">\n"
"		<div class=\"col-6\">\n"
"			<h4>OlegDB</h4>\n"
"			<span class=\"lnav-section\">Mayo Workbench</span>\n"
"			<ul>\n"
"				<li><a href=\"/\">Overview</a></li>\n"
"				<li><a href=\"/data\">Data</a></li>\n"
"			</ul>\n"
"			<span class=\"lnav-section\">OlegDB</span>\n"
"			<ul>\n"
"				<li><a href=\"https://olegdb.org/docs/0.1.5/en/documentation.html\">Documentation</a></li>\n"
"			</ul>\n"
"		</div>\n"
"	</div>\n"
"	<p class=\"copyright\">&copy;2015 Quinlan Pfiffer</p>\n"
"</nav>\n";

/* Various handlers for our routes: */
int static_handler(const http_request *request, http_response *response) {
	/* Remove the leading slash: */
	const char *file_path = request->resource + sizeof(char);
	return mmap_file(file_path, response);
}

int index_handler(const http_request *request, http_response *response) {
	int rc = mmap_file("./templates/index.html", response);
	if (rc != 200)
		return rc;
	// 1. Render the mmap()'d file with greshunkel
	const char *mmapd_region = (char *)response->out;
	const size_t original_size = response->outsize;

	/* Render that shit */
	size_t new_size = 0;
	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_string(ctext, "LNAV", LNAV);
	gshkl_add_string(ctext, "DB_NAME", conn.db_name);

	gshkl_add_int(ctext, "key_count", fetch_num_keyset_from_db(&conn));
	gshkl_add_int(ctext, "UPTIME", fetch_uptime_from_db(&conn));

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

int datum_handler(const http_request *request, http_response *response) {
	int rc = mmap_file("./templates/datum.html", response);
	if (rc != 200)
		return rc;
	const char *mmapd_region = (char *)response->out;
	const size_t original_size = response->outsize;

	size_t new_size = 0;
	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_string(ctext, "LNAV", LNAV);
	gshkl_add_string(ctext, "DB_NAME", conn.db_name);

	char key[MAX_KEY_SIZE] = {0};
	strncpy(key, request->resource + request->matches[1].rm_so, MAX_KEY_SIZE);

	gshkl_add_string(ctext, "RECORD", key);

	size_t dsize = 0;
	char *data = (char *)fetch_data_from_db(&conn, key, &dsize);
	if (data)
		gshkl_add_string(ctext, "VALUE", data);
	else
		gshkl_add_string(ctext, "VALUE", "(No data for this key)");
	free(data);

	char *rendered = gshkl_render(ctext, mmapd_region, original_size, &new_size);
	gshkl_free_context(ctext);

	munmap(response->out, original_size);
	free(response->extra_data);

	response->outsize = new_size;
	response->out = (unsigned char *)rendered;
	return 200;
}

int data_handler(const http_request *request, http_response *response) {
	int rc = mmap_file("./templates/data.html", response);
	if (rc != 200)
		return rc;
	const char *mmapd_region = (char *)response->out;
	const size_t original_size = response->outsize;

	size_t new_size = 0;
	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_string(ctext, "LNAV", LNAV);
	gshkl_add_string(ctext, "DB_NAME", conn.db_name);

	greshunkel_var records = gshkl_add_array(ctext, "RECORDS");
	int i = 0;
	db_key_match *matches = fetch_keyset_from_db(&conn);
	db_key_match *cur = matches;
	while (cur) {
		i++;
		db_key_match *n = cur->next;
		gshkl_add_string_to_loop(&records, cur->key);
		free(cur);
		cur = n;
	}

	gshkl_add_int(ctext, "RESULT_COUNT", i);

	char *rendered = gshkl_render(ctext, mmapd_region, original_size, &new_size);
	gshkl_free_context(ctext);

	munmap(response->out, original_size);
	free(response->extra_data);

	response->outsize = new_size;
	response->out = (unsigned char *)rendered;
	return 200;
}

int favicon_handler(const http_request *request, http_response *response) {
	strncpy(response->mimetype, "image/x-icon", sizeof(response->mimetype));
	return mmap_file("./static/favicon.ico", response);
}
