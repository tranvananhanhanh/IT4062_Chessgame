#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple hash: FNV-1a 64-bit (not secure, demo only)
unsigned long long simple_hash(const char *str) {
    unsigned long long hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= (unsigned char)(*str++);
        hash *= 1099511628211ULL;
    }
    return hash;
}

int db_create_user(PGconn *conn, const char *username, const char *password) {
    if (!db_check_connection(conn)) return 0;
    
    char query[1024];
    char escaped_username[256];
    char hashstr[32];
    
    // Hash password
    unsigned long long hashval = simple_hash(password);
    snprintf(hashstr, sizeof(hashstr), "%016llx", hashval);
    
    // Escape strings to prevent SQL injection
    PQescapeStringConn(conn, escaped_username, username, strlen(username), NULL);
    
    snprintf(query, sizeof(query),
        "INSERT INTO users (name, password_hash, elo_point) "
        "VALUES ('%s', '%s', 1200) RETURNING user_id",
        escaped_username, hashstr);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[DB] Create user failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }
    
    int user_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    printf("[DB] Created user '%s' with ID %d\n", username, user_id);
    return user_id;
}

int db_verify_user(PGconn *conn, const char *username, const char *password) {
    if (!db_check_connection(conn)) return 0;
    
    char query[1024];
    char escaped_username[256];
    char hashstr[32];
    
    // Hash password
    unsigned long long hashval = simple_hash(password);
    snprintf(hashstr, sizeof(hashstr), "%016llx", hashval);
    
    PQescapeStringConn(conn, escaped_username, username, strlen(username), NULL);
    
    snprintf(query, sizeof(query),
        "SELECT user_id FROM users WHERE name = '%s' AND password_hash = '%s'",
        escaped_username, hashstr);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return 0; // Authentication failed
    }
    
    int user_id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    return user_id;
}

int db_get_user_info(PGconn *conn, int user_id, char *output, int output_size) {
    if (!db_check_connection(conn)) {
        snprintf(output, output_size, "ERROR|Database connection failed\n");
        return 0;
    }
    
    char query[1024];
    sprintf(query,
        "SELECT name, elo_point FROM users WHERE user_id = %d",
        user_id);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        snprintf(output, output_size, "ERROR|User not found\n");
        PQclear(res);
        return 0;
    }
    
    const char *username = PQgetvalue(res, 0, 0);
    const char *elo = PQgetvalue(res, 0, 1);
    
    snprintf(output, output_size, "USER_INFO|%s|%s\n", username, elo);
    PQclear(res);
    
    return 1;

}

int db_get_user_email(PGconn *conn, const char *email_input, char *email, int email_size) {
    if (!db_check_connection(conn)) return 0;
    
    char query[1024];
    char escaped_email[256];
    
    PQescapeStringConn(conn, escaped_email, email_input, strlen(email_input), NULL);
    
    snprintf(query, sizeof(query),
        "SELECT email, user_id FROM users WHERE email = '%s'",
        escaped_email);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return 0;
    }
    
    const char *db_email = PQgetvalue(res, 0, 0);
    if (db_email && strlen(db_email) > 0) {
        snprintf(email, email_size, "%s", db_email);
        int user_id = atoi(PQgetvalue(res, 0, 1));
        PQclear(res);
        return user_id;
    }
    
    PQclear(res);
    return 0;
}

int db_save_otp(PGconn *conn, int user_id, const char *otp) {
    if (!db_check_connection(conn)) return 0;
    
    char query[1024];
    char escaped_otp[16];
    
    PQescapeStringConn(conn, escaped_otp, otp, strlen(otp), NULL);
    
    // Delete old OTP first
    snprintf(query, sizeof(query),
        "DELETE FROM password_reset WHERE user_id = %d", user_id);
    PQexec(conn, query);
    
    // Insert new OTP (expires in 10 minutes)
    snprintf(query, sizeof(query),
        "INSERT INTO password_reset (user_id, otp, expires_at) "
        "VALUES (%d, '%s', NOW() + INTERVAL '10 minutes')",
        user_id, escaped_otp);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "[DB] Save OTP failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }
    
    PQclear(res);
    return 1;
}

int db_verify_otp(PGconn *conn, int user_id, const char *otp) {
    if (!db_check_connection(conn)) return 0;
    
    char query[1024];
    char escaped_otp[16];
    
    PQescapeStringConn(conn, escaped_otp, otp, strlen(otp), NULL);
    
    snprintf(query, sizeof(query),
        "SELECT reset_id FROM password_reset "
        "WHERE user_id = %d AND otp = '%s' AND expires_at > NOW()",
        user_id, escaped_otp);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return 0;
    }
    
    PQclear(res);
    return 1;
}

int db_reset_password(PGconn *conn, int user_id, const char *new_password, const char *otp) {
    if (!db_check_connection(conn)) return 0;
    
    // Verify OTP first
    if (!db_verify_otp(conn, user_id, otp)) {
        return 0;
    }
    
    char query[1024];
    char hashstr[32];
    
    // Hash new password
    unsigned long long hashval = simple_hash(new_password);
    snprintf(hashstr, sizeof(hashstr), "%016llx", hashval);
    
    // Update password
    snprintf(query, sizeof(query),
        "UPDATE users SET password_hash = '%s' WHERE user_id = %d",
        hashstr, user_id);
    
    PGresult *res = PQexec(conn, query);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "[DB] Reset password failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }
    
    PQclear(res);
    
    // Delete used OTP
    snprintf(query, sizeof(query),
        "DELETE FROM password_reset WHERE user_id = %d", user_id);
    PQexec(conn, query);
    
    printf("[DB] Password reset successful for user_id %d\n", user_id);
    return 1;
}
