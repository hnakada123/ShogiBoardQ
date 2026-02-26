# MainWindow 責務外コード移譲 詳細計画書

作成日: 2026-02-25  
対象: `src/app/mainwindow.cpp`, `src/app/mainwindow.h`

---

## 1. 目的

`MainWindow` を「UIイベント受信と委譲のハブ」に限定し、責務外の処理を既存クラスまたは新規クラスへ計画的に移譲する。  
本計画は、既存挙動を維持しながら段階的に分割する実行計画を定義する。

---

## 2. 現状サマリ（ベースライン）

- `MainWindow` 実装規模: `3444`行
- `MainWindow::` メソッド数: `174`
- 長大メソッド（代表）:
  - `buildMatchWiringDeps`（約80行）
  - `buildRuntimeRefs`（約79行）
  - `buildGamePanels`（約66行）
  - `createAndWireKifuLoadCoordinator`（約58行）
  - `initializeDialogLaunchWiring`（約56行）

既に分離済みの資産:
- `MainWindowGameStartService`
- `MainWindowResetService`
- `MainWindowMatchWiringDepsService`
- `MainWindowCompositionRoot`
- `DockCreationService`
- `LiveGameSessionUpdater`
- `KifuNavigationCoordinator`

結論: 分離の土台はあるが、`MainWindow` 本体に「ポリシー判定」「配線組み立て」「セッション制御」「棋譜更新」が依然集中している。

---

## 3. MainWindow の責務再定義

`MainWindow` に残す責務（最終形）:

1. Qtウィンドウライフサイクル管理（`closeEvent` など）
2. UIイベント受信（menu/action/slot の受け口）
3. 委譲先へのルーティング（1〜3行程度の薄いメソッド）
4. 画面トップレベルの所有関係維持（QObject親子）

`MainWindow` から移譲する責務:

1. プレイモード依存の判定ロジック
2. 手番・時計同期のロジック
3. 棋譜追記・ライブセッション反映ロジック
4. 盤面SFEN読み込みとハイライト同期ロジック
5. `Deps` 構築や signal/slot 配線の詳細ロジック
6. セッション開始/終了のシーケンス制御

---

## 4. 移譲対象一覧（責務外コード）

| 分類 | 現在の代表メソッド | 移譲先 | 方針 |
|---|---|---|---|
| 配線組み立て | `buildRuntimeRefs`, `buildMatchWiringDeps`, `wireMatchWiringSignals`, `initializeDialogLaunchWiring`, `createAndWireKifuLoadCoordinator` | 新規 `MainWindowWiringAssembler`, 新規 `KifuLoadCoordinatorFactory` | `MainWindow` は `assembler->ensureXxx()` 呼び出しのみ |
| プレイモード判定 | `isHumanTurnNow`, `isHumanSide`, `isHvH`, `isGameActivelyInProgress` | 新規 `PlayModePolicyService` | 判定ロジックの単一化 |
| 手番同期 | `setCurrentTurn`, `onTurnManagerChanged`, `updateTurnAndTimekeepingDisplay` | 新規 `TurnStateSyncService` | TurnManager/GC/View同期の責務を集約 |
| 棋譜更新 | `appendKifuLine`, `updateGameRecord`, `onMoveCommitted` | 新規 `GameRecordUpdateService` | 棋譜モデル更新とLiveSession更新を明確に分離 |
| 盤面同期 | `loadBoardFromSfen`, `loadBoardWithHighlights`, `syncBoardAndHighlightsAtRow`, `onBranchNodeHandled` | `KifuNavigationCoordinator`拡張 + 新規 `BoardLoadService` | SFEN適用とハイライトを統一 |
| セッション制御 | `startNewShogiGame`, `resetToInitialState`, `resetGameState`, `onMatchGameEnded`, `onApplyTimeControlRequested` | 新規 `SessionLifecycleCoordinator` | 開始/終了/連続対局の一貫制御 |
| UI構築オーケストレーション | `buildGamePanels`, `setupRecordPane`, `setupEngineAnalysisTab`, `create*Dock` 呼び出し順制御 | 新規 `MainWindowUiBootstrapper` | 依存順序のみを持つブートストラップへ分離 |

---

## 5. 新規/拡張クラス設計

## 5.1 `PlayModePolicyService`（新規）

配置案:
- `src/app/playmodepolicyservice.h`
- `src/app/playmodepolicyservice.cpp`

責務:
- 人間手番判定
- 人間側判定
- 対局進行中判定
- HvH判定

入力:
- `PlayMode`
- `ShogiGameController::Player`
- `MatchCoordinator`の終局状態
- `CsaGameCoordinator`の手番状態

出力:
- `bool` 判定群

---

## 5.2 `TurnStateSyncService`（新規）

配置案:
- `src/app/turnstatesyncservice.h`
- `src/app/turnstatesyncservice.cpp`

責務:
- `TurnManager` の確保・更新
- GC/View/時計表示の手番同期
- ライブ対局と盤面ナビ時での情報源切替

分割対象:
- `setCurrentTurn`
- `onTurnManagerChanged`
- `updateTurnAndTimekeepingDisplay` の手番同期部分

---

## 5.3 `GameRecordUpdateService`（新規）

配置案:
- `src/app/gamerecordupdateservice.h`
- `src/app/gamerecordupdateservice.cpp`

責務:
- 棋譜1手追記
- `GameRecordPresenter` 更新
- `LiveGameSessionUpdater` 連携
- `onMoveCommitted` でのUSI追記/`currentSfen`更新

分割対象:
- `appendKifuLine`
- `updateGameRecord`
- `onMoveCommitted` の後半処理

---

## 5.4 `BoardLoadService`（新規） + `KifuNavigationCoordinator`（拡張）

配置案:
- `src/app/boardloadservice.h`
- `src/app/boardloadservice.cpp`

責務:
- SFENから盤面適用
- 手番表示更新
- 盤面ハイライト適用

分割対象:
- `loadBoardFromSfen`
- `loadBoardWithHighlights`

`KifuNavigationCoordinator` 拡張:
- 「分岐ナビ中ガード」の管理をMainWindowから移動
- `onBranchNodeHandled` の完全委譲

---

## 5.5 `SessionLifecycleCoordinator`（新規）

配置案:
- `src/app/sessionlifecyclecoordinator.h`
- `src/app/sessionlifecyclecoordinator.cpp`

責務:
- 新規対局開始前準備
- リセットシーケンス
- 終局時処理
- 連続対局判定と次局起動
- 時間設定の保持/適用

分割対象:
- `startNewShogiGame`
- `resetToInitialState`
- `resetGameState`
- `onMatchGameEnded`
- `onApplyTimeControlRequested`

---

## 5.6 `MainWindowWiringAssembler`（新規）

配置案:
- `src/app/mainwindowwiringassembler.h`
- `src/app/mainwindowwiringassembler.cpp`

責務:
- `buildRuntimeRefs` 相当の参照束ね
- `buildMatchWiringDeps` 相当の組み立て
- `wireMatchWiringSignals` 相当の接続
- `initializeDialogLaunchWiring` 相当の接続構築

備考:
- 既存 `MainWindowCompositionRoot` と競合させず、「組み立て専用」に限定する。

---

## 5.7 `MainWindowUiBootstrapper`（新規）

配置案:
- `src/app/mainwindowuibootstrapper.h`
- `src/app/mainwindowuibootstrapper.cpp`

責務:
- `buildGamePanels` の依存順序をカプセル化
- UI初期化のステップを明文化

対象:
- `buildGamePanels`
- `restoreWindowAndSync`
- `finalizeCoordinators` の一部

---

## 6. 実行フェーズ計画

## フェーズ0: 計測と安全柵整備

作業:
1. `MainWindow` メソッド一覧と責務分類を固定
2. 主要フローの手動テストシナリオを文書化
3. ログカテゴリの観測ポイントを固定

完了条件:
- 以降の差分で「挙動差分あり/なし」を判定できる状態

---

## フェーズ1: 判定ロジックの分離（低リスク）

作業:
1. `PlayModePolicyService` 追加
2. `isHumanTurnNow`, `isHumanSide`, `isHvH`, `isGameActivelyInProgress` を委譲化

完了条件:
- `MainWindow` 内にプレイモード分岐 `switch` が残らない

---

## フェーズ2: 手番同期の分離（中リスク）

作業:
1. `TurnStateSyncService` 追加
2. `setCurrentTurn`, `onTurnManagerChanged` を薄いラッパーへ変更
3. 時計表示更新のうち手番同期部分を移管

完了条件:
- TurnManager関連ロジックが `MainWindow` から除去

---

## フェーズ3: 棋譜追記とライブ更新の分離（中リスク）

作業:
1. `GameRecordUpdateService` 追加
2. `appendKifuLine`, `updateGameRecord`, `onMoveCommitted` の後処理を移管
3. `LiveGameSessionUpdater` 連携をサービス側に集約

完了条件:
- `MainWindow::updateGameRecord` が1〜3行の委譲になる

---

## フェーズ4: SFEN適用/分岐同期の分離（中〜高リスク）

作業:
1. `BoardLoadService` 追加
2. `loadBoardFromSfen`, `loadBoardWithHighlights` を移管
3. `KifuNavigationCoordinator` 側へ状態ガード管理を寄せる

完了条件:
- 盤面適用とハイライト更新の本体が `MainWindow` から消える

---

## フェーズ5: セッション開始・終了の分離（高リスク）

作業:
1. `SessionLifecycleCoordinator` 追加
2. `startNewShogiGame`, `resetToInitialState`, `onMatchGameEnded`, `onApplyTimeControlRequested` を移管

完了条件:
- 開始/終了シーケンスが1クラスに集約

---

## フェーズ6: 配線組み立ての分離（高リスク）

作業:
1. `MainWindowWiringAssembler` 追加
2. `buildRuntimeRefs`, `buildMatchWiringDeps`, `wireMatchWiringSignals` を移管
3. `initializeDialogLaunchWiring`, `createAndWireKifuLoadCoordinator` の組み立てを分離

完了条件:
- `MainWindow` の `ensure*` は `CompositionRoot/Assembler` 呼び出し中心になる

---

## フェーズ7: UIブート手順の分離（中リスク）

作業:
1. `MainWindowUiBootstrapper` 追加
2. `buildGamePanels` の順序制御を移管
3. 依存順序コメントをクラス内仕様に移す

完了条件:
- `buildGamePanels` が薄い委譲になる

---

## フェーズ8: 仕上げ

作業:
1. 不要メンバ・不要include削除
2. コメント整備
3. 関連ドキュメント更新

完了条件:
- `MainWindow` の責務が「受信と委譲」に一致

---

## 7. コミット戦略（推奨）

1. `PlayModePolicyService` 導入 + 判定関数委譲
2. `TurnStateSyncService` 導入 + 手番同期委譲
3. `GameRecordUpdateService` 導入 + 棋譜更新委譲
4. `BoardLoadService` 導入 + SFEN適用委譲
5. `SessionLifecycleCoordinator` 導入 + 開始/終了委譲
6. `MainWindowWiringAssembler` 導入 + 配線組み立て委譲
7. `MainWindowUiBootstrapper` 導入 + パネル構築委譲
8. 仕上げ（未使用コード削除・ドキュメント同期）

各コミットの原則:
- 1コミット1責務
- 挙動変更を含むコミットと純リファクタコミットを分離
- ビルドが通る中間状態を維持

---

## 8. 検証計画

## 8.1 各コミットで実施

```bash
cmake --build build
```

## 8.2 フェーズ完了ごとの手動確認

1. 新規対局開始（人間/エンジン/通信）
2. 指し手反映と棋譜追記
3. 棋譜行選択での盤面同期
4. 分岐選択での盤面同期
5. 終局処理と連続対局
6. リセット後の初期状態復帰
7. 定跡ウィンドウ・評価グラフ・コメント連携

## 8.3 重点回帰観点

- `skipBoardSyncForBranchNav` の競合
- `sfenRecord()` 参照先のライフタイム
- `TurnManager` と `GameController` の同期順序
- `KifuLoadCoordinator` 再生成時の古い接続残り

---

## 9. リスクと緩和策

| リスク | 内容 | 緩和策 |
|---|---|---|
| 初期化順序崩れ | `nullptr` 参照やUI未初期化 | `UiBootstrapper` で順序を固定し、フェーズごとに検証 |
| 二重同期 | 盤面同期が多重発火 | 同期責務を `KifuNavigationCoordinator` に一本化 |
| 接続漏れ | signal/slot の欠落 | `WiringAssembler` で接続を集約し一覧化 |
| 状態不整合 | `playMode/currentSfen/currentMoveIndex` の更新漏れ | 状態更新APIをサービスへ集中 |
| ライフタイム問題 | `deleteLater` と再配線の競合 | 生成/破棄責務をFactoryに固定 |

---

## 10. 完了定義（Definition of Done）

機能面:
1. 既存の主要操作が回帰しない
2. 既存の保存形式・表示仕様を維持

構造面:
1. `MainWindow` は「受信 + 委譲」中心
2. 長大メソッド（40行超）を大幅削減
3. `PlayMode` 判定 `switch` を `MainWindow` から除去

定量目標:
1. `mainwindow.cpp` を `<= 2200` 行へ削減
2. `MainWindow::` メソッド数を `<= 120` へ削減
3. 主要ロジッククラスを最低6個追加/拡張して責務分離を完了

---

## 11. 実施優先順位（短期）

直近の実施順:
1. フェーズ1（判定ロジック分離）
2. フェーズ3（棋譜更新分離）
3. フェーズ2（手番同期分離）

理由:
- 既存挙動への影響が比較的小さく、`MainWindow` から分岐ロジックを早く除去できるため。

---

## 12. 最終計測結果（フェーズ8完了時点）

実施日: 2026-02-26

### 定量結果

| 指標 | ベースライン | 目標 | 実績 | 達成 |
|---|---|---|---|---|
| `mainwindow.cpp` 行数 | 3444 | <= 2200 | 3216 | 未達（-228行） |
| `MainWindow::` メソッド数 | 174 | <= 120 | 173 | 未達（-1） |
| 追加/拡張ロジッククラス | 0 | >= 6 | 6 | **達成** |

### 追加された新規クラス（6個）

| クラス名 | 配置 | LOC | 責務 |
|---|---|---|---|
| `PlayModePolicyService` | `src/app/` | 142 | プレイモード依存の判定ロジック集約 |
| `TurnStateSyncService` | `src/app/` | 164 | 手番同期（TurnManager/GC/View/時計） |
| `GameRecordUpdateService` | `src/app/` | 154 | 棋譜追記・ライブセッション更新 |
| `BoardLoadService` | `src/app/` | 116 | SFEN盤面適用・ハイライト同期 |
| `MainWindowUiBootstrapper` | `src/app/` | 137 | UI初期化ステップのオーケストレーション |
| `KifuNavigationCoordinator` | `src/app/` | 305 | 棋譜ナビゲーション同期 |

### フェーズ8で実施した削除内容

- 未使用 Qt ヘッダー 16件削除（QInputDialog, QHeaderView, QFileDialog 等）
- 未使用プロジェクトヘッダー 11件削除（navigationpresenter.h, pvboarddialog.h 等）
- 空メソッド 2件削除（`setupHorizontalGameLayout`, `initializeCentralGameDisplay`）
- 未使用前方宣言 3件削除（`AnalysisCoordinator`, `CsaWaitingDialog`, `JosekiWindow`）
- 未使用メンバ変数 1件削除（`m_csaWaitingDialog`）

### 所見

行数・メソッド数の定量目標は未達。主な理由:
1. `MainWindow` には依然として多数の `ensure*()` 遅延初期化メソッドが残っている（約50個）
2. 配線組み立て（`buildMatchWiringDeps` 等）とセッション制御（`startNewShogiGame` 等）の本格移譲（フェーズ5-6相当）は未実施
3. これらを `SessionLifecycleCoordinator` / `MainWindowWiringAssembler` へ移管すれば大幅削減が見込める

クラス追加目標は達成済み。判定・手番・棋譜更新・盤面読み込み・UI初期化・ナビゲーション同期の6責務を分離した。

