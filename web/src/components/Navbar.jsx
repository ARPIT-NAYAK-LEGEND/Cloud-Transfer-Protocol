import React, { useState, useEffect } from 'react';
import { Terminal, Shield, FileText, Lock, Check, Copy, Menu, X, ArrowUpRight } from 'lucide-react';
import { GithubIcon } from './Icons.jsx';

export default function Navbar({ onOpenLegal }) {
  const [copied, setCopied] = useState(false);
  const [timeStr, setTimeStr] = useState('--:--:-- GMT');
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false);

  // Live GMT/UTC Clock matching Aonnoy's distinctive live time indicator
  useEffect(() => {
    const updateTime = () => {
      const now = new Date();
      const hours = String(now.getUTCHours()).padStart(2, '0');
      const minutes = String(now.getUTCMinutes()).padStart(2, '0');
      const seconds = String(now.getUTCSeconds()).padStart(2, '0');
      setTimeStr(`${hours}:${minutes}:${seconds} GMT`);
    };
    updateTime();
    const interval = setInterval(updateTime, 1000);
    return () => clearInterval(interval);
  }, []);

  const copyInstall = () => {
    navigator.clipboard.writeText('npx utp-engine transfer');
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <header className="fixed top-0 left-0 right-0 z-50 bg-[#0E0D0C]/90 backdrop-blur-md border-b border-[#242220]">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="flex items-center justify-between h-16">
          {/* Brand Mark */}
          <a href="#" className="flex items-center gap-3 group">
            <span className="font-mono text-sm font-semibold tracking-tight text-[#F5F4F0] group-hover:text-[#FF5B1F] transition-colors">
              /*ctp*/<span className="text-[#FF5B1F]">.</span>
            </span>
            <span className="text-[11px] font-mono px-2 py-0.5 rounded border border-[#33302C] bg-[#161513] text-[#9E9B94]">
              v2.6.0-pro
            </span>
          </a>

          {/* Center Status Beacon */}
          <div className="hidden lg:flex items-center gap-2 font-mono text-xs text-[#9E9B94]">
            <span className="live-beacon" />
            <span>// direct memory pipeline</span>
            <span className="text-[#66635C]">•</span>
            <span className="text-[#F5F4F0]">0 bytes disk wear</span>
          </div>

          {/* Right Navigation & Live Clock */}
          <div className="hidden md:flex items-center gap-6 font-mono text-xs">
            <div className="flex items-center gap-1.5 text-[#66635C] pr-4 border-r border-[#242220]">
              <span className="text-[#9E9B94]">{timeStr}</span>
            </div>

            <nav className="flex items-center gap-5 text-[#9E9B94]">
              <a href="#simulator" className="hover:text-[#F5F4F0] transition-colors">
                <span className="text-[#66635C] mr-1">01</span>Simulator
              </a>
              <a href="#features" className="hover:text-[#F5F4F0] transition-colors">
                <span className="text-[#66635C] mr-1">02</span>Feats
              </a>
              <a href="#protocols" className="hover:text-[#F5F4F0] transition-colors">
                <span className="text-[#66635C] mr-1">03</span>Protocols
              </a>
              <a href="#architecture" className="hover:text-[#F5F4F0] transition-colors">
                <span className="text-[#66635C] mr-1">04</span>Specs
              </a>
              <button
                onClick={() => onOpenLegal('privacy')}
                className="hover:text-[#FF5B1F] transition-colors cursor-pointer"
              >
                <span className="text-[#66635C] mr-1">05</span>Legal
              </button>
            </nav>

            <button
              onClick={copyInstall}
              className="flex items-center gap-1.5 px-3 py-1.5 rounded bg-[#161513] hover:bg-[#201E1C] border border-[#33302C] text-[#F5F4F0] transition-all cursor-pointer hover:border-[#FF5B1F]/60"
              title="Copy instant run command"
            >
              <Terminal className="w-3.5 h-3.5 text-[#FF5B1F]" />
              <span>npx utp-engine</span>
              {copied ? (
                <Check className="w-3 h-3 text-emerald-400" />
              ) : (
                <Copy className="w-3 h-3 text-[#66635C]" />
              )}
            </button>

            <a
              href="https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol"
              target="_blank"
              rel="noopener noreferrer"
              className="p-1.5 text-[#9E9B94] hover:text-[#F5F4F0] transition-colors"
              title="GitHub Repository"
            >
              <GithubIcon className="w-4 h-4" />
            </a>
          </div>

          {/* Mobile menu trigger */}
          <div className="md:hidden flex items-center gap-2">
            <button
              onClick={() => setMobileMenuOpen(!mobileMenuOpen)}
              className="p-2 rounded border border-[#242220] bg-[#161513] text-[#9E9B94] hover:text-[#F5F4F0] font-mono text-xs"
            >
              {mobileMenuOpen ? '// close' : '// menu'}
            </button>
          </div>
        </div>
      </div>

      {/* Mobile Drawer */}
      {mobileMenuOpen && (
        <div className="md:hidden bg-[#0E0D0C] border-b border-[#242220] px-4 py-6 font-mono text-xs space-y-4">
          <div className="text-[#66635C] pb-2 border-b border-[#242220]">
            TIME: {timeStr} • STATUS: READY
          </div>
          <a
            href="#simulator"
            onClick={() => setMobileMenuOpen(false)}
            className="block text-[#9E9B94] hover:text-[#F5F4F0]"
          >
            <span className="text-[#66635C] mr-2">01</span>Interactive Simulator
          </a>
          <a
            href="#features"
            onClick={() => setMobileMenuOpen(false)}
            className="block text-[#9E9B94] hover:text-[#F5F4F0]"
          >
            <span className="text-[#66635C] mr-2">02</span>Engineering Feats
          </a>
          <a
            href="#protocols"
            onClick={() => setMobileMenuOpen(false)}
            className="block text-[#9E9B94] hover:text-[#F5F4F0]"
          >
            <span className="text-[#66635C] mr-2">03</span>Supported Protocols
          </a>
          <a
            href="#architecture"
            onClick={() => setMobileMenuOpen(false)}
            className="block text-[#9E9B94] hover:text-[#F5F4F0]"
          >
            <span className="text-[#66635C] mr-2">04</span>Zero-Disk Architecture
          </a>
          <div className="pt-3 border-t border-[#242220] flex gap-4">
            <button
              onClick={() => { setMobileMenuOpen(false); onOpenLegal('privacy'); }}
              className="text-[#FF5B1F] flex items-center gap-1 cursor-pointer"
            >
              // privacy policy
            </button>
            <button
              onClick={() => { setMobileMenuOpen(false); onOpenLegal('terms'); }}
              className="text-[#F5F4F0] flex items-center gap-1 cursor-pointer"
            >
              // terms of service
            </button>
          </div>
        </div>
      )}
    </header>
  );
}
