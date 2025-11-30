
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PGconn* db_connect() {
    const char *conninfo = 
        "host=localhost "
        "port=5432 "
        "dbname=chess_db "
        "user=postgres "
        "password=123456 "
        "connect_timeout=5 ";

    PGconn *conn = PQconnectdb(conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "[DB] Connection failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }

    // Check encoding
    if (strcmp(PQparameterStatus(conn, "client_encoding"), "UTF8") != 0) {
        PQsetClientEncoding(conn, "UTF8");
    }

    printf("[DB] Connected successfully\n");
    return conn;
}

void db_disconnect(PGconn *conn) {
    if (conn != NULL) {
        PQfinish(conn);
        printf("[DB] Disconnected\n");
    }
}

int db_check_connection(PGconn *conn) {
    if (conn == NULL) return 0;

    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "[DB] Lost connection: %s\n", PQerrorMessage(conn));
        return 0;
    }

    return 1;
}
