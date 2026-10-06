# 获取源代码

ShogiBoardQ 的源代码和构建脚本可从以下仓库获取。请同时获取仓库记录的 Hayanagi
子模块版本。Release 附件仅包含可运行的软件包。

https://github.com/hnakada123/ShogiBoardQ

    git clone --recurse-submodules https://github.com/hnakada123/ShogiBoardQ.git
    cd ShogiBoardQ
    git checkout <release-tag-or-commit>
    git submodule update --init --recursive

GitHub 自动生成的源代码 ZIP 不包含 Hayanagi 的内容。重新构建时，请按上述方法初始化子模块。

发行包中的 `licenses/QT-SOURCE.json` 记录 Qt 的确切版本、源代码文件名、SHA-256、
下载来源及构建来源说明。`licenses/BUILD.json` 记录构建应用程序时使用的 Qt 版本。

请从 `QT-SOURCE.json` 的 `source_url` 获取 Qt 源代码。使用操作系统软件包或修改版时，
还应查看 `provenance` 中供应方的补丁和构建说明。在 Linux 上，这些文档位于 AppImage
内的 `usr/share/licenses/ShogiBoardQ/`；外层 ZIP 不包含 `licenses/` 目录。

Linux 版 AppImage 随附的 Qt 以外的库，其版本、许可及对应源代码的获取位置（Arch Linux
软件包的源代码和上游）记载于 `licenses/THIRD-PARTY-NOTICES.md`（“随附库的许可证”）。

## 重新构建

获取源代码，并准备对应的 Qt、CMake 和 C++17 编译器。
Qt 的构建方法请参阅 Qt 源代码中的 README 和各平台说明。

    cmake -B build -S . -DCMAKE_PREFIX_PATH=/path/to/your/Qt
    cmake --build build

打包和重新签名的步骤请参阅应用程序源代码中的以下文档：

- `docs/dev/linux-build-and-release.md`
- `docs/dev/macos-build-and-release.md`
- `docs/dev/windows-build-and-release.md`
- `docs/dev/qt-licensing.md`

Qt 共享库在 Windows 上位于可执行文件旁和各插件目录中，在 macOS 上位于 .app 内的
Contents/Frameworks 和 Contents/PlugIns，在 Linux 上位于解包后的 AppDir 内的
usr/lib 和 usr/plugins。可以使用接口兼容的修改版。修改后的 macOS 应用需要重新签名；
本地运行时可以使用临时签名，无需开发者的私钥。

## 本地或下游构建

本地或下游构建使用的 Qt 可能与官方发行版不同。使用操作系统软件包时，
请从其发行方获取对应的源代码、补丁和构建说明。再分发者须提供与实际二进制文件
相匹配的源代码和声明，并在分发二进制文件期间保持源代码可供下载。
