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

依存は各部品のコンストラクタに `Deps` で渡すので、部品は依存する部品より後に作る（作った後に `set*` で注入しない）。
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
   `PositionEditSession::allowsLeavingMove()`・`MatchCoordinator::hasHumanPlayer()`）。司令塔（`MatchCoordinator`）は
   フックを持たず、シグナルと `MatchRecordSink`（対局の手順の受け手。実体は `GameSession`）で知らせる。
5. 層の境界をまたぐときだけ関数で受け取る。例: `AutomationContext` は dialogs 層の
   メニューウィンドウを触れないので、お気に入りの取得・設定を関数で受け取る。
6. テストは `MainWindow::composer().parts()` と `state()` で部品と状態を読む。`#define private public` は使わない。

`tests/tst_structural_kpi.cpp` の `appLayerHasNoLazyEnsure` と `testsDoNotRedefinePrivate` が 2 と 6 を確かめる。

## 例外

- `MatchCoordinator` は手順の受け手の `GameSession` より先に作るので、受け手だけは `createSessions()` で
  `setRecordSink()` によって渡す。
- `GameSession` は `CsaGameSession` より先に作る。どちらもコンストラクタで `CsaGameCoordinator` の
  `gameStarted`・`gameEnded` をつなぎ、先につないだ `GameSession` が棋譜（作り直し・対局者・終局・自動保存）を
  済ませてから `CsaGameSession` が画面に反映する（接続の順に頼る）。
- `AutomationServer` は `--automation` 指定時だけ `startAutomationServer()` で作る。
