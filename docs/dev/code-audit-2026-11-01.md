# コード監査：責務の整理（2026-11-01）

「ソースコードが責務の見地から整理されているか」という依頼で、`src/` 全体（約8.6万行）と `mcp/` を監査した。前回の監査（`code-audit-2026-10-29.md`）の項目をすべて直した後の状態が対象である。前回の対象（0b441c2e）から35コミットで、src は 7,312 行増え、6,878 行減っている。

- 対象のコミットは 452980ae。本文の行番号はこの時点のもの。
- 6つの範囲に分けてコードを読み、呼び出し元を grep で確かめた。
  - core・kifu
  - engine・game・analysis・network・headless
  - app・ui・board・common
  - widgets・views・models・style
  - dialogs
  - services・automation・cli・mcp
- 観点は前回と同じ。
  - 1つのクラスが複数の責務を持っていないか
  - 同じ状態・規則を複数の場所で持っていないか
  - 層・ディレクトリの置き場所は合っているか
  - 転送だけの層や名残がないか
  - 名前と CLAUDE.md の説明が実態と合っているか
- 前回までの文書（10-16・10-23・10-29）と同じ指摘は除いた。ただし、前回「済」とした項目が残っていたものは挙げた。
- 主な指摘は、範囲ごとの監査とは別に、もう一度コードを読んで確かめた。棋譜の読み書きの食い違い（1.1）は、`shogiboardq-cli convert-kifu` で再現した。
- 監査ではソースを変更していない。

> **修正の進み具合**: 7章の順に直し、各項目に「状態: 済（コミット）」を書く。途中で止まったら、状態のない項目から再開する。

> **確かめた仕様（2026-11-01）**: USEN の終局の符号は、USEN を考案した作者の入出力ライブラリ `mogproject/mog-core-scala` の `shared/src/main/scala/com/mogproject/mogami/core/move/SpecialMove.scala`（最後の更新は 2023-01-03）で確かめた。作者の発表のスライド（「フロントエンド初心者の大学生が Scala.js で Web アプリを作ってみた話」の19枚目）は符号 `i`・`r`・`t`・`p`・`j` を挙げているが、ページから取れる文字には各符号の説明が無い。Shogi Playground の画面での動きは試していない。
> - `i` は `IllegalMove(move)`。手番側が指した反則手で、手番側の負け（KI2 では「まで N手で（手番でない側）の反則勝ち」と書く）。`i` の後ろに反則手を指し手と同じ3文字で付ける（例: `.i7bq`）。手の無い `.i` は「invalid move format」で読めない。
> - 反則勝ち（相手の反則。CSA の `±ILLEGAL_ACTION`）の符号は無い（ソースに `// todo: impl [+-]ILLEGAL_ACTION`）。
> - `r` は投了、`t` は時間切れ、`p` は中断。
> - `j` は `DeclareWin`（KIF の語は「持将棋」、CSA は `%JISHOGI`・`%KACHI`）。宣言勝ちと持将棋を区別しない。千日手・詰みの符号は無い。
> - 読むときは先頭の1文字だけで符号を決める（`s.headOption`）。
>
> F4・F31・F32・U7 はこれに基づく。

## 全体の評価

**骨組みは前回の状態を保っている。** 機械的に測れる指標は、どれも規則を満たしている。

- **層の依存**: `tst_layer_dependencies` は通る。ディレクトリの間で互いに include しあっているのは、前回と同じく views と widgets の1組だけ（各4か所。P1 で解消できる）。
- **構造の指標**: `tst_structural_kpi` は通る。
  - 1000行を超えるファイル: 0（最大は `csaclient.cpp` の842行）
  - connect にラムダを渡す箇所: 0
  - MainWindow: 45行、include は5つ
  - 裸の delete: 1（`flowlayout.cpp`）
- **合成ルート**: 72個の部品を作り、依存を注入し、シグナルをつなぐだけ。
- **Deps の大きさ**: 最大は `MenuActionsWiring` の18欄。Deps に std::function を持つのは `UiStatePolicyManager` の2つだけ（ほかは `KifuDocument::Hooks` の2つ）。
- **設定の宣言**: `Setting<T>` の宣言は、すべて外から読み書きされている。
- **自動化 API**: 登録しているメソッドと引数の表 `AutomationMethodSpecs` は31件すべて一致する。許可リストの QAction もすべて実在する。
- **CLAUDE.md**: 書かれているファイル名・クラス名・関数名は、すべて実在する（説明の中身の誤りは5章）。

**残っている問題は、部品の中身よりも「入口」と「経路」にある。** 前回までの監査で、1つの責務を1つの部品に集める作業はほぼ終わった。今回見つかった問題の多くは、同じ操作に入口が複数あり、規則がそのうちの1つにしかないものである。そのため、指摘の多くが挙動の食い違い（1章、39件）として現れている。

1. **形式ごとに読み込みの規則が違う。** 棋譜の6形式は読み込みの部品（`KifLineBuilder`・`MoveRecord::play`）を共有している。それでも、「局面に合う手か」「読めない手をどうするか」「終局の行はどちらの側か」「対局情報の行か」を、形式ごとに決めている（T1）。データが黙って変わるので、いちばん重い。
2. **UI の有効・無効の表にしか規則がない。** 「棋譜を移ってよいか」「棋譜を変えてよいか」は、`UiStatePolicyManager` が部品を無効にする表にしかない。表に載っていない入口からは、対局中・局面編集中でも移れる・変えられる（T2）。
3. **通信対局とローカル対局の差が、時間・終局・表示の消去に残っている。** 前回 T1 で棋譜の流れは1本になった（T3）。
4. **GUI と CLI・自動化 API・MCP で、同じ操作の規則が違う**（T4）。
5. **表・範囲・既定値の写し。** 前回 T8（保存形式の表）と M10（値の型に範囲を置く）の形を使っていない箇所が残っている（T5・T6）。
6. **名残。** 呼び出しのない関数・シグナル、古い説明（5章）。

**複数の範囲で別々に見つかった指摘は確度が高い。** 次のものがそれに当たる。
- 棋譜解析の結果ドックとダイアログの文字の大きさの共有（F23。services と widgets）
- 詰み探索の手番（F16。services と engine）
- 開始局面の検査（U1。kifu と services）
- 定跡を使う条件の範囲の写し（T6。kifu と dialogs）
- SFEN の手番を文字列で読む写し（M18。kifu と ui）
- 不要な `csaLegacyPassword.remove()`（5章。services と dialogs）
- headless の部品の「自動化 API も使う」という説明（P3。services と engine）
- CLAUDE.md の `UnsavedChangesPrompt`・`SettingsFormBinding` の説明（5章。dialogs と widgets）

**前回までに「済」とした項目のうち、残っていたもの。**
- 10-29 T8（保存形式の表）:
  - 通信対局の自動保存の形式の一覧（F21）
  - MCP の説明文の写し（T5）
  - `savableNames` の手書きの一覧（T5）
- 10-29 M15（`UsiPosition::split` だけで分ける）: 局面集と USI の読み込みに別の分け方が残る（M4）
- 10-29 T4（対局の記録を書き出しの文脈から外す）: `MatchTimeRecord` が読み手1つの名残になっている（M8）
- 10-29 T6（SFEN を section で切らない）: 矢印の駒打ちに残る（M18）
- 10-29 M9・P4（`BoardImageRenderer`）: コメントと既定値の 50（5章）
- 10-29 5章（`uiStatePolicy` を消す）: mcp-server.md の記述が古いまま（5章）
- 10-23 R10:
  - 入力欄の読み書きを手で書くダイアログが2つ（M17）
  - 大きさを手で保存するダイアログが2つ（T6）
- 10-23 の「入玉宣言の閾値は core だけで決める」: game と dialogs に 10 が残る（M9）

---

## 1. 責務の重複から生じている挙動の食い違い

書き直しとは別に、個別に直せる。

### 1.1 実行して確かめたもの

1〜6 はどれも `shogiboardq-cli convert-kifu --text` で再現した。GUI の読み込みも同じ `KifuLoadParser` を通るので、同じ結果になる。

| # | 食い違い | 原因 |
|---|---|---|
| F1 | 自分の駒を取る手と、表記の駒が移動元の駒と違う手は、CSA だけが「Move does not match the position」として打ち切る。KIF・JKF・USI・USEN は警告なしに受け入れる。<br>- USI の `position startpos moves 7g8h` は `▲８八歩(77)` になる。その後の局面は `… w b 2` で、自分の角が後手の持ち駒になる<br>- KIF の `７六飛(77)` は、黙って `▲７六歩(77)` になる<br>- JKF は `color` が手番と違っても受け入れる。CSA・KI2 は断る | 「局面に合う手か」の規則が3か所にあり、中身が違う（T1）。<br>- `MoveRecord::play`（`moverecord.h:32-44`）: 移動元が空か、相手の駒かだけを見る<br>- `CsaLexer::parseMoveLine`（`csalexer.cpp:101-118`）: 自分の駒を取る手と、駒種の食い違いも断る<br>- `PvText` の `movingPiece`（`pvtext.cpp:24-34`）: 自分の駒を取る手と、持ち駒にない駒を打つ手を断る<br>`Position::apply` は取る駒の色を見ない（`shogiposition.cpp:325-327`）。`csatosfenconverter.cpp:156` のコメント「（全形式で同じ）」と食い違う<br>**状態: 済（9260770f）**。規則を core の `Position::mismatchOf` に1つ置き（動かす駒の色・自分の駒を取らないか・打つ駒と打つ先）、`MoveRecord::play` と `PvText` が使う。書かれた駒と手番は `WrittenMove` として `KifLineBuilder::addMove` に渡し、局面と違う手は全形式で断る（KIF・KI2 の駒名と印、CSA の駒種と符号、JKF の piece と color）。`CsaLexer::parseMoveLine` は字句を読むだけにし、補助関数 `belongsTo` の2つは消した。`Position::apply` は今までどおり取る駒の色を見ない（呼ぶ側が `mismatchOf` か合法手で確かめる） |
| F2 | 読めない手の扱いが3通りある。<br>- KIF・KI2・CSA: `ParseOptions`（`kifparsetypes.h:125-137`。「全形式で同じ。ここだけで決める」）の既定どおり、手前で打ち切って警告する<br>- USI: 読めない語を黙って飛ばす。`position startpos moves 7g7f 7g7x 3c3d` は `7g7f 3c3d` として読まれ、後の手が1手ずれる（`usitosfenconverter.cpp:113-120`）。指し手が1つも読めないと局面だけになり、GUI は警告を捨てる（`kifuloader.cpp:113`）<br>- JKF・USEN: 棋譜全体が失敗する（`jkftosfenconverter.cpp:181-185`、`usencodec.cpp:146-151,166-168`） | 読めない手を `ParseReport::rejectMove` に通さない形式がある（T1）<br>**状態: 済（37aa90f9）**。USI・JKF・USEN の読めない手も `ParseReport::rejectMove` に通す。既定ではその手の手前で打ち切って警告し、`FailRecord` なら棋譜全体を読めない（全6形式を同じテストで確かめる）。`UsenCodec::decodeLine` は読めた手と読めなかった手（`UsenLine::unreadableMove`）を返す。USI は `moves` の後に語が無いときだけ局面だけの読み込みにし、GUI は局面だけの読み込みでも警告を出す（`kifuloader.cpp`）。JKF の構造の誤り（項目が object でない・forks が配列でない）は、これまでどおり棋譜全体を読めない |
| F3 | KI2 の `▲７六歩 △３四歩 ▲２六歩` と `まで3手で先手の反則負け` を KIF に書くと、`4 反則負け`（後手の手番の行）と `まで3手で先手の反則負け` が食い違う。読み直すと「後手の反則負け」になる。CSA・JKF・USI・USEN を経由しても同じ | 名指しされた側を記録するのは KI2 の結果行の読み（`ki2lexer.cpp:69-91`→`kiftextparser.cpp:134`）だけ。ほかの形式は、終局の行を手番側の行としてしか書けず、読むときも手番側にする。手番側にそろえる規則 `RecordEntry::perpetualCheckTerminal`（`recordentry.h:95-104`）は対局でだけ使う（T1。10-23 B2 の残り）<br>**状態: 済（a96fc436）**。`RecordEntry::forSideToMove` を置き、手番でない側の反則負け・反則勝ち・不戦敗・不戦勝を、勝敗を変えずに手番側の行にする（例の結果行は「後手の反則勝ち」として読む）。KI2 の結果行と印付きの終局、対局・通信対局の終局、CSA・JKF の +/-ILLEGAL_ACTION の読みがこれを使う（`perpetualCheckTerminal` は消した）。手番でない側の投了・入玉宣言勝ちなど、そろえられない行は名指しのまま読んで警告する。通信対局で手番でない側が反則したときも、手番側の反則勝ちの行になる |
| F4 | KIF の `3 反則勝ち` を USEN にすると `~0.7ku2jm.i` になり、読み直すと「まで2手で先手の反則負け」になる。対局の連続王手の千日手（手番側の反則勝ちとして記録する）を USEN で保存すると、勝敗が逆になる。<br>USEN の仕様（冒頭の「確かめた仕様」）では、`i` は手番側の反則（手番側の負け）で、反則勝ちの符号は無い。`.i` を手番側の反則負けと読む今の扱いは仕様と合い、反則勝ちを `i` で書くのが誤り。保存の確認の文（`kifusavedialogs.cpp`）は、もともと「詰み・反則勝ちなど一部の終局理由も保存されません」と書いていて、この表と食い違う | `kUsenCodes` が IllegalLoss と IllegalWin を同じ `"i"` にしている（`terminalcodes.cpp:54-55`）。読むときは表で先の IllegalLoss になる（T1）<br>**状態: 済（f9695dc1）**。反則勝ち・反則負けを USEN に書かない（`TerminalCodes::usen` が空。`i` は読むだけの符号にした）。保存の確認の文は「詰み・反則負け・反則勝ちなど」にした（F31） |
| F5 | CSA V2.2 の `$TIME_LIMIT:01:30+30`（1時間30分・秒読み30秒）を CSA に書き直すと `$TIME:90+30+0`（90秒）になる | 読み込みの表が TIME_LIMIT を「持ち時間」の見出しにする（`csaformatter.cpp:143`）。その値をこのアプリの書式「mm:ss+秒」（`timecontroltext.cpp:33-46`）として読み直す（`csaformatter.cpp:117-128`→`:93`）。`KifuPresentation::infoValue` も分:秒として読む（T1）<br>**状態: 済（f9695dc1）**。読むときに `CsaFormatter::timeLimitToTimeControl` で持ち時間の書式に直す（`01:30+30` は `90:00+30`）。CSA に書くと V3.0 の `$TIME:5400+30+0` になる。「時:分+秒」の形でない値はそのまま持つ |
| F6 | 行内コメントに対局情報の語がある KIF・KI2 の指し手の行が、黙って消える。<br>- KIF の `   1 ７六歩(77)   ( 0:00/00:00:00) *第1図` は1手目が消え、手番の書き違いとして直されて `3c3d 2g2f` になる<br>- KI2 の `▲７六歩    △３四歩 *第1図` は行ごと消える<br>- `*ここで先手番`・`*場所…` なども同じ | 「対局情報の行か」の判定が3つある（T1）。<br>- 手順の読み: 指し手の行かを見る前に `isKifSkippableHeaderLine`（`parsecommon.cpp:72-103`）で読み飛ばす（`kiftextparser.cpp:121`）。見出しの語を行の中に含むか（contains）で決める<br>- 対局情報の抽出（`parsecommon.cpp:132-167`）: 「見出し：値」の形で決める<br>- 手合割の行（`kiftextparser.cpp:52`）: 行頭の形で決める<br>**状態: 済（cd783021）**。手順の読みは指し手の行かを先に見る。それ以外の行は手順に入れないので、語を含むかで決める `isKifSkippableHeaderLine` は消した。局面図の持駒の見出しも行頭だけで見る（`isBoardHeaderOrFrame`）。残る判定は、対局情報の抽出（「見出し：値」、最初の指し手の行まで）と手合割の行（行頭）で、どちらも行頭で決める。KI2 の行内コメント（`*` から行末）は KIF と同じく行の最後の手のコメントにし、コメントの中の手番の印を指し手と読まない（`Ki2Lexer::splitMovesLine`） |
| F7 | MCP の `convert_kifu` に変化のある棋譜を渡すと、要約に「The record has variations; only the main line is exported.」と出る。実際の KIF・KI2・JKF・USEN の出力には「変化：」が入る | 最初の MCP コミットからある文（`handlers_cli.py:103-104`）。同じ行は平手の SFEN の先頭を Python に写して「from the initial position」を決めている（`:101`。`Handicaps::isHirate` の写し）（T5）<br>**状態: 済（23964a75）**。変化を書ける形式かを kifu の書き出しの表 `KifuExport::writesVariations`（KIF・KI2・JKF・USEN）に置き、CLI の convert-kifu が `variations_dropped`（変化があるのに出力の形式が書けない）と `is_hirate`（`Handicaps::isHirate`）を返す。MCP の要約はこの2つだけを読み、「only the main line」は csa・usi・sfen のときだけ出す。保存ダイアログの「失うもの」の switch（F20）はまだこの表を使っていない |
| F8 | 入力ファイルの検査が経路ごとに違う。<br>- CLI の `convert-kifu`・`analyze-kifu` は、相対パスの `--input-path` を受け付ける（表の説明は「absolute path」）。ディレクトリを渡すと `parse_error` になる<br>- 自動化 API の `kifu.load` と MCP の `paths.py` は、どちらも `invalid_argument`<br>- mcp-server.md:394 の「CLI/アプリ側は絶対パスの要求…を行う」と合わない | 出力側には共有の `OutputPath::check` があるが、入力側には無い。CLI は存在だけを見る（`kifuconversionservice.cpp:31-34`）。自動化 API は `automationcommands_kifu.cpp:25-28,90-97`、MCP は `paths.py:resolve_read_path` が別々に検査する（T4）<br>**状態: 済（6a41e35d）**。headless に `InputPath::check`（絶対パスでない・通常のファイルでないは `invalid_argument`、無いファイルは `not_found`。リンクはたどる）を置き、`KifuConversionService`（CLI の `convert-kifu`・`analyze-kifu`）と `kifu.load` が使う。`kifu.load` の手書きの検査は消し、棋譜を置き換えられるかより先に確かめる。`paths.py` は前から同じ分類で、テストを足した。mcp-server.md:394 も直した |
| F31 | ShogiBoardQ が反則負けを書いた USEN（`~0.7ku2jm.i` のように `i` だけ）は、USEN の作者の実装では「invalid move format」で読めない | 仕様では `i` の後ろに反則手の3文字が要る。`UsenExporter` は終局の行の符号だけを書く（`usenexporter.cpp:33,45,65`）。ShogiBoardQ の終局の行は反則手そのものを持たない（入玉宣言の失敗など、盤に指した手の無い反則もある）ので、そのままでは仕様どおりに書けない（T1）<br>**状態: 済（f9695dc1）**。反則負けは書かないと決めた（2026-11-01）。保存ダイアログで USEN を選ぶと、消費時間がなくても、USEN に書けない終局の行（詰み・反則負け・反則勝ちなど）があれば確認を出す（`KifuSaveDialogs::RecordContents::terminals`）。上書き保存・自動保存・CLI の変換で確認しないのは、ほかの形式と同じ（F20・T5） |
| F32 | USEN の作者の実装が書く反則手つきの USEN（`~0.7ku2jm.i7bq`）を、ShogiBoardQ は読めない。<br>- 形式を自動で判別すると KIF と誤認し、手数0の空の棋譜を `ok: true` で返す（警告も無い）<br>- 形式に usen を指定すると `parse_error`（「USEN: Move 3 '.i7' could not be decoded」） | 終局の符号の規則が3か所にある。<br>- 形式の判別の正規表現が、終局の符号を英字0〜1文字に限る（`kifuformat.cpp:165-166` の `(?:\\.[a-zA-Z]?)?`）<br>- 行の分解は、最後の `.` の後ろが表の符号と一致するときだけ符号とみなす（`usencodec.cpp:135-143`）。`i7bq` は一致しないので、指し手の続きとして読む<br>- 表（`terminalcodes.cpp:53-62`）は1文字の完全一致。作者の実装は先頭の1文字で決める<br>判別できないテキストを KIF として読み、指し手が0でも成功とする経路も、黙って空の棋譜を返す原因になっている（T1）<br>**状態: 済（f9695dc1）**。終局コードは先頭の1文字で決め、`i` の後ろの反則手は読んで警告し、局面には指さない（`UsenCodec` の終局コードの読み、`UsenLine::illegalMove`）。形式の判別は `UsenCodec::hasUsenShape` を使う。指し手も終局も開始局面の指定もない棋譜を失敗にする GUI の規則（`KifuLoader::applyRecord`）を `KifuLoadParser::parseContent` に移し、CLI・自動化 API も同じにした。対局者名だけの KIF は、GUI と同じく CLI でも読めない |

### 1.2 コードで確かめたもの

経路を読んで確かめた。アプリでは実行していない。

**棋譜の移動と編集**（T2）

| # | 食い違い | 原因 |
|---|---|---|
| F9 | 対局中は、棋譜欄の行・局面送りのボタン・分岐ツリー・評価値グラフを押せない。ところが、分岐候補欄の行（「本譜へ戻る」も）と、定跡ウィンドウの「N手目の…で定跡を外れています」ボタンは押せる。<br>押すと表示中の手が移り、`BoardSyncPresenter` が盤を移った先の局面に置き換える（`boardsyncpresenter.cpp:28`）。この盤は対局の盤 `GameBoard` と同じ1つ（`mainwindowcomposer_create.cpp:183`）なので、次のことが起きる。<br>- エンジンの次の手は、置き換わった局面で合法かを判定される（`matchcoordinator.cpp:182-184`）。非合法ならエンジンの反則負け、合法なら別の局面の行が棋譜に足される<br>- 人間の手番の判定も盤の手番で行う（`humanturnpolicy.cpp:24-25`）<br>分岐候補欄に行が出る場面はふつうにある（待ったの後に別の手を指した、読み込んだ棋譜の途中から対局した）。自動化 API の `kifu.goto`（`automationcommands_kifu.cpp:204`）も、動作状態を見ずに移る | 「棋譜を移ってよいか」は、表の `{E::WidgetNavigation, kBrowsing}`（`uistatepolicymanager.cpp:99,238-242`）にしかない。<br>- `RecordPane::setKifuViewEnabled` が止めるのは棋譜の表 `m_kifu` だけで、分岐候補の表 `m_branch` は止めない（`recordpane.cpp:358-371`）<br>- 定跡ウィンドウのボタンは `KifuNavigationController::selectPly` へ直接つながる（`josekiwindowcontroller.cpp:58`）<br>- `KifuNavigationController` の利用者の操作が通す `confirmLeave`（`kifunavigationcontroller.cpp:127`）は、未更新のコメントと局面編集中かの2つだけを見る<br>**状態: 済（ba6c80dd）**。`HumanTurnPolicy::canBrowseRecord`（表の WidgetNavigation の行から求める）を `KifuNavigationController` の移る前の確認の最初に足した。分岐候補欄（`RecordPane::setNavigationEnabled`）と定跡の「外れた手」（`JosekiWindow::setRecordOperationsEnabled`）も表で止め、`kifu.goto` は invalid_state を返す |
| F10 | 局面編集中に、定跡ウィンドウの「棋譜に展開…」を押せる。押すと編集前の局面に定跡の手が足され、ツリーの変更を受けて `BoardSyncPresenter` が盤を編集前の局面に戻す。編集中の配置は消え、足した手も編集を終えると消える（`mainwindowcomposer_wire.cpp:143`）。<br>また、棋譜解析の最中は、分岐ツリーからは削除・本譜にするができないのに、分岐候補欄の右クリックからはできる | 「棋譜（分岐ツリー）を変えてよいか」の規則が入口ごとに違う。<br>- 盤で指した手: 待機中だけ（`humanturnpolicy.cpp:18-21`）<br>- 分岐ツリーの右クリック: クリックの許可（表の kBrowsing）と `canEdit`（`branchtreeview.cpp:665`）<br>- 分岐候補欄の右クリック: `canEdit` だけ（`branchtreeeditcontroller.cpp:64`）<br>- 定跡の展開: `canEdit` だけ（`josekiwindowcontroller.cpp:129`）<br>`BranchTreeEditController::canEdit()` は「ライブ対局が無い」かしか見ない（`branchtreeeditcontroller.cpp:28-31`）<br>**状態: 済（ba6c80dd）**。`HumanTurnPolicy::canEditRecord`（移ってよく、対局の棋譜を書いていない）を分岐の編集・定跡の展開・盤の手の記録で使う。検討中・詰み探索中の分岐の編集は許すと決めた（2026-11-02）。展開は条件を聞く前にも確かめ、知らせの文を「対局中・棋譜解析中・局面編集中は…」にした |
| F11 | 棋譜解析の結果の行と、評価値グラフの解析の点をクリックすると、書きかけのコメントを「保存・破棄・キャンセル」と聞かずに、書いていた手へ黙って確定する | どちらも確認を通さない `goToPlyOnLine` を直接呼ぶ（`kifuanalysiscontroller.cpp:208`、`mainwindowcomposer_wire.cpp:78`）。`commentcoordinator.cpp:30`・`.h:20` の「利用者が別の手へ移るときは移る前に確認を通している」は、この2つの入口では成り立たない<br>**状態: 済（ba6c80dd）**。どちらも確認を通す `KifuNavigationController::selectPlyOnLine` を使う（表示中の手と手順が変わらなければ聞かない）。やめたら結果の行の選択を表示中の手に戻す |

**通信対局**（T3）

| # | 食い違い | 原因 |
|---|---|---|
| F12 | 通信対局の開始で、その対局の通信ログ（LOGIN〜Game_Summary〜AGREE〜START）、USI ログ（usi〜readyok）、思考ドックの E1 のエンジン名が消える。ローカル対局では消えない。評価値グラフの凡例も、エンジン名ではなく代わりの名前になる | 通信対局は、対局条件を受けた時点でエンジンを起動し（`csagamecoordinator_setup.cpp:191-200`）、START で棋譜を作り直す（`csagamecoordinator.cpp:327`）。棋譜の作り直しの `KifuDocument::replaced` を受けて、`EngineDisplayPresenter::clearAll` がログとエンジン名を消す（`mainwindowcomposer_wire.cpp:108`、`enginedisplaypresenter.cpp:74-90`、`usicommlogmodel.cpp:46`）。ローカル対局は棋譜を作り直した後でエンジンを作る。凡例は文書の対局者のエンジン名から作る（`enginedisplaypresenter.cpp:150-151`）が、通信対局は対局者を人間の名前だけで記録する（`csagamecoordinator.cpp:342-343`）<br>**状態: 済（254ac682）**。文書の置き換えで呼ぶ `clearAll` は棋譜に結び付く表示（思考・検討・解析結果）だけを消し、結んでいるエンジンの名前は残す（`UsiCommLogModel::clearStatus`）。通信ログは対局を始めるとき（`GameSession::gameStarting`・`CsaGameSession::gameStarting`。通信対局は接続の前）に `clearEngineLogs` で消す。凡例は `CsaGameCoordinator::matchEngineStarted` で自分のエンジンの名前を渡す。新規・棋譜の読み込みでは通信ログを消さなくなった |
| F13 | 連続の通信対局で、局と局の間は「待機中」になる。接続したまま次の対局条件を待っている間に、開く・新規・対局開始・局面編集・棋譜解析・検討ができる。次の対局が始まると、未保存の確認をせずに棋譜を置き換え、動作状態とエンジンの枠を取り上げる。未保存を確かめるのは1局目だけ（`csagamesession.cpp:40`） | 動作状態 `DuringCsaGame` を持つのは対局している間だけ（`csagamecoordinator.cpp:330,421`）。接続中を表す状態が無い<br>**状態: 済（aeaf077a）**。仕様は、局と局の間は棋譜を見る・保存するだけで、「中断」と「新規」でログアウトする（2026-11-03）。動作状態 `DuringCsaConnection` を足し、`CsaGameCoordinator` が接続してから通信対局を続ける間（`sessionContinues()`）持つ。対局の間は `DuringCsaGame`。自動化 API の `ui_state` は `csa_connected`。自動保存がオフで局と局の間に保存しなかった棋譜は、次の対局で置き換わる（対局はサーバーが始めるので、確認はできない） |
| F14 | 通信対局で先読みすると、直後の `go ponder` の自分の持ち時間に、今の手の消費が反映されない。例（秒読み10秒、手番の初めの残り100秒、60秒考えて指した）: ローカル対局は btime ≈ 40000、通信対局は btime ≈ 100000。ponderhit では時間を送り直さないので、持ち時間の終わり近くでは時間切れの原因になりうる | USI の btime への換算を2つの実装が別々に書いている。<br>- `ClockTimeSource::snapshot`（`matchtimesource.cpp:34-55`）は先読みかを見る。呼ばれるのは消費を精算した後<br>- `CsaGameCoordinator::snapshot(Turn, bool /*pondering*/)`（`csagamecoordinator.cpp:425-431`）は先読みかを捨てる。残り時間は、サーバーが着手を確認する（`csagamecoordinator_moves.cpp:125-129`）まで更新されない<br>先読みの時間のテストはローカルだけ（`tst_usimatchhandler.cpp:279`）<br>**状態: 済（21be4ec5）**。時計の残りから btime/wtime への換算を `UsiGoTimeCalculator::fromClock`（1人分は `UsiClockSide`: 残り・加算を足してあるか・まだ引いていない経過時間）の1つにし、`ClockTimeSource` と `CsaGameProgress::engineTiming(pondering, 経過時間, 余裕)` が使う。通信対局は手番側の経過時間を引くので、先読みでないときも手番の経過時間を引く（ローカル対局と同じ）。通信対局の go ponder の値と、上の例（btime ≈ 40000）をテストで確かめる |
| F15 | 最大手数と中断で、棋譜の終局語とエンジンへの gameover がローカル対局と通信対局で違う。<br>- 最大手数の終局語: ローカルは「持将棋」（CSA では `%JISHOGI`）、通信対局は「最大手数到達」（`%MAX_MOVES`）<br>- 最大手数のエンジンへ: ローカルは `gameover draw`、通信対局は送らない（quit だけ）<br>- 中断のエンジンへ: ローカルは stop → `gameover draw` → quit、通信対局は quit だけ | 終局の原因の型と、原因から終局語・エンジンへの結果への対応が2つずつある。<br>- ローカル: `MatchCause::Jishogi` が持将棋と最大手数を兼ねる（`matchcoordinator.h:34`、`.cpp:335-340,393-398,426`）<br>- 通信対局: `CsaClient::GameEndCause` は MaxMoves を分ける（`csagameprogress.cpp:132-133`）。Censored・Chudan・Unknown では `finish` を呼ばない（`csagamecoordinator.cpp:397-414`）<br>CLAUDE.md の `MatchEngineSeat` の「ローカル対局・通信対局で同じ手順」と合わない<br>**状態: 済（27ccc8b0）**。仕様は、最大手数は持将棋と書き、中断でも gameover draw を送る（2026-11-03）。終局の原因 `MatchCause`（`MaxMoves` を足した）と、原因から終局の行・gameover を決める `MatchEnding` を `src/game/matchending` に置き、両方の司令塔が使う。通信対局はサーバーの結果を `CsaGameProgress::endInfo` で原因に直す。知らない原因の終局（`#WIN` 付きでも）は中断として draw を送る |

**GUI と CLI・自動化 API・MCP**（T4）

| # | 食い違い | 原因 |
|---|---|---|
| F16 | 攻方の玉がない詰将棋の SFEN（手番の字が逆）で詰みを探すと、GUI と CLI の `mate`（MCP の `search_mate`）で違う手番を調べる。例: `4k4/9/4P4/9/9/9/9/9/9 w G 1` は、GUI では先手の1手詰を調べ、CLI では後手番のまま調べる | GUI は `MateSearchPosition::forLine`（`matesearchposition.h:21-43`）で開始局面を攻方の手番に直す。この規則は analysis にあって headless・cli からも使えるが、CLI は SFEN の手番のまま送る（`clicommands_engine.cpp:300-329`）<br>**状態: 済（54fdf76d）**。GUI にそろえる（2026-11-03 に決めた）。CLI の `mate` は指し手の無い局面に `MateSearchPosition::attackerToMove` をかける。CLI・MCP の説明にも書いた |
| F17 | 詰将棋生成の条件の範囲と既定値が、GUI と CLI（`generate-tsume`）で違う。<br>- 目標手数: GUI 1〜99、CLI 1〜19<br>- 生成上限: GUI 0（無制限）〜10000・既定10、CLI 1〜100・既定5<br>- 守り駒: GUI 0〜5、CLI 0〜10<br>- 時間: GUI 1〜300秒、CLI 0.5〜60秒<br>奇数の規則も GUI と CLI に別々にあり、analysis は検査しない | 範囲がダイアログの spinbox（`tsumeshogigeneratordialog.cpp:594-618`）と `clicommandspecs.cpp:195-203` に別々にある。設定は範囲のない `Setting<int>`（`tsumeshogisettings.h:69-85`）。奇数の規則は `OddSpinBox`・`loadSettings`（`:150-152`）・`clicommands_tsume.cpp:141,195` の4か所。CLI が上限を絞ったのは意図かもしれない<br>**状態: 済（5213e9c6）**。1か所の範囲にそろえる（2026-11-03 に決めた）。services の `TsumeshogiSettings::GeneratorLimits` に範囲と奇数の規則を置き、設定を `BoundedSetting` にした。生成ダイアログの欄と CLI の表はこの宣言から作り、CLI も GUI と同じ範囲と既定値（件数の 0 = 無制限は GUI だけ）。MCP は件数を 100 までに絞る。生成器は `Settings::normalized()` で同じ範囲に収める |
| F18 | 局面編集中に、自動化 API・MCP からは駒箱（筋12）を押せない。GUI ではクリックで駒箱から指せる | `automationmethodspecs.cpp:64-66` が `file` を 1〜11 に限っている。駒箱は自動化 API の後に足したもので、表を広げていない。さらに `automationcommands_board.cpp:43-45` が駒台の段の検査を手で書き、MCP の説明（`tooldefs.py:511-512`）にも駒台の並びを写している<br>**状態: 済（0705a4e0）**。`board.click` の `file` を 1〜12 にし、駒台・駒箱の段は `BoardCoordinates::standPieceAt` の表にある段だけを受け付ける。引数 `file` の説明（段と駒の並び）もこの表から作り、MCP の説明は駒台の並びを写さずに `file` の説明を指す。局面編集中でなければ駒箱は表示されないので、今までどおり「表示されていない」として断る |
| F19 | MCP の `get_kifu`（自動化 API の `kifu.get`）では、開始局面のコメント・しおりを読めない | `from_ply` の表の下限が1（`automationmethodspecs.cpp:61`）なので、開始局面の行を出す分岐（`automationcommands_kifu.cpp:171`）には届かない。CLI の `RecordJson::moves` も `RecordLine::openingComment` を出さない<br>**状態: 済（c9889368。別の欄で返すと決めた）**。`kifu.get`（format=moves）と convert-kifu が、開始局面のコメント・しおりを `opening_comment`・`opening_bookmark` に書く（空なら省く。共通の `RecordJson::addOpening`）。`moves[]` は1手目からのままなので、今の呼び出し側は変わらない。届かなかった「開始局面」の行を作る分岐は消した |

**保存形式**（T5）

| # | 食い違い | 原因 |
|---|---|---|
| F20 | コメント・しおりはあるが消費時間はない棋譜を USEN・USI で保存すると、コメント・しおりが黙って消える。USEN の確認の文は「消費時間・コメント・しおりに対応していない」と書くのに、出す条件は消費時間があるときだけ。USI はコメントを見ない。消費時間も本譜しか見ないので、変化にだけ消費時間があっても聞かない | 形式ごとの失う情報の知識が、ダイアログの switch にある（`kifusavedialogs.cpp:70-110`）。渡される判定は `hasBranches`・`hasTimeInfo` の2つだけ（`kifufilecontroller.cpp:229-236`）。同じ知識が `kifufilecontroller.cpp:345,350` のコピーの文にもある<br>**状態: 済（d0fd008d）**。kifu の `KifuExport` に、形式が書けるものの表 `capabilities`（変化・消費時間・コメント・しおり。`writesVariations` もこの表）と、終局の行を書けるか `writesTerminal`（CSA・USI・USEN は符号の表）を置いた。棋譜を形式で書くと失うもの `lossIn`（変化を書けない形式は本譜だけを見る）を、保存の確認（`KifuSaveDialogs::saveViaDialog` の `lossOf`。switch と `RecordContents` は消した）とコピーの知らせが使う。CSA・USI の書けない終局（独自の終局理由・最大手数など）も聞くようになった。消費時間のない形式を読んだ手は 0 秒の時間を持つので、`RecordTime::isRecorded` で数えない（今までは USI・KI2 から読んだ棋譜でも、無い消費時間について聞いていた） |
| F21 | 通信対局の自動保存で選べる形式は6つで、`.ki2u`・`.usen` が無い。保存ダイアログは8つ。見出しも保存ダイアログ（「KIF形式 Shift_JIS」など）と違う | `CsaGameDialog::populateAutoSaveFormats` が一覧を手書きしている（`csagamedialog.cpp:111-120`）。`matchautosave.h:13` のコメントにも別の一覧がある（10-29 T8 の残り）<br>**状態: 済（56a8085b）**。U6 と一緒に、一覧は保存の表・既定は .kifu と決めた（2026-11-03）。`CsaGameDialog` は `KifuFormats::saveTargets()` の8形式を「名前を付けて保存」と同じ見出し（`KifuFileFilter::saveLabel()`）で並べる。コメントの一覧は消した。ガイドの画像 `csa-01-advanced.png`（4言語）は古い見出しのまま |

**表示と設定**（T6・3章）

| # | 食い違い | 原因 |
|---|---|---|
| F22 | 英語表記（Western）にして定跡手の追加・編集ダイアログを開くと、指し手と予想応手の一覧だけが日本語の KIF 表記になる。同じダイアログの「定跡手: …」のラベルと定跡ウィンドウの表は英語表記 | 一覧は `KifMoveText::format` を直接呼ぶ（`josekimoveinputwidget.cpp:36-38`）。表とラベルは `KifuPresentation::move` を使う（`josekitablemodel.cpp:29-33`、`josekieditcontroller.cpp:61`）（M3）<br>**状態: 済（7eea6c89）**。一覧も表・ラベルと同じ `KifuPresentation` で作る（局面を読み直さない `KifuPresentation::move(Position, UsiMove, Options)` を足した）。英語表記なら成と不成は `+`・`=` で見分ける。日本語表記の一覧には、表・棋譜欄と同じく「不成」を付けなくなった（成る手は「成」が付くので見分けられる） |
| F23 | 棋譜解析の結果ドックで A+ を押すと、次に開く棋譜解析ダイアログも大きくなる。逆も同じ | 1つの宣言 `AnalysisSettings::kifuAnalysisFontSize`（`KifuAnalysis/fontSize`。`analysissettings.h:143-144`）を、ダイアログ（`kifuanalysisdialog.cpp:100`）と結果ドック（`analysisresultspanel.cpp:33-34`）が使う。宣言の説明はダイアログだけを書く。文字の大きさの宣言41個のうち、2つの部品が使うのはこれだけ<br>**状態: 済（5be6fa99。今の値は結果ドックが引き継ぐと決めた）**。結果ドックは `kifuAnalysisResultsFontSize`（以前と同じキー `KifuAnalysis/fontSize`。既定 10）、ダイアログは新しい `kifuAnalysisDialogFontSize`（`KifuAnalysis/dialogFontSize`。未保存ならダイアログの元の大きさ）にした。キーを変えないので引き継ぎの処理は要らない |
| F24 | 詰将棋を解く画面で判定時間を変えても、一覧に戻ると捨てられる。次に問題を開くと一覧の値に戻る。解く画面が保存する `TsumePlay/timeout` は使われない | 解く画面はコンストラクタで保存した値を入れる（`tsumeplaydialog.cpp:47`）が、一覧が開く前に必ず `setProblem(..., 一覧の値)` で上書きする（`tsumecollectiondialog.cpp:447,470`、`tsumeplaydialog.cpp:180`）。閉じるときに保存する（`:65`）が、一覧はその値を読み戻さない<br>**状態: 済（45dc7af4。解く画面で変えた値は一覧に戻すと決めた）**。一覧へ戻ると、解く画面の判定時間を一覧の欄に入れる（以後の判定・再判定に使い、一覧の `TsumeCollection/timeout` に保存する）。`PlayPreferences` から判定時間を消し、範囲 1〜600 秒は `CollectionPreferences::kMinTimeoutSec`・`kMaxTimeoutSec` にして2つの欄と整える関数で使う。T6 の `problemIndex`・`lastFile` の名残はまだ残っている |
| F25 | 同じ局面集ファイルでも、局面集ビューアと詰将棋問題一覧で開き方が違う。<br>- 絞り込み: ビューアは `*.txt *.sfen` で `.usi` を出さない。問題一覧は `.usi` も出す<br>- 開けないとき: ビューアは理由を出さない。問題一覧は英語の `BoundedFile::describe` の文を日本語の文に混ぜる | 規則（`PositionCollectionFile`）は共有している。ファイルを読む処理は、ビューアがダイアログの中に持ち（`sfencollectiondialog.cpp:156-221`）、問題一覧は `TsumeCollectionSession::load`（`tsumecollectionsession.cpp:31-34`）にある（M2）<br>**状態: 済（643ff1d2）**。kifu の `PositionCollectionFile` に `readFile`（通常のファイルだけを上限まで読み、読めない理由を訳した文で返す）と `fileFilter`（`*.txt *.sfen *.usi`）を置き、ビューアと `TsumeCollectionSession::load` が使う。問題一覧は理由の文をそのまま出し、英語の `BoundedFile::describe` を混ぜない。M2 もこれで済 |
| F26 | アプリの既定の書体が 10pt でない環境（Windows 9pt、macOS 13pt）では、起動直後の思考ドックの表と情報欄の値が既定の書体の大きさになる。A+→A- と押すと、同じ「10」が本当の 10pt になる。情報欄の見出しだけは最初から 10pt | `thinkingpanel.cpp:91-92` が `if (m_fontSize.fontSize() != 10) applyFontSize();` としている。ほかのパネルは起動時に必ず大きさを付ける<br>**状態: 済（45f8308e）**。思考ドックも起動時にいつも大きさを付ける。アプリの書体が 9pt・13pt のときも、表と情報欄が 10pt で始まることをテストで確かめる |
| F27 | 外観ダイアログのプレビューと CLI の render-board は、本体の盤より対局者名とカードを約2割大きく描く | 名前の大きさ 0.30 を本体の盤（`mainwindowcomposer_create.cpp:129`）と `BoardViewerWidget`（`boardviewerwidget.cpp:47`）が別々に入れ、ほかの盤は既定値 0.36（`boardchromewidget.h:91`）のまま描く。`BoardViewerWidget` も、機能を持たない（外観のプレビュー）ときは 0.30 を入れる前に return する<br>**状態: 済（63595550）**。比を `BoardChromeWidget::kNameFontScale`（0.30）の1つにし、`setNameFontScale` は消した。本体の盤・読み筋盤・外観の見本・何も付けない盤（CLI の画像）で名前の大きさが同じことをテストで確かめる |
| F28 | 元に戻す〜貼り付けのボタンが、コメント欄では橙と緑、対局情報では灰になる | `EditActions` は文字・アイコン・ツールチップだけを決め、見た目は呼び出し側が選ぶ（`commenteditorpanel.cpp:307` は `ButtonStyles::undoRedo` など、`gameinfopane.cpp:324-328` は既定のまま）<br>**状態: 済（971ebb16）**。見た目と大きさは `EditActions::createButton()` が `PanelButton::setup` の既定で付け、ほかのパネルの小さなボタンと同じ灰色にそろえた（コメント欄の橙・緑をやめた）。対局情報とコメント欄の5つのボタンが同じ見た目と大きさであることをテストで確かめる |
| F29 | A- を押しても、定跡ウィンドウと棋譜解析の結果の表は自動の列幅が縮まない。思考の表・エンジン情報欄・対局情報は測り直して縮む | 定跡ウィンドウは `qMax(今の列幅, 見本の幅)`（`josekiwindow.cpp:383`）、結果の表は行があればいつも `qMax(width, previousWidth)`（`analysisresultspanel.cpp:192`）で、A+ で広げた幅が下限になる<br>**状態: 済（93410add）**。定跡ウィンドウの自動の幅は見本の文字が収まる幅にし、下限は作ったときの幅（`JosekiTableView::minimumColumnWidth`）にした。結果の表は全行を測り直すとき（文字の大きさ・結果の置き換え・並べ替え）は今の幅を下限にせず、行を足した・変えただけのときだけ今の幅から広げる。どちらも A+ の後の A- で元の幅に戻ることをテストで確かめる |
| F30 | 通信対局の待機ダイアログの通信ログで A+ や折り返しを変えても、ドックの通信ログは次の起動まで前の値のまま。その後ドックで A+ を押すと、ドックの古い値から計算して上書きする | `CsaLogPanel` が2つあり（ドック `mainwindowcomposer_create.cpp:166`、待機ダイアログ `csawaitingdialog.cpp:202`）、どちらも作るときに1回だけ設定を読む<br>**状態: 済（a76e6600）**。`LogPanel` が生きているパネルの一覧を持ち、文字の大きさと折り返しを変えたら、設定のキーが同じほかのパネルにも同じ値を付ける。待機ダイアログで変えた値にドックが合わせ、その後のドックの A+ が合わせた値から1つ上げること、別の設定のパネル（USI 通信ログ）は変わらないことをテストで確かめる |

### 1.3 疑い・仕様の確認が要るもの

| # | 疑い | 根拠 |
|---|---|---|
| U1 | 同じ局面を、入口によって受け付けたり断ったりする。玉が1枚もない局面と、手番でない側に王手がかかった局面を、次のように扱う。<br>- 受け付ける: GUI の貼り付け・ファイル、自動化 API の `kifu.load`<br>- 断る: 自動化 API の `position.set`、局面集 | `kifuloadparser.cpp:18-35` は `boardProblem` だけを見る。`position.set` は `SfenValidator::validate().valid` を求める（`automationcommands_position.cpp:33-37`）。`kifuloadparser.cpp:123` のコメント「GUI・CLI・自動化 API で同じ規則」と合わない。kifu と services の範囲で別々に見つかった<br>**状態: 済（7113de23）**。position.set を局面の貼り付けにそろえる（2026-11-03 に決めた）。棋譜の開始局面の規則 `KifuLoadParser::startPositionProblem` を公開し、`KifuLoader::loadPositionFromSfen`（局面の貼り付け・局面集・SFEN だけの棋譜。これまでは読めるかだけを見ていた）と `position.set` が使う。局面集・詰将棋問題一覧のファイルの行は、検討・詰み探索に使う局面なので `SfenValidator` のまま（理由を `kifuloadparser.h` と CLAUDE.md に書いた） |
| U2 | `go mate` に bestmove で答えるエンジンだと、GUI の詰み探索は終わるが、`MateOracle` を使う経路（CLI の `mate`・`verify-tsume`・詰将棋生成）は待ち続ける | GUI の詰み探索（`AnalysisSession`）は bestmove で終える（`analysissession.cpp:66,221-224`）。`MateOracle` は `checkmateReceived` しか受けない（`mateoracle.cpp:28,89-94`）。bestmove を受けると `UsiEngineSession` が答えを待つタイマーを止めるので、`timedOut` も届かない。CLI は外側の期限なしに待つ。そう答えるエンジンが実在するかは確かめていない（M1）<br>**状態: 済（e5436b4e）**。M1 と一緒に、bestmove の答えを `MateOracle` が Unknown にする。テスト用エンジン（`mock_usi_match` は go mate に bestmove で答える）で、直す前は答えないことを確かめた |
| U3 | 1局面の評価値の取り出し元が、GUI の棋譜解析と CLI の `analyze-kifu` で違う。最後の info に評価値か読み筋の片方しかないと、値が変わる | GUI は `PlyAnalysis::score`（bestmove に付いた評価値）を使う。CLI は `PlyAnalysis` を受け取っているのに、読み筋のある info だけを覚える `latestLines()[1].score` から作る（`clicommands_engine.cpp:117-121`）。先手から見た値に直す手番も、CLI は `UsiPosition::sideToMove()`、GUI は SFEN の2語目（`kifuanalysisrowpresenter.cpp:26-31`）で読む<br>**状態: 済（00b460f3）**。`PlyAnalysis::score` を先手から見た値 `blackScore` にし、`PlyAnalysisCollector` が解析した局面の手番で直す。GUI の結果の表と CLI の `analyze-kifu` はこの値を使う |
| U4 | エンジン同士の連続対局は、1局ごとにモーダルの「対局終了」で止まる。通信対局は続けるときは止めない（`Wait::No`、`csagamesession.cpp:150`） | 通信対局はサーバーの時間制限があるので、違いには理由がある。ローカルの連続対局を席を外して回す用途では、毎局止まる |
| U5 | 中断した対局の自動保存が違う。ローカル対局は保存せず（`matchcoordinator.cpp:402`）、通信対局は保存する（`csagamecoordinator.cpp:420`） | ローカル対局は再開できるためと思われる。意図した仕様なら直さなくてよい |
| U6 | 自動保存の既定の形式が、ローカル対局は `.kifu`（UTF-8）固定、通信対局は `.kif`（Shift_JIS）が既定 | `gamestartoptionsbuilder.cpp:152` と `networksettings.h:83`。F21 と一緒に決める<br>**状態: 済（56a8085b）**。既定はどちらも .kifu（2026-11-03 に決めた）。既定は `MatchAutoSave::extension` だけに置き、ローカル対局は既定のまま、通信対局は選んだことがなければ同じ既定（`csaAutoSaveFormat` の宣言の既定は空）。保存してある選択はそのまま |
| U7 | ShogiBoardQ は持将棋・千日手・連続王手の千日手を USEN の `j` で書く（`terminalcodes.cpp:59-61`。コメントは「千日手は持将棋と同じく引き分けの j で書く」）。USEN の作者の実装は `j` を宣言勝ち（`DeclareWin`。表示は「持将棋」、CSA は `%JISHOGI`）として読む。ShogiBoardQ は入玉宣言勝ちを USEN に書かず、`j` を読むと持将棋（引き分け）にする | 仕様は宣言勝ちと持将棋を1つの `j` にしていて、千日手の符号は無い。千日手を `j` で書くのをやめるか、仕様のあいまいさとして今のままにするかを決める。急がない |

---

## 2. 直す価値が大きい箇所（重要度順）

### T1. 棋譜の形式ごとに読み込みの規則が違う（core・kifu）。F1〜F6・F31・F32 の原因

**問題**

6形式とも、手順は `KifLineBuilder` で組み立て、1行は `MoveRecord::play` で作る。ところが、次の4つの規則を形式ごとに持っている。

| 規則 | 今の置き場所 | 形式ごとの違い |
|---|---|---|
| 手番側の手として局面に合うか | `MoveRecord::play`・`CsaLexer`・`PvText`（補助関数 `belongsTo` も2つ。`pvtext.cpp:18`、`csalexer.cpp:50`） | CSA だけが自分の駒を取る手・駒種の違いを断る。JKF は color を手番と比べない（F1） |
| 読めない手の扱い | `ParseReport::rejectMove`（KIF・KI2・CSA）と各変換器 | USI は飛ばす、JKF・USEN は全体が失敗する（F2）。`ParseOptions::FailRecord` は src から使われない（テストのみ: `tst_kifconverter.cpp:927`） |
| 終局の行を記録する側 | KI2 の結果行の読みだけが名指しの側、ほかは手番側 | 手番でない側の行を、ほかの形式は書けない（F3）。USEN の符号が反則勝ち・反則負けで同じ（F4） |
| USEN の終局の符号 | 形式の判別の正規表現・`UsenCodec` の行の分解・`TerminalCodes` の表 | 3か所とも1文字の符号だけを受け付け、仕様の `i`＋反則手を読めない。書くときも反則手を付けない（F31・F32） |
| 対局情報の行か | `isKifSkippableHeaderLine`（contains）・`extractHeaderGameInfo`（見出し：値）・手合割の行（行頭） | 指し手の行が対局情報として読み飛ばされる（F6）。語の一覧は `GameInfoKeys` の写しに「図」「先手番」などを足したもの |

加えて、CSA の `$TIME_LIMIT` の値を「持ち時間」の見出しに入れるので、1つの見出しの値に2つの書式が入る（F5）。

`TerminalCodes::csaSpecial` の引数は `sideToMove` と説明されている（`terminalcodes.h:17-19`）。実際の呼び出し側は、記録した側の `terminalSide()` を渡している（`csaexporter.cpp:101`、`jkfexporter.cpp:117`）。

**書き直し案**

- core の `Position` に「手番側の手として局面に合うか」を1つ置く。見るのは、動かす駒の色・自分の駒を取らない・打つ駒と打つ先。`MoveRecord::play`・`CsaLexer`・`PvText` はそれを使う。
- `KifLineBuilder::addMove` に「表記の駒」（KIF の駒名・JKF の piece・CSA の駒種）と「表記の手番」（JKF の color・CSA の符号・KI2 の印）を渡して確かめる。そうすれば、食い違いの扱いが全形式で同じになる。
- USI・JKF・USEN の読めない手も `ParseReport::rejectMove` に通す。`UsenCodec::decodeLine` は、読めた手と失敗した位置を返す形にする。局面だけになったときも警告を出す（`kifuloader.cpp:113`）。
- 終局の行の側の規則を recordentry.h に1つ置く（`perpetualCheckTerminal` を一般にする。手番でない側の反則負けを手番側の反則勝ちにする、など）。KI2 の結果行の読み込みはこれを使う。投了など手番側にそろえられない行は、名指しのまま残して警告する。
- USEN は冒頭の「確かめた仕様」に合わせる。
  - 反則勝ち（IllegalWin）は USEN に書かない（符号が無い。保存の確認の文とも合う）。
  - 終局の符号は先頭の1文字で決め、`i` の後ろの3文字は反則手として読む（盤には指さない。必要なら終局の行のコメントにする）。形式の判別の正規表現も同じ規則にし、規則は `UsenCodec` に1つだけ置く。
  - 書くときは、反則手を持っていないので仕様どおりの `i`＋反則手は書けない。反則負けを書かない（失う情報として確認に出す）か、作者の実装で読めないことを承知で `.i` を書き続けるかを決める。→ 書かないと決めた（2026-11-01）
  - 判別できないテキストを KIF として読んで指し手が0のときは、成功ではなく読めなかったと知らせる。
- 対局情報の行の読み飛ばしは、指し手の行かを先に判定し、その後で「見出し：値」の行頭の形と `GameInfoKeys` の表で決める。
- CSA の `$TIME_LIMIT` は、読むときに `TimeControlText::format` の書式に直す。または CSA 専用の見出しにして、書くときは `$TIME_LIMIT` のまま戻す。

**影響**: core 2・kifu/formats 6〜8ファイル。テストは `tst_kifconverter`・`tst_ki2converter`・`tst_csaconverter`・`tst_jkfconverter`・`tst_usiconverter`・`tst_usenconverter`・`tst_pvtext`・`tst_parsecommon`・`tst_kifu_format_compliance`。1.1 の例はどれもそのまま回帰テストにできる。

### T2. 「棋譜を移ってよいか」「棋譜を変えてよいか」が、部品の有効・無効の表にしかない（ui・app・widgets・automation）。F9〜F11 の原因

**問題**

- 「棋譜を移ってよいか」の規則は、`UiStatePolicyManager` の WidgetNavigation・WidgetBranchTree の行（部品を無効にする表）だけにある。
  - `KifuNavigationController` の利用者の操作の入口（`kifunavigationcontroller.cpp:127-191`）は、動作状態を見ない。
  - 表に載っていない部品（分岐候補欄・定跡の「外れた手」のボタン）と、自動化 API の `kifu.goto` が規則を通らない（F9）。
  - `goToPlyOnLine` は、利用者の操作（棋譜解析の結果・グラフの解析の点）にも使われているのに、確認を通らない（F11）。
- 「棋譜を変えてよいか」の規則が3通りある（F10）。
  - 盤の手は `HumanTurnPolicy` で待機中だけ。
  - `BranchTreeEditController::canEdit` はライブ対局の有無だけ。
  - 分岐ツリーの右クリックは、その両方とクリックの許可を見る。
- `BranchTreeEditController` の説明（`branchtreeeditcontroller.h:5,25`）は「右クリックメニューから分岐を編集」だけで、盤の手の記録・定跡の展開を書いていない。

**書き直し案**

- 「見るだけの状態か（kBrowsing）」と「棋譜を変えてよいか」を、`HumanTurnPolicy` の関数にする（例: `canBrowseRecord()`・`canEditRecord()`）。`canEditRecord()` は「待機中かつライブ対局が無い」。
- `KifuNavigationController` の利用者の操作は、すべて `confirmLeave` を通す。`canBrowseRecord()` を `addLeaveGuard` で足す（局面編集の確認はこれに含まれる）。棋譜解析の結果・グラフの解析の点にも、確認を通す入口（`selectPlyOnLine` など）を置く。
- `BranchTreeEditController::canEdit()` と定跡の展開のボタンは `canEditRecord()` を使う。知らせの文（「対局中は…」）も直す。
- 表の WidgetNavigation で、`RecordPane` の分岐候補の表も止める。定跡ウィンドウの「外れた手」のボタンも、同じ規則で無効にする。
- 自動化 API の `kifu.goto` は、移れない状態なら invalid_state を返す。
- 検討中に分岐の編集を許すかは、仕様で決める。→ 許す（2026-11-02。詰み探索中も同じ。盤で指した手を記録するのは待機中だけのまま）

**影響**: ui 4（`kifunavigationcontroller`・`humanturnpolicy`・`branchtreeeditcontroller`・`josekiwindowcontroller`）・widgets 2（`recordpane`・`josekiwindow`）・合成ルート・`automationcommands_kifu`。テストは `tst_ui_display_consistency`（待ったの後の対局中に候補を押しても移らない）・`tst_app_ui_state_policy`・`tst_josekiwindow`（局面編集中は展開しない）・`tst_branchtree_editing`。

**状態: 済（ba6c80dd）**。案から変えた点: `canBrowseRecord` は動作状態の集合を写さず、`UiStatePolicyManager::tablePolicy` の WidgetNavigation の行から求める。`canEditRecord` は `BranchTreeEditController` からライブ対局の有無を移した（`HumanTurnPolicy` が `BranchTreeEditController` に頼る向きを逆にした）。移る前の確認は、移ってよい状態か→未更新のコメントの順に聞く（移れないのにコメントを聞かない）。定跡の「棋譜に展開...」も WidgetNavigation の行で止める（変えてよい状態と移ってよい状態は、対局の棋譜を書いている間を除いて同じ）。`kifu.goto` のテストは `tst_automation_security`

### T3. 通信対局に、ローカル対局との差が時間・終局・表示の消去に残っている（network・game・engine・ui）。F12〜F15 の原因

**問題**

前回 T1 で、棋譜の受け渡しと動作状態の持ち主は1本になった。次の5つが、まだ別々に書かれている。

| | ローカル対局 | 通信対局 |
|---|---|---|
| 終局の原因と対応 | `MatchCause`（持将棋と最大手数を兼ねる）→ 終局語と gameover（`matchcoordinator.cpp:335-340,393-398,426`） | `CsaClient::GameEndCause`（MaxMoves を分ける）→ 終局語（`csagameprogress.cpp:132-133`）と gameover（`csagamecoordinator.cpp:397-414`）（F15） |
| USI の btime への換算 | `ClockTimeSource::snapshot`（先読みかを見る） | `CsaGameCoordinator::snapshot`＋`CsaGameProgress::engineTiming`（先読みかを捨てる）（F14） |
| 前の対局の表示の消去 | 棋譜の作り直し（`KifuDocument::replaced`）で消える。エンジンはその後に作る | 同じ消去が、エンジンの起動と接続のログの後に来る（F12） |
| 動作状態 | 対局の間 `DuringMatch` | 対局の間だけ `DuringCsaGame`。局と局の間は待機中（F13） |
| 人間の手 | ui が盤に指し（`boardmovecontroller.cpp:74-75` の `GameBoard::applyMove`）、`MatchCoordinator::onHumanMove(RecordEntry)` に渡す。エンジンの手は司令塔が指す | `onHumanMove(UsiMove)` で手を受け取る。合法手の確認を ui（`boardmovecontroller.cpp:68`）と network（`csagamecoordinator.cpp:172`）で2回する |

「すぐ指させる」の経路も違う。ローカルは `requestImmediateMove`、通信対局は `Usi::stop` を直接呼ぶ（`csagamecoordinator.cpp:224`）。

**書き直し案**

- game に、終局の原因から `TerminalType` と `GameOverResult` への対応を1つだけ置く。`MatchCause` には MaxMoves を足す。通信対局は、サーバーの結果をこの原因に直してから同じ関数を使う。中断に `gameover draw` を送るかどうかも、ここで決める。最大手数を持将棋と書くか「最大手数到達」と書くかは仕様で決める。
- USI の btime への換算は、usigotime に関数を1つ置いて2つの実装が呼ぶ。入力は「残りに次の加算を含むか」と「今の手の経過時間」。先読みのときは、自分の残りから `elapsedThisTurn(自分)` を引き、加算は引かない。
- 通信ログ・USI ログ・エンジン名の消去は、文書の置き換えから外す。対局の開始の入口（`MatchCoordinator::start` の前と、通信対局の接続の前）で行う。あわせて `UsiCommLogModel::clear()` が、エンジンを結んだまま名前を消さないようにする。評価値グラフの系列名は、通信対局ではエンジン名を記録の外から渡す。
- 通信対局の接続中（ログインからログアウトまで）を動作状態に持つ。閲覧はできて、棋譜の置き換えと開始系の操作はできない状態にする（仕様の判断が要る）。
- `MatchCoordinator::playHumanMove(UsiMove)` をエンジンの手と同じ形で置き、`GameChannel` に `playMove(UsiMove)` を足す。`BoardMoveController` の2つの分岐は1つになり、`lastMoveDestination()` は非公開にできる。→ 済（4b8d5787。`GameChannel::playMove` の結果は `Rejected`・`Played`・`Sent`。`BoardMoveController` は `setActiveChannel()` で受け取る `GameSession::activeChannel()` へ渡し、通信対局は `CsaGameCoordinator::sendHumanMove` が合法手を1回だけ確かめる。`BoardInput::CsaServer` は `Game` にまとめた）

**影響**: network 3・game 3・engine 1・ui 3・合成ルート。テストは `tst_match_end`・`tst_csagamecoordinator`（先読みの時間のテストを足す）・`tst_usimatchhandler`・`tst_wiring_contracts`・`tst_csagamesession`・`tst_i18n_workflows:427`。

### T4. GUI と CLI・自動化 API・MCP で、同じ操作の規則が違う（headless・cli・automation・analysis・mcp）。F8・F16〜F19・U1・U3 の原因

**問題**

| 規則 | GUI | CLI・自動化 API・MCP |
|---|---|---|
| 詰み探索の開始局面の手番 | `MateSearchPosition::attackerToMove` | SFEN のまま（F16） |
| 詰将棋生成の範囲・既定値・奇数 | spinbox・`OddSpinBox` | `clicommandspecs.cpp`・`clicommands_tsume.cpp`（F17） |
| 盤の座標（駒箱） | `BoardCoordinates`・`ShogiView::clickedSquare` | 表で 1〜11、駒台の段を手で検査（F18） |
| 開始局面のコメント | 棋譜欄に出る | `kifu.get` の下限が1、`RecordJson::moves` が出さない（F19） |
| 入力ファイルの検査 | （ダイアログで選ぶ） | CLI・`kifu.load`・`paths.py` が別々（F8） |
| 開始局面を受け付けるか | 貼り付け・ファイルは緩い | `position.set` は厳しい（U1） |
| 1局面の評価値 | `PlyAnalysis::score` | `latestLines()[1].score`（U3） |

**書き直し案**

- headless に `InputPath::check(path)` を置く（絶対パス・存在・通常のファイル。分類は `invalid_argument`・`not_found`）。`KifuConversionService` と `kifu.load` はこれを使い、MCP の `paths.py` も同じ分類にする。→ 済（6a41e35d）
- CLI の `mate` は、`moves` が空なら `MateSearchPosition::attackerToMove` をかける。採らないなら、SFEN の手番どおりに探すことを CLI と MCP の説明に書く。→ 済（54fdf76d。かける）
- analysis の `TsumeshogiGenerator::Settings` に範囲の定数と `normalized()`（奇数にする処理を含む）を置く。設定は `BoundedSetting` にし、GUI の spinbox と CLI の表はその定数から作る（10-29 M10 の `ConsiderationSettings` と同じ形）。CLI だけ上限を絞るなら、その理由を表に書く。→ 済（5213e9c6。設定の宣言が範囲を持つので、定数は services の `TsumeshogiSettings::GeneratorLimits` に置いた。`normalized()` は analysis）
- `board.click` の `file` の上限を `BoardConstants::kPieceBoxFile` にし、駒台の段の手書きの検査を消す（`squareClickPosition` が表示されていない座標を断る）。MCP の説明に「局面編集中は file=12 が駒箱」と書く。
- `kifu.get` で開始局面を出すかを決める。出すなら下限を 0 にし、`RecordJson` も開始局面のコメントを出す。出さないなら届かない分岐を消す。
- 開始局面を受け付ける規則を kifu に1つ置く（例: `KifuLoader::loadPositionFromSfen` が `startPositionProblem` と同じ検査をする）。`position.set` をそれより厳しくするなら、理由を仕様として書く。→ 済（7113de23。厳しくしない）
- `PlyAnalysisCollector` は解析した局面を持っている（`engineanalysisrunner.h:50`）。そこで先手から見た評価値を `PlyAnalysis` に入れ、CLI と `KifuAnalysisRowPresenter` はその値を使う。→ 済（00b460f3）

**影響**: headless 2〜3・cli 3・automation 2・analysis 2・ui 1・mcp 3。`specs.json` の作り直し。テストは `tst_kifu_conversion_service`・`tst_param_specs`・CLI の mate と generate-tsume のテスト・MCP の `test_paths.py` と盤・棋譜のテスト。

### T5. 保存形式の知識が、ダイアログ・通信対局・MCP に写っている（kifu・dialogs・ui・widgets・mcp）。F7・F20・F21 の原因。10-29 T8 の残り

**問題**

前回 T8 で、保存の経路と形式の表は1つになった。表に載っていない知識と、表から作っていない一覧が残っている。

- どの形式が分岐・消費時間・コメント・しおりを書けないかを、`kifusavedialogs.cpp:70-110` の switch と、`kifufilecontroller.cpp:345,350` のコピーの文が持つ（F20）。
- 通信対局の自動保存の形式の一覧を、`csagamedialog.cpp:111-120` が手書きしている（F21）。
- `KifuFormats` の中でも、「保存できる形式」の定義が3つある。表の `saveLabel`（`kifuformat.cpp:24-35`）、`isSavable`（`:64-67`。src の呼び出し0、テストのみ）、enum を手で並べた `savableNames`（`:69-77`）。
- 保存・開くダイアログの絞り込み `KifuFileFilter` は widgets にあるが、QtWidgets を使わない（`kifufilefilter.cpp:6-10`）。kifu の表 `SaveTarget::label` が、翻訳の文脈に widgets のこのファイルを指定している（`kifuformat.h:36`）。`forOpening` は形式と見出しの表 `kinds[]`（`kifufilefilter.cpp:39-41`）を別に持つ。同じ種類の定跡ファイルの絞り込みは kifu にある（`josekibook.cpp:494-497`）。
- MCP の説明文に、形式の名前（`tooldefs.py:198-199`）・読める形式（`:445`）・保存の拡張子8つ（`:456-457`）を写している。`save_kifu` のパスの検査は `.sfen` を含む書ける拡張子で行うが、アプリの `kifu.save` は `.sfen` を断る。
- MCP の `convert_kifu` の要約が実際の出力と合わない（F7）。

**書き直し案**

- kifu の形式の表（`kifuformat.cpp` の `kExtensions` か `KifuExport`）に、分岐・消費時間・コメント・しおりを書けるかを持たせる。`KifuFileController` は文書が持つもの（全ノードの分岐・消費時間・コメント・しおり）を数えて渡す。ダイアログは「失うもの」の一覧から文を作る。→ 済（d0fd008d。表は `KifuExport::capabilities`、失うものは `KifuExport::lossIn` で、コントローラーは数えずに形式ごとに聞く関数を渡す）
- 通信対局の自動保存の一覧は `KifuFormats::saveTargets()` から作る。ローカル対局と通信対局の既定の違い（U6）は仕様を決め、`MatchAutoSave` の既定値か設定の1か所にする。`matchautosave.h:13` のコメントの一覧を消す。→ 済（56a8085b。既定は `MatchAutoSave` の .kifu）
- `isSavable`・`savableNames` は表の `saveLabel` と `isName` から作る。→ 済（0309bbee。`savableNames` は表から作り、使っていない `isSavable` は消した）
- `KifuFileFilter` を kifu へ移す（P2）。
- CLI の `environment` に保存の拡張子（`saveTargets` から作る）を足す。MCP の `save_kifu` はそれで検査し、説明もそれから作る。`convert_kifu` の要約は、変化を書けない形式（csa・usi・sfen）のときだけ「本譜だけ」と出す。平手かどうかは CLI の結果に `is_hirate` を足して読む。→ 済（0309bbee。`kifu_save_suffixes`。説明は文のままにし、テストが environment の一覧と比べる。`convert_kifu` の要約は F7 で済み）

**影響**: kifu 2・dialogs 2・ui 1・widgets→kifu 1・game 1・cli 1・mcp 3。翻訳は約6件。テストは `tst_kifusavedialogs`・`tst_kifuformat`・MCP の `test_paths.py`・`test_kifu_management.py`。

### T6. 値の範囲・既定値・保存の持ち主が、ダイアログとパネルに散らばっている（dialogs・widgets・services）。F23・F24・F30 の原因。10-23 R10 の残り

**問題**

- **範囲の写し。** 値の型にある範囲を、ダイアログが手で写している。手本は、値の型の定数（`S::kMinYLimit` など）から作る `EvaluationChartSettingsDialog`。
  - `JosekiPlayOptionsDialog` の6つの範囲（`josekiplayoptionsdialog.cpp:32,48,56,66,73,81`）は、`JosekiPlayOptions::normalized()`（`josekiplayoptions.h:36-47`）の写し（kifu と dialogs の範囲で別々に見つかった）。
  - 駒の大きさ 90〜112（`boardappearancedialog.cpp:247`）は、`BoardVisuals::normalized()`（`boardvisuals.h:15`）の写し。
  - 詰将棋の判定時間 1〜600 が、解く画面と問題一覧に別々にある（`tsumeplaydialog.cpp:109`、`tsumecollectiondialog.cpp:112`）。
  - 文字の大きさの範囲を、宣言ではなく各パネルの引数で決めている。8〜24 が7か所、定跡ウィンドウだけ 6〜24（`josekiwindow.cpp:111`）、メニューウィンドウは 7〜24（`menuwindow.h:208-209`）。メッセージボックスは `qBound(8, …, 24)` を直に書く（`dialogfontscale.cpp:146`）。CLAUDE.md の「値の範囲は宣言に書く」と合わない。
- **設定の持ち主が2つ。**
  - `KifuAnalysis/fontSize` をダイアログと結果ドックが共有する（F23）。結果ドックは `valueOr(10)` で読むが、これは CLAUDE.md の valueOr の用途（既定値が翻訳文や実行時の場所で決まる）に当たらない。
  - 詰将棋の判定時間を、解く画面（`PlayPreferences::timeoutSec`）と問題一覧（`CollectionPreferences::timeoutSec`）が別々に持つ（F24）。
  - `CsaLogPanel` の2つの部品が同じ設定を作るときに読むだけで、互いに知らせない（F30）。
- **`PlayPreferences` の名残。** 5つの欄のうち3つが働いていない（`tsumeshogisettings.h:19-40`）。
  - `problemIndex`: src では読みも書きもしない。テスト `tst_settings_roundtrip.cpp:109` だけが使う。
  - `lastFile`: 書く所がない。読むのは問題一覧の以前の版の補い（`tsumecollectiondialog.cpp:277`）だけ。
  - `timeoutSec`: F24 のとおり、書くが効かない。
- **大きさを手で保存するダイアログが残る**（10-23 R10 の残り。CLAUDE.md の規則違反）。
  - `VersionDialog`: `resize(...)` して、デストラクタで保存する（`versiondialog.cpp:82,88`）。
  - `FontSettingsDialog`: `resize(...)` して、`done()` で保存する（`fontsettingsdialog.cpp:52,86-90`）。
  - `CsaWaitingDialog` の通信ログの窓（`csawaitingdialog.cpp:189-215`）: QDialog を直接作り、`DialogFontScale` を付けていない。大きさを手で読み（`:193-198`）、デストラクタで保存する（`:50-55`）。

**書き直し案**

- 値の型（`JosekiPlayOptions`・`BoardVisuals`・詰将棋の判定時間）に範囲の定数を置き、spinbox はそれを使う。動作は変わらない。
- 文字の大きさの宣言を `BoundedSetting` にし、`FontSizeHelper` は宣言の範囲を読む。
- 結果ドック用に `AnalysisSettings::analysisResultsFontSize`（既定10）を新しく置く。今の値を引き継ぐなら初回だけ旧キーから写す。
- 詰将棋の判定時間は、問題一覧の1つにする。解く画面で変えた値を一覧に戻すか、解く画面では保存しないかを決める（→ 一覧に戻す。45dc7af4 で済）。`problemIndex` を消し、`lastFile` は以前の版の引き継ぎとして settings の関数にするか消す。
- `CsaLogPanel` の設定は、変わったら知らせる（`FontSizeHelper` の変更の知らせを共有する）か、待機ダイアログの窓はドックのパネルを見せる形にする。
- 3つのダイアログの大きさは `DialogFontScale::install` の `options.size` に渡す。

**影響**: services 3・dialogs 7・widgets 3・style 1。テストは `tst_settings_roundtrip`・`tst_fontsettingsdialog`・`tests/gui/tst_tsume_play_gui.cpp`・`tst_tsume_collection_gui.cpp`。

---

## 3. 中くらいの候補

| # | 候補 | 問題 | 直し方の案 |
|---|---|---|---|
| M1 | 詰みの問い合わせが2系統ある（engine・analysis）。U2 の原因 | CLAUDE.md は「詰みの問い合わせ（go mate）の部品 `MateOracle`」と書く。実際の GUI の詰み探索は、`Usi::startMateSearch`（`usi.cpp:155-159`）と自前の受け口（`analysissession.cpp:64-66,197-224`）で動く。bestmove の扱いが2つで違う | `MateOracle` を、`EngineAnalysisRunner` と同じく借りた `UsiEngineSession` でも動けるようにする。GUI の詰み探索は `Usi::session()` を渡した `MateOracle` を使い、`Usi::startMateSearch` を消す。bestmove が届いたら答えを Unknown にする処理を `MateOracle` に1つ置く。テストは `tst_analysisflow`・`tst_tsume_search`<br>**状態: 済（e5436b4e）**。`MateOracle` は接続を持つか借りる。GUI の `AnalysisSession` は `Usi::session()` を借りた `MateOracle` を使い、`Usi::startMateSearch`・`Usi::searchTimedOut` を消した。bestmove の答えは `MateOracle` が Unknown にする（U2 も済）。テストは `tst_analysisflow` |
| M2 | 局面集ファイルの読み込みが2系統（dialogs・analysis・kifu）。F25 の原因 | ビューアはダイアログ自身が、`BoundedFile::read`・改行の直し・`PositionCollectionFile::parse`・読めない行の数え方・絞り込みの文を持つ（`sfencollectiondialog.cpp:156-221`）。問題一覧は `TsumeCollectionSession::load` にある | kifu の `PositionCollectionFile` に、ファイルを読む関数・絞り込み・読めない理由の訳した文を置き、2つのダイアログと `TsumeCollectionSession` が使う<br>**状態: 済（643ff1d2。F25 と一緒に直した）**。ビューアの読めない行の数え方（`parseSfenLines`。指せない手の行も数える）はダイアログに残した |
| M3 | 定跡手の表記（dialogs・ui）。F22 の原因 | 定跡手の入力欄だけが `KifMoveText::format` を直接呼ぶ（`josekimoveinputwidget.cpp:26-48`）。ui・dialogs から `KifMoveText::format` を直接呼ぶのは、ここと `kifuanalysisrowpresenter.cpp:64` だけ。同じ ui でも、`JosekiEditController` は `JosekiTableModel::moveText` を3か所で使い（`josekieditcontroller.cpp:45,61,73`）、`JosekiWindowController` は `KifuPresentation::move` を直接呼ぶ（`josekiwindowcontroller.cpp:119`） | 一覧は `KifuPresentation::move(sfen, usi, options())` で作る。同じマスへの成・不成を見分ける「不成」は、日本語表記のときだけ付ける。定跡手の表記を1つの関数にする。テスト `tst_josekiwindow.cpp:1045,1055` に英語表記のテストを足す<br>**状態: 済（7c44d6ad）**。一覧の表記と「不成」は F22（7eea6c89）で直した。定跡手の表記は `JosekiTableModel::moveText` だけにし、登録の知らせ・メニューと定跡を外れた手もこれを使う（一覧は局面を読み直さない `moveText(Position, UsiMove)`）。`kifuanalysisrowpresenter.cpp` の `KifMoveText::format` は、棋譜の表記（KIF）を値として持つためのもので、表示は `KifuPresentation::moveLabel` が表記の設定に合わせるので残した |
| M4 | position 文字列の語の分け方が `UsiPosition::split` の外に2つ（kifu・core）。10-29 M15 の残り | `PositionCollectionFile::parseLine`（`positioncollectionfile.cpp:34-59`）は split と規則が違う（SFEN は4語必須、moves は省けない）。`UsiToSfenConverter` の `looksLikePositionLine`・`looksLikeSfenBoard`・`equalsKeyword`・`splitTokens`（`usitosfenconverter.cpp:58-84`）は、M15 で消したはずの「仮の SFEN `token + " b - 1"` で判定する」手口を使う。CLAUDE.md の「split だけで…」と合わない | `UsiPosition::Words` に「position を前に書いたか」の欄を足す。局面集はそれで指した後の局面か参考手順かを決め、USI の行の選択は split と `fromSfen` で行う<br>**状態: 済（942e9e7b）**。`Words` に `positionCommand`・`startpos` を足し、局面集の行は split で分ける（手数の欄と moves を省いた行も読む）。USI 形式のファイルの行の選択は split と `fromSfen` にし、仮の SFEN の判定を消した |
| M5 | CSA の変換器がファイルを読む（kifu）。CLAUDE.md の説明と食い違う | `CsaToSfenConverter::readFileText`（`csatosfenconverter.cpp:25-55`）が `BoundedFile` でファイルを読み、`KifuLoadParser::readFileText`（`kifuloadparser.cpp:38-43`）は CSA だけをそこへ振り分ける。CLAUDE.md:108 は「ファイルは `KifuLoadParser` が読んで渡す」。文字コードの宣言を読むのは CSA だけで、KIF・KI2 はアプリ自身が書いた `#KIF … encoding=`（`kifexporter.cpp:191-192`）を読まずに自動判別する。`csatosfenconverter.cpp:5`・`jkftosfenconverter.cpp:5` の「本ファイルはファイルI/O…」も古い | `KifuTextDecoder` に「宣言を見てから判別する」decode を置き、読み込みの経路を1本にする。KIF・KI2 の宣言の扱いもそこで決める<br>**状態: 済（7a0cb90c）**。`KifuTextDecoder::decodeAuto` が BOM の次に先頭のコメントの行の宣言（KIF・KI2 の `#KIF … encoding=`、CSA の `'CSA encoding=`・`'encoding=`）を見て、宣言どおりに読めるときだけ従う（形式で分けない。値は UTF-8 と Shift_JIS だけ）。`CsaToSfenConverter::readFileText`・`KifuLoadParser::readFileText` を消し、`parseFile` は形式によらず `readAllTextAuto` で読む。UTF-8 と宣言した Shift_JIS の CSA も文字化けしない。KIF・KI2 は本文に日本語の指し手があるので、宣言で結果が変わることは実際にはない |
| M6 | `CsaGameCoordinator` の残りの責務（network） | cpp は3ファイル合計1,002行、ヘッダー325行。1局の進行のほかに、裁定待ちの小さな状態機械（`_moves.cpp:20-56`）、対局数と再接続（`_setup.cpp:207-243`、`.cpp:237-250`）、対局条件を受け入れるかの判断とログ文（`_setup.cpp:119-165`）を持つ。10-29 M12 で `CsaClient` に指摘したのと同じ分け方。LOGIN のパスワードを2回伏せている（`csaclient.cpp:383-389` の後に `csagamecoordinator.cpp:452-463`。後者は実質的に効いていない） | 受け入れの判断とログ文は `CsaGameSummary` の関数にする。裁定待ち、対局数と再接続を小さな部品に出す。パスワードを伏せるのは client だけにする。T3 と一緒に進める<br>**状態: 済（1300a29e）**。`CsaGameSummary::unsupportedReason`・`conditionLines`、裁定の待ち `CsaVerdictWait`、対局数と再接続 `CsaGameSeries` に出した。パスワードは `CsaClient` だけが伏せる。T3 の「すぐ指させる」も `Usi::requestImmediateMove` にそろえた。司令塔の cpp は3ファイル合計1,002行から947行、ヘッダーは336行から315行になった |
| M7 | 持将棋ルールだけが対局の条件に入っていない（game） | 対局ダイアログで選ぶのに `MatchStartOptions` に入らず、宣言のたびに `NyugyokuJudgement::configuredRule()`（`nyugyokujudgement.cpp:19-22`）が設定を読む。game の層で設定を読むのはここだけ。設定を読む所が3つ（`nyugyokudeclarationhandler.cpp:36`、`gamechannel.cpp:43`、`nyugyokujudgement.cpp:87`）、「なし」のとき27点法で代える処理が2つある。`gamesettings.h:138` のコメントの `NyugyokuJudgement::Rule` は存在しない（実際は `JishogiCalculator::Rule`） | `StartGameDialogData` と `MatchStartOptions` に持将棋ルールを持たせ、司令塔の `options()` から読む<br>**状態: 済（d7e2b2b1）**。`StartGameDialogData`・`MatchStartOptions` に `jishogiRule`（nullopt は「なし」）を足し、コンボの位置からの換算は対局ダイアログだけにした。入玉宣言は人間もエンジンも `MatchCoordinator::declareNyugyoku(declarer)` が `options().jishogiRule` で `NyugyokuJudgement::judgeDeclaration` により判定し、27点法で代えるのはそこだけ。`GameChannel::judgesDeclaration` は宣言を受け付けるかの `acceptsDeclaration` にした。`configuredRule`・`judgeEngineDeclaration` を消し、`gamesettings.h` のコメントを直した。対局中に対局ダイアログは開けないので、利用者に見える動作は変わらない |
| M8 | `KifuDocument` の `MatchTimeRecord`（kifu・app）。10-29 T4 の残り | 持ち時間・開始日時・終了日時を持つが、外から読むのは `GameSession::noteContinuedMatch`（`gamesession.cpp:217`）だけ。しかも読むのは、直前に自分で渡した持ち時間と直前に入れた現在時刻。`hasTimeControl` はテストのみ。`matchtimerecord.h:11-14` の「棋譜の書き出しに使う記録」は古い | `noteContinuedMatch` は持ち時間を引数で受け取り、`MatchTimeRecord` を消す。CLAUDE.md の `KifuDocument` の説明（「対局の記録（対局者・持ち時間・開始と終了の日時）」）を直す<br>**状態: 済（c91073a6）**。`MatchTimeRecord` と `KifuDocument::timeRecord` を消し、持ち時間と開始・終了の日時は対局情報にだけ書く。`noteContinuedMatch` は持ち時間を引数で受け取る。終了日時は終局のたびに今を書く（以前は1回目の終了日時を残したが、終局は1局に1回で、再開すると消すので変わらない）。CLAUDE.md の説明も直した |
| M9 | 入玉宣言の閾値と点数の写し（core・game・dialogs）。10-23 の残り | 「敵陣に10枚」を `jishogiscoredialog.cpp:71-74` と `nyugyokujudgement.cpp:40` が書き、「/ 10枚」の文にも 10 を直書きする。core は `jishogicalculator.cpp:96` だけにすべき。説明文の 24・31・28・27 点（`jishogiscoredialog.cpp:85-89`）も `winningPoints`・`requiredPoints` の写し | core に `hasEnoughPiecesInCamp(score)` などを置き、2か所はそれを使う。説明文の点数は `%1` で core から入れる<br>**状態: 済（6f5996fb）**。`JishogiCalculator` に `kRequiredPiecesInCamp`・`hasEnoughPiecesInCamp` と駒の点数 `kMajorPiecePoints`・`kMinorPiecePoints` を置いた。点数のダイアログと `NyugyokuJudgement` の説明文（判定の説明の 31・24・30 点も同じ写しだった）の数は、これと `winningPoints`・`requiredPoints` から入れる。表示は変わらない。敵陣の段（一〜三段・七〜九段）は M10 で扱う |
| M10 | 駒の標準の枚数と敵陣の表の写し（core・analysis） | 駒の枚数の表が3つある（`Position::pieceBox` の `shogiposition.cpp:281-284`、`PositionValidator::kLimits` の `positionvalidator.cpp:15`、`TsumeshogiPositionGenerator::kMaxCounts` の `tsumeshogipositiongenerator.h:49`）。後者は独自の `PieceType` と `kCanPromote`（`:46,55`。core の `isPromotable` の写し）も持つ。`JishogiCalculator` は敵陣を `rank<=3`・`>=7` と手書きする（`jishogicalculator.cpp:39-59`。`PieceRules::inPromotionZone` の写し） | `PieceRules` に `PieceKind` の順の枚数の表を1つ置く。敵陣は `inPromotionZone` を使う<br>**状態: 済（709f7387）**。`PieceRules::kSetCounts`・`setCount` を駒箱・`PositionValidator`・`TsumeshogiPositionGenerator` が使う。候補局面の生成は自前の `PieceType`・`kCanPromote` をやめて `PieceKind`・`isPromotable` を使い、行き所のない段の判定（`needsPromotion`。`PieceRules::isDeadSquare` の写しだった）も core の規則にした。持将棋の敵陣は `inPromotionZone`。動作は変わらない |
| M11 | QAction とチェックの結び付け（widgets） | `ThinkingPanel` は `ensureAction`・`bindCheckBox`（`thinkingpanel.cpp:19-35`）、`JosekiWindow` は `ensureAction`（`josekiwindow.cpp:91-99,256-262`）、`ConsiderationPanel` は手書き（`considerationpanel.cpp:40-45,131-133`）。src は必ず QAction を渡すので、QAction を作る分岐と引数なしのコンストラクタはテストのためだけにある | widgets に結び付けの関数を1つ置き、QAction を必須にする。テスト7つ（`tst_candidate_arrows`・`tst_analysisflow`・`tst_memory_lifecycle`・`tst_layout_boundary_widget`・`tst_josekiwindow`・`tst_game_info_pane`・`tests/gui/tst_ponder_flow`）を直す<br>**状態: 済（0e1b530b）**。widgets に `ActionBinding::bindCheckable(button, action)` を置き、3つのパネルのチェックはこれで結ぶ。QAction は必須にし（`Q_ASSERT`）、パネルが QAction を作る分岐と引数なしのコンストラクタを消した。直したテストは、引数なしで作っていた `tst_josekiwindow`・`tst_app_ui_state_policy`・`tst_game_info_pane`・`tst_layout_boundary_widget`・`tst_memory_lifecycle`・`tst_analysisflow`・`tests/gui/tst_gui_functional`（`tst_candidate_arrows`・`tst_ponder_flow` は QAction を渡していたので直さずに通った）。動作は変わらない |
| M12 | ツールチップのエスケープ（models・widgets）。セキュリティ監査 S12 の共通の関数がない | 外部の文字列をツールチップに出すときのエスケープを6か所で書いている（`josekitablemodel.cpp:16-20`、`gameinfoeditmodel.cpp:49-52`、`kifurecordlistmodel.cpp:66-69`、`shogienginethinkingmodel.cpp:41`（改行を変換しない）、`menubuttonwidget.cpp:211`、`branchnodeitem.cpp:105`）。エンジン名は `engineselector.cpp:67` と `ElideLabel::setFullText`（`elidelabel.cpp:20`）がエスケープせずに入れる（危険度は低い） | style か widgets に `plainToolTip()` を1つ置く<br>**状態: 済（5331484e）**。style に `plainToolTip(text)`・`plainToolTip(QStringList)` を置いた（エスケープし、`white-space:pre-wrap` で空白と改行もそのまま出す。段は区切り線で分ける）。6か所と、エスケープしていなかったエンジン名（`EngineSelector` のコンボの項目とコンボ自身・`EngineInfoWidget`・`ElideLabel`）をこれで出す。以前の `<br/>` に替える書き方は連続した空白を詰めていた。`GlobalToolTip` はエスケープした文字を自動判定のラベルに入れていたので、「&」を含む対局者名が「&amp;」と見えていた。ラベルを PlainText にして直した。自動化 API の部品の一覧の `tooltip` は、見える文字（`shownToolTipText`）で返す。テストは `tst_plain_tooltip` |
| M13 | 対局情報の表示の文字を、モデルとデリゲートが別々に作る（models・widgets） | `GameInfoEditModel::presentedText`（`gameinfoeditmodel.cpp:194-200`。読み上げ・ツールチップ・列幅）と `GameInfoValueDelegate::initStyleOption`（`gameinfopane.cpp:53-65`。描画）が同じ処理を持つ。片方だけ変えると、列幅と描いた文字がずれる | モデルに表示用の役割を1つ置き、デリゲートはそれを読む<br>**状態: 済（463c764e）**。`GameInfoEditModel::PresentedTextRole` を足し、表のデリゲートは項目名・内容の表記と案内文を自分で作らずにこれを描く（案内文を薄く描くのは書いたままの値が空のとき）。項目名の列幅もこれで測る。読み上げとツールチップも同じ文字。今の表記の規則では2つの結果は同じだったので、動作は変わらない。テストは `tst_game_info_pane` の `cellsShowPresentedText` |
| M14 | 局面編集の操作の入口が2つ（board） | ドラッグは行き先 → `PositionEditSession::applyEditMove` と進むが、右クリックの巡回は `BoardInteractionController` が直接 `PositionEditRules::cyclePiece` を呼ぶ（`boardinteractioncontroller.cpp:298`）。「局面編集を1か所で受け持つ」（`positioneditsession.h:19`）と合わない | `BoardInputRoute` に右クリックの入口を足し、`PositionEditSession` が処理する<br>**状態: 済（002df4df）**。`BoardInputRoute::cycle(square)` を足し、`BoardInteractionController::onRightClick` は局面編集中なら盤を変えずに行き先へ渡す（`togglePiecePromotionOnClick` を消した）。主画面の `BoardMoveController::cycle` が新しい `PositionEditSession::cycleEditPiece` に渡し、詰将棋を解く画面は何もしない。利用者に見える動作は変わらない。テストは `tst_legal_move_highlights` の `rightClickCycleGoesToRoute` と `tst_service_initialization` の `positionEditRightClickReachesTheSession` |
| M15 | 盤の対局者名の補いが2か所（ui） | `PlayerInfoPresenter`（`playerinfopresenter.cpp:43`）と `PvClickController`（`pvclickcontroller.cpp:58-60`）が別々に「先手」「後手」を補う。読み筋盤は対局中も対局情報の名前を使うので、途中から続けた対局では本体の盤と名前が違う | 名前を決める関数を `PlayerInfoPresenter` の静的関数か `KifuDocument` に1つ置く<br>**状態: 済（1edf18d2）**。`PlayerInfoPresenter::boardName(document, activity, side)` を足し、本体の盤と読み筋盤（`PvClickController::showPvBoard`。`Deps` に `activity` を足した）が使う。補いは `PvBoardDialog::setPlayerNames` にもあった（3か所目）ので消した。続けた対局は対局情報の先手・後手の行を書き換えるので、名前が違ったのは、棋譜に先手・後手の行が無く省略名だけのときと、対局中に対局情報を編集したとき。テストは `tst_service_initialization` の `pvBoardPlayerNamesFollowMainBoard` |
| M16 | 知らせの経路（ui・app・widgets） | ui・app の5ファイル8か所が `ErrorBus` 経由で知らせる（題名は「Warning」などの決まった語）: `considerationuicontroller.cpp:43,50,57`、`tsumesearchcontroller.cpp:31,43`、`kifuanalysiscontroller.cpp:221`、`kifufilecontroller.cpp:305`、`gamesession.cpp:164`。ほかは `NotificationPresenter` を直接呼ぶ。`ErrorBus` が要るのは core（`shogiboard.cpp:51`）だけ。また `JosekiWindow` は `QMessageBox` を2か所で出す（`josekiwindow.cpp:504,530`）。`showMoveFailed` を呼ぶのは ui の `JosekiWindowController` だけ（`josekiwindowcontroller.cpp:170,176`）なので、ui の知らせが widgets 経由で `QMessageBox` の静的関数で出ている（CLAUDE.md の「ダイアログは出さない」とも合わない） | ui・app は `NotificationPresenter` を直接呼ぶ。`showMoveFailed` を消し、コントローラが `NotificationPresenter::warning` で出す。`playMoveAt` の案内をパネルに残すなら CLAUDE.md を直す<br>**状態: 済（6b3a32f3）**。8か所を題名付きの `NotificationPresenter::warning`・`critical` にした（深刻度は以前のまま）。`tst_structural_kpi` の `uiAndAppUseNotificationPresenter` で、ui・app から `ErrorBus` に流していないかも検査する。`showMoveFailed` を消し、`playMoveAt` の案内もパネルに残さなかった。「着手」はいつも `josekiMoveSelected` を出し（表を作った後で指せなくなっていれば着手ボタンを消す）、`JosekiWindowController` が指せるかを確かめて知らせる。`JosekiWindow` は `QMessageBox` を使わなくなった。テストは `tst_analysisflow`（`ScopedCapture` で確かめる形にした）・`tst_josekiwindow` の `testStalePlayLeavesNoticeToController`・`tst_service_initialization` の `josekiMoveNoticesComeFromController` |
| M17 | 入力欄の読み書きを手で書くダイアログ（dialogs・ui）。10-23 R10 の残り | `KifuAnalysisDialog`（`kifuanalysisdialog.cpp:54-69,180-183`）と `TsumeShogiSearchDialog`（`tsumeshogisearchdialog.cpp:33-36,97-98`）が、spinbox とラジオの組を手で読み書きする。詰み探索のダイアログは今も getter 3つ（`engine`・`byoyomiSec`・`unlimitedTimeFlag`）で返し、呼ぶ側が `unlimited ? 0 : sec*1000` を組み立てる（`tsumesearchcontroller.cpp:50`） | 秒の欄とラジオの組を `SettingsFormBinding` に入れる。手数の範囲は `setMaxPly` の後で戻すので手書きのままでよい。詰み探索も値1つ（時間 ms。0 は無制限）で返す<br>**状態: 済（d5b74bab）**。案のとおり。`KifuAnalysisDialog` は思考時間の欄と範囲のラジオの組、`TsumeShogiSearchDialog` は制限時間の欄とラジオの組を `SettingsFormBinding` で読み書きする（開始・終了手数は手書きのまま）。詰み探索のダイアログは時間を `timeLimitMs()`（ms。0 は無制限）の1つで返し、`byoyomiSec()`・`unlimitedTimeFlag()` を消した（`TsumeSearchController` はそのまま `byoyomiMs` に渡す）。棋譜解析のダイアログの OK の処理は `accept()` の上書きにし、直接呼んでも範囲と思考時間を決めて保存する（エンジンが無ければ閉じない。直す前は決めずに閉じた）。P5 も一緒に直した。テストは `tst_analysisflow` の `dialogAcceptDecidesAndSavesConditions`。GUI テストの `tst_tsume_search_dialog` は `timeLimitMs()` で確かめる |
| M18 | SFEN の手番を文字列で読む写し（ui・core）。10-29 T6 の残り | core に `SfenUtils::sideToMove`（`sfenutils.h:38`）があるのに、使うのは `pvline.cpp:64` の1か所だけ。ui に手書きの写しが3つある（`candidatearrowpresenter.cpp:153`、`kifuanalysisrowpresenter.cpp:29`、`josekiregistrationcontroller.cpp:24`）。テストからしか呼ばれない `turnToSfen`・`sfenToTurn`（`shogitypes.h:17,20`。後者は "b" 以外をすべて後手にする緩い読み）と `SfenUtils::kHirateBoardSfen`・`hirateBoardSfen`（`sfenutils.h:18-29`）もある | 3か所を `SfenUtils::sideToMove` か `Position::fromSfen` に替える。テストだけの関数は消す。小さい<br>**状態: 済（150ef3cc）**。盤の矢印の駒打ちは `Position::fromSfenLike` の手番、定跡の登録は `SfenUtils::sideToMove` で読む（`whiteToMove` を消した）。`kifuanalysisrowpresenter.cpp` の写しは U3 で消えていた。`turnToSfen`・`sfenToTurn`・`kHirateBoardSfen`・`hirateBoardSfen` を消し、テストは使わない形にした。10-29 T6 の残り（矢印の駒打ちを section で切る）もこれで無くなった。テストは `tst_candidate_arrows` の `dropArrowUsesPositionSideToMove` |
| M19 | `ShogiView` に同じ仕事の入口が2組（views） | `applyBoardAndRender`（`shogiview.cpp:219-231`）のコメント「棋譜をたどるたび・エンジンが指すたびに呼ばれる」は古い。src の呼び出しは起動時（`mainwindowcomposer.cpp:57`。直前に同じ盤を `setBoard` 済みなので何もしない）・CLI・外観のプレビューだけ。`configureFixedSizing`（`:233-238`）は `setSquareSize`（`:170-176`）とほぼ同じ | `setBoard` に描き直しを、`setSquareSize` に範囲の扱いを寄せる。影響は CLI・外観のプレビュー・テスト4つ<br>**状態: 済（1d3d3294）**。案のとおり。`applyBoardAndRender` を消し、別の盤を渡したときの描き直しは `setBoard` が行う（同じ盤なら何もしない。駒の画像は作ったとき・駒のセットと反転が変わったときに読み直していた）。`configureFixedSizing` を消し、範囲の外を端に寄せるのは `setSquareSize` が行う（以前は何もしなかった）。起動時の空振りの呼び出しと、外観の見本が局面・反転を替えるたびの呼び出しも消した。テストは `tst_board_drag_repaint` の `setBoardRepaintsOnlyWhenNeeded`（置き換え）と `tst_board_image_renderer` の `squareSizeOutsideRangeIsClamped` |

---

## 4. 置き場所と名前（移動・改名だけで済み、ロジックは変わらない）

| # | 対象 | 問題 | 案 |
|---|---|---|---|
| P1 | `BoardViewerWidget`・`TsumePositionPreview`（widgets） | views と widgets の相互 include は、この2クラスが原因（widgets→views の4か所）。どちらも盤の表示の部品で、widgets の部品を1つも include しない。views→widgets は `ElideLabel` と `GlobalToolTip` だけ（`playercard.h:14`、`playercard.cpp:6-7`、`boardchromewidget.cpp:6`）。`TsumePositionPreview` は `ShogiView::drawRanksAndFiles`（`shogiview.cpp:542-561`）の筋・段の文字の描き方を写している（`tsumepositionpreview.cpp:71-80`） | 2クラスを views へ移し、層の順を「widgets → views」にする（`tst_layer_dependencies.cpp:65` の widgets の行に "views" を足す）。筋・段の文字の描き方を `BoardStaticLayer` などの1つの関数にする。クラス名は変えないので、翻訳の文脈は変わらない<br>**状態: 済（e961ead4）**。案のとおり。2クラスを views へ移し、層の順を「widgets → views」にした（`tst_layer_dependencies` の widgets の行に views を足した。CLAUDE.md・開発者ガイドの層の順も同じ）。筋・段の文字の描き方は `BoardStaticLayer::drawCoordinates` の1つにし、`ShogiView::drawRanksAndFiles` とプレビューの写しを消した。新しい規則は移す前の配置で4か所を違反として失敗する |
| P2 | `KifuFileFilter`（widgets） | QtWidgets を使わず、kifu の表が翻訳の文脈に指定している（T5） | kifu へ移す。使う側は dialogs 2・ui 2・`tst_kifusavedialogs`。CLAUDE.md の kifuformat の行も直す |
| P3 | headless の `MateSearchRunner`・`TsumeVerificationRunner`・`KifuConversionService`・`EngineCatalog`・`SfenValidationJson` | include するのは cli だけで、自動化 API にはエンジンを受け取るメソッドがない。それなのに CLAUDE.md:192（「CLI と自動化 API が使うサービス」）・:226、各ヘッダー（`matesearchrunner.h:5`、`tsumeverificationrunner.h:5`、`kifuconversionservice.h:5`、`enginecatalog.h:4,13`、`registeredengines.h:24`）、mcp-server.md の8章が「自動化 API も使う」と書く。同じ節の「失敗は `Failure` で返す」が当てはまるのは `KifuConversionService` だけ | headless には「core〜analysis だけ」という保証があるので、置き場所はそのままにし、説明を直す |
| P4 | `AnalysisRequest`（analysis） | 棋譜解析の依頼なのに一般的な名前（`kifuanalysissession.h:20`）。検討の `AnalysisSession` の入力 `AnalysisOptions` と紛らわしい | `KifuAnalysisRequest` にする |
| P5 | dialogs の名前の名残 | `KifuAnalysisDialog::processEngineSettings` は中身が OK の処理で、説明「エンジン名、エンジン番号、…を取得する」も古い。`TsumeShogiSearchDialog` は詰み探索の制限時間に検討の名前を使う（`byoyomiSec()`・`ui->byoyomiSec`・`considerationTimeRadioButton`）。`using Engine = EngineEntry` が3つのダイアログにある（入れ子の構造体だった頃の名残）。`csagamedialog.h:38-42` の `using` に「@brief エンジン情報を格納する構造体」 | 中身に合う名前にする（M17 と一緒に）。`using` は消して `EngineEntry` を直接使う<br>**状態: 済（d5b74bab。M17 と一緒に直した）**。`processEngineSettings` を `accept()` の上書きにした。詰み探索の欄は `timeLimitSec`・`limitedTimeRadioButton`、返す値は `timeLimitMs()`。`using Engine = EngineEntry` は3つのダイアログと `EngineSelector` の4か所とも消し（`csagamedialog.h` の古い説明も消えた）、`EngineEntry` を直接使う |
| P6 | `settingskeys.h`（services） | キーの定数は、どれも1つの .cpp（analysissettings・enginelistsettings・docksettings）とテストからしか使われない。中身のない見出し `// --- Sound ---`・`// --- JosekiWindow ---`（`:13,29`）。`gamesettings.cpp:12-14` は同じ種類のキーを直書きする。Settings Update Guidelines の「キー定数は手で書かない」とも合わない | 定数を各 .cpp の無名名前空間に入れる。テスト（`tst_engineselector`・`tst_dock_layout`・`tst_settings_roundtrip`）は宣言から読む |
| P7 | 以前の版の値の引き継ぎ（widgets・dialogs） | CLAUDE.md は引き継ぎを設定の .cpp の関数に書くとするが、部品の中にある。<br>- 列幅の以前の保存形式: `thinkingtableview.cpp:41-46`（表を作るたびに3回走る）、`josekiwindow.cpp:334-339`。同じ種類の `kifuAnalysisColumnWidths`・`gameInfoColumnWidths` は設定の .cpp にある<br>- `jishogiscoredialog.cpp:106-109`（小さい保存サイズを捨てる）<br>- `tsumeshogigeneratordialog.cpp:150-152`（偶数の目標手数を直す）<br>- `tsumecollectiondialog.cpp:277`（`TsumePlay/file` の補い） | 設定の .cpp の関数にする。services は models の列の enum を include できないので、列幅は設定の側に列番号の定数を書く。T4・T6 と一緒に片付ける |

---

## 5. 名残・小さな修正

呼び出しが src・tests とも0のもの:
- `PieceSoundPlayer::enabledChanged`・`volumeChanged`・`toneChanged`（`piecesoundplayer.h:65-71`。ui・app の範囲で接続が0のシグナルはこの3つだけ）
- `Usi::search(const UsiSearchRequest&)`（`usi.h:89-90`。説明「棋譜解析の1局面など」も古い）
- `CsaGameCoordinator` の `username()`・`completedGames()`・`gameLimit()`・`gameSummary()`（`csagamecoordinator.h:162,169,175,176`）
- `MatchEngineSeat::side()` と `m_side`（`matchengineseat.h:49,81`。書くだけ）。CLAUDE.md は `MatchEngineSeat` が「手番」を持つと書くが、手番は送る局面で決まる
- `MatchStrategyHost::currentPly()`（`gamemodestrategy.h:33`。どの Strategy も呼ばない）
- `EngineRegistrationHandler::progressUpdated`（`engineregistrationhandler.h:64`。受け手がなく、訳文2つの行き先もない）
- `TsumeGameSession::legalMoves()`（`tsumegamesession.h:44`）
- `KifLineBuilder::lastIrregularity()`（`kiflinebuilder.h:74`）、`ParseReport::options()`（`kifparsetypes.h:147`）
- `CsaGameCoordinator::startEngine` の、名前で登録一覧を引く分岐（`_setup.cpp:96-104`。ダイアログは常にパスを渡す）

テストからしか使われない公開の口（公開を外すか、テストを部品の外から確かめる形にする）:
- engine・game・network・analysis: `Usi::sendSetOptionCommand`、`CsaGameCoordinator::record()`・`progress()`、`UsiBestMove::stopped`、`ThinkingInfoFormatter::processInfo` の公開、`KifuAnalysisSession::Deps::engine`、`CsaGameCoordinator()` の引数なしのコンストラクタ
- core・kifu: `ShogiBoard::pieceBoxCount`・`trySetSfen`（外から）、`ShogiClock::isRunning`、`KifuFormats::isSavable`（T5）、`RecordTime::fromText`（約40行。実際の KIF の時間は `KifLexer::timeOf` が読む。`recordtime.h:9` の「解析は取り込み境界でだけ」とも合わない）
- ui: `UiStatePolicyManager::isEnabled`・`currentState`、`KifuNavigationController::state()`。外からの呼び出しが0: `UiStatePolicyManager::tablePolicy`、`CandidateArrowPresenter::considerationActive`、`PositionEditRules::canMove`・`cycled`・`cleared`
- widgets・views・models:
  - `MenuWindow::favoritesChanged`（emit は4か所、src の接続は0。`tst_menu_window` の QSignalSpy だけ）
  - `ShogiView::Urgency`・`boardColors()`・`boardVisuals()`・`pieceBoxRect`・`pieceBoxCellRect`・`standPieceRect`
  - `UsiLogPanel::appendColoredLog`、`EngineInfoWidget::columnWidths()`・`fontSize()`、`StepButtons::firstButton`〜`lastButton`、`GameInfoPane::fontSize`・`setFontSize`、`ConsiderationPanel::considerationFontSize()`
  - `RecordPane` の public slots `onToggle*Column`（tests/gui が直接呼ぶので、ボタンのチェックと食い違ったまま設定を書ける）、`isNavigationDisabled`・`font*Button`
  - 内部だけで使う公開の口: `CommentEditorPanel::setCommentHtml`・`convertUrlsToLinks`、`ShogiViewHighlighting::arrowsPaintRect`、`UsiCommLogModel::setStatus`
  - `FontSizeHelper::applyTo` は `JosekiWindow`（`josekiwindow.cpp:365`）だけのためにある。そのために `fontsizehelper.h` が `dialogutils.h`（さらに `<QMessageBox>`）を include し、全パネルのヘッダーに広がっている
- `TsumeshogiVerifier::Options::requireNoSurplus`: true にするのはテストの監査ツール（`tests/tsumeshogi_collection_auditor.cpp`）だけ。そのため `TsumeVerificationRunner::statusName` の "running"・"surplus"（`tsumeverificationrunner.cpp:28,35`）は CLI が出さない。MCP の出力スキーマの enum にも無いので、もし出ると出力の検証で落ちる

届かない分岐:
- 読み筋盤の「盤の差分から直前の手を求める」経路（`PvLine::arrivalHighlight` の `pvline.cpp:66-81` と、ここだけが使う `sfendiffutils.h` の81行）。`setPrevSfenForHighlight` を呼ぶのは `pvclickcontroller.cpp:65` だけで、同時に直前の手（`UsiMove::toString()` なので必ず読める）も入れる。棋譜解析と詰将棋生成は `prevSfen` を渡さない。この分岐を通すのは `tests/tst_pvline.cpp:86-111` だけ。`setPrevSfenForHighlight`・`setArrival` の第2引数・`showPvBoard` の `prevSfen`・`sfendiffutils.h` を消す
- `JosekiWindow::setCurrentSfen` の入力の整え（"startpos" を平手に、'\r' を除く。`josekiwindow.cpp:469-472`）。唯一の呼び出し元はノードの SFEN しか渡さない。"startpos" を渡すのは `tst_josekiwindow.cpp:392` だけ（規則は `UsiPosition::sfenOf` にある）
- `ConsiderationPanel::buildConsiderationView` の `if (m_considerationModel)`（`considerationpanel.cpp:349-351`）はコンストラクタの中なので常に偽。コメントは存在しない `buildConsiderationUi()` を指す → 済（0e1b530b。M11 と一緒に直した）
- `findMatchingChild` の局面での比較と、そのコメント「KIF 本譜由来のノードなど」（`kifubranchtree.cpp:389-399,411,436-441`）。同じ備えが `kifexporter.cpp:87`、`ki2exporter.cpp:49-51`、`csaexporter.cpp:113-115` にもある。今は終局以外の行はすべて `MoveRecord::play` から作られる。`tst_kifubranchtree` が局面での比較に頼るので、直すならテストも合わせる（優先度は低い）
- `josekiimport.cpp:93-96` の、開始局面が空のときの分岐（`KifLineBuilder` が必ず開始局面を入れる。`kiflinebuilder.cpp:15`）
- `ParseOptions::FailRecord`（T1）
- `KifuFileController::loadKifuText`（`kifufilecontroller.cpp:194-199`）だけが `finishLoad` を通らない（`.h:30` の「後始末を同じ関数で行う」と合わない。違いは貼り付けダイアログを閉じない・ステータスバーに出さないことだけ）

小さな写し・二重:
- 候補手の数の上限10を書き写している（`analysissession.cpp:146`。正本は `ConsiderationSettings::kMaxMultiPV`）
- 開始局面の正規化が3か所（`GameStartOptionsBuilder::canonicalizeSfen`、それを済ませた後の `MatchCoordinator::start`（`matchcoordinator.cpp:65-70`）、正本の `UsiPosition::sfenOf`）
- 通信対局の手番を「自分の手番か」と「先後」から組み立てる式が3か所（`csagamechannel.cpp:34-38`、`csagamesession.cpp:101`、`csaclient.cpp:550`）。`CsaGameProgress::sideToMove()` を使う
- 探索の前の setoption の送り方が2つ（検討は `UsiSearchRequest::options`、棋譜解析は `engineanalysisrunner.cpp:89-90` が handler に直接送る）
- `positionKey` の1行の転送が4つ（`josekibook.cpp:91-94`、`sennichitedetector.cpp:11-15`、`kifubranchtree.cpp:389-393`、`candidatearrowpresenter.cpp:20-24`。どれも `SfenUtils::normalizeSfenKey` を呼ぶだけ）
- 指し手の表記 `entryLabel(node->record(), parent ? parent->sfen() : …)` を4か所で書く（`kifubranchlistmodel.cpp:32-36`、`kifurecordlistmodel.cpp:42-47`、`branchtreeview.cpp:56-59`、`branchtreeeditcontroller.cpp:120`）。`KifuBranchNode` 1つから表記を作る関数を1つ置く
- 挑戦履歴の文が2つ（問題一覧の札 `tsumecollectiondialog.cpp:253-254` は「挑戦済み・未正答 ／ 挑戦N回」「未挑戦」、解く画面 `tsumeplaydialog.cpp:334-335` は「未正答 ／ 挑戦N回」）
- `JishogiCalculator::isSentePiece`・`isGotePiece` は `isBlackPiece`・`isWhitePiece` への転送だけ（`jishogicalculator.cpp:155-163`、`.h:79-83`）。説明の「大文字」「小文字」と `:21-25` の ShogiBoard の添字の説明は、駒を文字で持っていた頃のもの
- 既定値の写し: `kifupastedialog.cpp:19`・`jishogiscoredialog.cpp:25` の `kDefaultSize`、`csawaitingdialog.cpp:21,193-198`（`DialogUtils::restoreDialogSize` の「>100」の確かめの手書き）、`changeenginesettingsdialog.cpp:128` の `setChecked(true)`（`PonderPreferences::sendUnreportedOption` の既定の写し）、`boardappearancedialog.cpp:211-215` の4色（`BoardColors::sameBoardPalette` の写し）
- `tsumecollectiondialog.cpp:470` は選んだエンジンを `m_engine->currentData()` で読み、`EngineSelector::selected()`（`:207`）を通らない
- 二重の保存: 問題一覧（`:70,76`）と生成ダイアログ（`:81,87`）が `done()` とデストラクタの両方で保存する。定跡の取り込みダイアログだけが、閉じても条件を保存する（デストラクタ）。展開・定跡ファイルを足す・解析結果の登録は OK のときだけ保存する
- `csaLegacyPassword.remove()` は起動時に `app/main.cpp:74` で消しているので、`csagamedialog.cpp:357,367` の2回は要らない（services と dialogs の範囲で別々に見つかった）
- `josekibook.cpp` の `qCWarning(lcUi)` 5か所は kifu へ移した後の名残。`lcKifu` にする
- `BoardImageRenderer`: `boardimagerenderer.cpp:35` の「CLI・自動化 API の引数の表も同じ範囲を検査する」（自動化 API に盤面画像のメソッドは無い）。`boardimagerenderer.h:23` の `squareSize = 50; ///< 20〜150` は `BoardSquareSize::kDefault` を使う（10-29 M9・P4 の残り）
- 成りの選び方: `DialogLauncher::choosePromotion`（`dialoglauncher.cpp:51-54`）は `PromoteDialog::ask` を呼ぶだけで、作った後の `setPromotionChooser` で注入している（`mainwindowcomposer_create.cpp:519`）。盤を作るときに `std::bind(&PromoteDialog::ask, &m_window)` を渡せば、1段と後からの注入が消える
- `DockLayoutManager::resetToDefault`・`saveDockStates`（`docklayoutmanager.cpp:48-51,249`）は `DockRegistry` を呼ぶだけ
- `TsumeSolutionReplay` は同じ正解手順を3回指す（検証は fmv、各局面は Hayanagi、表記はダイアログの `PvText`）。10-23 R4 で詰将棋の部品には Hayanagi を使ってよいとしたので、規則違反ではない。手を入れるなら後回しでよい
- analyze-kifu は、本譜を読むだけのために `KifuConversionService::convert(outputFormat=Sfen)` を呼ぶ（`clicommands_engine.cpp:229-237`）。必要なら読み込みだけの関数を置く

使っていない include:
- app: `mainwindowcomposer.cpp` の `commentcoordinator.h`・`considerationpanel.h`・`gameinfopane.h`・`playerinfopresenter.h`、`mainwindowcomposer_wire.cpp` の `QPushButton`・`playerinfopresenter.h`・`timedisplaypresenter.h`
- dialogs: `josekimovedialog.cpp` の `buttonstyles.h`、`pvboarddialog.cpp` の `analysissettings.h`、`tsumeplaydialog.cpp` の `appsettings.h`、`boardappearancepreview.cpp` の `pieceimageprovider.h`、`kifuanalysisdialog.cpp` の `<QFile>`・`<QTextStream>`・`<qmessagebox.h>`（済。d5b74bab で M17 と一緒に直した）、`csagamedialog.cpp` の `<QMessageBox>`、`startgamedialog.cpp` の `<QDir>`、`tsumecollectiondialog.cpp` の `<QFile>`、`kifusavedialogs.cpp` の `<QDateTime>`
- cli・automation: `cliargs.cpp:6-7,12`（`clicommandspecs.h`・`clioutput.h`・`QDir`）、`automationcommands_widget.cpp:13-14`（`QClipboard`・`QImage`）

古い説明・コメント:
- CLAUDE.md:
  - notificationpresenter の行の「dialogs の `UnsavedChangesPrompt`」（実体は `src/widgets/unsavedchangesprompt.h`。widgets の節にも載っていない）
  - settingsformbinding の行の「対局開始・詰将棋生成のダイアログが使う」（実際は7つ: csagame・josekiimport・josekiexpand・startgame・josekibookmerge・tsumeshogigenerator・josekianalysisregister）
  - dialogfontscale の行の「全ダイアログ共通」（`CsaWaitingDialog` の通信ログの窓は付けていない。T6）
  - MenuActionsWiring の「ここでつながないのは…だけ」に、3つ目の例外「盤に定跡手を表示」（`BoardArrowActions` が `toggled` をつなぐ）が無い（`menuactionswiring.h:34` には書いてある）
  - `MateOracle` の説明（M1）、headless の説明（P3）、`KifuDocument` の対局の記録（M8）、`MatchEngineSeat` の「手番」（上）、kifu/formats の「ファイルは `KifuLoadParser` が読んで渡す」（M5）、`UsiPosition::split` の「だけ」（M4）
  - environment コマンドの説明に `settings_file`・`version`・3つの拡張子の一覧が無い（`clicommandspecs.cpp:143` の summary も同じ）
- composition-root.md:
  - 規則5の「dialogs 層のメニューウィンドウ」（`MenuWindow` は 10-29 P2 で widgets に移った）
  - 規則6の `state()`（実際は `activity()`）
  - 「作った後に set* で注入しない」の例外に、`setPromotionChooser`・`attachDock`×2・`setSavedLayoutsMenu`・`addLeaveGuard`×2・`addPendingEditor`×2 が書かれていない
- mcp-server.md:
  - :85 `ui_state` の例の「詰将棋対局中」（そんな値は無い）
  - :108-112 ツールの表にメソッドの行（`widget.showDock` など5つ）が混ざり、`show_dock`・`click_widget`・`click_table_cell` の行が無い。4章のメソッドの表に `dock.*`・`menu.*`・`branch.click`・`clipboard.get`・`widget.click` などが無い
  - :204 `EngineListSettings::loadEngines()`（存在しない。今は `EngineCatalog::findByName` → `RegisteredEngines::load`）
  - :241 `kifu.get` の引数の `max_chars`（MCP だけの引数。アプリに渡すと断られる）
  - :272 `UiNotificationService::ScopedCapture`（今は `NotificationPresenter::ScopedCapture`）
  - :309 自動化 API が `UiStatePolicyManager` を呼ぶ（10-29 5章で消した）
  - :346 `CliTools._command()`（実際は `command()`）
  - :355 `SHOGIBOARDQ_OUTPUT_DIR` の既定 `shogiboardq-mcp`（実際は `shogiboardq-output-*` の専用フォルダ。README:27 は正しい）
  - :365「表に無ければ app_error」（実際は `internal_error`。`app_error` はどこでも使わないのに、`test_environment.py:14` の `PYTHON_ONLY` に残る）
  - :394 入力パスの検査（F8）→ 済（6a41e35d。F8 と一緒に直した）
  - :401「読み筋 20 手」（既定は12。`tooldefs.py:44`）
- versioning.md:10 は「自動化 API・MCP の version は APP_VERSION から取る」と書く。実際の MCP サーバーの版は `mcp/pyproject.toml:7` と `mcp/shogiboardq_mcp/__init__.py:8` の手書きの "2026.9.25" で、`server.py:211` がこれをクライアントに送る（APP_VERSION は 2026.10.31）。`environment()` の `version` を送るか、版を1か所にする
- MCP の Python: `jobs.py:4` の「Whole-record jobs run one position at a time…」は 10-23 R9 で消した Python 版の棋譜解析の説明。`cli_backend.py:3-5` と `server.py:81` の INSTRUCTIONS が、長い処理に `analyze_kifu` を挙げていない
- `notationstyle.cpp:12` の「起動時（と言語の変更時）に決める」: 実際に `configure()` を呼ぶのは起動時の `applicationtranslations.cpp:32` だけで、表記の変更は再起動で反映する。そのため `revision()` が変わるのはテストの中だけ
- engine・game・network:
  - `matchcoordinator.cpp:412` の「自動保存は gameEnded の受け手が行う」（今は `:402` の `finishRecord` が保存する）
  - `matchcoordinator.h:97` の「時計…はシグナルで知らせる」（時計のシグナルはない）
  - `csagamecoordinator_moves.cpp:140` の「駒音・定跡ウィンドウ」（`moveCommitted` の受け手は駒音だけ）
  - `gameboard.h:35` の「newGame・setSfen で入れ替える」（`GameBoard` に setSfen はなく、`newGame` は転送するだけ）
  - csagamecoordinator.cpp:396 の「ローカル対局の終局と同じ手順」（F15）
- core・kifu:
  - `shogiclock.h:148` の「→ TimeControlController」（存在しないクラス）
  - `sfenreplay.h:5,17,22` の「棋譜の読み込みに使う」（実際の呼び出し4つは定跡ダイアログ・定跡の表・usimodelbinding・矢印）
  - `matchtimerecord.h:11-14`（M8）、`recordtime.h:9`（上）
- app・ui:
  - F2（10-29）で既定の対局者名を空にした後も残る「対局情報の初期値「先手」「後手」」（`mainwindowcomposer.cpp:56`、`mainwindowactions.cpp:52`、`playerinfopresenter.h:19`） → 済（1edf18d2。M15 と一緒に直した）
  - `sessionresetcoordinator.h:5` の「棋譜の読み込み前」（無くなった役割）
  - `branchtreeeditcontroller.h:5,25`（T2）
  - 10-29 T3 の「`PositionEditSession` は `ShogiBoard` を受け取る」（実際は `view->board()` から読む。`positioneditsession.cpp:27,44,76`）
- widgets・dialogs:
  - `considerationpanel.h:32` の「…マネージャ」（ConsiderationTabManager の頃の名残） → 済（0e1b530b。M11 と一緒に直した）
  - `sfencollectiondialog.h:65` の「ウィンドウの大きさは DialogFontScale が保存する」（`.cpp:39` では保存しない）
  - `kifuanalysisdialog.cpp:70` の中身のないコメント「エンジン設定ボタンが押されたときの処理」 → 済（d5b74bab。M17 と一緒に直した）

---

## 6. 責務が整理できていて、手を入れなくてよい部分

- **app**
  - MainWindow（45行）、合成ルートの生成・接続・初期表示の分け方、`MainWindowActions`、`SessionResetCoordinator`
  - 範囲の Deps の欄はすべて使われている
- **core**
  - `Position`（SFEN の読み書きと指し手の適用が1か所）、`PieceRules`、`BoardCoordinates`（座標の表が1つ）、`Handicaps`
  - `UsiMove`・`UsiPosition` の値の型（M4 を除く）、`PositionValidator`→`SfenValidator` の層、`EngineMoveValidator`・fmv、`UsiScore`
  - `ShogiBoard`（局面の包みと `replacePosition` だけ）
- **kifu**
  - `KifuBranchTree`・`KifuBranchNode` の所有、型付きの知らせ、一括更新
  - `KifuDocument` の置き換えと未保存の判定
  - 全形式が `KifLineBuilder`→`MoveRecord::play` を共有する読み込みの形（違いは T1 だけ）
  - 書き出しの表 `KifuExport`、KI2・JKF の `KifuNodeReplay`、`KifuSaveFile` と形式の表
  - `TerminalTypes`・`TerminalCodes` の表の構造（中身の問題は T1）、`GameInfo`・`GameInfoKeys`
  - 定跡: `JosekiBook` の詰めた形、`JosekiBookSession`、`JosekiLines`、`JosekiMovePicker`
  - `LiveGameSession`、`KifuNavigationState`
- **engine**
  - `EngineProcessManager`・`UsiProtocolHandler`（状態機械と通番）・`UsiEngineSession`（探索の切り替えと、起動の失敗を後で知らせる処理）・`UsiMatchHandler`・`EngineSlots` と `EngineLease` の分け方
  - 前回の名残（`SearchCandidate`・`EngineLifecycleManager`・`PonderStop` など）は残っていない
- **game**
  - `MatchCoordinator` の本体、Strategy 3つと `MatchEngineSeat` の接続
  - `GameChannel`、`ConsecutiveGameSeries`、`GameStartOptionsBuilder::plan`
  - 「棋譜 → リース → 知らせ」の順は、ローカル対局と通信対局で揃っている（10-29 T1）
- **analysis**: `KifuAnalysisSession` と `EngineAnalysisRunner`（GUI と CLI で共通。10-29 T2）、検討の `AnalysisSession`、詰将棋の候補と検査のジョブ
- **network**: `CsaClient`・`CsaGameSummaryReader`・`CsaLogin`・`CsaGameProgress` の分け方、`GameState` を `Phase` から求める形
- **headless**: `ParamSpec`・`ErrorKinds`/`Failure`・`OutputPath`・`RecordJson`・`AutomationEndpoint`
- **ui**
  - 動作状態 `AppActivityState`/`ActivityLease` と、`PositionEditSession` のリースと取り上げ
  - 棋譜の受け手 `GameSession` の `MatchRecordSink` での一本化と、`GameChannel` 経由の投了・中断・即指し・入玉宣言
  - `KifuFileController` の置き換え前の検査と、自動化 API と共通の後始末
  - `DialogLauncher` の範囲（10-29 M2）、`MenuActionsWiring` の `bindCheck`、`UiStatePolicyManager` の表の形（static_assert）、`DockRegistry` の表
  - Joseki 系のコントローラの分担（10-29 T5）
  - 盤の手の入力: `BoardMoveController` と `HumanTurnPolicy::boardInput`（盤の手については1か所。T2 は盤以外の入口の話）
  - presenters: BoardSync・KifuDisplay・EvaluationGraph・TimeDisplay・UsiModelBinding・CandidateArrow（矢印の正本は `BoardArrowActions`）
- **common**: どれも小さく、層の向きも合っている
- **widgets・views・models・style**
  - `ShogiView` の分け方（`BoardGeometry`・`BoardStaticLayer`・`ShogiViewInteraction`・ハイライトと矢印・`BoardChromeWidget` と `PlayerCard`）。設定を読まず、駒箱の先後の正本は `PositionEditSession`
  - `BranchTreeView`（末尾に足しただけなら描き足す。配置は `BranchTreeLayout`）
  - 共通部品は一貫して使われている。A-/A+ は src のすべてが `FontSizeControl` を通り、列幅は5つの表が `ColumnWidthMemory` を使う。`StepButtons`・`LogPanel`・`EditActions`・`RecentFilesMenu`・`EngineSelector`・`SettingsFormBinding` も同じ
  - QDialog の派生クラスは、すべて `DialogFontScale` を付けている（T6 の2つは大きさの保存だけが手書き）
  - 局面送りの有効・無効は、棋譜欄・評価値グラフ・分岐ツリーの3つを `UiStatePolicyManager` の1か所が切り替える
  - models: 行を値で持ち、`NotationTextCache`・`ScoreText` で表記を作る。`EvaluationSeries` の書き手は1つ。`GameInfoEditModel` が持つのは履歴だけ
  - style: `BoardAppearance` が外観の唯一の持ち主で、`PieceImageProvider` はキャッシュだけ
  - 1つのダイアログだけが使う部品（`CollapsibleGroupBox`・`LongLongSpinBox`・`PlayerSetupWidget`・`TimeControlEditor`・`MenuButtonWidget`）は widgets のままでよい
- **dialogs**
  - 規則を下の層に任せている: 対局開始（`TimeControlValidator`、時間の換算）、CSA（`CsaLogin`）、定跡手（`EngineMoveValidator`）、問題一覧（`TsumeCollectionSession`）、生成（analysis の生成器・`TsumeCollectionWriter`）、持将棋（`JishogiCalculator::judge`）、駒音、外観（`BoardAppearance`・`BoardAppearanceCatalog`）
  - 共通部品: `EngineSelector`（6ダイアログ）、`ChangeEngineSettingsDialog::showFor`、`BoardViewerWidget`（4つ）、`PromoteDialog::ask`
  - `TsumePlayDialog` の `controlStates()` の表、生成ダイアログの `RunState`、`EvaluationChartSettingsDialog`（範囲を値の型から取る）
  - 1つのダイアログの部品（`PvLine`・`JosekiMoveInputWidget`・`BoardAppearancePreview`・`BoardAppearanceSamples`・`EngineSettingsOptionHandler`・`EngineOptionDescriptions`）は dialogs のままでよい
  - dialogs から ui・app・automation・cli を include している箇所は0件。受け手のないシグナルも0
- **services・automation・cli・mcp**
  - `Setting<T>` の宣言（すべて外から読み書きされる）。services は QtGui・QtWidgets を include せず、QSettings を直接開くのは `EngineProfileStore` だけ
  - `RegisteredEngines`（どの関数にも呼び出し元がある）
  - 自動化 API: メソッドと表の一致、許可リスト、`AutomationInteractions::queue` での予約（`AutomationDeferredCall` を直接使うのは `app.quit` だけ）。コマンドは GUI のコントローラ・headless・core を呼んで JSON にするだけ
  - CLI: コマンドの表・`CliCommandSpecs`・`CliStopWatcher`・Reporter の分け方。評価値の向きは `UsiScore::blackView`
  - MCP: 入力スキーマは `specs.json` から作り、CLI には `--params-json` でそのまま渡す。エラーの語は `environment` の表で検査する。ソケット・上限・拡張子は CLI から読む。ジョブ管理と `call_queued`

---

## 7. 進め方

1. **1.1 の棋譜の読み書きの食い違い（F1〜F6）を先に直す。** 黙ってデータが変わる・消えるもので、影響がいちばん大きい。1.1 の例はどれもそのまま回帰テストにできる。T1 の中で、次の順に小さなコミットに分けられる。
   - F4・F31・F32（USEN の符号）と F5（`$TIME_LIMIT`）: 表と書式の1か所ずつ。USEN は確かめた仕様に合わせる。F31 で反則負けをどう書くか（書かない・`.i` のまま）は先に決める。U7 は後でよい。→ 済（f9695dc1。F31 は書かない、F5 は読むときに直す）
   - F6（対局情報の行の判定）: 指し手の行を先に判定する。→ 済（cd783021）
   - F2（読めない手）: `rejectMove` に通す。→ 済（37aa90f9）
   - F1（局面に合う手か）: core に規則を1つ置く。→ 済（9260770f）
   - F3（終局の行の側）: recordentry の規則を一般にする。→ 済（a96fc436）
2. **T2（棋譜を移ってよいか・変えてよいか）を次に直す。** F9 は対局中にエンジンの反則負けになりうる。規則は `HumanTurnPolicy` に2つの関数を足すだけで、入口の数は少ない。→ 済（ba6c80dd。検討中の分岐の編集は許す）
3. **1.2 の小さな食い違いをまとめて直す。** F7・F11・F18・F19・F22〜F30 は、それぞれ1〜2ファイルで直せる。F23 の設定のキーを分けるときは、今の値を引き継ぐかを決める（→ 結果ドックが引き継ぐ。2026-11-02）。
4. **T3（通信対局）は、終局の対応表（F15）→ btime の換算（F14）→ 表示の消去（F12）→ 人間の手の順に進める。** 接続中の動作状態（F13）は、仕様（局と局の間に何を許すか）を決めてから。M6（`CsaGameCoordinator` の分割）は T3 と一緒に進める。
5. **T4・T5 は、MCP の `specs.json` の作り直しが要るものをまとめる。** 引数の範囲（F17・F18・F19）を変えたら、`python3 -m shogiboardq_mcp.specs build/shogiboardq-cli` で作り直す。U1（開始局面の検査）と F16（詰み探索の手番）は仕様を決めてから。
6. **T6 と3章・4章は、いつでも進められる。** P1（views と widgets の相互 include の解消）は、CMake・CLAUDE.md・`tst_layer_dependencies` の表を同じコミットで直す。
7. **5章と古い説明は、関わる項目を直すときに一緒に片付ける。** 単独で直すなら、mcp-server.md と CLAUDE.md の説明を1コミットにまとめる。
