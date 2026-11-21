import React, { useState, useEffect } from 'react';
import { Play, Pause, RotateCcw, CheckCircle2 } from 'lucide-react';

const SCENARIOS = [
  {
    id: 'transfer',
    tabName: '01 / transfer',
    title: 'Cloud-to-Cloud Volatile RAM Transfer',
    cmd: 'utp transfer gdrive://1GwVK676IBt... onedrive://Backups/dataset.tar.gz',
    pipeline: 'gdrive://1GwVK676IBt10iBEdRzh ──▶ onedrive://Backups/dataset.tar.gz',
    payload: '2.40 GB archive',
    velocity: '142.8 MB/s',
    diskIo: '0 Bytes (Volatile Ring Buffer)',
    verification: 'Server-Side SHA-256 (OneDrive sha256Hash Match)',
    logSteps: [
      { progress: 0.15, bytes: '360 MB', chunk: '3/16', text: 'Allocating volatile RAM ring buffer (32 MB chunks over TLS 1.3)...' },
      { progress: 0.45, bytes: '1.08 GB', chunk: '7/16', text: 'Streaming HTTP Range chunks concurrently into upload session...' },
      { progress: 0.75, bytes: '1.80 GB', chunk: '12/16', text: 'Dual in-flight digest pipeline calculating SHA-256 + MD5 in memory...' },
      { progress: 0.95, bytes: '2.28 GB', chunk: '15/16', text: 'Writing final chunk to Microsoft Graph upload session...' },
      { progress: 1.00, bytes: '2.40 GB', chunk: '16/16', text: 'Verified server-side SHA-256 hash. 0 Bytes written to local disk!' }
    ]
  },
  {
    id: 'sync',
    tabName: '02 / sync',
    title: 'Delta Folder Synchronization (0-Byte Skip)',
    cmd: 'utp sync file://./production_assets onedrive://Assets --policy newer_wins',
    pipeline: 'file://production_assets ──▶ onedrive://Assets (Bidirectional)',
    payload: '4.85 GB (148 files)',
    velocity: '188.4 MB/s',
    diskIo: '0 Bytes on unchanged files',
    verification: 'Incremental Reconciliation Matrix',
    logSteps: [
      { progress: 0.20, bytes: 'Scanning...', chunk: 'Planning', text: 'Traversing local tree (148 files) vs remote Microsoft Graph index...' },
      { progress: 0.55, bytes: '0 B Delta', chunk: 'Skip 142', text: 'Identified 142 unchanged files — SKIPPED (0 bytes transferred).' },
      { progress: 0.85, bytes: '320 MB', chunk: '6 to update', text: 'Synchronizing 6 delta modifications via policy NEWER_WINS...' },
      { progress: 1.00, bytes: '320 MB', chunk: 'Complete', text: 'Delta sync complete: 142 unchanged skipped, 6 updated cleanly.' }
    ]
  },
  {
    id: 'route',
    tabName: '03 / route',
    title: 'Rules-Based Smart Routing Engine',
    cmd: 'utp route ./data --rule "ext=mp4:s3://media" --rule "ext=pdf:onedrive://docs"',
    pipeline: 'file://data ──▶ [S3 Media Vault, OneDrive Documents, B2 Cold Archive]',
    payload: '12.6 GB dataset',
    velocity: '210.5 MB/s',
    diskIo: 'Single Read Pass Broadcast',
    verification: 'Simultaneous Multi-Destination Fan-Out',
    logSteps: [
      { progress: 0.20, bytes: 'Routing...', chunk: 'Rule Eval', text: 'Evaluating routing engine: 3 condition rules + 1 default fallback.' },
      { progress: 0.50, bytes: '8.4 GB', chunk: 'ext=mp4', text: 'Routing 4 .mp4 video files ──▶ s3://media-vault/ (Direct S3 Multipart)' },
      { progress: 0.80, bytes: '2.1 GB', chunk: 'ext=pdf', text: 'Routing 18 .pdf documents ──▶ onedrive://docs/ (Chunked Session)' },
      { progress: 1.00, bytes: '12.6 GB', chunk: 'All routed', text: 'Fan-out execution complete: 100% matched rules routed in 1 read pass.' }
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
        if (prev < scenario.logSteps.length - 1) return prev + 1;
        return prev;
      });
    }, 1800);
    return () => clearInterval(interval);
  }, [isPlaying, scenario.logSteps.length]);

  const currentStep = scenario.logSteps[stepIndex] || scenario.logSteps[0];
  const progressPercent = Math.round(currentStep.progress * 100);

  return (
    <section id="simulator" className="py-20 border-b border-[#242220]">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        {/* Section Header */}
        <div className="flex flex-wrap items-center justify-between gap-3 pb-6 border-b border-[#242220] font-mono text-xs text-[#9E9B94] mb-10">
          <div className="flex items-center gap-2">
            <span className="text-[#FF5B1F] font-semibold">// 03 / ENGINE SIMULATOR</span>
            <span className="text-[#66635C]">—</span>
            <span>INTERACTIVE RUNTIME BENCHMARK</span>
          </div>
          <div className="text-[#66635C]">
            [ADAPTIVE 2.5MB - 40MB RAM RING]
          </div>
        </div>

        {/* Terminal Container */}
        <div className="border border-[#242220] rounded-lg bg-[#11100F] overflow-hidden font-mono text-xs">
          {/* Top Bar */}
          <div className="flex items-center justify-between px-4 py-3 bg-[#0E0D0C] border-b border-[#242220]">
            <div className="flex items-center gap-2">
              <span className="w-2.5 h-2.5 rounded-full bg-[#383531] inline-block" />
              <span className="w-2.5 h-2.5 rounded-full bg-[#383531] inline-block" />
              <span className="w-2.5 h-2.5 rounded-full bg-[#383531] inline-block" />
              <span className="ml-2 text-[#9E9B94] text-xs">
                utp-runtime-v2.6.1 (x86_64)
              </span>
            </div>

            {/* Playback Controls */}
            <div className="flex items-center gap-3">
              <button
                onClick={() => setIsPlaying(!isPlaying)}
                className="text-[#9E9B94] hover:text-[#F5F4F0] transition-colors cursor-pointer flex items-center gap-1"
              >
                {isPlaying ? <Pause className="w-3 h-3" /> : <Play className="w-3 h-3 text-[#FF5B1F]" />}
                <span>{isPlaying ? 'pause' : 'resume'}</span>
              </button>
              <button
                onClick={() => setStepIndex(0)}
                className="text-[#9E9B94] hover:text-[#F5F4F0] transition-colors cursor-pointer flex items-center gap-1"
              >
                <RotateCcw className="w-3 h-3" />
                <span>replay</span>
              </button>
            </div>
          </div>

          {/* Scenario Tab Bar */}
          <div className="flex items-center gap-1 px-4 py-2 border-b border-[#242220] bg-[#141312] overflow-x-auto">
            {SCENARIOS.map(s => (
              <button
                key={s.id}
                onClick={() => setActiveScenarioId(s.id)}
                className={`px-3 py-1.5 rounded transition-all cursor-pointer whitespace-nowrap ${
                  activeScenarioId === s.id
                    ? 'bg-[#242220] text-[#FF5B1F] font-semibold'
                    : 'text-[#9E9B94] hover:text-[#F5F4F0]'
                }`}
              >
                {s.tabName}
              </button>
            ))}
          </div>

          {/* Console Body */}
          <div className="p-6 space-y-4 text-[#F5F4F0] bg-[#0E0D0C]">
            {/* Command Prompt */}
            <div className="flex items-center gap-2 text-[#9E9B94]">
              <span className="text-[#FF5B1F]">▲</span>
              <span className="text-[#66635C]">arpit@terminal:~$</span>
              <span className="text-[#F5F4F0]">{scenario.cmd}</span>
            </div>

            {/* Pipeline Header */}
            <div className="pl-4 border-l border-[#33302C] space-y-1 text-xs text-[#9E9B94]">
              <div>
                <span className="text-[#FF5B1F] font-semibold">PIPELINE:</span> {scenario.pipeline}
              </div>
              <div>
                <span className="text-[#66635C]">THROUGHPUT:</span>{' '}
                <span className="text-emerald-400 font-semibold">{scenario.velocity}</span> •{' '}
                <span className="text-[#66635C]">DISK I/O:</span>{' '}
                <span className="text-[#F5F4F0]">{scenario.diskIo}</span>
              </div>
              <div>
                <span className="text-[#66635C]">INTEGRITY:</span> {scenario.verification}
              </div>
            </div>

            {/* Progress Bar */}
            <div className="space-y-1.5 pt-2">
              <div className="flex items-center justify-between text-xs text-[#9E9B94]">
                <span>STREAM BUFFER PROGRESS:</span>
                <span className="text-[#F5F4F0] font-semibold">
                  {progressPercent}% [{currentStep.bytes} / {scenario.payload}]
                </span>
              </div>

              <div className="w-full bg-[#1C1A18] h-4 rounded overflow-hidden border border-[#242220] p-0.5">
                <div
                  className="h-full bg-gradient-to-r from-[#FF5B1F] to-[#FF804D] rounded transition-all duration-300"
                  style={{ width: `${progressPercent}%` }}
                />
              </div>
            </div>

            {/* Live Status Message */}
            <div className="flex items-center gap-2 text-xs pt-2 text-[#9E9B94]">
              <span className="live-beacon" />
              <span>LOG:</span>
              <span className="text-[#F5F4F0]">{currentStep.text}</span>
            </div>

            {/* Verified Box */}
            {progressPercent === 100 && (
              <div className="mt-4 p-4 rounded bg-[#151413] border border-emerald-500/40 text-emerald-300 flex items-start gap-3">
                <CheckCircle2 className="w-4 h-4 text-emerald-400 shrink-0 mt-0.5" />
                <div>
                  <div className="font-semibold text-[#F5F4F0]">Verified & Committed to Local SQLite Ledger (utp.db)</div>
                  <div className="text-xs text-[#9E9B94] mt-0.5">
                    Server-side cloud hash matched in-flight memory checksum. Zero read-back egress. Total local disk write wear: 0 Bytes.
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
