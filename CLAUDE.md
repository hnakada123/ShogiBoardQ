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
  - `mainwindowcomponents.h` - 合成ルートが作る部品の一覧。状態は `src/common/appstate.h` の `AppState` に1つだけ置く（部品には `AppState*` を渡し、個々のフィールドへのポインタは渡さない）
  - `mainwindowactions.cpp/.h` - メニュー・盤・棋譜欄からの利用者操作の振り分け先
  - `menuactionswiring.cpp/.h` - メニュー・ツールバーの項目と処理する部品の接続
  - `gamesession.cpp/.h` - 対局の開始・終局・再開と、対局の手順の棋譜への追記（`MatchRecordSink`）。ローカル対局と通信対局の棋譜を開始から終局・自動保存まで1本の流れで扱い、投了・中断・即指し・入玉宣言は `activeChannel()`（`GameChannel`）へ送る
  - `sessionresetcoordinator.cpp/.h` - 対局の開始前・新規・局面編集・棋譜の読み込み前に、棋譜と表示を作り直す（エンジンの表示の消去は `EngineDisplayPresenter::clearAll()`）

- **core/**: 純粋なゲームロジック（Qt GUI非依存）
  - `shogiposition.cpp/.h` - 局面の値 `Position`（盤81マス・持ち駒・手番・手数）。SFEN の読み書きと指し手の適用（`apply`）はここだけ。計算のための局面はこの値で持つ
  - `shogiboard.cpp/.h` - 表示・局面編集に使う盤。`Position` を1つ持ち（`position()`）、変更をシグナルで知らせる QObject
  - `shogitypes.h`・`piecerules.h` - 手番と駒（`Piece` は種類＋後手なら16。並びは fmv と同じ）。行き所のない駒・成れる段・持ち駒の並びの規則
  - `usiposition.cpp/.h` - エンジンに探索させる局面 `UsiPosition`（開始局面・指し手の列・直前の手）。kifu の `RecordLine` が作るので core に置く。position 文字列は `UsiPosition::commandFor(startSfen, moves)` だけで作る（`command()` と USI 形式の書き出し `UsiExporter` で共有）
  - `shogimove.cpp/.h`・`usimove.h` - Move representation
  - `enginemovevalidator.cpp/.h` - Move legality validation（`fmv::Converter` を境界に `fmvlegalcore` のビットボード合法手生成を使う）
  - `sfenreplay.cpp/.h` - 局面に USI の1手を指す（`applyLenient`・`positionAfter`。合法手かは確かめない）
  - `boardcoordinates.h` - 盤面表示の座標（駒台は筋10・11、駒箱は筋12）と駒・USI の指し手の対応
  - `handicaps.h` - 手合割の表（名前・JKF の preset・開始局面）
  - `jishogicalculator.cpp/.h` - 持将棋の点数計算・判定
  - `matchplayers.h` - 対局者（先手・後手を人間とエンジンのどちらが指すかと名前）
  - `shogiclock.cpp/.h` - Game clock
  - `shoginotation.h` - 筋・段・駒の表記（漢字・全角・CSA）

- **game/**: ゲーム進行・対局管理（GUI に依存しない）
  - `matchcoordinator.cpp/.h` - 対局の司令塔（開始・着手・終局・待った・中断と再開）。手順は `record()`、対局者は `players()`
  - `humanvshumanstrategy` / `humanvsenginestrategy` / `enginevsenginestrategy` - 対局者の組ごとの進め方（`MatchStrategyHost` 経由）
  - `enginelifecyclemanager.cpp/.h` - エンジンの枠 E1・E2（対局・検討・棋譜解析・CSA 通信対局で共有）。使う側は `acquire(owner, slot, side, name)` で move-only の `EngineLease`（`engine()`・`reset()`）を受け取り、持っている間だけ使う。手放すと枠を空ける（持ち主には知らせない）。ほかの持ち主が同じ枠を取った・`destroyEngines()` で全部の枠を破棄したときは、前の持ち主に `engineRevoked(owner)` で知らせる（枠にあるかは `holds(engine)`）
  - `gamechannel.cpp/.h` - 対局相手の窓口 `GameChannel`: 投了・中断・即指し・入玉宣言をローカル対局と通信対局で同じ操作にする（ローカル対局は `LocalGameChannel`、通信対局は network の `CsaGameChannel`）
  - `matchautosave.h` - 終局した棋譜の自動保存の指定 `MatchAutoSave`（ローカル対局・通信対局で同じ）
  - `consecutivegameseries.cpp/.h` - エンジン同士の連続対局（局数・手番の入れ替え・次局の計画）
  - `shogigamecontroller.cpp/.h` - Game flow control
  - `gamestartoptionsbuilder` - 対局開始ダイアログの値の解釈

- **kifu/**: 棋譜関連（GUI に依存しない）
  - `kifudocument.cpp/.h` - 開いている棋譜の文書 `KifuDocument`（分岐ツリー・対局情報・対局の記録（対局者・持ち時間・開始と終了の日時）・上書き保存先）。未保存かどうかは保存した時点との比較で求める
  - `kifubranchtree.cpp/.h`・`kifubranchnode.cpp/.h` - 分岐ツリー `KifuBranchTree`（ルートを所有し、ノードは子を `std::unique_ptr` で所有する。木の形を変えられるのはツリーだけ）。手を足すのは `addRecord(parent, RecordEntry)` だけ。構造の変更は型付きのシグナル `nodeAppended(parentId, nodeId)`・`childrenReordered(parentId)`・`subtreeRemoved(parentId, nodeId)`・`reset()` で知らせ、`beginBatchUpdate()`〜`endBatchUpdate()` の間は遅らせて順に出す（作り直しを含めば `reset()` 1回）。テストで手を足す・探す補助は `tests/branchtree_test_util.h`（`TestTree`）
  - `kifuexport.cpp/.h` - 形式ごとの書き出しの表（`KifuExport::lines`。GUI の保存・コピーと CLI の変換で共通）
  - `recordline.h`・`recordentry.h` - 1本の手順と1手の記録。手順の型は `RecordLine` だけ（読み込みの本譜・変化・書き出し・表示中の手順・対局の手順。末尾に終局の行を持てる）
  - `kifuformat.cpp/.h` - 棋譜の形式（読み込み・保存・変換で共通の1つの enum）と判定
  - `kifuloadparser.cpp/.h` - 棋譜の読み込み口（ファイルの文字コードの判別と形式ごとの変換器への振り分け）
  - `kifparsetypes.h` - 読み込み結果（`KifParseResult`・形式ごとの `parse()` が返す `KifuParseOutcome`）と、読めない手の扱い（`ParseOptions`・`ParseReport`。ここだけで決める）
  - `kifuioservice.cpp/.h`・`kifusavefile.cpp/.h` - Kifu file I/O
  - `josekibook.cpp/.h` - 定跡のデータの値型 `JosekiBook`（局面のキー `JosekiBook::positionKey()`（手数を除いた SFEN）から `JosekiPosition{登録したときの手数付きの SFEN, 定跡手}` を引く map 1つ。最後の定跡手を消すと局面も消える。マージで登録した印は `JosekiMoveKey`（局面のキーと USI の指し手の組。`josekitypes.h`））と、やねうら王定跡形式の読み書き `JosekiBookFile::load()`・`save()`（ワーカースレッドから呼べる）
  - `josekibooksession.cpp/.h` - 開いている定跡ファイル（`JosekiBook` の値・変更状態・読み書き）
  - `kifupresentation.cpp/.h` - 画面に出す棋譜の表記 `KifuPresentation`（指し手・読み筋・終局語。ファイルやプロトコルには使わない）。日本語の指し手は `KifMoveText`、段の漢数字は `ShogiNotation`、英語表記で移動元を付けるかは KI2 と同じ判定（`Ki2MoveNotation::annotate()` の `Annotation::sources`。合法手で決めるのでピンされた駒は数えない）。`Notation`・`Options` は `NotationStyle` の別名で、`configure`・`options`・`revision`・`resolveLanguage` は `NotationStyle` へ転送する
  - **formats/**: Format converters（テキストを読む。ファイルは `KifuLoadParser` が読んで渡す）
    - `kiftosfenconverter`, `ki2tosfenconverter`, `csatosfenconverter`, etc. - 形式ごとに `parse(text, options)` を1つだけ持ち、`KifuParseOutcome` を返す（BOD は `KifTextParser::parseBod`）
    - `kiftextparser.cpp/.h` - KIF・KI2 で共有する読み取り（行の分類・「変化：N手」の区切り・開始局面）。指し手の文法は `kifmovegrammar`（KIF の手は KI2 の手＋移動元）
    - `kiflinebuilder.cpp/.h` - 全形式で共通の手順の組み立て（`KifLineBuilder`。指しながら `RecordEntry` を作り、表記は `KifMoveText::format` で作る）
    - `*exporter` - 形式ごとの書き出し。KIF・KI2 の先頭（文字コード宣言・対局情報・盤面図）は `KifExporter::appendHeaderLines(ctx, out, HeaderOptions)` で共有し、KI2 の本譜と変化は `appendKi2Moves` の1つのループで書く
    - `kifunodereplay.cpp/.h` - 分岐ツリーのノードを指す前の局面と一緒にたどる `KifuNodeReplay`（`board()`・`mover()`・`previousTo()`・`canWrite(node)`・`advance(node)`・`branchFrom(parent)`。指せない手はノードの SFEN から取り直す）。KI2・JKF の書き出しが使う
    - `usencodec.cpp/.h` - USEN の符号表 `UsenCodec`（指し手の3文字・開始局面・1本の手順の符号化と復号 `encodeMove`・`decodeMove`・`encodePosition`・`decodePosition`・`decodeMoves`、結果 `UsenDecodeResult`）。`UsenToSfenConverter` と `UsenExporter` が使う

- **analysis/**: 解析機能（GUI に依存しない）
  - `analysissession.cpp/.h` - 検討・詰み探索のセッション
  - `analysisjob.cpp/.h` - 棋譜解析1回（`AnalysisJob`）。1局面の結果 `PlyAnalysis` は `PlyAnalysisCollector` が multipv 1 の info と bestmove から作る
  - `tsumecandidatejob.cpp/.h`・`tsumeverificationjob.cpp/.h` - 詰将棋の候補1つの処理（手数の確認・唯一解の検査・要らない駒の除去）と、総予算つきの唯一解の検査（引き出し型の `MateQuerySource`。生成と CLI の検査で共通）
  - `tsumeshogigenerator.cpp/.h` - 詰将棋の生成（候補の待ち行列・問い合わせの中継・結果の集計）
  - `tsumecollectionsession.cpp/.h` - 詰将棋集の進捗・解析待ちの管理
  - `positioncollectionfile.cpp/.h` - 局面集ファイル（1行1局面）の規則 `PositionCollectionFile`（`parseLine`・`parse`、読めた行 `Line`）。局面集ビューア（`SfenCollectionDialog`）と詰将棋問題一覧（`TsumeCollection::parse`）で共有する。空行・`#` の行は読み飛ばし、局面は SFEN の4語（`sfen` を前置可）か `startpos`（前に `position` 可）、その後は `moves <USI の手>` だけ。position コマンドの手は指した後の局面（`Line::position()`）、SFEN だけの行の手は参考手順（`referenceMoves()`）。局面は `SfenValidationService` で確かめ、それ以外は読めない行
  - `tsumepositionutil.h` - 詰み探索の position の組み立て（`TsumePositionUtil::buildPositionForMate`。文字列は `UsiPosition::commandFor` で作る）
  - `sfenvalidationservice` など

- **engine/**: USI (Universal Shogi Interface) エンジン連携（対局の仕組みに依存しない）
  - `usi.cpp/.h` - GUI から使うエンジン1台の窓口
  - `usienginesession.cpp/.h` - 起動と初期化（usi〜readyok）。GUI と CLI の実行器が共有する。`startAsync()`・`probeAsync()` は値を返さず、起動の失敗は必ず呼び出しから戻った後に `errorOccurred` で知らせる（知らせる前に `quit()`・次の起動をしたら知らせない）ので、使う側は起動の途中の再入に備えない（`Usi::startEngine`・`MateOracle::start` も同じ）
  - `usiprotocolhandler.cpp/.h` - USI protocol implementation
  - `thinkinginfoformatter.cpp/.h`・`enginesearchstatus.h` - info を思考の表の行と探索状況 `EngineSearchStatus`（数値のまま。info 1行ごとに `searchStatusUpdated` で1回送り、書式は表示する側が決める）にする
  - `usilineparser.cpp/.h` - 受信行（info・bestmove・checkmate・option・id）を型付きの値にする
  - `mateoracle.cpp/.h` - 詰みの問い合わせ（go mate）の部品 `MateOracle` と答え `MateAnswer`（`UsiCheckmate` からの変換は `MateAnswer::fromCheckmate` だけ）
  - `matequeryloop.cpp/.h` - 引き出し型の問い合わせ元 `MateQuerySource` を `MateOracle` で回す `MateQueryLoop`
  - `engineprocessmanager.cpp/.h` - Engine process lifecycle
  - `usimatchhandler.cpp/.h` - 対局・解析での USI のやり取り（position/go/bestmove）
  - `matchtimesource.cpp/.h` - エンジンの思考に使う時間の出どころ `MatchTimeSource`（ローカル対局は対局時計の `ClockTimeSource`、通信対局はサーバーの残り時間を返す `CsaGameCoordinator`、時計のない場合は `FixedTimeSource`）

- **network/**: CSA通信対局
  - `csaclient.cpp/.h` - CSA protocol client（受信の解釈は `csaclient_parser.cpp`、送信は `csaclient_commands.cpp`）
  - `csagamecoordinator.cpp/.h` - CSA game coordination（手順と持ち時間は `CsaGameProgress`）
  - `csagamechannel.cpp/.h` - 通信対局の窓口 `CsaGameChannel`（`GameChannel` の実装。操作はサーバーへ送り、入玉宣言の成否はサーバーが判定する）

- **navigation/**: ナビゲーション機能
  - `kifunavigationcontroller.cpp/.h` - 棋譜ナビゲーション（ボタン・分岐）。局面送りの6つの操作は `QAction` の組（`action(Step)`）で1つだけ持ち、棋譜欄と評価値グラフのボタンを `bindButtons()` で結ぶ（有効・無効も QAction に従う。切り替えは `setStepActionsEnabled()`）

- **style/**: ウィジェットの書体と見た目（QWidget に依存する。navigation の上、models の下の層。`engine`〜`navigation`・`models` を参照しない）
  - `applicationfonts` - 表示言語に応じた書体 `ApplicationFonts`
  - `applicationstyle`・`applicationrendering` - アプリ全体のスタイルと描画の設定
  - `buttonstyles.h`・`tablestyles.h` - ボタン・表のスタイルシート（`ButtonStyles`・`TableStyles`）
  - `movefont.h` - 指し手・読み筋の書体の規則 `MoveFont`（表のモデルの FontRole と `KifuMoveDelegate` で共通）

- **board/**: 盤面編集・操作
  - `boardinteractioncontroller.cpp/.h` - 盤のクリックを移動元・移動先にし、選択・直前の手・合法な移動先のハイライトを出す
  - `boardinputroute.h` - 盤で指した手の行き先 `BoardInputRoute`（つまめるか・局面編集中か・着手の処理。主画面は `BoardMoveController`、詰将棋を解くダイアログは `TsumePlayDialog`）
  - `positioneditsession.cpp/.h` - 局面編集（開始・終了・編集メニュー・編集中の着手）。「編集中か」は動作状態 `DuringPositionEdit` だけで決まる
  - `boardimageexporter.cpp/.h` - Board image export

- **ui/**: UI層
  - **presenters/**: 表示への反映
    - `enginedisplaypresenter`（エンジンの表示の持ち主。検討の表示は `AnalysisSession` の知らせだけから決める）, `playerinfopresenter`（盤の対局者名）, `evaluationgraphpresenter`（評価値グラフの点の唯一の書き手）, `kifudisplaypresenter`（棋譜欄・分岐）, `boardsyncpresenter`（盤と手番）, `candidatearrowpresenter`, etc.
  - **controllers/**: 利用者操作の処理
    - `boardmovecontroller`（主画面の `BoardInputRoute`。動作状態ごとに手を処理する）, `humanturnpolicy`（盤を人間が操作できる手番か・対局が進行中かの判定）, `considerationuicontroller`（検討タブの操作を `AnalysisSession` へ伝えるだけ）, `josekiwindowcontroller`, `menuwindowcontroller`, `pvclickcontroller`, etc.
  - **coordinators/**: 複数の部品にまたがる UI の流れ
    - `dialoglauncher`, `kifuloadcoordinator`, `kifufilecontroller`（棋譜の開く・保存・貼り付け・コピー）, `csagamesession`, etc.
    - `uistatepolicymanager` - 動作状態ごとのメニュー・ボタンの有効/無効（コンパイル時の表）
    - `dockregistry` - ドックの表 `DockRegistry`（`DockId`・`DockSpec`）。12個のドックをこの表だけから作り（`createAll()`）、規定の場所に置き（同じ場所のドックは表で前のドックのタブに重ねる。起動時と `resetToDefault()` で同じ配置）、浮動・表示・位置を保存・復元する（`saveStates()`・`DockSettings::DockState`）。ドックを足すときは `DockId` と表に1行足す
    - `docklayoutmanager` - 名前付きレイアウト・起動時のレイアウト・ドックの固定（ドックは `DockRegistry` から取る）

- **views/**: 盤の描画と入力（`shogiview.cpp/.h`）。対局者カード・時計は `boardchromewidget`、局面によらない部分の画像は `boardstaticlayer`、配置は `shogiviewlayout`、ドラッグは `shogiviewinteraction`、ハイライト（層 `BoardHighlightLayer` ごとのマスの値。色は層で決まる）と矢印は `shogiviewhighlighting`
- **automation/**: 自動化 API（`ShogiBoardQ --automation`）と、CLI・MCP サーバーが共有する GUI 非依存サービス（設計: `docs/dev/mcp-server.md`）
  - `automationserver.cpp/.h` - QLocalServer で改行区切り JSON-RPC 2.0 を待ち受け
  - `automationdispatcher.cpp/.h` - JSON-RPC 解析とメソッド表（GUI 非依存）。メソッドを呼ぶ前に引数を表で検査し（知らない引数は拒否）、既定値を補う。エラー応答（`makeError()`）は `error.code` と `error.data.slug` を分類から決める
  - `errorkinds.cpp/.h` - エラーの分類 `ErrorKind` と、語（slug）・JSON-RPC のコードの表（`ErrorKinds::slug()`・`rpcCode()`・`toJson()`）。CLI（`CliOutput::fail(ErrorKind, ...)`）・自動化 API（`AutomationError(ErrorKind, ...)`）・MCP サーバーで同じ失敗は同じ語にする。分類を足すときは enum と表に1行ずつ足す
  - `paramspec.cpp/.h` - 引数の表の部品 `ParamSpec`（名前・型・必須・範囲・既定値・選択肢・説明）と `OperationSpec`。JSON Schema の出力（`ParamSpecs::inputSchema()`）と検査（`validate()`）。CLI・自動化 API・MCP で共有する
  - `automationmethodspecs.cpp/.h` - 自動化 API の全メソッドの引数の表 `AutomationMethodSpecs`。メソッドは補われた値を読み、`AutomationParams` は範囲が画面の状態で決まる引数にだけ使う
  - `automationcommands_*.cpp` - app / position / kifu / ui の各メソッド、`automationactionpolicy` は QAction 許可リスト
  - `automationinteractions.cpp/.h` - 応答の後に行う UI 操作の予約 `AutomationInteractions::queue(QueuedInteraction{target, check, perform}, response)`。実行の直前に `check` で再検査し、結果（pending・running・done・failed と分類つきの error・cancelled）を記録して `ui.interaction` で返す（応答に `"queued": true` と `"interaction"` の番号）。UI を操作するメソッドはこれで予約し、`AutomationDeferredCall` を直接使うのは `app.quit` だけ。実行時の失敗は握りつぶさない。MCP サーバーは `AppClient.call_queued()` で実行を待つ
  - `automationinput.cpp/.h` - 通常のマウス入力 `AutomationInput::sendClick()`・`sendDrag()`（盤・表のセル・分岐ツリー・ドックのタイトルバー）
  - `automationwidgetadapters.cpp/.h` - 部品の型ごとの読み（`describe()`・`widget.text`）と書き（`writer()`・`widget.setValue`）の表 `AutomationWidgetAdapters`。型を足すときは表に1行足す
  - `kifuconversionservice`, `boardimagerenderer`, `engineanalysisrunner`, `matesearchrunner`, `tsumeverificationrunner`（詰みの問い合わせは `MateOracle`） - CLI と共有するサービス（局面の検証は `analysis/sfenvalidationservice`）
- **cli/**: `shogiboardq-cli`（GUI を起動しない JSON 出力コマンド。`mcp/` の MCP サーバーが呼び出す）
  - `clicommandspecs.cpp/.h` - 全コマンドの引数の表 `CliCommandSpecs`（オプション名は引数名の `_` を `-` にしたもの）。オプションの登録・使い方の文・コマンドラインから検査済みの JSON への変換（真偽値はフラグ、文字列の配列は空白区切り）をこの表から行う。各コマンドは `CliArgs::parse()` で読み、`resolvePosition(params)`・`resolveEngine(params)` で解決する
  - `environment` コマンドは自動化 API の既定のソケット・エンドポイントファイル・設定ファイルの場所、1メッセージの上限（`AutomationServer::kMaxMessageBytes`）、エラーの分類の表を出す。MCP サーバー（`mcp/shogiboardq_mcp/environment.py`）はこれを読み、場所や上限・語を Python に写さない
  - `describe` コマンドは CLI と自動化 API の表を JSON Schema で出す。MCP のツールのスキーマとの一致は `mcp/tests/test_param_specs.py` が検査するので、引数を変えたら `mcp/shogiboardq_mcp/tooldefs.py` も合わせる
- **widgets/**: Custom Qt widgets
  - ドックのパネル（`thinkingpanel`・`gameinfopane`・`recordpane`・`considerationtabmanager`・`usilogpanel`・`csalogpanel`・`commenteditorpanel`・`analysisresultspanel`・`branchtreeview`）はすべて `QWidget` の派生で、部品をコンストラクタで作りきる。`DockRegistry` はパネルそのものをドックに入れる（`buildUi()`・`containerWidget()` のような、外から後で中身を作らせる・取り出す公開の関数は持たない）
  - `thinkingpanel`・`thinkingtableview`（思考の表。対局と検討タブで共用）・`pvtableview`（読み筋の表 `PvTableView`。数値の列・「盤面」ボタン（`boardRequested`）・読み筋の描き方・行の高さを受け持ち、思考の表の基底で棋譜解析の結果の表にも使う）・`engineinfowidget`（探索状況の書式はここで決める）, `gameinfopane`（対局情報。中身は `models/gameinfoeditmodel`）, `recordpane`, `considerationtabmanager`（検討タブ。設定 `ConsiderationSettings`（`considerationsettings.h`）を正本として持ち、開始/中止のボタンは実行の状態 `RunState` で決まる）, `evaluationchartwidget`（評価値グラフ。`EvaluationSeries` を描くだけ）, `analysisresultspanel`（棋譜解析の結果の表。列は `KifuAnalysisListModel::Column`、モデルは `setModel()` で1度だけ結ぶ。ドックを出すのは `KifuAnalysisController`）, `branchtreeview`（分岐ツリーの図 `BranchTreeView`。`QGraphicsView` の派生でドックの中身そのもの。`setTree()` でツリーの変更の知らせを自分で受け、手順の末尾に1手足しただけならその手だけを描き足し、それ以外は作り直す。外とは `KifuBranchNode` の ID だけでやり取りする（`setCurrentNode(nodeId)`・`nodeActivated(nodeId)`・`nodeContextMenuAboutToShow(nodeId, menu)`・`refreshNode(nodeId)`）。行・段・折りたたみの配置は `branchtreelayout`）・`branchnodeitem`（図の1手（開始局面を含む）の図形 `BranchNodeItem`。ノード ID・行・手数を型付きのメンバで持ち、`BranchTreeItemRoles` は自動化 API のためだけに付ける）
  - `tableheaderview` - 表の見出し。列幅は利用者がドラッグしたときだけ `sectionResizedByUser` で保存する
  - `fontsizehelper` - A-/A+ の文字の大きさ（範囲・保存・変更の通知）
  - `fontsizecontrol` - A-/A+ のボタンの組 `FontSizeControl`（文字・見た目・ツールチップと、範囲の端で押せなくする処理はここだけ）。ドックのパネル（思考・検討・棋譜・対局情報・コメント・通信ログ・棋譜解析の結果・定跡ウィンドウ・メニューウィンドウ）が使う。ボタンは役割のプロパティ `FontSizeControl::kRoleProperty`（値は `decrease`・`increase`）で探せる
  - `editactions` - 元に戻す〜貼り付けの5つの `QAction` の組 `EditActions`（標準ショートカット・アイコン・実際のショートカットを添えたツールチップはここだけ。ボタンは `createButton()`。何をするか・有効にするかは使う側が決める）。コメント欄（`CommentEditorPanel`）と対局情報（`GameInfoPane`）が使う
  - `recentfilesmenu` - 「履歴」ボタンのメニュー `RecentFilesMenu`（ファイル・区切り・「履歴をクリア」、空なら「（履歴なし）」。選ばれた項目は `fileTriggered(QString)`・`clearTriggered()` で知らせる）。定跡ウィンドウ（`JosekiFileController::openRecent(path)` へ渡す）と局面集ビューアが使う
  - `dialogfontscale` - ダイアログの A-/A+ と保存 `DialogFontScale`（全ダイアログ共通。各部品を元の大きさからの差で拡大・縮小する）
  - `engineselector` - 登録エンジンを選ぶ部品 `EngineSelector`（呼び出し側が置いたコンボ・「設定」ボタン・未登録の案内に結び、一覧の読み込み・保存した選択の復元と保存（`EngineListSettings::SelectionKeys`）・未登録のときの無効化を受け持つ）。設定ダイアログは dialogs にあるので開かず、`settingsRequested(EngineEntry)` で受け手に頼む。棋譜解析・詰み探索・詰将棋生成・詰将棋問題一覧・CSA通信対局のダイアログ、検討タブ、対局開始ダイアログの対局者の欄が使う
  - `playersetupwidget` - 対局開始ダイアログの対局者1人の欄 `PlayerSetupWidget`（コンボの先頭が「人間」（`EngineSelector` の内蔵の項目）で、その後に登録エンジン。人間なら名前の欄、エンジンなら「エンジン設定」ボタン）。選択は `PlayerChoice`（`game/startgamedatabridge.h`）で読み書きし、保存したエンジンが削除されていれば人間に戻す
  - `timecontroleditor` - 片方の対局者の持ち時間・秒読み・加算の欄 `TimeControlEditor`（`TimeControlSide` で読み書き。秒読みと加算は排他で、相手側の欄にも `byoyomiEntered`・`incrementEntered` で知らせる）
  - `tablemodelutils.h` - 表へのモデルの付け替え `TableModelUtils::setModel`（Qt が置き換えた選択モデルを解放する）
  - `settingsformbinding` - 入力欄と設定の宣言（`Setting<T>`）の対応の表 `SettingsFormBinding`（`add()` を1度書けば `load()`・`save()`・`reset()`（既定値に戻す）を同じ表で行う。対局開始・詰将棋生成のダイアログが使う）
- **dialogs/**: Dialog implementations (with co-located .ui files)
  - `josekimovedialog`・`josekimoveinputwidget` - 定跡手の追加・編集。指し手は局面の合法手を棋譜の表記で並べた一覧から選ぶ（`JosekiMoveInputWidget`。一覧のオブジェクト名は `<name>Combo`。予想応手は着手後の局面の合法手で、先頭が「なし」）。値の受け渡しは `JosekiMoveDialog::setEntry(const JosekiMove&)`・`entry()`
  - `promotedialog` - 成り・不成の確認。`PromoteDialog::ask(parent)` で尋ねる（主画面の `DialogLauncher::choosePromotion()` と詰将棋を解く画面で共通）
  - `pvboarddialog`・`pvline` - 読み筋盤。読み筋1本の値 `PvLine`（各手数の局面 `positions()`・手の表記 `moveText(ply)`・`displayPv()`・直前の手のハイライト `highlight(ply)` が返す `PvLine::Highlight`）。表示する手数は `BoardViewerWidget` だけが持つ
  - `tsumeplaydialog` - 詰将棋を解く画面。対局と正解手順の再生は `Mode { Playing, Reviewing }`、部品の有効・無効は `controlStates()` の表1か所で決める。挑戦・正答の記録は `analysis/tsumegamesession`（`TsumeGameSession`）が付ける
- **models/**: Qt models for lists and data binding（`shogienginethinkingmodel`・`kifuanalysislistmodel`: 思考の表・棋譜解析の結果の表。行は `ShogiInfoRecord`・`KifuAnalysisRow`（`kifuanalysisrow.h`）の値で持つ、`usicommlogmodel`: エンジン1台の情報欄と通信ログの行の知らせ、`evaluationseries`: 評価値グラフの点。先手から見た値で持ち、向きの直しは `blackView()` だけ）
- **services/**: Cross-cutting services (settings, time keeping)
  - `engineprofilestore.cpp/.h` - 登録エンジンの設定（一覧・USIオプション・先読みの設定）の保存形式を知る唯一の場所（ほかの部品は設定ファイルを開かずにこの関数を使う）
  - `piecestylecatalog.cpp/.h` - 駒のセットの表 `PieceStyleCatalog`（ID・分類 `Family`・表示名・画像の場所）。駒のセットを足すときはこの表に1行足す（外観ダイアログの絞り込みの番号は `Family` の値）
  - `notationstyle.cpp/.h` - 表示言語と棋譜の表記の設定 `NotationStyle`（`Notation`・`Options`・`resolveLanguage`・`configure`・`options`・`revision`）。起動時に `ApplicationTranslations` が設定し、kifu の `KifuPresentation` が読む
  - `appearancestate.cpp/.h` - 外観（駒のセット・配色・見た目）をまとめた値 `AppearanceState`（`current()`・`apply()`）。おすすめの組み合わせは `boardappearancecatalog` の `combinations()`
- **common/**: 共有の土台（`appstate.h`・`appactivitystate.h`・`boardcolors.h`・`boardvisuals.h`・`branchtreeitemroles.h`・`errorbus`・`logcategories`・`threadtypes.h`・`tsumeevaluation.h`・`recentfilelist`（最近使ったファイルの一覧 `RecentFileList`。新しい順・重複なし・上限つき。定跡の `JosekiBookSession` と局面集ビューアが使う））

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

設定はドメイン別ヘッダー（`src/services/appsettings.h`・`gamesettings.h`・`analysissettings.h`・`networksettings.h`・`josekisettings.h`・`tsumeshogisettings.h`・`enginedialogsettings.h`・`docksettings.h`）に、`Setting<T>`（`src/services/setting.h`）で**キーと既定値を1行で宣言する**。getter と setter の組やキー定数は手で書かない。

**追加手順:**
1. ドメイン別ヘッダーに `inline const Setting<T> name{"Group/key", 既定値};` を1行足す
2. 読むときは `NS::name()`、書くときは `NS::name.set(value)`、既定値は `NS::name.defaultValue()`。既定値が翻訳文や実行時の場所で決まる項目は、宣言の既定値を空にして `NS::name.valueOr(tr("..."))` で読む
3. ダイアログのコンストラクタで読み込み、デストラクタ・`done()`・`closeEvent` で保存する。ウィンドウサイズは `DialogUtils::restoreDialogSize` / `saveDialogSize`（または `DialogFontScale::Options::size`）、A-/A+ は `DialogFontScale::install(this, 宣言, options)` に宣言をそのまま渡す。ダイアログに `FontSizeHelper` のメンバや A-/A+ 用の connect・`applyFontSize()` は書かない。入力欄の値は `SettingsFormBinding`（`src/widgets/settingsformbinding.h`）に欄と宣言を `add()` すれば、読み込み・保存・既定値に戻すを1つの表で行える
4. 値の範囲を絞る項目は `BoundedSetting<T>{"Group/key", 既定値, 最小, 最大}`、構造体を項目ごとのキーに保存する項目は `StructSetting<T>{{SettingField<T>::of("Group/key", &T::member), ...}, 整える関数}`、配列で保存する項目は `ListSetting<T>{配列のキー, 要素のキー}`（配列のキーは実行時に作ってもよい）・`StructListSetting<T>{配列のキー, {項目...}}` で宣言する（いずれも `setting.h`。使い方は `Setting<T>` と同じ）。これらで表せない処理（以前の版の値の引き継ぎ、`AppSettings::boardVisuals` の駒台の木目の代替のような未保存の項目の代わり、`kifuAnalysisColumnWidths` の `QVariantList` のような独自の保存形式）が要る項目だけを関数として `.cpp` に書く。その場合も `SettingsCommon::openSettings()` を使う（設定ファイルを `QSettings` で直接開くのは、`beginReadArray` などでグループの状態を変えたまま読み書きを続ける配列データ（`EngineProfileStore`）だけ）

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

    // 文字の大きさ（初期値の読み込みと変更のたびの保存）。A-/A+ を渡さなければ決定ボタンの列に作る
    DialogFontScale::Options fontOptions;
    fontOptions.decrease = ui->buttonFontDecrease;
    fontOptions.increase = ui->buttonFontIncrease;
    DialogFontScale::install(this, GameSettings::myDialogFontSize, fontOptions);

    DialogUtils::restoreDialogSize(this, GameSettings::myDialogSize);
}

MyDialog::~MyDialog()
{
    DialogUtils::saveDialogSize(this, GameSettings::myDialogSize);
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
- **層の依存**: 下の層は上の層を include しない。順に core・common → services → kifu → engine → game → analysis・network・navigation → style → models → views・widgets → board → dialogs → ui → app。`automation`・`cli` は GUI に依存しない（規則は `tests/tst_layer_dependencies.cpp` が検査する）
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
