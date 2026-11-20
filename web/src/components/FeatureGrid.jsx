import React from 'react';
import { HardDrive, ShieldCheck, Database, RefreshCw, GitFork, Cpu, Zap, Lock } from 'lucide-react';

const FEATURES = [
  {
    icon: HardDrive,
    color: 'from-cyan-500 to-sky-600',
    title: 'Zero-Disk Volatile RAM Pipeline',
    badge: 'Core Architecture',
    desc: 'Bypasses local NVMe/SSD storage completely. Streams cloud HTTP range chunks directly into volatile memory ring buffers (adaptive 2.5–40MB) and out to cloud upload sessions. Never wears down local disks or exhausts storage.'
  },
  {
    icon: ShieldCheck,
    color: 'from-emerald-500 to-teal-600',
    title: 'Fast-Path Server Checksum Match',
    badge: 'Zero Egress Overhead',
    desc: 'Simultaneously computes SHA-256 and MD5 hashes in memory during streaming and matches them directly against provider checksums (OneDrive sha256Hash, Google Drive md5Checksum, S3 ETag). Eliminates multi-GB cloud download readbacks.'
  },
  {
    icon: Database,
    color: 'from-violet-500 to-purple-600',
    title: 'Crash-Resilient SQLite Ledger',
    badge: 'ACID Persistence',
    desc: 'All chunk boundaries and transfer events are tracked in a high-performance SQLite WAL ledger (utp.db). If your machine loses power or network drops, `utp resume <id>` picks up instantly from the exact last chunk without re-transferring.'
  },
  {
    icon: RefreshCw,
    color: 'from-sky-500 to-blue-600',
    title: 'Delta Folder Synchronization',
    badge: '0-Byte Unchanged Skips',
    desc: 'Synchronize multi-gigabyte directory structures recursively. UTP performs cryptographic delta analysis to skip unchanged files (0 bytes transferred) and offers 5 conflict resolution strategies: Newer Wins, Overwrite, Skip, Rename, Fail.'
  },
  {
    icon: GitFork,
    color: 'from-amber-500 to-orange-600',
    title: 'Multi-Destination Fan-Out Stream',
    badge: 'Single-Pass Broadcast',
    desc: 'Read once from source and stream concurrently to multiple target cloud buckets (e.g. AWS S3 + Google Drive + Backblaze B2) in a single read pass, cutting egress costs and network time in half.'
  },
  {
    icon: Cpu,
    color: 'from-pink-500 to-rose-600',
    title: 'Smart Rules-Based Routing',
    badge: 'Pattern Engine',
    desc: 'Route datasets automatically based on dynamic conditions: file extension (ext=mp4,mkv), byte size (size>100M), path prefix (path=backups/), or filename keywords with a customizable default destination fallback.'
  }
];

export default function FeatureGrid() {
  return (
    <section id="features" className="py-24 relative bg-slate-950/60 border-t border-slate-900">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="text-center max-w-3xl mx-auto mb-16">
          <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-violet-500/10 text-violet-400 border border-violet-500/20 text-xs font-mono font-medium mb-3">
            <Zap className="w-3.5 h-3.5" />
            Engine Capabilities
          </div>
          <h2 className="text-3xl sm:text-4xl font-extrabold text-white tracking-tight">
            Engineered for High-Velocity Cloud Transport
          </h2>
          <p className="mt-4 text-slate-400 text-base">
            Every layer of UTP is built in C++20 for minimum memory overhead, line-rate network saturation, and unbreakable reliability.
          </p>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8">
          {FEATURES.map((f, i) => {
            const Icon = f.icon;
            return (
              <div
                key={i}
                className="group relative p-8 rounded-2xl bg-slate-900/40 border border-slate-800/80 hover:border-cyan-500/40 transition-all duration-300 hover:-translate-y-1 hover:shadow-xl hover:shadow-cyan-950/20 flex flex-col justify-between"
              >
                <div>
                  <div className="flex items-center justify-between mb-5">
                    <div className={`w-12 h-12 rounded-xl bg-gradient-to-br ${f.color} p-[1px] shadow-lg`}>
                      <div className="w-full h-full bg-[#0a0e14] rounded-[11px] flex items-center justify-center text-white">
                        <Icon className="w-6 h-6" />
                      </div>
                    </div>
                    <span className="text-[11px] font-mono px-2.5 py-1 rounded-full bg-slate-800/80 text-slate-300 border border-slate-700/60">
                      {f.badge}
                    </span>
                  </div>

                  <h3 className="text-xl font-bold text-white group-hover:text-cyan-300 transition-colors">
                    {f.title}
                  </h3>

                  <p className="mt-3 text-slate-400 text-sm leading-relaxed">
                    {f.desc}
                  </p>
                </div>

                <div className="mt-6 pt-4 border-t border-slate-800/60 flex items-center gap-2 text-xs font-mono text-slate-500 group-hover:text-cyan-400 transition-colors">
                  <span>0-Disk C++20 Core</span>
                  <span>•</span>
                  <span>TLS 1.3 Direct</span>
                </div>
              </div>
            );
          })}
        </div>
      </div>
    </section>
  );
}
