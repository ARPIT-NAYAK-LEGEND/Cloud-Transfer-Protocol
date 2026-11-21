import React, { useState, useEffect } from 'react';
import Navbar from './components/Navbar.jsx';
import Hero from './components/Hero.jsx';
import QuickstartGuide from './components/QuickstartGuide.jsx';
import TerminalSimulation from './components/TerminalSimulation.jsx';
import FeatureGrid from './components/FeatureGrid.jsx';
import ProtocolMatrix from './components/ProtocolMatrix.jsx';
import ComplianceHub from './components/ComplianceHub.jsx';
import Footer from './components/Footer.jsx';
import LegalModals from './components/LegalModals.jsx';

export default function App() {
  const [legalModalTab, setLegalModalTab] = useState(null); // 'privacy' | 'terms' | 'security' | null

  // Support direct deep-linking via URL hash (#privacy, #terms, #security) or path (/privacy, /terms, /security)
  useEffect(() => {
    const checkRoute = () => {
      const hash = window.location.hash.toLowerCase();
      const path = window.location.pathname.toLowerCase();

      if (hash === '#privacy' || hash === '#/privacy' || path === '/privacy') {
        setLegalModalTab('privacy');
      } else if (hash === '#terms' || hash === '#/terms' || path === '/terms') {
        setLegalModalTab('terms');
      } else if (hash === '#security' || hash === '#/security' || path === '/security') {
        setLegalModalTab('security');
      }
    };

    checkRoute();
    window.addEventListener('hashchange', checkRoute);
    window.addEventListener('popstate', checkRoute);
    return () => {
      window.removeEventListener('hashchange', checkRoute);
      window.removeEventListener('popstate', checkRoute);
    };
  }, []);

  const handleOpenLegal = (tab) => {
    setLegalModalTab(tab);
    window.history.pushState(null, '', `/${tab}`);
  };

  const handleCloseLegal = () => {
    setLegalModalTab(null);
    window.history.pushState(null, '', '/');
  };

  return (
    <div className="min-h-screen bg-[#0E0D0C] text-[#F5F4F0] flex flex-col relative selection:bg-[#FF5B1F]/30 selection:text-[#FF5B1F]">
      {/* Top Fixed Navbar */}
      <Navbar onOpenLegal={handleOpenLegal} />

      {/* Main Content */}
      <main className="flex-1">
        <Hero onOpenLegal={handleOpenLegal} />
        <QuickstartGuide />
        <TerminalSimulation />
        <FeatureGrid />
        <ProtocolMatrix />
        <ComplianceHub onOpenLegal={handleOpenLegal} />
      </main>

      {/* Structural Footer */}
      <Footer onOpenLegal={handleOpenLegal} />

      {/* Store Review & Compliance Modals */}
      <LegalModals
        activeTab={legalModalTab}
        onClose={handleCloseLegal}
        onSelectTab={(tab) => {
          setLegalModalTab(tab);
          window.history.pushState(null, '', `/${tab}`);
        }}
      />
    </div>
  );
}
