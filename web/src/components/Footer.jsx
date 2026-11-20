import React from 'react';
import { Shield, FileText, Lock, Terminal, Heart } from 'lucide-react';
import { GithubIcon } from './Icons.jsx';

export default function Footer({ onOpenLegal }) {
  return (
    <footer className="bg-[#05070a] border-t border-slate-900 py-16 text-xs text-slate-400">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="grid grid-cols-1 md:grid-cols-4 gap-10 mb-12">
          {/* Brand Col */}
          <div className="space-y-4 md:col-span-1">
            <div className="flex items-center gap-2.5">
              <div className="w-8 h-8 rounded-lg bg-gradient-to-br from-cyan-500 to-violet-600 flex items-center justify-center font-bold text-white font-mono text-sm shadow-md">
                ▲
              </div>
              <span className="font-bold text-lg text-white font-mono tracking-tight">CTP Engine</span>
            </div>
            <p className="text-slate-400 text-xs leading-relaxed">
              Universal Cloud Transfer Protocol (UTP). High-performance zero-disk memory pipeline for personal and enterprise cloud migration.
            </p>
            <div className="flex items-center gap-2 text-[11px] font-mono text-emerald-400">
              <span className="w-2 h-2 rounded-full bg-emerald-400 animate-pulse" />
              <span>v2.6.0-pro Operational</span>
            </div>
          </div>

          {/* Nav Links */}
          <div className="space-y-3">
            <h4 className="font-bold text-white text-xs uppercase tracking-wider font-mono">Product</h4>
            <ul className="space-y-2">
              <li><a href="#features" className="hover:text-cyan-400 transition-colors">Core Features</a></li>
              <li><a href="#terminal-demo" className="hover:text-cyan-400 transition-colors">Terminal Simulator</a></li>
              <li><a href="#protocols" className="hover:text-cyan-400 transition-colors">Supported Protocols</a></li>
              <li><a href="#architecture" className="hover:text-cyan-400 transition-colors">Zero-Disk Architecture</a></li>
            </ul>
          </div>

          {/* Legal Hub */}
          <div className="space-y-3">
            <h4 className="font-bold text-white text-xs uppercase tracking-wider font-mono">Compliance & Trust</h4>
            <ul className="space-y-2">
              <li>
                <button onClick={() => onOpenLegal('privacy')} className="hover:text-cyan-400 transition-colors text-left cursor-pointer">
                  Zero-Knowledge Privacy Policy
                </button>
              </li>
              <li>
                <button onClick={() => onOpenLegal('terms')} className="hover:text-cyan-400 transition-colors text-left cursor-pointer">
                  Terms of Service (MIT License)
                </button>
              </li>
              <li>
                <button onClick={() => onOpenLegal('security')} className="hover:text-cyan-400 transition-colors text-left cursor-pointer">
                  Security & Token Isolation Whitepaper
                </button>
              </li>
            </ul>
          </div>

          {/* Ecosystem Links */}
          <div className="space-y-3">
            <h4 className="font-bold text-white text-xs uppercase tracking-wider font-mono">Developer Ecosystem</h4>
            <ul className="space-y-2">
              <li>
                <a 
                  href="https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol" 
                  target="_blank" 
                  rel="noopener noreferrer" 
                  className="hover:text-white transition-colors flex items-center gap-1.5"
                >
                  <GithubIcon className="w-3.5 h-3.5 text-slate-300" />
                  GitHub Repository
                </a>
              </li>
              <li>
                <a 
                  href="https://www.npmjs.com/package/@utp/client" 
                  target="_blank" 
                  rel="noopener noreferrer" 
                  className="hover:text-white transition-colors"
                >
                  TypeScript SDK (@utp/client)
                </a>
              </li>
              <li>
                <a 
                  href="https://pypi.org/project/utp-client/" 
                  target="_blank" 
                  rel="noopener noreferrer" 
                  className="hover:text-white transition-colors"
                >
                  Python SDK (utp-client)
                </a>
              </li>
              <li>
                <a 
                  href="https://arpitnayak.tech" 
                  target="_blank" 
                  rel="noopener noreferrer" 
                  className="text-cyan-400 hover:text-cyan-300 transition-colors font-medium"
                >
                  Author: arpitnayak.tech
                </a>
              </li>
            </ul>
          </div>
        </div>

        {/* Bottom Bar */}
        <div className="pt-8 border-t border-slate-900 flex flex-wrap items-center justify-between gap-4 text-slate-500 text-[11px] font-mono">
          <div>
            © {new Date().getFullYear()} Universal Cloud Transfer Protocol (CTP). Licensed under the MIT License.
          </div>
          <div className="flex items-center gap-4">
            <span className="text-slate-400">Domain: ctp.arpitnayak.tech</span>
            <span>•</span>
            <span className="text-slate-400">Zero-Disk Memory Pipeline</span>
          </div>
        </div>
      </div>
    </footer>
  );
}
