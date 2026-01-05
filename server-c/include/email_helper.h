#ifndef EMAIL_HELPER_H
#define EMAIL_HELPER_H

void generate_otp(char *otp, int length);
int send_otp_email(const char *email, const char *otp);

#endif // EMAIL_HELPER_H
