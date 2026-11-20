import React, { useState, useEffect } from 'react';
import { Terminal, Shield, FileText, Lock, Check, Copy, ExternalLink, Menu, X } from 'lucide-react';
import { GithubIcon } from './Icons.jsx';

export default function Navbar({ onOpenLegal }) {
  const [copied, setCopied] = useState(false);
  const [scrolled, setScrolled] = useState(false);
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false);

  useEffect(() => {
    const handleScroll = () => {
      setScrolled(window.scrollY > 20);
    };
    window.addEventListener('scroll', handleScroll);
    return () => window.removeEventListener('scroll', handleScroll);
  }, []);

  const copyInstall = () => {
    navigator.clipboard.writeText('npx utp-engine transfer');
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <nav className={`fixed top-0 left-0 right-0 z-50 transition-all duration-300 ${
      scrolled ? 'bg-[#07090e]/85 backdrop-blur-md border-b border-white/10 shadow-2xl shadow-cyan-950/20' : 'bg-transparent border-b border-transparent'
    }`}>
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="flex items-center justify-between h-18">
          {/* Brand Logo */}
          <a href="#" className="flex items-center gap-3 group">
            <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-cyan-500 via-sky-600 to-violet-600 p-[1px] shadow-lg shadow-cyan-500/25 group-hover:shadow-cyan-500/40 transition-all">
              <div className="w-full h-full bg-[#07090e] rounded-[11px] flex items-center justify-center font-bold text-white text-lg tracking-wider font-mono">
                ▲
              </div>
            </div>
            <div className="flex flex-col">
              <div className="flex items-center gap-2">
                <span className="font-extrabold text-xl tracking-tight text-white group-hover:text-cyan-400 transition-colors">
                  CTP
                </span>
                <span className="text-[10px] font-mono px-2 py-0.5 rounded-full bg-cyan-500/10 text-cyan-400 border border-cyan-500/30 font-semibold">
                  v2.6.0-pro
                </span>
              </div>
              <span className="text-[11px] text-slate-400 font-mono tracking-tight hidden sm:inline">
                Universal Cloud Transfer Protocol
              </span>
            </div>
          </a>

          {/* Desktop Nav Links */}
          <div className="hidden md:flex items-center gap-7 text-sm font-medium text-slate-300">
            <a href="#features" className="hover:text-cyan-400 transition-colors">Features</a>
            <a href="#terminal-demo" className="hover:text-cyan-400 transition-colors">Terminal Demo</a>
            <a href="#protocols" className="hover:text-cyan-400 transition-colors">Protocols</a>
            <a href="#architecture" className="hover:text-cyan-400 transition-colors">Architecture</a>
            <button 
              onClick={() => onOpenLegal('privacy')} 
              className="hover:text-cyan-400 transition-colors flex items-center gap-1 cursor-pointer"
            >
              <Shield className="w-3.5 h-3.5 text-cyan-400" />
              Privacy
            </button>
            <button 
              onClick={() => onOpenLegal('terms')} 
              className="hover:text-cyan-400 transition-colors flex items-center gap-1 cursor-pointer"
            >
              <FileText className="w-3.5 h-3.5 text-violet-400" />
              Terms
            </button>
          </div>

          {/* Action CTAs */}
          <div className="hidden sm:flex items-center gap-3">
            <button
              onClick={copyInstall}
              className="flex items-center gap-2 px-3.5 py-2 rounded-lg bg-slate-900 hover:bg-slate-800 border border-slate-700/80 text-xs font-mono text-slate-200 transition-all cursor-pointer shadow-inner hover:border-cyan-500/50"
              title="Copy instant run command"
            >
              <Terminal className="w-3.5 h-3.5 text-cyan-400" />
              <span>npx utp-engine</span>
              {copied ? (
                <Check className="w-3.5 h-3.5 text-emerald-400" />
              ) : (
                <Copy className="w-3.5 h-3.5 text-slate-400" />
              )}
            </button>

            <a
              href="https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol"
              target="_blank"
              rel="noopener noreferrer"
              className="p-2.5 rounded-lg bg-slate-900/80 hover:bg-slate-800 border border-slate-700 text-slate-200 hover:text-white transition-colors"
              title="GitHub Repository"
            >
              <GithubIcon className="w-4 h-4" />
            </a>
          </div>

          {/* Mobile menu button */}
          <div className="md:hidden flex items-center">
            <button
              onClick={() => setMobileMenuOpen(!mobileMenuOpen)}
              className="p-2 rounded-lg text-slate-400 hover:text-white hover:bg-slate-800 transition-colors"
            >
              {mobileMenuOpen ? <X className="w-6 h-6" /> : <Menu className="w-6 h-6" />}
            </button>
          </div>
        </div>
      </div>

      {/* Mobile Drawer */}
      {mobileMenuOpen && (
        <div className="md:hidden bg-[#07090e]/95 backdrop-blur-xl border-b border-slate-800 px-4 pt-3 pb-6 space-y-3">
          <a
            href="#features"
            onClick={() => setMobileMenuOpen(false)}
            className="block px-3 py-2 rounded-md text-base font-medium text-slate-200 hover:bg-slate-800"
          >
            Features
          </a>
          <a
            href="#terminal-demo"
            onClick={() => setMobileMenuOpen(false)}
            className="block px-3 py-2 rounded-md text-base font-medium text-slate-200 hover:bg-slate-800"
          >
            Terminal Demo
          </a>
          <a
            href="#protocols"
            onClick={() => setMobileMenuOpen(false)}
            className="block px-3 py-2 rounded-md text-base font-medium text-slate-200 hover:bg-slate-800"
          >
            Supported Protocols
          </a>
          <a
            href="#architecture"
            onClick={() => setMobileMenuOpen(false)}
            className="block px-3 py-2 rounded-md text-base font-medium text-slate-200 hover:bg-slate-800"
          >
            Architecture
          </a>
          <div className="pt-2 border-t border-slate-800 flex gap-4">
            <button
              onClick={() => { setMobileMenuOpen(false); onOpenLegal('privacy'); }}
              className="text-sm text-cyan-400 flex items-center gap-1"
            >
              <Shield className="w-4 h-4" /> Privacy Policy
            </button>
            <button
              onClick={() => { setMobileMenuOpen(false); onOpenLegal('terms'); }}
              className="text-sm text-violet-400 flex items-center gap-1"
            >
              <FileText className="w-4 h-4" /> Terms of Service
            </button>
          </div>
        </div>
      )}
    </nav>
  );
}
