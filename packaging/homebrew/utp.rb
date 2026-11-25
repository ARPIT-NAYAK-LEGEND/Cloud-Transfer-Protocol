class Utp < Formula
  desc "Universal Cloud Transfer Protocol (UTP) - Direct 0-Disk High-Speed Cloud-to-Cloud Pipeline"
  homepage "https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol"
  version "2.6.2"
  license "MIT"

  on_macos do
    url "https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/releases/download/v2.6.2/utp-macos.tar.gz"
    sha256 "1aaff4c79d41085970c72e944406f891585acf06dc8e93bfeb2a130eb7a96df4"
  end

  on_linux do
    url "https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/releases/download/v2.6.2/utp-linux-x64.tar.gz"
    sha256 "fb8d769b008e46704533ab851a836e01505c54a2451843a4eb9256d9aa407952"
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
