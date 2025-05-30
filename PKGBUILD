pkgname=vkdevicechooser
pkgver=r13.c4c08f6
pkgrel=1
pkgdesc='Extracts resources from Microsoft Windows(R) binaries '
arch=('x86_64')
license=('MIT')
url='https://github.com/Zebra2711/vkdevicechooser'
makedepends=('cmake' 'meson')
depends=('vulkan-icd-loader' 'vulkan-utility-libraries')
source=("git+$url.git")
sha256sums=('SKIP')

pkgver() {
    cd "$pkgname"
    ( set -o pipefail
        git describe --long 2>/dev/null | sed 's/\([^-]*-g\)/r\1/;s/-/./g' ||
        printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
    )
}

prepare(){
    cd ${pkgname}
    git submodule update --init
    #patch -Np1 <"../patchname.patch"
}

build() {
    cd ${pkgname}
    meson setup build --prefix=/usr
    ninja -C build
}

package() {
    cd ${pkgname}
    DESTDIR="$pkgdir" ninja -C build install
}
