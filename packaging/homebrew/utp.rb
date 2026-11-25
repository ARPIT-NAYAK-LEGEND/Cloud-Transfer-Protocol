class Utp < Formula
  desc "Universal Cloud Transfer Protocol (UTP) - Direct 0-Disk High-Speed Cloud-to-Cloud Pipeline"
  homepage "https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol"
  version "2.6.2"
  license "MIT"

  on_macos do
    url "https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/releases/download/v2.6.2/utp-macos.tar.gz"
    sha256 "03944321ce44b51dea912e43aefcaf2acb374cd198eb5540d7b9eb5ad5d991dd"
  end

  on_linux do
    url "https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/releases/download/v2.6.2/utp-linux-x64.tar.gz"
    sha256 "5689fc2b130b73d76459149a5b775478160990e0efbaa855db49612f6b9b0ac1"
  end

  depends_on "curl"
  depends_on "openssl@3"
  depends_on "sqlite"

  def install
    bin.install "utp"
    bin.install "utpd"
  end

  test do
    system "#{bin}/utp", "--help"
  end
end
