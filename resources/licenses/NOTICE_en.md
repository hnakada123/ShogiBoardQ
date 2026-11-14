# ShogiBoardQ / Qt

ShogiBoardQ uses Qt.

Qt — Copyright (C) The Qt Company Ltd. and other contributors.

ShogiBoardQ is distributed under GNU GPL version 3. It uses the open-source
edition of Qt: Qt Charts under GPLv3, and Qt Base (Core / Gui / Widgets / Network /
Concurrent / Sql, etc.) and Qt Multimedia under LGPLv3 or GPL terms.
Individual files retain their own notices and licenses.

- [GNU GPL v3](qrc:/licenses/GPL-3.0.txt)
- [GNU LGPL v3](qrc:/licenses/LGPL-3.0.txt)
- [Source code and rebuilding](qrc:/licenses/SOURCE_CODE_en.md)
- [Qt licensing](https://doc.qt.io/qt-6/licensing.html)

Third-party code inside Qt has separate licenses. Original license texts,
copyright notices and attribution metadata are included in licenses/qt in
binary distributions, and in the corresponding source archives.

Besides Qt, the Linux AppImage bundles libraries from Arch Linux packages (such as
glib, PulseAudio and OpenSSL), each under its own license. They are listed with their
license texts in "Bundled library licenses" (licenses/THIRD-PARTY-NOTICES.md).

P2P games use libsodium (https://libsodium.org/, ISC License; bundled in the
Windows and macOS packages and in the Linux AppImage; its license text is at the
end of this document). The communication process of P2P games (shogiboardq-peer)
is written in Rust and is built from Rust libraries such as rust-libp2p, each under
its own license. They are listed with their license texts in
"[Licenses of the libraries for P2P games](qrc:/licenses/RUST-NOTICES.md)".

No additional restriction is imposed on modifying Qt, replacing it with an
interface-compatible library, or reverse engineering to debug such changes.
The software is provided without warranty as specified in its licenses.

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
