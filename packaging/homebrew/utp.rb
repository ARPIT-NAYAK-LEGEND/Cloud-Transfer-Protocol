class Utp < Formula
  desc "Universal Cloud Transfer Protocol (UTP) - Direct 0-Disk High-Speed Cloud-to-Cloud Pipeline"
  homepage "https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol"
  version "2.6.2"
  license "MIT"
  url "https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol.git",
      branch: "PROJECTS"

  depends_on "cmake" => :build
  depends_on "curl"
  depends_on "openssl@3"
  depends_on "sqlite"

  def install
    system "cmake", "-S", ".", "-B", "build", "-DBUILD_TESTS=OFF", *std_cmake_args
    system "cmake", "--build", "build", "--parallel"
    bin.install "build/cli/utp"
    bin.install "build/server/utpd"
  end

  test do
    system "#{bin}/utp", "--help"
  end
end
