'use strict';

const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');

function sameResolvedPath(a, b) {
  try {
    return fs.realpathSync(a) === fs.realpathSync(b);
  } catch {
    return path.resolve(a) === path.resolve(b);
  }
}

function isWrapperShim(candidate, selfDir, scriptPath) {
  const base = path.basename(candidate).toLowerCase();
  if (base.endsWith('.js') || base.endsWith('.cmd') || base.endsWith('.ps1')) {
    return true;
  }
  try {
    const realTarget = fs.realpathSync(candidate);
    if (realTarget.toLowerCase().endsWith('.js')) {
      return true;
    }
    if (scriptPath && sameResolvedPath(realTarget, scriptPath)) {
      return true;
    }
    if (path.dirname(realTarget) === fs.realpathSync(selfDir)) {
      return !candidate.toLowerCase().endsWith('.exe');
    }
  } catch {
    return false;
  }
  return false;
}

function findInPath(names, selfDir, scriptPath) {
  const pathEnv = process.env.PATH || '';
  const dirs = pathEnv.split(path.delimiter).filter(Boolean);
  for (const dir of dirs) {
    for (const name of names) {
      const candidate = path.join(dir, name);
      try {
        const stat = fs.statSync(candidate);
        if (stat.isFile() && !isWrapperShim(candidate, selfDir, scriptPath)) {
          return candidate;
        }
      } catch {
        // Ignore missing/unreadable entries
      }
    }
  }
  return null;
}

function resolveNativeBinary(names, scriptPath) {
  const binDir = __dirname;
  const pkgRoot = path.join(binDir, '..');
  const monorepoRoot = path.join(binDir, '..', '..');
  const isMonorepo = fs.existsSync(path.join(monorepoRoot, 'CMakeLists.txt'));

  const candidateDirs = [binDir, pkgRoot];
  if (isMonorepo) {
    candidateDirs.push(monorepoRoot);
  }

  for (const dir of candidateDirs) {
    for (const name of names) {
      const candidate = path.join(dir, name);
      try {
        if (fs.statSync(candidate).isFile()) {
          return candidate;
        }
      } catch {
        // Continue
      }
    }
  }

  return findInPath(names, binDir, scriptPath);
}

function launchBinary({ names, label, installHint, scriptPath }) {
  const platform = os.platform();
  const exeNames = names.map((n) => (platform === 'win32' ? `${n}.exe` : n));
  const binaryPath = resolveNativeBinary(exeNames, scriptPath);

  if (!binaryPath) {
    console.error(`\x1b[31m[${label} Error]\x1b[0m Native binary '${exeNames[0]}' was not found.`);
    console.error(`Run 'node "${path.join(__dirname, '..', 'scripts', 'install.js')}"' or ${installHint}`);
    process.exit(1);
  }

  const child = spawn(binaryPath, process.argv.slice(2), {
    stdio: 'inherit',
    shell: false
  });

  child.on('error', (err) => {
    if (err.code === 'ENOENT') {
      console.error(`\x1b[31m[${label} Error]\x1b[0m Failed to locate executable '${binaryPath}'.`);
    } else {
      console.error(`\x1b[31m[${label} Error]\x1b[0m Failed to execute:`, err.message);
    }
    process.exit(1);
  });

  child.on('exit', (code, signal) => {
    if (signal) {
      process.kill(process.pid, signal);
    } else {
      process.exit(code ?? 0);
    }
  });
}

module.exports = { launchBinary };
