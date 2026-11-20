import React from 'react';
import { Cpu, Server, HardDrive, ShieldCheck, Database, ArrowRight, Lock } from 'lucide-react';

export default function ArchitectureDiagram() {
  return (
    <section id="architecture" className="py-24 relative bg-slate-950/70 border-t border-slate-900">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="text-center max-w-3xl mx-auto mb-16">
          <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-cyan-500/10 text-cyan-400 border border-cyan-500/20 text-xs font-mono font-medium mb-3">
            <Cpu className="w-3.5 h-3.5" />
            Under the Hood
          </div>
          <h2 className="text-3xl sm:text-4xl font-extrabold text-white tracking-tight">
            How Zero-Disk Memory Streaming Works
          </h2>
          <p className="mt-4 text-slate-400 text-base">
            Traditional tools download files to a local temp folder before uploading. UTP eliminates intermediate disk I/O entirely using a high-throughput volatile RAM ring pipeline.
          </p>
        </div>

        {/* Visual Architecture Flow */}
        <div className="max-w-5xl mx-auto p-8 rounded-3xl bg-slate-900/50 border border-slate-800 shadow-2xl relative overflow-hidden backdrop-blur-xl">
          <div className="grid grid-cols-1 md:grid-cols-3 gap-8 items-center relative z-10">
            {/* Step 1: Source */}
            <div className="p-6 rounded-2xl bg-slate-950/90 border border-slate-800 text-center flex flex-col items-center">
              <div className="w-14 h-14 rounded-2xl bg-cyan-500/10 border border-cyan-500/30 flex items-center justify-center text-cyan-400 mb-4 shadow-lg shadow-cyan-500/10">
                <Server className="w-7 h-7" />
              </div>
              <h3 className="font-bold text-lg text-white">Source Cloud</h3>
              <p className="text-xs font-mono text-cyan-300 mt-1">Google Drive / S3 / R2</p>
              <div className="mt-4 text-xs text-slate-400 leading-relaxed text-left space-y-1.5 w-full pt-3 border-t border-slate-800/80">
                <div className="flex items-center gap-1.5 text-slate-300">
                  <span className="w-1.5 h-1.5 rounded-full bg-cyan-400" />
                  <span>HTTP Range Stream</span>
                </div>
                <div className="flex items-center gap-1.5 text-slate-300">
                  <span className="w-1.5 h-1.5 rounded-full bg-cyan-400" />
                  <span>TLS 1.3 Direct Socket</span>
                </div>
                <div className="flex items-center gap-1.5 text-slate-300">
                  <span className="w-1.5 h-1.5 rounded-full bg-cyan-400" />
                  <span>Adaptive Chunk Fetch</span>
                </div>
              </div>
            </div>

            {/* Step 2: The Core Ring Buffer (Center Highlight) */}
            <div className="p-6 rounded-2xl bg-gradient-to-b from-slate-900 to-slate-950 border-2 border-cyan-500/40 text-center flex flex-col items-center shadow-xl shadow-cyan-950/40 relative">
              <div className="absolute -top-3 px-3 py-0.5 rounded-full bg-cyan-500 text-white text-[10px] font-mono font-bold uppercase tracking-wider">
                0-Disk RAM Core
              </div>

              <div className="w-16 h-16 rounded-2xl bg-gradient-to-br from-cyan-500 to-violet-600 p-[1px] mb-4 shadow-xl shadow-cyan-500/30">
                <div className="w-full h-full bg-[#0a0e14] rounded-[15px] flex items-center justify-center text-cyan-300">
                  <Cpu className="w-8 h-8" />
                </div>
              </div>

              <h3 className="font-bold text-lg text-white">UTP Memory Ring</h3>
              <p className="text-xs font-mono text-violet-300 mt-1">Adaptive 2.5–40MB Ring</p>

              <div className="mt-4 text-xs text-slate-300 leading-relaxed text-left space-y-2 w-full pt-3 border-t border-slate-800/80">
                <div className="p-2 rounded-lg bg-slate-900/90 border border-slate-800 text-[11px] font-mono text-cyan-300">
                  Dual In-Flight Hasher
                  <div className="text-[10px] text-slate-400 font-sans">Simultaneous SHA-256 + MD5</div>
                </div>
                <div className="p-2 rounded-lg bg-slate-900/90 border border-slate-800 text-[11px] font-mono text-violet-300">
                  ACID Ledger (utp.db)
                  <div className="text-[10px] text-slate-400 font-sans">SQLite WAL chunk checkpoints</div>
                </div>
              </div>
            </div>

            {/* Step 3: Destination */}
            <div className="p-6 rounded-2xl bg-slate-950/90 border border-slate-800 text-center flex flex-col items-center">
              <div className="w-14 h-14 rounded-2xl bg-violet-500/10 border border-violet-500/30 flex items-center justify-center text-violet-400 mb-4 shadow-lg shadow-violet-500/10">
                <Server className="w-7 h-7" />
              </div>
              <h3 className="font-bold text-lg text-white">Destination Cloud</h3>
              <p className="text-xs font-mono text-violet-300 mt-1">OneDrive / S3 / WebDAV</p>
              <div className="mt-4 text-xs text-slate-400 leading-relaxed text-left space-y-1.5 w-full pt-3 border-t border-slate-800/80">
                <div className="flex items-center gap-1.5 text-slate-300">
                  <span className="w-1.5 h-1.5 rounded-full bg-violet-400" />
                  <span>Chunked Upload Session</span>
                </div>
                <div className="flex items-center gap-1.5 text-slate-300">
                  <span className="w-1.5 h-1.5 rounded-full bg-violet-400" />
                  <span>Fast-Path Hash Commit</span>
                </div>
                <div className="flex items-center gap-1.5 text-slate-300">
                  <span className="w-1.5 h-1.5 rounded-full bg-violet-400" />
                  <span>Zero-Readback Egress</span>
                </div>
              </div>
            </div>
          </div>

          {/* Bottom Security Banner */}
          <div className="mt-8 pt-6 border-t border-slate-800/80 flex flex-wrap items-center justify-between gap-4 text-xs font-mono text-slate-400">
            <div className="flex items-center gap-2 text-emerald-400">
              <ShieldCheck className="w-4 h-4" />
              <span>TLS 1.3 Direct Socket Stream</span>
            </div>
            <div className="flex items-center gap-2 text-cyan-400">
              <HardDrive className="w-4 h-4" />
              <span>0 Bytes Local Temp Disk Storage</span>
            </div>
            <div className="flex items-center gap-2 text-violet-400">
              <Lock className="w-4 h-4" />
              <span>Local-Only Credentials (.utp_*_token.json)</span>
            </div>
          </div>
        </div>
      </div>
    </section>
  );
}
