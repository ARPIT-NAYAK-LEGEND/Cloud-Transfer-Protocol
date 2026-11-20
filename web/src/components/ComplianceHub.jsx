import React from 'react';
import { ArrowRight, Shield, FileText, Lock, Check } from 'lucide-react';

export default function ComplianceHub({ onOpenLegal }) {
  return (
    <section id="compliance" className="py-20 border-b border-[#242220]">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        {/* Section Header */}
        <div className="flex flex-wrap items-center justify-between gap-3 pb-6 border-b border-[#242220] font-mono text-xs text-[#9E9B94] mb-12">
          <div className="flex items-center gap-2">
            <span className="text-[#FF5B1F] font-semibold">// 06 / COMPLIANCE & LEGAL HUB</span>
            <span className="text-[#66635C]">—</span>
            <span>STORE REVIEW & ENTERPRISE AUDIT APPROVAL</span>
          </div>
          <div className="text-[#66635C]">
            [100% ZERO-KNOWLEDGE ARCHITECTURE]
          </div>
        </div>

        {/* 3-Column Compliance Grid */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-px bg-[#242220] border border-[#242220] rounded-lg overflow-hidden">
          {/* Card 1: Privacy Policy */}
          <div className="bg-[#121110] hover:bg-[#161513] transition-colors p-8 flex flex-col justify-between group">
            <div>
              <div className="text-[#66635C] font-mono text-xs pb-3 border-b border-[#242220] flex items-center justify-between mb-5">
                <span>// COMPLIANCE: 01</span>
                <span className="text-[#FF5B1F]">[ZERO LOGS]</span>
              </div>

              <h3 className="text-xl font-bold text-[#F5F4F0] group-hover:text-[#FF5B1F] transition-colors">
                Zero-Knowledge Privacy Policy
              </h3>

              <p className="mt-3 text-sm text-[#9E9B94] leading-relaxed">
                Zero telemetry, zero external logging, zero tracking cookies. Direct peer-to-cloud streams ensure credentials and payloads never touch intermediate servers.
              </p>

              <div className="mt-6 space-y-2 font-mono text-xs text-[#9E9B94]">
                <div className="flex items-center gap-2 text-emerald-400">
                  <Check className="w-3.5 h-3.5" />
                  <span>No analytics or tracking cookies</span>
                </div>
                <div className="flex items-center gap-2 text-emerald-400">
                  <Check className="w-3.5 h-3.5" />
                  <span>Local tokens: ~/.utp_*_token.json</span>
                </div>
                <div className="flex items-center gap-2 text-emerald-400">
                  <Check className="w-3.5 h-3.5" />
                  <span>Fully GDPR & CCPA compliant</span>
                </div>
              </div>
            </div>

            <button
              onClick={() => onOpenLegal('privacy')}
              className="mt-8 pt-4 border-t border-[#242220] font-mono text-xs text-[#FF5B1F] flex items-center justify-between cursor-pointer group-hover:text-[#FF7036]"
            >
              <span>Read Privacy Policy</span>
              <ArrowRight className="w-3.5 h-3.5 group-hover:translate-x-1 transition-transform" />
            </button>
          </div>

          {/* Card 2: Terms of Service */}
          <div className="bg-[#121110] hover:bg-[#161513] transition-colors p-8 flex flex-col justify-between group">
            <div>
              <div className="text-[#66635C] font-mono text-xs pb-3 border-b border-[#242220] flex items-center justify-between mb-5">
                <span>// COMPLIANCE: 02</span>
                <span className="text-violet-400">[MIT LICENSE]</span>
              </div>

              <h3 className="text-xl font-bold text-[#F5F4F0] group-hover:text-[#FF5B1F] transition-colors">
                Terms of Service & Licensing
              </h3>

              <p className="mt-3 text-sm text-[#9E9B94] leading-relaxed">
                Licensed under the permissive MIT Open Source License. Complete terms governing personal, commercial, and enterprise usage of the CTP engine and SDKs.
              </p>

              <div className="mt-6 space-y-2 font-mono text-xs text-[#9E9B94]">
                <div className="flex items-center gap-2 text-emerald-400">
                  <Check className="w-3.5 h-3.5" />
                  <span>Permissive MIT license terms</span>
                </div>
                <div className="flex items-center gap-2 text-emerald-400">
                  <Check className="w-3.5 h-3.5" />
                  <span>Clear limitation of liability</span>
                </div>
                <div className="flex items-center gap-2 text-emerald-400">
                  <Check className="w-3.5 h-3.5" />
                  <span>Official cloud provider API compliance</span>
                </div>
              </div>
            </div>

            <button
              onClick={() => onOpenLegal('terms')}
              className="mt-8 pt-4 border-t border-[#242220] font-mono text-xs text-violet-400 flex items-center justify-between cursor-pointer group-hover:text-violet-300"
            >
              <span>Read Terms of Service</span>
              <ArrowRight className="w-3.5 h-3.5 group-hover:translate-x-1 transition-transform" />
            </button>
          </div>

          {/* Card 3: Security Whitepaper */}
          <div className="bg-[#121110] hover:bg-[#161513] transition-colors p-8 flex flex-col justify-between group">
            <div>
              <div className="text-[#66635C] font-mono text-xs pb-3 border-b border-[#242220] flex items-center justify-between mb-5">
                <span>// COMPLIANCE: 03</span>
                <span className="text-emerald-400">[TLS 1.3]</span>
              </div>

              <h3 className="text-xl font-bold text-[#F5F4F0] group-hover:text-[#FF5B1F] transition-colors">
                Security & Token Isolation
              </h3>

              <p className="mt-3 text-sm text-[#9E9B94] leading-relaxed">
                Cryptographic specifications, TLS 1.3 socket validation, client-side OAuth secret resolution, and detached API daemon security parameters.
              </p>

              <div className="mt-6 space-y-2 font-mono text-xs text-[#9E9B94]">
                <div className="flex items-center gap-2 text-emerald-400">
                  <Check className="w-3.5 h-3.5" />
                  <span>Dual in-flight SHA-256 + MD5 pipeline</span>
                </div>
                <div className="flex items-center gap-2 text-emerald-400">
                  <Check className="w-3.5 h-3.5" />
                  <span>Daemon CSPRNG Bearer token auth</span>
                </div>
                <div className="flex items-center gap-2 text-emerald-400">
                  <Check className="w-3.5 h-3.5" />
                  <span>Zero local temp-file residue</span>
                </div>
              </div>
            </div>

            <button
              onClick={() => onOpenLegal('security')}
              className="mt-8 pt-4 border-t border-[#242220] font-mono text-xs text-emerald-400 flex items-center justify-between cursor-pointer group-hover:text-emerald-300"
            >
              <span>View Security Specs</span>
              <ArrowRight className="w-3.5 h-3.5 group-hover:translate-x-1 transition-transform" />
            </button>
          </div>
        </div>
      </div>
    </section>
  );
}
