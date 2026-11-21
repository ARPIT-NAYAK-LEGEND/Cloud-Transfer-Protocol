# Security Policy

## Supported Versions

The following versions of Universal Cloud Transfer Protocol (UTP) are currently supported with security updates:

| Version | Supported          |
| ------- | ------------------ |
| 2.6.x   | :white_check_mark: |
| < 2.6.0 | :x:                |

---

## Reporting a Vulnerability

We take the security of UTP and user data seriously. If you discover a potential security vulnerability, please report it responsibly rather than opening a public issue.

### How to Report

1. **GitHub Security Advisory**: Use the private security advisory feature on GitHub:
   [https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/security/advisories/new](https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/security/advisories/new)
2. **Details to Include**:
   * A description of the vulnerability and its potential impact.
   * Clear, reproducible proof-of-concept steps or scripts.
   * The operating system, UTP version, and cloud provider involved.

### What to Expect

* **Initial Response**: We will acknowledge receipt of your vulnerability report within 48 hours.
* **Assessment & Fix**: We will triage and develop a security patch in private before public release.
* **Credit**: Vulnerability reporters will be credited in the release notes upon patch disclosure.

---

## Security Guarantees & Architecture

* **TLS 1.3 In-Transit**: All remote socket connections enforce TLS 1.3 / TLS 1.2 with Certificate Authority verification (`CURLOPT_SSL_VERIFYPEER = 1`, `CURLOPT_SSL_VERIFYHOST = 2`).
* **Zero-Disk Streaming**: File payloads are streamed strictly in bounded RAM buffers and are deallocated immediately upon commit.
* **Secret Hygiene**: Client secrets, tokens, and authorization codes are never committed to revision control, tracked in SQLite logs, or displayed in terminal output.
