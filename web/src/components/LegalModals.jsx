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
                <div className="text-[#FF5B1F] text-xs font-semibold">// ZERO-KNOWLEDGE & CLIENT-SIDE ARCHITECTURE</div>
                <h2 className="text-2xl font-bold text-[#F5F4F0] mt-1">Universal Cloud Transfer Protocol (CTP/UTP) Privacy Policy</h2>
                <div className="text-xs text-[#66635C] mt-1">Effective Date: November 20, 2026 · Release: v2.6.0-pro</div>
              </div>

              <div className="p-4 rounded bg-[#151413] border border-[#242220] text-[#F5F4F0] font-mono text-xs">
                <span className="text-[#FF5B1F] font-bold">// EXECUTIVE PRIVACY GUARANTEE:</span><br />
                CTP operates on a verifiable Zero-Knowledge model. We do not operate intermediate relay servers, we do not inspect payloads, we collect zero analytics, and we never transmit your credentials or file metadata to any server.
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 1.0 Data We Do NOT Collect</h3>
                <ul className="list-disc pl-5 space-y-1 text-[#9E9B94]">
                  <li><strong>Zero Personal Identifiable Information (PII)</strong>: No names, emails, IP addresses, or phone numbers.</li>
                  <li><strong>Zero File Data or Metadata</strong>: File contents and directory paths stream solely in your machine's volatile RAM.</li>
                  <li><strong>Zero Telemetry or Analytics</strong>: No analytics packages, crash reporters, or tracking beacons exist in the software.</li>
                  <li><strong>Zero Account Registration</strong>: No accounts or profiles are required or supported.</li>
                </ul>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 2.0 Direct Peer-to-Cloud Transmission</h3>
                <p>
                  Transfers stream directly between your device and the designated cloud storage APIs (Google Drive, Microsoft Graph, Amazon S3, Cloudflare, WebDAV). All traffic utilizes direct TLS 1.3 socket encryption without intermediary proxies.
                </p>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 3.0 Local Credential Hygiene</h3>
                <p>
                  OAuth access and refresh tokens are stored exclusively on your local device (e.g., <code className="text-[#FF5B1F] font-mono text-xs">.utp_*_token.json</code>) with standard operating system permissions. Tokens are never transmitted to any third party.
                </p>
              </div>

              <div className="space-y-2">
                <h3 className="font-bold text-[#F5F4F0] font-mono text-sm">§ 4.0 GDPR and CCPA Compliance</h3>
                <p>
                  Because CTP processes zero personal data externally, it inherently satisfies all data minimization and privacy standards of the European Union GDPR and California CCPA.
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
          <span>DOMAIN: ctp.arpitnayak.tech</span>
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
