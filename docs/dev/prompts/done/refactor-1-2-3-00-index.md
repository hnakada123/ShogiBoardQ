# MainWindow リファクタリング 1-2-3 プロンプト一覧

設計書: `docs/dev/mainwindow-refactor-1-2-3-plan.md`

各プロンプトは `/clear` 後にコピペして実行できる。
前のステップが完了・ビルド成功してから次に進むこと。

## フェーズ1: 依存注入・配線組み立ての分離

| # | ファイル | 内容 |
|---|---------|------|
| 01 | `refactor-1-2-3-01-runtime-refs-and-factory.md` | `MainWindowRuntimeRefs` + `MainWindowDepsFactory` 追加（未接続） |
| 02 | `refactor-1-2-3-02-factory-dialog-coordinator.md` | `ensureDialogCoordinator` の factory 化 |
| 03 | `refactor-1-2-3-03-factory-kifu-file-controller.md` | `ensureKifuFileController` の factory 化 |
| 04 | `refactor-1-2-3-04-factory-record-nav-handler.md` | `ensureRecordNavigationHandler` の factory 化 |
| 05 | `refactor-1-2-3-05-composition-root.md` | `MainWindowCompositionRoot` 導入 |

## フェーズ2: ライブ分岐セッション更新の分離

| # | ファイル | 内容 |
|---|---------|------|
| 06 | `refactor-1-2-3-06-live-game-session-updater.md` | `LiveGameSessionUpdater` 導入と `updateGameRecord` 委譲 |

## フェーズ3: 棋譜ナビゲーション同期の分離

| # | ファイル | 内容 |
|---|---------|------|
| 07 | `refactor-1-2-3-07-kifu-navigation-coordinator.md` | `KifuNavigationCoordinator` 導入 |

## 各ステップ完了後の検証コマンド

```bash
cmake -B build -S . && cmake --build build
```
