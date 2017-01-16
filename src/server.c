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
	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_string(ctext, "LNAV", LNAV);
	gshkl_add_string(ctext, "DB_NAME", conn.db_name);

	gshkl_add_int(ctext, "key_count", fetch_num_keyset_from_db(&conn));
	gshkl_add_int(ctext, "UPTIME", fetch_uptime_from_db(&conn));

	return render_file(ctext, "./templates/index.html", response);
}

int datum_handler(const http_request *request, http_response *response) {
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

	return render_file(ctext, "./templates/datum.html", response);
}

int datum_handler_save(const http_request *request, http_response *response) {
	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_string(ctext, "DATA", "{}");

	char key[MAX_KEY_SIZE] = {0};
	strncpy(key, request->resource + request->matches[1].rm_so, MAX_KEY_SIZE);

	const unsigned char *data = request->full_body;
	const size_t dlen = request->body_len;

	if (store_data_in_db(&conn, key, data, dlen)) {
		gshkl_add_string(ctext, "SUCCESS", "true");
		gshkl_add_string(ctext, "ERROR", "");
	} else {
		gshkl_add_string(ctext, "SUCCESS", "false");
		gshkl_add_string(ctext, "ERROR", "Could not save that record.");
	}

	gshkl_add_string(ctext, "SUCCESS", "true");
	gshkl_add_string(ctext, "ERROR", "");
	return render_file(ctext, "./templates/response.json", response);
}

int datum_handler_delete(const http_request *request, http_response *response) {
	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_string(ctext, "DATA", "{}");

	char key[MAX_KEY_SIZE] = {0};
	strncpy(key, request->resource + request->matches[1].rm_so, MAX_KEY_SIZE);

	if (delete_record_from_db(&conn, key)) {
		gshkl_add_string(ctext, "SUCCESS", "true");
		gshkl_add_string(ctext, "ERROR", "");
	} else {
		gshkl_add_string(ctext, "SUCCESS", "false");
		gshkl_add_string(ctext, "ERROR", "Could not delete that record.");
	}
	return render_file(ctext, "./templates/response.json", response);
}

int data_handler(const http_request *request, http_response *response) {
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

	return render_file(ctext, "./templates/data.html", response);
}

int data_handler_filter(const http_request *request, http_response *response) {
	greshunkel_ctext *ctext = gshkl_init_context();
	greshunkel_var items_arr = gshkl_add_array(ctext, "ITEMS");

	char prefix[MAX_KEY_SIZE] = {0};
	strncpy(prefix, request->resource + request->matches[1].rm_so, MAX_KEY_SIZE);

	db_key_match *matches = fetch_matches_from_db(&conn, prefix);
	if (matches) {
		gshkl_add_string(ctext, "SUCCESS", "true");
		gshkl_add_string(ctext, "ERROR", "");

		db_key_match *current = matches;
		while (current) {
			db_key_match *next = current->next;
			gshkl_add_string_to_loop(&items_arr, current->key);
			free(current);
			current = next;
		}
	} else {
		gshkl_add_string(ctext, "SUCCESS", "false");
		gshkl_add_string(ctext, "ERROR", "Could not get records matching that prefix.");
	}
	return render_file(ctext, "./templates/response_list.json", response);
}

int favicon_handler(const http_request *request, http_response *response) {
	strncpy(response->mimetype, "image/x-icon", sizeof(response->mimetype));
	return mmap_file("./static/favicon.ico", response);
}
