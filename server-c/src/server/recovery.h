#ifndef RECOVERY_H
#define RECOVERY_H

#include <libpq-fe.h>

// Initialize recovery system (create necessary tables)
void init_recovery_system(PGconn *conn);

// Recover interrupted matches (compensate ELO, notify users)
void recover_interrupted_matches(PGconn *conn);

#endif // RECOVERY_H
