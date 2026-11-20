import React, { useState } from 'react';
import { Terminal, Copy, Check, ArrowRight, ArrowDownRight, HardDrive, ShieldCheck, Zap } from 'lucide-react';

const INSTALL_OPTIONS = [
  { id: 'curl', label: '1-Line cURL', cmd: 'curl -fsSL https://raw.githubusercontent.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/PROJECTS/install.sh | bash' },
  { id: 'npx', label: 'npx instant run', cmd: 'npx utp-engine transfer' },
  { id: 'brew', label: 'homebrew', cmd: 'brew install ARPIT-NAYAK-LEGEND/utp/utp' },
  { id: 'scoop', label: 'scoop (win)', cmd: 'scoop install https://raw.githubusercontent.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/PROJECTS/packaging/scoop/utp.json' },
  { id: 'npm', label: 'npm global', cmd: 'npm install -g utp-engine' },
];

export default function Hero({ onOpenLegal }) {
  const [activeTab, setActiveTab] = useState('curl');
  const [copied, setCopied] = useState(false);

  const activeCmd = INSTALL_OPTIONS.find(t => t.id === activeTab)?.cmd || INSTALL_OPTIONS[0].cmd;

  const handleCopy = () => {
    navigator.clipboard.writeText(activeCmd);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <section className="pt-28 pb-16 md:pt-36 md:pb-24 border-b border-[#242220]">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        {/* Meta Header Tag */}
        <div className="flex flex-wrap items-center justify-between gap-3 pb-6 border-b border-[#242220] font-mono text-xs text-[#9E9B94]">
          <div className="flex items-center gap-2">
            <span className="live-beacon-orange" />
            <span className="text-[#FF5B1F] font-semibold">// 01 / SPECIFICATION</span>
            <span className="text-[#66635C]">—</span>
            <span>C++20 HIGH-PERFORMANCE CLOUD TRANSPORT</span>
          </div>
          <div className="flex items-center gap-3 text-[#66635C]">
            <span>ZERO DISK WEAR</span>
            <span>•</span>
            <span>END-TO-END TLS 1.3</span>
          </div>
        </div>

        {/* Hero Title & Subtitle */}
        <div className="py-12 md:py-16">
          <h1 className="text-4xl sm:text-6xl md:text-7xl font-bold tracking-tight text-[#F5F4F0] leading-[1.08] max-w-5xl">
            Direct memory transport for personal and enterprise cloud<span className="text-[#FF5B1F]">.</span>
          </h1>

          <p className="mt-8 text-lg sm:text-xl text-[#9E9B94] max-w-3xl leading-relaxed">
            Why download 100GB to your local hard drive just to re-upload it? UTP streams bytes <em className="text-[#F5F4F0] not-italic font-medium underline decoration-[#FF5B1F]/60 decoration-2 underline-offset-4">directly cloud-to-cloud through volatile RAM ring buffers</em> over encrypted TLS 1.3 tunnels. <span className="text-[#F5F4F0] font-medium">0 bytes written to local disk.</span>
          </p>
        </div>

        {/* Paradigm Comparison Block: The Old Way vs The CTP Way */}
        <div className="grid grid-cols-1 md:grid-cols-2 gap-px bg-[#242220] border border-[#242220] rounded-lg overflow-hidden my-6 font-mono text-xs">
          {/* Left: The Old Way */}
          <div className="bg-[#121110] p-6 space-y-4">
            <div className="text-[#66635C] flex items-center justify-between pb-3 border-b border-[#242220]">
              <span>// TRADITIONAL CLOUD MIGRATION (Rclone / Cyberduck / Web UI)</span>
              <span className="text-rose-400 font-semibold">[HIGH DISK WEAR]</span>
            </div>
            <div className="space-y-2 text-[#9E9B94]">
              <div className="flex items-center gap-2 text-rose-300/80">
                <span>✗</span>
                <span>Downloads 100GB file directly to local NVMe / SSD temp folder</span>
              </div>
              <div className="flex items-center gap-2 text-rose-300/80">
                <span>✗</span>
                <span>Exhausts local disk space; fails immediately if drive has &lt;100GB free</span>
              </div>
              <div className="flex items-center gap-2 text-rose-300/80">
                <span>✗</span>
                <span>Doubles time: 100% download finished BEFORE upload even begins</span>
              </div>
              <div className="flex items-center gap-2 text-rose-300/80">
                <span>✗</span>
                <span>Heavy NAND flash write fatigue on laptops and development machines</span>
              </div>
            </div>
          </div>

          {/* Right: The CTP Zero-Disk Way */}
          <div className="bg-[#151413] p-6 space-y-4">
            <div className="text-[#FF5B1F] flex items-center justify-between pb-3 border-b border-[#242220]">
              <span>// UTP ZERO-DISK STREAMING ENGINE</span>
              <span className="text-emerald-400 font-semibold">[0 BYTES DISK I/O]</span>
            </div>
            <div className="space-y-2 text-[#F5F4F0]">
              <div className="flex items-center gap-2 text-emerald-300">
                <span>✓</span>
                <span>Direct volatile RAM ring buffer (adaptive 2.5–40MB streaming slices)</span>
              </div>
              <div className="flex items-center gap-2 text-emerald-300">
                <span>✓</span>
                <span>Upload begins concurrently while download chunk streams in</span>
              </div>
              <div className="flex items-center gap-2 text-emerald-300">
                <span>✓</span>
                <span>Fast-path server-side SHA-256 match (zero cloud readback egress)</span>
              </div>
              <div className="flex items-center gap-2 text-emerald-300">
                <span>✓</span>
                <span>Crash-resilient SQLite WAL ledger (utp.db) for instant resumption</span>
              </div>
            </div>
          </div>
        </div>

        {/* Command Runner & Installation Box */}
        <div className="mt-8 border border-[#242220] rounded-lg bg-[#141312] overflow-hidden">
          {/* Tabs */}
          <div className="flex items-center gap-2 px-4 py-3 border-b border-[#242220] bg-[#0E0D0C] overflow-x-auto font-mono text-xs">
            <span className="text-[#66635C] mr-2">// INSTALL:</span>
            {INSTALL_OPTIONS.map(opt => (
              <button
                key={opt.id}
                onClick={() => setActiveTab(opt.id)}
                className={`px-3 py-1 rounded transition-colors cursor-pointer ${
                  activeTab === opt.id
                    ? 'bg-[#242220] text-[#FF5B1F] font-semibold'
                    : 'text-[#9E9B94] hover:text-[#F5F4F0]'
                }`}
              >
                {opt.label}
              </button>
            ))}
          </div>

          {/* Command Snippet */}
          <div className="p-4 sm:p-5 flex items-center justify-between gap-4 font-mono text-xs sm:text-sm">
            <div className="flex items-center gap-3 overflow-x-auto text-[#F5F4F0]">
              <span className="text-[#FF5B1F] select-none font-bold">$</span>
              <span className="truncate">{activeCmd}</span>
            </div>
            <button
              onClick={handleCopy}
              className="shrink-0 flex items-center gap-1.5 px-3 py-1.5 rounded bg-[#1C1A18] hover:bg-[#282522] border border-[#33302C] text-[#F5F4F0] font-mono text-xs transition-colors cursor-pointer"
            >
              {copied ? (
                <>
                  <Check className="w-3.5 h-3.5 text-emerald-400" />
                  <span className="text-emerald-400">copied</span>
                </>
              ) : (
                <>
                  <Copy className="w-3.5 h-3.5 text-[#9E9B94]" />
                  <span>copy</span>
                </>
              )}
            </button>
          </div>
        </div>

        {/* Technical Specification Ticker */}
        <div className="mt-12 grid grid-cols-2 md:grid-cols-4 gap-px bg-[#242220] border border-[#242220] rounded-lg overflow-hidden font-mono text-xs">
          <div className="bg-[#121110] p-4 text-center">
            <div className="text-2xl font-bold text-[#FF5B1F]">0 Bytes</div>
            <div className="text-[#66635C] mt-1 uppercase tracking-wider">Local Disk Write</div>
          </div>
          <div className="bg-[#121110] p-4 text-center">
            <div className="text-2xl font-bold text-[#F5F4F0]">10 Clouds</div>
            <div className="text-[#66635C] mt-1 uppercase tracking-wider">Native Ecosystem</div>
          </div>
          <div className="bg-[#121110] p-4 text-center">
            <div className="text-2xl font-bold text-emerald-400">TLS 1.3</div>
            <div className="text-[#66635C] mt-1 uppercase tracking-wider">Direct Socket Stream</div>
          </div>
          <div className="bg-[#121110] p-4 text-center">
            <div className="text-2xl font-bold text-[#9E9B94]">100%</div>
            <div className="text-[#66635C] mt-1 uppercase tracking-wider">Zero-Knowledge Privacy</div>
          </div>
        </div>
      </div>
    </section>
  );
}
