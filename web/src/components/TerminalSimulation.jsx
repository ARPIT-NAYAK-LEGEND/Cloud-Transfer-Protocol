import React, { useState, useEffect } from 'react';
import { Play, Pause, RotateCcw, Terminal as TerminalIcon, CheckCircle2, ShieldCheck, Cpu, HardDrive } from 'lucide-react';

const SCENARIOS = [
  {
    id: 'transfer',
    title: 'Cloud-to-Cloud RAM Transfer',
    cmd: 'utp transfer gdrive://1GwVK676IBt... onedrive://Backups/dataset.tar.gz',
    pipeline: 'gdrive://1GwVK676IBt10iBEdRzh ──▶ onedrive://Backups/dataset.tar.gz',
    totalSize: '2.40 GB',
    totalBytes: 2576980377,
    chunksTotal: 16,
    speed: '142.8 MB/s',
    integrity: 'SHA-256 Cloud Fast-Path (OneDrive sha256Hash matched)',
    steps: [
      { progress: 0.12, bytes: '309 MB', chunk: '2/16', log: 'Allocating volatile RAM ring buffer (32 MB chunks)...' },
      { progress: 0.35, bytes: '901 MB', chunk: '6/16', log: 'Streaming HTTP Range chunks over TLS 1.3 socket...' },
      { progress: 0.68, bytes: '1.63 GB', chunk: '11/16', log: 'Dual in-flight digest pipeline calculating SHA-256 + MD5...' },
      { progress: 0.94, bytes: '2.25 GB', chunk: '15/16', log: 'Writing chunked session directly to Microsoft Graph API...' },
      { progress: 1.00, bytes: '2.40 GB', chunk: '16/16', log: 'Verified server-side SHA-256 hash. 0 Bytes written to local disk!' }
    ]
  },
  {
    id: 'sync',
    title: 'Delta Folder Synchronization',
    cmd: 'utp sync file://./production_assets onedrive://Assets --policy newer_wins',
    pipeline: 'file://production_assets ──▶ onedrive://Assets (Bidirectional)',
    totalSize: '4.85 GB',
    totalBytes: 5208442010,
    chunksTotal: 32,
    speed: '188.4 MB/s',
    integrity: 'Incremental Reconciliation Matrix (0 Bytes for Unchanged)',
    steps: [
      { progress: 0.20, bytes: 'Scanning...', chunk: 'Planning', log: 'Scanning local tree (148 files) vs remote tree...' },
      { progress: 0.50, bytes: '0 B Delta', chunk: 'Skip 142', log: 'Detected 142 unchanged files — SKIPPED (0 bytes transferred).' },
      { progress: 0.85, bytes: '320 MB', chunk: '6 to update', log: 'Synchronizing 6 delta updates via policy NEWER_WINS...' },
      { progress: 1.00, bytes: '320 MB', chunk: 'Complete', log: 'Delta sync complete: 142 unchanged skipped, 6 updated successfully.' }
    ]
  },
  {
    id: 'route',
    title: 'Rules-Based Smart Routing',
    cmd: 'utp route ./raw_data --rule "ext=mp4:s3://media" --rule "ext=pdf:onedrive://docs"',
    pipeline: 'file://raw_data ──▶ [S3 Media Vault, OneDrive Documents, B2 Cold Archive]',
    totalSize: '12.6 GB',
    totalBytes: 13529146982,
    chunksTotal: 48,
    speed: '210.5 MB/s',
    integrity: 'Simultaneous Multi-Destination Fan-Out Stream',
    steps: [
      { progress: 0.15, bytes: 'Routing...', chunk: 'Rule Eval', log: 'Evaluating rule engine: 3 rules configured + 1 default fallback.' },
      { progress: 0.45, bytes: '8.4 GB', chunk: 'ext=mp4', log: 'Routing 4 .mp4 video files ──▶ s3://media-vault/ (Direct S3 Multipart)' },
      { progress: 0.80, bytes: '2.1 GB', chunk: 'ext=pdf', log: 'Routing 18 .pdf documents ──▶ onedrive://docs/ (Chunked Session)' },
      { progress: 1.00, bytes: '12.6 GB', chunk: 'All routed', log: 'Fan-out execution complete: 100% matched rules routed in 1 read pass.' }
    ]
  }
];

export default function TerminalSimulation() {
  const [activeScenarioId, setActiveScenarioId] = useState('transfer');
  const [stepIndex, setStepIndex] = useState(0);
  const [isPlaying, setIsPlaying] = useState(true);

  const scenario = SCENARIOS.find(s => s.id === activeScenarioId) || SCENARIOS[0];

  useEffect(() => {
    setStepIndex(0);
    setIsPlaying(true);
  }, [activeScenarioId]);

  useEffect(() => {
    if (!isPlaying) return;
    const interval = setInterval(() => {
      setStepIndex(prev => {
        if (prev < scenario.steps.length - 1) return prev + 1;
        return prev;
      });
    }, 1800);
    return () => clearInterval(interval);
  }, [isPlaying, scenario.steps.length]);

  const currentStep = scenario.steps[stepIndex] || scenario.steps[0];
  const progressPercent = Math.round(currentStep.progress * 100);

  // Render gradient bar representation
  const barBlocks = 24;
  const filledBlocks = Math.round(barBlocks * currentStep.progress);

  return (
    <section id="terminal-demo" className="py-20 relative">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="text-center max-w-3xl mx-auto mb-12">
          <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-cyan-500/10 text-cyan-400 border border-cyan-500/20 text-xs font-mono font-medium mb-3">
            <TerminalIcon className="w-3.5 h-3.5" />
            Live Interactive Simulator
          </div>
          <h2 className="text-3xl sm:text-4xl font-extrabold text-white tracking-tight">
            See the Zero-Disk Memory Pipeline in Action
          </h2>
          <p className="mt-4 text-slate-400 text-base">
            Watch how UTP routes chunks directly from cloud socket to cloud socket using adaptive volatile RAM buffers without ever creating temporary files.
          </p>
        </div>

        {/* Terminal Window Container */}
        <div className="max-w-4xl mx-auto rounded-2xl bg-[#0c1017] border border-slate-800 shadow-2xl shadow-cyan-950/30 overflow-hidden font-mono text-xs sm:text-sm">
          {/* Top Title Bar */}
          <div className="flex items-center justify-between px-4 py-3 bg-[#080b11] border-b border-slate-800/80">
            {/* Window Controls */}
            <div className="flex items-center gap-2">
              <span className="w-3 h-3 rounded-full bg-rose-500/80 inline-block" />
              <span className="w-3 h-3 rounded-full bg-amber-500/80 inline-block" />
              <span className="w-3 h-3 rounded-full bg-emerald-500/80 inline-block" />
              <span className="ml-3 text-slate-400 text-xs font-mono hidden sm:inline">
                utp-terminal-engine — v2.6.0-pro (x86_64)
              </span>
            </div>

            {/* Playback Controls */}
            <div className="flex items-center gap-2">
              <button
                onClick={() => setIsPlaying(!isPlaying)}
                className="p-1.5 rounded-md hover:bg-slate-800 text-slate-300 hover:text-white transition-colors cursor-pointer"
                title={isPlaying ? 'Pause simulation' : 'Play simulation'}
              >
                {isPlaying ? <Pause className="w-3.5 h-3.5" /> : <Play className="w-3.5 h-3.5 text-cyan-400" />}
              </button>
              <button
                onClick={() => setStepIndex(0)}
                className="p-1.5 rounded-md hover:bg-slate-800 text-slate-300 hover:text-white transition-colors cursor-pointer"
                title="Restart simulation"
              >
                <RotateCcw className="w-3.5 h-3.5" />
              </button>
            </div>
          </div>

          {/* Scenario Tab Bar */}
          <div className="flex items-center gap-1 px-4 py-2 bg-slate-900/60 border-b border-slate-800/60 overflow-x-auto">
            {SCENARIOS.map(s => (
              <button
                key={s.id}
                onClick={() => setActiveScenarioId(s.id)}
                className={`px-3 py-1.5 rounded-lg text-xs font-mono transition-all cursor-pointer whitespace-nowrap ${
                  activeScenarioId === s.id
                    ? 'bg-cyan-500/20 text-cyan-300 border border-cyan-500/40 font-semibold'
                    : 'text-slate-400 hover:text-slate-200 hover:bg-slate-800/50 border border-transparent'
                }`}
              >
                {s.title}
              </button>
            ))}
          </div>

          {/* Terminal Console Output */}
          <div className="p-6 space-y-4 text-slate-200 bg-[#0a0e14]">
            {/* Command Prompt */}
            <div className="flex items-center gap-2 text-cyan-400 font-bold">
              <span className="text-emerald-400">arpit@utp-workstation</span>
              <span className="text-slate-500">:</span>
              <span className="text-sky-400">~</span>
              <span className="text-slate-500">$</span>
              <span className="text-white font-normal">{scenario.cmd}</span>
            </div>

            {/* ASCII Engine Banner */}
            <div className="text-slate-400 text-xs sm:text-sm pl-2 border-l-2 border-cyan-500/50 space-y-1">
              <div>
                <span className="text-violet-400 font-bold">▲ UTP Engine</span>
                <span className="text-slate-500"> · </span>
                <span className="text-slate-300">Universal Cloud Transfer Protocol</span>
                <span className="text-cyan-400 font-mono ml-2">v2.6.0-pro</span>
              </div>
              <div className="text-slate-400 text-xs">
                Direct zero-disk memory streaming · End-to-end TLS 1.3 socket tunnel
              </div>
            </div>

            {/* Pipeline Info */}
            <div className="bg-slate-950/80 p-3.5 rounded-xl border border-slate-800/80 space-y-2 text-xs">
              <div className="flex flex-wrap items-center justify-between gap-2">
                <span className="text-slate-400">Pipeline Route:</span>
                <span className="text-cyan-300 font-medium">{scenario.pipeline}</span>
              </div>
              <div className="flex flex-wrap items-center justify-between gap-2">
                <span className="text-slate-400">Throughput Velocity:</span>
                <span className="text-emerald-400 font-semibold">{scenario.speed} (Active RAM Ring)</span>
              </div>
              <div className="flex flex-wrap items-center justify-between gap-2">
                <span className="text-slate-400">Integrity Protocol:</span>
                <span className="text-sky-300 font-medium">{scenario.integrity}</span>
              </div>
            </div>

            {/* Live Progress Bar HUD */}
            <div className="space-y-2 pt-2">
              <div className="flex items-center justify-between text-xs">
                <span className="text-slate-300 font-medium">Memory Stream:</span>
                <span className="text-cyan-400 font-mono font-bold">
                  {progressPercent}% ({currentStep.bytes} / {scenario.totalSize})
                </span>
              </div>

              {/* Graphical Progress Bar */}
              <div className="w-full bg-slate-900 h-6 rounded-lg p-1 border border-slate-800 flex items-center overflow-hidden">
                <div
                  className="h-full bg-gradient-to-r from-cyan-500 via-sky-500 to-violet-500 rounded transition-all duration-500 relative flex items-center justify-end px-2"
                  style={{ width: `${progressPercent}%` }}
                >
                  <span className="text-[10px] text-white font-mono font-bold drop-shadow">
                    {progressPercent > 15 ? `${progressPercent}%` : ''}
                  </span>
                </div>
              </div>
            </div>

            {/* Live Log Message */}
            <div className="flex items-center gap-2 text-xs pt-1">
              <span className="w-2 h-2 rounded-full bg-cyan-400 animate-pulse shrink-0" />
              <span className="text-slate-400">Status:</span>
              <span className="text-slate-200 font-mono">{currentStep.log}</span>
            </div>

            {/* Verified Box Card */}
            {progressPercent === 100 && (
              <div className="mt-4 p-4 rounded-xl bg-emerald-950/30 border border-emerald-500/30 text-emerald-300 flex items-start gap-3 animate-fadeIn">
                <CheckCircle2 className="w-5 h-5 text-emerald-400 shrink-0 mt-0.5" />
                <div>
                  <div className="font-semibold text-white">Transfer Completed & Committed to SQLite Ledger</div>
                  <div className="text-xs text-emerald-300/80 mt-1">
                    SHA-256 cloud checksum verified with 0 read-back egress. Total local disk I/O: <strong className="text-white">0 Bytes</strong>.
                  </div>
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
    </section>
  );
}
