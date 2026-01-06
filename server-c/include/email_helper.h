#ifndef EMAIL_HELPER_H
#define EMAIL_HELPER_H

#include <stddef.h>

/* Generate a numeric OTP of length `len` (len bytes, no terminator). */
void generate_otp(char *otp_out, size_t len);

/* Placeholder email sender; return 1 on success, 0 on failure. */
int send_otp_email(const char *email, const char *otp);

#endif /* EMAIL_HELPER_H */
