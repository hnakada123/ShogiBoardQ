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
# Version, automation socket, endpoint file, settings file, message limit, error kinds and file suffixes as JSON
./build/shogiboardq-cli environment

# Python MCP server tests (also registered in ctest as tst_mcp_python)
python3 -m pytest -q mcp/tests

# P2P対局（実験版。libsodium と Rust が要る。QtKeychain は任意）。通信プロセス shogiboardq-peer も cargo でビルドして同梱する
cmake -B build -S . -G Ninja -DENABLE_P2P_PLAY=ON -DBUILD_TESTING=ON
cargo test --locked --workspace && cargo clippy --locked --workspace --all-targets -- -D warnings
./build/shogiboardq-cli verify-match --input-path /path/to/game.sbqmatch

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
  - `menuactionswiring.cpp/.h` - メニュー・ツールバーの項目と処理する部品の接続。項目の triggered・toggled はすべてここでつなぎ、チェックできる項目（合法手の表示・ツールバー・駒音・ドックの固定・移動元）は設定を持つ部品の今の値から始める（保存は部品）。ここでつながないのは、排他の組（言語・指し手の表記。`LanguageController`）と、項目を作る部品のメニュー（保存したレイアウトの一覧。`DockLayoutManager::setSavedLayoutsMenu`）と、「盤に定跡手を表示」（正本の `BoardArrowActions` が toggled をつなぐ）だけ
  - `gamesession.cpp/.h` - 対局の開始・終局・再開・後始末（「新規」の `abandonGame()`）と、対局の棋譜の受け手（`MatchRecordSink`）。ローカル対局と通信対局の棋譜を開始から終局・自動保存まで1本の流れで扱い（司令塔が通信対局の開始で `beginRecord`、着手と終局で `appendRecord`、終局で `finishRecord` を呼ぶ）、投了・中断・即指し・入玉宣言は今の窓口 `GameChannels::active()`（`GameChannel`）へ送る。ローカル対局の終局の知らせ（`MatchCoordinator::gameOverMessage`）も出し（`showGameOverMessage`）、エンジン同士の連続対局で次の局が残っていれば閉じるのを待たずに次の局へ進める（通信対局と同じ。前の局の知らせは閉じる）
  - `sessionresetcoordinator.cpp/.h` - 対局の開始前（`prepareGameStart`。連続対局の次局・通信対局の開始も）・新規・局面編集で棋譜を作り直す。評価値グラフとエンジンの表示（`EngineDisplayPresenter::clearAll()`。思考・検討・解析結果。通信ログと結んでいるエンジンの名前は残す）は文書の置き換え（`KifuDocument::replaced`）を受けて自分で消える。通信ログ（USI・CSA）は対局を始めるとき（`GameSession::gameStarting`・`CsaGameUiController::gameStarting`）に `clearEngineLogs()` で消す（通信対局はログインとエンジンの起動の後に START で棋譜を作り直すので）

- **core/**: 純粋なゲームロジック（Qt GUI非依存）
  - `shogiposition.cpp/.h` - 局面の値 `Position`（盤81マス・持ち駒・手番・手数）。SFEN の読み書きと指し手の適用（`apply`）はここだけ。手番側の手として局面に合うか（動かす駒の色・自分の駒を取らないか・打つ駒と打つ先）は `mismatchOf` だけで決める（棋譜の1行の `MoveRecord::play` と読み筋の `PvText` が使う）。計算のための局面はこの値で持つ
  - `shogiboard.cpp/.h` - 表示・局面編集に使う盤。`Position` を1つ持ち（`position()`）、変更をシグナルで知らせる QObject（局面の包みと知らせだけ。局面を変える規則は持たず、着手と局面編集は次の局面を `Position` で作って `replacePosition(next)` で渡す。変わったマス・駒台・手番だけを知らせる）
  - `shogitypes.h`・`piecerules.h` - 手番と駒（`Piece` は種類＋後手なら16。並びは fmv と同じ）。1組の駒の枚数 `PieceRules::setCount`（駒箱 `Position::pieceBox`・局面の駒の数の上限 `PositionValidator`・詰将棋の候補局面 `TsumeshogiPositionGenerator` で共通）・行き所のない駒 `isDeadSquare`・成れる段（敵陣） `inPromotionZone`（持将棋の敵陣も）・持ち駒の並びの規則
  - `positionvalidator.cpp/.h` - 局面の検査 `PositionValidator`（`check()`：駒の数・駒台の玉・玉の重複と有無・手番でない側への王手と、手番側の王手・合法手の数。`boardProblem()` は盤と持ち駒だけ）。validate-sfen（`SfenValidator`）・局面集・詰将棋問題一覧・対局の開始で同じ規則を使う。合法手だけを指すのは `EngineMoveValidator::applyIfLegal`。Hayanagi は詰み探索だけに使う
  - `sfenvalidator.cpp/.h` - SFEN の検査 `SfenValidator::validate`（手番・手数の欄と `Position::fromSfen`・`PositionValidator` の結果を値 `SfenValidation` にし、読めた局面 `position` も返す）と、合法手だけを指した後の SFEN `applyUsiMoves`。JSON にするのは headless の `SfenValidationJson`
  - `pvtext.cpp/.h` - 読み筋の値 `PvText`（`PvText::play(開始局面, USI の語, 至った手の移動先)` が局面の上で1回だけ指し、指せた手（指した側・動かした駒）と指せなかった語を持つ）。思考の表・棋譜解析・読み筋盤・詰将棋の正解手順で共通。表記は kifu の `KifuPresentation::pv`・`pvMove` が表示のたびに作る
  - `usiposition.cpp/.h` - エンジンに探索させる局面 `UsiPosition`（開始局面・指し手の列・直前の手）。kifu の `RecordLine` が作るので core に置く。position 文字列は `UsiPosition::commandFor(startSfen, moves)` だけで作る（`command()` と USI 形式の書き出し `UsiExporter` で共有）。読む側の語の分け方は `UsiPosition::split`（`Words`。"position"・"sfen"・"moves" は省け、手数の欄も省ける）だけで、`parse`・局面の文字列の SFEN `sfenOf`（接頭辞を除き startpos を平手にする）・棋譜の USI 形式の読み込み（`UsiToSfenConverter`。ファイルの中の対象の行の選択も split と `Position::fromSfen`）・局面の読み込み（`KifuLoader::loadPositionFromSfen`）・局面集ファイルの行（`PositionCollectionFile::parseLine`。前に position を書いたか `Words::positionCommand`・startpos で書いたか `Words::startpos`）が使う
  - `usiscore.cpp/.h` - USI の評価値 `UsiScore`（cp・詰み・上限下限）。エンジンが出す手番側から見た値を先手から見た値に直すのは `blackView(Turn)` だけ（評価値グラフ・棋譜解析の結果の表・CLI の analyze-kifu で共通。逆の直しは `viewedBy(Turn)`）
  - `shogimove.cpp/.h`・`usimove.h` - Move representation
  - `enginemovevalidator.cpp/.h` - Move legality validation（`fmv::Converter` を境界に `fmvlegalcore` のビットボード合法手生成を使う）
  - `sfenreplay.cpp/.h` - 局面に USI の1手を指した後の局面 `SfenReplay::positionAfter`（合法手かは確かめない）
  - `boardsquaresize.h` - 盤のマスの幅の範囲 `BoardSquareSize`（20〜150、既定50。盤の描画・設定の宣言・CLI の盤面画像で共通）
  - `boardcoordinates.h` - 盤面表示の座標（駒台は筋10・11、駒箱は筋12）と駒・USI の指し手の対応（表示の座標の駒・駒台と駒箱の枚数の読み `pieceAt`・`isAvailable` もここだけ）
  - `handicaps.h` - 手合割の表（名前・JKF の preset・開始局面）と局面の手合割（`Handicaps::of(Position)`・`ofStart(開始局面の文字列)`。盤・持ち駒・手番で決め、手数は見ない）。平手かどうかは `Handicaps::isHirate` だけで判定する
  - `jishogicalculator.cpp/.h` - 持将棋の点数計算・判定 `JishogiCalculator`（駒の点数 `kMajorPiecePoints`・`kMinorPiecePoints`、敵陣に要る駒の数 `kRequiredPiecesInCamp`（`hasEnoughPiecesInCamp`）、点数の閾値 `winningPoints`・`requiredPoints` はここだけ。持将棋の点数のダイアログと入玉宣言の判定の説明 `NyugyokuJudgement` の数もここから入れる。敵陣は `PieceRules::inPromotionZone`）
  - `matchplayers.h` - 対局者（先手・後手を人間とエンジンのどちらが指すかと名前。1人分は `seat(Turn)`）
  - `shogiclock.cpp/.h` - Game clock
  - `recordtime.h` - 棋譜の1手の消費時間と累計 `RecordTime`（ミリ秒で持ち、端数があれば表記もミリ秒まで）。対局の手を棋譜に書くときは `RecordTime::wholeSeconds` で秒の単位にする（累計を秒に切り捨て、1手の秒数は前後の累計の秒の差。ローカル対局の時計・P2P対局・CSA通信対局・棋譜の途中から続けた対局で共通。端数で棋譜欄の幅が変わらない）
  - `shoginotation.h` - 筋・段・駒の表記（漢字・全角・CSA）

- **game/**: ゲーム進行・対局管理（GUI に依存しない）
  - `matchcoordinator.cpp/.h` - 対局の司令塔（開始・着手・終局・待った・中断と再開）。手順は `record()`、対局者は `players()`。開始の条件 `MatchStartOptions`（`startoptions.h`。手順 `recordLine` の `currentPly` の局面から始め、時間の決まり `time` は `start()` が盤の手番で時計に入れる。入玉宣言（人間とエンジンの bestmove win）は対局ダイアログで選んだ持将棋ルール `jishogiRule`（「なし」は nullopt で、人間は宣言できず `LocalGameChannel::acceptsDeclaration`、エンジンの宣言は27点法）で `NyugyokuJudgement::judgeDeclaration` が判定する。game は持将棋ルールの設定を読まない）。終局の行の後で棋譜の受け手の `finishRecord` を呼んでから待機に戻す。エンジンの手番で、定跡を使う対局者（`MatchStartOptions::engine(Turn).joseki`）なら `JosekiMovePicker` で定跡手を選び、`Usi::requestBookMove` で探索させずに最低1秒置いて指す（定跡は対局を始めたときに定跡ウィンドウで開いていた定跡の写し `MatchStartOptions::josekiBook`）
  - `humanvshumanstrategy` / `humanvsenginestrategy` / `enginevsenginestrategy` - 対局者の組ごとの進め方（`MatchStrategyHost` 経由）
  - `matchending.cpp/.h` - 終局の原因 `MatchCause`（ローカル対局・通信対局・P2P対局で共通。通信対局はサーバーの結果を `CsaGameProgress::endInfo` で、P2P対局は合意した結果を `P2p::GameRules::endInfo` で直す）と、原因から棋譜の終局の行・エンジンへの gameover への対応 `MatchEnding`（ここだけ。最大手数は持将棋と書き、中断を含めて勝ち負けのない終局はエンジンに draw）
  - `matchengineseat.cpp/.h` - 対局で指すエンジン1台 `MatchEngineSeat`（枠のリース・持ち時間の出どころ・知らせの接続・終局の手順。手番は持たず、送る局面で決まる）。人間対エンジン・エンジン同士の Strategy（`GameModeStrategy::engineSeat(side)`）と通信対局（`CsaGameCoordinator`）が持つ。初期化・指し手・投了・入玉宣言勝ち・エラー（枠を取り上げられたときも）を自分のシグナルで知らせ、終局は `finish(GameOverResult)`（探索・先読みを止めて gameover と quit）で、ローカル対局・通信対局で同じ手順
  - `gamechannel.cpp/.h` - 対局相手の窓口 `GameChannel`: 盤の人間の手（`playMove(UsiMove)`。ローカル対局は司令塔の `MatchCoordinator::playHumanMove` がエンジンの手と同じく合法手を確かめて盤に指し、通信対局は `CsaGameCoordinator::sendHumanMove` がサーバーへ送る）・投了・中断・即指し・入玉宣言をローカル対局と通信対局で同じ操作にする（ローカル対局は `LocalGameChannel`、通信対局は network の `CsaGameChannel`）
  - `matchautosave.h` - 終局した棋譜の自動保存の指定 `MatchAutoSave`（ローカル対局・通信対局で同じ。形式の既定 `kifu` はここだけ。通信対局のダイアログの形式の一覧は「名前を付けて保存」と同じ表 `KifuFormats::saveTargets()` と見出し `KifuFileFilter::saveLabel()`）
  - `consecutivegameseries.cpp/.h` - エンジン同士の連続対局（局数・手番の入れ替え・次局の計画）
  - `gameboard.cpp/.h` - 対局の盤 `GameBoard`（盤を持ち、合法手を確かめて指し、指した手を棋譜の1行にする。着手の確定の知らせ `moveCommitted`）。手番は盤（`ShogiBoard::currentPlayer()`）が持つ。盤と手番だけを使う部品は `ShogiBoard` を受け取る（局面編集の `PositionEditSession` は、編集する表示の盤 `ShogiView::board()` を使う）
  - `matchrecordsink.h` - 対局の棋譜の受け手 `MatchRecordSink`（新しい棋譜 `beginRecord(MatchRecordStart)`・行の追記と取り消し・終局での確定 `finishRecord`）。ローカル対局と通信対局の司令塔が同じ受け手（`GameSession`）に書く（合成ルートが `setRecordSink()` で渡す）
  - `gamestartoptionsbuilder` - 対局開始ダイアログの値の解釈

- **kifu/**: 棋譜関連（GUI に依存しない）
  - `kifudocument.cpp/.h` - 開いている棋譜の文書 `KifuDocument`（分岐ツリー・対局情報・最後に始めた対局の対局者・上書き保存先。対局の持ち時間と開始・終了の日時は対局情報にだけ書き、別には持たない）。未保存かどうかは保存した時点との比較で求める（対局情報欄の編集は表に入れた時点で文書に入る。入力中のセル・コメント欄の未更新の本文は `Hooks`（`commitPendingEdits`・`hasPendingEdits`）で、保存・書き出し・対局の開始の前に確定し、未保存に数える）
  - `kifubranchtree.cpp/.h`・`kifubranchnode.cpp/.h` - 分岐ツリー `KifuBranchTree`（ルートを所有し、ノードは子を `std::unique_ptr` で所有する。木の形を変えられるのはツリーだけ。ノードの行 `record()` は読むだけで、書き換えるのはツリーの組み立てと、コメント・しおりを版を上げて知らせる `KifuDocument::setComment`・`setBookmark` だけ）。手を足すのは `addRecord(parent, RecordEntry)` だけ。構造の変更は型付きのシグナル `nodeAppended(parentId, nodeId)`・`childrenReordered(parentId)`・`subtreeRemoved(parentId, nodeId)`・`reset()` で知らせ、`beginBatchUpdate()`〜`endBatchUpdate()` の間は遅らせて順に出す（作り直しを含めば `reset()` 1回）。テストで手を足す・探す補助は `tests/branchtree_test_util.h`（`TestTree`）
  - `kifuexport.cpp/.h` - 形式ごとの書き出しの表（`KifuExport::lines`。GUI の保存・コピーと CLI の変換で共通）と、形式が書けるもの `capabilities`（変化・消費時間・コメント・しおり。変化を書けるか `writesVariations` は KIF・KI2・JKF・USEN で、CLI の convert-kifu の `variations_dropped`）・終局の行を書けるか `writesTerminal`。棋譜を形式で書くと失うもの `lossIn` を、保存の確認（`KifuSaveDialogs`）とコピーの知らせ（`KifuFileController`）が使う
  - `gameinfo.cpp/.h` - 対局情報の規則 `GameInfo`（対局者の見出し `playerKey`・`playerSide`・`playerName`・下手上手と呼ぶか `usesHandicapNames`・手合割の値 `handicapLabel`・`isHandicap`）。見出しの語は `gameinfokeys.h`（画面で訳す見出しは `GameInfoKeys::kTranslated`。`KifuPresentation::infoKey` が引く）、持ち時間の値（「無制限」を含む）は `TimeControlText`。書き出しの文脈 `KifuExportMetadata` は対局情報と開始局面だけで、CSA も対局情報だけから書く
  - `recordline.h`・`recordentry.h` - 1本の手順と1手の記録。手順の型は `RecordLine` だけ（読み込みの本譜・変化・書き出し・表示中の手順・対局の手順。末尾に終局の行を持てる）。終局の行は、勝敗を変えずに手番側の行にそろえる（`RecordEntry::forSideToMove`。手番でない側の反則負けは手番側の反則勝ち。対局・通信対局・CSA と JKF の +/-ILLEGAL_ACTION・KI2 の結果行で共通。投了などそろえられない行は記録した側のまま）。エンジンに送る局面は `usiPosition(ply)`（作れなければ nullopt）と、範囲の各局面を手順を1度だけたどって作る `usiPositions(from, to)`（たどれなければ局面だけ。棋譜解析と CLI の analyze-kifu で共通）
  - `moverecord.h` - 指す前の局面と指し手から棋譜の1行を作る `MoveRecord::play`（動かす駒を局面から決めて指し、表記は `KifMoveText::format`）。棋譜の読み込み（`KifLineBuilder::addMove`）と対局の着手（`GameBoard`）で共通
  - `kifuformat.cpp/.h` - 棋譜の形式（読み込み・保存・変換で共通の1つの enum）と判定。拡張子・形式・文字コード（`KifuEncoding`）・保存ダイアログの見出しは1つの表（`saveTargets()`・`savableNames()`・`readableSuffixes()`）で、保存・開くダイアログの絞り込み（`kifufilefilter` の `KifuFileFilter::forSaving`・`forOpening`。保存の見出し `saveLabel`・拡張子を補う `pathForSaving` も。QtWidgets を使わないので kifu に置く）と CLI・自動化 API の形式の選択肢もこの表から作る
  - `kifuloadparser.cpp/.h` - 棋譜の読み込み口（ファイルを1回読み、形式ごとの変換器へ振り分ける。ファイルの文字コードは形式によらず `KifuTextDecoder`（`kifutextdecoder`）の1つの規則で決める: BOM → 先頭のコメントの行の宣言（KIF・KI2 の `#KIF version=2.0 encoding=Shift_JIS`、CSA の `'CSA encoding=UTF-8`。宣言どおりに読めるときだけ従う）→ UTF-8・CP932・EUC-JP の判別）。指し手も終局も開始局面の指定もない棋譜（形式を判別できずに KIF として読んだ文など）は、GUI・CLI・自動化 API で同じく失敗にする。開始局面の規則 `startPositionProblem`（盤と持駒として成り立つか。玉の無い図と手番でない側に王手がかかった図は受け付ける）は、棋譜の読み込み・局面の読み込み（`KifuLoader::loadPositionFromSfen`。局面の貼り付け・局面集）・自動化 API の `position.set` で共通。局面集・詰将棋問題一覧のファイルの行は、検討・詰み探索に使う局面なので `SfenValidator` で厳しく読む
  - `kifparsetypes.h` - 読み込み結果（`KifParseResult`・形式ごとの `parse()` が返す `KifuParseOutcome`）と、読めない手の扱い（`ParseOptions`・`ParseReport`。ここだけで決める）
  - `kifusavefile.cpp/.h` - 棋譜の保存 `KifuSaveFile`（保存先の拡張子から形式と文字コードを決め、`write(path, generateLines)` で行を作らせて書く。行の先頭の文字コードの宣言は書き出しの文脈 `KifuExportMetadata::encoding` で書く。Shift_JIS で表せない文字があれば受け手に聞いて UTF-8 で作り直す。CLI の変換の出力は `writeLines`。既定のファイル名 `makeDefaultSaveFileName`）。保存で聞くダイアログは dialogs の `KifuSaveDialogs`
  - `kifuloader.cpp/.h` - 棋譜ファイル・貼り付けた棋譜・局面を読み（`KifuLoadParser` をワーカースレッドで）、`KifuDocument` を置き換える `KifuLoader`（合成ルートが1つ作り、全部の読み込みで使い回す）
  - `josekibook.cpp/.h` - 定跡のデータの値型 `JosekiBook`（局面のキー `SfenUtils::normalizeSfenKey()`（手数を除いた SFEN）から局面の登録を引く表1つ。同じ局面を手数違いで別々に登録したファイルは手数ごとの登録のまま持ち、引くときは同じ手数の登録、無ければ最初の登録を使う。大きな定跡も読めるよう、指し手を16ビットの番号にした詰めた形で持ち、`movesAt()` で `JosekiMove` に戻す（予想応手は `std::optional`。無ければ nullopt でファイルには "none"。値を書き換えて予想応手を残すのは `updateMove(..., NextMoveUpdate::Keep)`）。最後の定跡手を消すと登録も消える。棋譜で指された手を数えるのは `addPlayedMove`。やねうら王が予想応手 "none" の定跡手の後に先読みに使う手（次の局面で出現頻度、次に評価値が最も高い手）は `leadingMove()` で、定跡ウィンドウの表が予想応手の列に灰色で出す）と、やねうら王定跡形式の読み書き `JosekiBookFile::load()`・`save()`（ワーカースレッドから呼べる）と、定跡ファイルの拡張子・ダイアログの絞り込み（`hasBookSuffix`・`withBookSuffix`・`fileFilter`）
  - `josekibooksession.cpp/.h` - 開いている定跡ファイル（`JosekiBook` の値・変更状態・非同期の読み書き・履歴と前回のファイル）。合成ルートが1つ作り（`MainWindowComponents::josekiBook`）、起動時に `restoreSaved()` で前回の履歴を戻す。履歴と前回のファイルは変わるたびに設定に保存し、自動読込の設定も読み書きする（`autoLoadEnabled`）。定跡ウィンドウはそれを表示し、操作は ui の `JosekiBookController` が受ける。定跡を変える関数は、変えられないとき（`canEdit()` が false。読み書き中と、前回のファイルを初めて使うときに読み込む `restoreSaved`〜`startDeferredLoad` の待ちの間）は何もせず false・0件を返す。どの登録も保存はしない（定跡ウィンドウの「保存」で保存する）。エンジンの読み筋の登録 `registerEnginePv`（2手目を予想応手。登録済みの手は評価値・深さ・予想応手だけ書き換える。評価値は `valueOf` で詰みを ±(32000−手数)）、複数の棋譜から集めた手の登録 `importMoves`、別の定跡ファイルを足す `startMergeBook`（ワーカーで `JosekiBook::mergeFrom`。どれも保存はしない）
  - `josekilines.cpp/.h` - 定跡と棋譜の手順 `JosekiLines`（`expand`: 表示中の局面から定跡手を表の順にたどって棋譜の変化の手順 `RecordLine` にする。手数・1局面の手の数・比率の下限で絞り、別の手順で同じ局面に着いたら止める。`firstDeviation`: 手順が定跡を外れた最初の手）
  - `josekiimport.cpp/.h` - 複数の棋譜ファイル・フォルダから定跡に登録する手を集める `JosekiImport`（`expandPaths`・`collect`。条件 `Options`（手数・先手/後手/勝った側・変化・最小出現回数・コメント）。1局で同じ局面と指し手は1回と数える。ワーカースレッドから呼べる）
  - `josekimovepicker.cpp/.h` - 対局で指す定跡手を選ぶ `JosekiMovePicker`（条件 `JosekiPlayOptions`（`core/josekiplayoptions.h`。意味はやねうら王の定跡オプションに合わせる。保存は `JosekiSettings::playOptions1/2`）に合い、局面で指せる手から、出現頻度に比例・評価値で選ぶ・先頭の手のどれかで選ぶ）
  - `kifupresentation.cpp/.h` - 画面に出す棋譜の表記 `KifuPresentation`（指し手・読み筋・終局語。ファイルやプロトコルには使わない）。棋譜の1行は値から作る（`entryLabel(RecordEntry, 指す前の局面)`。終局は `RecordEntry::terminal` から訳す。棋譜欄の手数と分岐の印は `numberedLabel`）。`RecordEntry::displayText` は棋譜の表記（KIF の指し手・終局語）で、表示の文字列を書き戻さない。日本語の指し手は `KifMoveText`、段の漢数字は `ShogiNotation`、英語表記で移動元を付けるかは KI2 と同じ判定（`Ki2MoveNotation::annotate()` の `Annotation::sources`。合法手で決めるのでピンされた駒は数えない）。`Notation`・`Options` は `NotationStyle` の別名で、`options`・`revision` は `NotationStyle` へ転送する（表記の設定を変えるのは `NotationStyle::configure`）
  - `positioncollectionfile.cpp/.h` - 局面集ファイル（1行1局面）の規則 `PositionCollectionFile`（`parseLine`・`parse`、読めた行 `Line`）。局面集ビューア（`SfenCollectionDialog`）と詰将棋問題一覧（`TsumeCollection::parse`）で共有する。空行・`#` の行は読み飛ばし、行の語の分け方は position コマンドと同じ core の `UsiPosition::split`（`position`・`sfen`・`moves`・手数の欄は省ける）。手は USI の手だけ。position コマンドの行と startpos の行の手は指した後の局面（`Line::position()`）、SFEN だけの行の手は参考手順（`referenceMoves()`）。局面は core の `SfenValidator` で確かめ、それ以外は読めない行。ファイルを読む `readFile`（通常のファイルだけを上限まで読み、読めない理由は訳した文）と開くダイアログの絞り込み `fileFilter` も、ビューアと `TsumeCollectionSession::load` で共通
  - **formats/**: Format converters（テキストを読む。ファイルは `KifuLoadParser` が読んで渡す）
    - `csatosfenconverter`, `jkftosfenconverter`, etc. - 形式ごとに `parse(text, options)` を1つだけ持ち、`KifuParseOutcome` を返す（KIF・KI2 は `KifTextParser::parseKif`・`parseKi2`、BOD は `KifTextParser::parseBod`）
    - `kiftextparser.cpp/.h` - KIF・KI2 で共有する読み取り（行の分類・「変化：N手」の区切り・開始局面）。開始局面は最初の指し手より前の、コメントとしおりを除いた行だけから読む（コメントの語で変わらない）。読めない部分のある局面図・知らない手合割（局面図のない「その他」も）は、読み飛ばさずに棋譜全体を失敗にする（CSA の P 行も同じ）。指し手の文法は `kifmovegrammar`（KIF の手は KI2 の手＋移動元）
    - `kiflinebuilder.cpp/.h` - 全形式で共通の手順の組み立て（`KifLineBuilder`。指しながら `RecordEntry` を作る。1行は対局の着手と同じ `MoveRecord::play`）。棋譜に書かれた駒と手番（`WrittenMove`。KIF・KI2 の駒名と手番の印、CSA の駒種と符号、JKF の piece と color）が局面と違う手は、全形式で同じく `addMove` が断る
    - `*exporter` - 形式ごとの書き出し。KIF・KI2 の先頭（文字コード宣言・対局情報・盤面図）は `KifExporter::appendHeaderLines(ctx, out, HeaderOptions)` で共有し、KI2 の本譜と変化は `appendKi2Moves` の1つのループで書く
    - `kifunodereplay.cpp/.h` - 分岐ツリーのノードを指す前の局面と一緒にたどる `KifuNodeReplay`（`board()`・`mover()`・`previousTo()`・`canWrite(node)`・`advance(node)`・`branchFrom(parent)`。指せない手はノードの SFEN から取り直す）。KI2・JKF の書き出しが使う
    - `usencodec.cpp/.h` - USEN の符号表 `UsenCodec`（指し手の3文字と `UsiMove` の符号化と復号 `encodeMove`・`decodeMove`、開始局面 `encodePosition`・`decodePosition`、手順 `decodeLine`・`decodeLines`（"~" で1回だけ分けて本譜と変化の `UsenLine` にする。読めない指し手はその手前までの手と `UsenLine::unreadableMove` にし、どう扱うかは読み手の `ParseReport` が決める）、形式の判別に使う全体の形 `hasUsenShape`）。終局コードは作者の実装（mog-core-scala）に合わせて先頭の1文字で決め、反則の `i` の後ろの反則手は読むだけ（`UsenLine::illegalMove`。局面には指さない）。反則負け・反則勝ちと、符号の無い千日手・連続王手の千日手は書かない（`TerminalCodes::usen` が空。`j` は持将棋だけで、作者の実装は `j` を宣言勝ちとも読むので千日手には使わない。書けない終局は保存の確認で知らせる）。`UsenToSfenConverter` と `UsenExporter` が使う

- **analysis/**: 解析機能（GUI に依存しない）
  - `analysissession.cpp/.h` - 検討・詰み探索のセッション。検討中に表示中の局面が変わったら `KifuNavigationState::currentUsiPosition()`（開始局面からの手順）で送り直す
  - `kifuanalysissession.cpp/.h` - 棋譜解析の実行 `KifuAnalysisSession`（`start(KifuAnalysisRequest)`・`stop()`。枠 E1 のエンジンと動作状態「棋譜解析中」のリースを持ち、取り上げられたらやめる。局面は `RecordLine::usiPositions`。stop の後に bestmove を返さないエンジンは CLI と同じ10秒で打ち切り、訳した理由を出す。知らせは `plyStarted`・`plyAnalyzed`・`finished`・`errorOccurred`）。画面（結果の表・評価値グラフ・棋譜の移動）は ui の `KifuAnalysisController`
  - `engineanalysisrunner.cpp/.h` - 局面の列を1つのエンジンで順に解析する実行器 `EngineAnalysisRunner`（借りた `UsiEngineSession` を起動し、局面ごとに go infinite → 一定時間で stop → bestmove。候補手の数 `multiPv`（0 は送らない）と stop の後に bestmove を待つ上限 `replyTimeoutMs` は依頼で決める。`stop()` は解析中の局面の結果を待ち、`cancel()` は待たない）。GUI の棋譜解析と CLI の analyze・analyze-kifu で共通。1局面の結果 `PlyAnalysis` は `PlyAnalysisCollector` が multipv 1 の info と bestmove から作る（評価値は解析した局面の手番で先手から見た値 `blackScore` にし、GUI の結果の表・評価値グラフと CLI の analyze-kifu が同じ値を使う）
  - `tsumecandidatejob.cpp/.h`・`tsumeverificationjob.cpp/.h` - 詰将棋の候補1つの処理（手数の確認・唯一解の検査・要らない駒の除去）と、総予算つきの唯一解の検査（引き出し型の `MateQuerySource`。生成と CLI の検査で共通）
  - `tsumeshogigenerator.cpp/.h` - 詰将棋の生成（候補の待ち行列・問い合わせの中継・結果の集計）。条件の範囲・既定値・目標手数の奇数の規則は services の `TsumeshogiSettings::GeneratorLimits` と設定の宣言だけで決め、生成ダイアログの欄と CLI の generate-tsume の引数がそれから作る（`Settings::normalized()` も同じ範囲）
  - `tsumecollectionsession.cpp/.h` - 詰将棋集の進捗・解析待ちの管理
  - `matesearchposition.h` - 詰み探索に送る局面 `MateSearchPosition`（`forLine`: 途中局面は開始局面からの手順、開始局面は `attackerToMove` で玉の有無から決めた攻方の手番・手数1。`Position` で合わせ、文字列を組み立てない。CLI の `mate` も指し手の無い局面に `attackerToMove` をかける）

- **engine/**: USI (Universal Shogi Interface) エンジン連携（対局の仕組みに依存しない）
  - `usi.cpp/.h` - GUI から使うエンジン1台の窓口（局面の列の解析には `session()` の `UsiEngineSession` を渡す）
  - `engineslots.cpp/.h` - エンジンの枠 E1・E2 `EngineSlots`（対局・検討・棋譜解析・CSA 通信対局で共有）。リース・取り上げの知らせ・表示の消去だけを受け持つ。使う側は `acquire(owner, slot, name)` で move-only の `EngineLease`（`engine()`・`reset()`）を受け取り、持っている間だけ使う。手放すと枠を空ける（持ち主には知らせない）。ほかの持ち主が同じ枠を取った・`destroyEngines()` で全部の枠を破棄したときは、前の持ち主に `engineRevoked(owner)` で知らせる（枠にあるかは `holds(engine)`）。アプリの終了は `shutdown()` で、持ち主には同じく知らせるが、対局のエンジンの席（`MatchEngineSeat`）は `isShuttingDown()` を見て異常として扱わない
  - `usienginesession.cpp/.h` - 起動と初期化（usi〜readyok）と探索の切り替え。GUI と CLI の実行器が共有する。探索（`search(UsiSearchRequest)`）は最後に頼んだものだけを行い、探索中なら stop を送って結果を捨ててから始める（検討のやり直し・先読みの外れ・詰みの問い合わせは頼み直すだけ）。空の局面は受け付けない（false）。`stop()` は最後の依頼をすぐ終わらせて結果を届け、`cancelSearch()` は取りやめる。`startAsync()`・`probeAsync()` は値を返さず、起動の失敗は必ず呼び出しから戻った後に `errorOccurred` で知らせる（知らせる前に `quit()`・次の起動をしたら知らせない）ので、使う側は起動の途中の再入に備えない（`Usi::startEngine`・`MateOracle::start` も同じ）
  - `usiprotocolhandler.cpp/.h` - USI protocol implementation
  - `thinkinginfoformatter.cpp/.h`・`enginesearchstatus.h` - info を思考の表の行（読み筋は `PvText`）と探索状況 `EngineSearchStatus`（数値のまま。info 1行ごとに `searchStatusUpdated` で1回送り、書式は表示する側が決める）にする
  - `usilineparser.cpp/.h` - 受信行（info・bestmove・checkmate・option・id）を型付きの値にする
  - `mateoracle.cpp/.h` - 詰みの問い合わせ（go mate）の部品 `MateOracle` と答え `MateAnswer`（`UsiCheckmate` からの変換は `MateAnswer::fromCheckmate` だけ）。GUI の詰み探索（`AnalysisSession` が枠のエンジンの `Usi::session()` を借りる）・CLI の mate・詰将棋の検査と生成（接続を自分で持つ）で1つ。go mate に bestmove で答えるエンジンの答えは `Unknown` にする
  - `matequeryloop.cpp/.h` - 引き出し型の問い合わせ元 `MateQuerySource` を `MateOracle` で回す `MateQueryLoop`
  - `engineprocessmanager.cpp/.h` - Engine process lifecycle
  - `usimatchhandler.cpp/.h` - 対局での USI のやり取り（先読みの当たりは ponderhit、外れは `UsiEngineSession::search` に頼み直す）。定跡手（`requestBookMove`）は探索させず、先読み中なら止めて、待つ時間の後で同じ `moveReady` で知らせる（「すぐ指させる」は `playBookMoveNow`）
  - `matchtimesource.cpp/.h` - エンジンの思考に使う時間の出どころ `MatchTimeSource`（ローカル対局は対局時計の `ClockTimeSource`、通信対局はサーバーの残り時間を返す `CsaGameCoordinator`、時計のない場合は `FixedTimeSource`）。時計の残りから USI の btime/wtime への換算は `usigotime` の `UsiGoTimeCalculator::fromClock` だけ（加算を足してある残りは加算を、まだ引いていない経過時間はその時間を引く。先読みする側の加算は引かない）

- **network/**: CSA通信対局
  - `csaclient.cpp/.h` - CSA protocol client（接続・ログイン・対局中の行の処理と送信・キープアライブ）。対局条件は値 `CsaGameSummary`（`csagamesummary`。持時間は `CsaGameSummary::Time`）で知らせ、Game_Summary の行は読み取り器 `CsaGameSummaryReader`（`csagamesummaryreader`。1行ずつ `read()` に渡し、読み終え・上限超えを状態で返す。ソケットを持たないので単体で試せる）が読む
  - `csalogin.cpp/.h` - ログインの規則 `CsaLogin`（ID・パスワード（CSA サーバープロトコル 3.1）とゲーム名（shogi-server）の検査 `check()` が問題の種類 `Issue` を返し、通信対局のダイアログは文に直すだけ。LOGIN に送るパスワードにゲーム名と手番の希望を載せる `passwordField()`）
  - `csagamecoordinator.cpp/.h` - CSA game coordination（手順と持ち時間と手番は `CsaGameProgress`。対局の状態 `gameState()` は持たずに、接続の段階を `CsaClient::ConnectionState` から、対局中・終局後・失敗を司令塔の段階 `Phase` から求める）。棋譜はローカル対局と同じ受け手（`MatchRecordSink`）に書き、動作状態も自分で持つ（接続してから通信対局を続ける間（`sessionContinues()`。ログアウト・失敗・予定の対局数を終えるまで）は `DuringCsaConnection` で、局と局の間は棋譜を見る・保存するだけ。対局の間は、棋譜を作ってから `DuringCsaGame` にし、終局では棋譜を確定してから接続中に戻すか手放す。`MatchCoordinator` と同じ順。局と局の間の「中断」（`CsaGameChannel::breakOff`）と「新規」はログアウトする）
  - `csagameseries.cpp/.h`・`csaverdictwait.cpp/.h` - 1つの接続（再接続を含む）で続ける対局の数と終局後の再接続の待ち `CsaGameSeries`、解釈できない指し手行の後のサーバーの裁定の待ち `CsaVerdictWait`（仕様 3.4）。対局条件の受け入れの判断とログの文は `CsaGameSummary::unsupportedReason`・`conditionLines`。LOGIN のパスワードを伏せるのは `CsaClient` だけ
  - `csagamechannel.cpp/.h` - 通信対局の窓口 `CsaGameChannel`（`GameChannel` の実装。操作はサーバーへ送り、入玉宣言の成否はサーバーが判定する）
  - `gamechannels.cpp/.h` - ローカル対局と通信対局の窓口の組 `GameChannels`（`active()` が今の動作状態の窓口を返す。`game` と `network` の両方を使うのでここに置く。合成ルートが司令塔の直後に作り、`GameSession`・`BoardMoveController`・`OperationPolicy` に渡す。人間が指せるか `isHumanToMove()`・対局が進行中か `isInProgress()` は窓口だけが決める）

- **P2P対局（実験版。`ENABLE_P2P_PLAY`、既定 OFF）**: 仕様は `docs/dev/p2p-game-spec.md`、開発方針は `docs/dev/p2p-development-strategy.md`、バイト列と JSON の形（決定的CBOR・フレーム・招待文字列・IPC・表示名の表・共通の試験入力）は `protocol/` に1つだけ置く。対局の相手は招待で決める（不特定の相手との公開募集は実装仕様の第5版でやめた。DHT・GossipSub は持たない）。別のソフトウェアが互換の実装を作るための仕様書は `docs/dev/p2p-interop-spec.html`（`protocol/` の取り決めと、交渉・対局・再接続・証跡の検証の手順を1つにまとめたもの。フレーム・手順・定数を変えたら同時に直す）。後続の案（合言葉による招待・友だち一覧）は `docs/dev/p2p-invite-code-and-friends.md`。アプリ名・設定ファイル・自動化 API のソケットの名前は `src/services/appidentity.h` の1か所（P2P対局は実験版リポジトリ ShogiBoardQ-P2P で別の名前にして開発し、2026-11-11 に通常版へ取り込んで通常版の名前に戻した。開発方針 11.3節）
  - `src/game/p2p/` - ソケットも GUI も使わない部分（headless の CLI からも使う）: 決定的CBOR `P2p::Cbor`（`p2pcbor`）、条件・状態・開始合意・結果の値（`p2pprotocol`）、Ed25519 と保護付きメモリ（`p2psignature`。libsodium）、表示名の判定（`p2pdisplayname`。表は CMake が `protocol/displayname-v1.txt` から生成。`QString::fromUtf8` は先頭の BOM を消すので、判定は受け取ったバイト列で行う）、フレームの外枠と payload（`p2pframes`）、着手の検査と時計の算術と棋譜から決まる終局（`p2pgamerules`）、証跡と検証（`p2pevidence`・`p2pevidenceverifier`）、開始までの交渉 `P2p::Negotiation` と開始後の対局 `P2p::Match`（どちらも送るフレームと知らせを返す純粋な状態機械。時刻は関数で受け、`tick()` で期限を確かめる。自分の消費時間・相手の手番の観測・切断の時間の記録は値 `P2p::MatchTimer`）、結果の確定のしかた `P2p::Settlement` とジャーナル・履歴に書く語（`p2psettlement`）、ジャーナルの件の形と読み戻し（`p2precordkinds`。書くのは `P2p::Match`、読み戻すのは `RecordKinds::replay` で、保存先の復旧が使う）
  - `src/network/p2p/` - 通信プロセスの起動と IPC `PeerProcess`（`peeripc` が受けたメッセージの形をすべて確かめる）、プレイヤー鍵の保管 `P2pIdentityStore`（OS の鍵の保管庫の項目は保存先ごとに別 `keychainItemKey`）、同じ PC のプレイヤーの保存先 `P2pPlayerSlot`（1人目は `p2p/`、2人目からは `p2p-2/`…。同じ PC でアプリをいくつも動かすと、「P2P対局」を開いた順に `P2pPlayerProfile::acquire` が空いている最初の番号にする。利用者に選ばせない。2人目からは `P2pWindow` の題名と上の案内に出し、表示名も `NetworkSettings::p2pDisplayName(player)` で別に覚える）、プレイヤーの保存先 `P2pPlayerProfile`（ロックと、対局の記録・既知の相手・プレイヤー鍵の持ち主。開く `open` でジャーナルを復旧する）、対局の記録 `P2pRecordStore`（ジャーナル・証跡・履歴）、既知の相手 `P2pKnownPeers`、司令塔 `P2pGameCoordinator`（交渉と対局を盤・時計・棋譜・動作状態・証跡へつなぐ。送り先は `P2pTransport`）、通信プロセスの窓口 `P2pLobbyService`（通信プロセスの起動・停止、招待と、開いた接続の振り分け。IPC の要求と応答・通知は `PeerIpc` の型で受け渡す。自分や相手のアドレス（IP）は通信プロセスだけが扱い、ここには届かない）、対局の窓口 `P2pGameChannel`（`GameChannels::setP2pChannel`）
  - 画面: `src/dialogs/p2pwindow`（タブ: 接続と条件・招待・対局中・記録 `p2phistorypanel`・相手 `p2ppeerspanel`・設定。アドレス（IP）は出さず、プレイヤー鍵による署名は画面で「確定」と呼ぶ）、条件確認 `p2pconfirmdialog`、プレイヤー鍵 `p2pidentitydialog`、共通の文 `p2pwindowtext`。`src/ui/controllers/p2pgameuicontroller` が通信プロセス・鍵・ウィンドウを「P2P対局」を開いたときに作る（ウィンドウはロビーと司令塔を表示のために読むだけで、招待と対局中の操作は意図のシグナルで知らせる。投了と合意中断はメニューと同じ `QAction` を起こす。共通の文は `P2pWindowText`）（自動化 API を有効にして起動したときは作らない。ガイドの画像を撮るためのビルド `ENABLE_P2P_DOCS_AUTOMATION`（既定 OFF。Release ではビルドできず、インストールもできない）だけは、自動化 API から操作できる）
  - 動作状態 `DuringP2pConnection`（通信プロセスを動かしている間のうち対局していない間）・`DuringP2pGame`（対局中）。対局中は検討・詰み探索・棋譜解析・定跡手の表示を止める（アプリが手の助言をしない）
  - `peer/`（Rust、`shogiboardq-peer`）と `protocol/rust`（`sbq-protocol`）: リポジトリ直下の Cargo ワークスペース。自前のコードは unsafe と panic を起こしうる書き方を禁止（`Cargo.toml` の lints）。Linux だけ権限の制限（Landlock・seccomp-bpf など）を掛けて動く。自分から接続を始める相手は、利用者が指定した協力ノードと招待・対局の相手だけ。「直接接続を使わない」（`relayOnly`）では、宛先の規則 `AddrPolicy`（`peer/src/addr.rs`）が Transport の層で、協力ノード以外への直接の接続を断る。ローカルのネットワーク（`sbq-local-1`。同じ PC・同じ LAN）は協力ノードなしで招待だけでつながり、LAN 側（IPv4。ポートは TCP・UDP とも 48210、使用中なら空いているポート）でも待ち受けて、接続元が loopback・私設アドレスの接続だけを受け付ける
  - 試験: `tst_p2p_protocol`（形式と記録）・`tst_p2p_match`（状態機械）・`tst_p2p_coordinator`（司令塔の結合）・`tst_p2p_endtoend`（本物の通信プロセス2つ）。`cargo test`

- **style/**: ウィジェットの書体と見た目（QWidget に依存する。analysis・network の上、models の下の層。`engine`〜`network`・`models` を参照しない）
  - `applicationfonts` - 表示言語に応じた書体 `ApplicationFonts`
  - `applicationstyle`・`applicationrendering` - アプリ全体のスタイルと描画の設定
  - `buttonstyles.h`・`tablestyles.h` - ボタン・表のスタイルシート（`ButtonStyles`・`TableStyles`）
  - `plaintooltip` - 外から来た文字列（棋譜のコメント・独自の終局語・対局情報・定跡ファイルのコメント・読み筋・エンジン名・対局者名・ファイルの場所）を、HTML として読ませずに文字のまま出すツールチップ `plainToolTip`（エスケープし、空白と改行もそのまま。区切り線で分ける段は `QStringList`）。`setToolTip`・`Qt::ToolTipRole` に外から来た文字列を入れるときはこれで包む（models・widgets で共通。自前のポップアップ `GlobalToolTip` は PlainText のラベルなので包まない）。利用者に見える文字に戻すのは `shownToolTipText`（自動化 API の部品の一覧の `tooltip`）
  - `movefont.h` - 指し手・読み筋の書体の規則 `MoveFont`（表のモデルの FontRole と `KifuMoveDelegate` で共通）
  - `boardappearance` - 対局画面の外観（駒のセット・配色・見た目）の持ち主 `BoardAppearance`（すべての盤面が共有する。`pieceStyle`・`colors`・`visuals` と、まとめた値 `AppearanceState`（`appearancestate.h`）の `state()`・`apply()`。変わったものだけを `pieceStyleChanged`・`colorsChanged`・`visualsChanged` で知らせる）。保存は `appearancesettings`（`AppearanceSettings`。外観ダイアログの大きさ・開いていたタブ・駒の絞り込みも）
  - `boardcolors.h`・`boardvisuals.h` - 配色 `BoardColors`（設定のキーと外観ダイアログの表示名の表 `fields()`）と、木目・駒の影・駒の大きさ `BoardVisuals`
  - `pieceimageprovider` - 駒の画像の解決とキャッシュ `PieceImageProvider`（駒のセットは持たず、今の駒のセットは `BoardAppearance`）
  - `piecestylecatalog` - 駒のセットの表 `PieceStyleCatalog`（ID・分類 `Family`・表示名・画像の場所）。駒のセットを足すときはこの表に1行足す（外観ダイアログの絞り込みの番号は `Family` の値）
  - `boardappearancecatalog` - 部品ごとの見本（将棋盤・駒台・対局者情報・背景）と、おすすめの組み合わせ `combinations()`（駒のセットと部品の見本の名前）。`boardcolorpresets` は盤・駒台・背景のおすすめ配色（「詳細」の配色のコンボ）

- **board/**: 盤面編集・操作
  - `boardinteractioncontroller.cpp/.h` - 盤のクリックを移動元・移動先にし、選択・直前の手・合法な移動先のハイライトを出す（合法な移動先を出すかは保存した設定から始め、`setLegalMovesVisible` で保存する。主画面・詰将棋を解く画面・定跡手のダイアログで共通。直前の手は `showLastMove`、つまんだ駒を放すのは `cancelInput`）。クリックを手番の側の手にする処理 `moveFromClick()`（成り・不成の両方が合法なら、作るときに渡した成りの選び方の関数で尋ねる）もここにあり、主画面と詰将棋を解く画面の行き先が使う。ドラッグは行き先の `submit()` から戻った後に終える
  - `positioneditrules.cpp/.h` - 局面編集の規則 `PositionEditRules`（駒の移動 `moved` は `Position` を変える関数。盤へは `apply`・右クリックの成・不成・先後の巡回 `cyclePiece`・盤を空にする `clear` が渡す。巡回と空にする局面・置ける形の判定は .cpp の中だけ）
  - `boardinputroute.h` - 盤で指した手の行き先 `BoardInputRoute`（つまめるか・局面編集中か・着手の処理・局面編集中の右クリック `cycle`。主画面は `BoardMoveController`、詰将棋を解くダイアログは `TsumePlayDialog`、定跡手のダイアログは `JosekiMoveBoard`。局面編集の `isEditing`・`cycle` は既定で何もしない）
  - `positioneditsession.cpp/.h` - 局面編集（開始・終了・編集中の盤の操作（全駒を駒箱へ・平手・詰将棋・手番の入れ替え。メニューは `MenuActionsWiring` がつなぐ）・編集中の着手 `applyEditMove` と右クリックの巡回 `cycleEditPiece`。どちらも盤の行き先 `BoardMoveController` から渡され、盤の操作は局面を変えない）。駒箱の先後の正本もここ（`pieceBoxSide()`。盤の切り替えのボタン `pieceBoxSideClicked` を受け、盤は表示するだけ）。「編集中か」は動作状態 `DuringPositionEdit` だけで決まる

- **ui/**: UI層
  - **presenters/**: 表示への反映
    - `notificationpresenter`（ui・app の知らせ（情報・警告・エラー）の唯一の出し手。静的な `NotificationPresenter::information`・`warning`・`critical` で出し（閉じるのを待たないなら `Wait::No`）、自動化 API の `ScopedCapture` の間は記録だけする。文は PlainText。ui・app で `QMessageBox` の静的関数を使わず、`ErrorBus` にも流さない（`ErrorBus` は core の知らせを受けて出すだけ。`tst_structural_kpi`）。確認の問いは `DialogUtils::confirmAction`・widgets の `UnsavedChangesPrompt`。dialogs・widgets の知らせは `DialogUtils::warning`・`critical`・`information` で出す（文は PlainText。`tst_structural_kpi`））, `enginedisplaypresenter`（エンジンの表示の持ち主。検討の表示は `AnalysisSession` の知らせだけから決める。情報帯の手番の印は文書の対局者と通信対局の `showMatchEngine` から決め、検討・詰み探索・棋譜解析が始まったら次の対局まで出さない）, `playerinfopresenter`（盤の対局者名。決め方（対局中は対局者、対局していなければ対局情報の名前、どちらも空なら「先手」「後手」）は `PlayerInfoPresenter::boardName` の1つで、読み筋盤（`PvBoardController::showPvBoard`）も同じ名前を出す）, `evaluationgraphpresenter`（評価値グラフの点の唯一の書き手）, `kifudisplaypresenter`（棋譜欄・分岐）, `boardsyncpresenter`（盤と手番）, `piecesoundplayer`（駒音 `PieceSoundPlayer`。`GameBoard::moveCommitted` と詰将棋対局の着手で鳴らす。音質の加工は `piecesoundprocessor`、音質の値と設定は services の `piecesoundtone.h`・`AppSettings`。駒音の設定ダイアログは値をシグナルで知らせるだけで、`DialogLauncher` がプレイヤーに結ぶ）, `candidatearrowpresenter`（検討・対局のエンジンの読みと、定跡ウィンドウで開いている定跡の定跡手（緑。「表示」→「盤に定跡手を表示」）の矢印。対局の矢印は E1・E2 の思考の表の最新の最善の読み筋から作り、探索中か・先読みかだけをエンジンに聞く。どれを出すかは ui/controllers の `BoardArrowActions` の `QAction` だけを読む）, etc.
  - **controllers/**: 利用者操作の処理
    - `boardarrowactions`（盤の矢印を出すかの4つの切り替え `BoardArrowActions`（手番側・先読み側・検討・定跡手の checkable な `QAction`）。設定から戻し、変わるたびに保存する。思考ドック・検討タブ・定跡ウィンドウのチェックと「表示」メニューの「盤に定跡手を表示」はこれに結ぶだけ）, `boardmovecontroller`（主画面の `BoardInputRoute`。行き先ごとに手を処理する。対局中の手は対局中の窓口（`GameChannels::active()`）の `GameChannel::playMove` へ渡すだけで、ローカル対局と通信対局で分けない）, `considerationuicontroller`（検討タブの操作を `AnalysisSession` へ伝えるだけ）, `josekiwindowcontroller`（定跡ウィンドウ（widgets の `JosekiWindow`）を作って表示中の局面に合わせ、表示中の手順が定跡を外れた手を出す。「着手」の定跡手を指し、指せるかの確かめと指せなかったときの知らせ（`NotificationPresenter`）もここで行う。「棋譜に展開...」は条件のダイアログを出し、`JosekiLines::expand` の手順を `BranchTreeEditController::addVariationLines` で変化として足す。定跡そのものの操作は、ここが作る `josekibookcontroller`（`JosekiBookController`。ウィンドウの意図のシグナルを受け、ファイルは `JosekiFileController`、定跡手の追加・編集・削除は `JosekiEditController`、マージは `JosekiMergeController`（「現在の棋譜から」は表示中の手順）に渡す。ドロップの振り分け、ドックを閉じるときの確認と題名の未保存の印も受け持つ）。定跡の表示（「■ 停止」）・「盤の手を登録」の checkable な `QAction` を持ち、正本にする（「盤に定跡手を表示」の正本は `BoardArrowActions`）。定跡ウィンドウのボタン・チェック、盤の矢印、盤の手の登録はこの `QAction` を読む）, `josekiregistrationcontroller`（思考の表・検討の表・棋譜解析の結果の表の右クリックで読み筋の最初の手を定跡に登録し、「盤の手を登録」がオンなら盤で指した手（`BranchTreeEditController::boardMoveRecorded`）と対局で指された手（`GameSession::gameMoveRecorded`。人間・エンジンとも、ローカル・通信対局とも）を登録する。定跡を変えられないとき（`JosekiBookSession::canEdit`）は押せない）, `menuwindowcontroller`（メニューウィンドウの分類と、ツールバーの「メニュー」でドックを出し入れする `toggleWindow`）, `pvboardcontroller`, `dialoglauncher`（結果を受ける部品のないダイアログ（設定・情報・詰将棋）を開く。成りの選択は合成ルートが `PromoteDialog::ask` を盤の操作に渡す。結果を使う部品があるダイアログはその部品が開く: 対局開始は `GameSession`、通信対局は `CsaGameUiController`、棋譜の貼り付けと局面集は `KifuFileController`）, `kifuclipboardcontroller`（棋譜・局面のコピー `KifuClipboardController`）, `kifufilecontroller`（棋譜の開く・保存・貼り付けと局面集ビューア（`openSfenCollection`。開いている間は1つ）。読み込みは kifu の `KifuLoader`、保存で聞くダイアログは dialogs の `KifuSaveDialogs`）, `imageexportcontroller`（盤・評価値グラフの画像のコピーと、形式を選んだ保存 `ImageExportController`。決まった場所への PNG の保存は `src/ui` の `screenshotservice`。F12 の撮影と自動化 API も使う）, etc.
    - `csagameuicontroller` - 通信対局の画面 `CsaGameUiController`（通信対局のダイアログ（`openGameDialog`。開いて結果を受け、対局を始める）・待機ダイアログ・結果のダイアログ（結果・終局の原因の表示名 `resultText`・`causeText` もここ）・ステータスバー・盤の反転だけ。評価値・エラー・通信ログは合成ルートの `connectGame()` がつなぐ。盤の手は `GameChannel` を通る）
    - `commentcontroller` - コメント欄 `CommentController`（コメント欄の表示と、別の手へ移る前の未更新のコメントの確認。確認は棋譜・定跡と同じ `UnsavedChangesPrompt` で、保存は書いていた手に確定する。コメント欄（`CommentEditorPanel`）は未更新か・確定・破棄を受けるだけ）
    - `kifunavigationcontroller` - 棋譜ナビゲーション（ボタン・分岐）。利用者の操作（`select*`・`on*Clicked`。棋譜解析の結果の行・グラフの解析の点は `selectPlyOnLine`）は、どれも移る前の確認（移ってよい状態か `setBrowseCheck`・未更新のコメント `addLeaveGuard`）を通し、確認のダイアログの後は状態を確かめ直す。確認を通さない `go*` はプログラムからの移動だけに使う。局面送りの6つの操作は `QAction` の組（`action(Step)`）で1つだけ持ち、棋譜欄と評価値グラフのボタンの列（widgets の `StepButtons`）を `bindButtons()` で結ぶ（ボタンは向きの記号だけで、説明（ツールチップ）と有効・無効は QAction に従う。切り替えは `setStepActionsEnabled()`）
  - **policy/**: 動作状態からの規則
    - `uistatepolicymanager` - 動作状態ごとのメニュー・ボタンの有効/無効（コンパイル時の表）
    - `operationpolicy` - 盤の手の行き先 `boardInput()`（`BoardInput`。動作状態ごとの規則はここだけ）・棋譜を移ってよいか `canBrowseRecord()`（`UiStatePolicyManager` の表の `WidgetNavigation` の行。待機中・検討中・詰み探索中・通信対局の局と局の間）・変えてよいか `canEditRecord()`（移ってよく、対局の棋譜を書いていない。検討中・詰み探索中も分岐の編集・定跡の展開はできる。盤の手を記録するのは待機中だけ）・人間が指せる手番か・対局が進行中か。棋譜を移る・変える入口（棋譜欄・分岐候補欄・分岐ツリー・評価値グラフ・定跡ウィンドウ・棋譜解析の結果・自動化 API の `kifu.goto`）はすべてこの2つを通す
  - **docks/**: ドック
    - `dockregistry` - ドックの表 `DockRegistry`（`DockId`・`DockSpec`）。12個のドックをこの表だけから作り（`createAll()`）、規定の場所に置き（同じ場所のドックは表で前のドックのタブに重ねる。起動時と `resetToDefault()` で同じ配置）、浮動・表示・位置を保存・復元する（`saveStates()`・`DockSettings::DockState`）。ドックを足すときは `DockId` と表に1行足す
    - `docklayoutmanager` - 名前付きレイアウト・起動時のレイアウト・ドックの固定（ドックは `DockRegistry` から取る）

- **views/**: 盤の描画と入力（`shogiview.cpp/.h`）と、盤を使う表示の部品（読み筋盤・局面集ビューア・詰将棋を解く画面・外観の見本の盤と拡大・縮小・回転・局面送り `boardviewerwidget`（`BoardViewerWidget`）、詰将棋問題一覧の小さな盤 `tsumepositionpreview`（`TsumePositionPreview`））。盤・マス・駒台のセル・座標の文字・対局者カードの位置は `boardgeometry`（`BoardGeometry`。マスの大きさ・反転・筋と段の数・駒台の大きさ（盤は2列4段。詰将棋問題一覧のプレビュー `TsumePositionPreview` は持ち駒を1列に並べて枚数を横に書くので幅を広く段を多くする）から1度だけ計算する変えない値で、すべてウィジェット座標。`ShogiView::boardGeometry()`。マスの大きさの範囲は core の `BoardSquareSize`。`ShogiView` は設定を読まず、持ち主が `setSquareSize()` で渡す（本体の盤は `MainWindowAppearanceController` が `AppSettings::squareSize` を読み書きする））、盤に重ねる対局者カード・編集終了ボタン・駒箱の先後切り替えは `boardchromewidget`（カードは1側1枚の `playercard`（`PlayerCard`。手番の札・名前・時計を子に持つ）で、`chrome()->card(Turn)` で触る）、局面によらない部分の画像は `boardstaticlayer`（筋・段の文字は `BoardStaticLayer::drawCoordinates` で、盤と詰将棋問題一覧のプレビューで共通）、ドラッグは `shogiviewinteraction`、ハイライト（層 `BoardHighlightLayer` ごとのマスの値。色は層で決まる）と矢印（`ShogiViewHighlighting::Arrow`）は `shogiviewhighlighting`（どちらも `ShogiView::highlighting()` から直接渡し、`ShogiView` は転送しない）。駒箱の先後は表示だけを持つ（正本は board の `PositionEditSession`）
- **automation/**: 自動化 API（`ShogiBoardQ --automation`）。引数の表・エラーの分類などの共有部品は headless/（設計: `docs/dev/mcp-server.md`）
  - `automationserver.cpp/.h` - QLocalServer で改行区切り JSON-RPC 2.0 を待ち受け
  - `automationdispatcher.cpp/.h` - JSON-RPC 解析とメソッド表（GUI 非依存）。メソッドを呼ぶ前に引数を表で検査し（知らない引数は拒否）、既定値を補う。エラー応答（`makeError()`）は `error.code` と `error.data.slug` を分類から決める
  - `automationcommands_*.cpp` - メソッドの実装。メソッド `x.y` は `automationcommands_x.cpp`（名前空間ごとに1ファイル。登録の一覧と共有の読み取りは `automationcommands.cpp`）、`automationactionpolicy` は QAction 許可リスト。`widget.showDock` は `dock.configure` の `operation=show` と同じ処理（`AutomationCommands::showDock`）
  - `automationcontext.h` - メソッドの実装に渡すアプリの状態と部品 `AutomationContext`（動作状態は `activity` から読む）
  - `automationwidgets.cpp/.h` - ウィンドウ・部品の名前での引き当てと、操作できるかの検査（`requireWidget`・`requireInteractive`）
  - `automationparams.h` - 範囲が画面の状態で決まる引数・操作によって必須になる引数の取り出し `AutomationParams`
  - `automationinteractions.cpp/.h` - 応答の後に行う UI 操作の予約 `AutomationInteractions::queue(QueuedInteraction{target, check, perform}, response)`。`check` は予約の時点と実行の直前の2回走る（メソッドは同じ検査を予約の前に書かない）。結果（pending・running・done・failed と分類つきの error・cancelled）を記録して `ui.interaction` で返す（応答に `"queued": true` と `"interaction"` の番号）。UI を操作するメソッドはこれで予約し、`AutomationDeferredCall` を直接使うのは `app.quit` だけ。実行時の失敗は握りつぶさない。MCP サーバーは `AppClient.call_queued()` で実行を待つ
  - `automationinput.cpp/.h` - 通常のマウス入力 `AutomationInput::sendClick()`・`sendDrag()`（盤・表のセル・分岐ツリー・ドックのタイトルバー）
  - `automationwidgetadapters.cpp/.h` - 部品の型ごとの読み（`describe()`・`widget.text`）と書き（`writer()`・`widget.setValue`）の表 `AutomationWidgetAdapters`。型を足すときは表に1行足す
- **headless/**: CLI・自動化 API・MCP サーバーが共有する GUI 非依存の部品（core〜analysis だけを使う。`tests/tst_layer_dependencies.cpp`）
  - `errorkinds.cpp/.h` - エラーの分類 `ErrorKind` と、語（slug）・JSON-RPC のコードの表（`ErrorKinds::slug()`・`rpcCode()`・`toJson()`。数値は `AutomationErrorCode`）、分類つきの失敗 `Failure`（分類・文・対処）。CLI（`CliOutput::fail(ErrorKind, ...)`・`fail(Failure)`）・自動化 API（`AutomationError(ErrorKind, ...)`・`AutomationError(Failure)`）・MCP サーバーで同じ失敗は同じ語にする。分類を足すときは enum と表に1行ずつ足す
  - `paramspec.cpp/.h` - 引数の表の部品 `ParamSpec`（名前・型・必須・範囲・既定値・選択肢・説明）と `OperationSpec`。JSON Schema の出力（`ParamSpecs::inputSchema()`）と検査（`validate()`）。CLI・自動化 API・MCP で共有する
  - `automationmethodspecs.cpp/.h` - 自動化 API の全メソッドの引数の表 `AutomationMethodSpecs`。メソッドは補われた値を読み、`AutomationParams` は範囲が画面の状態で決まる引数にだけ使う
  - `automationendpoint.cpp/.h` - 自動化 API の接続先 `AutomationEndpoint`（既定のソケットの場所・エンドポイントファイル・1メッセージの上限 `kMaxMessageBytes`）。自動化 API のサーバーと CLI の `environment` が共有する
  - `outputpath.cpp/.h`・`inputpath.cpp/.h` - 書き出し先の事前検査 `OutputPath::check`（絶対パス・ディレクトリでない・上書きの許可・置き場所。CLI の出力先と `kifu.save` で共通）と、読むファイルの事前検査 `InputPath::check`（絶対パス・存在・通常のファイル。CLI の `input_path`（`KifuConversionService`）と `kifu.load` で共通）。MCP の `paths.py` も同じ分類
  - `kifuconversionservice`, `enginecatalog`, `matesearchrunner`, `tsumeverificationrunner`（詰みの問い合わせは `MateOracle`） - CLI のコマンドが使うサービス（局面の検証は core の `SfenValidator`、その JSON は `sfenvalidationjson`）。今は CLI だけが使う（自動化 API にはエンジンを受け取るメソッドがない）が、core〜analysis だけを使う部品なので headless に置く。`KifuConversionService` は失敗を `Failure` で返し、エンジンの実行器（`MateSearchRunner`・`TsumeVerificationRunner`）はエンジンの失敗を `errorOccurred` で知らせる
- **cli/**: `shogiboardq-cli`（GUI を起動しない JSON 出力コマンド。`mcp/` の MCP サーバーが呼び出す）
  - `clicommandspecs.cpp/.h` - 全コマンドの引数の表 `CliCommandSpecs`（オプション名は引数名の `_` を `-` にしたもの）。オプションの登録・使い方の文・コマンドラインから検査済みの JSON への変換（真偽値はフラグ、文字列の配列は空白区切り）をこの表から行う。コマンドの名前・実装・出力の種類（JSON か JSON Lines のイベントか）は `main.cpp` の表の1行で決め、`CliCommands::run()` が引数を表で検査してから、検査済みの JSON をコマンドに渡す（コマンドは `resolvePosition(params)`・`resolveEngine(params)` で解決する）。指し手の JSON は CLI と自動化 API で共通の `headless/recordjson`（`RecordJson::entry`・`moves`。開始局面のコメント・しおりは行にせず `addOpening` で `opening_comment`・`opening_bookmark` に書く）
  - `environment` コマンドは自動化 API の既定のソケット・エンドポイントファイル・設定ファイルの場所、1メッセージの上限（headless の `AutomationEndpoint`。自動化 API のサーバーも同じ値を使う）、エラーの分類の表、読み書きできるファイルの拡張子（棋譜の読み込み・変換の書き出し・アプリの保存 `kifu_save_suffixes`（保存の表 `KifuFormats::saveTargets`）・盤面画像）を出す。MCP サーバー（`mcp/shogiboardq_mcp/environment.py`）はこれを読み、場所や上限・語を Python に写さない
  - `boardimagerenderer.cpp/.h` - 盤面画像 `BoardImageRenderer`（render-board。`ShogiView` を画面に出さずに描くので、cli で views を使うのはこれだけ）
  - `describe` コマンドは CLI と自動化 API の表を JSON Schema で出す。MCP のツールのスキーマはその出力を保存した `mcp/shogiboardq_mcp/specs.json` から作る（引数名は CLI・自動化 API・MCP で同じ）ので、引数を変えたら `python3 -m shogiboardq_mcp.specs build/shogiboardq-cli`（mcp/ で）で作り直す（`mcp/tests/test_param_specs.py` が古さを検査する）。CLI は `--params-json '<JSON>'` でも引数を受け、MCP サーバーはこれで渡す。以前のオプション名は旧名（`ParamSpec::alias`・`aliasFlag`）で受け付ける
- **widgets/**: Custom Qt widgets
  - ドックのパネル（`thinkingpanel`・`gameinfopane`・`recordpane`・`considerationpanel`・`usilogpanel`・`csalogpanel`・`commenteditorpanel`・`analysisresultspanel`・`branchtreeview`・`menuwindow`・`josekiwindow`）はすべて `QWidget` の派生で、部品をコンストラクタで作りきる（USI・CSA の通信ログは共通の基底 `logpanel` の `LogPanel`。ログ・折り返し・最新へ・コピー・消去・コマンドの欄。同じ設定を使うパネル（CSA 通信ログのドックと待機ダイアログ）は、文字の大きさと折り返しの変更を互いに合わせる）。`DockRegistry` はパネルそのものをドックに入れる（`buildUi()`・`containerWidget()` のような、外から後で中身を作らせる・取り出す公開の関数は持たない）
  - `thinkingpanel`・`thinkingtableview`（思考の表。対局と検討タブで共用）・`pvtableview`（読み筋の表 `PvTableView`。数値の列・「盤面」ボタン（`boardRequested`）・読み筋の描き方・行の高さを受け持ち、思考の表の基底で棋譜解析の結果の表にも使う。行の右クリックのメニューの項目は受け手が `rowContextMenuAboutToShow(row, menu)` で足す）・`engineinfowidget`（思考の表の上の情報帯 `EngineInfoWidget`。手番の印・エンジン名と「項目名 値」を1行に並べる。探索状況の書式はここで決める。項目の幅は項目名と見本の値を実際に描く書体で測るだけで決め、届いた値では変えず、余りはエンジン名、さらに余れば探索手の定跡の表記に使うので、同じ幅の帯はどの項目も同じ位置に揃う。想定より長い値は `ElideLabel` が省略しツールチップで読める。1行に収まらなければエンジン名と指し手の行と数値の行に折り返す。高さは固定で親の最小の高さになるので、隠れている間は折り返さず、行を増やすのは画面に描いた後にする（タブの裏の帯は `isVisible()` でも見えている扱いのまま起動時の仮の幅で残る）。部品は大きさを固定して置くので、値の更新でレイアウトの再計算は起きない）, `gameinfopane`（対局情報。中身は `models/gameinfoeditmodel`。画面に出す文字（項目名・内容の表記と空の内容の案内文）はモデルの `GameInfoEditModel::PresentedTextRole` の1つで作り、表の描画・項目名の列幅・読み上げ・ツールチップはそれを読む）, `recordpane`, `considerationpanel`（検討タブ。条件の値 `ConsiderationSettings`（services の `analysissettings.h`。範囲もそこで決める）を正本として持ち、保存は `AnalysisSettings::consideration`（`StructSetting`）、エンジンの選択は `EngineSelector::saveSelection`。開始/中止のボタンは実行の状態 `RunState` で決まる）, `evaluationchartwidget`（評価値グラフ。`EvaluationSeries` を描くだけ。表示の設定は値 `EvaluationChartSettings`（`AnalysisSettings::evalChart`）、軸は `evaluationchartrange` の `EvaluationChartRange::compute`。「表示設定…」は `settingsRequested` を出し、`DialogLauncher` が dialogs の `EvaluationChartSettingsDialog` を開く）, `analysisresultspanel`（棋譜解析の結果の表。列は `KifuAnalysisListModel::Column`、モデルは `setModel()` で1度だけ結ぶ。ドックを出すのは `KifuAnalysisController`）, `branchtreeview`（分岐ツリーの図 `BranchTreeView`。`QGraphicsView` の派生でドックの中身そのもの。`setTree()` でツリーの変更の知らせを自分で受け、手順の末尾に1手足しただけならその手だけを描き足し、それ以外は作り直す。外とは `KifuBranchNode` の ID だけでやり取りする（`setCurrentNode(nodeId)`・`nodeActivated(nodeId)`・`nodeContextMenuAboutToShow(nodeId, menu)`・`refreshNode(nodeId)`）。行・段・折りたたみの配置は `branchtreelayout`）・`branchnodeitem`（図の1手（開始局面を含む）の図形 `BranchNodeItem`。ノード ID・行・手数を型付きのメンバで持ち、`BranchTreeItemRoles` は自動化 API のためだけに付ける）
  - `tableheaderview` - 表の見出し。利用者が境目をドラッグした列だけを `sectionResizedByUser` で知らせる
  - `columnwidthmemory` - 利用者が決めた列幅の覚えと保存 `ColumnWidthMemory`（表の見出しに付ける。規則は1つ: 列ごとに 0 は自動、正の値は利用者が決めた幅で、表は `widthOr(column, 自動の幅)` で使う。伸び縮みする最後の列は覚えない。保存は `ListSetting<int>`。同じ設定を使う別の表が変えたら `reload()` で読み直す）。思考の表（対局の2つのエンジンの表は1つの列幅で、`ThinkingPanel` が `userColumnWidthChanged` を `reloadColumnWidths` に結ぶ）・棋譜解析の結果の表・対局情報・定跡ウィンドウが使う
  - `panelbutton` - ドックのパネルの小さなボタン（アイコン・短い文字）の見た目と大きさ `PanelButton::setup`（32×24 を基本に文字が収まるまで広げ、書体が変われば測り直す）。パネルはボタンの大きさを自分で決めない
  - `stepbuttons` - 局面送りの6つのボタンの列 `StepButtons`（棋譜欄は縦の ▲▼、評価値グラフは横の ◀▶。説明と操作は ui の `KifuNavigationController::bindButtons` が付ける）
  - `pvmovestrip` - 読み筋の手を押せる語にして折り返して並べる帯 `PvMoveStrip`（読み筋盤の上部。1語が1局面で先頭は開始局面。語の部品の名前は `pvMove<番号>`。今の局面の語を盤の移動先と同じ色で塗り（checked）、そこまでの語は濃く、この先の語は薄く出す。太さを変えないので局面を送っても語は動かない。押された語は `positionRequested` で知らせるだけで、表示する局面は受け手が `setCurrent` で返す。手のない語（注記・投了）は押せない `QLabel`）
  - `fontsizehelper` - A-/A+ の文字の大きさ（範囲・保存・変更の通知）
  - `fontsizecontrol` - A-/A+ のボタンの組 `FontSizeControl`（文字・見た目・大きさ・ツールチップと、範囲の端で押せなくする処理はここだけ。大きさはドックのパネルなら `PanelButton`、ダイアログと定跡ウィンドウなら `Sizing::Dialog`）。ドックのパネル（思考・検討・棋譜・対局情報・コメント・通信ログ・棋譜解析の結果・定跡ウィンドウ・メニューウィンドウ）が使う。ボタンは役割のプロパティ `FontSizeControl::kRoleProperty`（値は `decrease`・`increase`）で探せる
  - `editactions` - 元に戻す〜貼り付けの5つの `QAction` の組 `EditActions`（標準ショートカット・アイコン・実際のショートカットを添えたツールチップはここだけ。ボタンは `createButton()` で、見た目と大きさもここで決める（`PanelButton::setup` の既定。使う側は色を選ばない）。何をするか・有効にするかは使う側が決める）。コメント欄（`CommentEditorPanel`）と対局情報（`GameInfoPane`）が使う
  - `actionbinding` - パネルのチェックと、呼び出し側が持つ切り替えの `QAction` の結び付け `ActionBinding::bindCheckable`（QAction は必須で正本。チェックは今の値から始め、どちらを変えてももう片方が合う）。思考ドック（`ThinkingPanel`）・検討タブ（`ConsiderationPanel`）・定跡ウィンドウ（`JosekiWindow`）のチェックが使い、パネルは QAction を作らない（盤の矢印は ui の `BoardArrowActions`、定跡の表示と盤の手の登録は `JosekiWindowController` が作る）
  - `recentfilesmenu` - 「履歴」ボタンのメニュー `RecentFilesMenu`（ファイル・区切り・「履歴をクリア」、空なら「（履歴なし）」。選ばれた項目は `fileTriggered(QString)`・`clearTriggered()` で知らせる）。定跡ウィンドウ（`JosekiWindow::recentFileRequested` で知らせ、`JosekiFileController::openRecent(path)` が開く）と局面集ビューアが使う
  - `dialogfontscale` - ダイアログの A-/A+ と保存 `DialogFontScale`（ダイアログで共通。各部品を元の大きさからの差で拡大・縮小する。通信対局の待機ダイアログの通信ログの窓はパネルの A-/A+ を使い、大きさだけを `DialogUtils::rememberDialogSize` で保存する）
  - `unsavedchangesprompt` - 未保存の変更の確認 `UnsavedChangesPrompt`（保存・破棄・キャンセル。棋譜・定跡・コメント欄で共通）
  - `engineselector` - 登録エンジンを選ぶ部品 `EngineSelector`（呼び出し側が置いたコンボ・「設定」ボタン・未登録の案内に結び、一覧の読み込み・保存した選択の復元と保存（`EngineSelectionSettings::SelectionKeys`）・未登録のときの無効化を受け持つ）。設定ダイアログは dialogs にあるので開かず、`settingsRequested(EngineEntry)` で受け手に頼む（受け手は `ChangeEngineSettingsDialog::showFor(engine, parent)` で開く）。棋譜解析・詰み探索・詰将棋生成・詰将棋問題一覧・CSA通信対局のダイアログ、検討タブ、対局開始ダイアログの対局者の欄が使う
  - `playersetupwidget` - 対局開始ダイアログの対局者1人の欄 `PlayerSetupWidget`（コンボの先頭が「人間」（`EngineSelector` の内蔵の項目）で、その後に登録エンジン。人間なら名前の欄、エンジンなら「エンジン設定」ボタンと「定跡を使う」・「条件…」（条件のダイアログは dialogs の `JosekiPlayOptionsDialog`。定跡を開いていなければ押せない））。選択は `PlayerChoice`（`game/startgamedialogdata.h`）で読み書きし、保存したエンジンが削除されていれば人間に戻す
  - `timecontroleditor` - 片方の対局者の持ち時間・秒読み・加算の欄 `TimeControlEditor`（`TimeControlSide` で読み書き。秒読みと加算は排他で、相手側の欄にも `byoyomiEntered`・`incrementEntered` で知らせる）
  - `tablemodelutils.h` - 表へのモデルの付け替え `TableModelUtils::setModel`（Qt が置き換えた選択モデルを解放する）
  - `josekiwindow` - 定跡ウィンドウ `JosekiWindow`（ドックのパネル。表示中の局面の定跡手を表に出し（局面の SFEN とコピーは局面の行の `ElideLabel` で、定跡の表の位置を動かさない。定跡に登録された SFEN は SFEN のツールチップ）、ファイル・定跡手の追加/編集/削除・マージ・ドロップ・棋譜への展開・着手は意図のシグナルで知らせるだけ。ダイアログは出さない（受けるのは ui の `JosekiBookController`・`JosekiWindowController`）。表の並べ替えだけはセッションに直接書く。列幅は利用者がドラッグしたときに保存する）・`josekitableview`（定跡手の表 `JosekiTableView`。ボタンの列・ダブルクリック・右クリックのメニュー（着手・編集・削除・先頭へ/上へ/下へ移動・出現頻度/評価値の順に並べる・コピー）を行の操作 `rowActionRequested` にする。並びは定跡ファイルに書く順で、盤の矢印の番号・対局の「表の先頭の手」も同じ。`JosekiBook::moveTo`・`sortMoves`）
  - `settingsformbinding` - 入力欄と設定の宣言（`Setting<T>`）の対応の表 `SettingsFormBinding`（`add()` を1度書けば `load()`・`save()`・`reset()`（既定値に戻す）を同じ表で行う。対局開始・詰将棋生成・棋譜解析・詰み探索・通信対局・定跡の展開などのダイアログが使う。棋譜解析の開始・終了手数のように、欄の上限を後で決めてから戻す値は表に入れない）
- **dialogs/**: Dialog implementations (with co-located .ui files)
  - `josekiexpanddialog`（定跡を棋譜に展開する条件）・`josekiimportdialog`（複数の棋譜・フォルダから定跡に登録する条件と進み具合。集めるのは `JosekiBookSession::startImport`。定跡ウィンドウへのドロップでも開く）・`josekibookmergedialog`（別の定跡ファイルを足す条件）・`josekianalysisregisterdialog`（棋譜解析の結果をまとめて登録する手数と手番）・`josekimergedialog`（表示中の手順・棋譜ファイルの本譜の手を選んで登録。「登録済」は定跡に手があるか（`hasMove`）で、登録は `importMoves`）。定跡ウィンドウの「盤の手を登録」（保存しない設定で、起動するとオフ）の登録は `JosekiRegistrationController` が行う
  - `josekimovedialog`・`josekimoveinputwidget`・`josekimoveboard` - 定跡手の追加・編集。指し手と予想応手は盤で指すか（`JosekiMoveBoard`。`BoardViewerWidget` と `BoardInteractionController` をまとめた `BoardInputRoute` の行き先で、指した手を `movePlayed` で知らせるだけで局面は進めない。大きさと向きは `JosekiSettings::josekiMoveBoardSquareSize`・`josekiMoveBoardFlipped`）、局面の合法手を画面の表記（`KifuPresentation::move`。定跡ウィンドウの表と同じく表記の設定に従う）で並べた一覧から選ぶ（`JosekiMoveInputWidget`。一覧のオブジェクト名は `<name>Combo`。予想応手は着手後の局面の合法手で、先頭が「なし」。「指し直す」は `<name>Retry`）。選んだ手の正本は一覧だけで、盤の局面は一覧の手から `JosekiMoveDialog::updateBoard` が決める。値の受け渡しは `JosekiMoveDialog::setEntry(const JosekiMove&)`・`entry()`
  - `boardappearancedialog`・`boardappearancesamples` - 対局画面の外観ダイアログ `BoardAppearanceDialog`（駒・将棋盤・背景・駒台・対局者情報の見本の一覧、「おすすめの組み合わせ」、「詳細」の色と木目・影・駒の大きさ。選んだ外観は `BoardAppearance` に書く）と、見本の画像を描く `BoardAppearanceSamples`（見えた項目だけを描く一覧の `LazySampleDelegate`）
  - `promotedialog` - 成り・不成の確認。`PromoteDialog::ask(parent)` で尋ねる（主画面・詰将棋を解く画面・定跡手のダイアログの盤の操作 `BoardInteractionController` に、作るときに渡す）
  - `pvboarddialog`・`pvline` - 読み筋盤（`PvBoardDialog(PvText)`）。読み筋1本の値 `PvLine`（`PvText` から作る。各手数の局面 `positions()`・手の表記 `moveText(ply)`・`displayPv()`・手の後に続く語 `ending()`（`KifuPresentation::pvEnding`。局面を伴わないので手数に入らない）・直前の手のハイライト `highlight(ply)` が返す `PvLine::Highlight`）。表示する手数は `BoardViewerWidget` だけが持ち、上部の帯（widgets の `PvMoveStrip`）はそれに従う
  - `tsumeplaydialog` - 詰将棋を解く画面。対局と正解手順の再生は `Mode { Playing, Reviewing }`、部品の有効・無効は `controlStates()` の表1か所で決める。挑戦・正答の記録は `analysis/tsumegamesession`（`TsumeGameSession`）が付ける
- **models/**: Qt models for lists and data binding（`shogienginethinkingmodel`・`kifuanalysislistmodel`: 思考の表・棋譜解析の結果の表。行は `ShogiInfoRecord`・`KifuAnalysisRow`（`kifuanalysisrow.h`）の値で持ち、表記の設定ごとの表示は `NotationTextCache` で覚える。詰みの評価値の表記（「3手詰」・手数の分からない詰みは「詰み」）は `scoretext` の `ScoreText` だけで作り、評価値グラフも「先手勝ち・3手詰」のようにこれを使う、`josekitablemodel`: 定跡ウィンドウの表（定跡手を画面に出す表記は、表・定跡手のダイアログ・登録の知らせ・定跡を外れた手のどれも `JosekiTableModel::moveText` だけで作る。予想応手が「なし」の手には、指した後の局面の定跡から導いた応手を灰色で出す。`setMoves` に `JosekiBookSession::leadingMove` を渡す）、`enginestatusmodel`: エンジン1台の情報欄と通信ログの行の知らせ、`evaluationseries`: 評価値グラフの点。先手から見た値で持ち、向きの直しは core の `UsiScore::blackView()`）
- **services/**: 設定の宣言と保存・登録エンジン・表記と翻訳の設定など、GUI に依存しない共有のサービス（外観は style、駒音は ui）
  - `engineprofilestore.cpp/.h` - 登録エンジンの設定（一覧・USIオプション・先読みの設定）の保存形式を知る唯一の場所（ほかの部品は設定ファイルを開かずにこの関数を使う）
  - `registeredengines.cpp/.h` - 登録エンジン1つ `EngineEntry` と一覧 `RegisteredEngines`（`load()`・名前とパスでの検索・登録名の規則 `uniqueName`・比べるときのパス `canonicalPath`。GUI・CLI と、エンジンの登録 `EngineRegistrationHandler`（一覧をこの値で持つ）で共通）。機能ごとに選んだエンジンの保存は `engineselectionsettings`（`EngineSelectionSettings::SelectionKeys`）。オプションの型は `EngineOption` 1つ（`UsiLineParser::parseOption` が返す）
  - `notationstyle.cpp/.h` - 表示言語と棋譜の表記の設定 `NotationStyle`（`Notation`・`Options`・`resolveLanguage`・`configure`・`options`・`revision`）。起動時に `ApplicationTranslations` が設定し、kifu の `KifuPresentation` が読む
- **common/**: 共有の土台（`appactivitystate`（動作状態 `AppActivityState` と持ち主のリース `ActivityLease`）・`branchtreeitemroles.h`・`errorbus`・`logcategories`・`threadtypes.h`・`boundedfile.h`（利用者が選んだ棋譜・局面集を、通常のファイルだけ上限つきで読む `BoundedFile::read`）・`recentfilelist`（新しい順・重複なし・上限つきの一覧 `RecentList<T>` と、最近使ったファイルの一覧 `RecentFileList`。定跡の `JosekiBookSession` と局面集ビューアが使い、通信対局の接続先の履歴も `RecentList` を使う））

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
4. 値の範囲を絞る項目は `BoundedSetting<T>{"Group/key", 既定値, 最小, 最大}`、構造体を項目ごとのキーに保存する項目は `StructSetting<T>{{SettingField<T>::of("Group/key", &T::member), ...}, 整える関数}`、配列で保存する項目は `ListSetting<T>{配列のキー, 要素のキー}`（配列のキーは実行時に作ってもよい）・`StructListSetting<T>{配列のキー, {項目...}}` で宣言する（いずれも `setting.h`。使い方は `Setting<T>` と同じ）。グループ名や番号だけ違う組は、キーを組み立てて宣言を返す関数を1つ書き、組ごとにその関数で宣言する（`JosekiSettings::playOptionsSetting`・`GameSettings::StartGame::playerSettings`）。値の範囲は宣言に書き、読む側で `qBound` し直さない（範囲は `minimum()`・`maximum()` で読める）。これらで表せない処理（以前の版の値の引き継ぎ（`GameSettings::StartGame::startingPosition`）、保存の形と値の形の換算（対局開始の時間 `Player::time`・`setTime`）、`AppearanceSettings::boardVisuals` の駒台の木目の代替のような未保存の項目の代わり、`kifuAnalysisColumnWidths`・`GameSettings::gameInfoColumnWidths` の以前の版の保存形式（1つの値）を配列に移すような引き継ぎ。列の数が変わった列幅 `AnalysisSettings::thinkingViewColumnWidths`（2つ目のエンジンの表を別に保存していた版の保存の引き継ぎも）・`JosekiSettings::josekiWindowColumnWidths`（services は models の列の enum を include できないので、列の番号は設定の側の定数にし、表の部品が `static_assert` で照らす）、小さい保存サイズを捨てる `GameSettings::jishogiScoreDialogSize()`、偶数の目標手数を直す `TsumeshogiSettings::upgradeLegacyGeneratorTargetMoves()`、以前のキーのファイル `TsumeshogiSettings::lastCollectionFile()` も同じ）が要る項目だけを関数として `.cpp` に書く。以前の版の値の引き継ぎはダイアログ・パネルの中に書かない。その場合も `SettingsCommon::openSettings()` を使う（設定ファイルを `QSettings` で直接開くのは、`beginReadArray` などでグループの状態を変えたまま読み書きを続ける配列データ（`EngineProfileStore`）だけ）

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

- `DialogFontScale`（`src/widgets/dialogfontscale.h`）は各部品を元の大きさからの差で拡大・縮小する。太字や大きめの見出しは作るときに1回だけ相対の大きさを決めればよく、変更のたびに付け直さない。行の高さなどの追加の調整は `install()` が返す部品の `fontSize()`（`FontSizeHelper*`）の `fontSizeChanged` につなぐ。盤面など文字の大きさを自分で決める部品は、プロパティ `dialogFontScaleExcluded` を true にして対象外にする
- 文字の大きさの宣言の既定値を 0 にすると、未保存のときはダイアログの元の大きさから始める（補助ダイアログの `AppSettings::promotionDialogFontSize` など）
- ドックのパネルは `FontSizeHelper` を持ち（文字の大きさの範囲は設定の宣言 `BoundedSetting<int>` に書く）、ボタンを `FontSizeControl` で結ぶ（`FontSizeControl::createToolButtons(parent, helper, 縮小の名前, 拡大の名前)` か、置いたボタンを渡す）。A-/A+ の文字・見た目・大きさ・説明はパネルで決めない（ほかの小さなボタンも `PanelButton::setup` で整える）。列幅を保存する表は `ColumnWidthMemory` を付ける

## Development Environment

- **Qt Creator** is used for development

## Design Principles

- **MainWindow should stay lean**: Delegate logic to existing or new classes (coordinators, controllers, services). MainWindow should only handle MainWindow-specific responsibilities.
- When adding new features, prefer creating new classes in appropriate directories (`app/`, `controllers/`, `services/`, etc.) rather than adding code to MainWindow.
- **Deps/Hooks/Refs パターン**: `docs/dev/deps-hooks-refs-pattern.md` に従う。依存注入に3種の構造体パターン（Deps=外部依存注入、Hooks=上位層へのコールバック、Refs=親状態への参照）を使い分ける
- **スレッド安全性**: `docs/dev/threading-guidelines.md` に従う。UIアクセスはメインスレッド限定、ワーカーには値オブジェクトのみ渡す
- **層の依存**: 下の層は上の層を include しない。順に core・common → services → kifu → engine → game → analysis・network → style → models → widgets → views → board → dialogs → ui → app（views は盤の描画で、widgets の `ElideLabel`・`GlobalToolTip` を使う。widgets は views を使わない）。`headless` は core〜analysis だけを使い、`automation`・`cli` は MainWindow・ダイアログ・ウィジェットに依存しない（`cli` は盤面画像 `boardimagerenderer` だけが views を使う。規則は `tests/tst_layer_dependencies.cpp` が検査する）
- **クラス名の役割**: `*Wiring` はシグナルの接続だけ、`*Presenter` は表示への反映、`*Controller` は利用者操作の処理、`*Coordinator` は複数の部品にまたがる流れ、`*Session` は開始から終わりまでの状態を持つ一続きの処理（対局・検討・棋譜解析・定跡ファイル）。役割が変わったら名前も変える
- **モーダルのダイアログをまたいでポインタを持たない**: `exec()` の間はイベントループが回り、通信対局の次の局などで棋譜が作り直される。`KifuBranchNode*` は番号（`nodeId()`）で持ち、閉じた後に `KifuBranchTree::nodeAt` で引き直す（番号は再利用されない）。棋譜を移って・変えてよいかも閉じた後に確かめ直す
- **同じ操作の入口**: 規則を足す・直すときは、同じ操作の入口がほかにないかを確かめる（開始局面の読みは全形式、保存は名前を付けて・上書き・自動保存・`kifu.save`・`convert-kifu`、局面の読みは貼り付け・`position.set`・棋譜の読み込みなど）。1つの入口にだけ規則を置かない
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
