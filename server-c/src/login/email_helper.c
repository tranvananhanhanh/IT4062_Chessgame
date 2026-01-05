#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Generate 6-digit OTP
void generate_otp(char *otp, int length) {
    srand((unsigned int)(time(NULL) + getpid()));
    for (int i = 0; i < length; i++) {
        otp[i] = '0' + (rand() % 10);
    }
    otp[length] = '\0';
}

// Send OTP via email using external Python helper (smtplib)
int send_otp_email(const char *email, const char *otp) {
    const char *smtp_user = getenv("SMTP_USER");
    const char *smtp_pass = getenv("SMTP_PASS");
    const char *smtp_host = getenv("SMTP_HOST");
    const char *smtp_port = getenv("SMTP_PORT");
    const char *smtp_from = getenv("SMTP_FROM");
    const char *use_tls   = getenv("SMTP_USE_TLS");
    const char *script_override = getenv("OTP_PYTHON_SCRIPT");

    // Fallback: still log to console if missing config
    if (!smtp_user || !smtp_pass || !smtp_host || !smtp_port || !smtp_from) {
        fprintf(stderr, "[EMAIL] SMTP env not set. Falling back to console output.\n");
        printf("========================================\n");
        printf("To: %s\n", email);
        printf("Subject: Chess Game - Password Reset OTP\n");
        printf("Body:\n");
        printf("Your OTP code is: %s\n", otp);
        printf("This code will expire in 10 minutes.\n");
        printf("========================================\n");
        return 1;
    }

    const char *script_path = script_override ? script_override : "scripts/send_otp_email.py";

    // Build command: python3 <script> --to ... --otp ...
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "python3 %s --to '%s' --otp '%s'",
             script_path, email, otp);

    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "[EMAIL] Failed to send email via script. Exit code: %d\n", ret);
        return 0;
    }

    printf("[EMAIL] OTP sent to %s\n", email);
    return 1;
}
