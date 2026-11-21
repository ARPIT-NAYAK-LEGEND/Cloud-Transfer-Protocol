# Privacy Policy for Universal Cloud Transfer Protocol (UTP)

Last updated: November 2026

Universal Cloud Transfer Protocol ("UTP", "we", "our", or "the application") is an open-source, peer-to-cloud data migration and synchronization engine. We are committed to protecting user privacy and ensuring full sovereignty over your data.

---

## 1. Zero-Telemetry & Zero Data Collection

* **No Analytics or Tracking**: UTP contains zero analytics libraries, tracking pixels, telemetry probes, or third-party monitoring SDKs.
* **No Middleman Servers**: All transfers and API requests occur directly between your device and the cloud service providers (Google Drive, Microsoft OneDrive, Amazon Web Services, etc.). Your data never touches any intermediary proxy, relay server, or developer-owned backend.
* **No Profiling**: We do not collect, store, sell, or profile your personal information, transfer metadata, filenames, file contents, or device identifiers.

---

## 2. Zero-Disk Memory Architecture

* **In-Memory Volatile Buffers**: UTP processes data chunks strictly within volatile Random Access Memory (RAM). 
* **Zero Disk Footprint**: UTP never caches, stages, or writes transient file payloads to local solid-state drives (SSD) or hard drives (HDD) during cloud-to-cloud streaming.
* **Immediate Memory Reclamation**: Buffer memory is overwritten and reclaimed immediately upon chunk delivery. No residual forensic file artifacts remain on your operating system.

---

## 3. Credential & Authentication Hygiene

* **Client-Side Token Storage**: OAuth access and refresh tokens acquired during authentication are stored locally on your machine in user-scoped files (`.utp_*_token.json`) within your project or configuration folder.
* **Narrowest API Scopes**: UTP requests only the minimum storage permissions necessary to read and write files (`https://www.googleapis.com/auth/drive`, `Files.ReadWrite.All`). UTP never requests access to your email inbox, calendar, contacts, or personal messages.
* **Exclusion from Logs**: Access tokens, bearer credentials, and authorization codes are strictly masked and excluded from console output, error reports, and the local SQLite transaction ledger (`utp.db`).

---

## 4. Google API Services User Data Policy Compliance

UTP's use and transfer of information received from Google APIs adheres to the **Google API Services User Data Policy**, including the **Limited Use** requirements:

* UTP only accesses user Google Drive files for the explicit purpose of user-directed file transfers, synchronization, and local backups requested by the user.
* UTP does not transfer, disclose, or sell Google user data to any external parties.
* UTP does not use Google user data for advertising, market research, or training machine learning / artificial intelligence models.
* Human interaction with your files is restricted solely to your own actions in the local terminal user interface.

---

## 5. Security & In-Transit Encryption

* All data transfers to and from cloud endpoints are encrypted in transit using **Transport Layer Security (TLS 1.3 / TLS 1.2)** with mutual certificate verification and Perfect Forward Secrecy (PFS).
* Cryptographic streaming checksums (SHA-256) are calculated in flight to guarantee end-to-end data integrity.

---

## 6. Open Source Transparency

UTP is open-source software released under the MIT License. You can inspect the source code, build the binaries independently, and verify all privacy claims directly at:
[https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol](https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol)

---

## 7. Contact & Inquiries

For questions or concerns regarding this Privacy Policy:
* **Repository**: [https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/issues](https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/issues)
* **Email**: Contact through GitHub profile
