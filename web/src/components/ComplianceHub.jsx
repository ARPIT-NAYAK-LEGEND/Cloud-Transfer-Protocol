import React from 'react';
import { Shield, FileText, Lock, CheckCircle2, ExternalLink, ArrowRight } from 'lucide-react';

export default function ComplianceHub({ onOpenLegal }) {
  return (
    <section id="compliance" className="py-24 relative">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="text-center max-w-3xl mx-auto mb-16">
          <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-emerald-500/10 text-emerald-400 border border-emerald-500/20 text-xs font-mono font-medium mb-3">
            <Shield className="w-3.5 h-3.5" />
            Store Review & Enterprise Compliance
          </div>
          <h2 className="text-3xl sm:text-4xl font-extrabold text-white tracking-tight">
            Built for Privacy, Security & Compliance
          </h2>
          <p className="mt-4 text-slate-400 text-base">
            Whether for App Store and Google Play review approval, Stripe verification, or enterprise data auditing—CTP is designed with verifiable zero-knowledge privacy from day one.
          </p>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-3 gap-8">
          {/* Card 1: Privacy Policy */}
          <div className="p-8 rounded-2xl bg-slate-900/50 border border-slate-800 hover:border-cyan-500/40 transition-all flex flex-col justify-between group">
            <div>
              <div className="w-12 h-12 rounded-xl bg-cyan-500/10 border border-cyan-500/20 flex items-center justify-center text-cyan-400 mb-6 group-hover:scale-110 transition-transform">
                <Shield className="w-6 h-6" />
              </div>
              <h3 className="text-xl font-bold text-white group-hover:text-cyan-300 transition-colors">
                Zero-Knowledge Privacy Policy
              </h3>
              <p className="mt-3 text-slate-400 text-sm leading-relaxed">
                Zero telemetry, zero analytics, zero external logging. All transfers stream directly from cloud to cloud without any intermediary relay servers or diagnostic pings.
              </p>
              <div className="mt-5 space-y-2 text-xs text-slate-300">
                <div className="flex items-center gap-2">
                  <CheckCircle2 className="w-4 h-4 text-emerald-400" />
                  <span>No account creation or tracking cookies</span>
                </div>
                <div className="flex items-center gap-2">
                  <CheckCircle2 className="w-4 h-4 text-emerald-400" />
                  <span>Tokens saved locally in user directory</span>
                </div>
                <div className="flex items-center gap-2">
                  <CheckCircle2 className="w-4 h-4 text-emerald-400" />
                  <span>100% GDPR & CCPA compliant</span>
                </div>
              </div>
            </div>

            <button
              onClick={() => onOpenLegal('privacy')}
              className="mt-8 pt-4 border-t border-slate-800/80 flex items-center justify-between text-sm font-semibold text-cyan-400 group-hover:text-cyan-300 cursor-pointer"
            >
              <span>Read Full Privacy Policy</span>
              <ArrowRight className="w-4 h-4 group-hover:translate-x-1 transition-transform" />
            </button>
          </div>

          {/* Card 2: Terms of Service */}
          <div className="p-8 rounded-2xl bg-slate-900/50 border border-slate-800 hover:border-violet-500/40 transition-all flex flex-col justify-between group">
            <div>
              <div className="w-12 h-12 rounded-xl bg-violet-500/10 border border-violet-500/20 flex items-center justify-center text-violet-400 mb-6 group-hover:scale-110 transition-transform">
                <FileText className="w-6 h-6" />
              </div>
              <h3 className="text-xl font-bold text-white group-hover:text-violet-300 transition-colors">
                Terms of Service & Licensing
              </h3>
              <p className="mt-3 text-slate-400 text-sm leading-relaxed">
                Licensed under the permissive MIT Open Source License. Complete terms governing personal, commercial, and enterprise usage of the CTP engine and SDKs.
              </p>
              <div className="mt-5 space-y-2 text-xs text-slate-300">
                <div className="flex items-center gap-2">
                  <CheckCircle2 className="w-4 h-4 text-emerald-400" />
                  <span>Permissive MIT License for personal & commercial</span>
                </div>
                <div className="flex items-center gap-2">
                  <CheckCircle2 className="w-4 h-4 text-emerald-400" />
                  <span>Clear liability & warranty disclaimers</span>
                </div>
                <div className="flex items-center gap-2">
                  <CheckCircle2 className="w-4 h-4 text-emerald-400" />
                  <span>Official cloud provider API compliance</span>
                </div>
              </div>
            </div>

            <button
              onClick={() => onOpenLegal('terms')}
              className="mt-8 pt-4 border-t border-slate-800/80 flex items-center justify-between text-sm font-semibold text-violet-400 group-hover:text-violet-300 cursor-pointer"
            >
              <span>Read Terms of Service</span>
              <ArrowRight className="w-4 h-4 group-hover:translate-x-1 transition-transform" />
            </button>
          </div>

          {/* Card 3: Security & Token Isolation */}
          <div className="p-8 rounded-2xl bg-slate-900/50 border border-slate-800 hover:border-emerald-500/40 transition-all flex flex-col justify-between group">
            <div>
              <div className="w-12 h-12 rounded-xl bg-emerald-500/10 border border-emerald-500/20 flex items-center justify-center text-emerald-400 mb-6 group-hover:scale-110 transition-transform">
                <Lock className="w-6 h-6" />
              </div>
              <h3 className="text-xl font-bold text-white group-hover:text-emerald-300 transition-colors">
                Security & Data Handling
              </h3>
              <p className="mt-3 text-slate-400 text-sm leading-relaxed">
                Cryptographic specifications, TLS 1.3 socket validation, client-side OAuth secret resolution, and background daemon process isolation parameters.
              </p>
              <div className="mt-5 space-y-2 text-xs text-slate-300">
                <div className="flex items-center gap-2">
                  <CheckCircle2 className="w-4 h-4 text-emerald-400" />
                  <span>In-flight SHA-256 + MD5 dual pipeline</span>
                </div>
                <div className="flex items-center gap-2">
                  <CheckCircle2 className="w-4 h-4 text-emerald-400" />
                  <span>Daemon Bearer token authentication</span>
                </div>
                <div className="flex items-center gap-2">
                  <CheckCircle2 className="w-4 h-4 text-emerald-400" />
                  <span>Zero local temp-file residue</span>
                </div>
              </div>
            </div>

            <button
              onClick={() => onOpenLegal('security')}
              className="mt-8 pt-4 border-t border-slate-800/80 flex items-center justify-between text-sm font-semibold text-emerald-400 group-hover:text-emerald-300 cursor-pointer"
            >
              <span>View Security Specs</span>
              <ArrowRight className="w-4 h-4 group-hover:translate-x-1 transition-transform" />
            </button>
          </div>
        </div>
      </div>
    </section>
  );
}
