# Task 13: 残存大型ファイル分割（dialogs/navigation/widgets/ui/network）

## 概要

600行超の残存ファイル群（5ファイル）を分割し、600行超を9件以下に削減する。

## 前提条件

- Task 06〜12（core/engine/kifu/analysis分割）完了推奨（分割パターンが確立されている）

## 現状

Task 06〜12 で対処しない600行超ファイル（5件）:

| 行数 | ファイル | 分割方針 |
|---:|---|---|
| 720 | `src/ui/coordinators/kifudisplaycoordinator.cpp` | ライブ対局セッション処理の分離 |
| 693 | `src/network/csaclient.cpp` | プロトコル解析の分離 |
| 689 | `src/engine/shogiengineinfoparser.cpp` | 盤面シミュレーション/変換の分離 |
| 657 | `src/dialogs/tsumeshogigeneratordialog.cpp` | 漢字PV構築/UI構築の分離 |
| 642 | `src/navigation/kifunavigationcontroller.cpp` | 分岐ツリー処理の分離 |

追加対象（600行超だがTask 06〜12でカバー済みのため残存する場合のみ）:
| 638 | `src/dialogs/startgamedialog.cpp` | 設定I/Oの分離 |
| 635 | `src/widgets/evaluationchartwidget.cpp` | ツールチップ/データ操作の分離 |

## 実施内容

### Step 1: kifudisplaycoordinator.cpp の分割

1. ファイルを読み込み、責務グループを確認
2. **ライブ対局セッション**グループ（6メソッド、~124行）を `src/ui/coordinators/kifudisplaycoordinator_live.cpp` に分離
   - `onLiveGameMoveAdded()`, `onLiveGameSessionStarted()`(59行), `onLiveGameBranchMarksUpdated()`, `onLiveGameCommitted()`, `onLiveGameDiscarded()`, `onLiveGameRecordModelUpdateRequired()`
3. `onPositionChanged()`(119行) を `kifudisplaycoordinator_nav.cpp` に分離（必要に応じて）
4. 目標: 600行以下

### Step 2: csaclient.cpp の分割

1. ファイルを読み込み、責務グループを確認
2. **受信行解析**グループを `src/network/csaclient_parser.cpp` に分離:
   - `processLine()`, `processLoginResponse()`, `processGameSummary()`(146行), `processGameMessage()`, `processResultLine()`(67行), `processMoveLine()`, `parseConsumedTime()`
3. 本体には接続管理・コマンド送信・ソケットコールバックを残す
4. 目標: 600行以下

### Step 3: shogiengineinfoparser.cpp の分割

1. ファイルを読み込み、責務グループを確認
2. **盤面操作/座標変換**グループを `src/engine/shogiengineinfoparser_board.cpp` に分離:
   - `getPieceCharacter()`, `setData()`, `movePieceToSquare()`, `parseMoveString()`(87行), 座標変換ヘルパー群
3. **info行解析**の `parsePvAndSimulateMoves()` 等が盤面操作に依存するため、分離境界を慎重に設計
4. 目標: 600行以下

### Step 4: tsumeshogigeneratordialog.cpp の分割

1. ファイルを読み込み、責務グループを確認
2. **静的ヘルパー関数**（`buildKanjiPv()`(84行), `pieceCharToKanji()`）を `src/dialogs/tsumeshogikanjibuilder.h/.cpp` に分離
3. `setupUi()`(153行) は本体に残す（ダイアログ構築はダイアログクラスの責務）
4. 目標: 600行以下

### Step 5: kifunavigationcontroller.cpp の分割

1. ファイルを読み込み、責務グループを確認
2. **分岐ツリーノード処理** `handleBranchNodeActivated()`(112行) を `src/navigation/kifunavigationcontroller_branch.cpp` に分離（実装ファイル分割）
3. 目標: 600行以下

### Step 6: 追加ファイル（必要に応じて）

Task 06〜12 の結果によっては以下も対象:

- **startgamedialog.cpp**（638行）: `saveGameSettings()`/`loadGameSettings()`/`resetSettingsToDefault()` を `StartGameSettingsIO` に分離
- **evaluationchartwidget.cpp**（635行）: `onSeriesHovered()`(89行) のツールチップロジックを分離

### Step 7: KPI更新

1. `tst_structural_kpi.cpp` の `knownLargeFiles()` を全面更新
2. CMakeLists.txt に新規ファイルを追加
3. 600行超ファイル数の目標達成を確認

## 完了条件

- 600行超ファイル: 15 → 9以下
- 800行超ファイル: 0（Task 06, 07 と合わせて）
- 各新規ファイルが 600行以下
- ビルド成功
- 全テスト通過

## 注意事項

- 各ファイルは QObject 派生のため、基本的に **実装ファイル分割**（同一クラスの .cpp を複数に分ける）を採用
- `tsumeshogigeneratordialog.cpp` の `buildKanjiPv()` は非メンバ静的関数のため、別クラスへの分離が容易
- `csaclient.cpp` の `GameSummary` 構造体はヘッダーに定義済みのため、パーサー分離時にアクセスは容易
- 5ファイル分を1タスクにまとめているが、実際の作業は1ファイルずつ段階的に実施する
- 1ファイルの分割ごとにビルド・テストを確認してからコミットする
