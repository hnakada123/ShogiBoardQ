# ShogiBoardQ テストスイート

## 概要

Qt Test フレームワークによるユニットテスト・統合テストスイート。57 テスト実行ファイルを収録し、コアロジックから棋譜フォーマット変換、合法手検証、配線契約、構造的KPI、統合ストレステストまでをカバーする。

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

57 テスト実行ファイル、57 CTest ケース（各実行ファイルに複数のテスト関数を含む）。

```
 1/57 tst_coredatastructures ............... Passed
 2/57 tst_errorbus ......................... Passed
 3/57 tst_shogiboard ....................... Passed
 4/57 tst_shogimove ........................ Passed
 5/57 tst_shogiutils ....................... Passed
 6/57 tst_movevalidator .................... Passed
 7/57 tst_sfentracer ....................... Passed
 8/57 tst_shogiclock ....................... Passed
 9/57 tst_kifconverter ..................... Passed
10/57 tst_ki2converter ..................... Passed
11/57 tst_csaconverter ..................... Passed
12/57 tst_jkfconverter ..................... Passed
13/57 tst_usiconverter ..................... Passed
14/57 tst_usenconverter .................... Passed
15/57 tst_kifubranchtree ................... Passed
16/57 tst_livegamesession .................. Passed
17/57 tst_navigation ....................... Passed
18/57 tst_gamerecordmodel .................. Passed
19/57 tst_kifu_comment_sync ................ Passed
20/57 tst_josekiwindow ..................... Passed
21/57 tst_positionedit_gamestart ........... Passed
22/57 tst_preset_gamestart_cleanup ......... Passed
23/57 tst_integration ...................... Passed
24/57 tst_usiprotocolhandler ............... Passed
25/57 tst_ui_display_consistency ........... Passed
26/57 tst_analysisflow ..................... Passed
27/57 tst_game_start_flow .................. Passed
28/57 tst_game_end_handler ................. Passed
29/57 tst_game_start_orchestrator .......... Passed
30/57 tst_fmvbitboard81 .................... Passed
31/57 tst_fmvconverter ..................... Passed
32/57 tst_fmvposition ...................... Passed
33/57 tst_fmvlegalcore ..................... Passed
34/57 tst_enginemovevalidator_compat ....... Passed
35/57 tst_enginemovevalidator_context ...... Passed
36/57 tst_fmv_perft ........................ Passed
37/57 tst_enginemovevalidator_crosscheck ... Passed
38/57 tst_parsecommon ...................... Passed
39/57 tst_layer_dependencies ............... Passed
40/57 tst_structural_kpi ................... Passed
41/57 tst_csaprotocol ...................... Passed
42/57 tst_settings_roundtrip ............... Passed
43/57 tst_app_lifecycle_pipeline ........... Passed
44/57 tst_app_game_session ................. Passed
45/57 tst_app_kifu_load .................... Passed
46/57 tst_app_ui_state_policy .............. Passed
47/57 tst_app_branch_navigation ............ Passed
48/57 tst_wiring_contracts ................. Passed
49/57 tst_matchcoordinator ................. Passed
50/57 tst_gamestrategy ..................... Passed
51/57 tst_app_error_handling ............... Passed
52/57 tst_wiring_csagame ................... Passed
53/57 tst_wiring_analysistab ............... Passed
54/57 tst_wiring_consideration ............. Passed
55/57 tst_wiring_playerinfo ................ Passed
56/57 tst_lifecycle_scenario ............... Passed
57/57 tst_wiring_slot_coverage ............. Passed

100% tests passed, 0 tests failed out of 57
```

## テストファイル構成

```
tests/
├── CMakeLists.txt                        # テストビルド定義
├── test_stubs.cpp                        # ロギングカテゴリスタブ
├── test_stubs_*.cpp                      # テスト固有スタブ群
├── fixtures/                             # テスト用棋譜フィクスチャ
│   ├── test_basic.kif                    # KIF形式（7手平手対局）
│   ├── test_branch.kif                   # KIF形式（分岐付き）
│   ├── test_comments.kif                 # KIF形式（コメント付き）
│   ├── test_basic.ki2                    # KI2形式
│   ├── test_basic.csa                    # CSA形式
│   ├── test_basic.jkf                    # JKF形式（JSON）
│   ├── test_basic.usi                    # USI形式
│   └── test_basic.usen                   # USEN形式
│
│   ── コアロジック ──
├── tst_coredatastructures.cpp            # 基本データ構造
├── tst_errorbus.cpp                      # ErrorBus
├── tst_shogiboard.cpp                    # 盤面状態管理
├── tst_shogimove.cpp                     # 指し手表現
├── tst_shogiutils.cpp                    # ユーティリティ
├── tst_movevalidator.cpp                 # 駒移動ルール
├── tst_sfentracer.cpp                    # SFEN局面追跡
├── tst_shogiclock.cpp                    # 対局時計
│
│   ── 合法手検証 (EngineMoveValidator) ──
├── tst_fmvbitboard81.cpp                 # Bitboard81
├── tst_fmvconverter.cpp                  # SFEN↔内部変換
├── tst_fmvposition.cpp                   # Position do/undo
├── tst_fmvlegalcore.cpp                  # 合法手生成コア
├── tst_enginemovevalidator_compat.cpp    # 互換ラッパ
├── tst_enginemovevalidator_context.cpp   # Context API
├── tst_enginemovevalidator_crosscheck.cpp # 既知値クロスチェック
├── tst_fmv_perft.cpp                     # Perft テスト
│
│   ── 棋譜形式変換 ──
├── tst_kifconverter.cpp                  # KIF形式
├── tst_ki2converter.cpp                  # KI2形式
├── tst_csaconverter.cpp                  # CSA形式
├── tst_jkfconverter.cpp                  # JKF形式
├── tst_usiconverter.cpp                  # USI形式
├── tst_usenconverter.cpp                 # USEN形式
├── tst_parsecommon.cpp                   # 棋譜共通ユーティリティ
│
│   ── 棋譜・ナビゲーション ──
├── tst_kifubranchtree.cpp                # 分岐木
├── tst_livegamesession.cpp               # 対局セッション
├── tst_navigation.cpp                    # ナビゲーション
├── tst_gamerecordmodel.cpp               # 棋譜データモデル
├── tst_kifu_comment_sync.cpp             # コメント同期
│
│   ── 対局・ゲーム進行 ──
├── tst_game_start_flow.cpp               # 対局開始フロー
├── tst_game_start_orchestrator.cpp       # 対局開始オーケストレータ
├── tst_game_end_handler.cpp              # 終局処理
├── tst_matchcoordinator.cpp              # 対局進行コーディネータ
├── tst_gamestrategy.cpp                  # 対局モード別Strategy
│
│   ── エンジン・プロトコル ──
├── tst_usiprotocolhandler.cpp            # USIプロトコル
├── tst_csaprotocol.cpp                   # CSAプロトコル
│
│   ── UI・解析 ──
├── tst_josekiwindow.cpp                  # 定跡ウィンドウ
├── tst_positionedit_gamestart.cpp        # 局面編集→対局開始
├── tst_preset_gamestart_cleanup.cpp      # プリセット対局開始→クリーンアップ
├── tst_ui_display_consistency.cpp        # UI表示整合性
├── tst_analysisflow.cpp                  # 解析フロー
├── tst_app_ui_state_policy.cpp           # UI状態ポリシー
│
│   ── 配線契約・構造検証 ──
├── tst_wiring_contracts.cpp              # MatchCoordinatorWiring契約
├── tst_wiring_csagame.cpp                # CsaGameWiring契約
├── tst_wiring_analysistab.cpp            # AnalysisTabWiring契約
├── tst_wiring_consideration.cpp          # ConsiderationWiring契約
├── tst_wiring_playerinfo.cpp             # PlayerInfoWiring契約
├── tst_wiring_slot_coverage.cpp          # 接続漏れ/重複検出
│
│   ── アプリ構造・ライフサイクル ──
├── tst_app_lifecycle_pipeline.cpp        # 起動/終了フロー構造契約
├── tst_app_game_session.cpp              # 対局セッション構造契約
├── tst_app_kifu_load.cpp                 # 棋譜ロード構造契約
├── tst_app_branch_navigation.cpp         # 分岐ナビゲーション構造契約
├── tst_app_error_handling.cpp            # 異常系・防御パターン構造契約
├── tst_lifecycle_scenario.cpp            # 全フェーズシナリオ
├── tst_layer_dependencies.cpp            # レイヤ間依存方向検証
├── tst_structural_kpi.cpp                # コード品質指標
│
│   ── 設定 ──
├── tst_settings_roundtrip.cpp            # 設定の保存→復元
│
│   ── 統合テスト ──
└── tst_integration.cpp                   # 複数コンポーネント連携
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

### 4. tst_movevalidator（駒移動ルール）

駒種別の移動ルール検証。

※ 旧 `tst_fastmovevalidator` は `tst_movevalidator` + `tst_fmv*` + `tst_enginemovevalidator_*` に分割された。

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
| `testRepeatedRandomGames` | ShogiBoard + EngineMoveValidator でランダム合法手対局×100回。不変条件: 盤上+駒台の駒数合計==40 |
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

| カテゴリ | テスト実行ファイル | CTest数 |
|---------|-------------------|--------|
| コアロジック | tst_coredatastructures, tst_errorbus, tst_shogiboard, tst_shogimove, tst_shogiutils, tst_movevalidator, tst_sfentracer, tst_shogiclock | 8 |
| 合法手検証 (EMV) | tst_fmvbitboard81, tst_fmvconverter, tst_fmvposition, tst_fmvlegalcore, tst_enginemovevalidator_compat, tst_enginemovevalidator_context, tst_enginemovevalidator_crosscheck, tst_fmv_perft | 8 |
| 棋譜形式変換 | tst_kifconverter, tst_ki2converter, tst_csaconverter, tst_jkfconverter, tst_usiconverter, tst_usenconverter, tst_parsecommon | 7 |
| 棋譜・ナビゲーション | tst_kifubranchtree, tst_livegamesession, tst_navigation, tst_gamerecordmodel, tst_kifu_comment_sync | 5 |
| 対局・ゲーム進行 | tst_game_start_flow, tst_game_start_orchestrator, tst_game_end_handler, tst_matchcoordinator, tst_gamestrategy | 5 |
| エンジン・プロトコル | tst_usiprotocolhandler, tst_csaprotocol | 2 |
| UI・解析 | tst_josekiwindow, tst_positionedit_gamestart, tst_preset_gamestart_cleanup, tst_ui_display_consistency, tst_analysisflow, tst_app_ui_state_policy | 6 |
| 配線契約・構造検証 | tst_wiring_contracts, tst_wiring_csagame, tst_wiring_analysistab, tst_wiring_consideration, tst_wiring_playerinfo, tst_wiring_slot_coverage | 6 |
| アプリ構造・ライフサイクル | tst_app_lifecycle_pipeline, tst_app_game_session, tst_app_kifu_load, tst_app_branch_navigation, tst_app_error_handling, tst_lifecycle_scenario, tst_layer_dependencies, tst_structural_kpi | 8 |
| 設定 | tst_settings_roundtrip | 1 |
| 統合テスト | tst_integration | 1 |
| **合計** | | **57** |

## テストフィクスチャ

全フォーマットで同一の7手平手対局を使用:

```
▲７六歩 △３四歩 ▲２六歩 △８四歩 ▲２五歩 △８五歩 ▲７八金
```

USI表記: `7g7f 3c3d 2g2f 8c8d 2f2e 8d8e 6i7h`

分岐付きフィクスチャ（`test_branch.kif`）には 3手目と 5手目に変化手順を含む。
