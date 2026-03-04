_pkgname="hyprlock"
pkgname="$_pkgname-pine"
pkgver=pine
pkgrel=1
pkgdesc="Hyprland's GPU-accelerated screen locking utility"
arch=('x86_64' 'aarch64')
url="https://github.com/Mr-Pine/hyprlock"
license=('BSD-3-Clause')

depends=(
  'sdbus-cpp>=2.0.0'
  hyprgraphics
  hyprlang
  hyprutils
  libdrm
  libglvnd
  libxkbcommon
  mesa
  pam
  pango
  wayland
)
makedepends=(
  cmake
  git
  hyprwayland-scanner-git
  ninja
  wayland-protocols
)

provides=("$_pkgname=${pkgver%%.r*}")
conflicts=("$_pkgname")

_pkgsrc=$_pkgname
source=("$_pkgsrc::git+$url.git")
sha256sums=('SKIP')

backup=('etc/pam.d/hyprlock')

pkgver() {
  cd "$_pkgsrc"
  # git describe --long --tags --abbrev=7 | sed 's/^v//;s/\([^-]*-g\)/r\1/;s/-/./g'
  echo -n "pine."
  git rev-parse HEAD
}

build() {
  local cmake_options=(
    -B build
    -S "$_pkgsrc"
    -G Ninja
    -W no-dev
    -D CMAKE_BUILD_TYPE=None
    -D CMAKE_INSTALL_PREFIX=/usr
  )
  cmake "${cmake_options[@]}"
  cmake --build build
}

package() {
  DESTDIR="$pkgdir" cmake --install build
  install -Dm644 "$_pkgsrc/LICENSE" -t "$pkgdir/usr/share/licenses/$pkgname/"
}
