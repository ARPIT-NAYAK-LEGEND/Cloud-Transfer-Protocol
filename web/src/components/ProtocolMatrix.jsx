import React, { useState } from 'react';
import { Cloud, Check, ExternalLink, ShieldCheck, Zap } from 'lucide-react';

const PROTOCOLS = [
  {
    name: 'Google Drive',
    scheme: 'gdrive://<id>',
    category: 'Core Production MVP',
    status: 'Production Ready',
    auth: 'OAuth 2.0 PKCE + Token Refresh',
    features: ['Multi-Account Switching', 'Resumable Upload Sessions', 'Server-Side MD5 Match', 'Folder Recursion']
  },
  {
    name: 'Microsoft OneDrive',
    scheme: 'onedrive://<path>',
    category: 'Core Production MVP',
    status: 'Production Ready',
    auth: 'Microsoft Graph OAuth 2.0',
    features: ['Personal & Microsoft 365', 'Parent Folder Auto-Creation', 'Server-Side SHA-256 Match', 'Vault Protection']
  },
  {
    name: 'Local Storage',
    scheme: 'file://<path>',
    category: 'Core Production MVP',
    status: 'Production Ready',
    auth: 'OS File Permissions',
    features: ['NVMe / SSD Direct DMA', 'POSIX & Windows Pathing', 'Symlink Preservation', 'Zero-Copy OS Read']
  },
  {
    name: 'Amazon S3',
    scheme: 's3://<bucket>/<key>',
    category: 'Extended Cloud Adapter',
    status: 'Post-MVP Extension',
    auth: 'AWS SigV4 HMAC / IAM Roles',
    features: ['Multipart S3 Uploads', 'Server-Side ETag Validation', 'All Global AWS Regions', 'Storage Class Tiering']
  },
  {
    name: 'Cloudflare R2',
    scheme: 'r2://<bucket>/<key>',
    category: 'Extended Cloud Adapter',
    status: 'Post-MVP Extension',
    auth: 'S3-Compatible Token Auth',
    features: ['Zero Egress Bandwidth Fees', 'Low Latency Edge Endpoints', 'Chunked Object Streams', 'S3 API Mirroring']
  },
  {
    name: 'Backblaze B2',
    scheme: 'b2://<bucket>/<key>',
    category: 'Extended Cloud Adapter',
    status: 'Post-MVP Extension',
    auth: 'B2 Native / S3 API Tokens',
    features: ['Cold Storage Archival', 'Cost-Effective Backups', 'Large File Multipart Sessions', 'High Concurrency']
  },
  {
    name: 'Apple iCloud Drive',
    scheme: 'icloud://<path>',
    category: 'Extended Cloud Adapter',
    status: 'Post-MVP Extension',
    auth: 'Direct Web API & CloudKit Bridge',
    features: ['Dual-Mode Sync Bridge', 'macOS & Windows iCloud', 'Document Storage Mirror', 'Selective Sync']
  },
  {
    name: 'Dropbox',
    scheme: 'dropbox://<path>',
    category: 'Extended Cloud Adapter',
    status: 'Post-MVP Extension',
    auth: 'Dropbox v2 OAuth Token',
    features: ['files/copy_v2 Server Copy', 'Chunked Upload Sessions', 'Personal & Team Workspaces', 'Delta Tracking']
  },
  {
    name: 'Nextcloud / WebDAV',
    scheme: 'webdav://<path>',
    category: 'Extended Cloud Adapter',
    status: 'Post-MVP Extension',
    auth: 'HTTP Basic / App Password',
    features: ['RFC 4918 Compliant', 'ownCloud, Synology, TrueNAS', 'Self-Hosted Privacy', 'Chunked MKCOL / PUT']
  },
  {
    name: 'Microsoft Azure Blob',
    scheme: 'azure://<container>/<blob>',
    category: 'Extended Cloud Adapter',
    status: 'Post-MVP Extension',
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
    <section id="protocols" className="py-24 relative">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="text-center max-w-3xl mx-auto mb-12">
          <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-cyan-500/10 text-cyan-400 border border-cyan-500/20 text-xs font-mono font-medium mb-3">
            <Cloud className="w-3.5 h-3.5" />
            10-Cloud Ecosystem
          </div>
          <h2 className="text-3xl sm:text-4xl font-extrabold text-white tracking-tight">
            One Unified Protocol for Every Cloud
          </h2>
          <p className="mt-4 text-slate-400 text-base">
            No vendor lock-in. Move data seamlessly across consumer storage, enterprise object buckets, and self-hosted private servers.
          </p>

          {/* Filter pills */}
          <div className="mt-8 flex items-center justify-center gap-2">
            <button
              onClick={() => setFilter('all')}
              className={`px-4 py-1.5 rounded-full text-xs font-medium transition-colors cursor-pointer ${
                filter === 'all'
                  ? 'bg-cyan-500 text-white font-semibold shadow-lg shadow-cyan-500/20'
                  : 'bg-slate-900 text-slate-400 hover:text-white border border-slate-800'
              }`}
            >
              All 10 Providers
            </button>
            <button
              onClick={() => setFilter('core')}
              className={`px-4 py-1.5 rounded-full text-xs font-medium transition-colors cursor-pointer ${
                filter === 'core'
                  ? 'bg-cyan-500 text-white font-semibold shadow-lg shadow-cyan-500/20'
                  : 'bg-slate-900 text-slate-400 hover:text-white border border-slate-800'
              }`}
            >
              Core MVP (Production)
            </button>
            <button
              onClick={() => setFilter('extended')}
              className={`px-4 py-1.5 rounded-full text-xs font-medium transition-colors cursor-pointer ${
                filter === 'extended'
                  ? 'bg-cyan-500 text-white font-semibold shadow-lg shadow-cyan-500/20'
                  : 'bg-slate-900 text-slate-400 hover:text-white border border-slate-800'
              }`}
            >
              Extended Adapters
            </button>
          </div>
        </div>

        {/* Matrix Grid */}
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
          {filtered.map((item, idx) => (
            <div
              key={idx}
              className="p-6 rounded-2xl bg-slate-900/40 border border-slate-800/80 hover:border-slate-700 transition-all flex flex-col justify-between"
            >
              <div>
                <div className="flex items-center justify-between mb-3">
                  <h3 className="text-lg font-bold text-white">{item.name}</h3>
                  <span className={`text-[10px] font-mono px-2 py-0.5 rounded-full font-semibold ${
                    item.category.includes('Core')
                      ? 'bg-emerald-500/10 text-emerald-400 border border-emerald-500/30'
                      : 'bg-amber-500/10 text-amber-400 border border-amber-500/30'
                  }`}>
                    {item.status}
                  </span>
                </div>

                <div className="font-mono text-xs text-cyan-300 bg-slate-950/80 px-3 py-1.5 rounded-lg border border-slate-800 mb-4 flex items-center justify-between">
                  <span>{item.scheme}</span>
                </div>

                <div className="text-xs text-slate-400 mb-4 font-mono">
                  <span className="text-slate-500">Auth:</span> {item.auth}
                </div>

                <div className="space-y-1.5 pt-2 border-t border-slate-800/60">
                  {item.features.map((f, fi) => (
                    <div key={fi} className="flex items-center gap-2 text-xs text-slate-300">
                      <Check className="w-3.5 h-3.5 text-cyan-400 shrink-0" />
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
