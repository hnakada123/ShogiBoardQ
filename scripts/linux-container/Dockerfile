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
