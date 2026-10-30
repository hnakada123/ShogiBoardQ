# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ShogiBoardQ is a Japanese chess (Shogi) board application built with Qt6 and C++17. It supports game play, kifu (game record) management, USI engine integration for analysis, and CSA network play.

## Build Commands

```bash
# Configure and build
cmake -B build -S .
cmake --build build

# Or with ninja (faster)
cmake -B build -S . -G Ninja
ninja -C build

# Run the application
./build/ShogiBoardQ

# Command line tool used by the MCP server (no GUI)
./build/shogiboardq-cli version
# Parameters of every CLI command and automation method as JSON Schema
./build/shogiboardq-cli describe
# Automation socket, endpoint file, settings file, message limit and error kinds as JSON
./build/shogiboardq-cli environment

# Python MCP server tests (also registered in ctest as tst_mcp_python)
python3 -m pytest -q mcp/tests

# With static analysis
cmake -B build -S . -DENABLE_CLANG_TIDY=ON
cmake -B build -S . -DENABLE_CPPCHECK=ON

# Update translations (.ts files from source, then rebuild .qm)
cmake --build build --target update_translations
cmake --build build --target translations
```

## Architecture

### Source Organization (`src/`)

ヘッダーファイル（.h）は対応するソースファイル（.cpp）と同じディレクトリに配置されています。

- **app/**: アプリケーションエントリーポイントと委譲先サービス
  - `main.cpp` - Entry point
  - `mainwindow.cpp/.h/.ui` - Main window
  - `mainwindowcomposer.cpp/.h` - 合成ルート。起動時に全部品を依存順に作り、依存を注入し、シグナルをつなぐ（実装は `mainwindowcomposer_*.cpp` に段階ごとに分割）。設計: `docs/dev/composition-root.md`
  - `mainwindowcomponents.h` - 合成ルートが作る部品の一覧。動作状態は `src/common/appactivitystate.h` の `AppActivityState` に1つだけ置き（合成ルートが持ち、部品には `AppActivityState*` を渡す）、変えるのは `begin(動作, 持ち主)` が返す move-only の `ActivityLease` を持つ部品だけ（手放すと待機中に戻る。別の部品が別の動作を始めたら、前の持ち主に `revoked(owner)` で次のイベントループで知らせる）
  - `mainwindowactions.cpp/.h` - メニュー・盤・棋譜欄からの利用者操作の振り分け先。「新規」は未保存を確かめてから `startNewRecord()`（検討を止め、対局の後始末 `GameSession::abandonGame()`、エンジンを止め、平手の棋譜にする）
  - `menuactionswiring.cpp/.h` - メニュー・ツールバーの項目と処理する部品の接続。項目の triggered・toggled はすべてここでつなぎ、チェックできる項目（合法手の表示・ツールバー・駒音・ドックの固定・移動元）は設定を持つ部品の今の値から始める（保存は部品）。ここでつながないのは、排他の組（言語・指し手の表記。`LanguageController`）と、項目を作る部品のメニュー（保存したレイアウトの一覧。`DockLayoutManager::setSavedLayoutsMenu`）だけ
  - `gamesession.cpp/.h` - 対局の開始・終局・再開・後始末（「新規」の `abandonGame()`）と、対局の棋譜の受け手（`MatchRecordSink`）。ローカル対局と通信対局の棋譜を開始から終局・自動保存まで1本の流れで扱い（司令塔が通信対局の開始で `beginRecord`、着手と終局で `appendRecord`、終局で `finishRecord` を呼ぶ）、投了・中断・即指し・入玉宣言は `activeChannel()`（`GameChannel`）へ送る
  - `sessionresetcoordinator.cpp/.h` - 対局の開始前（`prepareGameStart`。連続対局の次局・通信対局の開始も）・新規・局面編集で棋譜を作り直す。評価値グラフとエンジンの表示（`EngineDisplayPresenter::clearAll()`）は文書の置き換え（`KifuDocument::replaced`）を受けて自分で消える

- **core/**: 純粋なゲームロジック（Qt GUI非依存）
  - `shogiposition.cpp/.h` - 局面の値 `Position`（盤81マス・持ち駒・手番・手数）。SFEN の読み書きと指し手の適用（`apply`）はここだけ。計算のための局面はこの値で持つ
  - `shogiboard.cpp/.h` - 表示・局面編集に使う盤。`Position` を1つ持ち（`position()`）、変更をシグナルで知らせる QObject（局面の包みと知らせだけ。局面を変える規則は持たず、着手と局面編集は次の局面を `Position` で作って `replacePosition(next)` で渡す。変わったマス・駒台・手番だけを知らせる）
  - `shogitypes.h`・`piecerules.h` - 手番と駒（`Piece` は種類＋後手なら16。並びは fmv と同じ）。行き所のない駒・成れる段・持ち駒の並びの規則
  - `positionvalidator.cpp/.h` - 局面の検査 `PositionValidator`（`check()`：駒の数・駒台の玉・玉の重複と有無・手番でない側への王手と、手番側の王手・合法手の数。`boardProblem()` は盤と持ち駒だけ）。validate-sfen（`SfenValidator`）・局面集・詰将棋問題一覧・対局の開始で同じ規則を使う。合法手だけを指すのは `EngineMoveValidator::applyIfLegal`。Hayanagi は詰み探索だけに使う
  - `sfenvalidator.cpp/.h` - SFEN の検査 `SfenValidator::validate`（手番・手数の欄と `Position::fromSfen`・`PositionValidator` の結果を値 `SfenValidation` にし、読めた局面 `position` も返す）と、合法手だけを指した後の SFEN `applyUsiMoves`。JSON にするのは headless の `SfenValidationJson`
  - `pvtext.cpp/.h` - 読み筋の値 `PvText`（`PvText::play(開始局面, USI の語, 至った手の移動先)` が局面の上で1回だけ指し、指せた手（指した側・動かした駒）と指せなかった語を持つ）。思考の表・棋譜解析・読み筋盤・詰将棋の正解手順で共通。表記は kifu の `KifuPresentation::pv`・`pvMove` が表示のたびに作る
  - `usiposition.cpp/.h` - エンジンに探索させる局面 `UsiPosition`（開始局面・指し手の列・直前の手）。kifu の `RecordLine` が作るので core に置く。position 文字列は `UsiPosition::commandFor(startSfen, moves)` だけで作る（`command()` と USI 形式の書き出し `UsiExporter` で共有）
  - `shogimove.cpp/.h`・`usimove.h` - Move representation
  - `enginemovevalidator.cpp/.h` - Move legality validation（`fmv::Converter` を境界に `fmvlegalcore` のビットボード合法手生成を使う）
  - `sfenreplay.cpp/.h` - 局面に USI の1手を指す（`applyLenient`・`positionAfter`。合法手かは確かめない）
  - `boardcoordinates.h` - 盤面表示の座標（駒台は筋10・11、駒箱は筋12）と駒・USI の指し手の対応（表示の座標の駒・駒台と駒箱の枚数の読み `pieceAt`・`isAvailable` もここだけ）
  - `handicaps.h` - 手合割の表（名前・JKF の preset・開始局面）と局面の手合割（`Handicaps::of(Position)`・`ofStart(開始局面の文字列)`。盤・持ち駒・手番で決め、手数は見ない）。平手かどうかは `Handicaps::isHirate` だけで判定する
  - `jishogicalculator.cpp/.h` - 持将棋の点数計算・判定
  - `matchplayers.h` - 対局者（先手・後手を人間とエンジンのどちらが指すかと名前。1人分は `seat(Turn)`）
  - `shogiclock.cpp/.h` - Game clock
  - `shoginotation.h` - 筋・段・駒の表記（漢字・全角・CSA）

- **game/**: ゲーム進行・対局管理（GUI に依存しない）
  - `matchcoordinator.cpp/.h` - 対局の司令塔（開始・着手・終局・待った・中断と再開）。手順は `record()`、対局者は `players()`。開始の条件 `MatchStartOptions`（`startoptions.h`。手順 `recordLine` の `currentPly` の局面から始め、時間の決まり `time` は `start()` が盤の手番で時計に入れる）。終局の行の後で棋譜の受け手の `finishRecord` を呼んでから待機に戻す。エンジンの手番で、定跡を使う対局者（`MatchStartOptions::engine(Turn).joseki`）なら `JosekiMovePicker` で定跡手を選び、`Usi::requestBookMove` で探索させずに最低1秒置いて指す（定跡は対局を始めたときに定跡ウィンドウで開いていた定跡の写し `MatchStartOptions::josekiBook`）
  - `humanvshumanstrategy` / `humanvsenginestrategy` / `enginevsenginestrategy` - 対局者の組ごとの進め方（`MatchStrategyHost` 経由）
  - `matchengineseat.cpp/.h` - 対局で指すエンジン1台 `MatchEngineSeat`（枠のリース・手番・持ち時間の出どころ・知らせの接続・終局の手順）。人間対エンジン・エンジン同士の Strategy（`GameModeStrategy::engineSeat(side)`）と通信対局（`CsaGameCoordinator`）が持つ。初期化・指し手・投了・入玉宣言勝ち・エラー（枠を取り上げられたときも）を自分のシグナルで知らせ、終局は `finish(GameOverResult)`（探索・先読みを止めて gameover と quit）で、ローカル対局・通信対局で同じ手順
  - `gamechannel.cpp/.h` - 対局相手の窓口 `GameChannel`: 投了・中断・即指し・入玉宣言をローカル対局と通信対局で同じ操作にする（ローカル対局は `LocalGameChannel`、通信対局は network の `CsaGameChannel`）
  - `matchautosave.h` - 終局した棋譜の自動保存の指定 `MatchAutoSave`（ローカル対局・通信対局で同じ）
  - `consecutivegameseries.cpp/.h` - エンジン同士の連続対局（局数・手番の入れ替え・次局の計画）
  - `gameboard.cpp/.h` - 対局の盤 `GameBoard`（盤を持ち、合法手を確かめて指し、指した手を棋譜の1行にする。着手の確定の知らせ `moveCommitted`）。手番は盤（`ShogiBoard::currentPlayer()`）が持つ。盤と手番だけを使う部品は `ShogiBoard` を受け取る
  - `matchrecordsink.h` - 対局の棋譜の受け手 `MatchRecordSink`（新しい棋譜 `beginRecord(MatchRecordStart)`・行の追記と取り消し・終局での確定 `finishRecord`）。ローカル対局と通信対局の司令塔が同じ受け手（`GameSession`）に書く（合成ルートが `setRecordSink()` で渡す）
  - `gamestartoptionsbuilder` - 対局開始ダイアログの値の解釈

- **kifu/**: 棋譜関連（GUI に依存しない）
  - `kifudocument.cpp/.h` - 開いている棋譜の文書 `KifuDocument`（分岐ツリー・対局情報・対局の記録（対局者・持ち時間・開始と終了の日時）・上書き保存先）。未保存かどうかは保存した時点との比較で求める（対局情報欄の編集は表に入れた時点で文書に入る。入力中のセル・コメント欄の未更新の本文は `Hooks`（`commitPendingEdits`・`hasPendingEdits`）で、保存・書き出し・対局の開始の前に確定し、未保存に数える）
  - `kifubranchtree.cpp/.h`・`kifubranchnode.cpp/.h` - 分岐ツリー `KifuBranchTree`（ルートを所有し、ノードは子を `std::unique_ptr` で所有する。木の形を変えられるのはツリーだけ）。手を足すのは `addRecord(parent, RecordEntry)` だけ。構造の変更は型付きのシグナル `nodeAppended(parentId, nodeId)`・`childrenReordered(parentId)`・`subtreeRemoved(parentId, nodeId)`・`reset()` で知らせ、`beginBatchUpdate()`〜`endBatchUpdate()` の間は遅らせて順に出す（作り直しを含めば `reset()` 1回）。テストで手を足す・探す補助は `tests/branchtree_test_util.h`（`TestTree`）
  - `kifuexport.cpp/.h` - 形式ごとの書き出しの表（`KifuExport::lines`。GUI の保存・コピーと CLI の変換で共通）
  - `gameinfo.cpp/.h` - 対局情報の規則 `GameInfo`（対局者の見出し `playerKey`・`playerSide`・`playerName`・下手上手と呼ぶか `usesHandicapNames`・手合割の値 `handicapLabel`・`isHandicap`）。見出しの語は `gameinfokeys.h`（画面で訳す見出しは `GameInfoKeys::kTranslated`。`KifuPresentation::infoKey` が引く）、持ち時間の値（「無制限」を含む）は `TimeControlText`。書き出しの文脈 `KifuExportMetadata` は対局情報と開始局面だけで、CSA も対局情報だけから書く
  - `recordline.h`・`recordentry.h` - 1本の手順と1手の記録。手順の型は `RecordLine` だけ（読み込みの本譜・変化・書き出し・表示中の手順・対局の手順。末尾に終局の行を持てる）。エンジンに送る局面は `usiPosition(ply)`（作れなければ nullopt）と、範囲の各局面を手順を1度だけたどって作る `usiPositions(from, to)`（たどれなければ局面だけ。棋譜解析と CLI の analyze-kifu で共通）
  - `moverecord.h` - 指す前の局面と指し手から棋譜の1行を作る `MoveRecord::play`（動かす駒を局面から決めて指し、表記は `KifMoveText::format`）。棋譜の読み込み（`KifLineBuilder::addMove`）と対局の着手（`GameBoard`）で共通
  - `kifuformat.cpp/.h` - 棋譜の形式（読み込み・保存・変換で共通の1つの enum）と判定。拡張子・形式・文字コード（`KifuEncoding`）・保存ダイアログの見出しは1つの表（`saveTargets()`・`savableNames()`・`readableSuffixes()`）で、保存・開くダイアログの絞り込み（widgets の `KifuFileFilter::forSaving`・`forOpening`）と CLI・自動化 API の形式の選択肢もこの表から作る
  - `kifuloadparser.cpp/.h` - 棋譜の読み込み口（ファイルの文字コードの判別と形式ごとの変換器への振り分け）
  - `kifparsetypes.h` - 読み込み結果（`KifParseResult`・形式ごとの `parse()` が返す `KifuParseOutcome`）と、読めない手の扱い（`ParseOptions`・`ParseReport`。ここだけで決める）
  - `kifusavefile.cpp/.h` - 棋譜の保存 `KifuSaveFile`（保存先の拡張子から形式と文字コードを決め、`write(path, generateLines)` で行を作らせて書く。行の先頭の文字コードの宣言は書き出しの文脈 `KifuExportMetadata::encoding` で書く。Shift_JIS で表せない文字があれば受け手に聞いて UTF-8 で作り直す。CLI の変換の出力は `writeLines`。既定のファイル名 `makeDefaultSaveFileName`）。保存で聞くダイアログは dialogs の `KifuSaveDialogs`
  - `kifuloader.cpp/.h` - 棋譜ファイル・貼り付けた棋譜・局面を読み（`KifuLoadParser` をワーカースレッドで）、`KifuDocument` を置き換える `KifuLoader`（合成ルートが1つ作り、全部の読み込みで使い回す）
  - `josekibook.cpp/.h` - 定跡のデータの値型 `JosekiBook`（局面のキー `JosekiBook::positionKey()`（手数を除いた SFEN）から局面の登録を引く表1つ。同じ局面を手数違いで別々に登録したファイルは手数ごとの登録のまま持ち、引くときは同じ手数の登録、無ければ最初の登録を使う。大きな定跡も読めるよう、指し手を16ビットの番号にした詰めた形で持ち、`movesAt()` で `JosekiMove` に戻す（予想応手は `std::optional`。無ければ nullopt でファイルには "none"。値を書き換えて予想応手を残すのは `updateMove(..., NextMoveUpdate::Keep)`）。最後の定跡手を消すと登録も消える。棋譜で指された手を数えるのは `addPlayedMove`。やねうら王が予想応手 "none" の定跡手の後に先読みに使う手（次の局面で出現頻度、次に評価値が最も高い手）は `leadingMove()` で、定跡ウィンドウの表が予想応手の列に灰色で出す）と、やねうら王定跡形式の読み書き `JosekiBookFile::load()`・`save()`（ワーカースレッドから呼べる）と、定跡ファイルの拡張子・ダイアログの絞り込み（`hasBookSuffix`・`withBookSuffix`・`fileFilter`）
  - `josekibooksession.cpp/.h` - 開いている定跡ファイル（`JosekiBook` の値・変更状態・非同期の読み書き・履歴と前回のファイル）。合成ルートが1つ作り（`MainWindowComponents::josekiBook`）、起動時に `restoreSaved()` で前回の履歴を戻す。履歴と前回のファイルは変わるたびに設定に保存し、自動読込の設定も読み書きする（`autoLoadEnabled`）。定跡ウィンドウはそれを表示し、操作は ui の `JosekiBookController` が受ける。定跡を変える関数は、変えられないとき（`canEdit()` が false。読み書き中と、前回のファイルを初めて使うときに読み込む `restoreSaved`〜`startDeferredLoad` の待ちの間）は何もせず false・0件を返す。どの登録も保存はしない（定跡ウィンドウの「保存」で保存する）。エンジンの読み筋の登録 `registerEnginePv`（2手目を予想応手。登録済みの手は評価値・深さ・予想応手だけ書き換える。評価値は `valueOf` で詰みを ±(32000−手数)）、複数の棋譜から集めた手の登録 `importMoves`、別の定跡ファイルを足す `startMergeBook`（ワーカーで `JosekiBook::mergeFrom`。どれも保存はしない）
  - `josekilines.cpp/.h` - 定跡と棋譜の手順 `JosekiLines`（`expand`: 表示中の局面から定跡手を表の順にたどって棋譜の変化の手順 `RecordLine` にする。手数・1局面の手の数・比率の下限で絞り、別の手順で同じ局面に着いたら止める。`firstDeviation`: 手順が定跡を外れた最初の手）
  - `josekiimport.cpp/.h` - 複数の棋譜ファイル・フォルダから定跡に登録する手を集める `JosekiImport`（`expandPaths`・`collect`。条件 `Options`（手数・先手/後手/勝った側・変化・最小出現回数・コメント）。1局で同じ局面と指し手は1回と数える。ワーカースレッドから呼べる）
  - `josekimovepicker.cpp/.h` - 対局で指す定跡手を選ぶ `JosekiMovePicker`（条件 `JosekiPlayOptions`（`core/josekiplayoptions.h`。意味はやねうら王の定跡オプションに合わせる。保存は `JosekiSettings::playOptions1/2`）に合い、局面で指せる手から、出現頻度に比例・評価値で選ぶ・先頭の手のどれかで選ぶ）
  - `kifupresentation.cpp/.h` - 画面に出す棋譜の表記 `KifuPresentation`（指し手・読み筋・終局語。ファイルやプロトコルには使わない）。棋譜の1行は値から作る（`entryLabel(RecordEntry, 指す前の局面)`。終局は `RecordEntry::terminal` から訳す。棋譜欄の手数と分岐の印は `numberedLabel`）。`RecordEntry::displayText` は棋譜の表記（KIF の指し手・終局語）で、表示の文字列を書き戻さない。日本語の指し手は `KifMoveText`、段の漢数字は `ShogiNotation`、英語表記で移動元を付けるかは KI2 と同じ判定（`Ki2MoveNotation::annotate()` の `Annotation::sources`。合法手で決めるのでピンされた駒は数えない）。`Notation`・`Options` は `NotationStyle` の別名で、`options`・`revision` は `NotationStyle` へ転送する（表記の設定を変えるのは `NotationStyle::configure`）
  - `positioncollectionfile.cpp/.h` - 局面集ファイル（1行1局面）の規則 `PositionCollectionFile`（`parseLine`・`parse`、読めた行 `Line`）。局面集ビューア（`SfenCollectionDialog`）と詰将棋問題一覧（`TsumeCollection::parse`）で共有する。空行・`#` の行は読み飛ばし、局面は SFEN の4語（`sfen` を前置可）か `startpos`（前に `position` 可）、その後は `moves <USI の手>` だけ。position コマンドの手は指した後の局面（`Line::position()`）、SFEN だけの行の手は参考手順（`referenceMoves()`）。局面は core の `SfenValidator` で確かめ、それ以外は読めない行
  - **formats/**: Format converters（テキストを読む。ファイルは `KifuLoadParser` が読んで渡す）
    - `csatosfenconverter`, `jkftosfenconverter`, etc. - 形式ごとに `parse(text, options)` を1つだけ持ち、`KifuParseOutcome` を返す（KIF・KI2 は `KifTextParser::parseKif`・`parseKi2`、BOD は `KifTextParser::parseBod`）
    - `kiftextparser.cpp/.h` - KIF・KI2 で共有する読み取り（行の分類・「変化：N手」の区切り・開始局面）。指し手の文法は `kifmovegrammar`（KIF の手は KI2 の手＋移動元）
    - `kiflinebuilder.cpp/.h` - 全形式で共通の手順の組み立て（`KifLineBuilder`。指しながら `RecordEntry` を作る。1行は対局の着手と同じ `MoveRecord::play`）
    - `*exporter` - 形式ごとの書き出し。KIF・KI2 の先頭（文字コード宣言・対局情報・盤面図）は `KifExporter::appendHeaderLines(ctx, out, HeaderOptions)` で共有し、KI2 の本譜と変化は `appendKi2Moves` の1つのループで書く
    - `kifunodereplay.cpp/.h` - 分岐ツリーのノードを指す前の局面と一緒にたどる `KifuNodeReplay`（`board()`・`mover()`・`previousTo()`・`canWrite(node)`・`advance(node)`・`branchFrom(parent)`。指せない手はノードの SFEN から取り直す）。KI2・JKF の書き出しが使う
    - `usencodec.cpp/.h` - USEN の符号表 `UsenCodec`（指し手の3文字と `UsiMove` の符号化と復号 `encodeMove`・`decodeMove`、開始局面 `encodePosition`・`decodePosition`、手順 `decodeLine`・`decodeLines`（"~" で1回だけ分けて本譜と変化の `UsenLine` にする））。`UsenToSfenConverter` と `UsenExporter` が使う

- **analysis/**: 解析機能（GUI に依存しない）
  - `analysissession.cpp/.h` - 検討・詰み探索のセッション。検討中に表示中の局面が変わったら `KifuNavigationState::currentUsiPosition()`（開始局面からの手順）で送り直す
  - `kifuanalysissession.cpp/.h` - 棋譜解析の実行 `KifuAnalysisSession`（`start(AnalysisRequest)`・`stop()`。枠 E1 のエンジンと動作状態「棋譜解析中」のリースを持ち、取り上げられたらやめる。局面は `RecordLine::usiPositions`。stop の後に bestmove を返さないエンジンは CLI と同じ10秒で打ち切り、訳した理由を出す。知らせは `plyStarted`・`plyAnalyzed`・`finished`・`errorOccurred`）。画面（結果の表・評価値グラフ・棋譜の移動）は ui の `KifuAnalysisController`
  - `engineanalysisrunner.cpp/.h` - 局面の列を1つのエンジンで順に解析する実行器 `EngineAnalysisRunner`（借りた `UsiEngineSession` を起動し、局面ごとに go infinite → 一定時間で stop → bestmove。候補手の数 `multiPv`（0 は送らない）と stop の後に bestmove を待つ上限 `replyTimeoutMs` は依頼で決める。`stop()` は解析中の局面の結果を待ち、`cancel()` は待たない）。GUI の棋譜解析と CLI の analyze・analyze-kifu で共通。1局面の結果 `PlyAnalysis` は `PlyAnalysisCollector` が multipv 1 の info と bestmove から作る
  - `tsumecandidatejob.cpp/.h`・`tsumeverificationjob.cpp/.h` - 詰将棋の候補1つの処理（手数の確認・唯一解の検査・要らない駒の除去）と、総予算つきの唯一解の検査（引き出し型の `MateQuerySource`。生成と CLI の検査で共通）
  - `tsumeshogigenerator.cpp/.h` - 詰将棋の生成（候補の待ち行列・問い合わせの中継・結果の集計）
  - `tsumecollectionsession.cpp/.h` - 詰将棋集の進捗・解析待ちの管理
  - `matesearchposition.h` - 詰み探索に送る局面 `MateSearchPosition`（`forLine`: 途中局面は開始局面からの手順、開始局面は `attackerToMove` で玉の有無から決めた攻方の手番・手数1。`Position` で合わせ、文字列を組み立てない）

- **engine/**: USI (Universal Shogi Interface) エンジン連携（対局の仕組みに依存しない）
  - `usi.cpp/.h` - GUI から使うエンジン1台の窓口（局面の列の解析には `session()` の `UsiEngineSession` を渡す）
  - `engineslots.cpp/.h` - エンジンの枠 E1・E2 `EngineSlots`（対局・検討・棋譜解析・CSA 通信対局で共有）。リース・取り上げの知らせ・表示の消去だけを受け持つ。使う側は `acquire(owner, slot, name)` で move-only の `EngineLease`（`engine()`・`reset()`）を受け取り、持っている間だけ使う。手放すと枠を空ける（持ち主には知らせない）。ほかの持ち主が同じ枠を取った・`destroyEngines()` で全部の枠を破棄したときは、前の持ち主に `engineRevoked(owner)` で知らせる（枠にあるかは `holds(engine)`）
  - `usienginesession.cpp/.h` - 起動と初期化（usi〜readyok）と探索の切り替え。GUI と CLI の実行器が共有する。探索（`search(UsiSearchRequest)`）は最後に頼んだものだけを行い、探索中なら stop を送って結果を捨ててから始める（検討のやり直し・先読みの外れ・詰みの問い合わせは頼み直すだけ）。空の局面は受け付けない（false）。`stop()` は最後の依頼をすぐ終わらせて結果を届け、`cancelSearch()` は取りやめる。`startAsync()`・`probeAsync()` は値を返さず、起動の失敗は必ず呼び出しから戻った後に `errorOccurred` で知らせる（知らせる前に `quit()`・次の起動をしたら知らせない）ので、使う側は起動の途中の再入に備えない（`Usi::startEngine`・`MateOracle::start` も同じ）
  - `usiprotocolhandler.cpp/.h` - USI protocol implementation
  - `thinkinginfoformatter.cpp/.h`・`enginesearchstatus.h` - info を思考の表の行（読み筋は `PvText`）と探索状況 `EngineSearchStatus`（数値のまま。info 1行ごとに `searchStatusUpdated` で1回送り、書式は表示する側が決める）にする
  - `usilineparser.cpp/.h` - 受信行（info・bestmove・checkmate・option・id）を型付きの値にする
  - `mateoracle.cpp/.h` - 詰みの問い合わせ（go mate）の部品 `MateOracle` と答え `MateAnswer`（`UsiCheckmate` からの変換は `MateAnswer::fromCheckmate` だけ）
  - `matequeryloop.cpp/.h` - 引き出し型の問い合わせ元 `MateQuerySource` を `MateOracle` で回す `MateQueryLoop`
  - `engineprocessmanager.cpp/.h` - Engine process lifecycle
  - `usimatchhandler.cpp/.h` - 対局での USI のやり取り（先読みの当たりは ponderhit、外れは `UsiEngineSession::search` に頼み直す）。定跡手（`requestBookMove`）は探索させず、先読み中なら止めて、待つ時間の後で同じ `moveReady` で知らせる（「すぐ指させる」は `playBookMoveNow`）
  - `matchtimesource.cpp/.h` - エンジンの思考に使う時間の出どころ `MatchTimeSource`（ローカル対局は対局時計の `ClockTimeSource`、通信対局はサーバーの残り時間を返す `CsaGameCoordinator`、時計のない場合は `FixedTimeSource`）

- **network/**: CSA通信対局
  - `csaclient.cpp/.h` - CSA protocol client（受信の解釈は `csaclient_parser.cpp`、送信は `csaclient_commands.cpp`）
  - `csagamecoordinator.cpp/.h` - CSA game coordination（手順と持ち時間と手番は `CsaGameProgress`。対局の状態 `gameState()` は持たずに、接続の段階を `CsaClient::ConnectionState` から、対局中・終局後・失敗を司令塔の段階 `Phase` から求める）。棋譜はローカル対局と同じ受け手（`MatchRecordSink`）に書き、動作状態 `DuringCsaGame` も自分で持つ（棋譜を作ってから対局中にし、終局では棋譜を確定してから待機に戻す。`MatchCoordinator` と同じ順）
  - `csagamechannel.cpp/.h` - 通信対局の窓口 `CsaGameChannel`（`GameChannel` の実装。操作はサーバーへ送り、入玉宣言の成否はサーバーが判定する）

- **style/**: ウィジェットの書体と見た目（QWidget に依存する。analysis・network の上、models の下の層。`engine`〜`network`・`models` を参照しない）
  - `applicationfonts` - 表示言語に応じた書体 `ApplicationFonts`
  - `applicationstyle`・`applicationrendering` - アプリ全体のスタイルと描画の設定
  - `buttonstyles.h`・`tablestyles.h` - ボタン・表のスタイルシート（`ButtonStyles`・`TableStyles`）
  - `movefont.h` - 指し手・読み筋の書体の規則 `MoveFont`（表のモデルの FontRole と `KifuMoveDelegate` で共通）
  - `boardappearance` - 対局画面の外観（駒のセット・配色・見た目）の持ち主 `BoardAppearance`（すべての盤面が共有する。`pieceStyle`・`colors`・`visuals` と、まとめた値 `AppearanceState`（`appearancestate.h`）の `state()`・`apply()`。変わったものだけを `pieceStyleChanged`・`colorsChanged`・`visualsChanged` で知らせる）。保存は `appearancesettings`（`AppearanceSettings`。外観ダイアログの大きさ・開いていたタブ・駒の絞り込みも）
  - `boardcolors.h`・`boardvisuals.h` - 配色 `BoardColors`（設定のキーと外観ダイアログの表示名の表 `fields()`）と、木目・駒の影・駒の大きさ `BoardVisuals`
  - `pieceimageprovider` - 駒の画像の解決とキャッシュ `PieceImageProvider`（駒のセットは持たず、今の駒のセットは `BoardAppearance`）
  - `piecestylecatalog` - 駒のセットの表 `PieceStyleCatalog`（ID・分類 `Family`・表示名・画像の場所）。駒のセットを足すときはこの表に1行足す（外観ダイアログの絞り込みの番号は `Family` の値）
  - `boardappearancecatalog` - 部品ごとの見本（将棋盤・駒台・対局者情報・背景）と、おすすめの組み合わせ `combinations()`（駒のセットと部品の見本の名前）。`boardcolorpresets` は盤・駒台・背景のおすすめ配色（「詳細」の配色のコンボ）

- **board/**: 盤面編集・操作
  - `boardinteractioncontroller.cpp/.h` - 盤のクリックを移動元・移動先にし、選択・直前の手・合法な移動先のハイライトを出す（合法な移動先を出すかは保存した設定から始め、`setLegalMovesVisible` で保存する。主画面と詰将棋を解く画面で共通）。クリックを手番の側の手にする処理 `moveFromClick()`（成り・不成の両方が合法なら `setPromotionChooser()` の関数で尋ねる）もここにあり、主画面と詰将棋を解く画面の行き先が使う。ドラッグは行き先の `submit()` から戻った後に終える
  - `positioneditrules.cpp/.h` - 局面編集の規則 `PositionEditRules`（駒の移動 `moved`・右クリックの成・不成・先後の巡回 `cycled`・盤を空にする `cleared`。`Position` を変える関数で、盤へは `apply`・`cyclePiece`・`clear` が渡す）
  - `boardinputroute.h` - 盤で指した手の行き先 `BoardInputRoute`（つまめるか・局面編集中か・着手の処理。主画面は `BoardMoveController`、詰将棋を解くダイアログは `TsumePlayDialog`）
  - `positioneditsession.cpp/.h` - 局面編集（開始・終了・編集中の盤の操作（全駒を駒箱へ・平手・詰将棋・手番の入れ替え。メニューは `MenuActionsWiring` がつなぐ）・編集中の着手）。「編集中か」は動作状態 `DuringPositionEdit` だけで決まる

- **ui/**: UI層
  - **presenters/**: 表示への反映
    - `notificationpresenter`（ui・app の知らせ（情報・警告・エラー）の唯一の出し手。静的な `NotificationPresenter::information`・`warning`・`critical` で出し（閉じるのを待たないなら `Wait::No`）、自動化 API の `ScopedCapture` の間は記録だけする。文は PlainText。ui・app で `QMessageBox` の静的関数を使わない（`tst_structural_kpi`）。確認の問いは `DialogUtils::confirmAction`・dialogs の `UnsavedChangesPrompt`。dialogs・widgets は自分で出す）, `enginedisplaypresenter`（エンジンの表示の持ち主。検討の表示は `AnalysisSession` の知らせだけから決める）, `playerinfopresenter`（盤の対局者名）, `evaluationgraphpresenter`（評価値グラフの点の唯一の書き手）, `kifudisplaypresenter`（棋譜欄・分岐）, `boardsyncpresenter`（盤と手番）, `piecesoundplayer`（駒音 `PieceSoundPlayer`。`GameBoard::moveCommitted` と詰将棋対局の着手で鳴らす。音質の加工は `piecesoundprocessor`、音質の値と設定は services の `piecesoundtone.h`・`AppSettings`。駒音の設定ダイアログは値をシグナルで知らせるだけで、`DialogLauncher` がプレイヤーに結ぶ）, `candidatearrowpresenter`（検討・対局のエンジンの読みと、定跡ウィンドウで開いている定跡の定跡手（緑。「表示」→「盤に定跡手を表示」）の矢印。対局の矢印は E1・E2 の思考の表の最新の最善の読み筋から作り、探索中か・先読みかだけをエンジンに聞く。どれを出すかは ui/controllers の `BoardArrowActions` の `QAction` だけを読む）, etc.
  - **controllers/**: 利用者操作の処理
    - `boardarrowactions`（盤の矢印を出すかの4つの切り替え `BoardArrowActions`（手番側・先読み側・検討・定跡手の checkable な `QAction`）。設定から戻し、変わるたびに保存する。思考ドック・検討タブ・定跡ウィンドウのチェックと「表示」メニューの「盤に定跡手を表示」はこれに結ぶだけ）, `boardmovecontroller`（主画面の `BoardInputRoute`。行き先ごとに手を処理する）, `considerationuicontroller`（検討タブの操作を `AnalysisSession` へ伝えるだけ）, `josekiwindowcontroller`（定跡ウィンドウ（widgets の `JosekiWindow`）を作って表示中の局面に合わせ、表示中の手順が定跡を外れた手を出す。「棋譜に展開...」は条件のダイアログを出し、`JosekiLines::expand` の手順を `BranchTreeEditController::addVariationLines` で変化として足す。定跡そのものの操作は、ここが作る `josekibookcontroller`（`JosekiBookController`。ウィンドウの意図のシグナルを受け、ファイルは `JosekiFileController`、定跡手の追加・編集・削除は `JosekiEditController`、マージは `JosekiMergeController`（「現在の棋譜から」は表示中の手順）に渡す。ドロップの振り分け、ドックを閉じるときの確認と題名の未保存の印も受け持つ）。定跡の表示（「■ 停止」）・「盤の手を登録」の checkable な `QAction` を持ち、正本にする（「盤に定跡手を表示」の正本は `BoardArrowActions`）。定跡ウィンドウのボタン・チェック、盤の矢印、盤の手の登録はこの `QAction` を読む）, `josekiregistrationcontroller`（思考の表・検討の表・棋譜解析の結果の表の右クリックで読み筋の最初の手を定跡に登録し、「盤の手を登録」がオンなら盤で指した手（`BranchTreeEditController::boardMoveRecorded`）と対局で指された手（`GameSession::gameMoveRecorded`。人間・エンジンとも、ローカル・通信対局とも）を登録する。定跡を変えられないとき（`JosekiBookSession::canEdit`）は押せない）, `menuwindowcontroller`（メニューウィンドウの分類と、ツールバーの「メニュー」でドックを出し入れする `toggleWindow`）, `pvclickcontroller`, `dialoglauncher`（結果を受ける部品のないダイアログ（設定・情報・詰将棋）と成りの選択を開く。結果を使う部品があるダイアログはその部品が開く: 対局開始は `GameSession`、通信対局は `CsaGameSession`、棋譜の貼り付けと局面集は `KifuFileController`）, `kifufilecontroller`（棋譜の開く・保存・貼り付け・コピーと局面集ビューア（`openSfenCollection`。開いている間は1つ）。読み込みは kifu の `KifuLoader`、保存で聞くダイアログは dialogs の `KifuSaveDialogs`）, `widgetimageexporter`（盤・評価値グラフの画像のコピーと、形式を選んだ保存 `WidgetImageExporter`。決まった場所への PNG の保存は `screenshotservice`）, etc.
    - `kifunavigationcontroller` - 棋譜ナビゲーション（ボタン・分岐）。局面送りの6つの操作は `QAction` の組（`action(Step)`）で1つだけ持ち、棋譜欄と評価値グラフのボタンを `bindButtons()` で結ぶ（有効・無効も QAction に従う。切り替えは `setStepActionsEnabled()`）
  - **coordinators/**: 複数の部品にまたがる UI の流れ（`csagamesession`（通信対局のダイアログ（`openGameDialog`。開いて結果を受け、対局を始める）・待機ダイアログ・結果のダイアログ・ステータスバー・盤の反転だけ。評価値・エラー・通信ログ・盤の手は合成ルートの `connectGame()` がつなぐ）・`commentcoordinator`）
  - **policy/**: 動作状態からの規則
    - `uistatepolicymanager` - 動作状態ごとのメニュー・ボタンの有効/無効（コンパイル時の表）
    - `humanturnpolicy` - 盤の手の行き先 `boardInput()`（`BoardInput`。動作状態ごとの規則はここだけ）・人間が指せる手番か・対局が進行中か
  - **docks/**: ドック
    - `dockregistry` - ドックの表 `DockRegistry`（`DockId`・`DockSpec`）。12個のドックをこの表だけから作り（`createAll()`）、規定の場所に置き（同じ場所のドックは表で前のドックのタブに重ねる。起動時と `resetToDefault()` で同じ配置）、浮動・表示・位置を保存・復元する（`saveStates()`・`DockSettings::DockState`）。ドックを足すときは `DockId` と表に1行足す
    - `docklayoutmanager` - 名前付きレイアウト・起動時のレイアウト・ドックの固定（ドックは `DockRegistry` から取る）

- **views/**: 盤の描画と入力（`shogiview.cpp/.h`）。盤・マス・駒台のセル・座標の文字・対局者カードの位置は `boardgeometry`（`BoardGeometry`。マスの大きさ・反転・筋と段の数・駒台の大きさ（盤は2列4段。詰将棋問題一覧のプレビュー `TsumePositionPreview` は持ち駒を1列に並べて枚数を横に書くので幅を広く段を多くする）から1度だけ計算する変えない値で、すべてウィジェット座標。`ShogiView::boardGeometry()`）、盤に重ねる対局者カード・編集終了ボタン・駒箱の先後切り替えは `boardchromewidget`（カードは1側1枚の `playercard`（`PlayerCard`。手番の札・名前・時計を子に持つ）で、`chrome()->card(Turn)` で触る）、局面によらない部分の画像は `boardstaticlayer`、ドラッグは `shogiviewinteraction`、ハイライト（層 `BoardHighlightLayer` ごとのマスの値。色は層で決まる）と矢印は `shogiviewhighlighting`
- **automation/**: 自動化 API（`ShogiBoardQ --automation`）。引数の表・エラーの分類などの共有部品は headless/（設計: `docs/dev/mcp-server.md`）
  - `automationserver.cpp/.h` - QLocalServer で改行区切り JSON-RPC 2.0 を待ち受け
  - `automationdispatcher.cpp/.h` - JSON-RPC 解析とメソッド表（GUI 非依存）。メソッドを呼ぶ前に引数を表で検査し（知らない引数は拒否）、既定値を補う。エラー応答（`makeError()`）は `error.code` と `error.data.slug` を分類から決める
  - `automationcommands_*.cpp` - メソッドの実装。メソッド `x.y` は `automationcommands_x.cpp`（名前空間ごとに1ファイル。登録の一覧と共有の読み取りは `automationcommands.cpp`）、`automationactionpolicy` は QAction 許可リスト
  - `automationinteractions.cpp/.h` - 応答の後に行う UI 操作の予約 `AutomationInteractions::queue(QueuedInteraction{target, check, perform}, response)`。`check` は予約の時点と実行の直前の2回走る（メソッドは同じ検査を予約の前に書かない）。結果（pending・running・done・failed と分類つきの error・cancelled）を記録して `ui.interaction` で返す（応答に `"queued": true` と `"interaction"` の番号）。UI を操作するメソッドはこれで予約し、`AutomationDeferredCall` を直接使うのは `app.quit` だけ。実行時の失敗は握りつぶさない。MCP サーバーは `AppClient.call_queued()` で実行を待つ
  - `automationinput.cpp/.h` - 通常のマウス入力 `AutomationInput::sendClick()`・`sendDrag()`（盤・表のセル・分岐ツリー・ドックのタイトルバー）
  - `automationwidgetadapters.cpp/.h` - 部品の型ごとの読み（`describe()`・`widget.text`）と書き（`writer()`・`widget.setValue`）の表 `AutomationWidgetAdapters`。型を足すときは表に1行足す
- **headless/**: CLI・自動化 API・MCP サーバーが共有する GUI 非依存の部品（core〜analysis だけを使う。`tests/tst_layer_dependencies.cpp`）
  - `errorkinds.cpp/.h` - エラーの分類 `ErrorKind` と、語（slug）・JSON-RPC のコードの表（`ErrorKinds::slug()`・`rpcCode()`・`toJson()`。数値は `AutomationErrorCode`）、分類つきの失敗 `Failure`（分類・文・対処）。CLI（`CliOutput::fail(ErrorKind, ...)`・`fail(Failure)`）・自動化 API（`AutomationError(ErrorKind, ...)`・`AutomationError(Failure)`）・MCP サーバーで同じ失敗は同じ語にする。分類を足すときは enum と表に1行ずつ足す
  - `paramspec.cpp/.h` - 引数の表の部品 `ParamSpec`（名前・型・必須・範囲・既定値・選択肢・説明）と `OperationSpec`。JSON Schema の出力（`ParamSpecs::inputSchema()`）と検査（`validate()`）。CLI・自動化 API・MCP で共有する
  - `automationmethodspecs.cpp/.h` - 自動化 API の全メソッドの引数の表 `AutomationMethodSpecs`。メソッドは補われた値を読み、`AutomationParams` は範囲が画面の状態で決まる引数にだけ使う
  - `automationendpoint.cpp/.h` - 自動化 API の接続先 `AutomationEndpoint`（既定のソケットの場所・エンドポイントファイル・1メッセージの上限 `kMaxMessageBytes`）。自動化 API のサーバーと CLI の `environment` が共有する
  - `outputpath.cpp/.h` - 書き出し先の事前検査 `OutputPath::check`（絶対パス・ディレクトリでない・上書きの許可・置き場所。CLI の出力先と `kifu.save` で共通。MCP の `paths.py` も同じ分類）
  - `kifuconversionservice`, `enginecatalog`, `matesearchrunner`, `tsumeverificationrunner`（詰みの問い合わせは `MateOracle`） - CLI と自動化 API が使うサービス（局面の検証は core の `SfenValidator`、その JSON は `sfenvalidationjson`）。失敗は `Failure` で返す
- **cli/**: `shogiboardq-cli`（GUI を起動しない JSON 出力コマンド。`mcp/` の MCP サーバーが呼び出す）
  - `clicommandspecs.cpp/.h` - 全コマンドの引数の表 `CliCommandSpecs`（オプション名は引数名の `_` を `-` にしたもの）。オプションの登録・使い方の文・コマンドラインから検査済みの JSON への変換（真偽値はフラグ、文字列の配列は空白区切り）をこの表から行う。コマンドの名前・実装・出力の種類（JSON か JSON Lines のイベントか）は `main.cpp` の表の1行で決め、`CliCommands::run()` が引数を表で検査してから、検査済みの JSON をコマンドに渡す（コマンドは `resolvePosition(params)`・`resolveEngine(params)` で解決する）。指し手の JSON は CLI と自動化 API で共通の `headless/recordjson`（`RecordJson::entry`・`moves`）
  - `environment` コマンドは自動化 API の既定のソケット・エンドポイントファイル・設定ファイルの場所、1メッセージの上限（headless の `AutomationEndpoint`。自動化 API のサーバーも同じ値を使う）、エラーの分類の表を出す。MCP サーバー（`mcp/shogiboardq_mcp/environment.py`）はこれを読み、場所や上限・語を Python に写さない
  - `boardimagerenderer.cpp/.h` - 盤面画像 `BoardImageRenderer`（render-board。`ShogiView` を画面に出さずに描くので、cli で views を使うのはこれだけ）
  - `describe` コマンドは CLI と自動化 API の表を JSON Schema で出す。MCP のツールのスキーマはその出力を保存した `mcp/shogiboardq_mcp/specs.json` から作る（引数名は CLI・自動化 API・MCP で同じ）ので、引数を変えたら `python3 -m shogiboardq_mcp.specs build/shogiboardq-cli`（mcp/ で）で作り直す（`mcp/tests/test_param_specs.py` が古さを検査する）。CLI は `--params-json '<JSON>'` でも引数を受け、MCP サーバーはこれで渡す。以前のオプション名は旧名（`ParamSpec::alias`・`aliasFlag`）で受け付ける
- **widgets/**: Custom Qt widgets
  - ドックのパネル（`thinkingpanel`・`gameinfopane`・`recordpane`・`considerationpanel`・`usilogpanel`・`csalogpanel`・`commenteditorpanel`・`analysisresultspanel`・`branchtreeview`・`menuwindow`・`josekiwindow`）はすべて `QWidget` の派生で、部品をコンストラクタで作りきる（USI・CSA の通信ログは共通の基底 `logpanel` の `LogPanel`。ログ・折り返し・最新へ・コピー・消去・コマンドの欄）。`DockRegistry` はパネルそのものをドックに入れる（`buildUi()`・`containerWidget()` のような、外から後で中身を作らせる・取り出す公開の関数は持たない）
  - `thinkingpanel`・`thinkingtableview`（思考の表。対局と検討タブで共用）・`pvtableview`（読み筋の表 `PvTableView`。数値の列・「盤面」ボタン（`boardRequested`）・読み筋の描き方・行の高さを受け持ち、思考の表の基底で棋譜解析の結果の表にも使う。行の右クリックのメニューの項目は受け手が `rowContextMenuAboutToShow(row, menu)` で足す）・`engineinfowidget`（探索状況の書式はここで決める）, `gameinfopane`（対局情報。中身は `models/gameinfoeditmodel`）, `recordpane`, `considerationpanel`（検討タブ。設定 `ConsiderationSettings`（`considerationsettings.h`）を正本として持ち、開始/中止のボタンは実行の状態 `RunState` で決まる）, `evaluationchartwidget`（評価値グラフ。`EvaluationSeries` を描くだけ。表示の設定は値 `EvaluationChartSettings`（`AnalysisSettings::evalChart`）、軸は `evaluationchartrange` の `EvaluationChartRange::compute`。「表示設定…」は `settingsRequested` を出し、`DialogLauncher` が dialogs の `EvaluationChartSettingsDialog` を開く）, `analysisresultspanel`（棋譜解析の結果の表。列は `KifuAnalysisListModel::Column`、モデルは `setModel()` で1度だけ結ぶ。ドックを出すのは `KifuAnalysisController`）, `branchtreeview`（分岐ツリーの図 `BranchTreeView`。`QGraphicsView` の派生でドックの中身そのもの。`setTree()` でツリーの変更の知らせを自分で受け、手順の末尾に1手足しただけならその手だけを描き足し、それ以外は作り直す。外とは `KifuBranchNode` の ID だけでやり取りする（`setCurrentNode(nodeId)`・`nodeActivated(nodeId)`・`nodeContextMenuAboutToShow(nodeId, menu)`・`refreshNode(nodeId)`）。行・段・折りたたみの配置は `branchtreelayout`）・`branchnodeitem`（図の1手（開始局面を含む）の図形 `BranchNodeItem`。ノード ID・行・手数を型付きのメンバで持ち、`BranchTreeItemRoles` は自動化 API のためだけに付ける）
  - `tableheaderview` - 表の見出し。列幅は利用者がドラッグしたときだけ `sectionResizedByUser` で保存する
  - `fontsizehelper` - A-/A+ の文字の大きさ（範囲・保存・変更の通知）
  - `fontsizecontrol` - A-/A+ のボタンの組 `FontSizeControl`（文字・見た目・ツールチップと、範囲の端で押せなくする処理はここだけ）。ドックのパネル（思考・検討・棋譜・対局情報・コメント・通信ログ・棋譜解析の結果・定跡ウィンドウ・メニューウィンドウ）が使う。ボタンは役割のプロパティ `FontSizeControl::kRoleProperty`（値は `decrease`・`increase`）で探せる
  - `editactions` - 元に戻す〜貼り付けの5つの `QAction` の組 `EditActions`（標準ショートカット・アイコン・実際のショートカットを添えたツールチップはここだけ。ボタンは `createButton()`。何をするか・有効にするかは使う側が決める）。コメント欄（`CommentEditorPanel`）と対局情報（`GameInfoPane`）が使う
  - `recentfilesmenu` - 「履歴」ボタンのメニュー `RecentFilesMenu`（ファイル・区切り・「履歴をクリア」、空なら「（履歴なし）」。選ばれた項目は `fileTriggered(QString)`・`clearTriggered()` で知らせる）。定跡ウィンドウ（`JosekiWindow::recentFileRequested` で知らせ、`JosekiFileController::openRecent(path)` が開く）と局面集ビューアが使う
  - `dialogfontscale` - ダイアログの A-/A+ と保存 `DialogFontScale`（全ダイアログ共通。各部品を元の大きさからの差で拡大・縮小する）
  - `engineselector` - 登録エンジンを選ぶ部品 `EngineSelector`（呼び出し側が置いたコンボ・「設定」ボタン・未登録の案内に結び、一覧の読み込み・保存した選択の復元と保存（`EngineListSettings::SelectionKeys`）・未登録のときの無効化を受け持つ）。設定ダイアログは dialogs にあるので開かず、`settingsRequested(EngineEntry)` で受け手に頼む。棋譜解析・詰み探索・詰将棋生成・詰将棋問題一覧・CSA通信対局のダイアログ、検討タブ、対局開始ダイアログの対局者の欄が使う
  - `playersetupwidget` - 対局開始ダイアログの対局者1人の欄 `PlayerSetupWidget`（コンボの先頭が「人間」（`EngineSelector` の内蔵の項目）で、その後に登録エンジン。人間なら名前の欄、エンジンなら「エンジン設定」ボタンと「定跡を使う」・「条件…」（条件のダイアログは dialogs の `JosekiPlayOptionsDialog`。定跡を開いていなければ押せない））。選択は `PlayerChoice`（`game/startgamedialogdata.h`）で読み書きし、保存したエンジンが削除されていれば人間に戻す
  - `timecontroleditor` - 片方の対局者の持ち時間・秒読み・加算の欄 `TimeControlEditor`（`TimeControlSide` で読み書き。秒読みと加算は排他で、相手側の欄にも `byoyomiEntered`・`incrementEntered` で知らせる）
  - `tablemodelutils.h` - 表へのモデルの付け替え `TableModelUtils::setModel`（Qt が置き換えた選択モデルを解放する）
  - `josekiwindow` - 定跡ウィンドウ `JosekiWindow`（ドックのパネル。表示中の局面の定跡手を表に出し、ファイル・定跡手の追加/編集/削除・マージ・ドロップ・棋譜への展開・着手は意図のシグナルで知らせるだけ。ダイアログは出さない（受けるのは ui の `JosekiBookController`・`JosekiWindowController`）。表の並べ替えだけはセッションに直接書く。列幅は利用者がドラッグしたときに保存する）・`josekitableview`（定跡手の表 `JosekiTableView`。ボタンの列・ダブルクリック・右クリックのメニュー（着手・編集・削除・先頭へ/上へ/下へ移動・出現頻度/評価値の順に並べる・コピー）を行の操作 `rowActionRequested` にする。並びは定跡ファイルに書く順で、盤の矢印の番号・対局の「表の先頭の手」も同じ。`JosekiBook::moveTo`・`sortMoves`）
  - `settingsformbinding` - 入力欄と設定の宣言（`Setting<T>`）の対応の表 `SettingsFormBinding`（`add()` を1度書けば `load()`・`save()`・`reset()`（既定値に戻す）を同じ表で行う。対局開始・詰将棋生成のダイアログが使う）
- **dialogs/**: Dialog implementations (with co-located .ui files)
  - `josekiexpanddialog`（定跡を棋譜に展開する条件）・`josekiimportdialog`（複数の棋譜・フォルダから定跡に登録。集めるのはワーカースレッドで、進み具合と中止を出す。定跡ウィンドウへのドロップでも開く）・`josekibookmergedialog`（別の定跡ファイルを足す条件）・`josekianalysisregisterdialog`（棋譜解析の結果をまとめて登録する手数と手番）・`josekimergedialog`（表示中の手順・棋譜ファイルの本譜の手を選んで登録。「登録済」は定跡に手があるか（`hasMove`）で、登録は `importMoves`）。定跡ウィンドウの「盤の手を登録」（保存しない設定で、起動するとオフ）の登録は `JosekiRegistrationController` が行う
  - `josekimovedialog`・`josekimoveinputwidget` - 定跡手の追加・編集。指し手は局面の合法手を棋譜の表記で並べた一覧から選ぶ（`JosekiMoveInputWidget`。一覧のオブジェクト名は `<name>Combo`。予想応手は着手後の局面の合法手で、先頭が「なし」）。値の受け渡しは `JosekiMoveDialog::setEntry(const JosekiMove&)`・`entry()`
  - `boardappearancedialog`・`boardappearancesamples` - 対局画面の外観ダイアログ `BoardAppearanceDialog`（駒・将棋盤・背景・駒台・対局者情報の見本の一覧、「おすすめの組み合わせ」、「詳細」の色と木目・影・駒の大きさ。選んだ外観は `BoardAppearance` に書く）と、見本の画像を描く `BoardAppearanceSamples`（見えた項目だけを描く一覧の `LazySampleDelegate`）
  - `promotedialog` - 成り・不成の確認。`PromoteDialog::ask(parent)` で尋ねる（主画面の `DialogLauncher::choosePromotion()` と詰将棋を解く画面で共通）
  - `pvboarddialog`・`pvline` - 読み筋盤（`PvBoardDialog(PvText)`）。読み筋1本の値 `PvLine`（`PvText` から作る。各手数の局面 `positions()`・手の表記 `moveText(ply)`・`displayPv()`・直前の手のハイライト `highlight(ply)` が返す `PvLine::Highlight`）。表示する手数は `BoardViewerWidget` だけが持つ
  - `tsumeplaydialog` - 詰将棋を解く画面。対局と正解手順の再生は `Mode { Playing, Reviewing }`、部品の有効・無効は `controlStates()` の表1か所で決める。挑戦・正答の記録は `analysis/tsumegamesession`（`TsumeGameSession`）が付ける
- **models/**: Qt models for lists and data binding（`shogienginethinkingmodel`・`kifuanalysislistmodel`: 思考の表・棋譜解析の結果の表。行は `ShogiInfoRecord`・`KifuAnalysisRow`（`kifuanalysisrow.h`）の値で持ち、表記の設定ごとの表示は `NotationTextCache` で覚える。詰みの評価値の表記（「3手詰」・手数の分からない詰みは「詰み」）は `scoretext` の `ScoreText` だけで作り、評価値グラフも「先手勝ち・3手詰」のようにこれを使う、`josekitablemodel`: 定跡ウィンドウの表（予想応手が「なし」の手には、指した後の局面の定跡から導いた応手を灰色で出す。`setMoves` に `JosekiBookSession::leadingMove` を渡す）、`usicommlogmodel`: エンジン1台の情報欄と通信ログの行の知らせ、`evaluationseries`: 評価値グラフの点。先手から見た値で持ち、向きの直しは `blackView()` だけ）
- **services/**: 設定の宣言と保存・登録エンジン・表記と翻訳の設定など、GUI に依存しない共有のサービス（外観は style、駒音は ui）
  - `engineprofilestore.cpp/.h` - 登録エンジンの設定（一覧・USIオプション・先読みの設定）の保存形式を知る唯一の場所（ほかの部品は設定ファイルを開かずにこの関数を使う）
  - `registeredengines.cpp/.h` - 登録エンジン1つ `EngineEntry` と一覧 `RegisteredEngines`（`load()`・名前とパスでの検索。GUI・CLI・自動化 API で共通）。機能ごとに選んだエンジンの保存は `enginelistsettings`（`EngineListSettings::SelectionKeys`）。オプションの型は `EngineOption` 1つ（`UsiLineParser::parseOption` が返す）
  - `notationstyle.cpp/.h` - 表示言語と棋譜の表記の設定 `NotationStyle`（`Notation`・`Options`・`resolveLanguage`・`configure`・`options`・`revision`）。起動時に `ApplicationTranslations` が設定し、kifu の `KifuPresentation` が読む
- **common/**: 共有の土台（`appactivitystate`（動作状態 `AppActivityState` と持ち主のリース `ActivityLease`）・`branchtreeitemroles.h`・`errorbus`・`logcategories`・`threadtypes.h`・`boundedfile.h`（利用者が選んだ棋譜・局面集を、通常のファイルだけ上限つきで読む `BoundedFile::read`）・`recentfilelist`（最近使ったファイルの一覧 `RecentFileList`。新しい順・重複なし・上限つき。定跡の `JosekiBookSession` と局面集ビューアが使う））

### UI Files

Qt Designer `.ui` files are co-located with their corresponding source files:
- `src/app/mainwindow.ui` - Main window
- `src/dialogs/*.ui` - Dialog files

Auto-processed by CMake's AUTOUIC.

### Resources (`resources/`)

- `shogiboardq.qrc` - Qt resource file
- `icons/` - Application icons (.icns, .ico, .png)
- `images/`
  - `actions/` - Menu/Toolbar action icons
  - `pieces/` - Shogi piece images
- `platform/` - Platform-specific files (app.rc for Windows)
- `translations/` - Translation files (Japanese `_ja_JP.ts`, English `_en.ts`)

## Key Technologies

- **Qt 6** (Qt6専用、Qt5非対応): Widgets, Charts, Network, LinguistTools
- **C++17** required
- **CMake 3.16+** build system
- **USI Protocol** for engine communication
- **CSA Protocol** for network play

## Internationalization

The app supports Japanese and English. Translations are in `.ts` files under `resources/translations/`. After modifying translatable strings, run `lupdate` to update `.ts` files, then rebuild to generate `.qm` files.

### Translation Update Guidelines

**ソースコードを修正した際は、必ず翻訳ファイルも更新すること。**

1. `tr()` でラップされた文字列を追加・変更した場合:
   ```bash
   cmake --build build --target update_translations
   ```
2. 翻訳ファイル（`.ts`）を開き、`type="unfinished"` となっている新規エントリに翻訳を追加する
3. 日本語が原文の場合は英語翻訳を、英語が原文の場合は必要に応じて日本語翻訳を追加する

### Translation Baseline (CI)

CI の `translation-check` ジョブは、未翻訳数がベースラインから増加すると fail する。
ベースラインは `resources/translations/baseline-unfinished.txt` に記録されている。

翻訳を追加したらベースラインを更新すること:
```bash
# ベースライン更新
grep -c 'type="unfinished"' resources/translations/*.ts | \
  sed 's|resources/translations/||' > resources/translations/baseline-unfinished.txt
```

### Settings Update Guidelines

**ダイアログに関するソースコードを修正した場合は、設定の宣言の表も更新すること。**

ダイアログやウィンドウを追加・修正する際は、以下の設定を永続化するか検討する:

1. **ウィンドウサイズ**: ユーザーがリサイズ可能なダイアログは、サイズを保存・復元する
2. **フォントサイズ**: フォントサイズ調整機能（A-/A+ボタン）がある場合は、サイズを保存
3. **列幅**: テーブルやリストビューの列幅を保存
4. **最後に選択した項目**: コンボボックスの選択状態など

設定はドメイン別ヘッダー（`src/services/appsettings.h`・`gamesettings.h`・`analysissettings.h`・`networksettings.h`・`josekisettings.h`・`tsumeshogisettings.h`・`enginedialogsettings.h`・`docksettings.h`、外観は値の型と同じ層の `src/style/appearancesettings.h`）に、`Setting<T>`（`src/services/setting.h`）で**キーと既定値を1行で宣言する**。getter と setter の組やキー定数は手で書かない。

**追加手順:**
1. ドメイン別ヘッダーに `inline const Setting<T> name{"Group/key", 既定値};` を1行足す
2. 読むときは `NS::name()`、書くときは `NS::name.set(value)`、既定値は `NS::name.defaultValue()`。既定値が翻訳文や実行時の場所で決まる項目は、宣言の既定値を空にして `NS::name.valueOr(tr("..."))` で読む
3. ダイアログのウィンドウサイズと A-/A+ は `DialogFontScale::install(this, 文字の大きさの宣言, options)` に渡す（`options.size` に大きさの宣言を入れれば、作るときに戻し、閉じた（Hide の）ときに保存する。デストラクタ・`done()`・`closeEvent` で `DialogUtils::saveDialogSize` を書かない）。ダイアログに `FontSizeHelper` のメンバや A-/A+ 用の connect・`applyFontSize()` は書かない。入力欄の値は `SettingsFormBinding`（`src/widgets/settingsformbinding.h`。数値・小数・チェック・ラジオの組（位置か2択）・コンボ・文字の欄）に欄と宣言を `add()` すれば、読み込み・保存・既定値に戻すを1つの表で行える（文字は前後の空白を除いて保存し、範囲外の位置は端に寄せる）。大きさを構造体の設定（`StructSetting`）に入れない
4. 値の範囲を絞る項目は `BoundedSetting<T>{"Group/key", 既定値, 最小, 最大}`、構造体を項目ごとのキーに保存する項目は `StructSetting<T>{{SettingField<T>::of("Group/key", &T::member), ...}, 整える関数}`、配列で保存する項目は `ListSetting<T>{配列のキー, 要素のキー}`（配列のキーは実行時に作ってもよい）・`StructListSetting<T>{配列のキー, {項目...}}` で宣言する（いずれも `setting.h`。使い方は `Setting<T>` と同じ）。グループ名や番号だけ違う組は、キーを組み立てて宣言を返す関数を1つ書き、組ごとにその関数で宣言する（`JosekiSettings::playOptionsSetting`・`GameSettings::StartGame::playerSettings`）。値の範囲は宣言に書き、読む側で `qBound` し直さない（範囲は `minimum()`・`maximum()` で読める）。これらで表せない処理（以前の版の値の引き継ぎ、`AppSettings::boardVisuals` の駒台の木目の代替のような未保存の項目の代わり、`kifuAnalysisColumnWidths` の `QVariantList` のような独自の保存形式）が要る項目だけを関数として `.cpp` に書く。その場合も `SettingsCommon::openSettings()` を使う（設定ファイルを `QSettings` で直接開くのは、`beginReadArray` などでグループの状態を変えたまま読み書きを続ける配列データ（`EngineProfileStore`）だけ）

**実装例:**
```cpp
// gamesettings.h
/// 〇〇ダイアログのウィンドウサイズ（デフォルト: 800x600）
inline const Setting<QSize> myDialogSize{"SizeRelated/myDialogSize", QSize(800, 600)};
/// 〇〇ダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> myDialogFontSize{"FontSize/myDialog", 10};

// mydialog.cpp
MyDialog::MyDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();

    // 文字の大きさ（初期値の読み込みと変更のたびの保存）。A-/A+ を渡さなければ決定ボタンの列に作る。
    // 大きさは作るときに戻し、閉じたときに保存する
    DialogFontScale::Options fontOptions;
    fontOptions.decrease = ui->buttonFontDecrease;
    fontOptions.increase = ui->buttonFontIncrease;
    fontOptions.size = &GameSettings::myDialogSize;
    DialogFontScale::install(this, GameSettings::myDialogFontSize, fontOptions);
}
```

- `DialogFontScale`（`src/widgets/dialogfontscale.h`）は各部品を元の大きさからの差で拡大・縮小する。太字や大きめの見出しは作るときに1回だけ相対の大きさを決めればよく、変更のたびに付け直さない。行の高さなどの追加の調整は `install()` が返す部品の `fontSize()`（`FontSizeHelper*`）の `fontSizeChanged` につなぐ。後から作った部品には `rescale()` で今の大きさを付ける。盤面など文字の大きさを自分で決める部品は、プロパティ `dialogFontScaleExcluded` を true にして対象外にする
- 文字の大きさの宣言の既定値を 0 にすると、未保存のときはダイアログの元の大きさから始める（補助ダイアログの `AppSettings::promotionDialogFontSize` など）
- ドックのパネルは `FontSizeHelper` を持ち、ボタンを `FontSizeControl` で結ぶ（`FontSizeControl::createToolButtons(parent, helper, 縮小の名前, 拡大の名前)` か、置いたボタンを渡す）。A-/A+ の文字・見た目・説明はパネルで決めない

## Development Environment

- **Qt Creator** is used for development

## Design Principles

- **MainWindow should stay lean**: Delegate logic to existing or new classes (coordinators, controllers, services). MainWindow should only handle MainWindow-specific responsibilities.
- When adding new features, prefer creating new classes in appropriate directories (`app/`, `controllers/`, `services/`, etc.) rather than adding code to MainWindow.
- **Deps/Hooks/Refs パターン**: `docs/dev/done/deps-hooks-refs-pattern.md` に従う。依存注入に3種の構造体パターン（Deps=外部依存注入、Hooks=上位層へのコールバック、Refs=親状態への参照）を使い分ける
- **スレッド安全性**: `docs/dev/threading-guidelines.md` に従う。UIアクセスはメインスレッド限定、ワーカーには値オブジェクトのみ渡す
- **層の依存**: 下の層は上の層を include しない。順に core・common → services → kifu → engine → game → analysis・network → style → models → views・widgets → board → dialogs → ui → app。`headless` は core〜analysis だけを使い、`automation`・`cli` は MainWindow・ダイアログ・ウィジェットに依存しない（`cli` は盤面画像 `boardimagerenderer` だけが views を使う。規則は `tests/tst_layer_dependencies.cpp` が検査する）
- **クラス名の役割**: `*Wiring` はシグナルの接続だけ、`*Presenter` は表示への反映、`*Controller` は利用者操作の処理、`*Coordinator` は複数の部品にまたがる流れ。役割が変わったら名前も変える
- **ファイルの分割**: 1ファイル1000行の上限（`tests/tst_structural_kpi.cpp`）に近づいたら、行数で `_ui`・`_draw` などに機械的に分けず、責務を別のクラスに切り出す

## Code Style

- **所有権ルール**: `docs/dev/ownership-guidelines.md` に従う。QObject は parent ownership、非QObject は `std::unique_ptr` を基本とする。裸の `new/delete` は原則禁止
- Compiler warnings enabled: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` and more
- clang-tidy configured for unused code detection (see `.clang-tidy`)
- **Do not use lambda expressions in `connect()` statements**. Use the member function pointer syntax:
  ```cpp
  // Good
  connect(sender, &Sender::signal, receiver, &Receiver::slot);

  // Avoid
  connect(sender, &Sender::signal, [this]() { ... });
  ```
- **Avoid Qt container detachment in range-loops**: Use `std::as_const()` or `qAsConst()` to prevent detachment warnings:
  ```cpp
  // Good
  for (const auto &item : std::as_const(list)) { ... }

  // Warning: clazy-range-loop-detach
  for (const auto &item : list) { ... }
  ```
- **メンバ変数の初期化**: C++ Core Guidelines (C.45, C.48) に従い、以下のように使い分ける:
  - **ヘッダーでのインクラス初期化（NSDMI）**: デフォルト値が固定の場合に使用
  - **コンストラクタ初期化子リスト**: コンストラクタ引数に依存する値、基底クラス、`this`ポインタが必要な場合に使用
  ```cpp
  // ヘッダー (.h) - 固定のデフォルト値
  int m_count = 0;
  bool m_isReady = false;

  // コンストラクタ (.cpp) - 引数依存・基底クラス・this が必要な値
  MyClass::MyClass(Model* model, QObject* parent)
      : QObject(parent)
      , m_handler(std::make_unique<Handler>(this))
      , m_model(model)
  ```
- **Debug logging**: `docs/dev/debug-logging-guidelines.md` に従う。`qDebug()` はデバッグビルド専用。リリースビルドでは `QT_NO_DEBUG_OUTPUT` により無効化される
- **clazy 警告を出さないコーディング**: clazy level0,level1 の警告が出ないようにコードを書くこと。特に以下に注意:
  - **`clazy-fully-qualified-moc-types`**: `signals:` / `public slots:` の引数型はMOCが正規化できるよう完全修飾する。ネスト型は `ClassName::NestedType` と記述する。Qt6では `QVector` は `QList` に正規化されるため `QList` を使う
    ```cpp
    // Good - ネスト型を完全修飾
    void gameEnded(const MatchCoordinator::GameEndInfo& info);
    void gameStateChanged(CsaGameCoordinator::GameState state);

    // Bad - MOCが正規化できない
    void gameEnded(const GameEndInfo& info);
    void gameStateChanged(GameState state);
    ```
  - **`clazy-const-signal-or-slot`**: `const` メソッドやgetter（引数なし＋戻り値あり）を `signals:` / `public slots:` に置かない。`connect()` で使わないメソッドは `public:` セクションに配置する
  - **`clazy-range-loop-detach`**: 上記の `std::as_const()` を使用する
