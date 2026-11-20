import React, { useState, useEffect } from 'react';
import Navbar from './components/Navbar.jsx';
import Hero from './components/Hero.jsx';
import TerminalSimulation from './components/TerminalSimulation.jsx';
import FeatureGrid from './components/FeatureGrid.jsx';
import ArchitectureDiagram from './components/ArchitectureDiagram.jsx';
import ProtocolMatrix from './components/ProtocolMatrix.jsx';
import ComplianceHub from './components/ComplianceHub.jsx';
import Footer from './components/Footer.jsx';
import LegalModals from './components/LegalModals.jsx';

export default function App() {
  const [legalModalTab, setLegalModalTab] = useState(null); // 'privacy' | 'terms' | 'security' | null

  // Support direct deep-linking via URL hash (e.g., #privacy, #terms, #security) or path (/privacy, /terms, /security)
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
    <div className="min-h-screen bg-[#07090e] text-slate-100 flex flex-col relative overflow-hidden bg-cyber-grid">
      {/* Top subtle ambient mesh */}
      <div className="absolute top-0 left-0 right-0 h-96 glow-mesh pointer-events-none" />

      {/* Top Fixed Navbar */}
      <Navbar onOpenLegal={handleOpenLegal} />

      {/* Main Content */}
      <main className="flex-1">
        <Hero onOpenLegal={handleOpenLegal} />
        <TerminalSimulation />
        <FeatureGrid />
        <ArchitectureDiagram />
        <ProtocolMatrix />
        <ComplianceHub onOpenLegal={handleOpenLegal} />
      </main>

      {/* Footer */}
      <Footer onOpenLegal={handleOpenLegal} />

      {/* Store Review & Compliance Modals */}
      <LegalModals
        activeTab={legalModalTab}
        onClose={handleCloseLegal}
        onSelectTab={(tab) => {
          setLegalModalTab(tab);
          window.history.pushState(null, '', `#${tab}`);
        }}
      />
    </div>
  );
}
