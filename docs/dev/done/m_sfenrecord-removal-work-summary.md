# m_sfenRecord 全削除 実施まとめ

## 1. 目的
`MainWindow::m_sfenRecord`（レガシー局面履歴）を廃止し、SFEN履歴の参照元を `MatchCoordinator` 管理へ一本化する。

## 2. 実施方針
- `MainWindow` は SFEN 実体を持たず、`MatchCoordinator` から取得する。
- 既存の各 Controller/Coordinator/Wiring には `QStringList*` を渡すが、実体は `MatchCoordinator` 側で一元管理する。
- 旧メンバ名 `m_sfenRecord` はソース/ドキュメントから除去する。

## 3. 実施内容

### 3.1 MainWindow から `m_sfenRecord` を削除
- `MainWindow` の SFEN 実体保持を廃止。
- 代替としてアクセサを追加:
  - `QStringList* MainWindow::sfenRecord()`
  - `const QStringList* MainWindow::sfenRecord() const`
- 参照先は `MatchCoordinator::sfenRecordPtr()` に統一。

対象:
- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`

### 3.2 SFEN 実体の所有を MatchCoordinator に集約
- `MatchCoordinator` 側で共有SFEN履歴ポインタ `m_sfenHistory` を保持。
- 外部から履歴が与えられないケース向けに内部フォールバック `m_sharedSfenRecord` を保持。
- 外部公開アクセサ:
  - `QStringList* sfenRecordPtr()`
  - `const QStringList* sfenRecordPtr() const`

対象:
- `src/game/matchcoordinator.h`
- `src/game/matchcoordinator.cpp`

### 3.3 依存注入/参照呼び出しの置換
`m_sfenRecord` 直参照を `sfenRecord()` 経由に置換。

主な反映先:
- 対局初期化/司令塔配線:
  - `src/app/mainwindow.cpp` (`initMatchCoordinator`, `initializeGame`, `startNewShogiGame` など)
- UIコントローラ:
  - `src/ui/controllers/boardsetupcontroller.*`
  - `src/ui/controllers/evaluationgraphcontroller.*`
  - `src/ui/controllers/pvclickcontroller.*`
  - `src/ui/coordinators/positioneditcoordinator.*`
- プレゼンタ/ワイヤリング:
  - `src/ui/presenters/boardsyncpresenter.*`
  - `src/ui/wiring/csagamewiring.*`
  - `src/ui/wiring/josekiwindowwiring.*`
- 棋譜/解析/通信:
  - `src/kifu/kifuloadcoordinator.*`
  - `src/analysis/analysisflowcontroller.*`
  - `src/network/csagamecoordinator.*`
  - `src/navigation/recordnavigationhandler.cpp`

### 3.4 ドキュメントの名称更新
`docs/` 配下の旧名称 `m_sfenRecord` 記述を、現行設計（`sfenRecord()`/`m_sfenHistory`）ベースに更新。

対象例:
- `docs/dev/move-and-record-data-structures.md`
- `docs/dev/developer-guide.md`
- `docs/dev/game-start-flow.md`
- `docs/dev/game-play-to-resignation-flow.md`
- `docs/dev/commenting-style-guide.md`

### 3.5 削除後に発生した不具合への追従修正

#### A. `validateAndMove` 呼び出し時の履歴参照クラッシュ対策
- 人間手入力時に `BoardSetupController` へ最新の `sfenRecord()` を都度同期。

対象:
- `src/app/mainwindow.cpp`

#### B. HvE でエンジンが応答しない問題
- 人間手後、エンジン返し手直前に GC 手番がずれるケースを補正。

対象:
- `src/game/matchcoordinator.cpp`

#### C. EvH でエンジン初手後に手番が戻る問題
- ライブ対局中の手番同期を「Board SFEN」ではなく「GC」を正とするよう修正。
- ライブ棋譜へのSFEN書き出し時の turn 生成も GC 由来へ修正。

対象:
- `src/app/mainwindow.cpp`

## 4. 確認結果

### 4.1 旧シンボル残存チェック
実行コマンド:
- `rg -n "m_sfenRecord" src docs`

結果:
- ヒット 0（`m_sfenRecord` はソース/ドキュメントから除去済み）

### 4.2 ビルド確認
実行コマンド:
- `cmake --build build-debug`

結果:
- 成功

## 5. 現在の設計上の扱い
- 廃止されたのは **`MainWindow` 所有の `m_sfenRecord`**。
- 現在は `MatchCoordinator` が SFEN 履歴の提供点。
- 各所の `m_sfenHistory` は「共有履歴ポインタを受けるためのメンバ名」として残っている（実体は MainWindow では保持しない）。
