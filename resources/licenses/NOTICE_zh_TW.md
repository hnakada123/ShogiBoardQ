# ShogiBoardQ / Qt

ShogiBoardQ 使用 Qt。

Qt — Copyright (C) The Qt Company Ltd. and other contributors.

ShogiBoardQ 依 GNU GPL 第 3 版散布。本軟體使用 Qt 開源版：Qt Charts 遵循
GPLv3，Qt Base（Core / Gui / Widgets / Network / Concurrent / Sql 等）和
Qt Multimedia 遵循 LGPLv3 或 GPL。各檔案的著作權聲明及授權條款均適用。

- [GNU GPL v3](qrc:/licenses/GPL-3.0.txt)
- [GNU LGPL v3](qrc:/licenses/LGPL-3.0.txt)
- [取得原始碼及重新建置](qrc:/licenses/SOURCE_CODE_zh_TW.md)
- [Qt 授權說明](https://doc.qt.io/qt-6/licensing.html)

Qt 內的第三方程式碼各自適用其授權。二進位發行套件的 licenses/qt 目錄包含授權原文、
著作權聲明和歸屬資訊；對應的原始碼壓縮檔中也包含這些原文。

Linux 版 AppImage 除 Qt 外，還隨附從 Arch Linux 套件取得的程式庫（glib、PulseAudio、OpenSSL 等），
各程式庫適用其各自的授權。清單及授權原文請參閱「隨附程式庫的授權」（licenses/THIRD-PARTY-NOTICES.md）。

P2P對局使用 libsodium（https://libsodium.org/ ，ISC License；隨附於 Windows 版、macOS 版和
Linux 版 AppImage，授權原文請參閱本文件末尾）。P2P對局的通訊程序（shogiboardq-peer）以 Rust 撰寫，
並包含 rust-libp2p 等 Rust 程式庫，各程式庫適用其各自的授權。清單及授權原文請參閱
「[P2P對局所用程式庫的授權](qrc:/licenses/RUST-NOTICES.md)」。

本軟體不附加禁止修改 Qt、替換為介面相容的程式庫，或為偵錯此類修改而進行還原工程的限制。
本軟體在適用授權規定的範圍內不提供擔保。

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
