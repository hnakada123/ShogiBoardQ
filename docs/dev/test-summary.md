# ShogiBoardQ テストスイート

## 概要

Qt Test フレームワークによるユニットテスト・統合テストスイート。18 のテスト実行ファイルを収録し、コアロジックから棋譜フォーマット変換、統合ストレステストまでをカバーする。

## ビルドと実行

```bash
# テスト付きビルド（デフォルトは BUILD_TESTING=OFF）
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build

# 全テスト実行
cd build && ctest --output-on-failure

# 個別テスト実行
ctest --output-on-failure -R tst_shogiboard

# 詳細出力
./build/tests/tst_shogiboard -v2
```

## テスト結果

```
 1/18 tst_coredatastructures ... Passed
 2/18 tst_shogiboard .......... Passed
 3/18 tst_sfentracer .......... Passed
 4/18 tst_fastmovevalidator ... Passed
 5/18 tst_shogiclock .......... Passed
 6/18 tst_kifconverter ........ Passed
 7/18 tst_ki2converter ........ Passed
 8/18 tst_csaconverter ........ Passed
 9/18 tst_jkfconverter ........ Passed
10/18 tst_usiconverter ........ Passed
11/18 tst_usenconverter ....... Passed
12/18 tst_kifubranchtree ...... Passed
13/18 tst_livegamesession ..... Passed
14/18 tst_navigation .......... Passed
15/18 tst_gamerecordmodel ..... Passed
16/18 tst_josekiwindow ........ Passed
17/18 tst_positionedit_gamestart ... Passed
18/18 tst_integration ......... Passed

100% tests passed, 0 tests failed out of 18
```

## テストファイル構成

```
tests/
├── CMakeLists.txt            # テストビルド定義
├── test_stubs.cpp            # ロギングカテゴリスタブ
├── fixtures/                 # テスト用棋譜フィクスチャ
│   ├── test_basic.kif        # KIF形式（7手平手対局）
│   ├── test_branch.kif       # KIF形式（分岐付き）
│   ├── test_comments.kif     # KIF形式（コメント付き）
│   ├── test_basic.ki2        # KI2形式
│   ├── test_basic.csa        # CSA形式
│   ├── test_basic.jkf        # JKF形式（JSON）
│   ├── test_basic.usi        # USI形式
│   └── test_basic.usen       # USEN形式
├── tst_coredatastructures.cpp
├── tst_shogiboard.cpp
├── tst_sfentracer.cpp
├── tst_fastmovevalidator.cpp
├── tst_shogiclock.cpp
├── tst_kifconverter.cpp
├── tst_ki2converter.cpp
├── tst_csaconverter.cpp
├── tst_jkfconverter.cpp
├── tst_usiconverter.cpp
├── tst_usenconverter.cpp
├── tst_kifubranchtree.cpp
├── tst_livegamesession.cpp
├── tst_navigation.cpp
├── tst_gamerecordmodel.cpp
└── tst_integration.cpp
```

## テスト詳細

### 1. tst_coredatastructures（16テスト）

基本データ構造のユニットテスト。

| テスト名 | テスト対象 | 検証内容 |
|----------|-----------|---------|
| `shogiMove_defaultConstructor` | ShogiMove | デフォルトコンストラクタの初期値 |
| `shogiMove_parameterConstructor` | ShogiMove | パラメータコンストラクタの各フィールド |
| `shogiMove_equalityOperator` | ShogiMove | == 演算子の同値・非同値判定 |
| `shogiMove_dropFromPieceStand` | ShogiMove | 駒台からの打ち（fromSquare.x() >= 9） |
| `turnManager_initialState` | TurnManager | 初期状態の side() |
| `turnManager_setPlayer1` | TurnManager | Player1設定 → toSfenToken()=="b", toClockPlayer()==1 |
| `turnManager_setPlayer2` | TurnManager | Player2設定 → toSfenToken()=="w", toClockPlayer()==2 |
| `turnManager_toggle` | TurnManager | Player1↔Player2 の切替 |
| `turnManager_setFromSfenToken` | TurnManager | SFEN トークン "w" → Player2 |
| `turnManager_changedSignal` | TurnManager | changed() シグナルの発火（QSignalSpy） |
| `shogiUtils_transRankTo` | ShogiUtils | 段の変換: 1→"一", 5→"五", 9→"九" |
| `shogiUtils_transFileTo` | ShogiUtils | 筋の変換: 1→"１", 9→"９" |
| `shogiUtils_moveToUsi_boardMove` | ShogiUtils | 盤上移動 → USI文字列変換 |
| `shogiUtils_moveToUsi_promotion` | ShogiUtils | 成り移動 → USI文字列変換（末尾 "+"） |
| `shogiUtils_moveToUsi_drop` | ShogiUtils | 駒打ち → USI文字列変換（"P*5e"形式） |
| `jishogi_calculate` | JishogiCalculator | 大駒5点・小駒1点の入玉宣言計算 |

### 2. tst_shogiboard（15テスト）

盤面状態管理のユニットテスト。

| テスト名 | 検証内容 |
|----------|---------|
| `setSfen_hirate` | 平手初期局面SFENの設定と81マスの駒配置 |
| `sfen_roundTrip_hirate` | 初期局面の setSfen → convertBoardToSfen ラウンドトリップ |
| `sfen_roundTrip_midgame` | 中盤局面のSFENラウンドトリップ |
| `sfen_promotedPieces` | 成駒含むSFEN（"+P"→内部表現 'Q'） |
| `convertStandToSfen` | 空の駒台 → SFEN "-" |
| `convertStandToSfen_withPieces` | 駒台に歩3枚 → SFEN "3P" |
| `movePiece_basic` | 基本移動: 元マス空白、先マスに駒 |
| `movePiece_withPromotion` | 成り移動: 成駒が配置される |
| `addPieceToStand_normalPiece` | 通常駒の取得 → 駒台+1 |
| `addPieceToStand_promotedPiece` | 成駒の取得 → 元駒に変換して駒台追加 |
| `incrementDecrementStand` | 駒台の increment/decrement |
| `resetGameBoard` | 全マス空白化、全駒が駒台へ |
| `flipSides` | 先後反転（180°回転 + 大文字小文字入替） |
| `signal_boardReset` | boardReset シグナルの発火 |
| `signal_dataChanged` | dataChanged シグナルの発火 |

### 3. tst_sfentracer（8テスト）

SFEN局面追跡のユニットテスト。

| テスト名 | 検証内容 |
|----------|---------|
| `resetToStartpos` | 初期局面リセット → SFEN一致 |
| `setFromSfen_roundTrip` | SFEN設定 → 出力ラウンドトリップ |
| `applyUsiMove_pawnAdvance` | USI手 "7g7f" 適用: 7gが空白、7fにP |
| `applyUsiMove_bishopPromote` | USI手 "8h2b+" 適用: 角成り |
| `applyUsiMove_drop` | USI手 "P*5e" 適用: 歩打ち |
| `generateSfensForMoves` | 3手適用 → 3つのSFEN生成 |
| `buildGameMoves` | USI手列 → ShogiMove列変換 |
| `buildSfenRecord` | 初期局面+USI手列 → SFEN列構築 |

### 4. tst_fastmovevalidator（9テスト）

合法手判定のユニットテスト。

| テスト名 | 検証内容 |
|----------|---------|
| `legalMoves_hirate` | 平手初期局面の先手合法手数 == 30 |
| `isLegalMove_pawnAdvance` | 7g7f（歩前進）が合法 |
| `isLegalMove_pawnBackward_illegal` | 歩の後退が非合法 |
| `twoPawn_illegal` | 二歩判定: 同筋に歩がある場合の打ち歩が非合法 |
| `kingInCheck` | 王手判定: checkIfKingInCheck() >= 1 |
| `promotionInEnemyTerritory` | 敵陣での成り: promotingMoveExists == true |
| `mandatoryPromotion_pawnOnFirstRank` | 強制成り: 1段目歩で不成非合法、成のみ合法 |

### 5. tst_shogiclock（9テスト）

対局時計のユニットテスト。

| テスト名 | 検証内容 |
|----------|---------|
| `setPlayerTimes_basic` | 持ち時間300秒 → 300000ms |
| `timeString_format` | 300秒 → "00:05:00" 形式 |
| `byoyomi_setup` | 秒読み設定 → hasByoyomi == true |
| `byoyomi_application` | 秒読み30秒適用後 → 30000ms |
| `fischer_increment` | フィッシャー加算: 300秒 + 10秒 → 310000ms |
| `undo_restoresState` | startClock→applyByoyomi→undo で状態復元 |
| `stressTest_startStop` | startClock/stopClock 100回繰り返し → クラッシュなし |
| `stressTest_byoyomiUndo` | applyByoyomi + undo 50回繰り返し → 状態整合 |
| `byoyomi_and_fischer_exclusive` | 秒読みとフィッシャーの排他設定 |

### 6. tst_kifconverter（7テスト）

KIF形式コンバータのユニットテスト。

| テスト名 | 検証内容 |
|----------|---------|
| `detectInitialSfen` | ファイルから平手SFEN検出 |
| `convertFile_sevenMoves` | 7手 → USI手列 ["7g7f", "3c3d", ...] |
| `extractMovesWithTimes` | KifDisplayItem 8個（開始局面 + 7手）、先手初手 "７六歩" 含有 |
| `parseWithVariations_basic` | 基本棋譜: 本譜7手、分岐なし |
| `parseWithVariations_branch` | 分岐棋譜: 本譜7手 + 2分岐（ply 3, 5） |
| `extractGameInfo` | 先手名・後手名の抽出 |
| `comments_extraction` | コメント付き棋譜からのコメント抽出 |

### 7. tst_ki2converter（3テスト）

KI2形式コンバータのユニットテスト。

| テスト名 | 検証内容 |
|----------|---------|
| `detectInitialSfen` | KI2ファイルから平手SFEN検出 |
| `convertFile_sevenMoves` | KIF同等のUSI手列を出力 |
| `extractMovesWithTimes` | KifDisplayItem 8個（開始局面 + 7手） |

### 8. tst_csaconverter（2テスト）

CSA形式コンバータのユニットテスト。

| テスト名 | 検証内容 |
|----------|---------|
| `parse_basic` | CSA解析: 本譜7手、初期局面は平手 |
| `extractGameInfo` | 先手名・後手名の抽出 |

### 9. tst_jkfconverter（3テスト）

JKF形式コンバータのユニットテスト。

| テスト名 | 検証内容 |
|----------|---------|
| `detectInitialSfen` | JKFから平手SFEN検出 |
| `convertFile_sevenMoves` | 7手USI手列の一致 |
| `mapPresetToSfen_hirate` | "HIRATE" → 平手SFEN変換 |

### 10. tst_usiconverter（2テスト）

USI形式コンバータのユニットテスト。

| テスト名 | 検証内容 |
|----------|---------|
| `detectInitialSfen` | USI形式から平手SFEN検出 |
| `convertFile_sevenMoves` | 7手USI手列の一致 |

### 11. tst_usenconverter（4テスト）

USEN形式コンバータのユニットテスト。

| テスト名 | 検証内容 |
|----------|---------|
| `detectInitialSfen` | USEN形式から平手SFEN検出 |
| `convertFile_sevenMoves` | 7手USI手列の一致 |
| `decodeUsenMoves` | USENエンコード文字列のデコード |
| `crossFormat_matchesKif` | KIF と USEN の convertFile 結果が同一 |

### 12. tst_kifubranchtree（13テスト）

棋譜分岐木のユニットテスト。

| テスト名 | 検証内容 |
|----------|---------|
| `setRootSfen` | ルートノード生成、ply==0 |
| `addMove_basic` | ノード追加、ply==parent->ply()+1 |
| `nodeCount` | 7手追加後 → 8ノード（root含む） |
| `mainLine` | 本譜ライン: 8ノード |
| `allLines` | 分岐付きで2ライン |
| `pathToNode` | 分岐末端 → ルートへのパス |
| `hasBranch` | 分岐点で true |
| `clear` | ツリークリア → isEmpty() |
| `signal_treeChanged` | treeChanged シグナルの発火 |
| `signal_nodeAdded` | nodeAdded シグナルの発火 |
| `stressTest_200Moves` | 200手連続追加 → クラッシュなし |
| `lineCount` | 分岐付きツリーのライン数 |
| `findByPlyOnMainLine` | 本譜上の指定手数ノード検索 |

### 13. tst_livegamesession（7テスト）

対局セッション管理のユニットテスト。

| テスト名 | 検証内容 |
|----------|---------|
| `startFromRoot` | ルートから開始 → isActive, anchorPly==0 |
| `startFromNode` | 指定ノードから開始 → anchorPly==指定値 |
| `addMove_incrementsMoveCount` | 指し手追加で moveCount 増加 |
| `commit_addsToTree` | コミット → ツリーにノード追加 |
| `discard_resetsSession` | 破棄 → セッション状態リセット |
| `stressTest_repeatedSessions` | start→addMove×10→commit を50回繰り返し |
| `totalPly` | anchorPly + moveCount の合計 |

### 14. tst_navigation（9テスト）

棋譜ナビゲーションのユニットテスト。

| テスト名 | 検証内容 |
|----------|---------|
| `goToFirst` | 先頭へ移動 → ply==0, node==root |
| `goToLast` | 末尾へ移動 → ply==7 |
| `goForward` | 1手進む → ply 1→2 |
| `goBack` | 1手戻る → ply 5→4→3 |
| `canGoForward_atEnd` | 末尾で canGoForward==false |
| `canGoBack_atRoot` | ルートで canGoBack==false |
| `isOnMainLine` | 本譜上で true |
| `goToPly` | 指定手数へ直接移動 |
| `stressTest_randomNavigation` | goFirst/goLast/goForward/goBack を1000回ランダム繰り返し |

### 15. tst_gamerecordmodel（9テスト）

棋譜データモデルのユニットテスト。

| テスト名 | 検証内容 |
|----------|---------|
| `setComment_roundTrip` | コメント設定・取得のラウンドトリップ |
| `commentChanged_signal` | commentChanged シグナルの発火 |
| `setBookmark_roundTrip` | しおり設定・取得のラウンドトリップ |
| `clear_resetsAll` | クリア → 全データリセット |
| `dirty_flag` | setComment → isDirty==true → clearDirty() |
| `toKifLines` | KIF形式エクスポート: "手合割：平手" 含有 |
| `toKi2Lines` | KI2形式エクスポート: ▲△ 付き指し手 |
| `toJkfLines` | JKF形式エクスポート: JSON パース可能 |
| `toUsenLines` | USEN形式エクスポート: "~0." 開始 |

### 16. tst_integration（5テスト）

統合ストレステスト。複数コンポーネントを組み合わせた操作を繰り返し、クラッシュや状態破壊を検出する。

| テスト名 | 検証内容 |
|----------|---------|
| `testRepeatedRandomGames` | ShogiBoard + FastMoveValidator でランダム合法手対局×100回。不変条件: 盤上+駒台の駒数合計==40 |
| `testKifuLoadAndNavigate` | KIF読込 → ツリー構築 → ランダムナビゲーション1000回。不変条件: currentNode()!=nullptr |
| `testRepeatedLiveGameSessions` | start→addMove×10→commit を20サイクル。不変条件: lineCount 単調増加 |
| `testSfenConsistencyInvariant` | 複数SFENで setSfen→convertBoardToSfen ラウンドトリップ×100回 |
| `testFullWorkflow` | KIF読込→ツリー構築→ナビゲーション→LiveGameSession→commit→結果検証 |

## テスト中に発見・修正されたバグ

### ki2tosfenconverter.cpp: kTerminalWords 配列サイズ不一致

**ファイル:** `src/kifu/formats/ki2tosfenconverter.cpp:58`

**問題:** `std::array<QString, 17>` と宣言されていたが、実際の要素は 16 個しかなかった。17番目の要素がデフォルトコンストラクタにより空の `QString` で初期化され、`containsAnyTerminal()` 内の `s.contains("")` が常に `true` を返すため、KI2形式の全ての指し手行が終局語として判定されていた。

**影響:** KI2形式の棋譜ファイルが全く解析できない状態だった。

**修正:** 配列サイズを `17` から `16` に変更。

```cpp
// 修正前
static const std::array<QString, 17> a = {{ ... /* 16要素 */ }};

// 修正後
static const std::array<QString, 16> a = {{ ... /* 16要素 */ }};
```

## テストカバレッジの範囲

| カテゴリ | 対象クラス | テスト数 |
|---------|-----------|---------|
| 基本データ構造 | ShogiMove, TurnManager, ShogiUtils, JishogiCalculator | 16 |
| 盤面管理 | ShogiBoard | 15 |
| 局面追跡 | SfenPositionTracer | 8 |
| 合法手判定 | FastMoveValidator | 9 |
| 対局時計 | ShogiClock | 9 |
| 棋譜形式変換 | KIF, KI2, CSA, JKF, USI, USEN コンバータ | 21 |
| 棋譜分岐木 | KifuBranchTree | 13 |
| 対局セッション | LiveGameSession | 7 |
| ナビゲーション | KifuNavigationController, KifuNavigationState | 9 |
| 棋譜データモデル | GameRecordModel | 9 |
| 統合テスト | 複数コンポーネント連携 | 5 |
| **合計** | | **更新中（テスト追加に追随）** |

## テストフィクスチャ

全フォーマットで同一の7手平手対局を使用:

```
▲７六歩 △３四歩 ▲２六歩 △８四歩 ▲２五歩 △８五歩 ▲７八金
```

USI表記: `7g7f 3c3d 2g2f 8c8d 2f2e 8d8e 6i7h`

分岐付きフィクスチャ（`test_branch.kif`）には 3手目と 5手目に変化手順を含む。
