
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PGconn* db_connect() {
    // Default back to the previous working settings (host 192.168.1.7, port 5433, db=postgres)
    // but still allow overriding via environment variables.
    // First try environment variables, otherwise fall back to a local default.
    const char *host = getenv("DB_HOST") ? getenv("DB_HOST") : " 172.26.192.1 ";
    const char *port = getenv("DB_PORT") ? getenv("DB_PORT") : "5433";
    const char *dbname = getenv("DB_NAME") ? getenv("DB_NAME") : "chess_db";
    const char *user = getenv("DB_USER") ? getenv("DB_USER") : "postgres";
    const char *password = getenv("DB_PASSWORD") ? getenv("DB_PASSWORD") : "database";

    char conninfo[512];
    snprintf(conninfo, sizeof(conninfo),
        "host=%s port=%s dbname=%s user=%s password=%s connect_timeout=10",
        host, port, dbname, user, password);

    printf("[DB] Connecting with: host=%s port=%s db=%s user=%s\n", host, port, dbname, user);

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
