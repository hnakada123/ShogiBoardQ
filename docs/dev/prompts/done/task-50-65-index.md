# MainWindow責務移譲タスク一覧（task-50〜65）

マスタープラン: `docs/dev/mainwindow-delegation-master-plan.md`

## 実装順序と依存関係

| # | ファイル | カテゴリ | 概要 | 依存 |
|---|---|---|---|---|
| 50 | `task-50-c13-ensure-separation.md` | C13 | ensure* 群の完全分離（DI基盤） | なし（最初に実施） |
| 51 | `task-51-c04-dock-panel-construction.md` | C04 | ドック/パネル構築 | 50 |
| 52 | `task-52-c03-action-signal-wiring.md` | C03 | アクション/シグナル配線 | 50 |
| 53 | `task-53-c08-kifu-navigation.md` | C08 | 棋譜ナビゲーション/分岐同期 | 50 |
| 54 | `task-54-c07-board-operations.md` | C07 | 盤面操作/着手入力 | 50 |
| 55 | `task-55-c05-game-lifecycle.md` | C05 | 対局開始/終了ライフサイクル | 50 |
| 56 | `task-56-c06-session-reset.md` | C06 | セッションリセット/終了保存 | 50 |
| 57 | `task-57-c01-ui-appearance.md` | C01 | UI外観/ウィンドウ表示制御 | 50 |
| 58 | `task-58-c09-player-info.md` | C09 | プレイヤー情報/表示同期 | 50 |
| 59 | `task-59-c10-kifu-io-notification.md` | C10 | 棋譜/画像/通知I/O | 50 |
| 60 | `task-60-c11-policy-accessors.md` | C11 | ポリシー判定/時間取得アクセサ | 50 |
| 61 | `task-61-c12-match-adapter.md` | C12 | MatchCoordinator配線adapter | 50, 60 |
| 62 | `task-62-c14-auxiliary-features.md` | C14 | CSA/考慮モード/補助機能 | 50 |
| 63 | `task-63-c02-core-init.md` | C02 | 起動時コア初期化 | 50〜62の大部分 |
| 64 | `task-64-c15-settings-restore.md` | C15 | 設定復元フロー一本化 | 50〜63の大部分 |
| 65 | `task-65-c16-facade-finalization.md` | C16 | Facade最終化 | 50〜64全て |

## 推奨グループ

1. **基盤** (task-50): ensure分離 → 他全タスクの前提
2. **UI構築/配線** (task-51, 52): 並行実施可能
3. **棋譜/盤面** (task-53, 54): 並行実施可能
4. **ライフサイクル** (task-55, 56): 55→56の順序推奨
5. **表示/I/O/判定** (task-57, 58, 59, 60): 並行実施可能
6. **adapter/補助** (task-61, 62): 61は60に依存
7. **最終整理** (task-63, 64, 65): 63→64→65の順序で実施

## 現在の MainWindow 状態（作成時点）

- `mainwindow.cpp`: 2,754行
- `mainwindow.h`: 839行
- `ensure*` メソッド: 47個
- `#include`: 113個
