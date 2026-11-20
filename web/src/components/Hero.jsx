import React, { useState } from 'react';
import { Terminal, Copy, Check, ArrowRight, ShieldCheck, Zap, Server, HardDrive, Download, ChevronRight } from 'lucide-react';

const INSTALL_TABS = [
  { id: 'curl', label: '1-Line cURL', cmd: 'curl -fsSL https://raw.githubusercontent.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/PROJECTS/install.sh | bash' },
  { id: 'npx', label: 'npx (Instant Run)', cmd: 'npx utp-engine transfer' },
  { id: 'brew', label: 'Homebrew', cmd: 'brew install ARPIT-NAYAK-LEGEND/utp/utp' },
  { id: 'scoop', label: 'Scoop (Win)', cmd: 'scoop install https://raw.githubusercontent.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/PROJECTS/packaging/scoop/utp.json' },
  { id: 'npm', label: 'npm global', cmd: 'npm install -g utp-engine' },
];

export default function Hero({ onOpenLegal }) {
  const [activeTab, setActiveTab] = useState('curl');
  const [copied, setCopied] = useState(false);

  const activeCmd = INSTALL_TABS.find(t => t.id === activeTab)?.cmd || INSTALL_TABS[0].cmd;

  const handleCopy = () => {
    navigator.clipboard.writeText(activeCmd);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <section className="relative pt-32 pb-20 md:pt-40 md:pb-28 overflow-hidden">
      {/* Background radial glow */}
      <div className="absolute top-1/4 left-1/2 -translate-x-1/2 -translate-y-1/2 w-[800px] h-[500px] bg-gradient-to-tr from-cyan-600/15 via-sky-600/10 to-violet-600/15 blur-[120px] pointer-events-none rounded-full" />
      
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 relative z-10 text-center">
        {/* Release Pill */}
        <div className="inline-flex items-center gap-2 px-3 py-1.5 rounded-full bg-slate-900/90 border border-cyan-500/30 text-xs font-mono text-cyan-300 mb-8 backdrop-blur-md shadow-lg shadow-cyan-950/40">
          <span className="w-2 h-2 rounded-full bg-cyan-400 animate-pulse" />
          <span className="font-semibold text-white">v2.6.0-pro Released</span>
          <span className="text-slate-500">•</span>
          <span>Zero-Disk RAM Streaming Pipeline</span>
          <ChevronRight className="w-3.5 h-3.5 text-cyan-400" />
        </div>

        {/* Hero Title */}
        <h1 className="text-4xl sm:text-6xl lg:text-7xl font-extrabold tracking-tight text-white max-w-5xl mx-auto leading-[1.12]">
          The Direct <span className="text-transparent bg-clip-text bg-gradient-to-r from-cyan-400 via-sky-300 to-violet-400">Zero-Disk</span> Cloud Data Transport Engine.
        </h1>

        {/* Subtitle */}
        <p className="mt-6 text-lg sm:text-xl text-slate-300 max-w-3xl mx-auto font-normal leading-relaxed">
          Migrate and sync 100GB+ datasets directly between <span className="text-white font-medium">Google Drive</span>, <span className="text-white font-medium">Microsoft OneDrive</span>, <span className="text-white font-medium">Amazon S3</span>, <span className="text-white font-medium">Cloudflare R2</span>, and <span className="text-white font-medium">WebDAV</span> through volatile RAM over TLS 1.3 tunnels. <span className="text-cyan-400 font-semibold">0 bytes written to local disk.</span>
        </p>

        {/* Action Buttons */}
        <div className="mt-10 flex flex-wrap items-center justify-center gap-4">
          <a
            href="#terminal-demo"
            className="flex items-center gap-2.5 px-6 py-3.5 rounded-xl bg-gradient-to-r from-cyan-500 to-sky-600 hover:from-cyan-400 hover:to-sky-500 text-white font-semibold text-sm shadow-xl shadow-cyan-500/25 transition-all hover:shadow-cyan-500/40 hover:-translate-y-0.5"
          >
            <Terminal className="w-4 h-4" />
            <span>Launch Visual TUI</span>
            <ArrowRight className="w-4 h-4 ml-1" />
          </a>

          <a
            href="https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/releases"
            target="_blank"
            rel="noopener noreferrer"
            className="flex items-center gap-2 px-6 py-3.5 rounded-xl bg-slate-900/90 hover:bg-slate-800 border border-slate-700/80 text-white font-semibold text-sm transition-all hover:border-slate-600 hover:-translate-y-0.5"
          >
            <Download className="w-4 h-4 text-slate-400" />
            <span>Download Binaries (Win/Mac/Linux)</span>
          </a>
        </div>

        {/* Interactive CLI Install Box */}
        <div className="mt-12 max-w-3xl mx-auto text-left">
          <div className="rounded-2xl bg-slate-950/90 border border-slate-800 shadow-2xl backdrop-blur-xl overflow-hidden">
            {/* Tabs */}
            <div className="flex items-center gap-1 px-3 pt-3 pb-2 border-b border-slate-800/80 overflow-x-auto bg-slate-900/40">
              {INSTALL_TABS.map(tab => (
                <button
                  key={tab.id}
                  onClick={() => setActiveTab(tab.id)}
                  className={`px-3 py-1.5 rounded-lg text-xs font-mono font-medium transition-colors cursor-pointer ${
                    activeTab === tab.id
                      ? 'bg-cyan-500/15 text-cyan-300 border border-cyan-500/30'
                      : 'text-slate-400 hover:text-slate-200 hover:bg-slate-800/60 border border-transparent'
                  }`}
                >
                  {tab.label}
                </button>
              ))}
            </div>

            {/* Code Box */}
            <div className="p-4 sm:p-5 flex items-center justify-between gap-4 font-mono text-xs sm:text-sm">
              <div className="flex items-center gap-3 overflow-x-auto text-slate-200">
                <span className="text-cyan-500 font-bold select-none">$</span>
                <span className="truncate">{activeCmd}</span>
              </div>
              <button
                onClick={handleCopy}
                className="shrink-0 p-2.5 rounded-lg bg-slate-800/90 hover:bg-slate-700 text-slate-300 hover:text-white border border-slate-700 transition-colors cursor-pointer flex items-center gap-1.5 text-xs"
                title="Copy to clipboard"
              >
                {copied ? (
                  <>
                    <Check className="w-4 h-4 text-emerald-400" />
                    <span className="text-emerald-400 font-sans font-medium">Copied</span>
                  </>
                ) : (
                  <>
                    <Copy className="w-4 h-4 text-slate-400" />
                    <span className="font-sans font-medium">Copy</span>
                  </>
                )}
              </button>
            </div>
          </div>
        </div>

        {/* Metrics Banner */}
        <div className="mt-16 grid grid-cols-2 md:grid-cols-4 gap-4 max-w-4xl mx-auto pt-8 border-t border-slate-800/60">
          <div className="p-4 rounded-xl bg-slate-900/40 border border-slate-800/50 text-center">
            <div className="text-2xl sm:text-3xl font-extrabold text-cyan-400 font-mono">0 Bytes</div>
            <div className="text-xs text-slate-400 mt-1 uppercase tracking-wider font-medium">Local Disk Wear</div>
          </div>
          <div className="p-4 rounded-xl bg-slate-900/40 border border-slate-800/50 text-center">
            <div className="text-2xl sm:text-3xl font-extrabold text-violet-400 font-mono">10 Clouds</div>
            <div className="text-xs text-slate-400 mt-1 uppercase tracking-wider font-medium">Unified Ecosystem</div>
          </div>
          <div className="p-4 rounded-xl bg-slate-900/40 border border-slate-800/50 text-center">
            <div className="text-2xl sm:text-3xl font-extrabold text-emerald-400 font-mono">TLS 1.3</div>
            <div className="text-xs text-slate-400 mt-1 uppercase tracking-wider font-medium">Direct Tunnel Stream</div>
          </div>
          <div className="p-4 rounded-xl bg-slate-900/40 border border-slate-800/50 text-center">
            <div className="text-2xl sm:text-3xl font-extrabold text-sky-400 font-mono">100%</div>
            <div className="text-xs text-slate-400 mt-1 uppercase tracking-wider font-medium">Zero-Knowledge Privacy</div>
          </div>
        </div>

      </div>
    </section>
  );
}
