import React, { useState } from 'react';
import { Terminal, Copy, Check, ArrowRight, ShieldCheck, Zap, HardDrive, KeyRound, CheckCircle2 } from 'lucide-react';

const RUN_COMMANDS = [
  {
    id: 'npx',
    label: 'npx (zero-install)',
    cmd: 'npx utp-engine transfer',
    badge: 'Recommended',
    hint: 'Runs immediately without installation. Downloads the self-contained 1.18 MB engine directly into temporary memory.'
  },
  {
    id: 'npm',
    label: 'npm (global)',
    cmd: 'npm install -g utp-engine',
    badge: 'Permanent CLI',
    hint: 'Installs the global `utp` command in your terminal PATH for daily workflow and automation scripts.'
  },
  {
    id: 'pnpm',
    label: 'pnpm dlx',
    cmd: 'pnpm dlx utp-engine transfer',
    badge: 'Fastest',
    hint: 'Ultra-fast isolated execution via pnpm’s package runner.'
  },
  {
    id: 'direct',
    label: 'direct transfer',
    cmd: 'utp transfer gdrive://<file_id> onedrive://Backups/',
    badge: 'Scripting',
    hint: 'Pass source and destination URIs directly for non-interactive background pipelines.'
  }
];

const STEPS = [
  {
    num: '01',
    title: 'Run The Engine',
    badge: 'Instant Execution',
    icon: Terminal,
    code: 'npx utp-engine transfer',
    desc: 'Execute one command in your terminal. The self-contained C++20 engine initializes in RAM with zero prerequisite runtimes or compilers.',
    highlights: ['Zero installation required', '1.18 MB standalone binary', 'Instant launch']
  },
  {
    num: '02',
    title: 'Connect Your Cloud Accounts',
    badge: 'OAuth2 PKCE',
    icon: KeyRound,
    code: 'Browser popup ──▶ Google Drive / Microsoft 365 OAuth',
    desc: 'Authorize Google Drive or OneDrive with standard browser consent. Tokens are stored securely on your local machine only. Zero credentials touch external servers.',
    highlights: ['Local-only token storage (~/.utp_*)', 'Zero telemetry', 'Google API Limited Use verified']
  },
  {
    num: '03',
    title: 'Select Files Interactively',
    badge: 'Terminal TUI',
    icon: HardDrive,
    code: '↑/↓ Navigate  •  Enter Expand  •  Space Select',
    desc: 'Explore cloud files and folders in an interactive visual tree with breadcrumb trails, search filtering, and multi-file batch selection.',
    highlights: ['Folder creation & drill-down', 'Recursive batch queues', 'Keyboard shortcuts']
  },
  {
    num: '04',
    title: 'Zero-Disk RAM Streaming',
    badge: '378 MB/s Throughput',
    icon: Zap,
    code: 'Source Cloud ──[ Volatile RAM Ring Buffer ]──▶ Target Cloud',
    desc: 'Bytes stream directly between clouds through volatile circular memory buffers over TLS 1.3. Dual in-flight SHA-256 + MD5 checksums verify integrity before commit.',
    highlights: ['0 Bytes written to local SSD', 'Crash-resilient SQLite WAL ledger', 'Instant RAM wiping']
  }
];

const CHEATSHEET = [
  {
    title: 'Interactive Visual Mode',
    cmd: 'npx utp-engine transfer',
    desc: 'Launches full interactive cloud file explorer and transfer manager.'
  },
  {
    title: 'Direct Cloud-to-Cloud Stream',
    cmd: 'utp transfer gdrive://1GwVK67... onedrive://Backups/',
    desc: 'Streams a file directly between cloud endpoints with 0 disk writes.'
  },
  {
    title: 'Delta Folder Synchronization',
    cmd: 'utp sync file://./assets onedrive://Assets --policy newer_wins',
    desc: 'Reconciles folders; skips unchanged files with 0 bytes transferred.'
  },
  {
    title: 'Multi-Destination Fan-Out',
    cmd: 'utp route gdrive://data s3://bucket-us onedrive://backup',
    desc: 'Streams 1 source to multiple destinations simultaneously in a single read pass.'
  },
  {
    title: 'Smart Rules-Based Routing',
    cmd: 'utp route ./data --rule "ext=mp4:s3://media" --rule "ext=pdf:onedrive://docs"',
    desc: 'Routes items dynamically by extension, file size, or path pattern.'
  }
];

export default function QuickstartGuide() {
  const [activeTab, setActiveTab] = useState('npx');
  const [copiedId, setCopiedId] = useState(null);

  const selectedCommand = RUN_COMMANDS.find(c => c.id === activeTab) || RUN_COMMANDS[0];

  const handleCopy = (cmd, id) => {
    navigator.clipboard.writeText(cmd);
    setCopiedId(id);
    setTimeout(() => setCopiedId(null), 2000);
  };

  return (
    <section id="quickstart" className="py-20 border-b border-[#242220] bg-[#0A0908]">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        
        {/* Section Header */}
        <div className="flex flex-wrap items-center justify-between gap-3 pb-6 border-b border-[#242220] font-mono text-xs text-[#9E9B94] mb-12">
          <div className="flex items-center gap-2">
            <span className="live-beacon" />
            <span className="text-[#FF5B1F] font-semibold">// 02 / QUICKSTART & HOW-TO GUIDE</span>
            <span className="text-[#66635C]">—</span>
            <span>GET UP AND RUNNING IN SECONDS</span>
          </div>
          <div className="text-[#66635C]">
            [ZERO INSTALLATION · RUN VIA NPX]
          </div>
        </div>

        {/* ── PART 1: THE PRIMARY COMMAND (FRONT & CENTER) ── */}
        <div className="mb-16">
          <div className="max-w-3xl mb-6">
            <h2 className="text-3xl sm:text-4xl font-bold tracking-tight text-[#F5F4F0]">
              First, the command<span className="text-[#FF5B1F]">.</span>
            </h2>
            <p className="mt-3 text-sm sm:text-base text-[#9E9B94] leading-relaxed">
              No cloning repositories, no compiling C++ binaries, and no runtime setup. Just paste this command into your terminal:
            </p>
          </div>

          <div className="border border-[#242220] rounded-xl bg-[#121110] overflow-hidden shadow-2xl">
            {/* Command Selector Tabs */}
            <div className="flex items-center justify-between px-4 py-2.5 bg-[#0E0D0C] border-b border-[#242220] font-mono text-xs overflow-x-auto">
              <div className="flex items-center gap-2">
                <span className="text-[#66635C] mr-2">// SELECT RUNNER:</span>
                {RUN_COMMANDS.map(c => (
                  <button
                    key={c.id}
                    onClick={() => setActiveTab(c.id)}
                    className={`px-3 py-1.5 rounded transition-all cursor-pointer whitespace-nowrap flex items-center gap-2 ${
                      activeTab === c.id
                        ? 'bg-[#242220] text-[#FF5B1F] font-semibold border border-[#383531]'
                        : 'text-[#9E9B94] hover:text-[#F5F4F0]'
                    }`}
                  >
                    <span>{c.label}</span>
                    {c.badge && (
                      <span className={`text-[10px] px-1.5 py-0.2 rounded ${
                        activeTab === c.id
                          ? 'bg-[#FF5B1F]/20 text-[#FF5B1F]'
                          : 'bg-[#1C1A18] text-[#66635C]'
                      }`}>
                        {c.badge}
                      </span>
                    )}
                  </button>
                ))}
              </div>
            </div>

            {/* Live Command Line Display */}
            <div className="p-6 sm:p-8 flex flex-col md:flex-row md:items-center justify-between gap-6 bg-gradient-to-r from-[#141312] to-[#11100F]">
              <div className="flex items-center gap-4 font-mono text-base sm:text-xl overflow-x-auto text-[#F5F4F0]">
                <span className="text-[#FF5B1F] select-none font-bold text-2xl">❯</span>
                <span className="font-semibold text-white tracking-wide">{selectedCommand.cmd}</span>
              </div>

              <button
                onClick={() => handleCopy(selectedCommand.cmd, 'hero-cmd')}
                className="shrink-0 flex items-center justify-center gap-2 px-6 py-3 rounded-lg bg-[#FF5B1F] hover:bg-[#FF7036] text-black font-mono font-bold text-xs sm:text-sm transition-all shadow-lg shadow-[#FF5B1F]/10 cursor-pointer"
              >
                {copiedId === 'hero-cmd' ? (
                  <>
                    <Check className="w-4 h-4" />
                    <span>COPIED TO CLIPBOARD!</span>
                  </>
                ) : (
                  <>
                    <Copy className="w-4 h-4" />
                    <span>COPY COMMAND</span>
                  </>
                )}
              </button>
            </div>

            {/* Context Explainer */}
            <div className="px-6 py-3 bg-[#0E0D0C] border-t border-[#242220] flex flex-wrap items-center justify-between gap-3 font-mono text-xs text-[#9E9B94]">
              <div className="flex items-center gap-2">
                <span className="text-emerald-400">✔</span>
                <span>{selectedCommand.hint}</span>
              </div>
              <div className="text-[#66635C] hidden sm:block">
                Supports Windows, macOS & Linux
              </div>
            </div>
          </div>
        </div>

        {/* ── PART 2: STEP-BY-STEP HOW IT WORKS ("THEN ALL LIKE THAT") ── */}
        <div className="mb-16">
          <div className="max-w-3xl mb-8">
            <div className="text-xs font-mono text-[#FF5B1F] uppercase tracking-wider mb-2">// 4-STEP STREAMING FLOW</div>
            <h3 className="text-2xl sm:text-3xl font-bold tracking-tight text-[#F5F4F0]">
              How it works from start to finish<span className="text-[#FF5B1F]">.</span>
            </h3>
            <p className="mt-2 text-sm text-[#9E9B94]">
              Here is what happens behind the scenes when you launch a transfer:
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-6 font-mono text-xs">
            {STEPS.map((s, idx) => {
              const IconComp = s.icon;
              return (
                <div
                  key={idx}
                  className="border border-[#242220] rounded-xl bg-[#121110] p-6 hover:border-[#383531] transition-all flex flex-col justify-between group"
                >
                  <div>
                    {/* Top step tag */}
                    <div className="flex items-center justify-between mb-4">
                      <div className="flex items-center gap-2">
                        <span className="w-6 h-6 rounded-md bg-[#1C1A18] border border-[#282624] text-[#FF5B1F] flex items-center justify-center font-bold">
                          {s.num}
                        </span>
                        <span className="text-base font-bold text-[#F5F4F0] font-sans">
                          {s.title}
                        </span>
                      </div>
                      <span className="px-2 py-0.5 rounded bg-[#1C1A18] border border-[#282624] text-[#9E9B94] text-[11px]">
                        {s.badge}
                      </span>
                    </div>

                    {/* Code snippet display */}
                    <div className="mb-4 p-3 rounded-md bg-[#0A0908] border border-[#1E1C1A] text-[#38BDF8] text-[11px] overflow-x-auto truncate">
                      <code>{s.code}</code>
                    </div>

                    {/* Description */}
                    <p className="text-xs text-[#9E9B94] font-sans leading-relaxed mb-5">
                      {s.desc}
                    </p>
                  </div>

                  {/* Highlights list */}
                  <div className="pt-4 border-t border-[#1C1A18] space-y-1.5 text-[11px] text-[#66635C]">
                    {s.highlights.map((h, i) => (
                      <div key={i} className="flex items-center gap-2">
                        <span className="text-emerald-400">✓</span>
                        <span className="text-[#9E9B94]">{h}</span>
                      </div>
                    ))}
                  </div>
                </div>
              );
            })}
          </div>
        </div>

        {/* ── PART 3: COMMON WORKFLOWS & CLI CHEATSHEET ── */}
        <div className="border border-[#242220] rounded-xl bg-[#11100F] p-6 sm:p-8">
          <div className="flex flex-wrap items-center justify-between gap-3 mb-6 pb-4 border-b border-[#242220] font-mono text-xs">
            <div className="flex items-center gap-2">
              <span className="text-[#FF5B1F] font-bold">// COMMAND CHEATSHEET</span>
              <span className="text-[#66635C]">—</span>
              <span className="text-[#9E9B94]">SCRIPTABLE CLI EXAMPLES</span>
            </div>
            <a
              href="#simulator"
              className="text-[#FF5B1F] hover:text-[#FF7036] flex items-center gap-1 transition-colors"
            >
              <span>Test in Live Simulator</span>
              <ArrowRight className="w-3.5 h-3.5" />
            </a>
          </div>

          <div className="space-y-4 font-mono text-xs">
            {CHEATSHEET.map((item, idx) => (
              <div
                key={idx}
                className="p-3.5 rounded-lg bg-[#0E0D0C] border border-[#1E1C1A] hover:border-[#2C2926] transition-colors flex flex-col sm:flex-row sm:items-center justify-between gap-3"
              >
                <div className="space-y-1">
                  <div className="text-[#F5F4F0] font-semibold text-xs font-sans">
                    {item.title}
                  </div>
                  <div className="text-[#38BDF8] text-[11px] select-all">
                    $ {item.cmd}
                  </div>
                  <div className="text-[#66635C] text-[10px] font-sans">
                    {item.desc}
                  </div>
                </div>

                <button
                  onClick={() => handleCopy(item.cmd, `cheat-${idx}`)}
                  className="shrink-0 self-start sm:self-center px-3 py-1.5 rounded bg-[#181615] hover:bg-[#22201D] border border-[#2E2B27] text-[#9E9B94] hover:text-[#F5F4F0] transition-colors cursor-pointer flex items-center gap-1.5"
                >
                  {copiedId === `cheat-${idx}` ? (
                    <>
                      <Check className="w-3 h-3 text-emerald-400" />
                      <span className="text-emerald-400 text-[10px]">copied</span>
                    </>
                  ) : (
                    <>
                      <Copy className="w-3 h-3" />
                      <span className="text-[10px]">copy</span>
                    </>
                  )}
                </button>
              </div>
            ))}
          </div>
        </div>

      </div>
    </section>
  );
}
