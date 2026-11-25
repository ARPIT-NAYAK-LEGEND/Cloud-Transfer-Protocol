'use strict';

const fs = require('fs');
const path = require('path');
const https = require('https');
const crypto = require('crypto');
const os = require('os');
const { execFileSync } = require('child_process');

const platform = os.platform();
const ext = platform === 'win32' ? '.exe' : '';
const cliBinaryName = `utp${ext}`;
const daemonBinaryName = `utpd${ext}`;

const targetDir = path.join(__dirname, '..', 'bin');
const targetCliFile = path.join(targetDir, cliBinaryName);
const targetDaemonFile = path.join(targetDir, daemonBinaryName);

function hasFile(p) {
  try {
    return fs.statSync(p).isFile();
  } catch {
    return false;
  }
}

// 1. If both binaries already exist in bin/, nothing to do
if (hasFile(targetCliFile) && hasFile(targetDaemonFile)) {
  process.exit(0);
}

// 2. If running inside a local monorepo checkout, reuse compiled binaries safely
const monorepoRoot = path.join(__dirname, '..', '..');
if (hasFile(path.join(monorepoRoot, 'CMakeLists.txt'))) {
  const rootCli = path.join(monorepoRoot, cliBinaryName);
  const rootDaemon = path.join(monorepoRoot, daemonBinaryName);
  const altDaemon = path.join(monorepoRoot, platform === 'win32' ? 'utp-server.exe' : 'utp-server');

  try {
    if (!hasFile(targetCliFile) && hasFile(rootCli)) {
      fs.copyFileSync(rootCli, targetCliFile);
    }
    if (!hasFile(targetDaemonFile)) {
      const daemonSrc = hasFile(rootDaemon) ? rootDaemon : (hasFile(altDaemon) ? altDaemon : null);
      if (daemonSrc) {
        fs.copyFileSync(daemonSrc, targetDaemonFile);
      }
    }
    if (hasFile(targetCliFile) && hasFile(targetDaemonFile)) {
      process.exit(0);
    }
  } catch {
    // Fall through to release download
  }
}

// 3. Download platform prebuilt release archive over HTTPS
const VERSION = 'v2.6.1';
const REPO = 'ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol';
let assetName = '';

if (platform === 'win32') {
  assetName = 'utp-windows-x64.zip';
} else if (platform === 'linux') {
  assetName = 'utp-linux-x64.tar.gz';
} else if (platform === 'darwin') {
  assetName = 'utp-macos.tar.gz';
}

if (!assetName) {
  process.exit(0);
}

const url = `https://github.com/${REPO}/releases/download/${VERSION}/${assetName}`;
const MAX_REDIRECTS = 5;
const REQUEST_TIMEOUT_MS = 30000;

function download(downloadUrl, dest, redirectsRemaining, cb) {
  let parsed;
  try {
    parsed = new URL(downloadUrl);
  } catch (err) {
    return cb(err);
  }

  if (parsed.protocol !== 'https:') {
    return cb(new Error(`Refusing insecure non-HTTPS URL: ${parsed.protocol}`));
  }

  const req = https.get(parsed, (res) => {
    if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
      res.resume();
      if (redirectsRemaining <= 0) {
        return cb(new Error('Too many redirects'));
      }
      const nextUrl = new URL(res.headers.location, parsed).toString();
      return download(nextUrl, dest, redirectsRemaining - 1, cb);
    }

    if (res.statusCode !== 200) {
      res.resume();
      return cb(new Error(`HTTP ${res.statusCode}`));
    }

    const file = fs.createWriteStream(dest, { mode: 0o600 });
    let finished = false;
    const fail = (err) => {
      if (finished) return;
      finished = true;
      file.destroy();
      try { fs.unlinkSync(dest); } catch {}
      cb(err);
    };

    res.on('error', fail);
    file.on('error', fail);
    file.on('finish', () => {
      if (finished) return;
      finished = true;
      file.close(cb);
    });
    res.pipe(file);
  });

  req.setTimeout(REQUEST_TIMEOUT_MS, () => {
    req.destroy(new Error('Download timed out'));
  });
  req.on('error', cb);
}

function copyBinaryIfRegular(srcPath, destPath) {
  try {
    const stat = fs.lstatSync(srcPath);
    if (!stat.isFile() || stat.isSymbolicLink()) return false;
    fs.copyFileSync(srcPath, destPath);
    if (platform !== 'win32') {
      fs.chmodSync(destPath, 0o755);
    }
    return true;
  } catch {
    return false;
  }
}

const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'utp-install-'));
const archiveFile = path.join(tmpDir, assetName);
const checksumFile = path.join(tmpDir, 'SHA256SUMS.txt');
const checksumUrl = `https://github.com/${REPO}/releases/download/${VERSION}/SHA256SUMS.txt`;
const extractDir = path.join(tmpDir, 'extracted');

function cleanupTmp() {
  try {
    fs.rmSync(tmpDir, { recursive: true, force: true });
  } catch {}
}

function verifyArchiveSha256(filePath, sumsPath, expectedAssetName) {
  const sumsContent = fs.readFileSync(sumsPath, 'utf8');
  let expectedHex = null;
  for (const rawLine of sumsContent.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line) continue;
    const match = line.match(/^([a-fA-F0-9]{64})\s+\*?(.+)$/);
    if (match && path.basename(match[2].trim()) === expectedAssetName) {
      expectedHex = match[1].toLowerCase();
      break;
    }
  }
  if (!expectedHex) return false;

  const actualHex = crypto.createHash('sha256').update(fs.readFileSync(filePath)).digest('hex').toLowerCase();
  const expectedBuf = Buffer.from(expectedHex, 'hex');
  const actualBuf = Buffer.from(actualHex, 'hex');
  return expectedBuf.length === actualBuf.length && crypto.timingSafeEqual(expectedBuf, actualBuf);
}

download(url, archiveFile, MAX_REDIRECTS, (err) => {
  if (err) {
    cleanupTmp();
    // Silent fallback so offline / source installs never break npm install
    process.exit(0);
  }

  download(checksumUrl, checksumFile, MAX_REDIRECTS, (sumErr) => {
    if (sumErr) {
      cleanupTmp();
      process.exit(0);
    }

    try {
      const stat = fs.statSync(archiveFile);
      if (stat.size === 0 || !verifyArchiveSha256(archiveFile, checksumFile, assetName)) {
        cleanupTmp();
        process.exit(0);
      }

      fs.mkdirSync(extractDir, { recursive: true });

      const tarArgs = platform === 'win32'
        ? ['-xf', archiveFile, '-C', extractDir]
        : ['-xzf', archiveFile, '-C', extractDir];

      execFileSync('tar', tarArgs, { stdio: 'ignore', shell: false });

      copyBinaryIfRegular(path.join(extractDir, cliBinaryName), targetCliFile);
      const extractedDaemon = hasFile(path.join(extractDir, daemonBinaryName))
        ? path.join(extractDir, daemonBinaryName)
        : path.join(extractDir, platform === 'win32' ? 'utp-server.exe' : 'utp-server');
      copyBinaryIfRegular(extractedDaemon, targetDaemonFile);
    } catch {
      // Silent fallback to local build instructions on runtime
    } finally {
      cleanupTmp();
    }
  });
});
