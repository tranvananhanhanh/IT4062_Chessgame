#ifndef MATCHMAKING_H
#define MATCHMAKING_H

#include <stddef.h>

int handle_mmjoin(const char *payload, char *out, size_t out_size);
int handle_mmstatus(const char *payload, char *out, size_t out_size);
int handle_mmcancel(const char *payload, char *out, size_t out_size);

#endif
