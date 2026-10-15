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
- `gdrive`: Google Drive.
- `onedrive`: Microsoft OneDrive.
- `s3`: Amazon S3 / MinIO / Wasabi.
- `azure`: Azure Blob Storage.

### Host / Bucket
For object storage systems like S3, the `host_or_bucket` defines the container (e.g., `s3://my-bucket/...`).
For hierarchical personal cloud storage or local disk, this field is often empty (e.g., `local:///C:/Users/file.txt` or `gdrive://file_id`).

### Path
The `path` identifies the specific object, file, or folder. In systems like Google Drive, the `path` may represent an opaque node ID rather than a literal filesystem path.

## Examples
- **Local File**: `local:///var/log/syslog`
- **Amazon S3 Object**: `s3://production-backups/db-dump.sql`
- **Google Drive File**: `gdrive://1B2M2Y8AsgTpgAmY7PhCfg`
- **OneDrive File**: `onedrive://documents/report.pdf`
