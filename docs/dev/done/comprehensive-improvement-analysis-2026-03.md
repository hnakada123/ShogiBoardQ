# ShogiBoardQ 総合改善分析レポート（2026-03）

## 1. 概要

本ドキュメントは、ShogiBoardQ コードベース全体を対象に、コード品質・アーキテクチャ・ビルドシステム・CI/CD・プラットフォーム対応の5軸で改善すべき箇所を分析したレポートである。

既存の `future-improvement-analysis-2026-03-01-reassessment.md`（KPI検知・MainWindow依存ハブ等）を補完し、**そのドキュメントでは扱われていない領域**を中心にまとめる。

### 1.1 プロジェクト規模（実測）

| 指標 | 値 |
|---|---:|
| `src/` 配下 `.cpp/.h` ファイル数 | 589 |
| `src/` 配下総行数 | ~105,700 |
| テスト登録数 | 56（全pass） |
| 500行超ファイル数 | 31 |
| 600行超ファイル数 | 0 |

---

## 2. コード品質

### 2.1 [高] 座標系定数の不整合（潜在バグ）

**駒台ファイル座標が2系統存在し、値が異なる。**

| 場所 | 先手駒台 | 後手駒台 |
|---|---|---|
| `src/board/boardinteractioncontroller.h` | `kBlackStandFile = 10` | `kWhiteStandFile = 11` |
| `src/core/fmvconverter.cpp` | `kBlackHandFile = 9` | `kWhiteHandFile = 10` |

FMV（高速合法手生成）エンジン層と盤面操作層で座標体系が異なっている。現時点では各層内で閉じているため顕在化していないが、**層間で座標値を直接受け渡す改修が入ると即座にバグ化する。**

さらに、`piecemoverules.cpp`、`shogiboard.cpp`、`shogiboard_edit.cpp`、`csamoveprogresshandler.cpp`、`csamoveconverter.cpp`、`shogigamecontroller.cpp` の合計10箇所以上で `10` / `11` がマジックナンバーとしてハードコードされている。

**改善案:**
1. 共有定数ヘッダ（`src/common/boardconstants.h`）を作成し、全モジュールで統一的に使用する
2. FMV層の座標変換ロジックをドキュメント化し、なぜ値が異なるかを明記する

---

### 2.2 [高] `<iostream>` がコアヘッダに含まれている

`src/core/shogimove.h` が `<iostream>` をインクルードしている。このヘッダはプロジェクト全体から広く参照されるため、**すべての翻訳単位でC++iostream機構のパースコストが発生する。**

```cpp
// src/core/shogimove.h:8-10
#include <iostream>  // ← 不要
#include <QVector>
#include <QDebug>
```

`std::ostream& operator<<` オーバーロードが宣言されているが、**実際の使用箇所はゼロ**（QDebug のみ使用）。`<iostream>` と `std::ostream` 演算子の両方を削除可能。

**改善案:** `<iostream>` include と `std::ostream` operator を削除する。ビルド時間短縮に直結する。

---

### 2.3 [中] `QVector` → `QList` 未移行（100箇所以上）

CLAUDE.md の clazy ガイドラインで「Qt6では `QVector` は `QList` に正規化されるため `QList` を使う」と明記されているが、`QVector` が100箇所以上のヘッダ・ソースで使用されている。

代表例:
- `src/game/matchcoordinator.h:489`: `QVector<ShogiMove> m_gameMoves;`
- `src/kifu/kifubranchtree.h:223`: `mutable QVector<BranchLine> m_linesCache;`
- `src/app/mainwindowstate.h:104`: `QVector<ShogiMove> gameMoves;`
- `src/kifu/livegamesession.h:206-207`: `QVector<KifDisplayItem>`, `QVector<ShogiMove>`
- `src/engine/shogiengineinfoparser.h`: 複数の `QVector<QChar>` パラメータ

**改善案:** 一括置換で `QVector` → `QList`、`#include <QVector>` → `#include <QList>` に移行する。

---

### 2.4 [中] `std::as_const()` 欠落（clazy range-loop-detach）

メンバコンテナに対する range-based for ループで `std::as_const()` が欠落している箇所:

| ファイル | 行 | コンテナ |
|---|---|---|
| `src/engine/usiprotocolhandler.cpp` | 83 | `m_setOptionCommands` (QStringList) |
| `src/models/kifubranchlistmodel.cpp` | 284 | `m_nodes` (QVector\<Node\>) |

**改善案:** `std::as_const()` でラップする。

---

### 2.5 [低] インクルードガードの不統一

291個の `.h` ファイルのうち:
- **280ファイル**: `#ifndef` ガードのみ
- **7ファイル**: `#pragma once` のみ
- **4ファイル**: `#pragma once` と `#ifndef` の**二重ガード**
  - `evaluationchartwidget.h`、`apptooltipfilter.h`、`globaltooltip.h`、`evaluationchartconfigurator.h`

**改善案:** プロジェクト全体でどちらかに統一する（既存多数派は `#ifndef`）。

---

### 2.6 [低] `TsumeResult` 構造体の重複定義

`Usi` と `UsiProtocolHandler` の両方に同一の `TsumeResult` 構造体（`Kind { Solved, NoMate, NotImplemented, Unknown }` + `QStringList pvMoves`）が定義されている。

同様に `SENTE_HAND_FILE`、`GOTE_HAND_FILE`、`BOARD_SIZE`、`NUM_BOARD_SQUARES` の定数も両ファイルで重複している。

**改善案:** 共通定義を1箇所にまとめ、もう一方から参照する。

---

### 2.7 [低] マジックナンバー（UI/タイマー）

| ファイル | 値 | 用途 |
|---|---|---|
| `kifuloadcoordinator.cpp:483` | `QColor(255, 220, 160)` | 分岐ハイライト色 |
| `evaluationchartwidget.cpp:54` | `QColor(255, 255, 255, 80)` | グリッド線色 |
| `evaluationchartwidget.cpp:62` | `QColor(0, 100, 0)` | チャート背景色 |
| `consecutivegamescontroller.cpp:114` | `100` (ms) | 次局開始待ち |
| `engineanalysispresenter.cpp:168` | `500` (ms) | 遅延表示 |
| `csaclient.cpp:49,52` | `60000`, `1000` | 時間単位変換 |

**改善案:** 名前付き定数に置き換える。

---

## 3. アーキテクチャ

### 3.1 [高] レイヤ違反の検知漏れ（`game/` → `dialogs/`）

`tst_layer_dependencies` は `#include` パス文字列に `"dialogs/"` を含むかで違反を検出するが、**フラットインクルード（ディレクトリプレフィックスなし）を検出できない。**

実際の違反箇所:
- `src/game/gamestartorchestrator.cpp:6`: `#include "startgamedialog.h"`
- `src/game/matchcoordinator.h:30`: `class StartGameDialog;`（前方宣言）
- `GameStartOrchestrator` / `MatchCoordinator` が `const StartGameDialog*` を引数に取る

`StartGameDialog` は `src/dialogs/` 配下の `QDialog` サブクラスである。CMake のフラットインクルードパス設定により、パス文字列ベースの検査をすり抜けている。

**改善案:**
1. `StartGameDialog` の代わりに抽象インターフェースまたはデータ構造体（`GameStartOptions`）を `game/` 層で定義し、`dialogs/` 層への依存を切る
2. `tst_layer_dependencies` の検出ロジックを、パス文字列マッチングからファイル所在ディレクトリベースの検査に改善する

---

### 3.2 [中] `DialogCoordinator` の DI スタイル混在

`src/ui/coordinators/dialogcoordinator.h`（387行）は、他のコーディネータが `Deps` 構造体 + `updateDeps()` パターンを使用しているのに対し、**個別 setter メソッド**（`setMatchCoordinator()`、`setGameController()`、`setUsiEngine()` 等10個以上）を使用している。

さらに3つのコンテキスト構造体（`ConsiderationContext`、`TsumeSearchContext`、`KifuAnalysisContext`）と3つのパラメータ構造体をヘッダ内に定義しており、半ゴッドクラス化している。

**改善案:** `Deps` 構造体パターンに統一する。

---

### 3.3 [中] `Deps` vs `Dependencies` 命名の不統一

| 命名 | 使用数 | 例 |
|---|---|---|
| `struct Deps` | 多数 | `BoardLoadService`、`TurnStateSyncService` 等 |
| `struct Dependencies` | 7 | `CsaGameWiring`、`PlayerInfoWiring`、`MenuWindowWiring`、`JosekiWindowWiring`、`PreStartCleanupHandler`、`KifuExportController`、`CsaGameCoordinator` |

**改善案:** `Deps` に統一する。

---

### 3.4 [中] `signals:` vs `Q_SIGNALS:` の不統一

| 記法 | 使用数 |
|---|---:|
| `signals:` / `public slots:` | 72 / 43 |
| `Q_SIGNALS:` / `public Q_SLOTS:` | 5 / 6 |

`Q_SIGNALS:` / `Q_SLOTS:` を使用しているファイル:
- `dialogcoordinator.h`、`positioneditcoordinator.h`、`playerinfocontroller.h`、`evaluationgraphcontroller.h`、`gamestatecontroller.h`、`analysisflowcontroller.h`

**改善案:** 多数派の `signals:` / `public slots:` に統一する。

---

### 3.5 [低] `matchcoordinator.h` の巨大ヘッダ問題

546行のヘッダファイルで、57以上の public API エントリ、6つのネスト構造体（`Hooks`、`Deps`、`StartOptions`、`TimeControl`、`AnalysisOptions`、`GameOverState`）を含む。

このヘッダをインクルードする全ての翻訳単位がパースコストを負担する。特に `Hooks` の4つのサブ構造体（`UI`、`Time`、`Engine`、`Game`）は独立ヘッダに分離可能。

---

## 4. ビルドシステム

### 4.1 [高] macOS バンドル識別子がプレースホルダ

```cmake
MACOSX_BUNDLE_GUI_IDENTIFIER com.example.ShogiBoardQ
```

`com.example` は配布に使用できない。Mac App Store 提出やコード署名・公証には正式な逆DNS識別子が必要。

**改善案:** 正式な識別子に置き換える。

---

### 4.2 [高] バージョン番号の二重管理

```cmake
project(ShogiBoardQ VERSION 0.1 ...)       # → MACOSX_BUNDLE_*_VERSION = 0.1
set(APP_VERSION "2026.02.25")               # → 実際のアプリバージョン
```

macOS バンドルは `PROJECT_VERSION`（0.1）を使用するため、実際のCalVerバージョンと異なる値が表示される。

**改善案:** `project(VERSION ...)` を CalVer に統一するか、`MACOSX_BUNDLE_*_VERSION` を `APP_VERSION` から明示設定する。

---

### 4.3 [中] `-Werror` 未設定

コンパイラ警告（`-Wall -Wextra -Wpedantic -Wshadow -Wconversion` 等）は有効だが、`-Werror` がないため警告がビルドを失敗させない。

**改善案:** `ENABLE_WERROR` オプションを追加し、CI では有効化する。

---

### 4.4 [中] GCC固有警告がコメントアウト

CMakeLists.txt 94-131行にある以下の警告が無効化されている:
- `-Wduplicated-cond`（重複条件）
- `-Wduplicated-branches`（重複分岐）
- `-Wlogical-op`（論理演算の誤り）
- `-Wuseless-cast`（無意味なキャスト）
- `-Wold-style-cast`（C言語スタイルのキャスト）

**改善案:** `CMAKE_CXX_COMPILER_ID STREQUAL "GNU"` で条件分岐し、GCCビルドで有効化する。

---

### 4.5 [低] `threadtypes.h` が CMakeLists.txt に未記載

`src/common/threadtypes.h` がディスク上に存在するが `SRC_COMMON` に含まれていない。ヘッダオンリーファイルのためビルドには影響しないが、IDE のソースツリーに表示されない。

---

### 4.6 [低] `cmake_minimum_required` のポリシー上限未指定

```cmake
cmake_minimum_required(VERSION 3.16)
```

`cmake_minimum_required(VERSION 3.16...3.28)` のように上限を指定すると、新しい CMake バージョンでモダンなポリシーが自動適用される。

---

## 5. CI/CD

### 5.1 [高] Windows / macOS の CI ジョブが存在しない

全 CI ジョブが `ubuntu-latest` のみで実行されている。`scripts/build-macos.sh` や `scripts/build-windows.ps1` が存在するが CI に統合されていない。

プラットフォーム固有コード（`WIN32_EXECUTABLE`、macOS バンドル、`app.rc`）が CI で一切検証されていない。

**改善案:**
1. `macos-latest` でのビルドジョブを追加（Qt6インストール + ビルド + テスト）
2. `windows-latest` でのビルドジョブを追加（MSVC + Qt6）
3. 最低限ビルド成功のみ検証し、テストは段階的に追加

---

### 5.2 [中] clazy の CI 不在

CLAUDE.md で「clazy level0,level1 の警告が出ないようにコードを書くこと」と明記されているが、**CI に clazy を実行するジョブが存在しない。** ポリシー違反の蓄積を検出できない。

**改善案:** `clazy-standalone` を使用した CI ジョブを追加する。

---

### 5.3 [中] `static-analysis` ジョブの不備

- `BUILD_TESTING=ON` が未設定 → テストコードが clang-tidy で解析されない
- `CMAKE_BUILD_TYPE` が未設定 → デフォルトの空ビルドタイプで実行される（Release/Debug とは異なる結果になる可能性）

**改善案:** ビルドタイプを明示し、テストコードも解析対象に含める。

---

### 5.4 [中] カバレッジの外部サービス連携なし

`coverage` ジョブは HTML/XML を生成して GitHub Artifact にアップロードしているが、Codecov や Coveralls 等の外部サービスに送信されていない。PR レビュー時にカバレッジバッジが表示されず、ダウンロードしないと確認できない。

---

### 5.5 [低] GitHub Actions の SHA ピン留め未実施

`actions/checkout@v4`、`jurplel/install-qt-action@v4` 等がタグ参照のみ。サプライチェーン攻撃に対する耐性を高めるには SHA ハッシュでのピン留めが望ましい。

---

### 5.6 [低] Qt バージョン制約の明示なし

CI では `6.7.*` を使用しているが、`find_package(Qt6 ...)` にバージョン下限がない。ローカル開発者が異なる Qt6 バージョン（6.5、6.6、6.8）を使用した場合の互換性が保証されない。

**改善案:** `find_package(Qt6 6.7 REQUIRED ...)` のように最低バージョンを指定する。

---

## 6. `.clang-tidy` 設定の強化余地

### 6.1 [中] 未有効化の有用チェック

| チェックグループ | 代表チェック | 検出対象 |
|---|---|---|
| `cppcoreguidelines-*` | `slicing`, `prefer-member-initializer` | スライシング、初期化順 |
| `clang-analyzer-cplusplus.*` | use-after-free, double-free | メモリ安全性 |
| `clang-analyzer-security.*` | バッファオーバーフロー等 | セキュリティ |
| `readability-magic-numbers` | マジックナンバー | 可読性 |
| `readability-container-size-empty` | `.size() == 0` → `.empty()` | イディオム |
| `misc-const-correctness` | const 付与漏れ | 正確性 |

### 6.2 [低] `WarningsAsErrors` が空

`.clang-tidy` の `WarningsAsErrors: ''` が空のため、ローカル実行でも警告止まりとなる。`WarningsAsErrors: '*'` に設定すれば CI 外でも検知力が上がる。

---

## 7. スレッド安全性・堅牢性

### 7.1 [中] `QThread::msleep(1)` のビジーウェイト

`src/engine/usiprotocolhandler_wait.cpp:93` で `QThread::msleep(1)` によるポーリングループが存在する。メインスレッドで実行された場合はイベントループをブロックし、UI フリーズの原因になる。

**改善案:** `QEventLoop` + シグナル待機、または `QWaitCondition` による非ビジーウェイトに変更する。

---

### 7.2 [低] `QTimer::singleShot` + `this` キャプチャの安全性

`src/game/consecutivegamescontroller.cpp:114`:
```cpp
QTimer::singleShot(100, this, [this]() {
    if (m_timeController) { ... }
});
```

`this` が第2引数に渡されているため Qt がオブジェクト破壊時にタイマーをキャンセルする（Qt5.4+の仕様）。安全ではあるが、プロジェクトの「connect にラムダ禁止」方針の拡張適用を検討する余地がある。6箇所に同パターンが存在する。

---

## 8. リソース管理

### 8.1 [中] 翻訳ファイルのロードパス

`main.cpp` で `:/i18n/ShogiBoardQ_ja_JP`（QRC パス）を最初に試行するが、`.qm` ファイルは QRC に埋め込まれていないため**常に失敗する**。その後 `applicationDirPath()` からのロードにフォールバックする。

**改善案:**
- `.qm` を QRC に埋め込む、または
- 最初の QRC パス試行を削除してフォールバックのみにする

### 8.2 [情報] 翻訳の未完了率

| ファイル | 総メッセージ | 未翻訳 | 未翻訳率 |
|---|---:|---:|---:|
| `ShogiBoardQ_en.ts` | 1,450 | 273 | 18.8% |
| `ShogiBoardQ_ja_JP.ts` | 1,081 | 952 | 88.1% |

ソース文字列が日本語であるため `_ja_JP.ts` の未翻訳率が高いのは自然だが、英語翻訳の18.8%未完了は改善の余地がある。

---

## 9. 改善優先度サマリ

### P0（即時対応推奨）

| # | 項目 | セクション |
|---|---|---|
| 1 | 座標系定数の不整合（潜在バグ） | 2.1 |
| 2 | macOS バンドル識別子がプレースホルダ | 4.1 |

### P1（短期対応推奨）

| # | 項目 | セクション |
|---|---|---|
| 3 | `<iostream>` のコアヘッダ汚染 | 2.2 |
| 4 | レイヤ違反の検知漏れ（`game/` → `dialogs/`） | 3.1 |
| 5 | Windows / macOS の CI 不在 | 5.1 |
| 6 | バージョン番号の二重管理 | 4.2 |
| 7 | `-Werror` 未設定 | 4.3 |

### P2（中期改善）

| # | 項目 | セクション |
|---|---|---|
| 8 | `QVector` → `QList` 移行 | 2.3 |
| 9 | clazy CI ジョブの追加 | 5.2 |
| 10 | `DialogCoordinator` DI 統一 | 3.2 |
| 11 | `Deps` / `Dependencies` 命名統一 | 3.3 |
| 12 | `signals:` / `Q_SIGNALS:` 統一 | 3.4 |
| 13 | `static-analysis` ジョブの補強 | 5.3 |
| 14 | カバレッジ外部サービス連携 | 5.4 |
| 15 | 翻訳ファイルロードパス修正 | 8.1 |
| 16 | `std::as_const()` 欠落修正 | 2.4 |
| 17 | `QThread::msleep` ビジーウェイト | 7.1 |

### P3（長期・低優先）

| # | 項目 | セクション |
|---|---|---|
| 18 | インクルードガード統一 | 2.5 |
| 19 | `TsumeResult` 重複定義解消 | 2.6 |
| 20 | マジックナンバー定数化 | 2.7 |
| 21 | GCC 固有警告の有効化 | 4.4 |
| 22 | `threadtypes.h` CMakeLists 追記 | 4.5 |
| 23 | `cmake_minimum_required` 上限指定 | 4.6 |
| 24 | `matchcoordinator.h` 分割 | 3.5 |
| 25 | `.clang-tidy` チェック拡充 | 6.1 |
| 26 | GitHub Actions SHA ピン留め | 5.5 |
| 27 | Qt バージョン下限指定 | 5.6 |

---

## 10. 総評

ShogiBoardQ は、テスト（56件全pass）、構造KPI（自動監視）、レイヤ依存チェック、リファクタリング文化が高い水準で実装されたコードベースである。600行超ファイルがゼロという目標を達成しており、設計品質は優れている。

主な改善機会は以下の3カテゴリに集約される:

1. **潜在的正確性リスク**: 座標系定数の不整合（2.1）とレイヤ違反検知漏れ（3.1）は、将来の機能追加時にバグや設計劣化を招く可能性がある
2. **ビルド・CI 成熟度**: クロスプラットフォーム CI の不在（5.1）とバージョン管理の不統一（4.2）は、配布品質に影響する
3. **コードスタイル統一**: `QVector`/`QList`、`Deps`/`Dependencies`、`signals:`/`Q_SIGNALS:` 等の不統一は、読み手のコグニティブロードを上げている

いずれも「大規模な設計欠陥」ではなく、**成熟したコードベースの次段階最適化**に該当する改善項目である。
