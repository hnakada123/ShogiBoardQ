# ShogiBoardQ コード品質改善計画（2026-02-26）

## 1. この文書の目的

この文書は、2026-02-26 時点で確認できた懸念点を**全件列挙**し、
優先度・実施順・完了条件を明示した改善計画に落とし込むことを目的とする。

## 2. 調査スコープと前提

- 対象: `src/`, `tests/`, `CMakeLists.txt`, `AGENTS.md`, `resources/translations/*.ts`
- 実測コマンド: `wc -l`, `rg`, `ctest`, `cmake --build`
- ビルド確認:
  - `cmake --build build`（既存ビルド）
  - `cmake -B build-review -S . -DBUILD_TESTING=ON`
  - `cmake --build build-review -j4`
- テスト確認:
  - `ctest --test-dir build` は headless 環境で大量 abort
  - `QT_QPA_PLATFORM=offscreen ctest --test-dir build` は全 31 件 pass
  - `QT_QPA_PLATFORM=offscreen ctest --test-dir build-review` も全 31 件 pass

## 3. 懸念点の全列挙（優先度順）

### P0-1. テスト実行条件がローカルで再現しづらい

- 事象:
  - `ctest --test-dir build` では 31 件中 26 件が `Subprocess aborted`
  - `QT_QPA_PLATFORM=offscreen` を付与すると 31/31 pass
- 根拠:
  - `tests/CMakeLists.txt` の `QT_QPA_PLATFORM=offscreen` 設定は 3 テストのみ（`tst_ui_display_consistency`, `tst_analysisflow`, `tst_game_start_flow`）
  - [tests/CMakeLists.txt:382](/home/nakada/GitHub/ShogiBoardQ/tests/CMakeLists.txt:382)
  - [tests/CMakeLists.txt:397](/home/nakada/GitHub/ShogiBoardQ/tests/CMakeLists.txt:397)
  - [tests/CMakeLists.txt:412](/home/nakada/GitHub/ShogiBoardQ/tests/CMakeLists.txt:412)
- 影響:
  - 開発者のローカル実行で誤って「テスト壊れている」と判断しやすい

### P0-2. `MainWindow` 周辺の結合度が高い（設計負債）

- 事象:
  - `MainWindow` に `friend class` が 13 件
  - `nullptr` 初期化の生ポインタメンバが 71 件
  - `ensure*` 系が 38 件
- 根拠:
  - [src/app/mainwindow.h:144](/home/nakada/GitHub/ShogiBoardQ/src/app/mainwindow.h:144)
  - [src/app/mainwindow.h:310](/home/nakada/GitHub/ShogiBoardQ/src/app/mainwindow.h:310)
  - [src/app/mainwindow.h:455](/home/nakada/GitHub/ShogiBoardQ/src/app/mainwindow.h:455)
- 影響:
  - 変更時に影響範囲が読み取りづらく、回帰リスクが高まりやすい

### P0-3. `MainWindowServiceRegistry` への責務集中

- 事象:
  - `mainwindowserviceregistry.cpp` は 1353 行
  - `MainWindowServiceRegistry::` 定義が 95 件
  - `std::bind` の 42 件が同ファイルに集中
- 根拠:
  - [src/app/mainwindowserviceregistry.cpp](/home/nakada/GitHub/ShogiBoardQ/src/app/mainwindowserviceregistry.cpp)
- 影響:
  - 配線変更・初期化順変更時のレビュー難度が高い

### P0-4. 巨大ファイルが多く、保守単位が粗い

- 事象:
  - 1000 行以上ファイルが 17 件
  - 例:
    - `src/views/shogiview.cpp` 2836 行
    - `src/dialogs/josekiwindow.cpp` 2406 行
    - `src/services/settingsservice.cpp` 1608 行
    - `src/game/matchcoordinator.cpp` 1444 行
- 根拠:
  - [src/views/shogiview.cpp](/home/nakada/GitHub/ShogiBoardQ/src/views/shogiview.cpp)
  - [src/dialogs/josekiwindow.cpp](/home/nakada/GitHub/ShogiBoardQ/src/dialogs/josekiwindow.cpp)
  - [src/services/settingsservice.cpp](/home/nakada/GitHub/ShogiBoardQ/src/services/settingsservice.cpp)
  - [src/game/matchcoordinator.cpp](/home/nakada/GitHub/ShogiBoardQ/src/game/matchcoordinator.cpp)
- 影響:
  - 局所変更でも副作用確認コストが高い

### P1-1. `SettingsService` API が過密で型境界が弱い

- 事象:
  - `settingsservice.h` が 564 行
  - `SettingsService` namespace 内の宣言が 216 件
  - `settingsservice.cpp` が 1608 行
- 根拠:
  - [src/services/settingsservice.h](/home/nakada/GitHub/ShogiBoardQ/src/services/settingsservice.h)
  - [src/services/settingsservice.cpp](/home/nakada/GitHub/ShogiBoardQ/src/services/settingsservice.cpp)
- 影響:
  - 設定追加時に API が単調増加し、責務境界が曖昧

### P1-2. 生メモリ管理が局所的に残る

- 事象:
  - `new` 使用 624 箇所
  - `delete` 明示 29 箇所
- 根拠:
  - 例: [src/app/mainwindowlifecyclepipeline.cpp:97](/home/nakada/GitHub/ShogiBoardQ/src/app/mainwindowlifecyclepipeline.cpp:97)
  - 例: [src/game/shogigamecontroller.cpp:67](/home/nakada/GitHub/ShogiBoardQ/src/game/shogigamecontroller.cpp:67)
  - 例: [src/engine/usi.cpp:97](/home/nakada/GitHub/ShogiBoardQ/src/engine/usi.cpp:97)
- 影響:
  - 所有権を誤読しやすく、リファクタ時の破壊リスクが上がる

### P1-3. `GameStartCoordinator` の参照が二重化し意図が読みづらい

- 事象:
  - `m_gameStart` と `m_gameStartCoordinator` が同型で併存
- 根拠:
  - [src/app/mainwindow.h:378](/home/nakada/GitHub/ShogiBoardQ/src/app/mainwindow.h:378)
  - [src/app/mainwindow.h:380](/home/nakada/GitHub/ShogiBoardQ/src/app/mainwindow.h:380)
  - [src/app/mainwindowserviceregistry.cpp:642](/home/nakada/GitHub/ShogiBoardQ/src/app/mainwindowserviceregistry.cpp:642)
  - [src/app/mainwindowserviceregistry.cpp:1278](/home/nakada/GitHub/ShogiBoardQ/src/app/mainwindowserviceregistry.cpp:1278)
- 影響:
  - 参照先の意味差分がコードから直感しにくい

### P1-4. コメント負債（完了済み TODO が残存）

- 事象:
  - `@todo remove コメントスタイルガイド適用済み` が 66 件
- 根拠:
  - 例: [src/engine/usi.h:41](/home/nakada/GitHub/ShogiBoardQ/src/engine/usi.h:41)
  - 例: [src/core/shogiboard.h:18](/home/nakada/GitHub/ShogiBoardQ/src/core/shogiboard.h:18)
- 影響:
  - TODO がノイズ化し、真に未対応の TODO を見分けにくい

### P1-5. ドキュメント整合性の崩れ

- 事象:
  - `AGENTS.md` は「自動テストなし」と記述
  - 実際は `tests/` に 31 テスト、`tests/CMakeLists.txt` で `add_test` 登録
- 根拠:
  - [AGENTS.md:25](/home/nakada/GitHub/ShogiBoardQ/AGENTS.md:25)
  - [tests/CMakeLists.txt:60](/home/nakada/GitHub/ShogiBoardQ/tests/CMakeLists.txt:60)
- 影響:
  - 新規開発者の理解が古い情報に引っ張られる

### P2-1. 翻訳未完了が多い（運用課題）

- 事象:
  - `ShogiBoardQ_en.ts`: unfinished 214
  - `ShogiBoardQ_ja_JP.ts`: unfinished 987
- 根拠:
  - [resources/translations/ShogiBoardQ_en.ts](/home/nakada/GitHub/ShogiBoardQ/resources/translations/ShogiBoardQ_en.ts)
  - [resources/translations/ShogiBoardQ_ja_JP.ts](/home/nakada/GitHub/ShogiBoardQ/resources/translations/ShogiBoardQ_ja_JP.ts)
- 影響:
  - UI 表示品質のばらつき、QA コスト増

### P2-2. 翻訳生成が常時ビルド対象

- 事象:
  - `translations` ターゲットが `ALL`
  - 本体ターゲットが常に `translations` に依存
- 根拠:
  - [CMakeLists.txt:743](/home/nakada/GitHub/ShogiBoardQ/CMakeLists.txt:743)
  - [CMakeLists.txt:744](/home/nakada/GitHub/ShogiBoardQ/CMakeLists.txt:744)
- 影響:
  - 通常開発ビルドの実行時間に翻訳更新コストが常時加算される

## 4. 改善ロードマップ（詳細）

## フェーズ0（即時: 1〜2日）

### 目的

開発体験を壊している P0 を先に除去し、以降のリファクタを安全に進める。

### 実施項目

1. CTest の headless 実行を全テストに一括適用
- `tests/CMakeLists.txt` で全 `add_test` に対して `QT_QPA_PLATFORM=offscreen` を設定
- 既存の個別 `set_tests_properties` は重複を整理

2. 実行手順の明文化
- `AGENTS.md` の Testing セクションを現状に更新
- `ctest --test-dir build` と `QT_QPA_PLATFORM` の関係を明記

3. 完了条件
- `ctest --test-dir build --output-on-failure` が headless 環境で全 pass
- CI とローカルで同じ手順を再現可能

## フェーズ1（短期: 1〜2週間）

### 目的

巨大配線レイヤの分割準備（責務境界の定義）を先に固める。

### 実施項目

1. `MainWindowServiceRegistry` の分割方針を固定
- 分割先（例）
  - `MainWindowUiRegistry`
  - `MainWindowGameRegistry`
  - `MainWindowKifuRegistry`
  - `MainWindowAnalysisRegistry`
- まず `ensure*` をカテゴリ単位で move し、挙動不変を維持

2. `GameStartCoordinator` 二重参照の意味統一
- `m_gameStart` / `m_gameStartCoordinator` の命名と責務を統一
- 同一概念なら一本化、別概念なら型 alias か wrapper で区別

3. 完了条件
- `MainWindowServiceRegistry::` 実装数を 95 → 60 以下
- `GameStartCoordinator` 参照の意図が名前で判別可能

## フェーズ2（中期: 2〜4週間）

### 目的

`MainWindow` の高結合を段階的に緩和する。

### 実施項目

1. `friend` 依存の縮小
- 直接 private 参照が必要な領域を `RuntimeRefs` / `Deps` 経由へ寄せる
- `friend class` の削減目標を設定

2. 所有権の明文化
- `MainWindow` メンバを「所有」「非所有」「弱参照」に分類し、型と命名で表現
- Qt 親子所有に乗せられるものは parent を徹底

3. 完了条件
- `friend class` 13 → 7 以下
- `MainWindow` の生ポインタ（`= nullptr`）71 → 50 以下

## フェーズ3（中期: 2〜3週間）

### 目的

`SettingsService` を「巨大な関数群」から「型付き設定境界」へ移行する。

### 実施項目

1. 設定ドメイン分割
- `WindowSettings`, `FontSettings`, `JosekiSettings`, `AnalysisSettings` 等の構造体化

2. API の集約
- getter/setter の単機能関数を段階的に repository API へ移行
- 既存キー互換は維持（移行期間は adapter 併存）

3. 完了条件
- `SettingsService` 公開宣言数 216 → 120 以下
- `settingsservice.cpp` 1608 行 → 900 行以下

## フェーズ4（中長期: 4〜8週間）

### 目的

巨大ファイルの分割でレビュー単位を小さくし、回帰局所化を進める。

### 対象優先順位

1. `shogiview.cpp`（2836行）
- 描画・入力・レイアウト責務を `shogiview_*.cpp` へ分離

2. `josekiwindow.cpp`（2406行）
- View ロジックとデータ操作を coordinator/presenter へ抽出

3. `matchcoordinator.cpp`（1444行）
- 対局状態遷移と UI 通知の分割

4. `kifu/formats/*tosfenconverter.cpp` 群
- 共通パース処理を `parsecommon` へ再集約し重複削減

### 完了条件

- 1000 行超ファイル数 17 → 8 以下
- 上記 4 領域で既存テスト + 回帰テストを green 維持

## フェーズ5（継続運用）

### 目的

技術的負債の再蓄積を防ぐ。

### 実施項目

1. コメント負債の解消
- `@todo remove コメントスタイルガイド適用済み` を一掃

2. 翻訳負債の運用ルール化
- unfinished 件数の上限運用（増加を CI で検知）

3. ビルド時間最適化
- `translations` を `ALL` から分離するか、開発時オプション化

4. 完了条件
- 上記 TODO 66 件を 0 件
- 翻訳 unfinished の net increase を 0 維持

## 5. 実施順の理由

- 先にフェーズ0で開発体験の誤判定要因（テスト abort）を消す
- 次にフェーズ1〜3で「構造負債（結合/責務/API）」を減らす
- 最後にフェーズ4〜5で大規模分割と運用ガードレールを入れる

## 6. KPI（追跡指標）

- テスト再現性:
  - `ctest --test-dir build` の pass 率 100%
- 構造健全性:
  - `MainWindowServiceRegistry::` 実装数: 95 → 60 以下
  - `friend class` 数: 13 → 7 以下
  - `MainWindow` 生ポインタ数: 71 → 50 以下
- 規模管理:
  - 1000行超ファイル数: 17 → 8 以下
- 設定API健全性:
  - `SettingsService` 宣言数: 216 → 120 以下
- 負債管理:
  - stale `@todo` 件数: 66 → 0

## 7. 補足（今回のビルド/テスト結果）

- `build-review` でフルビルド成功
- `QT_QPA_PLATFORM=offscreen` 指定時、31/31 テスト pass
- 現時点では「明確なコンパイル警告」は再現しなかった

