#!/usr/bin/env python3
import os
import smtplib
import ssl
import argparse


def send_mail(to_email: str, otp: str):
    smtp_host = os.environ.get("SMTP_HOST")
    smtp_port = int(os.environ.get("SMTP_PORT", "587"))
    smtp_user = os.environ.get("SMTP_USER")
    smtp_pass = os.environ.get("SMTP_PASS")
    smtp_from = os.environ.get("SMTP_FROM", smtp_user)
    use_tls = os.environ.get("SMTP_USE_TLS", "true").lower() != "false"

    if not smtp_host or not smtp_user or not smtp_pass or not smtp_from:
        raise RuntimeError("SMTP env vars missing (SMTP_HOST, SMTP_PORT, SMTP_USER, SMTP_PASS, SMTP_FROM)")

    subject = "Chess Game - Password Reset OTP"
    body = f"Your OTP code is: {otp}\nThis code will expire in 10 minutes."

    msg = f"From: {smtp_from}\nTo: {to_email}\nSubject: {subject}\n\n{body}"

    if use_tls:
        context = ssl.create_default_context()
        with smtplib.SMTP(smtp_host, smtp_port) as server:
            server.starttls(context=context)
            server.login(smtp_user, smtp_pass)
            server.sendmail(smtp_from, [to_email], msg)
    else:
        with smtplib.SMTP(smtp_host, smtp_port) as server:
            server.login(smtp_user, smtp_pass)
            server.sendmail(smtp_from, [to_email], msg)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--to", required=True)
    parser.add_argument("--otp", required=True)
    args = parser.parse_args()
    send_mail(args.to, args.otp)


if __name__ == "__main__":
    main()
