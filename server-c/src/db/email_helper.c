#include "email_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

void generate_otp(char *otp_out, size_t len) {
    if (!otp_out || len == 0) return;
    const char digits[] = "0123456789";
    /* Simple RNG seed */
    srand((unsigned int)(time(NULL) ^ (intptr_t)otp_out));
    for (size_t i = 0; i < len; i++) {
        otp_out[i] = digits[rand() % 10];
    }
    otp_out[len] = '\0';
}

int send_otp_email(const char *email, const char *otp) {
    /* Stub: integrate real email service here */
    printf("[Email] send_otp_email stub called for %s with OTP %s\n", email ? email : "(null)", otp ? otp : "(null)");
    return 0;  /* Return 0 to indicate not actually sent */
}
