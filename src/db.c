// vim: noet ts=4 sw=4
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include "db.h"
#include "http.h"
#include "logging.h"
#include "utils.h"

static const char DB_REQUEST[] = "GET /%s/%s HTTP/1.1\r\n"
	"Host: "DB_HOST":"DB_PORT"\r\n"
	"\r\n";

static const char DB_POST[] = "POST /%s/%s HTTP/1.1\r\n"
	"Host: "DB_HOST":"DB_PORT"\r\n"
	"Content-Length: %zu\r\n"
	"Content-Type: application/json\r\n"
	"\r\n"
	"%s";

/* We use 'Accept-Encoding: identity' here so we don't get back chunked
 * transfer shit. I hate parsing that garbage.
 */
static const char DB_MATCH[] =  "GET /%s/%s/_match HTTP/1.1\r\n"
	"Host: "DB_HOST":"DB_PORT"\r\n"
	"Accept-Encoding: identity\r\n"
	"\r\n";

static int _fetch_matches_common(const char prefix[static MAX_KEY_SIZE]) {
	const size_t db_match_siz = strlen(WAIFU_NMSPC) + strlen(DB_MATCH) + strnlen(prefix, MAX_KEY_SIZE);
	char new_db_request[db_match_siz];
	memset(new_db_request, '\0', db_match_siz);

	int sock = 0;
	sock = connect_to_host_with_port(DB_HOST, DB_PORT);
	if (sock == 0)
		goto error;

	snprintf(new_db_request, db_match_siz, DB_MATCH, WAIFU_NMSPC, prefix);
	int rc = send(sock, new_db_request, strlen(new_db_request), 0);
	if (strlen(new_db_request) != rc)
		goto error;

	return sock;

error:
	close(sock);
	return 0;
}

unsigned int fetch_num_matches_from_db(const char prefix[static MAX_KEY_SIZE]) {
	size_t outdata = 0;
	char *_data = NULL;
	char *_value = NULL;

	int sock = _fetch_matches_common(prefix);
	if (!sock)
		goto error;

	_data = receieve_only_http_header(sock, SELECT_TIMEOUT, &outdata);
	if (!_data)
		goto error;

	_value = get_header_value(_data, outdata, "X-Olegdb-Num-Matches");
	if (!_value)
		goto error;

	unsigned int to_return = strtol(_value, NULL, 10);

	free(_data);
	free(_value);
	close(sock);
	return to_return;

error:
	free(_value);
	free(_data);
	close(sock);
	return 0;
}

db_key_match *fetch_matches_from_db(const char prefix[static MAX_KEY_SIZE]) {
	size_t dsize = 0;
	unsigned char *_data = NULL;

	int sock = _fetch_matches_common(prefix);
	if (!sock)
		goto error;

	_data = receive_http(sock, &dsize);
	if (!_data)
		goto error;

	db_key_match *eol = NULL;
	db_key_match *cur = eol;
	int i;
	unsigned char *line_start = _data, *line_end = NULL;
	for (i = 0; i < dsize; i++) {
		if (_data[i] == '\n' && i + 1 < dsize) {
			line_end = &_data[i];
			const size_t line_size = line_end - line_start;

			db_key_match _stack = {
				.key = {0},
				.next = cur
			};
			memcpy((char *)_stack.key, line_start, line_size);

			db_key_match *new = calloc(1, sizeof(db_key_match));
			memcpy(new, &_stack, sizeof(db_key_match));

			cur = new;
			line_start = &_data[++i];
		}
	}

	free(_data);
	close(sock);
	return cur;

error:
	free(_data);
	close(sock);
	return NULL;
}

unsigned char *fetch_data_from_db(const char key[static MAX_KEY_SIZE], size_t *outdata) {
	unsigned char *_data = NULL;

	const size_t db_request_siz = strlen(WAIFU_NMSPC) + strlen(DB_REQUEST) + strnlen(key, MAX_KEY_SIZE);
	char new_db_request[db_request_siz];
	memset(new_db_request, '\0', db_request_siz);

	int sock = 0;
	sock = connect_to_host_with_port(DB_HOST, DB_PORT);
	if (sock == 0)
		goto error;

	snprintf(new_db_request, db_request_siz, DB_REQUEST, WAIFU_NMSPC, key);
	int rc = send(sock, new_db_request, strlen(new_db_request), 0);
	if (strlen(new_db_request) != rc)
		goto error;

	_data = receive_http(sock, outdata);
	if (!_data)
		goto error;

	/* log_msg(LOG_INFO, "Received: %s", _data); */

	close(sock);
	return _data;

error:
	free(_data);
	close(sock);
	return NULL;
}

int store_data_in_db(const char key[static MAX_KEY_SIZE], const unsigned char *val, const size_t vlen) {
	unsigned char *_data = NULL;

	const size_t vlen_len = UINT_LEN(vlen);
	/* See DB_POST for why we need all this. */
	const size_t db_post_siz = strlen(WAIFU_NMSPC) + strlen(key) + strlen(DB_POST) + vlen_len + vlen;
	char new_db_post[db_post_siz + 1];
	memset(new_db_post, '\0', db_post_siz + 1);

	int sock = 0;
	sock = connect_to_host_with_port(DB_HOST, DB_PORT);
	if (sock == 0)
		goto error;

	sprintf(new_db_post, DB_POST, WAIFU_NMSPC, key, vlen, val);
	int rc = send(sock, new_db_post, strlen(new_db_post), 0);
	if (strlen(new_db_post) != rc) {
		log_msg(LOG_ERR, "Could not send stuff to DB.");
		goto error;
	}

	/* I don't really care about the reply, but I probably should. */
	size_t out;
	_data = receive_http(sock, &out);
	if (!_data) {
		log_msg(LOG_ERR, "No reply from DB.");
		goto error;
	}

	free(_data);
	close(sock);
	return 1;

error:
	free(_data);
	close(sock);
	return 0;
}

db_match *filter(const char prefix[static MAX_KEY_SIZE], const void *extrainput,
		int (*filter)(const unsigned char *data, const size_t dsize, const void *extrainput, void **extradata)) {
	db_match *eol = NULL;
	db_match *cur = eol;

	db_key_match *prefix_matches = fetch_matches_from_db(prefix);
	db_key_match *cur_km = prefix_matches;
	while (cur_km) {
		db_key_match *next = cur_km->next;

		/* 1. Fetch data for this key from DB. */
		size_t dsize = 0;
		unsigned char *_data = fetch_data_from_db(cur_km->key, &dsize);
		/* 2. Apply filter predicate. */
		if (_data) {
			/* 3. If it returns true, add it to the list.*/
			void *extradata = NULL;
			if (filter(_data, dsize, extrainput, &extradata)) {
				db_match _new = {
					.data = _data,
					.dsize = dsize,
					.extradata = extradata,
					.next = cur
				};

				db_match *new = calloc(1, sizeof(db_match));
				memcpy(new, &_new, sizeof(db_match));
				cur = new;
			} else {
				free((unsigned char *)_data);
			}
		}
		/* 4. Continue.*/
		free(cur_km);
		cur_km = next;
	}

	return cur;
}
