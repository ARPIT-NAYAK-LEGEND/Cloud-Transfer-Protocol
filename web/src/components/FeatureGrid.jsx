import React from 'react';
import { HardDrive, ShieldCheck, Database, RefreshCw, GitFork, Cpu } from 'lucide-react';

const FEATS = [
  {
    num: '[FEAT-01]',
    title: 'Volatile RAM Ring Buffer Pipeline',
    tag: 'C++20 Zero-Disk Core',
    desc: 'Adaptive 2.5–40MB chunk ring buffer directly bridges cloud HTTP range read sockets to upload chunk sessions in memory. Volatile chunks are released the instant the cloud destination acknowledges receipt. Zero disk storage required.'
  },
  {
    num: '[FEAT-02]',
    title: 'Server-Side Hash Fast Path',
    tag: 'Zero Egress Overhead',
    desc: 'Simultaneously computes SHA-256 and MD5 hashes in memory during streaming and matches them directly against provider cloud checksums (OneDrive sha256Hash, Google Drive md5Checksum, S3 ETag). Eliminates cloud download read-back.'
  },
  {
    num: '[FEAT-03]',
    title: 'Crash-Resilient SQLite Ledger',
    tag: 'ACID Persistence (utp.db)',
    desc: 'All chunk states and transfer events are committed to a local SQLite database in WAL mode. Interrupted transfers (network dropped, laptop closed, power lost) resume instantly via `utp resume <id>` from the exact last chunk.'
  },
  {
    num: '[FEAT-04]',
    title: 'Delta Folder Synchronization',
    tag: '0-Byte Unchanged Skip',
    desc: 'Synchronizes complex directory trees recursively. Cryptographic delta analysis identifies identical files and transfers 0 bytes. Supports 5 conflict resolution strategies: Newer Wins, Overwrite, Skip, Rename, and Fail.'
  },
  {
    num: '[FEAT-05]',
    title: 'Multi-Destination Fan-Out Broadcast',
    tag: 'Single-Pass Stream',
    desc: 'Reads once from source and concurrently broadcasts bytes to multiple target cloud buckets (e.g. AWS S3 + Google Drive + Backblaze B2) in a single read pass, cutting network bandwidth and egress charges.'
  },
  {
    num: '[FEAT-06]',
    title: 'Rules-Based Smart Routing Engine',
    tag: 'Pattern Predicates',
    desc: 'Dynamically routes files based on configurable conditions: file extension (ext=mp4,mkv), byte size (size>100M), path prefix (path=backups/), or filename keywords with customizable default fallback.'
  }
];

export default function FeatureGrid() {
  return (
    <section id="features" className="py-20 border-b border-[#242220]">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        {/* Section Header */}
        <div className="flex flex-wrap items-center justify-between gap-3 pb-6 border-b border-[#242220] font-mono text-xs text-[#9E9B94] mb-12">
          <div className="flex items-center gap-2">
            <span className="text-[#FF5B1F] font-semibold">// 03 / ARCHITECTURE & FEATS</span>
            <span className="text-[#66635C]">—</span>
            <span>ENGINEERED FOR DATA INTENSIVE MIGRATIONS</span>
          </div>
          <div className="text-[#66635C]">
            [STRICT MVP ARCHITECTURE DISCIPLINE]
          </div>
        </div>

        {/* Grid Frame */}
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-px bg-[#242220] border border-[#242220] rounded-lg overflow-hidden">
          {FEATS.map((f, i) => (
            <div
              key={i}
              className="bg-[#121110] hover:bg-[#161513] transition-colors p-8 flex flex-col justify-between group"
            >
              <div>
                <div className="flex items-center justify-between font-mono text-xs text-[#66635C] mb-4">
                  <span className="text-[#FF5B1F] font-bold">{f.num}</span>
                  <span className="px-2 py-0.5 rounded bg-[#1C1A18] border border-[#282624] text-[#9E9B94]">
                    {f.tag}
                  </span>
                </div>

                <h3 className="text-lg font-bold text-[#F5F4F0] group-hover:text-[#FF5B1F] transition-colors">
                  {f.title}
                </h3>

                <p className="mt-3 text-sm text-[#9E9B94] leading-relaxed">
                  {f.desc}
                </p>
              </div>

              <div className="mt-6 pt-4 border-t border-[#242220] font-mono text-xs text-[#66635C] flex items-center justify-between">
                <span>// C++20 Core</span>
                <span className="text-[#9E9B94]">Verified in CI</span>
              </div>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
