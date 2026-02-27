# Task 07: MainWindow ensure* メソッド削減の実装（第2弾: 残りの削減 + 状態整理）

## 目的
Task 06 で残った ensure* メソッドをさらに削減し、目標の 12以下を達成する。
あわせて MainWindow の状態保持を縮小する。

## 前提
- Task 06 が完了していること

## 作業内容

### Phase A: 起動時一括初期化への移行
`MainWindowLifecyclePipeline::runStartup()` で一括初期化できる ensure* を特定し移行する:
- 起動時に必ず必要なオブジェクトは遅延初期化を止め、起動パイプラインで生成する
- 例: `ensureGameRecordModel()`, `ensureRecordPresenter()`, `ensureBoardSyncPresenter()` など

### Phase B: 状態構造体の外部化
`MainWindow` 内の状態構造体を専用クラスに移動する:

1. **`PlayerState`** → `src/app/mainwindowstate.h` の `MainWindowPlayerState` に移動
2. **`KifuState`** → 既存の `GameRecordModel` / `KifuNavigationState` に統合を検討
3. **`GameState`** → `SessionLifecycleCoordinator` または新規 `GameSessionState` に移動
4. **`DataModels`** → `MainWindowRuntimeRefs` に統合を検討
5. **`DockWidgets`** → `DockCreationService` に移動を検討

### Phase C: mainwindow.h 行数の最終削減
- 状態構造体の外部化により、メンバ変数宣言を大幅に削減する
- 不要な前方宣言を削除する
- 目標: mainwindow.h 350行以下

### テスト実行
```bash
cmake --build build
cd build && ctest --output-on-failure
```

## 完了条件
- [ ] ensure* 宣言数が 12以下
- [ ] mainwindow.h が 350行以下
- [ ] MainWindow が自前で `new` を行わない
- [ ] 全テストが PASS する
- [ ] 構造KPIテストの上限値を更新する

## 注意事項
- 状態構造体の移動時は、アクセスしている全箇所を `grep` で確認する
- 状態の初期値が変わらないよう注意する（特に `startSfenStr` の初期値）
- `m_kifu.activePly` や `m_state.playMode` など、頻繁にアクセスされるメンバは getter 経由のオーバーヘッドを考慮する
