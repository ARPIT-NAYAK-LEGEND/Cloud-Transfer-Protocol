import React, { useState } from 'react';
import { Check } from 'lucide-react';

const PROTOCOLS = [
  {
    num: '01',
    name: 'Google Drive',
    scheme: 'gdrive://<id>',
    category: 'Core Production',
    auth: 'OAuth 2.0 PKCE + Token Refresh',
    features: ['Multi-Account Switching', 'Resumable Upload Sessions', 'Server-Side MD5 Match', 'Folder Recursion']
  },
  {
    num: '02',
    name: 'Microsoft OneDrive',
    scheme: 'onedrive://<path>',
    category: 'Core Production',
    auth: 'Microsoft Graph OAuth 2.0',
    features: ['Personal & Microsoft 365', 'Parent Folder Auto-Creation', 'Server-Side SHA-256 Match', 'Personal Vault Protection']
  },
  {
    num: '03',
    name: 'Local Storage',
    scheme: 'file://<path>',
    category: 'Core Production',
    auth: 'OS File Permissions',
    features: ['NVMe / SSD Direct DMA', 'POSIX & Windows Paths', 'Symlink Preservation', 'Zero-Copy OS Read']
  },
  {
    num: '04',
    name: 'Amazon S3',
    scheme: 's3://<bucket>/<key>',
    category: 'Extended Adapter',
    auth: 'AWS SigV4 HMAC / IAM Roles',
    features: ['Multipart S3 Uploads', 'Server-Side ETag Validation', 'All AWS Regions', 'Storage Class Tiering']
  },
  {
    num: '05',
    name: 'Cloudflare R2',
    scheme: 'r2://<bucket>/<key>',
    category: 'Extended Adapter',
    auth: 'S3-Compatible Token Auth',
    features: ['Zero Egress Bandwidth Fees', 'Low Latency Edge Endpoints', 'Chunked Object Streams', 'S3 API Mirroring']
  },
  {
    num: '06',
    name: 'Backblaze B2',
    scheme: 'b2://<bucket>/<key>',
    category: 'Extended Adapter',
    auth: 'B2 Native / S3 API Tokens',
    features: ['Cold Storage Archival', 'Cost-Effective Backups', 'Large File Multipart Sessions', 'High Concurrency']
  },
  {
    num: '07',
    name: 'Apple iCloud Drive',
    scheme: 'icloud://<path>',
    category: 'Extended Adapter',
    auth: 'Direct Web API & CloudKit Bridge',
    features: ['Dual-Mode Sync Bridge', 'macOS & Windows iCloud', 'Document Storage Mirror', 'Selective Sync']
  },
  {
    num: '08',
    name: 'Dropbox',
    scheme: 'dropbox://<path>',
    category: 'Extended Adapter',
    auth: 'Dropbox v2 OAuth Token',
    features: ['files/copy_v2 Server Copy', 'Chunked Upload Sessions', 'Personal & Team Workspaces', 'Delta Tracking']
  },
  {
    num: '09',
    name: 'Nextcloud / WebDAV',
    scheme: 'webdav://<path>',
    category: 'Extended Adapter',
    auth: 'HTTP Basic / App Password',
    features: ['RFC 4918 Compliant', 'ownCloud, Synology, TrueNAS', 'Self-Hosted Privacy', 'Chunked MKCOL / PUT']
  },
  {
    num: '10',
    name: 'Microsoft Azure Blob',
    scheme: 'azure://<container>/<blob>',
    category: 'Extended Adapter',
    auth: 'Shared Access Signatures (SAS)',
    features: ['Block Blob Streaming', 'Container Auto-Provisioning', 'Hot / Cool / Cold Tiers', 'Enterprise Security']
  }
];

export default function ProtocolMatrix() {
  const [filter, setFilter] = useState('all');

  const filtered = PROTOCOLS.filter(p => {
    if (filter === 'core') return p.category.includes('Core');
    if (filter === 'extended') return p.category.includes('Extended');
    return true;
  });

  return (
    <section id="protocols" className="py-20 border-b border-[#242220]">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        {/* Section Header */}
        <div className="flex flex-wrap items-center justify-between gap-3 pb-6 border-b border-[#242220] font-mono text-xs text-[#9E9B94] mb-12">
          <div className="flex items-center gap-2">
            <span className="text-[#FF5B1F] font-semibold">// 04 / UNIFIED STORAGE PROTOCOLS</span>
            <span className="text-[#66635C]">—</span>
            <span>10 CLOUD STORAGE INTEGRATIONS</span>
          </div>

          {/* Filter Pills */}
          <div className="flex items-center gap-1">
            <button
              onClick={() => setFilter('all')}
              className={`px-2.5 py-1 rounded transition-colors cursor-pointer ${
                filter === 'all'
                  ? 'bg-[#242220] text-[#FF5B1F] font-semibold'
                  : 'text-[#66635C] hover:text-[#F5F4F0]'
              }`}
            >
              [all 10]
            </button>
            <button
              onClick={() => setFilter('core')}
              className={`px-2.5 py-1 rounded transition-colors cursor-pointer ${
                filter === 'core'
                  ? 'bg-[#242220] text-[#FF5B1F] font-semibold'
                  : 'text-[#66635C] hover:text-[#F5F4F0]'
              }`}
            >
              [core mvp]
            </button>
            <button
              onClick={() => setFilter('extended')}
              className={`px-2.5 py-1 rounded transition-colors cursor-pointer ${
                filter === 'extended'
                  ? 'bg-[#242220] text-[#FF5B1F] font-semibold'
                  : 'text-[#66635C] hover:text-[#F5F4F0]'
              }`}
            >
              [extended]
            </button>
          </div>
        </div>

        {/* Matrix Grid */}
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-px bg-[#242220] border border-[#242220] rounded-lg overflow-hidden">
          {filtered.map((item, idx) => (
            <div
              key={idx}
              className="bg-[#121110] hover:bg-[#161513] transition-colors p-6 flex flex-col justify-between"
            >
              <div>
                <div className="flex items-center justify-between font-mono text-xs mb-3">
                  <span className="text-[#66635C]">{item.num}</span>
                  <span className={`px-2 py-0.5 rounded text-[10px] font-mono font-medium ${
                    item.category.includes('Core')
                      ? 'bg-emerald-950/40 border border-emerald-500/30 text-emerald-400'
                      : 'bg-[#1C1A18] border border-[#282624] text-[#9E9B94]'
                  }`}>
                    {item.category}
                  </span>
                </div>

                <h3 className="text-base font-bold text-[#F5F4F0]">{item.name}</h3>

                <div className="mt-2 p-2 rounded bg-[#0E0D0C] border border-[#242220] font-mono text-xs text-[#FF5B1F] flex items-center justify-between">
                  <span>{item.scheme}</span>
                </div>

                <div className="mt-2 text-xs font-mono text-[#66635C]">
                  AUTH: <span className="text-[#9E9B94]">{item.auth}</span>
                </div>

                <div className="mt-4 pt-3 border-t border-[#242220] space-y-1.5 font-mono text-xs">
                  {item.features.map((f, fi) => (
                    <div key={fi} className="flex items-center gap-2 text-[#9E9B94]">
                      <Check className="w-3 h-3 text-[#FF5B1F] shrink-0" />
                      <span>{f}</span>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
