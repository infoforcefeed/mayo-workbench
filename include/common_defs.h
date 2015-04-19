// vim: noet ts=4 sw=4
#pragma once

#define MAX_KEY_SIZE 250

#define FNV_HASH_SIZE 64

#define DB_HOST_SIZ 256
#define DB_PORT_SIZ 64
#define DB_NAME_SIZ 64

#include <oleg-http/oleg-http.h>

/* Global connection object. */
extern struct db_conn conn;
