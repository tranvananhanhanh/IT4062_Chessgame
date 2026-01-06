#include "database.h"
#include <stdio.h>
#include <string.h>

int db_get_user_email(PGconn *conn, const char *email, char *email_out, size_t email_out_size) {
    (void)conn;
    if (!email || !email_out || email_out_size == 0) return -1;
    /* Stub: echo back the provided email; return -1 to indicate not found */
    strncpy(email_out, email, email_out_size - 1);
    email_out[email_out_size - 1] = '\0';
    return -1;
}

int db_save_otp(PGconn *conn, int user_id, const char *otp) {
    (void)conn;
    (void)user_id;
    (void)otp;
    /* Stub: pretend failure (no persistence implemented) */
    printf("[DB] db_save_otp stub called (not implemented)\n");
    return 0;
}

int db_reset_password(PGconn *conn, int user_id, const char *otp, const char *new_password) {
    (void)conn;
    (void)user_id;
    (void)otp;
    (void)new_password;
    /* Stub: pretend failure */
    printf("[DB] db_reset_password stub called (not implemented)\n");
    return 0;
}
