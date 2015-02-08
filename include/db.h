// vim: noet ts=4 sw=4
#pragma once
#include "common_defs.h"

/* Holds connection stuff: */
typedef struct db_conn {
	char host[DB_HOST_SIZ];
	char port[DB_PORT_SIZ];
	char db_name[DB_NAME_SIZ];
} db_conn;

/* Set and get functions. */
unsigned char *fetch_data_from_db(const db_conn *conn, const char key[static MAX_KEY_SIZE], size_t *outdata);
int store_data_in_db(const db_conn *conn, const char key[static MAX_KEY_SIZE], const unsigned char *val, const size_t vlen);

typedef struct db_key_match {
	const char key[MAX_KEY_SIZE];
	struct db_key_match *next;
} db_key_match;

/* Does a prefix match for the passed in prefix. */
db_key_match *fetch_matches_from_db(const db_conn *conn, const char prefix[static MAX_KEY_SIZE]);
unsigned int fetch_num_matches_from_db(const db_conn *conn, const char prefix[static MAX_KEY_SIZE]);
/* Dumps the WHOLE DAMN KEYSPACE! */
db_key_match *fetch_keyset_from_db(const db_conn *conn);
unsigned int fetch_num_keyset_from_db(const db_conn *conn);

const time_t fetch_uptime_from_db(const db_conn *conn);

typedef struct db_match {
	const unsigned char *data;
	const size_t dsize;
	const void *extradata;
	struct db_match *next;
} db_match;

/* Takes prefix and a predicate, and produces a list of db_match objects.
 * extrainput can be used to pass in anything extra you might want to pass in. Like something to compare to.
 * Extradata will be taken and stored in the db_match object. This is good if you're doing some working
 * you want to be able to access later, like deserializing and object. */
db_match *filter(const db_conn *conn, const char prefix[static MAX_KEY_SIZE], const void *extrainput,
		int (*filter)(const unsigned char *data, const size_t dsize,  const void *extrainput, void **extradata));
