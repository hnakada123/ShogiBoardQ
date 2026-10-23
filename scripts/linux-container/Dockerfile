# 古い Arch Linux で Linux 版を作るためのビルド環境（scripts/build-linux-container.sh が使う）。
#
# AppImage は、ビルドした環境と同じか新しい glibc のシステムでしか動かない。最新の Arch では
# glibc が新しすぎるため、Arch Linux Archive の日付固定のリポジトリで、glibc が古く Qt 6.7 以上が
# そろっている時点（2024-07-15: glibc 2.39・Qt 6.7.2）の環境を作る。同梱ライブラリのライセンス文書を
# pacman で集める仕組み（scripts/bundled_licenses.py）は、そのまま使える。
ARG BASE_IMAGE=archlinux:base-devel-20240714.0.246936
FROM ${BASE_IMAGE}

ARG SNAPSHOT=2024/07/15
ARG PACKAGES="cmake ninja git curl file python fuse2 imagemagick qt6-base qt6-charts qt6-multimedia qt6-svg qt6-tools qt6-wayland fcitx5-qt"

# 公式イメージの鍵束は初期化されていないので用意する。当時のパッケージに署名した鍵には、その後に
# 期限が切れたものがある（例: 2025-10 に期限切れ）。署名は公式の鍵束の鍵で確かめたうえで、鍵の
# 信頼度は問わない（TrustAll）。
RUN pacman-key --init && pacman-key --populate archlinux \
 && sed -i 's/^SigLevel.*/SigLevel = Required DatabaseOptional TrustAll/' /etc/pacman.conf \
 && echo "Server = https://archive.archlinux.org/repos/${SNAPSHOT}/\$repo/os/\$arch" > /etc/pacman.d/mirrorlist \
 && sed -i 's/^#\?ParallelDownloads.*/ParallelDownloads = 2/' /etc/pacman.conf

# Arch Linux Archive は遅く途中で止まることがあるため、タイムアウトで打ち切らずに取得し、失敗したら
# キャッシュに残った分から再試行する。取得とインストールを分け、取得済みの段を再利用できるようにする。
RUN for attempt in 1 2 3 4 5 6; do \
      pacman -Syyuuw --noconfirm --needed --disable-download-timeout ${PACKAGES} && break; \
      echo "download failed (attempt ${attempt}); retrying"; sleep 15; \
    done

RUN pacman -Syyuu --noconfirm --needed ${PACKAGES} \
 && pacman -Q ${PACKAGES} \
 && pacman -Scc --noconfirm

# 同梱する OpenSSL・libcurl と、libcurl が使う通信のライブラリを新しい版にする（2026-10-23 のセキュリティ監査 S17）。
# スナップショットの版（OpenSSL 3.3.1・curl 8.8.0 など）には、その後のセキュリティの修正が入っていない。
# - OpenSSL・nghttp2・nghttp3・ngtcp2・libssh2 は、今の Arch のパッケージがこの古い glibc でも動く（GLIBC_2.38 まで）
#   ので、Arch Linux Archive のパッケージを packages.sha256 の値で確かめて入れる（署名も pacman が確かめる）
# - curl は今の Arch のパッケージが新しい glibc の関数（__memset_explicit_chk、GLIBC_2.43）を使うので、同じ版の
#   Arch の PKGBUILD でこの環境で作り直す（curl のタグの署名は curl-signing-key.asc の鍵で makepkg が確かめる）
# 版を上げるときは packages.sha256・CURL_PKGVER・CURL_PKGBUILD_SHA256 を書き換え、配布物の glibc の版を確かめる
# （docs/dev/linux-build-and-release.md）。
ARG CURL_PKGVER=8.23.0-1
ARG CURL_PKGBUILD_SHA256=8a2c61727c7d55d06f4e5e896f93bca16599b6fb6ddd446ba190d36747d981e4
COPY packages.sha256 curl-signing-key.asc /tmp/updates/
RUN cd /tmp/updates \
 && while read -r sum file; do \
      url="https://archive.archlinux.org/packages/${file:0:1}/${file%-*-*-*}/${file}"; \
      curl -fsSL --retry 5 -o "${file}" "${url}" && curl -fsSL --retry 5 -o "${file}.sig" "${url}.sig"; \
    done < packages.sha256 \
 && sha256sum -c packages.sha256 \
 && pacman -U --noconfirm $(awk '{print $2}' packages.sha256)

RUN pacman -S --needed --noconfirm --disable-download-timeout git patchelf gnutls nettle \
 && useradd -m curlbuild \
 && install -d -o curlbuild /home/curlbuild/curl \
 && curl -fsSL --retry 5 -o /home/curlbuild/curl/PKGBUILD \
      "https://gitlab.archlinux.org/archlinux/packaging/packages/curl/-/raw/${CURL_PKGVER}/PKGBUILD" \
 && echo "${CURL_PKGBUILD_SHA256}  /home/curlbuild/curl/PKGBUILD" | sha256sum -c - \
 && chown -R curlbuild /home/curlbuild \
 && su curlbuild -c 'gpg --import /tmp/updates/curl-signing-key.asc && cd ~/curl && MAKEFLAGS="-j$(nproc)" makepkg --nocheck --noconfirm' \
 && pacman -U --noconfirm "/home/curlbuild/curl/curl-${CURL_PKGVER}-x86_64.pkg.tar.zst" \
 && userdel -r curlbuild \
 && rm -rf /tmp/updates \
 && pacman -Q openssl curl libnghttp2 libnghttp3 libngtcp2 libssh2 \
 && pacman -Scc --noconfirm
