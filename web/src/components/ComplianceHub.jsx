import React from 'react';
import { ArrowRight, Shield, FileText, Lock } from 'lucide-react';

export default function ComplianceHub({ onOpenLegal }) {
  return (
    <section id="compliance" className="py-20 border-b border-[#242220]">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        {/* Section Header */}
        <div className="flex flex-wrap items-center justify-between gap-3 pb-6 border-b border-[#242220] font-mono text-xs text-[#9E9B94] mb-10">
          <div className="flex items-center gap-2">
            <span className="text-[#FF5B1F] font-semibold">// 05 / COMPLIANCE & LEGAL SPECIFICATION</span>
            <span className="text-[#66635C]">—</span>
            <span>ZERO-KNOWLEDGE ARCHITECTURE</span>
          </div>
          <div className="text-[#66635C]">
            [STORE REVIEW & ENTERPRISE AUDIT]
          </div>
        </div>

        {/* Streamlined 3-Column Compliance Grid */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-px bg-[#242220] border border-[#242220] rounded-lg overflow-hidden font-mono text-xs">
          {/* Item 1 */}
          <div className="bg-[#121110] hover:bg-[#161513] transition-colors p-6 flex flex-col justify-between">
            <div>
              <div className="text-[#66635C] mb-3 flex items-center justify-between">
                <span>// 01 PRIVACY</span>
                <span className="text-[#FF5B1F]">[ZERO LOGS]</span>
              </div>
              <h4 className="text-base font-bold text-[#F5F4F0] mb-2">Zero-Knowledge Policy</h4>
              <p className="text-xs text-[#9E9B94] font-sans leading-relaxed">
                Zero telemetry, zero analytics, zero external logging. Tokens saved locally in <code className="text-[#FF5B1F]">~/.utp_*_token.json</code>.
              </p>
            </div>
            <button
              onClick={() => onOpenLegal('privacy')}
              className="mt-6 pt-3 border-t border-[#242220] text-[#FF5B1F] hover:text-[#FF7036] flex items-center justify-between cursor-pointer"
            >
              <span>Read Policy</span>
              <ArrowRight className="w-3.5 h-3.5" />
            </button>
          </div>

          {/* Item 2 */}
          <div className="bg-[#121110] hover:bg-[#161513] transition-colors p-6 flex flex-col justify-between">
            <div>
              <div className="text-[#66635C] mb-3 flex items-center justify-between">
                <span>// 02 TERMS</span>
                <span className="text-violet-400">[MIT LICENSE]</span>
              </div>
              <h4 className="text-base font-bold text-[#F5F4F0] mb-2">Terms of Service</h4>
              <p className="text-xs text-[#9E9B94] font-sans leading-relaxed">
                Permissive MIT open source licensing for personal and enterprise deployments with clear liability limitations.
              </p>
            </div>
            <button
              onClick={() => onOpenLegal('terms')}
              className="mt-6 pt-3 border-t border-[#242220] text-violet-400 hover:text-violet-300 flex items-center justify-between cursor-pointer"
            >
              <span>Read Terms</span>
              <ArrowRight className="w-3.5 h-3.5" />
            </button>
          </div>

          {/* Item 3 */}
          <div className="bg-[#121110] hover:bg-[#161513] transition-colors p-6 flex flex-col justify-between">
            <div>
              <div className="text-[#66635C] mb-3 flex items-center justify-between">
                <span>// 03 SECURITY</span>
                <span className="text-emerald-400">[TLS 1.3]</span>
              </div>
              <h4 className="text-base font-bold text-[#F5F4F0] mb-2">Security Whitepaper</h4>
              <p className="text-xs text-[#9E9B94] font-sans leading-relaxed">
                Dual in-flight SHA-256 + MD5 cryptographic pipeline, CSPRNG daemon Bearer token authentication, and volatile RAM isolation.
              </p>
            </div>
            <button
              onClick={() => onOpenLegal('security')}
              className="mt-6 pt-3 border-t border-[#242220] text-emerald-400 hover:text-emerald-300 flex items-center justify-between cursor-pointer"
            >
              <span>View Specs</span>
              <ArrowRight className="w-3.5 h-3.5" />
            </button>
          </div>
        </div>
      </div>
    </section>
  );
}
