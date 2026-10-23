#!/usr/bin/env node

const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');

const platform = os.platform();
const arch = os.arch();

let binaryName = 'utp';
if (platform === 'win32') {
  binaryName = 'utp.exe';
}

const candidatePaths = [
  path.join(__dirname, binaryName),
  path.join(__dirname, '..', binaryName),
  path.join(__dirname, '..', '..', binaryName)
];

let binaryPath = candidatePaths.find(p => fs.existsSync(p)) || binaryName;

const child = spawn(binaryPath, process.argv.slice(2), {
  stdio: 'inherit',
  shell: false
});

child.on('error', (err) => {
  if (err.code === 'ENOENT') {
    console.error(`\x1b[31m[UTP Error]\x1b[0m Native UTP binary '${binaryName}' was not found.`);
    console.error(`Please compile UTP or ensure 'utp' is available in your PATH.`);
  } else {
    console.error(`\x1b[31m[UTP Error]\x1b[0m Failed to execute UTP engine:`, err.message);
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
