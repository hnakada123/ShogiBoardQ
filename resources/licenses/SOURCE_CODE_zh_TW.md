# 取得原始碼

ShogiBoardQ 的原始碼和建置指令碼可從以下儲存庫取得。請同時取得儲存庫記錄的 Hayanagi
子模組版本。Release 附件僅包含可執行的軟體套件。

https://github.com/hnakada123/ShogiBoardQ

    git clone --recurse-submodules https://github.com/hnakada123/ShogiBoardQ.git
    cd ShogiBoardQ
    git checkout <release-tag-or-commit>
    git submodule update --init --recursive

GitHub 自動產生的原始碼 ZIP 不包含 Hayanagi 的內容。重新建置時，請依上述方法初始化子模組。

發行套件中的 `licenses/QT-SOURCE.json` 記錄 Qt 的確切版本、原始碼檔名、SHA-256、
下載來源及建置來源說明。`licenses/BUILD.json` 記錄建置應用程式時使用的 Qt 版本。

請從 `QT-SOURCE.json` 的 `source_url` 取得 Qt 原始碼。使用作業系統套件或修改版時，
亦應查看 `provenance` 中供應方的修補程式和建置說明。在 Linux 上，這些文件位於 AppImage
內的 `usr/share/licenses/ShogiBoardQ/`；外層 ZIP 不包含 `licenses/` 目錄。

## 重新建置

取得原始碼，並準備對應的 Qt、CMake 和 C++17 編譯器。
Qt 的建置方法請參閱 Qt 原始碼中的 README 和各平台說明。

    cmake -B build -S . -DCMAKE_PREFIX_PATH=/path/to/your/Qt
    cmake --build build

打包和重新簽章的步驟請參閱應用程式原始碼中的以下文件：

- `docs/dev/linux-build-and-release.md`
- `docs/dev/macos-build-and-release.md`
- `docs/dev/windows-build-and-release.md`
- `docs/dev/qt-licensing.md`

Qt 共用程式庫在 Windows 上位於執行檔旁和各外掛目錄中，在 macOS 上位於 .app 內的
Contents/Frameworks 和 Contents/PlugIns，在 Linux 上位於解壓後的 AppDir 內的
usr/lib 和 usr/plugins。可以使用介面相容的修改版。修改後的 macOS 應用程式需要重新簽章；
本機執行時可以使用臨時簽章，無需開發者的私密金鑰。

## 本機或下游建置

本機或下游建置使用的 Qt 可能與官方發行版不同。使用作業系統套件時，
請從其發行方取得對應的原始碼、修補程式和建置說明。再散布者須提供與實際二進位檔案
相符的原始碼和聲明，並在散布二進位檔案期間保持原始碼可供下載。
