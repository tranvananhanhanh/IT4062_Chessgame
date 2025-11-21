#include "database.h"
#include <stdio.h>
#include <string.h>

PGconn* db_connect() {
    const char *conninfo = "host=localhost dbname=chess_game user=postgres password=postgres";
    PGconn *conn = PQconnectdb(conninfo);
    
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "[DB] Connection failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }
    
    printf("[DB] Connected successfully\n");
    return conn;
}

void db_disconnect(PGconn *conn) {
    if (conn) {
        PQfinish(conn);
        printf("[DB] Disconnected\n");
    }
}

int db_check_connection(PGconn *conn) {
    if (!conn || PQstatus(conn) != CONNECTION_OK) {
        return 0;
    }
    return 1;
}
