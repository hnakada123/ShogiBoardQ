# Obtaining source code

Obtain ShogiBoardQ sources and build scripts from the repository below, including
the recorded Hayanagi submodule revision. Release attachments contain only runnable packages.

https://github.com/hnakada123/ShogiBoardQ

    git clone --recurse-submodules https://github.com/hnakada123/ShogiBoardQ.git
    cd ShogiBoardQ
    git checkout <release-tag-or-commit>
    git submodule update --init --recursive

GitHub's automatically generated source ZIP omits Hayanagi's contents; initialize
the submodule as shown above when rebuilding.

`licenses/QT-SOURCE.json` identifies the exact Qt version, source archive,
SHA-256, origin and source provenance. `licenses/BUILD.json` records the Qt
version used to build the application.

For Qt sources, use the `source_url` recorded in `QT-SOURCE.json`. For downstream
builds, also consult `provenance` for the supplier's patches and build instructions.
On Linux these documents reside inside the AppImage at `usr/share/licenses/ShogiBoardQ/`;
the outer ZIP has no `licenses/` directory.

## Rebuilding

Obtain the sources and install/build the corresponding Qt, CMake and a C++17
compiler. Qt's own README and platform instructions explain how to build Qt.

    cmake -B build -S . -DCMAKE_PREFIX_PATH=/path/to/your/Qt
    cmake --build build

See these documents in the application sources for packaging and re-signing:

- `docs/dev/linux-build-and-release.md`
- `docs/dev/macos-build-and-release.md`
- `docs/dev/windows-build-and-release.md`
- `docs/dev/qt-licensing.md`

Shared Qt libraries reside beside the executable and in plugin directories on
Windows, in Contents/Frameworks and Contents/PlugIns on macOS, and in usr/lib
and usr/plugins of the extracted AppDir on Linux. Interface-compatible modified
versions can be used. A modified macOS app must be re-signed; a local ad-hoc
signature can be used without the developer's private signing key.

## Local or downstream builds

Local/downstream builds may use a different Qt. For distribution packages,
obtain the matching sources, patches and build instructions from that
distribution. Redistributors must provide sources and notices matching their
actual binaries and keep the source downloads available while distributing them.
