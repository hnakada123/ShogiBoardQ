# 合成ルート（MainWindowComposer）

## 概要

アプリの部品は `MainWindowComposer`（`src/app/mainwindowcomposer*.cpp`）が起動時に全部作る。
部品の一覧は `MainWindowComponents`（`src/app/mainwindowcomponents.h`）、部品どうしで共有する動作状態は
`AppActivityState`（`src/common/appactivitystate.h`）に1つだけ置く。MainWindow は `.ui` と閉じるときの確認・終了処理だけを持つ。

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
| 生成 | `mainwindowcomposer_create.cpp` | 棋譜の正本（分岐ツリー）・棋譜の文書（`KifuDocument`）とモデル・画面の骨格（`createRecordAndModels`）→ 盤・時計（`createBoard`）→ 棋譜欄・ドックの中身と棋譜の表示・編集（`createPanels`）→ 対局・検討・通信対局の司令塔（`createMatchAndAnalysis`）→ 表示とドック（`createPresentersAndDocks`）→ UI 状態・棋譜のファイル・対局のセッション（`createSessions`）→ 利用者の操作（`createControllers`） |
| シグナル接続 | `mainwindowcomposer_wire.cpp` | 棋譜（`connectRecord`）→ 盤（`connectBoard`）→ 対局（`connectGame`）→ 検討・思考・メニュー・通知（`connectAnalysisAndUi`。メニュー・ツールバーの項目は `MenuActionsWiring::wire()` でつなぐ） |
| 初期表示 | `mainwindowcomposer.cpp` | `finalizeUi()` が盤・棋譜欄・対局者名を表示し、ウィンドウの大きさとドックの配置を復元する |

自動化 API のサーバーは `mainwindowcomposer_automation.cpp` の `startAutomationServer()` が作る。

依存は各部品のコンストラクタに `Deps` で渡すので、部品は依存する部品より後に作る（作った後に `set*` で注入しない。例外は下の「例外」）。
部品どうしのシグナルは全部作ってからつなぐので、シグナルでつながるだけの部品どうしは作る順を気にしなくてよい。

## 規則

1. 新しい部品は `MainWindowComponents` にメンバを足し、`mainwindowcomposer_create.cpp` の該当する段階（`createSessions()` など）で1回だけ作る。
2. 依存は部品のポインタで渡す。`std::function<X*()>` のゲッターや `ensure*` コールバックは使わない。
3. 動作状態は `AppActivityState` に置く。部品のメンバに状態のコピーを持たない（例: アプリが何をしているかは `AppActivityState::activity()` だけ）。
   状態を変えるのは `AppActivityState::begin()` が返す `ActivityLease` を持つ部品だけで、終えられるのも持ち主だけ
   （別の部品が別の動作を始めると、前の持ち主に `revoked(owner)` が届き、受けた部品は自分の処理を終える）。
   棋譜に付く状態は棋譜の文書 `KifuDocument` が持つ（最後に始めた対局の対局者は `KifuDocument::players()`、
   上書き保存先は `KifuDocument::filePath()` だけ）。
4. `std::function` は、上位へ問い合わせ・操作を委ねる「フック」にだけ使う（例: `UiStatePolicyManager::Deps::canResumeGame`、
   `KifuDocument::Hooks::commitPendingEdits`）。部品の取得には使わない。フックには部品の関数をそのまま結び
   （`std::bind`）、合成ルートに判断を書かない（判断が要るなら、それを持つ部品の関数にする。例:
   `OperationPolicy::canBrowseRecord()`・`MatchCoordinator::hasHumanPlayer()`）。司令塔（`MatchCoordinator`・
   `CsaGameCoordinator`）はフックを持たず、シグナルと `MatchRecordSink`（対局の棋譜の受け手。実体は `GameSession`）で
   知らせる。棋譜を作ってから動作状態を対局中にし、終局では棋譜を確定してから待機に戻すのも司令塔で、シグナルを
   つないだ順には頼らない。
5. 層の境界をまたぐときだけ関数で受け取る。例: `AutomationContext` は widgets 層の
   メニューウィンドウ（`MenuWindow`）を触れないので、お気に入りの取得・設定を関数で受け取る。
6. テストは `MainWindow::composer().parts()` と `activity()` で部品と動作状態を読む。`#define private public` は使わない。

`tests/tst_structural_kpi.cpp` の `appLayerHasNoLazyEnsure` と `testsDoNotRedefinePrivate` が 2 と 6 を確かめる。

## 例外

- 司令塔（`MatchCoordinator`・`CsaGameCoordinator`、P2P対局を含むビルドでは `P2pGameCoordinator` も）は棋譜の受け手の
  `GameSession` より先に作るので、受け手だけは `createSessions()` で `setRecordSink()` によって渡す。
- P2P対局（`ENABLE_P2P_PLAY`）の通信プロセス・プレイヤー鍵・保存先を持つ `P2pLobbyService` と、そのウィンドウは、
  利用者が「P2P対局」を開いたときに `P2pGameUiController` が作る（使わない利用者の PC で通信プロセスを動かさない）。
  `--automation` で起動したときは `startAutomationServer()` が `P2pGameUiController::disableForAutomation()` を呼び、作らない。
- 複数の部品が足す口（`KifuNavigationController::addLeaveGuard()`×2・`KifuDocument::addPendingEditor()`×2）と、
  ドックの表 `DockRegistry` が作ったドックを受け取る部品（`MenuWindowController::attachDock()`・
  `JosekiWindowController::attachDock()`）、メニューの項目を作る部品（`DockLayoutManager::setSavedLayoutsMenu()`）は、
  相手ができあがってから渡す。
- `AutomationServer` は `--automation` 指定時だけ `startAutomationServer()` で作る。
