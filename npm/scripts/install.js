const fs = require('fs');
const path = require('path');
const https = require('https');
const os = require('os');
const { execSync } = require('child_process');

const platform = os.platform();
const binaryName = platform === 'win32' ? 'utp.exe' : 'utp';
const targetDir = path.join(__dirname, '..', 'bin');
const targetFile = path.join(targetDir, binaryName);

// 1. If binary already exists in bin/, nothing to do
if (fs.existsSync(targetFile)) {
  process.exit(0);
}

// 2. Check if project root already has compiled binary
const rootBinary = path.join(__dirname, '..', '..', binaryName);
if (fs.existsSync(rootBinary)) {
  try {
    fs.copyFileSync(rootBinary, targetFile);
    process.exit(0);
  } catch (e) {}
}

// 3. Fallback: Download platform prebuilt binary from GitHub Release
const VERSION = 'v2.5.0';
const REPO = 'ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol';
let assetName = '';

if (platform === 'win32') {
  assetName = 'utp-windows-x64.zip';
} else if (platform === 'linux') {
  assetName = 'utp-linux-x64.tar.gz';
}

if (!assetName) {
  process.exit(0);
}

const url = `https://github.com/${REPO}/releases/download/${VERSION}/${assetName}`;

function download(downloadUrl, dest, cb) {
  const file = fs.createWriteStream(dest);
  https.get(downloadUrl, (res) => {
    if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
      return download(res.headers.location, dest, cb);
    }
    if (res.statusCode !== 200) {
      try { fs.unlinkSync(dest); } catch (e) {}
      return cb(new Error(`HTTP ${res.statusCode}`));
    }
    res.pipe(file);
    file.on('finish', () => file.close(cb));
  }).on('error', (err) => {
    try { fs.unlinkSync(dest); } catch (e) {}
    cb(err);
  });
}

const archiveFile = path.join(targetDir, assetName);

download(url, archiveFile, (err) => {
  if (err) {
    // Silent fallback to local build instructions
    process.exit(0);
  }

  try {
    if (platform === 'win32') {
      execSync(`tar -xf "${archiveFile}" -C "${targetDir}"`, { stdio: 'ignore' });
    } else {
      execSync(`tar -xzf "${archiveFile}" -C "${targetDir}"`, { stdio: 'ignore' });
      fs.chmodSync(targetFile, 0o755);
    }
    try { fs.unlinkSync(archiveFile); } catch (e) {}
  } catch (e) {}
});
