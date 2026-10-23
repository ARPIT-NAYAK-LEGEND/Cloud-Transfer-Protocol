class Utp < Formula
  desc "Universal Cloud Transfer Protocol (UTP) - Direct 0-Disk High-Speed Cloud-to-Cloud Pipeline"
  homepage "https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol"
  version "2.5.0"
  license "MIT"

  if OS.mac?
    if Hardware::CPU.arm?
      url "https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/releases/download/v2.5.0/utp-macos-arm64.tar.gz"
      sha256 "SKIP"
    else
      url "https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/releases/download/v2.5.0/utp-macos-x64.tar.gz"
      sha256 "SKIP"
    end
  elsif OS.linux?
    url "https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/releases/download/v2.5.0/utp-linux-x64.tar.gz"
    sha256 "SKIP"
  end

  depends_on "curl"
  depends_on "openssl@3"
  depends_on "sqlite"

  def install
    bin.install "utp"
  end

  test do
    system "#{bin}/utp", "--help"
  end
end
