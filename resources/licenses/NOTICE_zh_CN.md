# ShogiBoardQ / Qt

ShogiBoardQ 使用 Qt。

Qt — Copyright (C) The Qt Company Ltd. and other contributors.

ShogiBoardQ 根据 GNU GPL 第 3 版分发。本软件使用 Qt 开源版：Qt Charts 遵循
GPLv3，Qt Base（Core / Gui / Widgets / Network / Concurrent / Sql 等）和
Qt Multimedia 遵循 LGPLv3 或 GPL。各文件的版权声明及许可条款均适用。

- [GNU GPL v3](qrc:/licenses/GPL-3.0.txt)
- [GNU LGPL v3](qrc:/licenses/LGPL-3.0.txt)
- [获取源代码及重新构建](qrc:/licenses/SOURCE_CODE_zh_CN.md)
- [Qt 许可说明](https://doc.qt.io/qt-6/licensing.html)

Qt 内的第三方代码各自适用其许可。二进制发行包的 licenses/qt 目录包含许可原文、
版权声明和归属信息；对应的源代码压缩包中也包含这些原文。

Linux 版 AppImage 除 Qt 外还随附从 Arch Linux 软件包获取的库（glib、PulseAudio、OpenSSL 等），
各库适用其各自的许可。列表及许可原文见“随附库的许可证”（licenses/THIRD-PARTY-NOTICES.md）。

P2P对局使用 libsodium（https://libsodium.org/ ，ISC License；随附于 Windows 版、macOS 版和
Linux 版 AppImage，许可原文见本文档末尾）。P2P对局的通信进程（shogiboardq-peer）用 Rust 编写，
并包含 rust-libp2p 等 Rust 库，各库适用其各自的许可。列表及许可原文见
“[P2P对局所用库的许可证](qrc:/licenses/RUST-NOTICES.md)”。

本软件不附加禁止修改 Qt、替换为接口兼容的库，或为调试此类修改而进行逆向工程的限制。
本软件在适用许可规定的范围内不提供担保。

## libsodium

    ISC License

    Copyright (c) 2013-2026
    Frank Denis <j at pureftpd dot org>

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
