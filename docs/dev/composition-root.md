# 合成ルート（MainWindowComposer）

## 概要

アプリの部品は `MainWindowComposer`（`src/app/mainwindowcomposer*.cpp`）が起動時に全部作る。
部品の一覧は `MainWindowComponents`（`src/app/mainwindowcomponents.h`）、部品どうしで共有する状態は
`AppState`（`src/common/appstate.h`）に1つだけ置く。MainWindow は `.ui` と閉じるときの確認・終了処理だけを持つ。

2026-10-15 に、それまでのレジストリ構成（`MainWindowServiceRegistry`・`MainWindowFoundationRegistry`・
`KifuSubRegistry` と約50個の `ensure*`）をこの構成に置き換えた。

## 置き換えた理由

レジストリ構成では、部品を初めて使うときに作っていた（`ensure*`）。作る順番が実行時に決まるため、
次の仕組みが積み重なっていた。

- 生成前の部品を指せるよう、依存を `std::function<X*()>` のゲッターで渡す（52個）
- 依存を後から差し替えるための `refresh*Deps()` と `updateDeps()` の再呼び出し（33回）
- MainWindow の非公開メンバを外から書き換えるための `friend`（3クラスから約1,000か所）
- 状態のフィールドを `PlayMode*` や `QString*` で40以上のヘッダーへ配る

起動の最後には全部品をまとめて作る処理（`assembleRuntimeServices`）があり、遅延生成は実際には使われていなかった。

## 起動の手順

`MainWindowComposer::compose()` が次の順に呼ぶ。

| 段階 | ファイル | 内容 |
|---|---|---|
| 生成 | `mainwindowcomposer_create.cpp` | モデル → 画面の骨格 → 盤・時計 → 棋譜欄・解析タブ → 分岐ツリー → 対局者情報・ドック → 残りのサービス |
| 依存の注入 | `mainwindowcomposer_wire.cpp`・`mainwindowcomposer_game.cpp` | 共通基盤 → 司令塔 → CSA 通信対局 → 棋譜 → UI → ダイアログ → 対局 |
| 接続と初期表示 | `mainwindowcomposer_finalize.cpp` | シグナル接続、初期局面、ウィンドウとドックの復元 |

依存は「全部作ってから結ぶ」。互いを参照する部品（検討モードの配線とダイアログの司令塔など）も、
作成順を気にせず結べる。

## 規則

1. 新しい部品は `MainWindowComponents` にメンバを足し、`createServices()` などで1回だけ作る。
2. 依存は部品のポインタで渡す。`std::function<X*()>` のゲッターや `ensure*` コールバックは使わない。
3. 状態は `AppState` に置く。部品のメンバに状態のコピーを持たない（例: 対局モードは `AppState::playMode` だけ）。
4. `std::function` は、上位へ操作を委ねる「フック」にだけ使う（例: `MatchCoordinator::Hooks`、
   `UndoFlowService::Deps::undoTwoPlies`）。部品の取得には使わない。
5. 層の境界をまたぐときだけ関数で受け取る。例: `AutomationContext` は dialogs 層の
   メニューウィンドウを触れないので、お気に入りの取得・設定を関数で受け取る。
6. テストは `MainWindow::composer().parts()` と `state()` で部品と状態を読む。`#define private public` は使わない。

`tests/tst_structural_kpi.cpp` の `appLayerHasNoLazyEnsure` と `testsDoNotRedefinePrivate` が 2 と 6 を確かめる。

## 例外

- `CsaGameWiring` は最初の CSA 通信対局で司令塔（`CsaGameCoordinator`）を作る。部品は `coordinator()` で
  そのときの司令塔を返すので、使う側は `CsaGameWiring*` を持つ。
- `AutomationServer` は `--automation` 指定時だけ `startAutomationServer()` で作る。
