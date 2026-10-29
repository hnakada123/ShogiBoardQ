# コード監査：責務の整理（2026-10-29）

「ソースコードが責務できちんと整理されているか」という依頼で、`src/` 全体（約8.6万行）と `mcp/` を監査した。前回の監査（`code-audit-2026-10-23.md`）の項目をすべて直した後の状態が対象である。その後の54コミットで、src は約8,900行増え、約9,400行減っている。

- 対象のコミットは 0b441c2e。本文の行番号はこの時点のもの。
- 6つの範囲に分けてコードを読み、呼び出し元を grep で確かめた。
  - core・kifu
  - engine・game・analysis・network・headless
  - app・ui・navigation・board・common
  - widgets・views・models・style
  - dialogs
  - services・automation・cli・mcp
- 観点は次のとおり。
  - 1つのクラスが複数の責務を持っていないか
  - 同じ状態・規則を複数の場所で持っていないか
  - 層・ディレクトリの置き場所は合っているか
  - 転送だけの層や名残がないか
  - 名前と CLAUDE.md の説明が実態と合っているか
- 前回の文書と同じ指摘は除いた。ただし、前回「済」とした項目が残っていたものは挙げた（5章）。
- 主な指摘は、範囲ごとの監査とは別に、もう一度コードを読んで確かめた。
- 監査ではソースを変更していない。

> **修正の進み具合**: 7章の順に直し、各項目に「状態: 済（コミット）」を書く。途中で止まったら、状態のない項目から再開する。

> **決めた仕様（2026-10-29）**: 監査で判断が要るとした4つは、次のとおりに決めた。詳しくは各項目に書いた。
> - 途中から続けた対局では、対局情報に開始日時・持ち時間を書かない。CSA も対局の記録から足さない（T4・F3）。
> - 読み込んだ棋譜を保存するときの既定のファイル名は、対局情報の対局者名から作る（U1・T4）。
> - 外観のテーマのコンボはやめ、「おすすめの組み合わせ」に1つにする（T7）。
> - 自動化 API の `widget.showDock`・MCP の `show_dock` は名前を残し、中身は `dock.configure` の show と同じ処理にする（5章）。

## 全体の評価

**構造の骨組みは整理できている。** 機械的に測れる指標は、どれも規則を満たしている。

- **層の依存**: `tst_layer_dependencies` は通る。ディレクトリの間で互いに include しあっているのは views と widgets の1組だけ（各4か所）。
- **構造の指標**: `tst_structural_kpi` は通る。
  - 1000行を超えるファイル: 0（最大は `josekiwindow.cpp` の911行）
  - connect にラムダを渡す箇所: 0
  - MainWindow: 45行、include は5つ
  - 裸の delete: 1（`flowlayout.cpp`）
- **合成ルート**: 72個の部品を作り、依存を注入し、シグナルをつなぐだけ。遅延生成も、部品を後から取り出すゲッターもない。
- **Deps の大きさ**: 最大は `GameSession` の17欄。Deps に std::function を持つのは `UiStatePolicyManager` の2つだけ。
- **CLAUDE.md**: 書かれているファイル名・クラス名は、すべて実在する。

**残っている問題は、部品の中身と部品の境目にある。** 6つの範囲で別々に見つかった問題を突き合わせると、次の5つに分けられる。

1. **同じ状態・規則の正本が複数ある。** いちばん多い。
   - 例: 盤の矢印を出すか（3通り）、対局情報と対局の記録、外観の状態、列幅の保存（5通り）、保存形式の表（4か所）、マスの大きさの範囲（7か所）、評価値の向きの直し、局面編集の規則、エンジン一覧の検索。
2. **ローカル対局と通信対局で流れが2本ある。**
   - 前回 R8 で、エンジンの枠と終局の手順は共有になった。棋譜の受け渡し・動作状態の持ち主・ハイライト・時計の設定は、別の仕組みのまま残っている。
3. **「実行」と「画面」が同じクラスにある。**
   - 棋譜解析の実行が ui のコントローラにある。
   - 盤のクリックの解釈が game の `ShogiGameController` にある。
   - 定跡ウィンドウ（911行）が画面とコントローラの生成・設定の保存を兼ねる。
4. **ディレクトリ・名前が役割と合わない。**
   - ui/coordinators に Controller がある。
   - dialogs にドックのパネルがある。
   - services に GUI 専用の外観・駒音がある。
   - automation に CLI 専用の盤面描画がある。
   - navigation が QtWidgets を使う。
   - `ShogiGameController`・`EngineLifecycleManager`・`KifuExportMetadataBuilder` は名前と中身が合わない。
5. **名残。** 呼び出しのない関数・欄・シグナル、古い説明。

複数の範囲で別々に見つかった問題は確度が高い。次の3つがそれに当たる。
- 通信対局の流れ（app・ui の監査と network の監査）
- 棋譜解析の実行の置き場所（ui の監査と analysis の監査）
- 盤の矢印の正本（widgets の監査と engine の監査）

---

## 1. 責務の重複から生じている挙動の食い違い

書き直しとは別に、個別に直せる。どれもコードの経路を読んで確かめたもので、アプリでは実行していない。

### 1.1 コードで確認したもの

| # | 食い違い | 原因 |
|---|---|---|
| F1 | 同じ棋譜を GUI の棋譜解析と CLI の `analyze-kifu` で解析すると、エンジンに送る局面が違う。GUI は SFEN だけを送り、CLI は開始局面と指し手を送る（CLI のコメントでは「千日手の判断材料」）。ほかにも違いがある。<br>- bestmove の待ち: GUI は期限なし、CLI は10秒<br>- MultiPV: GUI は送らない、CLI は送る | 局面の列を解析する実行器が2つある（T2）。<br>`analysisjob.cpp:22`<br>`clicommands_engine.cpp:244-256`<br>`engineanalysisrunner.cpp:39,55`<br>**状態: 済（f4079f7f）**。送る局面だけを直した。GUI も CLI と同じく `UsiPosition::alongMoves` で手順を1度だけたどり、開始局面と指し手で送る（手順をたどれない棋譜だけ局面だけで送る）。待ち時間と MultiPV の違いは T2 で扱う（テスト `jobRunsWithoutUiAndOwnsSnapshot` が送った position を確かめる） |
| F2 | 英語表示で、対局していない新規の棋譜を保存すると、対局者名に「Black」「White」が入る（KIF では「先手：Black」） | `KifuDocument::defaultHeader` が対局者名の値を `tr("先手")`・`tr("後手")` にしている（`kifudocument.cpp:92-93`）。表示言語がファイルの中身に入る。名前の表示の補いは `PlayerInfoPresenter` が別に持っている（T4）<br>**状態: 済（f4079f7f）**。既定の対局者名を空にした。盤は今までどおり `PlayerInfoPresenter` が「先手」「後手」と補い、対局情報の欄は入力の案内（「未設定（名前を入力できます）」）を出す。KIF は「先手：」と書く。CSA の `N+`・`N-` も訳した語（「先手」「Black」「エンジン」「引擎」など）で補わず、名前が無ければ書かない（`KifuExportMetadataBuilder::resolvePlayerNames` を消した） |
| F3 | 対局の後に、対局情報の「持ち時間」を手で直して保存すると、形式によって書く値が違う。KIF は直した値を書き、CSA は対局の記録の値を `$TIME` に書く。途中から続けた対局では、CSA にだけ開始日時（`$START_TIME`）が出る | 書き出しの文脈が対局情報と対局の記録の両方を運び、CSA だけが記録を読む（`csaexporter.cpp:88-93,110,115-127`）（T4）<br>**状態: 済（f4079f7f）**。CSA も `$START_TIME`・`$TIME` を対局情報だけから書く。どこからも読まれなくなった `KifuExportMetadata` の `timeControl`・`gameStartDateTime`・`gameEndDateTime` を消した（テスト `csaWritesTimesFromGameInfo`） |
| F4 | 通信対局で駒を打つと、駒台の移動元のハイライトが消える。ローカル対局では出る | `BoardSyncPresenter` がハイライトを出した直後に、通信対局だけ `moveHighlightRequested` でもう一度出す。このとき駒打ちの移動元を `(-1,-1)` にしている（`csagamecoordinator_moves.cpp:80,143`→`csagamesession.cpp:36-39`→`boardinteractioncontroller.cpp:135-141`）（T1）<br>**状態: 済（f4079f7f）**。`moveHighlightRequested` と `CsaGameSession::Deps::boardController` を消した。通信対局の着手もローカル対局と同じく、棋譜の表示中の手から `BoardSyncPresenter` がハイライトを出す。`validateMoveText` は座標を返さず、通知文のための検査だけにした |

### 1.2 疑い・仕様の確認が要るもの

| # | 疑い | 根拠 |
|---|---|---|
| U1 | 対局者名の入った棋譜を開いて保存すると、既定のファイル名が「日時_Session」になる（対局した棋譜なら「日時_先手_vs_後手」） | 既定のファイル名は対局の記録 `players()` から作る（`kifufilecontroller.cpp:218,264`、`kifuioservice.cpp:48`）。棋譜を読むと記録は空になる（`kifudocument.cpp:129`）。意図した動作かもしれない<br>**仕様: 決定**。既定のファイル名は対局情報の対局者名（先手・後手、駒落ちは下手・上手）から作り、「日時_先手_vs_後手」にする。対局者名がなければ今までどおり「日時_Session」。対局した棋譜は対局情報と記録の対局者が同じなので、名前は変わらない。開いた元のファイル名は使わない（このアプリは読み込んだファイルの場所を持たず、自分で保存したファイルだけを上書きするため）<br>**状態: 済（f4079f7f）**。名前を付けて保存と自動保存の既定のファイル名を `KifuFileController::defaultFileName()` 1つで作り、対局情報の対局者名（`KifuDocument::headerPlayerName`）を渡す。途中から続けた対局も対局情報の対局者名を今の対局者にするので、自動保存の名前は変わらない（テスト `defaultFileNameUsesGameInfoPlayers`） |
| U2 | 検討タブの「矢印を表示」だけが保存されない。対局の矢印は保存される | 正本が3通りある（T6）。`considerationpanel.cpp:88-95,299` |
| U3 | メニューウィンドウのドックの大きさが保存されない | `MenuWindow` が独立ウィンドウだった頃の名残で、大きさは `closeEvent` で保存している（`menuwindow.cpp:421-440`）。ドックに入った子のウィジェットでは呼ばれないと思われる（P2） |
| U4 | 読み筋盤のマスの大きさが、本体の盤の今の大きさではなく、前回アプリを終了したときの大きさになる | `ShogiView` のコンストラクタが本体の盤の設定 `AppSettings::squareSize` を読む（`shogiview.cpp:46-48`）。書き込むのは終了時だけ（`mainwindowcomposer.cpp:76`）（M9） |
| U5 | 思考の表で列を狭めても、次の起動で戻らない | 戻すときに `qMax(最小値, 保存した幅)` にしている（`thinkingtableview.cpp:62-87`）。列幅の規則が部品ごとに違う（M8） |

---

## 2. 直す価値が大きい箇所（重要度順）

### T1. 通信対局の流れがローカル対局と別の仕組みになっている（app・ui・network・game）。F4 の原因

**問題**

ローカル対局と通信対局で、同じことを別の仕組みで行っている。

| | ローカル対局 | 通信対局 |
|---|---|---|
| 棋譜の受け渡し | `MatchRecordSink::appendRecord` を呼ぶ（`matchcoordinator.cpp:190,423`） | シグナル `gameStarted`・`recordEntryReady`・`gameEnded` を出し、`GameSession` のコンストラクタがつなぐ（`gamesession.cpp:70-74,278-281`） |
| 動作状態のリース | game の `MatchCoordinator` が取る（`matchcoordinator.cpp:75,526`） | ui の `CsaGameSession` が取る（`csagamesession.cpp:103`） |
| 表示の接続（評価値→グラフ、エラー→通知、ログ欄、盤のハイライト、人間の手） | 合成ルートがつなぐ（`mainwindowcomposer_wire.cpp:155-163`） | `CsaGameSession` が自分のコンストラクタでつなぐ（`csagamesession.cpp:29-50,136`） |
| 時計の設定 | `GameSession` が `TimeControlUtil::applyToClock` で入れる。手番は SFEN の2語目を切って求める（`gamesession.cpp:47-51,183`）。再開のときは司令塔が入れる（`matchcoordinator.cpp:531`） | `CsaGameCoordinator` が入れる（`csagamecoordinator_setup.cpp:63-79`） |
| 着手のハイライト | 棋譜の表示中の手が変わったのを受けて、`BoardSyncPresenter` が出す | 上と同じハイライトを、`moveHighlightRequested` でもう一度出す（F4）。そのために `validateMoveText` が CSA の指し手を読み直す（`csagamecoordinator_moves.cpp:57-83`）。同じ検査は `CsaMoveConverter::toUsi` にもある |

- 「`GameSession` が先に棋譜を作り、その後 `CsaGameSession` が通信対局中にする」という順番は、シグナルを接続した順でしか保証されていない。`composition-root.md:65` に「例外」として書かれている。
- CLAUDE.md の `GameSession` の説明「ローカル対局と通信対局の棋譜を開始から終局・自動保存まで1本の流れで扱い」と食い違う。

**書き直し案**

- `CsaGameCoordinator` にも `setRecordSink(GameSession)` で棋譜の受け手を渡す。開始と終局の受け口が要るなら、`MatchRecordSink` に足す。
- 開始局面の棋譜を作った後に、司令塔が自分で `DuringCsaGame` のリースを取る（`MatchCoordinator::start` と同じ形）。
- `moveHighlightRequested` と、`validateMoveText` の座標の出力を消す。ハイライトは `BoardSyncPresenter` の1か所にする。
- 時計の設定は `MatchStartOptions` に入れ、`MatchCoordinator::start` が盤の手番で時計に入れる。あわせて `MatchStartOptions` の `sfenStart`（`recordLine` の写し。`startoptions.h:34-35`）を消す。
- `CsaGameSession` には、待機ダイアログ・結果のダイアログ・ステータスバー・盤の反転だけを残す。表示の接続は合成ルートの `connectGame` へ移す。
- これで composition-root.md の「接続の順に頼る」例外がなくなる。

**影響**: network 3・game 3・app 2・ui 1・合成ルート。テストは `tst_csagamesession`・`tst_csagamecoordinator`・`tst_gamestrategy` など。差し引き約100行減。

### T2. 棋譜解析の実行が UI のコントローラにあり、局面の列の実行器が GUI と CLI で2つある（ui・analysis・headless・cli）。F1 の原因

**問題**

- `KifuAnalysisController`（ui/controllers、Deps 12欄）が、実行と画面の両方を持つ。
  - 実行: `AnalysisJob`・`EngineLease`・`ActivityLease` と、取り上げられたときの処理（`kifuanalysiscontroller.h:105-107`、`.cpp:114-171`）
  - 画面: 条件ダイアログ、結果のドック、評価値グラフ、棋譜の移動、読み筋盤
- 同じ種類の検討・詰み探索は、GUI に依存しない `AnalysisSession`（analysis）が実行を持ち、`ConsiderationUiController`・`TsumeSearchController` は伝えるだけになっている。棋譜解析だけが形が違う。
- 「局面の列を1つのエンジンで順に解析する」流れを、GUI の `AnalysisJob`（analysis）と CLI の `EngineAnalysisRunner`（headless）が別々に書いている。送る局面・待ち時間・MultiPV が違う（F1）。
- エンジンに送る局面の作り方が4通りある。
  - 検討の開始: 手順つき（`kifunavigationstate.cpp:76-81`→`RecordLine::usiPosition`）
  - 検討中の局面送り: SFEN だけ（`analysissession.cpp:160`）
  - 棋譜解析（GUI）: SFEN だけ（`analysisjob.cpp:22`）
  - 詰み探索: 文字列を組み立ててから `UsiPosition::parse` で読み直す（`tsumepositionutil.h:21-43`→`tsumesearchcontroller.cpp:27-29`）
- CLI の analyze-kifu は、`RecordLine` を持っているのに `usiMoves` の文字列を `UsiMove` に読み直している（`clicommands_engine.cpp:247-252`）。

**書き直し案**

- analysis に、局面の列の実行器を1つ置く。`UsiEngineSession` を受け取り、`positionFinished(PlyAnalysis)` を返す。GUI と CLI の両方がこれを使う。待ち時間・MultiPV は引数にする。
- analysis に `KifuAnalysisSession` を置く。
  - 持つもの: `start(AnalysisRequest)`・`stop()`、2つのリースとジョブ
  - シグナル: `plyStarted`・`plyAnalyzed`・`finished`・`error`
  - `KifuAnalysisController` には、ダイアログと表示への反映だけを残す（Deps は8欄前後になる）。
- エンジンに送る局面は、いつも `RecordLine::usiPosition(ply)` で作る。詰み探索の手番合わせは `Position` で行い、`TsumePositionUtil` を消す。

**影響**: analysis 3（1つは新規）・headless 2・cli 1・ui 2。差し引き約120行減。テストは `tst_analysisflow`・`tst_tsume_search`・CLI のテスト。

### T3. 盤と指し手の部品の責務があいまい（game・core・board・ui・dialogs）

**問題**

- **`ShogiGameController`（game）**: CLAUDE.md の説明は「Game flow control」だが、対局の流れは `MatchCoordinator` が持つ。実際に持っているのは次の3つ。
  1. 盤の持ち主。`currentPlayer`・`setCurrentPlayer` は `ShogiBoard` へ転送するだけ（`shogigamecontroller.cpp:56-64`）。
  2. 合法手を確かめて指し、棋譜の行を作る（`applyMove`・`applyRecordedMove`・`isLegal`）。
  3. 盤のクリックの解釈（`prepareMove`、`:75-116`）。これは UI の入力処理である。
     - 表示の座標を読み、駒台の段の範囲 1〜7・3〜9 を直書きしている（`:83-85`）。「この対応はここだけで決める」と書かれた `BoardCoordinates`（`boardcoordinates.h:14-18`）の写しになっている。
     - 成りのダイアログを呼ぶ。
     - ビュー向けの `endDragSignal` を出す。
     - 呼び出し元は ui の `boardmovecontroller.cpp:50` の1か所だけ。
  - 同じ「クリックから、成り・不成を決めた手を作る」処理を、`TsumePlayDialog::submit`（`tsumeplaydialog.cpp:285-300`）も別に書いている。
  - このクラスを受け取る部品の多くは、盤か手番を取るためだけに使っている。例: `KifuFileController`・`HumanTurnPolicy`・`PositionEditSession`・`BoardSyncPresenter`・`AutomationContext`・`TsumePlayDialog`。
- **棋譜の1行の組み立てが2つ**: `KifLineBuilder::addMove`（`kiflinebuilder.cpp:17-46`）と `ShogiGameController::commitMove`（`shogigamecontroller.cpp:146-162`）が、同じ手順を別々に書いている（動かす駒を決める → 指す → `KifMoveText::format` → `RecordEntry::played`）。指した側の決め方も違う（片方は手番、もう片方は駒の色）。
- **局面編集の規則が core と board に分かれている**
  - ドラッグで動かす規則は board の `PositionEditRules::canMove` にある。
  - 右クリックの成・不成・先後の巡回、行き所のない段での自動の成り、駒台の増減、駒箱の判定、盤を空にする処理は、core の `ShogiBoard` にある（`shogiboard.cpp:114,169,189,217,227`）。
  - 二歩の判定が2つある（`positioneditrules.cpp:28-35` と `shogiboard.cpp:241-247`）。
  - `boardinteractioncontroller.cpp:260` は、`PositionEditRules` を通らずに盤を直接変える。
  - core が表示の座標（駒台の筋10・11、駒箱の筋12）で読み書きしている。

**書き直し案**

- `prepareMove`・`endDragSignal`・成りの選び方は board へ移し、詰将棋を解く画面と共有する。
- 盤と手番だけを使う部品は `ShogiBoard*` を受け取る。
- 残り（着手と `moveCommitted`）は、Controller でない名前にする（例: `GameBoard`）。CLAUDE.md の説明も直す。
- `RecordEntry` を指す前の局面と指し手から作る関数を kifu に1つ置き、`KifLineBuilder` と着手の両方で使う。
- 局面編集の操作は `PositionEditRules` へ移し、`Position` を変える純粋な関数にする。`ShogiBoard` には、差分を知らせる `replacePosition(next)` を置く（`applyUsiMove` の差分のループ `shogiboard.cpp:125-149` を一般にしたもの）。これで `ShogiBoard` は局面の包みと知らせだけになる。

**影響**: 約20ファイル、±250行。テストは `tst_shogiboard`（15か所）・`tst_gamestrategy` など。段階に分けられる（クリックの解釈 → 棋譜の行 → 局面編集）。

### T4. 対局情報の規則に持ち主がなく、対局者・持ち時間・日時の正本が2つある（kifu・app・ui）。F2・F3 の原因

**問題**

- **同じ規則を、文字列の比較で何度も書いている**
  - 「先手（または下手）の見出しか」の判定が4か所にある: `gameinfoeditmodel.cpp:12-20`、`csaexporter.cpp:46-61`、`kifudocument.cpp:152-158`、`kifuexportmetadata.cpp:62-75`。
  - 語 `"平手"` の直書きが kifu に5か所ある。表 `Handicaps::kAll[0]` があるのに使っていない。
  - 番兵の `"無制限"` を `TimeControlText` の外の3か所で扱い、`gamesession.cpp:215` も `tr` で同じ分岐を書いている。
  - 平手か駒落ちかの判定が5通りある。
    - `SfenUtils::isHirateStart`
    - `kifexporter.cpp:217-218`
    - `Position == hirate()`
    - 文字列の完全一致（`usiposition.cpp:121`）
    - 名前の文字列を経由する `isHandicap(handicapLabel(sfen))`（3か所）
  - `KifuExportMetadataBuilder::handicapLabel`（`kifuexportmetadata.cpp:27-44`）は `Handicaps::bySfen` の書き直しである。
- **名前と中身が合わない**: `KifuExportMetadataBuilder` の7関数のうち、書き出し用は `resolvePlayerNames` だけ。残りは対局情報の規則で、`KifuDocument` から6回呼ばれている。
- **見出しの一覧が2つある**: `KifuPresentation::infoKey`（`kifupresentation.cpp:180-191`）が、見出しの一覧を `GameInfoKeys` と別に手で持っている。
- **表示言語がファイルに入る**: 既定の対局者名 `tr("先手")`（F2）と、CSA の `N+`・`N-` の補い（`kifuexportmetadata.cpp:13-19`）。`kifupresentation.h` の「ファイルには使わない」と食い違う。
- **正本が2つある**: `KifuDocument::beginMatch`（`kifudocument.cpp:143-171`）は、同じ事実を対局情報の文字列と `MatchPlayers`・`MatchTimeRecord` の両方に書く。読む側ごとに、どちらを使うかが違う。
  - CSA の `$TIME`・`$START_TIME` は記録から書く（F3）。
  - 既定のファイル名は記録の対局者から作る（U1）。
  - 盤の名前は、対局中だけ記録から取る（`playerinfopresenter.cpp:29-37`）。
- `KifuExportMetadata::gameEndDateTime` は書くだけで、読む所がない（`kifuexport.cpp:49`）。
- `$TIME` の書式が2つある（`csaexporter.cpp:115-127`、`csaformatter.cpp:84-87,119-128`）。

**書き直し案**

- kifu に対局情報の規則を1つ置く。`kifuexportmetadata` を `GameInfo` などに改名して中身を寄せる。
  - `playerKey(side, handicap)`・`isPlayerKey(key, side)`・`usesHandicapNames`
  - `handicapOf(Position)`: core の `Handicaps` に `const Handicap*` を返す関数を1つ置いて使う。
- `TimeControlText::format`・`parse` が「無制限」も扱う。
- `GameInfoKeys` を `Handicaps` と同じく `QT_TRANSLATE_NOOP` つきの表にし、`infoKey` はその表を引くだけにする。
- 既定の対局者名は空にする。表示の補いは Presenter に任せる。
- 書き出しとファイル名の正本は、対局情報にする。`KifuExportMetadata` から時刻・持ち時間・対局者の欄を外し、CSA も対局情報から書く。記録に残すのは、対局情報に無い事実（エンジンか人間か）だけにする。（時刻・持ち時間の欄と CSA の書き方は F3、ファイル名は U1 で済（f4079f7f）。残るのは `players` の欄）
  - 先後で違う持ち時間も、対局情報の文字列から `$TIME+`・`$TIME-` に戻せる（`CsaFormatter::convertToCsaTimeLines` が `TimeControlText::parse` の `perSide` を見る。`csaformatter.cpp:117-125`）。
- **仕様: 決定**
  - 途中から続けた対局では、開始日時・持ち時間を書かない。対局情報は元の棋譜のまま残し、続けた対局の対局者・開始日時・持ち時間は分岐の最初の手のメモに書く、という今の作り（`kifudocument.cpp:150`、`gamesession.cpp:208-217`）に CSA も合わせる。`$START_TIME` と `$TIME` を対局の記録から足すのをやめる（`csaexporter.cpp:88-93,110-127`）。
  - 既定のファイル名は対局情報の対局者名から作る（U1）。

**影響**: 約15ファイル、差し引き約50行減。テストは `tst_csaconverter`・`tst_kifudocument`・`tst_gameinfoeditmodel`。

### T5. 定跡ウィンドウが dialogs にあり、9つの責務を持つ（dialogs・ui・kifu・widgets・models）

**問題**

- `JosekiWindow`（911行。src で最大）は、ダイアログではなくドックのパネル。1つのクラスで次を持つ。
  1. ツールバーと配置（`josekiwindow.cpp:211-399`）
  2. 文字の大きさ（`:401-442`）
  3. 自分の設定と、セッションの設定の保存（`:448-480`）
  4. ボタンの有効・無効（`:502-521`）
  5. 行の操作（`:594-678`）
  6. 表と、導いた予想応手の計算（`:749-795`）
  7. 状態表示と案内文（`:797-851`）
  8. 局面の要約（`:79-96,853-873`）
  9. ドロップの振り分けとドックを閉じるときの確認（`:148-205`）
- dialogs から出られないのは、ウィンドウが `JosekiFileController`・`JosekiMergeController`・`JosekiEditController`（`:124-126`）と、ダイアログ（`JosekiExpandDialog`・`JosekiImportDialog`）を自分で作るため。Controller が dialogs にあり、もう1つの `JosekiWindowController` は ui/controllers にある。
- 「現在の棋譜から」の手順を4段で中継している。
  - `JosekiMergeController::kifuDataRequested`
  - → `JosekiWindow::requestKifuDataForMerge`
  - → `JosekiWindowController::onRequestKifuDataForMerge`
  - → `JosekiWindow::openMergeForLine`（呼び出しは1か所）
  - → `m_merge->openForLine`
  - 手順の持ち主 `navState` はコントローラにあるので、中継しなくてよい。
- 前回 R2 の案「前回のファイル・履歴の保存はセッションへ」が途中で止まっている。セッションが持つ履歴・前回のファイル・自動読込を、ウィンドウが `JosekiSettings` から読み書きしている（`:459-476`）。自動読込は2回書く（`:687`・`:475`）。
- `josekiWindowSize` の保存（`:451,473`）は、`DockRegistry` によるドックの大きさの保存と二重になっている。
- 同じ計算と判定を2か所でしている。
  - 指した後の局面: ウィンドウ（`:783`）とモデル（`josekitablemodel.cpp:57`）
  - 人間が指せるか: ウィンドウ（`josekiwindow.cpp:597-601`）とコントローラ（`josekiwindowcontroller.cpp:163-166`）で判定し、値の写しも2つある
- `JosekiRegistrationController`（ui）が、表記のためだけに dialogs の `JosekiTableModel::moveText` を include している（`josekiregistrationcontroller.cpp:8,51`）。
- ドックへの結び付けが2か所ある（`mainwindowcomposer_create.cpp:375-376`）。

**書き直し案**（どれも層の規則で許される）

1. `JosekiTableModel` を models へ、`JosekiTableView` を widgets へ移す。兄弟の `KifuAnalysisListModel`・`PvTableView` と同じ置き場になる。
2. `JosekiWindow` を widgets へ移し、意図をシグナルで出すだけのパネルにする。ほかのドックの10パネルと同じ形になる。
3. File・Edit・Merge の3つのコントローラを ui/controllers へ移し、`JosekiWindowController` が持つ。閉じるときの確認・ドロップの振り分け・展開のダイアログもこちらが受け持つ。手順は `navState` から直接渡す。
4. 履歴・前回のファイル・自動読込の保存を `JosekiBookSession` に移す。`josekiWindowSize` を消す。
5. 導いた予想応手はモデルかセッションの API で作る。人間が指せるかの判定は1か所にする。

**影響**: ウィンドウは600行台になる。CMake・CLAUDE.md の更新が要る。`tst_josekiwindow` は friend 経由のアクセスが約140か所ある。クラス名を変えなければ、翻訳の文脈は変わらない。

### T6. 盤の矢印を出すかの正本が3通りあり、対局の矢印だけプロトコルの状態から文字列で作る（widgets・ui・engine・app）。U2 の原因

**問題**

| 矢印 | 正本 | 読み方 | 保存 |
|---|---|---|---|
| 対局（手番側・先読み側） | 設定ファイル。`ThinkingPanel` のチェックが書く（`thinkingpanel.cpp:113-124`） | `CandidateArrowPresenter` が設定を直接読む（`candidatearrowpresenter.cpp:140,147`）。描き直しはシグナルで頼む（`mainwindowcomposer_wire.cpp:185`） | する |
| 検討 | `ConsiderationPanel` のメンバ `ConsiderationSettings::showArrows` | presenter がウィジェットのメンバへの生ポインタを持つ（`mainwindowcomposer_create.cpp:320`、`candidatearrowpresenter.cpp:177`） | しない |
| 定跡 | checkable な `QAction`（前回 R2 で正本にした） | `QAction` を読む | する |

- 矢印の中身も2系統ある。
  - 検討の矢印: 思考の表の行の `PvText::firstMove()` から作る（`:182`）。
  - 対局の矢印: `UsiProtocolHandler::SearchCandidate`（`usiprotocolhandler.h:55`、`.cpp:148-196,347-375`）から作る。中身は `baseSfen`・`move`・`predictedMove` の文字列で、presenter が SFEN を `section` で切って読み直している（`candidatearrowpresenter.cpp:144`）。
  - プロトコルの状態機械が、矢印のためだけの表示の状態を持っている。

**書き直し案**

- 4つの切り替え（手番側・先読み側・検討・定跡）を、すべて checkable な `QAction` にする。持ち主は ui の1か所にする。保存する項目は `Setting<bool>` から戻す。
- パネルはチェックを `QAction` に結ぶだけにする。presenter は `QAction` だけを読む。`ConsiderationSettings::showArrows` は消す。
- 対局の矢印も、思考の表の行（`ThinkingInfoFormatter` の出力）から作る。`SearchCandidate`・`Usi::searchCandidate`・`searchCandidateChanged` は消す。

**影響**: widgets 2・ui 1・app 2・engine 3・settings 1。差し引き約60行減。

### T7. services に GUI 専用の外観・駒音があり、外観の状態の持ち主が2つある（services・style・dialogs）

**問題**

- services は CLAUDE.md で「Cross-cutting services (settings, time keeping)」とされ、層の下の方（kifu より下）にある。しかし、GUI だけが使う部品が17ファイル・1,376行ある（services 全体の36%）。kifu〜navigation からの呼び出しは0。
  - 外観: `pieceimageprovider`（QIcon）・`boardappearance`・`appearancestate`・`boardappearancecatalog`・`boardcolorpresets`・`piecestylecatalog`
  - 駒音: `piecesound*`（QtMultimedia）
  - time keeping は services に無い。CLAUDE.md の説明も古い。
- 外観の状態の持ち主が2つある。
  - 駒のセットは `PieceImageProvider` が持つ。同じクラスがアイコンのキャッシュも兼ねる。
  - 配色と見た目は `BoardAppearance` が持つ。
  - 2つを `AppearanceState`（`appearancestate.cpp:8-22`）が束ねる。
  - そのため外観ダイアログは、`setStyle` を直接呼ぶ経路（`boardcolordialog.cpp:648`）と、`AppearanceState::apply` の経路（`:178,233,331,712`）の両方で書く。
- プリセットの表が3つあり、同じ外観が重なっている。
  - 表: `BoardColorPresets::themes()`・`palettes()`、`BoardAppearanceCatalog::combinations()`
  - 重なり: テーマ「榧と畳」と、組み合わせ「標準・榧と畳」は同じ外観になる。
  - ダイアログにも、配色コンボ・「外観」タブ・「おすすめの組み合わせ」の3つの入口がある。

**書き直し案**

- 外観の一群を style へ移す。style はウィジェットの見た目の層で、services を include してよい。
  - `BoardAppearance` が駒のセット・配色・見た目の3つを持つ（`AppearanceState::current`・`apply` を寄せる）。
  - `PieceImageProvider` は、アイコンの解決とキャッシュだけにする。
- 駒音は ui か app へ移す。
- **仕様: 決定**。外観タブのテーマのコンボ（`boardThemeCombo`、3種）はやめ、「おすすめの組み合わせ」（24種）に1つにする。
  - テーマ「榧と畳」は組み合わせ「標準・榧と畳」と同じなので足さない。「墨と榧」「琥珀」は組み合わせに足して26種にする。部品ごとの見本（とくに対局者情報のカードの色）に同じ色が無ければ、見本も足す。
  - `BoardColorPresets::themes()` と、テーマの選択を色から戻す処理（`BoardColorDialog::syncThemeSelection`、102行）を消す。
  - 設定には色の値を保存しているので、テーマを選んでいた人の外観は変わらない。
  - ガイドが説明しているのは「おすすめの組み合わせ」だけ。直すのはホームの「おすすめの組み合わせも24種類」（`docs/index.html:177` と en・zh-cn・zh-tw の同じ行。見本を足したら部品ごとの種類の数も）、外観タブが写った画像（あれば撮り直す）、`boardThemeCombo` を使うテスト3か所（`tst_board_image_renderer.cpp:175,202`、`tests/gui/tst_gui_functional.cpp:1298,1338`、`mcp/tests/test_feature_coverage.py:107`）。
- 外観ダイアログ（`BoardColorDialog`、739行）は、約170行ある見本の描画を別ファイルに出し、`BoardAppearanceDialog` に改名する（翻訳の文脈が変わるので、tr 40件を旧い訳から埋め直す）。

**影響**: 移動17ファイル、include の修正15か所前後。プリセットの統合を除けば動作は変わらない。

### T8. 棋譜の保存の経路と保存形式の表が分かれている（kifu・dialogs・automation・cli・mcp）

**問題**

- `KifuIoService::writeKifuFile` の呼び出し元は `KifuSaveFile::write`（`kifusavefile.cpp:83`）だけ。
  - Shift_JIS への変換を2回行っている（`kifuioservice.cpp:58`・`:91`）。そのため `:93-96` のエラーには届かない。
  - 書き出しは保存先の文字コードを知らず、1行目の `encoding=UTF-8` を文字列置換で書き換えている（`kifusavefile.cpp:26-33`）。
- 保存形式の対応が4か所に分かれている。
  - 拡張子と形式: `kifuformat.cpp:21-31`
  - 拡張子と Shift_JIS: `kifusavefile.cpp:56-60`
  - 保存ダイアログの絞り込み: 手書きの文字列（`kifusavedialogs.cpp:58-67`）
  - 選んだ絞り込みからの拡張子: 正規表現で読み戻す（`kifusavefile.cpp:45-52`）
  - 開く側は前回の監査で表から作るようになったが、保存側は残っている。
- 保存できる形式の一覧（`KifuFormats` は一覧を返す関数を持たない）も、手で4か所に書いている。
  - `clicommandspecs.cpp:68-72`
  - `automationmethodspecs.cpp:55-57`
  - `automationcommands_kifu.cpp:124,181-185`（後者は表で検査済みなので届かないエラー文）
  - MCP の `handlers_cli.py:56-57`
- 定跡ファイルの拡張子と絞り込みも3か所にある（`josekifilecontroller.cpp:19,24-25`、`josekibookmergedialog.cpp:102`、`josekiwindow.cpp:185`）。

**書き直し案**

- `KifuIoService` を `KifuSaveFile` に統合する。
- 形式の表に、文字コードと保存の見出しを持たせる。保存の絞り込みも表から作る（`KifuFileFilter::forSaving`）。書き出しの文脈には文字コードを渡す。
- `KifuFormats::savableNames()` を足し、CLI・自動化 API の表と文はそれから作る。MCP の対応表は `specs.json` の選択肢から作る。
- 定跡ファイルの絞り込みは、`JosekiBookFile` の1か所にまとめる。

**影響**: 約12ファイル、差し引き約80行減。テストは `tst_kifusavedialogs`（27か所）。

---

## 3. 中くらいの候補

| # | 候補 | 問題 | 直し方の案 |
|---|---|---|---|
| M1 | 知らせの出し方（ui・app） | `NotificationPresenter` は「知らせの表示の唯一の持ち主」と書かれている（`notificationpresenter.h:12`）。実際には、ui・app の16か所が `QMessageBox::warning`・`information`・`critical` を直接出している。例: `nyugyokudeclarationhandler.cpp:24,30,37,59`、`kifufilecontroller.cpp:220,234`、`josekiwindowcontroller.cpp:126,144`、`docklayoutmanager.cpp:78-127`。これらは自動化 API の `ScopedCapture`（記録だけしてダイアログを出さない）と PlainText の指定を通らない | 知らせは `NotificationPresenter` に集める。集めないなら、説明を実態に合わせる |
| M2 | ダイアログの持ち主（ui） | 開く場所と結果の受け手が、操作ごとに違う。対局開始は `GameSession` が開いて自分で受ける。通信対局は `DialogLauncher` が開き、`CsaGameSession` が受ける（`dialoglauncher.cpp:100-116`）。棋譜貼り付けは `KifuFileController` が開いて受ける。局面集ビューアは `DialogLauncher` が開き、シグナルを転送し、合成ルートが `KifuFileController` につなぐ（`_wire.cpp:119-120`）。`displayMenuWindow` はダイアログではなくドックを切り替える（`dialoglauncher.cpp:87`） | 「結果を使う部品が自分でダイアログを開く」にそろえる。通信対局は `CsaGameSession`、局面集は `KifuFileController` へ移す。メニューウィンドウの切り替えはドックの toggleViewAction に任せる |
| M3 | 「新規」の流れ（app） | 処理が3クラスに分かれている。`MainWindowActions::resetToInitialState` が未保存を確認し、`GameSession::resetToInitialState` が検討・通信対局・全エンジン・司令塔・連続対局を止め（`gamesession.cpp:296-309`）、`SessionResetCoordinator` が棋譜を作り直す。`GameSession` の Deps の `analysis`・`engines` は、この関数でしか使わない | 止める手順は `MainWindowActions` へ移す。`GameSession` には対局の後始末（`abandonGame()`）だけを残す |
| M4 | メニュー項目の接続（app・ui） | 接続する場所が5か所・2方式ある（`MenuActionsWiring` の52本、`PositionEditSession`、`LanguageController`、`DockLayoutManager`、`JosekiWindowController`）。局面編集のメニューは、「編集終了」だけが `MenuActionsWiring`（`menuactionswiring.cpp:57`）で、残りの4項目は `PositionEditSession` がつなぐ。チェックできる項目の設定の読み書きも4通りある（合法手の表示・駒音・ツールバー・定跡の矢印） | triggered・toggled の接続はすべて `MenuActionsWiring` で行い、チェックの初期値は設定を持つ部品が入れる |
| M5 | エンジン一覧の検索の二重（engine・services） | `EngineRegistrationHandler` が自分の `QList<EngineEntry>` を持ち、`RegisteredEngines` と同じ処理を書き直している。`normalizeEnginePath` は `canonicalPath` と同じ。`isDuplicatePath` は `indexOfPath` と同じ。`uniqueRegisteredName` は `indexOfName` を使わない（`engineregistrationhandler.cpp:21-29,66-75,186-197`） | ハンドラは `RegisteredEngines` を持ち、その検索を使う |
| M6 | エンジン設定ダイアログを開くスロット（dialogs・ui） | `ChangeEngineSettingsDialog dialog(engine, this); dialog.exec();` という同じ処理が7か所にある。`EngineSelector` は widgets にあってダイアログを開けないため | `ChangeEngineSettingsDialog` に static な開く関数を1つ置く |
| M7 | 局面送りのボタン（widgets・navigation） | 6つのボタンを `RecordPane`（`recordpane.cpp:198-227`）と `EvaluationChartWidget`（`evaluationchartwidget.cpp:164-198`）が別々に作る。文言は `KifuNavigationController` の `QAction`（`kifunavigationcontroller.cpp:20-27`）と合わせて3か所にある | widgets に共通の部品（`StepButtons::create(parent, Qt::Orientation)`）を置く。文言とツールチップは `QAction` から取る |
| M8 | 列幅の保存（widgets・dialogs・services） | 利用者が決めた列幅の保存と戻し方が5通りある。<br>- `ThinkingTableView`: 保存した幅を最小値として扱う（U5）<br>- `EngineInfoWidget`: 1度保存したら全部の列を固定にする<br>- `AnalysisResultsPanel`: 列ごとに持ち、0は自動<br>- `GameInfoPane`: 1列だけ<br>- `JosekiWindow`: 閉じるときに保存する<br>保存の形も4通りある。`analysissettings.h:84-89` には、規則で禁じた getter・setter の組が残っている | `TableHeaderView` に付ける共通の部品（`ColumnWidthMemory`）にする。規則は「0は自動、正の値は利用者が決めた幅」の1つにする |
| M9 | マスの大きさ（views・dialogs・services・cli） | 範囲 20〜150 は views にしかない（`boardgeometry.h:31-32`）。そのため読む側の7か所で範囲に収め、CLI の表も直書きしている（`clicommandspecs.cpp:162`）。`ShogiView` のコンストラクタが本体の盤の設定を読むので、盤ビューアと CLI の盤面画像も同じ値を読む（U4） | 範囲を下の層に移し、`BoundedSetting` にする。設定はコンストラクタで読まず、合成ルートで読み書きする |
| M10 | 検討タブの条件（widgets・services・ui） | 値の型 `ConsiderationSettings` は widgets にある。同じ種類の `EvaluationChartSettings` は services にあり、`StructSetting` で保存している。パネルは読み込み・表示・保存を手で書き（`considerationpanel.cpp:55-126`）、エンジンの選択も `EngineSelector::saveSelection()` を使わずに書く。宣言の範囲と既定値を手で写している（`:268,289`） | 値の型を services へ移し、`StructSetting` で保存する。エンジンの選択は `EngineSelector` に任せる |
| M11 | ShogiView の残り（views・board） | 駒箱の先後（局面編集の状態）をビューが持つ。`ShogiViewInteraction` とカードのボタンの2か所に写しがあり、編集の処理がビューから読む（`positioneditsession.cpp:113`）。ハイライトは `highlighting()` で直接触り、矢印は転送だけの `ShogiView::setArrows` を通す。`Arrow` 型は `ShogiView` に定義されているが、実際に持つのは `ShogiViewHighlighting` | 駒箱の先後は `PositionEditSession` が持ち、ビューは表示するだけにする。矢印は `highlighting()` に揃え、`Arrow` 型をそちらへ移す |
| M12 | CsaClient（network） | ヘッダーが530行ある。ソケット、状態の enum 3つ、`GameSummary`（関数約20）、Game_Summary の段の状態機械（`csaclient_parser.cpp:118-273`）、対局中の手番、表示文 `gameResultText` を1クラスに持つ。core の型と名前が重なる `CsaClient::TimeControl` もある。構文解析を公開の口で試せないため、テストが非公開のメンバに46回直接触れる。1つのクラスを `_parser`・`_commands` に分けた形は、CLAUDE.md の「機械的に分けず、責務を切り出す」に反する | 値型 `CsaGameSummary` と、純粋な読み取り器 `CsaGameSummaryReader`（1行ずつ渡し、結果を値で返す）を別ファイルにする。表示文は ui に置く |
| M13 | CSA のログインの規則（dialogs・network） | CSA プロトコル 3.1 の ID・パスワードの規則と、ゲーム名の形式を、`CsaGameDialog` だけが正規表現で持つ（`csagamedialog.cpp:17-22,131-164`）。単体テストはない。対局開始の時間の規則は、game の `TimeControlValidator` に出してある | network に検査関数（問題の種類を enum で返す）を置き、ダイアログは文に直すだけにする |
| M14 | 評価値の向き（models・core・cli） | CLAUDE.md は「向きの直しは `blackView()` だけ」とするが、CLI に手書きの反転がある（`clicommands_engine.cpp:115-121`）。`EvaluationSeries::blackView` は core の型だけで書ける純粋な関数なのに models にあり、CLI から使えなかった | core の `UsiScore` に `blackView(Turn)` を置き、`EvaluationSeries`・`KifuAnalysisRowPresenter`・CLI がそれを呼ぶ |
| M15 | position の文字列の読み取り（core・kifu） | 前回 R4 で `UsiPosition::parse` の1つにしたが、まだ3つ残っている。<br>- `SfenUtils::normalizePositionLikeSfen`・`normalizeStart`（呼び出し7）<br>- `UsiToSfenConverter::parseUsiPositionString`（約60行。仮の SFEN を組み立てて語を判定する。`usitosfenconverter.cpp:66,76`）<br>- `KifuLoader::loadPositionFromSfen`（局面を確かめずに置き換える。`kifuloader.cpp:88-99`） | `UsiPosition` に、語を分ける関数か寛容な読み方を足して共有する |
| M16 | ドックのパネルのボタン（widgets） | ツールバーのボタンの大きさが、パネルごとに5通りある（36×24、28×24、最小32×28、幅40・高さ28以上、文字に合わせる）。検討タブだけが A-/A+ の大きさを自分で決めている。CLAUDE.md の「A-/A+ の見た目はパネルで決めない」に反する | `FontSizeControl` が大きさを決める。アイコン・文字のボタンは共通の関数1つで作る |
| M17 | 対局開始ダイアログの保存形式（dialogs・services） | 持ち時間の「時・分・秒 ↔ ミリ秒」の変換（`startgamedialog.cpp:33-59`）と、以前の版の開始局面の番号の表 `legacyStartingPosition`（`:61-72,379-397`）をダイアログが持つ。CLAUDE.md の規則では、以前の版の値の引き継ぎは設定の .cpp に関数で書く | gamesettings.cpp に移す |
| M18 | コメント欄の確認（widgets・ui） | 確認を出して破棄を決める処理を、ウィジェットの `CommentEditorPanel`（`commenteditorpanel.cpp:161-197`）が持つ。`CommentCoordinator` の3つの関数は、それを転送するだけ（`commentcoordinator.cpp:38-51`）。ほかの未保存の確認は dialogs の `UnsavedChangesPrompt` | 確認は `CommentCoordinator` が `UnsavedChangesPrompt` で出し、パネルは状態を返すだけにする |

---

## 4. 置き場所と名前（移動・改名だけで済み、ロジックは変わらない）

| # | 対象 | 問題 | 移す先・名前 |
|---|---|---|---|
| P1 | ui/coordinators | 7クラスのうち、複数の部品にまたがる流れは `CsaGameSession`・`CommentCoordinator` だけ。<br>- `KifuFileController`・`DialogLauncher`: 利用者の操作の処理<br>- `UiStatePolicyManager`: 動作状態をメニュー・ボタンの有効/無効に映すもの<br>- `DockRegistry`・`DockLayoutManager`: ドックの部品 | `KifuFileController`・`DialogLauncher` → controllers。`UiStatePolicyManager` と `HumanTurnPolicy`（ui/controllers にある規則のクラス）→ ui/policy。ドックの2つ → ui/docks |
| P2 | dialogs/menuwindow | ドックのパネル。使うのは `MenuWindowController` だけで、依存は services・style・widgets だけ。独立ウィンドウだった頃の名残（`Qt::Window`・`closeEvent`・`menuWindowSize`・受け手のないシグナル `actionTriggered`）がある（U3） | widgets へ移し、名残を消す |
| P3 | navigation | 中身は `KifuNavigationController` の1クラスだけ。QtWidgets の `QAbstractButton` を使う（`kifunavigationcontroller.cpp:9,42`）。CLAUDE.md では navigation は「QWidget に依存する style」より下の層なので、層の決まりと合わない。include しているのは ui・app・automation だけ | ui/controllers へ移し、navigation の層をなくす（`tst_layer_dependencies` の表も直す） |
| P4 | automation/boardimagerenderer | 呼び出しは CLI とテストだけで、自動化 API からは0。CLAUDE.md と developer-guide は「CLI・自動化 API と共有」と書いているが、実態と違う。cli は views を禁じているのに、automation 経由で `ShogiView`（QWidget）に依存し、検査をすり抜けている。CLI は、静的関数3つのためだけに `automationserver.h`（QLocalServer）も include している（`clicommands_basic.cpp:73-76`） | `BoardImageRenderer` は cli へ移し、層の規則に「cli は盤面描画のためだけに views を使う」と例外を書く。ソケットの場所・エンドポイントファイル・`kMaxMessageBytes` は headless の `AutomationEndpoint` へ移す |
| P5 | game/enginelifecyclemanager | 依存は `Usi` だけ。analysis・ui・app からも使う、エンジンの枠の貸し出し部品。起動はもう行わないので、名前の「Lifecycle」は実態と合わない。中の `Seat` も `MatchEngineSeat` と紛らわしい | engine へ移し、`EngineSlots` などに改名する |
| P6 | analysis/sfenvalidationservice・positioncollectionfile | どちらも core だけに依存し、解析の要素はない。`SfenValidationService` は局面を `normalizedSfen` の文字列で返し、呼び出し側が `Position::fromSfen` で読み直している（`sfenvalidationservice.cpp:132`、`cliargs.cpp:26`） | 局面を値で返す検査は core か kifu に置き、JSON にする部分は headless へ移す |
| P7 | engine/engineoptiondescriptions | エンジン設定画面の説明文と分類（cpp は373行）。使うのは dialogs の `enginesettingsoptionhandler` だけ | dialogs へ移す |
| P8 | core/kifmovetext・sfendiffutils | `KifMoveText`（KIF の表記）は core からの利用が0で、使う11ファイルはすべて kifu 以上。`SfenDiffUtils` の利用は dialogs の `pvline.cpp` の1か所だけ | `KifMoveText` は kifu/formats（`KifMoveGrammar` の隣）へ。`SfenDiffUtils` は使う側へ |
| P9 | common/boardcolors.h・boardvisuals.h・tsumeevaluation.h | `boardcolors.h` は設定のキー、配色ダイアログの表示名、ボタンの objectName まで持つ（`boardcolors.h:36-41,78-114`）。最も下の層での使い手は services。`tsumeevaluation.h` は詰みの判定結果の値で、使うのは engine と analysis。engine の `MateAnswer` と内容が近い | boardcolors・boardvisuals は T7 で外観と一緒に style へ。tsumeevaluation は engine へ移し、`MateAnswer` と合わせられるか見る |
| P10 | game の小さなヘッダー | 責務ではなく、使う側の都合で分かれている。<br>- `matchtypes.h`: 説明は「時間パラメータ等の軽量ヘッダ」だが、時間の型を持たず、include は2つ<br>- `interruptedgamestate.h`・`matchsessionstate.h`: `matchcoordinator.h` からしか include されない<br>- `matchtimesettings.h`・`timecontrolutil`: `GameSession` のためだけにある<br>- `startgamedatabridge.h`: 「bridge」が中の値の型 `PlayerChoice`・`StartGameDialogData` を表していない | 司令塔専用の3つは `matchcoordinator.h` へまとめる。`timecontrolutil` は T1 で消える。`startgamedatabridge` は中身に合う名前にする |
| P11 | automation のファイル分け | メソッドの名前空間と合っていない。<br>- `_ui.cpp` に `board.click`・`ui.interaction`・`widget.text` がある<br>- `widget.*` が2ファイルに分かれている<br>- `_navigation.cpp` の中身は `menu.*` と `branch.click`<br>- 基盤のクラス `AutomationDeferredCall` が commands のヘッダーにある<br>CLAUDE.md は7ファイルのうち4つしか挙げていない | 名前空間の接頭辞ごとに分け直す。`AutomationDeferredCall` は automationinteractions へ移す |
| P12 | 名前 | `KifReader` は全形式の文字コードの判別に使われている（5ファイル）。`JkfMoveParser` は書き出し側の関数（`presetFromSfen`・`modifierToRelative`）も持ち、`jkftosfenconverter.cpp:122` は `presetToSfen` を書き直している。`board/BoardImageExporter` は盤に限らない画像の書き出し（評価値グラフにも使う）で、ui の `ScreenshotService` と同じく `grab()` して書く | 中身に合う名前にし、画像の書き出しは1つにする |

---

## 5. 名残・小さな修正

前回の監査で「済」とした項目のうち、残っていたもの:
- `KifuBranchTree::setComment`（`kifubranchtree.cpp:262`）は呼び出しが0。文書の版を上げずにコメントを書く2つ目の経路で、前回の4章に挙げたもの。
- `MatchTimeRecord::clearStart`（`matchtimerecord.h:26`）は呼び出しが0。前回は「既に消えていた」と記録されている。

呼び出しがない・テストからしか呼ばれないもの:
- `KifuExportMetadata::gameEndDateTime`（書くだけ）。**状態: 済（f4079f7f）**。F3 で消した
- `BoardMoveController::Deps::view`（Q_ASSERT でだけ必須とされ、使われていない。`boardmovecontroller.cpp:19`）
- `CsaGameSession::isActive()`・`coordinator()`
- `KifuBranchTree::hasBranch(node)`、`RecordLine::displayTexts()`
- 外からの呼び出しがない公開関数 `SfenReplay::applyLenient`
- `ButtonStyles::toggleButton`、`RecordPane::appearance()`
- テスト1つだけが使う `settingsservice.h`
- 使っていない include:
  - `gamesession.cpp:25` の `playerinfopresenter.h`
  - `kifurecordlistmodel.cpp`・`shogienginethinkingmodel.cpp` の `applicationfonts.h`（後者は `kifupresentation.h` も）

転送だけ・二重のもの:
- `AutomationContext::uiStatePolicy` は動作状態を転送するだけで、同じ値を指す `activity` の欄がある（`automationcontext.h:36,41`）。
- 自動化 API の `widget.showDock` と、`dock.configure` の `operation=show` は同じ操作。前提の検査だけが違う。MCP もツールを2つ公開している（`show_dock`・`configure_dock`）。
  - **仕様: 決定**。名前は残し、`widget.showDock` は `dock.configure` の show と同じ処理を呼ぶ。前提の検査は `dock.configure` の方（メインウィンドウがモーダルで塞がれていないか、ドックが有効か）にそろえる。
  - 残す理由: `show_dock` は公開しているツールで、MCP のテスト7ファイル、4言語の MCP ガイド、ガイドの画像を撮るスクリプト（`scripts/docs/shoot_tables.py`）が使っている。ドックを見せるだけなら引数1つの `show_dock` の方が選びやすい。
- キーのエスケープ（%25・%2F・%5C）が2か所にある（`enginesettingsconstants.h:35-46`、`docksettings.cpp:33-53`）。`SettingsCommon` に1つ置く。
- `EngineSettingsConstants` に、INI のキーと USI のオプションの型を表す語が同居している。キーは `EngineProfileStore` の中に閉じ、型の語は `engineoption.h` へ移す。
- `TimeControlValidator::hasTime` は `TimeControl::normalized` の規則の写し（`timecontrolvalidator.cpp:12-17`）。
- `TsumeshogiSettings::migrateCollectionEngineSelection` は、`EngineListSettings` のキーを書き換える処理なのに詰将棋の設定にあり、ダイアログのコンストラクタから呼ばれる。

置き場所が少しずれているもの:
- 時計の警告の閾値 `BoardChromeWidget::kWarn10Ms`・`kWarn5Ms` を使うのは `TimeDisplayPresenter` だけ。
- `EngineRegistrationDialog` がハンドラを `unique_ptr` と QObject の親の両方で持つ（`engineregistrationdialog.cpp:23`）。

古い説明・コメント:
- `menuactionswiring.h:26`: 2026-10-16 に削除した `DockUiCoordinator` を挙げている。
- `MenuWindowController` の説明（遅延生成・押されたボタンの知らせ）。
- `candidatearrowpresenter.h:39-40`: 説明が二重に書かれている。
- `shogiviewhighlighting.h:24`: 「値メンバ」とあるが、実際はポインタ。
- R5 の後に残った「手合割の名前を返す」説明7行（`kiftextparser.h:24,27`、`csatosfenconverter.h:23`、`usentosfenconverter.h:29`、`jkftosfenconverter.h:22,28`、`kifuloadparser.cpp:57`）。
- `enginelistsettings.cpp:2`・`analysissettings.h:7`・`tsumeshogisettings.h:7` の @brief。
- 合成ルート: コードのないコメント（`_wire.cpp:138`）、直後の行と合わないコメント（`_wire.cpp:110`）。
- CLAUDE.md:
  - services の説明「(settings, time keeping)」
  - `ShogiGameController` の説明「Game flow control」
  - automation のファイル一覧
  - `BoardImageRenderer` の説明「CLI の render-board も使う」
  - dialogs・widgets の節が `JosekiWindow`・`MenuWindow` を説明していない

公開の範囲:
- `KifuBranchNode::record()` が書き換えられる参照を公開している（`kifubranchnode.h:37`）。そのため、ツリーと文書以外の4か所（ツリーの組み立て・ライブ記録など）がコメントなどを直接書く。
- `EvaluationChartView` は、描画の状態を公開のメンバ変数で受け取る。

---

## 6. 責務が整理できていて、手を入れなくてよい部分

- **app**
  - MainWindow（45行、閉じるときの処理だけ）
  - 合成ルートの生成・注入・接続
  - `SessionResetCoordinator`（棋譜の作り直しだけ）
- **core**
  - 値の型 `Position`・`UsiPosition`・`UsiMove`・`PvText`・`UsiScore`
  - `PositionValidator` と `EngineMoveValidator::applyIfLegal`、fmv
  - `ShogiClock`・`JishogiCalculator`
- **kifu**
  - `KifuBranchTree`・`KifuBranchNode` の所有と型付きの知らせ、`KifuNavigationState`、`LiveGameSession`
  - `KifuDocument` の置き換えと未保存の判定
  - 読み込み: `KifLineBuilder`・`ParseReport`・`KifTextParser`・`KifMoveGrammar`・`Ki2MoveNotation`・`KifuLoadParser`・`KifuLoader`
  - 書き出し: `KifuExport` の表、`KifuNodeReplay`
  - 対応表: `TerminalTypes`・`TerminalCodes`・`UsenCodec`
  - 定跡: `JosekiBook`・`JosekiBookFile`・`JosekiBookSession` の `canEdit`・`JosekiLines`・`JosekiImport`・`JosekiMovePicker`
- **engine**: 層ははっきり分かれていて、転送だけの層はない。`Usi` は約10個の関数が転送だが、組み合わせの窓口として妥当。
  - `EngineProcessManager`: 行の入出力
  - `UsiProtocolHandler`: 状態機械と通番（T6 の `SearchCandidate` を除く）
  - `UsiEngineSession`: 起動と探索の切り替え
  - `UsiMatchHandler`: 対局と先読み
  - `Usi`: 組み合わせ
  - そのほか: `UsiLineParser`・`MateOracle`・`MateQueryLoop`・`UsiGoTimeCalculator`・`MatchTimeSource`
- **game**
  - `MatchCoordinator` の本体（着手の確定・終局・待った・中断と再開）
  - `MatchStrategyHost` と Strategy 3つ、`MatchEngineSeat`、`GameChannel`
  - `ConsecutiveGameSeries`・`SennichiteDetector`・`GameStartOptionsBuilder::plan`
- **analysis**
  - `AnalysisSession`
  - `TsumeCandidateJob`・`TsumeVerificationJob`・`PlyAnalysisCollector`
  - `TsumeCollectionSession`・`TsumeProgressStore`
  - tsume の21ファイルは前綴りで分かれているので、サブディレクトリにするかは任意。
- **network**: `CsaGameProgress`・`CsaMoveConverter`・`CsaGameChannel`、`CsaGameCoordinator` の `Phase` と裁定待ち。
- **headless**
  - `ParamSpec`・`ErrorKinds`/`Failure`・`OutputPath`・`RecordJson`
  - `TsumeVerificationRunner`・`MateSearchRunner`
- **ui**
  - 盤の入力の規則（`HumanTurnPolicy::boardInput()` の1か所）
  - presenters: PlayerInfo・BoardSync・KifuDisplay・EvaluationGraph・TimeDisplay・UsiModelBinding・KifuAnalysisRow。利用者の操作を扱わず、文書を書き換えない。
  - 薄いコントローラ: ConsiderationUi・TsumeSearch・PvClick・UsiCommand・ImageExport・SettingsReset
  - `KifuNavigationController` の中身（P3 の置き場所だけが問題）
- **widgets・views・models・style**
  - ShogiView の分け方（`BoardGeometry`・`BoardStaticLayer`・`ShogiViewInteraction`・ハイライト・カード）
  - `BranchTreeView`
  - models の行の値と `NotationTextCache`
  - `GameInfoEditModel` が持つのは履歴だけで、文書が正本になっている
  - 共通部品: `LogPanel`・`EditActions`・`EngineSelector`・`SettingsFormBinding`・`DialogFontScale`・`IncrementalTableView`
  - style 全体
  - 1つのダイアログだけが使う部品（`MenuButtonWidget`・`TsumePositionPreview`・`PlayerSetupWidget`・`TimeControlEditor`）は、層の順に合っているので widgets のままでよい。
- **dialogs**
  - 大きなダイアログ（詰将棋の生成・問題一覧・解く画面、持将棋、局面集）は、ドメインの処理を下の層に任せている。行数が多いのは画面の組み立てのため。
  - 盤の表示は `BoardViewerWidget` 1つを4つのダイアログで共有している。
  - 定跡の Edit・File・Merge の各コントローラの中身（置き場所だけが T5 の問題）
  - `PvLine`・`JosekiMoveInputWidget`・`BoardAppearancePreview` は、1つのダイアログの部品として dialogs に置いたままでよい。
  - dialogs から ui・app を include している箇所は0件。
- **services・automation・cli・mcp**
  - `setting.h` の宣言型と、大半の1行宣言
  - エンジンの設定の分担（保存形式 `EngineProfileStore`、一覧 `RegisteredEngines`、選択 `EngineListSettings`）
  - `NotationStyle`・`ApplicationTranslations`・`AppPaths`
  - `AutomationInteractions`・`AutomationWidgetAdapters`・`AutomationDispatcher`・`AutomationServer`・許可リスト
  - automationcommands は GUI のコントローラと headless を呼んで JSON にするだけで、処理を重複していない。`AutomationContext` は god object ではない（どの欄も1〜2ファイルからしか使われない）。
  - CLI のコマンド表・`CliCommandSpecs`・`CliStopWatcher`。コマンドは headless のランナーを呼ぶだけ（M14 を除く）。
  - MCP の `specs.py`・`app_client.py`・`paths.py`・`environment.py`・`jobs.py`。入力スキーマは `specs.json` から作っている（残る写しは T8 の形式の一覧と、出力スキーマの状態の列挙）。

---

## 7. 進め方

1. **1章の食い違い F1〜F4 を先に直す。** どれも小さく、それぞれテストを足せる。
   - F1: GUI の棋譜解析も `RecordLine::usiPosition(ply)` で送る（T2 までのつなぎ）。
   - F2: 既定の対局者名を空にする。
   - F3: CSA の `$TIME`・`$START_TIME` を対局情報から書き、対局の記録からは足さない。途中から続けた対局では書かない（T4 の一部。決めた仕様）。
   - F4: 通信対局の `moveHighlightRequested` を消す（T1 の一部）。
   - U1: 既定のファイル名を対局情報の対局者名から作る（決めた仕様）。
   - U2〜U5 は、実行で確かめてから扱う。
2. **4章（移動・改名）を早めにまとめて行う。** ロジックは変わらず、後の書き直しで差分が読みやすくなる。CMake・CLAUDE.md・`tst_layer_dependencies` の表を同じコミットで直す。翻訳の文脈が変わるクラス名の変更（P5 など）は、旧い文脈の訳から埋め直す。
3. **T1（通信対局の流れ）を最初の書き直しにする。** composition-root.md の唯一の「接続の順に頼る」例外を消せる。T1 の時計の設定と `sfenStart` の削除は、game の小さなヘッダーの整理（P10）と一緒に進める。
4. **T2（棋譜解析）と T3（盤と指し手）は独立に進められる。** T3 は段階に分ける（クリックの解釈 → 棋譜の行 → 局面編集）。
5. **T4（対局情報）は、決めた仕様に沿って進める。** 1で F3・U1 を直していれば、残りは規則の持ち主（`GameInfo`）と、書き出しの文脈から記録の欄を外すこと。
6. **T5（定跡ウィンドウ）は、定跡の機能を足す前に進める。** T6 の矢印の `QAction` と、P2 の MenuWindow の移動とあわせると、ドックのパネルの形が揃う。
7. **T7・T8 と3章・5章は、いつでも進められる。** T7 のテーマの廃止は見た目の入口が変わるので、ホームの表記・画像と同じコミットで直す。5章の `widget.showDock` は名前を残して中身だけ1つにする。
