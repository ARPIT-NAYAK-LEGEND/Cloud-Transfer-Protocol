# UTP Resource Identifiers

UTP identifies files and directories across disparate cloud systems using a uniform URI syntax. This allows the engine to dynamically route operations to the correct provider.

## Format
```
<scheme>://<host_or_bucket>/<path>
```

### Scheme
The `scheme` explicitly declares the underlying Storage Provider.
Known schemas include:
- `local` or `file`: Local filesystem.
- `gdrive`: Google Drive (supports file ID or remote folder paths).
- `onedrive`: Microsoft OneDrive (Personal, Business, SharePoint).
- `dropbox`: Dropbox (v2 API, team spaces & personal).
- `icloud`: Apple iCloud Drive (Direct Web API & local sync bridge).
- `s3`: Amazon S3, MinIO, Wasabi, Ceph.
- `r2`: Cloudflare R2 (zero egress fees S3-compatible API).
- `b2`: Backblaze B2 (high-efficiency S3-compatible API).
- `azure`: Azure Blob Storage.
- `webdav`, `nextcloud`, `owncloud`: RFC 4918 compliant WebDAV storage.

### Multi-Account Contexts
Schemes support an optional account alias separated by a colon (`:`):
```
gdrive:my_personal://1B2M2Y8AsgTpgAmY7PhCfg
gdrive:work_account://Projects/Dataset.zip
onedrive:backup://Archives/2026.tar
```

### Host / Bucket
For object storage systems like S3, R2, and B2, the `host_or_bucket` defines the container (e.g., `s3://my-bucket/...`).
For hierarchical personal cloud storage or local disk, this field is often empty (e.g., `file:///C:/Users/file.txt` or `gdrive://file_id`).

### Path
The `path` identifies the specific object, file, or folder. In systems like Google Drive, the `path` may represent an opaque node ID or a virtual hierarchical path.

## Examples
- **Local File**: `local:///var/log/syslog` or `file:///D:/Dataset.bin`
- **Google Drive File**: `gdrive://1B2M2Y8AsgTpgAmY7PhCfg`
- **OneDrive File**: `onedrive://documents/report.pdf`
- **Dropbox File**: `dropbox://Datasets/train.parquet`
- **Apple iCloud File**: `icloud://Documents/Presentation.key`
- **Amazon S3 Object**: `s3://production-backups/db-dump.sql`
- **Cloudflare R2 Object**: `r2://media-bucket/video.mp4`
- **Backblaze B2 Object**: `b2://cold-storage/snapshot.tar.gz`
- **WebDAV / Nextcloud**: `webdav://Photos/Vacation2026.zip`
- **Azure Blob**: `azure://container-name/blob.bin`
