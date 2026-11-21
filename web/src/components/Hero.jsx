import React, { useState } from 'react';
import { Terminal, Copy, Check, ArrowDownRight, ArrowRight } from 'lucide-react';

const INSTALL_OPTIONS = [
  { id: 'npx', label: 'npx (zero-install)', cmd: 'npx utp-engine transfer' },
  { id: 'npm', label: 'npm (global)', cmd: 'npm install -g utp-engine' },
  { id: 'pnpm', label: 'pnpm dlx', cmd: 'pnpm dlx utp-engine transfer' },
  { id: 'curl', label: 'curl | bash', cmd: 'curl -fsSL https://raw.githubusercontent.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/PROJECTS/install.sh | bash' },
];

export default function Hero({ onOpenLegal }) {
  const [activeTab, setActiveTab] = useState('npx');
  const [copied, setCopied] = useState(false);

  const activeCmd = INSTALL_OPTIONS.find(t => t.id === activeTab)?.cmd || INSTALL_OPTIONS[0].cmd;

  const handleCopy = () => {
    navigator.clipboard.writeText(activeCmd);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <section className="pt-28 pb-16 md:pt-36 md:pb-20 border-b border-[#242220]">
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
            <span>0 BYTES LOCAL DISK WEAR</span>
            <span>•</span>
            <span>TLS 1.3 DIRECT SOCKETS</span>
          </div>
        </div>

        {/* Hero Title & Subtitle */}
        <div className="py-12 md:py-16">
          <h1 className="text-4xl sm:text-6xl md:text-7xl font-bold tracking-tight text-[#F5F4F0] leading-[1.08] max-w-5xl">
            Direct memory transport for personal and enterprise cloud<span className="text-[#FF5B1F]">.</span>
          </h1>

          <p className="mt-6 text-lg sm:text-xl text-[#9E9B94] max-w-3xl leading-relaxed">
            Why download 100GB to your local hard drive just to re-upload it? UTP streams bytes directly from cloud to cloud through <span className="text-[#F5F4F0] font-medium">volatile RAM ring buffers</span> over encrypted TLS 1.3 tunnels. <span className="text-[#FF5B1F] font-mono text-sm">[0 bytes written to local disk]</span>
          </p>
        </div>

        {/* Streamlined Command Box */}
        <div className="border border-[#242220] rounded-lg bg-[#141312] overflow-hidden">
          {/* Tabs */}
          <div className="flex items-center justify-between px-4 py-2.5 border-b border-[#242220] bg-[#0E0D0C] font-mono text-xs">
            <div className="flex items-center gap-2">
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

            <a
              href="#quickstart"
              className="hidden sm:flex items-center gap-1 text-[#FF5B1F] hover:text-[#FF7036] transition-colors"
            >
              <span>how to run & workflow</span>
              <span>↓</span>
            </a>
          </div>

          {/* Snippet Line */}
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

        {/* Minimal Metric Bar */}
        <div className="mt-8 grid grid-cols-2 md:grid-cols-4 gap-px bg-[#242220] border border-[#242220] rounded-lg overflow-hidden font-mono text-xs">
          <div className="bg-[#121110] p-3 text-center">
            <div className="text-xl font-bold text-[#FF5B1F]">0 Bytes</div>
            <div className="text-[#66635C] text-[10px] uppercase tracking-wider mt-0.5">Local Disk Wear</div>
          </div>
          <div className="bg-[#121110] p-3 text-center">
            <div className="text-xl font-bold text-[#F5F4F0]">10 Clouds</div>
            <div className="text-[#66635C] text-[10px] uppercase tracking-wider mt-0.5">Unified Ecosystem</div>
          </div>
          <div className="bg-[#121110] p-3 text-center">
            <div className="text-xl font-bold text-emerald-400">TLS 1.3</div>
            <div className="text-[#66635C] text-[10px] uppercase tracking-wider mt-0.5">Line-Rate Socket</div>
          </div>
          <div className="bg-[#121110] p-3 text-center">
            <div className="text-xl font-bold text-[#9E9B94]">100%</div>
            <div className="text-[#66635C] text-[10px] uppercase tracking-wider mt-0.5">Zero-Knowledge</div>
          </div>
        </div>
      </div>
    </section>
  );
}
