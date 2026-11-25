import React from 'react';
import { ArrowUpRight } from 'lucide-react';
import { GithubIcon } from './Icons.jsx';

export default function Footer({ onOpenLegal }) {
  const scrollToTop = () => {
    window.scrollTo({ top: 0, behavior: 'smooth' });
  };

  return (
    <footer className="bg-[#0A0908] border-t border-[#242220] py-16 font-mono text-xs text-[#9E9B94]">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="grid grid-cols-1 md:grid-cols-4 gap-10 mb-12">
          {/* Col 1: Brand */}
          <div className="space-y-3">
            <div className="flex items-center gap-2.5">
              <img
                src="/logo.png"
                alt="Cloud Transfer Protocol Logo"
                className="w-7 h-7 rounded-full object-cover border border-[#33302C]"
              />
              <div className="text-[#F5F4F0] font-bold text-sm">
                Cloud Transfer Protocol<span className="text-[#FF5B1F]">.</span>
              </div>
            </div>
            <p className="text-[#66635C] leading-relaxed font-sans text-xs">
              Cloud Transfer Protocol (CTP). Direct zero-disk volatile RAM memory streaming between Google Drive, OneDrive, S3, and Cloudflare R2.
            </p>
            <div className="flex items-center gap-2 text-[#9E9B94]">
              <span className="live-beacon" />
              <span>v2.6.2 operational</span>
            </div>
          </div>

          {/* Col 2: Architecture & Specs */}
          <div className="space-y-2.5">
            <div className="text-[#66635C]">// architecture</div>
            <ul className="space-y-1.5">
              <li><a href="#quickstart" className="hover:text-[#F5F4F0] transition-colors">01 / Quickstart Guide</a></li>
              <li><a href="#simulator" className="hover:text-[#F5F4F0] transition-colors">02 / Runtime Simulator</a></li>
              <li><a href="#features" className="hover:text-[#F5F4F0] transition-colors">03 / Engineering Feats</a></li>
              <li><a href="#protocols" className="hover:text-[#F5F4F0] transition-colors">04 / Supported Protocols</a></li>
              <li><a href="#compliance" className="hover:text-[#F5F4F0] transition-colors">05 / Compliance Specs</a></li>
            </ul>
          </div>

          {/* Col 3: Compliance & Legal */}
          <div className="space-y-2.5">
            <div className="text-[#66635C]">// compliance</div>
            <ul className="space-y-1.5">
              <li>
                <button onClick={() => onOpenLegal('privacy')} className="hover:text-[#FF5B1F] transition-colors cursor-pointer text-left">
                  Zero-Knowledge Privacy Policy
                </button>
              </li>
              <li>
                <button onClick={() => onOpenLegal('terms')} className="hover:text-[#F5F4F0] transition-colors cursor-pointer text-left">
                  Terms of Service (MIT)
                </button>
              </li>
              <li>
                <button onClick={() => onOpenLegal('security')} className="hover:text-emerald-400 transition-colors cursor-pointer text-left">
                  Security Architecture Whitepaper
                </button>
              </li>
            </ul>
          </div>

          {/* Col 4: Ecosystem & Author */}
          <div className="space-y-2.5">
            <div className="text-[#66635C]">// elsewhere</div>
            <ul className="space-y-1.5">
              <li>
                <a
                  href="https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="hover:text-[#F5F4F0] transition-colors flex items-center gap-1.5"
                >
                  <GithubIcon className="w-3.5 h-3.5" />
                  <span>GitHub Repository</span>
                </a>
              </li>
              <li>
                <a
                  href="https://www.npmjs.com/package/utp-engine"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="hover:text-[#F5F4F0] transition-colors flex items-center gap-1"
                >
                  <span className="text-[#FF5B1F] font-semibold">CLI Package (utp-engine)</span>
                  <ArrowUpRight className="w-3 h-3 text-[#66635C]" />
                </a>
              </li>
              <li>
                <a
                  href="https://www.npmjs.com/package/@utp/client"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="hover:text-[#F5F4F0] transition-colors flex items-center gap-1"
                >
                  <span>TypeScript SDK (@utp/client)</span>
                  <ArrowUpRight className="w-3 h-3 text-[#66635C]" />
                </a>
              </li>
              <li>
                <a
                  href="https://pypi.org/project/utp-client/"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="hover:text-[#F5F4F0] transition-colors flex items-center gap-1"
                >
                  <span>Python SDK (utp-client)</span>
                  <ArrowUpRight className="w-3 h-3 text-[#66635C]" />
                </a>
              </li>
              <li>
                <a
                  href="https://arpitnayak.tech"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="text-[#FF5B1F] hover:underline flex items-center gap-1 font-semibold"
                >
                  <span>Built by Nayak & Nayak Co.</span>
                  <ArrowUpRight className="w-3 h-3" />
                </a>
              </li>
            </ul>
          </div>
        </div>

        {/* Bottom Bar */}
        <div className="pt-8 border-t border-[#242220] flex flex-wrap items-center justify-between gap-4 text-[#66635C]">
          <div>
            © {new Date().getFullYear()} Universal Cloud Transfer Protocol (CTP/UTP). MIT License.
          </div>
          <div className="flex items-center gap-6">
            <span>
              Built by{' '}
              <a
                href="https://arpitnayak.tech"
                target="_blank"
                rel="noopener noreferrer"
                className="text-[#FF5B1F] hover:underline font-semibold transition-colors"
              >
                Nayak & Nayak Co.
              </a>
            </span>
            <button
              onClick={scrollToTop}
              className="text-[#9E9B94] hover:text-[#F5F4F0] cursor-pointer flex items-center gap-1"
            >
              <span>Back to top</span>
              <span>↑</span>
            </button>
          </div>
        </div>
      </div>
    </footer>
  );
}
