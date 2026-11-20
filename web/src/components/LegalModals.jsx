import React from 'react';
import { X, Shield, FileText, Lock, CheckCircle2, AlertTriangle, ExternalLink } from 'lucide-react';

export default function LegalModals({ activeTab, onClose, onSelectTab }) {
  if (!activeTab) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center p-4 sm:p-6 bg-black/85 backdrop-blur-md animate-fadeIn">
      <div 
        className="relative w-full max-w-4xl max-h-[90vh] bg-[#0c1017] border border-slate-700/80 rounded-3xl shadow-2xl overflow-hidden flex flex-col"
        onClick={(e) => e.stopPropagation()}
      >
        {/* Modal Header */}
        <div className="px-6 py-5 bg-slate-900/80 border-b border-slate-800 flex items-center justify-between">
          <div className="flex items-center gap-3">
            <div className="flex items-center gap-2">
              <button
                onClick={() => onSelectTab('privacy')}
                className={`flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs font-mono font-medium transition-colors cursor-pointer ${
                  activeTab === 'privacy'
                    ? 'bg-cyan-500/20 text-cyan-300 border border-cyan-500/40'
                    : 'text-slate-400 hover:text-white'
                }`}
              >
                <Shield className="w-3.5 h-3.5" />
                Privacy Policy
              </button>
              <button
                onClick={() => onSelectTab('terms')}
                className={`flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs font-mono font-medium transition-colors cursor-pointer ${
                  activeTab === 'terms'
                    ? 'bg-violet-500/20 text-violet-300 border border-violet-500/40'
                    : 'text-slate-400 hover:text-white'
                }`}
              >
                <FileText className="w-3.5 h-3.5" />
                Terms of Service
              </button>
              <button
                onClick={() => onSelectTab('security')}
                className={`flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs font-mono font-medium transition-colors cursor-pointer ${
                  activeTab === 'security'
                    ? 'bg-emerald-500/20 text-emerald-300 border border-emerald-500/40'
                    : 'text-slate-400 hover:text-white'
                }`}
              >
                <Lock className="w-3.5 h-3.5" />
                Security Whitepaper
              </button>
            </div>
          </div>

          <button
            onClick={onClose}
            className="p-2 rounded-xl text-slate-400 hover:text-white hover:bg-slate-800 transition-colors cursor-pointer"
            title="Close dialog (Esc)"
          >
            <X className="w-5 h-5" />
          </button>
        </div>

        {/* Modal Scrollable Body */}
        <div className="p-6 sm:p-8 overflow-y-auto space-y-6 text-sm text-slate-300 leading-relaxed">
          {/* TAB 1: PRIVACY POLICY */}
          {activeTab === 'privacy' && (
            <div className="space-y-6">
              <div className="border-b border-slate-800 pb-4">
                <span className="text-xs font-mono text-cyan-400 uppercase tracking-widest font-semibold">
                  Zero-Knowledge & Client-Side Privacy
                </span>
                <h2 className="text-2xl font-extrabold text-white mt-1">Universal Cloud Transfer Protocol (CTP/UTP) Privacy Policy</h2>
                <p className="text-xs text-slate-400 mt-1">Effective Date: November 20, 2026 · Software Version: v2.6.0-pro</p>
              </div>

              <div className="p-4 rounded-xl bg-cyan-950/20 border border-cyan-500/30 text-cyan-300 flex items-start gap-3">
                <Shield className="w-5 h-5 text-cyan-400 shrink-0 mt-0.5" />
                <div>
                  <strong className="text-white block font-semibold">Executive Privacy Guarantee:</strong>
                  CTP operates on a strict <strong>Zero-Knowledge Architecture</strong>. We do not operate intermediate relay servers, we do not inspect payloads, we do not track analytics, and we never collect, store, or transmit your credentials.
                </div>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">1. Data We Do NOT Collect</h3>
                <p>
                  Unlike web SaaS platforms, CTP is client-side software. The software does <strong>not</strong> collect:
                </p>
                <ul className="list-disc pl-5 space-y-1.5 text-slate-400">
                  <li><strong>No Personal Identifiable Information (PII)</strong>: No names, email addresses, phone numbers, or physical locations.</li>
                  <li><strong>No File Data or Metadata</strong>: File contents, filenames, folder structures, and hashes are processed entirely in your machine's volatile RAM.</li>
                  <li><strong>No Diagnostic Telemetry or Analytics</strong>: No Google Analytics, Mixpanel, Sentry, or tracking pixels are embedded in the CLI, daemon, SDKs, or web app.</li>
                  <li><strong>No Account Sign-Ups</strong>: You do not need to register or create an account with CTP to use the software.</li>
                </ul>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">2. Direct Peer-to-Cloud Transmission</h3>
                <p>
                  All data migrations occur directly between your endpoint and the designated storage providers (e.g., Google Drive, Microsoft Graph, Amazon AWS, Cloudflare, WebDAV). Every byte flows through <strong>direct TLS 1.3 encrypted sockets</strong> negotiated exclusively between your computer and the official provider API endpoints.
                </p>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">3. Local-Only Credential Hygiene</h3>
                <p>
                  Authentication tokens (e.g., OAuth 2.0 access and refresh tokens) are obtained directly via standard OAuth PKCE authorization flows. These tokens are saved strictly on your local filesystem (e.g., <code className="text-cyan-300 font-mono text-xs">.utp_*_token.json</code>) protected by standard operating system file permissions. Tokens never leave your local device.
                </p>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">4. SQLite State Persistence (ACID Ledger)</h3>
                <p>
                  Transfer tracking states and chunk ledgers are persisted locally in <code className="text-cyan-300 font-mono text-xs">utp.db</code>. This database exists solely on your local hard drive to allow transfer resumption and is never broadcast or synchronized to any external server.
                </p>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">5. GDPR, CCPA, and Regulatory Compliance</h3>
                <p>
                  Because CTP processes zero personal data on external servers and acts solely as client-side software, CTP is fully compliant with the European Union General Data Protection Regulation (GDPR) and the California Consumer Privacy Act (CCPA).
                </p>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">6. Contact & Data Officer</h3>
                <p>
                  For any privacy inquiries or security verifications, contact: <a href="mailto:arpit@utp.cloud" className="text-cyan-400 hover:underline">arpit@utp.cloud</a> or visit <a href="https://arpitnayak.tech" target="_blank" rel="noopener noreferrer" className="text-cyan-400 hover:underline">arpitnayak.tech</a>.
                </p>
              </div>
            </div>
          )}

          {/* TAB 2: TERMS OF SERVICE */}
          {activeTab === 'terms' && (
            <div className="space-y-6">
              <div className="border-b border-slate-800 pb-4">
                <span className="text-xs font-mono text-violet-400 uppercase tracking-widest font-semibold">
                  Open Source & Commercial Usage
                </span>
                <h2 className="text-2xl font-extrabold text-white mt-1">Universal Cloud Transfer Protocol (CTP/UTP) Terms of Service</h2>
                <p className="text-xs text-slate-400 mt-1">Effective Date: November 20, 2026 · Software License: MIT</p>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">1. Acceptance of Terms</h3>
                <p>
                  By downloading, compiling, executing, or integrating the Universal Cloud Transfer Protocol (CTP/UTP) software, CLI, daemon, SDKs, or web interfaces, you agree to be bound by these Terms of Service.
                </p>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">2. License Grant (MIT License)</h3>
                <p>
                  The CTP core software is distributed under the permissive <strong>MIT License</strong>. You are granted free of charge the right to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the software, subject to the condition that the copyright notice and permission notice are included in all copies or substantial portions of the Software.
                </p>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">3. Acceptable Use Policy</h3>
                <p>
                  You agree to use CTP only for lawful purposes. You must not use CTP to migrate, transmit, or store:
                </p>
                <ul className="list-disc pl-5 space-y-1.5 text-slate-400">
                  <li>Data that violates any applicable domestic or international intellectual property rights.</li>
                  <li>Malicious payloads, ransomware, or software intended to disrupt computer systems.</li>
                  <li>Data in violation of the terms of service of the respective third-party cloud providers (Google Drive, Microsoft, Amazon AWS, Cloudflare, etc.).</li>
                </ul>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">4. Third-Party Cloud Providers</h3>
                <p>
                  CTP interfaces with third-party cloud storage APIs. You acknowledge that your use of third-party cloud services is governed by their respective terms and rate limits. CTP is not responsible for cloud provider service interruptions, account suspensions, or API deprecations.
                </p>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">5. Disclaimer of Warranty</h3>
                <div className="p-4 rounded-xl bg-slate-900 border border-slate-800 text-xs font-mono text-slate-300">
                  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY.
                </div>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">6. Modifications</h3>
                <p>
                  We reserve the right to modify these terms as new protocol features, SDK bindings, and storage adapters are released. Updates will be reflected in the official repository.
                </p>
              </div>
            </div>
          )}

          {/* TAB 3: SECURITY WHITEPAPER */}
          {activeTab === 'security' && (
            <div className="space-y-6">
              <div className="border-b border-slate-800 pb-4">
                <span className="text-xs font-mono text-emerald-400 uppercase tracking-widest font-semibold">
                  Technical Cryptography & Integrity
                </span>
                <h2 className="text-2xl font-extrabold text-white mt-1">CTP Security Architecture & Data Handling</h2>
                <p className="text-xs text-slate-400 mt-1">Specification Document · Standard: RFC 8446 / NIST FIPS 180-4</p>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">1. Transport Layer Security (TLS 1.3)</h3>
                <p>
                  All socket streams in CTP mandate <strong>TLS 1.3</strong> (RFC 8446) with modern AEAD cipher suites (AES-256-GCM and ChaCha20-Poly1305). Insecure legacy TLS versions (1.0, 1.1) and broken ciphers are strictly rejected at the socket layer via OpenSSL 3.x.
                </p>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">2. Dual In-Flight Cryptographic Pipeline</h3>
                <p>
                  As bytes flow through the volatile RAM ring buffer, CTP passes chunks simultaneously into dual OpenSSL digest streams:
                </p>
                <ul className="list-disc pl-5 space-y-1.5 text-slate-400">
                  <li><strong>SHA-256 (NIST FIPS 180-4)</strong>: Verified against Microsoft OneDrive (<code className="text-cyan-300 font-mono">sha256Hash</code>) and AWS S3 object checksums.</li>
                  <li><strong>MD5 (RFC 1321)</strong>: Verified against Google Drive (<code className="text-cyan-300 font-mono">md5Checksum</code>).</li>
                </ul>
                <p className="text-xs text-emerald-400">
                  Fast-path verification matches hashes directly against cloud server response headers upon session commit, eliminating unnecessary read-back egress.
                </p>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">3. Zero-Disk Memory Isolation</h3>
                <p>
                  Transfer buffers reside solely in volatile RAM ring structures. Bytes are released from RAM the moment a chunk is acknowledged as committed by the destination provider. Temporary unencrypted cache files are never written to disk, preventing forensic recovery from unallocated disk sectors.
                </p>
              </div>

              <div className="space-y-3">
                <h3 className="text-base font-bold text-white">4. API Server Daemon (`utpd`) Authentication</h3>
                <p>
                  When running in daemon mode, all REST endpoints (<code className="text-cyan-300 font-mono">/api/transfers</code>) require Bearer token authorization (<code className="text-cyan-300 font-mono">Authorization: Bearer &lt;token&gt;</code>). Tokens are generated cryptographically at startup via CSPRNG and bound to localhost (<code className="text-cyan-300 font-mono">127.0.0.1</code>) by default.
                </p>
              </div>
            </div>
          )}
        </div>

        {/* Modal Footer */}
        <div className="px-6 py-4 bg-slate-900/60 border-t border-slate-800 flex items-center justify-between text-xs text-slate-400">
          <span>Official Legal Hub · ctp.arpitnayak.tech</span>
          <button
            onClick={onClose}
            className="px-4 py-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-white font-medium transition-colors cursor-pointer"
          >
            Close Document
          </button>
        </div>
      </div>
    </div>
  );
}
