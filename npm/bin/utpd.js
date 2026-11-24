#!/usr/bin/env node

const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');

const platform = os.platform();
const binaryName = platform === 'win32' ? 'utpd.exe' : 'utpd';
const altBinaryName = platform === 'win32' ? 'utp-server.exe' : 'utp-server';

const candidatePaths = [
  path.join(__dirname, binaryName),
  path.join(__dirname, altBinaryName),
  path.join(__dirname, '..', binaryName),
  path.join(__dirname, '..', altBinaryName),
  path.join(__dirname, '..', '..', binaryName),
  path.join(__dirname, '..', '..', altBinaryName)
];

let binaryPath = candidatePaths.find(p => fs.existsSync(p)) || binaryName;

const child = spawn(binaryPath, process.argv.slice(2), {
  stdio: 'inherit',
  shell: false
});

child.on('error', (err) => {
  if (err.code === 'ENOENT') {
    console.error(`\x1b[31m[UTPD Error]\x1b[0m Native UTP daemon binary '${binaryName}' was not found.`);
    console.error(`Please compile UTP server or ensure 'utpd' is available in your PATH.`);
  } else {
    console.error(`\x1b[31m[UTPD Error]\x1b[0m Failed to execute UTP daemon:`, err.message);
  }
  process.exit(1);
});

child.on('exit', (code, signal) => {
  if (signal) {
    process.kill(process.pid, signal);
  } else {
    process.exit(code || 0);
  }
});
