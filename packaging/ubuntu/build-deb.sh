#!/bin/bash
#
# ShogiBoardQ を Ubuntu のパッケージ（.deb）にする。リリースタグのソースを、Ubuntu の Qt 6 でビルドする。
#
# Ubuntu 26.04 の素のコンテナの中で root として実行する（手順は README.md）:
#   mkdir -p out
#   docker run --rm -v "$PWD:/packaging:ro" -v "$PWD/out:/out" ubuntu:26.04 bash /packaging/build-deb.sh
#
# 26.04 でビルドしたパッケージは 26.04.1 LTS と 26.10 の両方に入る。新しい Qt でビルドしたものは古い Qt では
# 動かないので、必ず古い方の 26.04 でビルドする。
#
# 配置:
#   /usr/lib/shogiboardq/          ShogiBoardQ・shogiboardq-cli の本体と翻訳（.qm）、P2P対局の通信プロセス
#                                  shogiboardq-peer。アプリは実行ファイルと同じフォルダの .qm と通信プロセスを
#                                  使うため、ここにまとめる
#   /usr/bin/ShogiBoardQ           本体へのリンク（shogiboardq-cli も同じ）
#   /usr/lib/shogiboardq/Hayanagi/ 通常対局用の USI エンジン Hayanagi と定跡（book/）
#   /usr/share/shogiboardq/tsumeshogi/  詰将棋問題集（3〜13手詰、各1,000題）
#   /usr/share/licenses/shogiboardq/    「バージョン情報」で表示するライセンス文書
set -euo pipefail

pkgname=shogiboardq
pkgver=2026.11.09
pkgrel=1
maintainer='hnakada123 <hnakada123@gmail.com>'

. /etc/os-release
if [ "$VERSION_ID" != 26.04 ]; then
  echo "Ubuntu 26.04 の中で実行してください（今は $VERSION_ID）" >&2
  exit 1
fi

srcdir=/src/ShogiBoardQ
builddir=/build
pkgdir=/pkg

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends \
  build-essential ca-certificates cmake dpkg-dev git ninja-build libgl-dev \
  qt6-base-dev qt6-charts-dev qt6-multimedia-dev qt6-svg-dev \
  qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools linguist-qt6 qt6-translations-l10n \
  cargo rustc libsodium-dev pkg-config qtkeychain-qt6-dev
# cargo・rustc・libsodium-dev・qtkeychain-qt6-dev は P2P対局に使う（通信プロセス shogiboardq-peer は Rust。依存の
# クレートは cargo がビルド中に crates.io から取得する。版とハッシュは Cargo.lock で固定）

git clone --depth 1 --branch "$pkgver" --recurse-submodules --shallow-submodules \
  https://github.com/hnakada123/ShogiBoardQ.git "$srcdir"

cmake -B "$builddir" -S "$srcdir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_BINDIR=lib/$pkgname \
  -DBUILD_TESTING=OFF \
  -Wno-dev
cmake --build "$builddir"

DESTDIR="$pkgdir" cmake --install "$builddir"
# P2P対局の通信プロセスが入っていないパッケージを作らない
test -x "$pkgdir/usr/lib/$pkgname/shogiboardq-peer"

libdir="$pkgdir/usr/lib/$pkgname"
install -d "$pkgdir/usr/bin"
ln -s "../lib/$pkgname/ShogiBoardQ" "$pkgdir/usr/bin/ShogiBoardQ"
ln -s "../lib/$pkgname/shogiboardq-cli" "$pkgdir/usr/bin/shogiboardq-cli"

# ライセンス文書はパッケージ名のフォルダに置き、「バージョン情報」が探す licenses/ からリンクする。
# /usr/share/doc は最小構成の Ubuntu では入らないことがあるので、ここには置かない
mv "$pkgdir/usr/share/licenses/ShogiBoardQ" "$pkgdir/usr/share/licenses/$pkgname"
ln -s "../../share/licenses/$pkgname" "$libdir/licenses"

# Hayanagi は実行ファイルの横の book/hayanagi_book.db を定跡として読む
install -Dm755 "$builddir/Hayanagi/hayanagi" "$libdir/Hayanagi/hayanagi"
install -Dm644 "$builddir/Hayanagi/book/hayanagi_book.db" "$libdir/Hayanagi/book/hayanagi_book.db"
install -Dm644 "$srcdir/Hayanagi/README.md" "$libdir/Hayanagi/README.md"

install -Dm644 -t "$pkgdir/usr/share/$pkgname/tsumeshogi" \
  "$srcdir"/data/tsumeshogi/tsume_*ply_1000_20261001.txt \
  "$srcdir/data/tsumeshogi/README.md"

strip --strip-unneeded "$libdir/ShogiBoardQ" "$libdir/shogiboardq-cli" "$libdir/shogiboardq-peer" \
  "$libdir/Hayanagi/hayanagi"

# Qt の非公開 API を使うと、Qt の版が変わると動かなくなり、26.10 に入れられない
if objdump -T "$libdir/ShogiBoardQ" "$libdir/shogiboardq-cli" | grep -q 'PRIVATE_API'; then
  echo 'Qt の非公開 API のシンボルを使っています' >&2
  exit 1
fi

docdir="$pkgdir/usr/share/doc/$pkgname"
install -d "$docdir"
cat > "$docdir/copyright" <<EOF
ShogiBoardQ

Source: https://github.com/hnakada123/ShogiBoardQ/tree/$pkgver
Copyright (C) hnakada123 and contributors

ShogiBoardQ is distributed under the GNU General Public License version 3.
Individual files retain their own notices and licenses.

On Debian and Ubuntu systems, the complete text of the GNU General Public
License version 3 is in /usr/share/common-licenses/GPL-3.

The license documents shown in the About dialog are in
/usr/share/licenses/$pkgname/.
EOF

released=$(date -R -d "@$(git -C "$srcdir" log -1 --format=%ct)")
gzip -9n > "$docdir/changelog.Debian.gz" <<EOF
$pkgname ($pkgver-$pkgrel) stable; urgency=medium

  * Package of the release tag $pkgver.

 -- $maintainer  $released
EOF

# dpkg-shlibdeps はプラグインを見つけられないので、起動時に読むプラグインは Depends に手で足す
#   qt6-qpa-plugins:   画面（xcb）と入力メソッド
#   qt6-svg-plugins:   駒の SVG
#   libqt6sql6-sqlite: 詰将棋の進捗
mkdir -p /deb/debian "$pkgdir/DEBIAN"
printf 'Source: %s\n\nPackage: %s\nArchitecture: amd64\n' "$pkgname" "$pkgname" > /deb/debian/control
shlibdeps=$(cd /deb && dpkg-shlibdeps -O \
  "$libdir/ShogiBoardQ" "$libdir/shogiboardq-cli" "$libdir/shogiboardq-peer" "$libdir/Hayanagi/hayanagi" \
  | sed -e 's/^shlibs:Depends=//')

cat > "$pkgdir/DEBIAN/control" <<EOF
Package: $pkgname
Version: $pkgver-$pkgrel
Architecture: amd64
Maintainer: $maintainer
Installed-Size: $(du -sk "$pkgdir/usr" | cut -f1)
Depends: $shlibdeps, qt6-qpa-plugins, qt6-svg-plugins, libqt6sql6-sqlite, hicolor-icon-theme
Recommends: qt6-wayland, fonts-noto-cjk
Section: games
Priority: optional
Homepage: https://hnakada123.github.io/ShogiBoardQ/
Description: Shogi board GUI for playing, kifu management and analysis
 ShogiBoardQ plays shogi (Japanese chess) against USI engines and over the
 CSA protocol, reads and writes game records, and analyzes positions and
 whole games with USI engines.
 .
 The package also contains the command line tool shogiboardq-cli, the USI
 engine Hayanagi with its opening book, and tsume-shogi problem collections.
 Games can also be played peer to peer with people you invite.
EOF

dpkg-deb --root-owner-group -Zxz --build "$pkgdir" "/out/${pkgname}_${pkgver}-${pkgrel}_amd64.deb"
chown --reference=/out "/out/${pkgname}_${pkgver}-${pkgrel}_amd64.deb"

echo "ShogiBoardQ: $(git -C "$srcdir" rev-parse HEAD)"
echo "Hayanagi:    $(git -C "$srcdir/Hayanagi" rev-parse HEAD)"
echo "Qt:          $(dpkg-query -W -f='${Version}' qt6-base-dev)"
echo "g++:         $(g++ -dumpfullversion)"
echo "rustc:       $(rustc --version)"
echo "glibc:       $(dpkg-query -W -f='${Version}' libc6)"
cat "$pkgdir/DEBIAN/control"
