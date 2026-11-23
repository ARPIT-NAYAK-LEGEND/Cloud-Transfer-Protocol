# Privacy Policy for Cloud Transfer Protocol

**Effective Date:** November 20, 2026  
**Last Updated:** November 23, 2026  
**Application Name:** Cloud Transfer Protocol (CTP)  
**Developer:** Arpit Nayak  
**Contact Email:** [arpitpupu@gmail.com](mailto:arpitpupu@gmail.com)  
**Official Homepage:** [https://ctp.arpitnayak.tech](https://ctp.arpitnayak.tech)  
**Hosted Privacy Policy:** [https://ctp.arpitnayak.tech/privacy](https://ctp.arpitnayak.tech/privacy)  
**Hosted Terms of Service:** [https://ctp.arpitnayak.tech/terms](https://ctp.arpitnayak.tech/terms)  

---

## 1. Application Overview & Purpose
**Cloud Transfer Protocol** ("Cloud Transfer Protocol", "CTP", "the Application", "we", or "us") is an open-source, client-side, zero-disk cloud migration engine and command-line utility. 

The sole and exclusive purpose of Cloud Transfer Protocol is to enable users to migrate, back up, and synchronize their own files between supported cloud storage providers (such as Google Drive, Microsoft OneDrive, and local storage) directly through in-memory streaming, without caching or writing transient file data to local storage disks.

---

## 2. Google User Data: What We Access & How It Is Used

Cloud Transfer Protocol requests OAuth 2.0 authorization from Google for the following specific scopes:

* **`https://www.googleapis.com/auth/drive.readonly`**: Used exclusively to list folders and files in your Google Drive and read file metadata (file names, sizes, MIME types, and file IDs) and content streams so that you can view and select files to transfer to your designated destination storage.
* **`https://www.googleapis.com/auth/drive.file`**: Used exclusively to upload and create files in your Google Drive that you specifically and explicitly direct the application to transfer from another storage source.

### Data Accessed:
1. **File Metadata:** File identifiers, names, byte sizes, and MIME types strictly to display file listings to you in your terminal.
2. **File Content:** In-memory byte chunks streamed in transient RAM solely to deliver the payload to your chosen destination cloud provider.

### Data NOT Collected or Stored:
* Cloud Transfer Protocol operates with **Zero Intermediary Servers**. There is no backend server, database, or proxy operated by the developer. All network requests are made directly from your client machine to Google API endpoints (`https://www.googleapis.com/...`).
* Cloud Transfer Protocol **never** collects, logs, transmits, or shares your file names, folder hierarchies, or file contents to any third party or developer server.

---

## 3. Google API Limited Use Disclosure

> **Cloud Transfer Protocol's use and transfer to any other app of information received from Google APIs will adhere to the [Google API Services User Data Policy](https://developers.google.com/terms/api-services-user-data-policy), including the Limited Use requirements.**

* **No Advertising or Commercial Profiling:** Google user data is never used, shared, or sold for targeted advertising, retargeting, interest-based advertising, credit scoring, or user profiling.
* **No AI/ML Model Training:** Data accessed via Google APIs is **never** used to develop, train, fine-tune, or improve generalized machine learning (ML) or artificial intelligence (AI) models.
* **Strict Human Oversight Restrictions:** No human beings ever inspect or view your file contents, except in the normal course of you viewing your own files in your local terminal.
* **Prohibition of AI NCII:** Cloud Transfer Protocol complies fully with the Google APIs Terms of Service and strictly prohibits the generation, facilitation, processing, or distribution of AI-generated Non-Consensual Intimate Imagery (AI NCII) or any non-consensual sexual content.

---

## 4. Local Credential Storage & Security

* **Local Token Storage:** When you authenticate with Google Drive, the resulting OAuth 2.0 access and refresh tokens are stored locally on your device in a user-restricted JSON file (e.g., `.utp_gdrive_token.json`).
* **Transport Encryption:** All API communications are secured with strict Transport Layer Security (TLS 1.3 / TLS 1.2) with mutual certificate authority validation (`ca-bundle.crt`).
* **Token Protection:** Authorization tokens and secret values are never printed in plaintext console logs or stored in persistent unencrypted transaction ledgers.

---

## 5. User Control: Data Deletion & Revoking Access

Because Cloud Transfer Protocol does not operate external databases or servers, no Google user data or personal data is ever stored on any remote system.

You retain complete and total control over your Google credentials:
1. **Revoke Google API Permissions:** You may revoke Cloud Transfer Protocol's access to your Google account at any time by visiting [Google Third-party apps & services](https://myaccount.google.com/permissions).
2. **Local Token Deletion:** You can delete your locally stored authorization tokens at any time by running:
   ```bash
   utp account remove gdrive
   ```
   or by manually deleting the local `.utp_gdrive_token.json` file on your machine.
3. **Cache & Ledger Deletion:** You can delete the local SQLite transaction ledger by deleting `utp.db` from your machine.

---

## 6. Open Source Transparency & Source Code

Cloud Transfer Protocol is published as open-source software under the MIT License. The entire source code is publicly auditable at:  
[https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol](https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol)

---

## 7. Contact Information

For any inquiries, questions, or concerns regarding this Privacy Policy or our security practices:
* **Developer:** Arpit Nayak
* **Email:** [arpitpupu@gmail.com](mailto:arpitpupu@gmail.com)
* **GitHub Issues:** [https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/issues](https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/issues)

