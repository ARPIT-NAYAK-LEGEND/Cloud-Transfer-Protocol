import React from 'react';

const PROTOCOLS = [
  { num: '01', name: 'Google Drive', scheme: 'gdrive://<id>', type: 'Core MVP', auth: 'OAuth2 PKCE', highlight: 'Multi-account, server-side MD5 verification' },
  { num: '02', name: 'Microsoft OneDrive', scheme: 'onedrive://<path>', type: 'Core MVP', auth: 'Microsoft Graph', highlight: 'Personal & 365, server-side SHA-256 match' },
  { num: '03', name: 'Local Storage', scheme: 'file://<path>', type: 'Core MVP', auth: 'OS Permissions', highlight: 'NVMe / SSD Direct DMA, symlink support' },
  { num: '04', name: 'Amazon S3', scheme: 's3://<bucket>/<key>', type: 'Extended', auth: 'SigV4 HMAC', highlight: 'Multipart uploads, ETag validation, all regions' },
  { num: '05', name: 'Cloudflare R2', scheme: 'r2://<bucket>/<key>', type: 'Extended', auth: 'S3 API Tokens', highlight: 'Zero egress fees, low-latency object store' },
  { num: '06', name: 'Backblaze B2', scheme: 'b2://<bucket>/<key>', type: 'Extended', auth: 'B2 / S3 Tokens', highlight: 'Cost-effective cold archival, high concurrency' },
  { num: '07', name: 'Apple iCloud Drive', scheme: 'icloud://<path>', type: 'Extended', auth: 'Web & CloudKit', highlight: 'Dual-mode sync bridge, macOS & Windows' },
  { num: '08', name: 'Dropbox', scheme: 'dropbox://<path>', type: 'Extended', auth: 'OAuth 2.0', highlight: 'files/copy_v2 server copy, chunked sessions' },
  { num: '09', name: 'Nextcloud / WebDAV', scheme: 'webdav://<path>', type: 'Extended', auth: 'HTTP Basic', highlight: 'RFC 4918 compliant, self-hosted NAS, ownCloud' },
  { num: '10', name: 'Azure Blob Storage', scheme: 'azure://<c>/<b>', type: 'Extended', auth: 'Azure SAS', highlight: 'Block blob streaming, container provisioning' }
];

export default function ProtocolMatrix() {
  return (
    <section id="protocols" className="py-20 border-b border-[#242220]">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        {/* Section Header */}
        <div className="flex flex-wrap items-center justify-between gap-3 pb-6 border-b border-[#242220] font-mono text-xs text-[#9E9B94] mb-10">
          <div className="flex items-center gap-2">
            <span className="text-[#FF5B1F] font-semibold">// 04 / UNIFIED STORAGE PROTOCOLS</span>
            <span className="text-[#66635C]">—</span>
            <span>10 CLOUD STORAGE INTEGRATIONS</span>
          </div>
          <div className="text-[#66635C]">
            [UNIFIED URI SCHEME SPEC]
          </div>
        </div>

        {/* Compact Scannable Grid */}
        <div className="grid grid-cols-1 md:grid-cols-2 gap-px bg-[#242220] border border-[#242220] rounded-lg overflow-hidden font-mono text-xs">
          {PROTOCOLS.map((p, idx) => (
            <div
              key={idx}
              className="bg-[#121110] hover:bg-[#161513] transition-colors p-4 flex items-center justify-between gap-4"
            >
              <div className="flex items-center gap-3 overflow-hidden">
                <span className="text-[#66635C] shrink-0">{p.num}</span>
                <div className="truncate">
                  <div className="font-bold text-[#F5F4F0] flex items-center gap-2">
                    <span>{p.name}</span>
                    <span className={`text-[10px] px-1.5 py-0.2 rounded font-normal ${
                      p.type === 'Core MVP'
                        ? 'bg-emerald-950/40 text-emerald-400 border border-emerald-500/30'
                        : 'text-[#66635C]'
                    }`}>
                      {p.type}
                    </span>
                  </div>
                  <div className="text-[11px] text-[#9E9B94] font-sans truncate mt-0.5">
                    {p.highlight}
                  </div>
                </div>
              </div>

              <div className="shrink-0 text-right">
                <div className="text-[#FF5B1F] font-bold">{p.scheme}</div>
                <div className="text-[10px] text-[#66635C]">{p.auth}</div>
              </div>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
