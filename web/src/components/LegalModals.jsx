import React from 'react';
import { X, Shield, FileText, Lock } from 'lucide-react';

export default function LegalModals({ activeTab, onClose, onSelectTab }) {
  if (!activeTab) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center p-4 sm:p-6 bg-black/80 backdrop-blur-sm animate-fadeIn">
      <div 
        className="relative w-full max-w-4xl max-h-[90vh] bg-[#0E0D0C] border border-[#242220] rounded-lg shadow-2xl overflow-hidden flex flex-col font-mono text-xs"
        onClick={(e) => e.stopPropagation()}
      >
        {/* Top Header */}
        <div className="px-6 py-4 bg-[#141312] border-b border-[#242220] flex items-center justify-between">
          <div className="flex items-center gap-2">
            <button
              onClick={() => onSelectTab('privacy')}
              className={`px-3 py-1.5 rounded transition-colors cursor-pointer ${
                activeTab === 'privacy'
                  ? 'bg-[#242220] text-[#FF5B1F] font-semibold'
                  : 'text-[#9E9B94] hover:text-[#F5F4F0]'
              }`}
            >
              // 01 privacy policy
            </button>
            <button
              onClick={() => onSelectTab('terms')}
              className={`px-3 py-1.5 rounded transition-colors cursor-pointer ${
                activeTab === 'terms'
                  ? 'bg-[#242220] text-violet-400 font-semibold'
                  : 'text-[#9E9B94] hover:text-[#F5F4F0]'
              }`}
            >
              // 02 terms of service
            </button>
            <button
              onClick={() => onSelectTab('security')}
              className={`px-3 py-1.5 rounded transition-colors cursor-pointer ${
                activeTab === 'security'
                  ? 'bg-[#242220] text-emerald-400 font-semibold'
                  : 'text-[#9E9B94] hover:text-[#F5F4F0]'
              }`}
            >
              // 03 security whitepaper
            </button>
          </div>

          <button
            onClick={onClose}
            className="p-1.5 text-[#9E9B94] hover:text-[#F5F4F0] cursor-pointer"
            title="Close dialog"
          >
            [esc ✕]
          </button>
        </div>

        {/* Scrollable Body */}
        <div className="p-6 sm:p-8 overflow-y-auto space-y-6 text-[#9E9B94] font-sans leading-relaxed text-sm">
          {/* PRIVACY POLICY */}
          {activeTab === 'privacy' && (
            <div className="space-y-6">
              <div className="border-b border-[#242220] pb-4 font-mono">
                <div className="text-[#FF5B1F] text-xs font-semibold">// ZERO-KNOWLEDGE &amp; CLIENT-SIDE ARCHITECTURE</div>
                <h2 className="text-2xl font-bold text-[#F5F4F0] mt-1">Cloud Transfer Protocol (CTP) Privacy Policy</h2>
                <div className="text-xs text-[#66635C] mt-1">Application: Cloud Transfer Protocol · Effective Date: November 23, 2026 · Author: Arpit Nayak</div>
              </div>

              <div className="p-4 rounded bg-[#151413] border border-[#242220] text-[#F5F4F0] font-mono text-xs">
                <span className="text-[#FF5B1F] font-bold">// EXECUTIVE PRIVACY GUARANTEE:</span><br />
                Cloud Transfer Protocol (CTP) operates on a verifiable Zero-Knowledge model. We operate zero intermediate relay servers, store zero payload bytes on disk, collect zero analytics, and never transmit your credentials or file metadata to any third party.
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 1.0 Application Purpose &amp; Public Access</h3>
                <p>
                  Cloud Transfer Protocol is an open-source data migration utility that allows users to stream files directly between cloud providers (Google Drive, Microsoft OneDrive, Amazon S3, Cloudflare R2) via in-memory volatile RAM buffers. No user login or registration is required to view information about this software.
                </p>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 2.0 Google User Data Accessed &amp; Scopes Requested</h3>
                <p>
                  When connecting Google Drive, Cloud Transfer Protocol requests only the following OAuth scopes:
                </p>
                <ul className="list-disc pl-5 space-y-1 text-[#9E9B94]">
                  <li><code className="text-[#FF5B1F] font-mono text-xs">https://www.googleapis.com/auth/drive.readonly</code> — To discover and read files user explicitly selects to transfer.</li>
                  <li><code className="text-[#FF5B1F] font-mono text-xs">https://www.googleapis.com/auth/drive.file</code> — To write files user transfers into Google Drive.</li>
                </ul>
                <p>
                  Data accessed includes file IDs, names, sizes, and binary stream contents strictly during active user-directed transfer sessions.
                </p>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 3.0 How Data is Stored &amp; Protected</h3>
                <p>
                  <strong>Zero-Disk RAM Streaming:</strong> File chunks are held in volatile RAM ring buffers solely for the milliseconds required to pipe them into the destination cloud provider. No files are ever saved to local disks or remote servers.
                </p>
                <p>
                  <strong>Local OAuth Tokens:</strong> Authentication tokens are stored exclusively on your local computer (e.g. <code className="text-[#FF5B1F] font-mono text-xs">~/.utp_gdrive_token.json</code>) with user-level OS file permissions and are never sent to any external server.
                </p>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 4.0 Google API Limited Use Disclosure</h3>
                <div className="p-3 rounded bg-[#101726] border border-blue-900/50 text-blue-200 text-xs font-mono">
                  Cloud Transfer Protocol's use and transfer to any other app of information received from Google APIs will adhere to the <a href="https://developers.google.com/terms/api-services-user-data-policy" target="_blank" rel="noopener noreferrer" className="text-[#FF5B1F] underline">Google API Services User Data Policy</a>, including the Limited Use requirements.
                </div>
                <ul className="list-disc pl-5 space-y-1 text-[#9E9B94] text-xs">
                  <li>Google user data is never used for serving advertisements or retargeting.</li>
                  <li>Google user data is <strong>never used to develop, improve, or train generalized AI and/or ML models</strong>.</li>
                  <li>Google user data is never sold or transferred to data brokers or third parties.</li>
                </ul>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 5.0 Google APIs Terms of Service &amp; AI NCII Prohibition</h3>
                <p>
                  In strict compliance with the Google APIs Terms of Service, Cloud Transfer Protocol strictly prohibits the use of Google APIs or CTP services for creating, generating, hosting, processing, or distributing Non-Consensual Intimate Imagery (NCII), CSAM, harassment, or unlawful content.
                </p>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 6.0 Data Deletion &amp; Access Revocation</h3>
                <p>
                  You may revoke Cloud Transfer Protocol's access at any time via your <a href="https://myaccount.google.com/permissions" target="_blank" rel="noopener noreferrer" className="text-[#FF5B1F] underline">Google Account Security Permissions</a>. You can also delete local credentials by deleting the <code className="text-[#FF5B1F] font-mono text-xs">~/.utp_*_token.json</code> files from your device.
                </p>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 7.0 Developer Contact</h3>
                <p>
                  Lead Developer: Arpit Nayak · Email: <a href="mailto:arpitpupu@gmail.com" className="text-[#FF5B1F] underline">arpitpupu@gmail.com</a> · Website: <a href="https://ctp.arpitnayak.tech" className="text-[#FF5B1F] underline">https://ctp.arpitnayak.tech</a>
                </p>
              </div>
            </div>
          )}

          {/* TERMS OF SERVICE */}
          {activeTab === 'terms' && (
            <div className="space-y-6">
              <div className="border-b border-[#242220] pb-4 font-mono">
                <div className="text-violet-400 text-xs font-semibold">// OPEN SOURCE & COMMERCIAL USAGE</div>
                <h2 className="text-2xl font-bold text-[#F5F4F0] mt-1">Universal Cloud Transfer Protocol (CTP/UTP) Terms of Service</h2>
                <div className="text-xs text-[#66635C] mt-1">Effective Date: November 20, 2026 · License: MIT</div>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 1.0 License Grant (MIT License)</h3>
                <p>
                  CTP is free, open-source software licensed under the MIT License. Permission is hereby granted to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the software for personal, commercial, and enterprise purposes.
                </p>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 2.0 Acceptable Use</h3>
                <p>
                  You agree to use CTP only for lawful data transmission. You must not use the protocol to transmit malicious software, violate intellectual property rights, or breach the terms of service of any third-party cloud provider.
                </p>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 3.0 Disclaimer of Warranty</h3>
                <div className="p-4 rounded bg-[#151413] border border-[#242220] text-xs font-mono text-[#F5F4F0]">
                  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
                </div>
              </div>
            </div>
          )}

          {/* SECURITY WHITEPAPER */}
          {activeTab === 'security' && (
            <div className="space-y-6">
              <div className="border-b border-[#242220] pb-4 font-mono">
                <div className="text-emerald-400 text-xs font-semibold">// CRYPTOGRAPHIC ARCHITECTURE & ISOLATION</div>
                <h2 className="text-2xl font-bold text-[#F5F4F0] mt-1">CTP Security Architecture & Data Handling</h2>
                <div className="text-xs text-[#66635C] mt-1">Standards: RFC 8446 (TLS 1.3) · NIST FIPS 180-4 (SHA-256)</div>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 1.0 Transport Layer Security (TLS 1.3)</h3>
                <p>
                  Socket streams mandate TLS 1.3 with AEAD cipher suites (AES-256-GCM, ChaCha20-Poly1305). Legacy, insecure TLS versions (1.0, 1.1) and broken ciphers are strictly rejected at the socket layer.
                </p>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 2.0 Dual In-Flight Hash Pipeline</h3>
                <p>
                  As bytes flow through the volatile RAM ring buffer, CTP passes chunks simultaneously into dual OpenSSL digest streams: SHA-256 (NIST FIPS 180-4) and MD5 (RFC 1321). Hashes are matched directly against server response headers upon upload commit.
                </p>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 3.0 Daemon Process Group Security</h3>
                <p>
                  Background REST API daemon processes require Bearer token authorization (<code className="text-[#FF5B1F] font-mono text-xs">Authorization: Bearer &lt;token&gt;</code>) generated cryptographically via CSPRNG and bound to localhost (<code className="text-xs font-mono">127.0.0.1</code>).
                </p>
              </div>
            </div>
          )}
        </div>

        {/* Bottom Footer */}
        <div className="px-6 py-3 bg-[#141312] border-t border-[#242220] flex items-center justify-between text-[#66635C]">
          <span>
            Built by{' '}
            <a
              href="https://arpitnayak.tech"
              target="_blank"
              rel="noopener noreferrer"
              className="text-[#FF5B1F] hover:underline font-semibold"
            >
              Nayak & Nayak Co.
            </a>
          </span>
          <button
            onClick={onClose}
            className="text-[#9E9B94] hover:text-[#F5F4F0] cursor-pointer"
          >
            [close]
          </button>
        </div>
      </div>
    </div>
  );
}
