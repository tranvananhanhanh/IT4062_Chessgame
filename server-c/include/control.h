#ifndef CONTROL_H
#define CONTROL_H

#include "client_session.h"
#include <libpq-fe.h>

/**
 * Control Module - Handles game control actions
 * (Pause, Resume, Draw offers, Rematch)
 */

// Pause/Resume
void handle_pause(ClientSession *session, int num_params, char *param1, 
                 char *param2, PGconn *db);
void handle_resume(ClientSession *session, int num_params, char *param1, 
                  char *param2, PGconn *db);

// Draw offers
void handle_draw_request(ClientSession *session, int num_params, char *param1, 
                        char *param2, PGconn *db);
void handle_draw_accept(ClientSession *session, int num_params, char *param1, 
                       char *param2, PGconn *db);
void handle_draw_decline(ClientSession *session, int num_params, char *param1, 
                        char *param2, PGconn *db);

// Rematch
void handle_rematch_request(ClientSession *session, int num_params, char *param1, 
                           char *param2, PGconn *db);
void handle_rematch_accept(ClientSession *session, int num_params, char *param1, 
                          char *param2, PGconn *db);
void handle_rematch_decline(ClientSession *session, int num_params, char *param1, 
                           char *param2, PGconn *db);

#endif // CONTROL_H
