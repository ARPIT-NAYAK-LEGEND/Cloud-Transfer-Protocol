import React from 'react';

const FEATS = [
  {
    num: '[01]',
    title: 'Volatile RAM Ring Buffer',
    tag: 'Zero Local Disk I/O',
    desc: 'Adaptive 2.5–40MB chunk slices bridge cloud HTTP range download sockets directly to cloud upload sessions in memory. Volatile chunks are released the moment receipt is acknowledged. Never touches storage media.'
  },
  {
    num: '[02]',
    title: 'Server-Side Hash Fast Path',
    tag: 'Zero Egress Overhead',
    desc: 'Simultaneously computes SHA-256 and MD5 digests in memory during streaming and matches them directly against cloud provider checksums (OneDrive sha256Hash, Google Drive md5Checksum, S3 ETag). Eliminates readback egress.'
  },
  {
    num: '[03]',
    title: 'Crash-Resilient SQLite Ledger',
    tag: 'ACID Persistence (utp.db)',
    desc: 'Every chunk boundary is committed to a local SQLite WAL ledger. If network drops or power is cut, `utp resume <id>` picks up instantly from the exact last committed chunk without re-transferring.'
  },
  {
    num: '[04]',
    title: 'Delta Sync & Smart Routing',
    tag: 'Pattern Engine',
    desc: 'Recursive folder synchronizer skips unchanged files (0 bytes transferred) with 5 conflict policies. Rules engine dynamically routes files by extension, file size, or path prefix across multiple cloud destinations.'
  }
];

export default function FeatureGrid() {
  return (
    <section id="features" className="py-20 border-b border-[#242220]">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        {/* Section Header */}
        <div className="flex flex-wrap items-center justify-between gap-3 pb-6 border-b border-[#242220] font-mono text-xs text-[#9E9B94] mb-10">
          <div className="flex items-center gap-2">
            <span className="text-[#FF5B1F] font-semibold">// 03 / ARCHITECTURE & CORE FEATS</span>
            <span className="text-[#66635C]">—</span>
            <span>C++20 STREAMING FOUNDATION</span>
          </div>
          <div className="text-[#66635C]">
            [STRICT MVP DISCIPLINE]
          </div>
        </div>

        {/* 4-Item Clean 2x2 Grid */}
        <div className="grid grid-cols-1 md:grid-cols-2 gap-px bg-[#242220] border border-[#242220] rounded-lg overflow-hidden font-mono text-xs">
          {FEATS.map((f, i) => (
            <div
              key={i}
              className="bg-[#121110] hover:bg-[#161513] transition-colors p-7 flex flex-col justify-between"
            >
              <div>
                <div className="flex items-center justify-between text-[#66635C] mb-3">
                  <span className="text-[#FF5B1F] font-bold">{f.num}</span>
                  <span className="px-2 py-0.5 rounded bg-[#1C1A18] border border-[#282624] text-[#9E9B94]">
                    {f.tag}
                  </span>
                </div>

                <h3 className="text-base font-bold text-[#F5F4F0]">
                  {f.title}
                </h3>

                <p className="mt-2.5 text-xs text-[#9E9B94] leading-relaxed font-sans">
                  {f.desc}
                </p>
              </div>

              <div className="mt-5 pt-3 border-t border-[#242220] text-[#66635C] flex items-center justify-between">
                <span>// C++20 Core</span>
                <span className="text-emerald-400">Production Verified</span>
              </div>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
