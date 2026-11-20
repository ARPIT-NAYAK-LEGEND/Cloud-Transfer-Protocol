import React from 'react';
import { Server, Cpu, HardDrive, ShieldCheck, Lock, ArrowRight } from 'lucide-react';

export default function ArchitectureDiagram() {
  return (
    <section id="architecture" className="py-20 border-b border-[#242220]">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        {/* Section Header */}
        <div className="flex flex-wrap items-center justify-between gap-3 pb-6 border-b border-[#242220] font-mono text-xs text-[#9E9B94] mb-12">
          <div className="flex items-center gap-2">
            <span className="text-[#FF5B1F] font-semibold">// 05 / TECHNICAL SPECIFICATION</span>
            <span className="text-[#66635C]">—</span>
            <span>DIRECT VOLATILE RAM STREAMING PIPELINE</span>
          </div>
          <div className="text-[#66635C]">
            [ZERO DISK RESIDUE]
          </div>
        </div>

        {/* Technical Wireframe Frame */}
        <div className="border border-[#242220] rounded-lg bg-[#11100F] p-8">
          <div className="grid grid-cols-1 md:grid-cols-3 gap-6 items-stretch">
            {/* Step 1: Ingress */}
            <div className="p-6 rounded border border-[#242220] bg-[#0E0D0C] flex flex-col justify-between font-mono text-xs">
              <div>
                <div className="text-[#66635C] flex items-center justify-between pb-3 border-b border-[#242220] mb-4">
                  <span>// SOURCE INGRESS</span>
                  <span className="text-[#FF5B1F]">[STAGE 01]</span>
                </div>
                <h4 className="font-bold text-base text-[#F5F4F0] mb-2">HTTP Range Stream</h4>
                <p className="text-[#9E9B94] text-xs leading-relaxed font-sans mb-4">
                  Pulls adaptive byte slices (2.5MB - 40MB) directly from source cloud over TLS 1.3 encrypted sockets.
                </p>
                <div className="space-y-1.5 text-[#66635C]">
                  <div>• TLS 1.3 socket negotiation</div>
                  <div>• Resumable Range: bytes=X-Y</div>
                  <div>• Google Drive / S3 / R2 / WebDAV</div>
                </div>
              </div>
              <div className="mt-6 pt-3 border-t border-[#242220] text-[#FF5B1F]">
                Stream Ingress ──▶
              </div>
            </div>

            {/* Step 2: In-Memory Ring Buffer (Center Highlight) */}
            <div className="p-6 rounded border border-[#FF5B1F]/40 bg-[#161413] flex flex-col justify-between font-mono text-xs relative">
              <div className="absolute -top-3 left-4 px-2 py-0.5 rounded bg-[#FF5B1F] text-[#0E0D0C] font-bold text-[10px] uppercase">
                Zero-Disk RAM Ring
              </div>
              <div>
                <div className="text-[#FF5B1F] flex items-center justify-between pb-3 border-b border-[#2D2A27] mb-4">
                  <span>// MEMORY ENGINE</span>
                  <span>[STAGE 02]</span>
                </div>
                <h4 className="font-bold text-base text-[#F5F4F0] mb-2">Volatile Ring Buffer</h4>
                <p className="text-[#9E9B94] text-xs leading-relaxed font-sans mb-4">
                  Never touches storage drive. Concurrently runs in-flight SHA-256 / MD5 digest streams while relaying bytes.
                </p>
                <div className="space-y-2">
                  <div className="p-2 rounded bg-[#0E0D0C] border border-[#242220]">
                    <div className="text-[#FF5B1F] font-semibold">Dual In-Flight Digest</div>
                    <div className="text-[#66635C] text-[11px] font-sans">Simultaneous SHA-256 + MD5</div>
                  </div>
                  <div className="p-2 rounded bg-[#0E0D0C] border border-[#242220]">
                    <div className="text-emerald-400 font-semibold">SQLite WAL Ledger</div>
                    <div className="text-[#66635C] text-[11px] font-sans">ACID crash recovery state</div>
                  </div>
                </div>
              </div>
              <div className="mt-6 pt-3 border-t border-[#2D2A27] text-emerald-400">
                0 Bytes Disk I/O ──▶
              </div>
            </div>

            {/* Step 3: Destination Egress */}
            <div className="p-6 rounded border border-[#242220] bg-[#0E0D0C] flex flex-col justify-between font-mono text-xs">
              <div>
                <div className="text-[#66635C] flex items-center justify-between pb-3 border-b border-[#242220] mb-4">
                  <span>// TARGET EGRESS</span>
                  <span className="text-[#FF5B1F]">[STAGE 03]</span>
                </div>
                <h4 className="font-bold text-base text-[#F5F4F0] mb-2">Chunked Upload Session</h4>
                <p className="text-[#9E9B94] text-xs leading-relaxed font-sans mb-4">
                  Writes bytes directly to cloud API sessions. Server response hash matches memory digest with zero read-back.
                </p>
                <div className="space-y-1.5 text-[#66635C]">
                  <div>• Microsoft Graph / S3 Multipart</div>
                  <div>• Fast-path server checksum match</div>
                  <div>• Verified & committed to ledger</div>
                </div>
              </div>
              <div className="mt-6 pt-3 border-t border-[#242220] text-emerald-400">
                ✓ Transfer Committed
              </div>
            </div>
          </div>
        </div>
      </div>
    </section>
  );
}
