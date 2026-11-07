# コード監査：責務の整理（2026-11-07）

「ソースコードが責務の見地から整理されているか」という依頼で、`src/` 全体（約8.7万行、757ファイル）と `mcp/` を監査した。前回の対象（452980ae）から143コミットで、src は 4,177 行増え、3,002 行減っている。

- 対象のコミットは ce114a6c。本文の行番号はこの時点のもの。
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
  - 同じ操作の入口が複数あり、規則がそのうちの1つにしかないものはないか
- 前回までの文書（10-16・10-23・10-29・11-01）と同じ指摘と、「見送り」と決めた項目は除いた。ただし、前回「済」とした項目が残っていたものは挙げた。
- 1章と2章の指摘は、範囲ごとの監査とは別に、もう一度コードを読んで確かめた。3章以降の一部は、範囲ごとの監査の確認のままである。
- GUI は起動していない。「コードで確認」は経路を読んだもので、実行では確かめていない。
- 監査ではソースを変更していない。

> **修正の進み具合**: 7章の順に直し、各項目に「状態: 済（コミット）」を書く。途中で止まったら、状態のない項目から再開する。

## 全体の評価

**責務の骨組みは整理できている。部品の中に複数の責務が混ざっている箇所は、ほとんど見つからなかった。** 6つの範囲のどれからも、設計を組み替えるほどの問題は出なかった。特に dialogs・style・services・core は、指摘のほとんどが名残と小さな写しである。

機械的に測れる指標は、どれも規則を満たしている。

| 指標 | 結果 |
|---|---|
| `tst_layer_dependencies`・`tst_structural_kpi` | すべて通る |
| ディレクトリの間で互いに include しあう組 | 0（前回の views・widgets の1組は解消） |
| 1000行を超えるファイル | 0（最大は `csaclient.cpp` の842行） |
| connect にラムダを渡す箇所・SIGNAL/SLOT マクロ | 0 |
| MainWindow | `.cpp` 45行・`.h` 48行、include は5つ |
| 6行以上同じ並びが別のファイルにある組 | 実質0（定跡の2つのダイアログの決定ボタンと、`GameChannel` の宣言の繰り返しだけ） |
| CLAUDE.md に書かれた名前 | すべて実在する（説明の中身の誤りは5章） |

**残っている問題は、部品の中身ではなく「部品の間の規則の届き方」にある。** 前回と同じ傾向で、次の2つの形が大半を占める。

1. **修正が届かなかった隣の入口。** 1つの入口で規則を決めたが、同じ操作の別の入口に届いていない。
   - 指し手の読みは全形式で1つになったが、開始局面の読みには同じ整理が及んでいない（T1）。
   - 失うものの確認は「名前を付けて保存」にだけある（T2）。
   - 入玉宣言は手番を確かめるが、投了は確かめない（T3）。
2. **状態の変化の知らせを受けていない読み手。** 定跡ウィンドウの「着手」は、局面の変化は受けるが動作状態の変化は受けない（F10）。

**いちばん重いのは T1 である。** 棋譜の開始局面が、コメントの語や見出しの書き方で黙って変わる。そのまま後の手がすべて誤った局面に載り、CLI の結果は `ok: true` になる。

**複数の範囲で別々に見つかった指摘は確度が高い。**

- 定跡ウィンドウの「着手」（F10）: app・ui と widgets
- 通信対局の結果の知らせの積み重なり（F13）: engine と app・ui
- 詰将棋の手数の上限（F17）: engine と services
- `KifuExport::lossIn` の届き方（F6・F19・F20）: app・ui と services と kifu が別の面から
- 前回 T6 の残り（T6）: dialogs と widgets

**範囲ごとの監査の間で食い違った点が1つある。** F10 で、ダブルクリックが「効かない」と「指せる」に分かれた。コードでは指せる（`josekitableview.cpp:131-139` は表の覚えを見ず、`josekiwindow.cpp:488` がその場で確かめる）。

---

## 1. 責務の重複から生じている挙動の食い違い

「CLI で再現」は `shogiboardq-cli` で出力を確かめたもの、「コードで確認」は経路を読んで確かめたもの（実行していない）。

### 1.1 実行して確かめたもの

| # | 食い違い | 原因 | 確認 |
|---|---|---|---|
| F1 | KIF・KI2 のコメントの語で開始局面が変わる。<br>(a) 平手の棋譜の `*駒の配置：まだ序盤` の後に9行以上あると「棋譜ファイルの開始局面が不正です」で読めない。<br>(b) 局面図の棋譜の末尾の `*玉の配置：良い` で局面図を捨て、平手として読む。結果は `ok: true`。<br>(c) 先手番の局面図に `*ここで後手番なら負け。手数＝30` と書くと、開始局面が `w G 31` になる。<br>(d) 局面図の前の `# ここは先手番の棋譜です` が、実際の `後手番` の行を隠す | `KifLexer::readBodPosition` が指し手・コメントを含む全行を `contains` で探す（`kiflexer.cpp:340,378-379,384-386`）。呼び出しは `kiftextparser.cpp:45`。手合割の行（`kiftextparser.cpp:53`）と局面図のないときの手番の行は行頭・完全一致で読むので、規則が2つある | CLI で再現<br>**状態: 済（cc972426）** |
| F2 | 局面図の見出しが `先手の持ち駒：` だと、持ち駒が消える（`持駒` と書けば読める）。後の打つ手に「持ち駒にない」の警告が出るだけ | 見出しの表が3つある。`isBodHandsLine` は8語（`parsecommon.cpp:84-87`）、`addBodHands` は4語（`kiflexer.cpp:302-303`）、形式の判別は4語（`kifuformat.cpp:215-218`） | CLI で再現<br>**状態: 済（f8011242）** |
| F3 | 壊れた開始局面の扱いが形式で違う。CSA の駒が8個の `P1` は段ごと捨て、`XX` は空きマスにする。KIF の局面図の `X銀` は先手の銀になる。`手合割：九枚落ち` は平手になる。どれも警告なしで `ok: true`。JKF・USEN・USI は失敗にする | 指し手の `ParseReport` に当たる規則が開始局面にない。`parseStartPos` は行の読みの結果を見ない（`csalexer.cpp:234,262,317,320`）。読めない文字は飛ばす（`kiflexer.cpp:267,270`）。知らない手合割は平手（`kiftextparser.cpp:66-68`） | CLI で再現<br>**状態: 済（8659347e）。Q3 のとおり失敗にする** |
| F4 | 持駒に玉がある SFEN（`8k/9/9/9/9/9/9/9/8K b K 1`）を、`validate-sfen` は断り、USI の棋譜の読み込みは玉を黙って消して `ok: true` にする。局面の貼り付けも同じ経路を通る（コードで確認）。`automationcommands_position.cpp:33` の「GUI の局面の貼り付けと同じ規則」と合わない | `Position::fromSfen` は持駒の `K` を受け付け、`handsSfen` は7種だけを書くので玉が消える（`shogiposition.cpp:169-173`）。`parseContent` は書き戻した文字列で検査するので `KingInHand` に届かない（`kifuloadparser.cpp:132`）。前回 U1 の「済」の残り | CLI で再現<br>**状態: 済（51bf2303）。Q4 のとおり `fromSfen` が断る** |
| F8 | `generate-tsume` のエンジンの失敗（起動しない・`go mate` に対応しない）が、MCP では `finished` になる。`engine_error` の語が失われ、「Started…」を返す | 生成器は `errorOccurred` の直後に `finished` を出す（`tsumeshogigenerator.cpp:205-206,214-215,223-224`）。CLI はどちらもイベントに書く（`clicommands_tsume.cpp:60,67`）。`JobManager._apply` は `finished` で状態を上書きする（`jobs.py:217-223`）。ほかの中継役は error で loop を抜ける。`mcp-server.md` の「error → failed」と合わない | CLI のイベント列は再現。`jobs.py` はコードで確認<br>**状態: 済（60b07c27）。error の後に finished を書かない** |
| F16 | 同じ問題の SFEN で、`mate` は攻方の手番に直して探索し、`verify-tsume` は `invalid` を返す（`4k4/9/4P4/9/9/9/9/9/9 w G 1`） | `MateSearchPosition::attackerToMove` を `mate` だけが通す（`clicommands_engine.cpp:306-308`。`clicommands_tsume.cpp:202` は通さない）。前回 F16 の修正が届いていない | CLI で再現（範囲の監査）、コードで確認<br>**状態: 済（60b07c27）** |
| F17 | `generate-tsume` は41手を受け付けるのに、`verify-tsume --target-moves 41` は「between 1 and 39」で断る。生成した問題を検査できない | 上限が4か所にある。`clicommandspecs.cpp:226`（39。根拠なし）、`tsumeshogiverifier.cpp:196`（99の直書き）、`tsumeshogicandidatescreener.cpp:14`（63）、`tsumeshogisettings.h:92`（99）。`clicommands_tsume.cpp:197` のコメントも「1〜39」と書く | CLI で再現<br>**状態: 済（60b07c27）。`GeneratorLimits` を正本にする** |
| F18 | `convert-kifu --output-format usi --output-path m.kif` は成功し、できたファイルを CLI 自身が読めない（`parse_error`） | GUI と `kifu.save` は拡張子が形式を決める（`KifuSaveFile::saveFormatForPath`）。CLI は `output_format` と `output_path` が独立（`clicommands_basic.cpp:119,123,153`）。MCP も拡張子が一覧にあるかだけを見る（`tooldefs.py:207`） | CLI で再現<br>**状態: 済（45286073）** |
| F19 | AI クライアントが `.usen`・`.usi`・`.csa` へ変換・保存すると、コメント・消費時間・しおりが消えても `warnings: []`。結果の値に出ない | `KifuExport::lossIn` の呼び出しは保存の確認とコピーの知らせだけ（`kifufilecontroller.cpp:236,328`）。`convert-kifu` は変化だけを別の式で求める（`kifuconversionservice.cpp:47`）。`kifu.save` の結果には無い（`automationcommands_kifu.cpp:120-132`） | CLI で再現（範囲の監査）<br>**状態: 済（eddf4d67）。結果の `lost`** |

### 1.2 コードで確かめたもの

| # | 食い違い | 原因 |
|---|---|---|
| F5 | 人間対エンジンで、エンジンの手番に「投了」を押すと、**エンジンの投了**として終局する。人間の勝ちになり、棋譜の終局の行・自動保存もそうなる。エンジンには `gameover lose` が届く | `MatchCoordinator::handleResign` は敗者を無条件に `sideToMove()` とする（`matchcoordinator.cpp:267-272`）。「投了」の QAction は対局中ずっと有効で、見るのは人間が参加するかだけ（`uistatepolicymanager.cpp:88,201-206`）。通信対局は `performResign` が「相手の手番です」で断り（`csagamecoordinator.cpp:213-217`）、入玉宣言は `isHumanToMove()` で断る（`nyugyokudeclarationhandler.cpp:28-32`）。`gamechannel.h:42` の説明は「入玉宣言・投了」と書くが、`isHumanToMove()` を使うのは入玉宣言だけ。テストは人間同士か時計の順序だけで、この場合を試していない<br>**状態: 済（2dd077fe）。Q1 のとおり断る。通信対局は自分の手番なら、エンジンに指させている対局でも人が投了できる（`GameChannel::canResign`）** |
| F6 | `.csa`・`.usi`・`.usen` を開き、盤で変化を足すかコメントを書いて「上書き保存」すると、その内容がファイルに入らないまま未保存の印が消える。閉じるときも確認が出ない。「名前を付けて保存」なら同じ形式で確認が出る。未保存の確認で「保存」を選んだときも同じ経路 | 確認は `KifuSaveDialogs::saveViaDialog` の中だけ（`kifusavedialogs.cpp:92-96`）。`overwriteKifuFile` → `writeToPath` は `lossIn` を見ず、`markSaved` する（`kifufilecontroller.cpp:60-65,242-253,260-271`）。開いたファイルは `finishLoad` が保存先にする（`:166`）。前回 F31 の状態欄に「上書き保存…で確認しないのは、ほかの形式と同じ」とあるが、これは現状の説明で、決めた記録はない（1.3 の Q2）<br>**状態: 済（62569abc・c75d42f6）。Q2 のとおり、失うものがあるときだけ確認する。変更が無ければ失うものは増えないので、形式を選んで保存した後の上書き保存では聞かない（GUI テスト `fileOpenSave` で分かった）** |
| F7 | `kifu.save` だけが動作状態を見ない。対局中・棋譜解析中・局面編集中でも保存し、未保存の印を消す。局面編集中は編集前の棋譜を書く。GUI の保存は kBrowsing の間だけ有効 | `automationcommands_kifu.cpp:107-120` に検査がない。`kifu.goto` は `canBrowseRecord`、`kifu.load`・`position.set` は `canReplaceRecord` を通す<br>**状態: 済（0f2fbc84）。`HumanTurnPolicy::canSaveRecord`** |
| F9 | 棋譜欄・分岐候補欄・棋譜解析の結果の表のツールチップが `plainToolTip` を通らない。KIF の独自の終局語に `<img src="file:///…">` を書いた棋譜の行にマウスを乗せると、セキュリティ監査 S12 と同じくリッチテキストとして描かれる（読み込みは CLI で再現。`△<img src=…>` が表記に入る）。同じ手を出す分岐ツリーの図は包んでいる。読み筋も、指せる手が無いとエンジンの語のまま（`kifupresentation.cpp:103`）で、思考の表は包むが結果の表は包まない | `kifurecordlistmodel.cpp:66`、`kifubranchlistmodel.cpp:47-48`、`kifuanalysislistmodel.cpp:101-108`。前回 M12 の「済」の残り。`tst_plain_tooltip` はこの3つを試していない。CLAUDE.md の「外から来た文字列」の列挙に終局語・読み筋が無い。付随して、`josekiwindow.cpp:322,713` は AutoText のラベルにファイル名を入れる<br>**状態: 済（7d1710c6）。ファイルの場所のツールチップと読み筋盤も包み、定跡ウィンドウの状態の行は PlainText にした** |
| F10 | 定跡ウィンドウの「着手」が動作状態の変化で更新されない。対局の後は「着手」ボタンが消えたままで、右クリックとダブルクリックでは指せる。検討の開始などでは逆にボタンが残り、押すと知らせが出る。別の手へ移るまで直らない | 表の `humanCanPlay` を作り直すのは、局面の変化・定跡の変化・ドックの表示だけ（`josekiwindowcontroller.cpp:54-56`）。`AppActivityState::changed` を受けない。対局は棋譜の確定の後にリースを手放す（`matchcoordinator.cpp:402-403`）ので、その後に局面の変化は起きない。`JosekiWindow::updateHumanCanPlay` は公開だが、外からの呼び出しは0<br>**状態: 済（e5d7187c）** |
| F11 | 通信対局: エンジンの初期化中に Game_Summary が届くと、起動中のエンジンを捨てて起動し直す。評価関数の読み込みが長いエンジンで、相手が待っていると、初期化を最初からやり直し、AGREE が遅れる | `onLoginSucceeded` が `startEngine()` を呼ぶ（`csagamecoordinator.cpp:322`）。`onGameSummaryReceived` は `!isReady()` なら再び呼ぶ（`csagamecoordinator_setup.cpp:137-142`）。`isReady()` は初期化中も false で、`MatchEngineSeat::start` の `release()` が前のプロセスを終わらせる。すぐ後の `:146` は初期化中を想定しており、`:109` のコメントとも合わない<br>**状態: 済（583a27e4）** |
| F12 | 通信対局: 開始局面の失敗の扱いが2通りあり、どちらも START の後に起きる。Position を読めないと警告のログだけで平手にして続け、誤った開始局面の棋譜を自動保存する。再開局面の手を指せないと `setPhase(Failed)` にするが、切断もエンジンの解放もしない | `csagamecoordinator_setup.cpp:29-36,44-53`。`failGame` を通らない（`csagamecoordinator.cpp:508-521`）。読めない局面に AGREE している<br>**状態: 済（a66802cc）。`CsaGameProgress::startLine` で同意の前に確かめ、読めなければ REJECT する** |
| F13 | 通信対局を続けると、結果の知らせが局の数だけ積み重なる。ローカルの連続対局は前の知らせを閉じる（前回 U4）。`gamesession.h` は「通信対局と同じ」と書く | `csagamesession.cpp:148-154` は `NotificationPresenter::information` の戻り値を捨てる。`gamesession.cpp:298-305` は閉じる<br>**状態: 済（e4a314c5）。`NotificationPresenter::replaceInformation`** |
| F14 | 自動保存の保存先が空のとき、通信対局は開始できず、ローカル対局は黙って保存しない。空白だけの欄の扱いも違う（通信対局は `trimmed()`） | `csagamedialog.cpp:141-143`、`startgamedialog.cpp:259-265`、`matchautosave.h:17`。`isActive()` が偽なので `autoSaveKifu` の知らせにも届かない。前回 F21・U6 で形式と既定はそろえたが、保存先の規則が残った<br>**状態: 済（625e69a6）。`MatchAutoSave::lacksDir`** |
| F15 | 局面編集中の `position.get`・`app.state` の `sfen` が編集前の局面になる。説明は「shown in ShogiBoardQ」。制限モードでは編集中の手番・持ち駒を読む手段が無い | 「表示中の局面」の定義が2つある。`AutomationCommands::currentSfen` はノードを先に読み（`automationcommands.cpp:30-35`）、`KifuFileController::boardSfen` は盤を読む（`kifufilecontroller.cpp:360-368`）<br>**状態: 済（731f84d2）** |
| F20 | `KifuExport::lossIn` の再帰が、セキュリティ監査 S5 を再び開いている。変化を書ける形式の保存・コピーで手数の深さだけ再帰する。読み込みと変換は通るのに、保存の確認で落ちる（範囲の監査の見積もりは 8MB のスタックで約10万手、Windows のメインスレッドで約1.3万手。実行は未確認） | `collectSubtree`（`kifuexport.cpp:38-43,122`）。F20（前回）の d0fd008d で足された。同じ群の `collectLines`・`forgetSubtree`・`subtreeSize`・`~KifuBranchNode` は S5 で明示のスタックにしてある<br>**状態: 済（c1b6e581）** |
| F21 | 局面編集の後の対局情報が、開始局面によらず「手合割：平手」になる。対局せずに保存すると、KIF には `手合割：平手` と局面図が両方書かれる。このアプリは局面図を優先するので読み戻せる。ほかのソフトの扱いは未確認 | `KifuDocument::startNew` が `defaultHeader()` を使う（`kifudocument.cpp:91,107`）。編集の終わりは `resetRecordForEditedPosition`（`mainwindowactions.cpp:36`）→ `resetToNewRecord` → `startNew`。`GameInfo::handicapLabel` があるのに使っていない<br>**状態: 済（14b52473）。局面図で表す局面は「その他」にする** |
| F22 | 定跡の比率の計算が3か所にあり、出現頻度がすべて0の局面の扱いが違う。比率の下限を付けると、「棋譜に展開」では全部使われ、対局では1手も使われない | 合計0のとき、`JosekiLines` は100%（`josekilines.cpp:45,52-53`）、`JosekiMovePicker` は0%（`josekimovepicker.cpp:25-28,75`）、`JosekiTableModel` は空欄（`josekitablemodel.cpp:46-47,131-132`）。誤差の `1e-9` も2か所に写している<br>**状態: 済（6cae0aa1）。合計0の局面は比率で絞らないに揃えた（`meetsMinRatio`）** |
| F23 | 「履歴」メニューの項目のツールチップ（ファイルの場所）が出ない。同じ名前のファイルが別のフォルダにあると区別できない | `RecentFilesMenu` は場所をツールチップに入れる（`recentfilesmenu.cpp:27`）が、`setToolTipsVisible(true)` は src で `pvtableview.cpp:83` だけ。Qt の既定は false。画面では未確認<br>**状態: 済（5b41d608）** |
| F24 | MCP の `load_kifu` の要約に `warnings` が出ない。読めない手で打ち切った棋譜が「Loaded N plies」としか見えない。`convert_kifu` は要約に書く | `handlers_app.py` の `load_kifu`。出力スキーマにも無い<br>**状態: 済（ecbd0030）** |

### 1.3 仕様の判断が要るもの

| # | 問い | 推奨 |
|---|---|---|
| Q1 | F5: エンジンの手番の「投了」をどう扱うか | **断る**（「投了は自分の手番で行います」）。通信対局・入玉宣言と同じ扱いで、棋譜の形式の制約（`RecordEntry::forSideToMove`）にも触れない。人間の負けとして受け付ける案は、手番でない側の投了の行が書けない<br>**状態: 済（2dd077fe）** |
| Q2 | F6: 「上書き保存」で失うものを確認するか | **失うものがあるときだけ確認する**。無ければ今のまま何も出ない。自動保存と自動化 API は確認を出さず、F19 の結果の値で知らせる<br>**状態: 済（62569abc）** |
| Q3 | F3: 開始局面を読めないとき、失敗にするか警告にするか | **失敗にする**。指し手と違い、開始局面が違うと以後の手がすべて意味を失う。知らない手合割だけは、局面図があれば局面図で読めるので今のままでよい<br>**状態: 済（8659347e）** |
| Q4 | F4: 持駒の玉を断るか、往復で保つか | **`fromSfen` が断る**。局面編集でも持ち駒にできず（`positioneditrules.cpp:51`）、使う場面がない。`startPositionProblem(Position)` の重ね書きも要らなくなる<br>**状態: 済（51bf2303）** |
| Q5 | `GameSession::startGameFromDialog` の `hasEditedStart` が前の対局の手順も見る（`gamesession.cpp:149`）。`m_record` は次の対局まで残るので、駒落ちの対局の後に「新規」や平手の棋譜の読み込みをしても、ダイアログが「現在の局面」に固定される。意図か | 意図でなければ `|| isEditedStart(m_deps.match->record().startSfen)` を消す（「中断した対局の再開のため」などの理由があれば、コメントに書く）<br>**状態: 済（19415f4e）。コードの履歴に理由が無いので消した** |

---

## 2. 直す価値が大きい箇所（重要度順）

### T1. 開始局面の読み取りに共通の規則がない（kifu）。F1〜F4 の原因

**状態: 済（cc972426・f8011242・51bf2303・8659347e）**

前回 T1 で「指し手の読み」は全形式で `KifLineBuilder` → `MoveRecord::play` に揃った。開始局面（局面図・P 行・手合割）は、この整理の外に残っている。

| 規則 | KIF・KI2 | CSA | JKF | USEN・USI |
|---|---|---|---|---|
| 読む行の範囲 | **全行**（指し手・コメントを含む） | 開始局面の行 | header | 先頭 |
| 読めない部分 | **飛ばす** | **段ごと捨てる** | 失敗 | 失敗 |
| 知らない手合割 | **平手** | 失敗（`PI`） | 失敗 | — |
| 持駒の見出し | **3つの表** | — | — | — |

**案**

- 開始局面の読みに渡す行を、最初の指し手の行より前で、コメント行を除いたものに限る。`配置：` は行頭で判定する。手番の行の判定は1つの関数にし、`initialPosition` と共有する。
- 持駒の見出しから側を返す関数を1つ置く（例: `std::optional<Turn> bodHandsSide(line)`）。3か所はそれを使う。
- 開始局面が読めないときの方針を `ParseReport` に足し（Q3）、CSA の P 行・局面図の行・手合割の名前を通す。`parseStartPos` の bool はそのために使う。
- 持駒の玉を断る（Q4）。

**影響**: `kiflexer`・`kiftextparser`・`parsecommon`・`kifuformat`・`csalexer`・`shogiposition`。1.1 の例はどれもそのまま回帰テストにできる。

### T2. 棋譜の保存の規則が入口ごとに違う（kifu・ui・automation・cli・mcp）。F6・F7・F14・F18・F19・F20 の原因

**状態: 済（62569abc・0f2fbc84・eddf4d67・45286073・625e69a6・c1b6e581）**

入口は5つある。規則は「名前を付けて保存」に集まっている。

| 規則 | 名前を付けて保存 | 上書き保存 | `kifu.save` | `convert-kifu` | 対局の自動保存 |
|---|---|---|---|---|---|
| 失うものを知らせる | 確認 | **なし** | **なし** | 変化だけ | **なし** |
| 動作状態を見る | 表で無効 | 表で無効 | **見ない** | — | 終局の時 |
| 拡張子が形式を決める | する | する | する | **しない** | 指定どおり |
| 保存先が空のとき | ダイアログ | ダイアログへ | エラー | 本文を返す | **黙って保存しない**（ローカル） |

**案**

- `overwriteExisting` に `lossOf` を渡し、`dialogParent` があるときだけ確認する（Q2）。
- `kifu.save` の入口で、保存できる状態かを表の `FileSaveAs` の行から求める関数で確かめ、`invalid_state` を返す。
- CLI と `kifu.save` の結果に `lost: [...]`（`lossIn` から）を足す。`variations_dropped` はそれから作る。MCP の要約に出す。
- `convert-kifu` は、拡張子から形式が決まるなら `output_format` との不一致を `invalid_argument` にする（kifu は kif、ki2u は ki2 として比べる）。
- `MatchAutoSave` に「有効なのに保存先が無い」の判定を1つ置き、`StartGameDialog::onStartRequested` でも止める。
- `collectSubtree` を明示のスタックにし、`longLineIsListedWithoutDeepRecursion` に回帰テストを足す。

**影響**: `kifuexport`・`kifusavedialogs`・`kifufilecontroller`・`automationcommands_kifu`・`clicommands_basic`・`matchautosave`・`startgamedialog`・`mcp`。`specs.json` は引数を変えないので作り直さなくてよいが、出力スキーマは足す。

### T3. 「人間が操作できるか」の正本が分かれている（game・network・ui・app）。F5・F10 の原因

**状態: 済（2dd077fe・5c9669fb・e5d7187c）。窓口の組は `GameChannels`（`game` と `network` を使うので network に置いた。案の ui より下の層）**

`gamechannel.h:16-19` は「受ける側は対局の種類で分岐しない」と書く。実際には次の3か所に同じ判定がある。

| 場所 | ローカル対局の条件 | 通信対局の条件 |
|---|---|---|
| `GameChannel::isHumanToMove`（`gamechannel.cpp:34-37`、`csagamechannel.cpp:36-39`） | `!isGameOver()`・`isHumanSide(sideToMove())` | 別の式 |
| `HumanTurnPolicy::boardInput`（`humanturnpolicy.cpp:23-34`） | `canAcceptMove()`・`isHumanSide(board->currentPlayer())` | `gameState()==InGame`・`isHumanPlayer()`・`isMyTurn()` |
| `MatchCoordinator::playHumanMove`（`matchcoordinator.cpp:168`） | 同じ趣旨の確認 | — |

`HumanTurnPolicy::isGameActivelyInProgress`（`:62-75`）も同じ形で分岐する。今の UI の有効・無効の範囲では違いは表に出ないが、手を受け付ける条件を変えると3か所を直すことになる。`HumanTurnPolicy::Deps` の `board`・`match`・`csaGame` は、この写しのために残っている。

原因の1つは、窓口2つを `GameSession`（app）が持つことである。`GameSession` は `BoardMoveController` より後に作り、ui から include できない。そのため `BoardMoveController::setActiveChannel` が `std::function<GameChannel*()>` を後から受け取る（`boardmovecontroller.h:50-52`、`mainwindowcomposer_create.cpp:445`）。これは `composition-root.md` の規則2と「作った後に `set*` で注入しない」に反し、「例外」の節にも載っていない。

**案**

- 投了の手番の確認を、`LocalGameChannel::resign` か `GameSession::resign` に置く（Q1）。
- 窓口2つと `activeChannel()` を小さな部品（例: `GameChannels`。`CsaGameChannel` を使うので ui に置く）に出し、`createMatchAndAnalysis()` で司令塔の直後に作る。`HumanTurnPolicy`・`BoardMoveController`・`GameSession` に Deps で渡す。
- `GameChannel::isHumanToMove()` を正本にして条件をそろえ、`boardInput`・`isGameActivelyInProgress` は窓口に聞く。
- `JosekiWindowController` の Deps に `activity` を足し、`AppActivityState::changed` を `updateJosekiWindow` か `JosekiWindow::updateHumanCanPlay` に結ぶ（F10。これだけなら単独で直せる）。

**影響**: `gamechannel`・`csagamechannel`・`humanturnpolicy`・`boardmovecontroller`・`gamesession`・`mainwindowcomposer_create`。`tst_app_game_session` の書き直しが要る。

### T4. 通信対局の開始と終局の流れに、つなぎが残っている（network・ui）。F11〜F13 の原因

**状態: 済（583a27e4・a66802cc・e4a314c5）。再接続の判断は `endGame` の引数にした**

前回 M6 で `CsaVerdictWait`・`CsaGameSeries` を切り出し、T3 で時間・終局の対応表はそろった。次の4つが残っている。

- 開始局面の検証が START の後にある（F12）。Position の解釈と再開の手の再生を、盤を使わない値の関数にする（`CsaMoveConverter::toUsi` と `MoveRecord::play` で足りる）。`onGameSummaryReceived` で `unsupportedReason` と一緒に確かめ、読めなければ REJECT する。START の後の失敗の経路がなくなる。
- エンジンの起動の条件（F11）。`!isReady() && !isInitializing()` にする。1行で済む。
- 再接続の判断が「呼ぶ前に立てるフラグ」になっている。`onClientGameEnded` の直接の呼び出し3か所と `onServerGameEnded` の経路が、それぞれ事前に `setLastEndedByServer` を書く（`csagamecoordinator.cpp:270,299,306-311,511`、`csagamecoordinator_setup.cpp:155`）。終局の経路を足すときに書き忘れると、前の値で判断する。`finishGame(bool endedByServer)` にする。`csaclient.h:253` の説明も、今は接続されていない `onClientGameEnded` を挙げている。
- 終局で動作状態が棋譜の確定より先に変わる。`setPhase(Ended)` が `updateActivity()` を呼ぶ（`csagamecoordinator.cpp:422`）ので、`finishRecord`（`:440`）より前に接続中か待機中になる。`:438-441` のコメントと CLAUDE.md の「`MatchCoordinator` と同じ順」は逆を書いている。今の受け手はどれも同期なので、見える不具合は見つからなかった。テスト `activityFollowsRecordAtStartAndEnd` は `gameEnded` の時点しか見ないので、この順を検出できない。

F13 は、「前の知らせを閉じて次を出す」小さな部品（または `NotificationPresenter` の関数）を1つ置き、`GameSession` と `CsaGameSession` で使う。

### T5. 詰将棋のヘッドレスの入口の規則が揃っていない（cli・analysis・mcp）。F8・F16・F17 の原因

**状態: 済（60b07c27）**

- `GenerateReporter` が error の後の `finished` を書かないか、`JobManager._apply` が failed を上書きしないようにする（F8）。`mcp/tests/test_jobs.py:63` に error の後に finished が来る場合を足す。
- `verify-tsume` でも `attackerToMove` を通す（F16）。通さないなら CLI と MCP の説明に書く。
- 手数の上限は `GeneratorLimits::kMaxTargetMoves` を正本にする（F17）。`tsumeshogiverifier.cpp:196` と CLI の表がそれを読む。`specs.json` の作り直しが要る。Hayanagi の 63 は探索の上限なので別の値のままでよい。

### T6. 前回 T6 の未着手の部分（dialogs・widgets・services）

**状態: 済（8dd19714）。文字の大きさを持たない通信ログの窓のために `DialogUtils::rememberDialogSize` を足した**

前回 T6 のうち F23・F24・F30 は済んだ。次の項目は `状態` の記載が無いまま残っている。指摘は前回と同じなので、理由は前回の文書を見る。

- 範囲の写し: `josekiplayoptionsdialog.cpp:32,48,56,66,73,81`、`boardappearancedialog.cpp:243`
- 文字の大きさの範囲: `logpanel.cpp:63`・`gameinfopane.cpp:68`・`considerationpanel.cpp:33`・`analysisresultspanel.cpp:33`（8〜24）、`josekiwindow.cpp:99`（6〜24）、メニューウィンドウ（7〜24）、`dialogfontscale.cpp:146`（`qBound`）。`analysisresultspanel.cpp:33-34` は `Setting<int>` なのにラムダを渡している
- 大きさの手保存: `versiondialog.cpp:82,88`、`fontsettingsdialog.cpp:52,88`、`csawaitingdialog.cpp:49,190-191`

---

## 3. 中くらいの候補

| # | 箇所 | 問題 | 案 |
|---|---|---|---|
| M1 | 盤の入力の手順（dialogs・board） | 新しい `JosekiMoveBoard`（acf2f467）が `TsumePlayDialog` の手順を写している。組み立て（`josekimoveboard.cpp:20-42`・`tsumeplaydialog.cpp:199-213`）、着手（`:70-76`・`:282-290`）、直前の手のハイライト（`:59-66`・`:297-307`）、選択の取り消し。差が1つ生まれている（読めない手のとき、詰将棋は前のハイライトを残し、定跡は消す）。`isEditing()`・`cycle()` は2つの行き先で空の実装 | `BoardInteractionController` に `showPlayedMove(UsiMove)`・`cancelInput()` を置く。`BoardInputRoute` の2つに既定の実装を与える。動作は変わらない<br>**状態: 済（e7f181e9）** |
| M2 | `KifuFileController`（ui） | ファイルの操作とクリップボードの操作が同居する（`:306-400`）。`Deps` の `board`・`navState` はコピーだけに使う。`boardSfen()` は SFEN を `section(' ', 3, 3)` で切る（10-29 T6 の残り） | `KifuClipboardController` に切り出す。移動だけで済む。`boardSfen()` は `board->toSfen()` にする（F15 と一緒に）<br>**状態: 済（5659357a）** |
| M3 | 動作状態の規則の表と関数（ui・board） | `canReplaceRecord()`（`kifufilecontroller.cpp:46-49`）と `PositionEditSession::begin`（`:46`）が `Idle` を手書きし、表の `FileOpen` などの写しになっている。`WidgetBranchTree` の行は `WidgetNavigation` と同じ値。「棋譜に展開...」は移ってよい行で有効になるので、通信対局の局と局の間でも押せて断られる。文も実態と合わない | `canReplaceRecord` を policy に移して表から読む。`WidgetBranchTree` の行をまとめる。展開のボタンは `canEditRecord` に合わせる<br>**状態: 済（dfab4276）。`PositionEditSession::begin` の待機中の判定は残した（board は ui の表を読めない層にある）** |
| M4 | `EngineDisplayPresenter`（ui） | 情報帯の手番の印は、ローカル対局では文書の対局者から枠の決まりを再現し（`:165-168,196`）、通信対局は `matchEngineStarted` の知らせから決める。枠の割り当てを変えると印だけが黙ってずれる（1729903e） | ローカル対局も司令塔が側と枠を知らせ、同じ受け口にする<br>**状態: 済（f2e139ad）。枠の決まりを `MatchPlayers::sideInSlot` に置いた。司令塔からの知らせは足していない（`Strategy` は決まりどおりに枠へ置くだけで、表示の経路を増やすほどの差がない）** |
| M5 | `KifuAnalysisRequest`（analysis・ui） | `lineEndNodeId`・`branchPlies` はセッションが読まず、同じ `KifuAnalysisController::start` が取り出すだけ（`kifuanalysissession.h:31-32`）。範囲の収め方もセッションとコントローラーに同じ形である | 2つの欄はコントローラーの引数にする（分岐ツリーのノード ID が analysis から消える）。範囲は `Options` の関数に1つ置く<br>**状態: 済（c93d0116）** |
| M6 | `JosekiImportDialog`（dialogs） | ダイアログの中でワーカーの実行と登録の手順を持つ。`QtConcurrent` を使う src のファイルのうち、analysis・kifu の外にあるのはこれだけ。同じ定跡の非同期処理（読み込み・保存・マージ）は `JosekiBookSession` が持つ | `JosekiBookSession::startImport`＋進み具合・完了のシグナル＋`Activity` の項目にする。実害は小さいので急がない<br>**状態: 済（d650dbb5）。`JosekiBookSession::startImport`** |
| M7 | エンジンの登録の規則（dialogs・engine） | 同じ実行ファイルの拒否・実行できるか・ファイルかの確認が、ダイアログとハンドラに分かれている（`engineregistrationdialog.cpp:122-134`、`engineregistrationhandler.cpp:45-49,68-71,128`）。`startRegistration` を直接呼ぶと二重に登録できる | 3つとも `startRegistration` に入れ、`errorOccurred` で知らせる<br>**状態: 済（680e11fa）** |
| M8 | `JosekiMergeDialog`（dialogs・kifu） | 「1局で同じ局面と指し手は1回」の規則と集計のキーを写している（`josekimergedialog.cpp:39-49,216-229`、`josekiimport.cpp:107-136`） | `JosekiImport` に手順1本から `Move` の一覧を作る関数を置く<br>**状態: 済（59bdddb4）。キーを `JosekiImport::moveKey` の1つにした（手順から Move の一覧を作る関数までは要らなかった）** |
| M9 | 定跡ウィンドウの文字の大きさ（widgets） | ドックになった後も `DialogUtils::applyFontSize` → `standardizeDialog` を通り、`setupUi` の余白を毎回上書きする。`DialogUtils::applyFontSize` の呼び出し元はこの1か所。A-/A+ の見た目もウィンドウが上書きする（CLAUDE.md の「パネルで決めない」と違う） | ほかのパネルと同じ形にして、`DialogUtils::applyFontSize` を消す。今の余白は `setupUi` に書く<br>**状態: 済（84bbf2cb）** |
| M10 | CSA の指し手の書式（network・kifu） | `validateMoveText`（`csagamecoordinator_moves.cpp:44-67`）・`toUsi`（`csamoveconverter.cpp:37-65`）・`CsaLexer::parseMoveLine` の3か所。`validateMoveText` の検査は直後の `toUsi` が全部行う | `validateMoveText` を消す。`toUsi` は `CsaLexer::parseMoveLine` の上に載せる<br>**状態: 済（e2e2c5aa）** |
| M11 | 画像の保存（ui） | `ImageExportController` の4関数はどれも1行の転送で、`WidgetImageExporter` の呼び出し元はここだけ | 2クラスを1つにする。ロジックは変わらない<br>**状態: 済（aad3b190）** |
| M12 | `KifLineBuilder`（kifu） | 反則の駒打ちの判定と文を写している（`kiflinebuilder.cpp:40-45`）。CLAUDE.md の「`mismatchOf` だけで決める」と合わない | `MoveRecord::mismatchText(position.mismatchOf(move, RequireHand))` にする<br>**状態: 済（d612d2a3）** |
| M13 | 形式の判別の読み取り（kifu） | `.json`・未知の拡張子のファイルは、判別と解析で2回読む（`kifuformat.cpp:132`、`kifuloadparser.cpp:48,52`）。CLAUDE.md の「ファイルを1回読み」と違う | 先に `readAllTextAuto` で読み、パスと本文から形式を決める<br>**状態: 済（6892e3e8）** |
| M14 | MCP の予約操作の宣言（mcp） | `_tool()` は出力スキーマに `queued` があるときだけ予約の説明を足す。`call_queued` を使う12ツールのうち `trigger_action`・`close_dialog` には付かない。足された説明にも古い文が残る（「Returns when queued」と、直後の「実行まで待つ」が矛盾） | `queued=True` を明示して宣言し、古い文を消す<br>**状態: 済（90d390f1）** |
| M15 | 以前の版の引き継ぎ（widgets・services） | 「名前の無い以前の版は番号・パスで探す」が `PlayerSetupWidget::load` にある（`:175-186`）。CLAUDE.md の「引き継ぎはパネルの中に書かない」と合わない | 復元の規則を `EngineListSettings` の関数にする<br>**状態: 済（af9124d7）** |
| M16 | 内蔵の詰み判定の上限（dialogs・analysis） | 31手を `tsumecollectiondialog.cpp:50,212` と `tsumepositionanalyzer.cpp:132` の文が直書きしている。正本 `kBuiltInMaxPlies` は無名名前空間にある | 定数を公開し、文は `%1` にする<br>**状態: 済（9b87a8f2）** |

---

## 4. 置き場所と名前（移動・改名だけで済み、ロジックは変わらない）

| # | 現在 | 問題 | 案 |
|---|---|---|---|
| P1 | `HumanTurnPolicy`（ui/policy） | T2（前回）で「棋譜を移って・変えてよいか」が入り、手番の判定だけではなくなった。`context.turnPolicy->canBrowseRecord()` と読みにくい | `OperationPolicy` など<br>**状態: 済（ea4beb46）。自動化 API の `turnPolicy` なども `operationPolicy` にした** |
| P2 | `CsaGameSession`（ui/coordinators） | `GameSession` の通信対局版に見えるが、棋譜を持たない。ダイアログ・ステータスバー・盤の反転だけ | 役割に合う名前にする（`CsaGameUi` など）。CLAUDE.md の役割の表に `*Session` を足す<br>**状態: 済（e3cecf96）。`CsaGameUiController`（`ConsiderationUiController` に合わせた名前）にして controllers へ移した。`ui/coordinators` は空になったので消した** |
| P3 | `CommentCoordinator`（ui/coordinators） | 複数部品の流れではなく、コメント欄1つの表示と操作 | `CommentController`<br>**状態: 済（f6f8ee63）** |
| P4 | `PvClickController`（ui/controllers） | 棋譜解析の結果からも開く。「読み筋盤を開く」部品 | `PvBoardController`<br>**状態: 済（f6046a68）** |
| P5 | `UsiCommLogModel`（models） | 中身はエンジン名・探索状況・予想手で、通信ログは行を素通しするだけ | `EngineStatusModel` など<br>**状態: 済（80f77792）** |
| P6 | `AnalysisResultsPanel::rowDoubleClicked` | 「盤面」ボタンのクリック・Enter で出る。ダブルクリックとは関係がない。同じ操作に3つの名前がある | `boardRequested` に揃える<br>**状態: 済（80f77792）** |
| P7 | `EngineListSettings`（services） | 一覧は `RegisteredEngines`・`EngineProfileStore` に移り、残るのは機能ごとの選択だけ | `EngineSelectionSettings`<br>**状態: 済（c8b47870）** |
| P8 | `TsumeMateEngine`・`TsumeEvaluation`（engine） | 使うのは analysis の Tsume 系と `tsumecollectiondialog.cpp` だけ。KomoringHeights 専用のオプションと「再判定」の訳文を持つ | analysis へ移す<br>**状態: 済（dda3e1c3）** |
| P9 | `SpecialMove`・`parseSpecialMove`・`GameOverResult`（`core/shogitypes.h:26-51`） | 基本型の見出しに USI の語が混ざる。使うのは engine と game だけ | `engine/usimessages.h` へ移す<br>**状態: 済（14324437）。既存の `usimessages.h` に移した** |
| P10 | `ScreenshotService`（ui/controllers） | 利用者の操作ではなく、自動化 API も使う | controllers の外へ移す<br>**状態: 済（14324437）。`src/ui` に置いた** |

翻訳の文脈はクラス名で決まるので、改名で訳が unfinished になったら、旧文脈の vanished 訳から埋め直す。

---

## 5. 名残・小さな修正

**呼び出しの無いもの**（src・tests を grep で確かめた）

- `Position::kingSquare`、`toSide`（`shogitypes.h:168`）
- `DialogFontScale::rescale()`。CLAUDE.md は「後から作った部品には `rescale()`」と勧めているので、消すなら説明も直す
- 受け手の無いシグナル: `longLongValueChanged`・`textValueChanged`・`expandedChanged`
- `isClickEnabled`・`isCompactLayout`・`coordinateBand`・`turnBadge`・`elideMode`
- `MatchTimeSource::elapsedThisTurn` の仮想関数: インターフェースを通る呼び出しが0。`CsaGameCoordinator` の自分の3か所だけ（F14（前回）の取り残し）

**届かない分岐・効かない設定**

- `EngineSelector::onSettingsClicked` の `QMessageBox::critical`（`engineselector.cpp:133-138,145-146`）
- `considerationpanel.cpp:241` のツールチップは `updateState()` が上書きする
- `EngineInfoWidget::onStatusChanged` の書体の比較（`engineinfowidget.cpp:336-337`）。表記の設定は実行中に変わらない（`notationstyle.cpp:11-12`）。info が届くたびに通る経路にある
- `Position::applyAs` と `KifuNodeReplay::m_mover`: 前回 F1 の後は手番と常に同じ
- エクスポーターの `const KifuBranchTree* tree = &recordTree;` の null の確認（`csaexporter.cpp:93`、`kifexporter.cpp:145,147,166`、`jkfexporter.cpp:164`、`usiexporter.cpp:18`、`usenexporter.cpp:28-30,48,80`）
- `StartKind` の4つの区別: 製品のコードは `== Default` しか読まない（`kifuloadparser.cpp:123`）。読むのはテストとデバッグログだけ

**写し**

- `sennichitedetector.cpp:64-69`: `Position` を読んだ上で、手番を SFEN の `split` で判定している（10-29 T6・前回 M18 の残り）
- `csagameprogress.h:91,112`: `moveRecord` が `played` から作り直して2欄を写す

**指標**

- `tst_structural_kpi` の `long_functions_over_100` は誤検出を含む。正規表現が `return QString::fromUtf8(` のような行を関数の定義と数える（`kiflexer.cpp:67`・`automationwidgetadapters.cpp:76` など）。実質的に長い関数は `JosekiBookFile::load`（194行）のほかは、UI の組み立てと表がほとんどである。上限 60 は実測 21 より緩く、`kpi-thresholds.md` の実測 38 も古い

**使っていない include**

- `csagamecoordinator_setup.cpp:8,11,17`、`matchcoordinator.cpp:19`（`sfenutils.h`）
- `automationcommands_kifu.cpp` の `automationparams.h`・`sfenutils.h`、`automationcommands_position.cpp` の `automationparams.h`
- `csaformatter.cpp`・`usiexporter.cpp`・`usenexporter.cpp` の `kifuexportcommon.h`、`csaexporter.cpp` の `<QDateTime>`

**古い説明**

- CLAUDE.md
  - `automationparams.cpp/.h` は `.h` だけ
  - `AppSettings::boardVisuals` は `AppearanceSettings::boardVisuals`
  - 「棋譜を移ってよいか」の「待機中・検討中・詰み探索中」に通信対局の局間が無い（`humanturnpolicy.h:50`、`automationcommands_kifu.cpp:200` も同じ）
  - 「成りの選択」を `dialoglauncher` が開くとある（`dialoglauncher.h:22,27` も同じ。実際は合成ルートが `PromoteDialog::ask` を結ぶ）
  - `csagamesession` の「盤の手」は T3（前回）で `GameChannel` 経由になった（`csagamesession.h:27` も同じ）
  - 盤の入力の行き先が3つになったのに「主画面と詰将棋を解く画面」のまま（`boardinputroute.h:12-13`、`boardinteractioncontroller.h:24`、`promotedialog.h:28`、`boardviewerwidget.h:18` も同じ）
  - common に `RecentList<T>` が無い
  - 通信対局の終局の順（T4）
- `docs/dev/mcp-server.md:126`（「`get_kifu` は本譜を返す」。既定は表示中の手順）、`:327`（`exportLines(format)` は private）
- `docs/dev/composition-root.md` の例外の節に `setActiveChannel` が無い（T3 で消す）
- `docs/dev/game-play-to-resignation-flow.md:580`（Q1 に合わせる）
- ヘッダーのコメント
  - `gamesession.h:84`、`mainwindowactions.cpp:47`（`DuringCsaConnection` の取り残し）
  - `mainwindowcomponents.h:115`、`sessionresetcoordinator.h:21`（「対局の記録」は前回 M8 で消えた）
  - `networksettings.h:50`（消すのは起動時だけ）
  - `clicommands.h:13`（使い方の誤りは 1）
  - `setting.h:32`、`cliargs.h:5`、`clicommands_basic.cpp:2`
  - `shogitypes.h:68`・`shogiposition.h:21`（「局面編集で玉も駒台に置ける」は `positioneditrules.cpp:51` と逆。8種目は駒箱のため）
  - `josekimovedialog.h:56-60`・`josekimoveinputwidget.h:15`（表記の設定に従う文字を受ける）
  - `pvboarddialog.h:51`（範囲外は端に寄せる）
  - `shogiview.h:17-18`、`shogiview.cpp:512`、`boardchromewidget.cpp:99`、`considerationpanel.cpp:2`
  - `matchengineseat.h:5`（`@brief` の「手番」）
  - `boardinputroute.h:15`（尋ねるのは `moveFromClick`）、`docklayoutmanager.h:20`、`branchtreeeditcontroller.cpp:2`

`engineregistrationhandler.cpp` の `lcUi` 4か所も、joseki で直したのと同じ種類である。

**状態（5章）: 済（8e943f7d）。次の2つは残した。**

- `StartKind` の4つの区別: 読むのはテストだけだが、テストが形式ごとの開始局面の書き方を確かめる口として使う（37か所）。前回「見送り」とした、テストが部品の状態を確かめる口と同じ扱いにした。
- `Position::applyAs`: `KifuNodeReplay::m_mover` は消した。`applyAs` は手番を指定して局面を組み立てるテストだけが使う（同じ扱い）。

---

## 6. 責務が整理できていて、手を入れなくてよい部分

- **core**: `Position` と `mismatchOf`、`PieceRules`、`BoardCoordinates`、`Handicaps`、`UsiPosition::split`、`PositionValidator` → `SfenValidator`、`UsiScore`、`PvText`、`JishogiCalculator`
- **kifu**: 指し手の経路（全形式で `KifLineBuilder` → `MoveRecord::play`）、`WrittenMove`、`ParseReport`、`RecordEntry::forSideToMove`、`TerminalCodes`、`UsenCodec`、`KifuTextDecoder`、`KifuExport` の能力表、`KifuNodeReplay`、`KifuSaveFile`、`KifuBranchTree`・`KifuBranchNode`・`KifuNavigationState`、`KifuDocument` の未保存の判定、`PositionCollectionFile`、`JosekiBook`・`JosekiBookSession`・`JosekiLines`
- **engine**: `UsiEngineSession`、`UsiProtocolHandler`、`UsiMatchHandler`、`EngineSlots`、`MateOracle` と `MateQueryLoop`、`fromClock` に1本化した時間の換算
- **game**: `MatchEnding` と `MatchCause`、`MatchCoordinator` の本体と Strategy 3つ、`MatchEngineSeat`、`ConsecutiveGameSeries`、`GameStartOptionsBuilder::plan`、`NyugyokuJudgement`
- **analysis**: `AnalysisSession`、`EngineAnalysisRunner` と `PlyAnalysisCollector`、`KifuAnalysisSession` の本体、`TsumeCandidateJob`・`TsumeVerificationJob`・`TsumeshogiGenerator` の分担
- **network**: `CsaClient`、`CsaGameSummary` と読み取り器、`CsaLogin`、`CsaVerdictWait`・`CsaGameSeries`、`Phase` と接続の状態から `GameState` を求める形、`CsaGameProgress` の時間の計算
- **headless**: `InputPath`・`OutputPath`、`ParamSpec`、`ErrorKinds`・`Failure`、`RecordJson`、`KifuConversionService`
- **app・ui**: 合成ルートの生成・接続・初期表示の分け方（`setActiveChannel` を除く）、`MenuActionsWiring`、前回 T2 の棋譜を移る・変える入口、`DuringCsaConnection`、盤の入力（`BoardMoveController`・`BoardInteractionController`・`PositionEditSession`）、定跡の5つのコントローラ、`UiStatePolicyManager` の表の形、`DockRegistry`、presenters の全部、`common` の全部品
- **widgets・views**: 書き直した `EngineInfoWidget`（数値の書式と配置は1か所で、値の更新では配置を計算しない）、`ActionBinding`、`plainToolTip`・`GlobalToolTip`（部品の分担は正しく、問題は適用漏れだけ）、`ShogiView` の分割、`JosekiWindow`（ダイアログを出さず意図のシグナルだけ）、`BranchTreeView`、`ColumnWidthMemory`・`FontSizeControl`・`PanelButton`・`EditActions`・`LogPanel`・`EngineSelector`・`SettingsFormBinding`
- **models・style**: `KifuRecordListModel`・`KifuBranchListModel`（ノード ID だけを持つ）、`GameInfoEditModel::PresentedTextRole`、`EvaluationSeries`、`NotationTextCache`、`ScoreText`、`BoardAppearance`、`PieceStyleCatalog`
- **dialogs**: 定跡手のダイアログ（合法手は core、表記は `JosekiTableModel::moveText`、選んだ手の正本は一覧）、規則を下の層に任せている形（`TimeControlValidator`・`CsaLogin`・`JishogiCalculator`・`GeneratorLimits`・`KifuExport::lossIn`・`KifuSaveFile`・`PositionCollectionFile`）、`TsumePlayDialog` の `Mode` と `controlStates()`、`BoardAppearanceDialog`、`EngineSettingsOptionHandler`
- **services**: `Setting<T>` の宣言と `settingskeys.h` の削除の後始末、`GeneratorLimits`、`EngineProfileStore`・`RegisteredEngines`
- **automation・cli・mcp**: メソッドと表の一致（31件）、`AutomationInteractions::queue`、`AutomationWidgetAdapters`、コマンドの表と `CliCommandSpecs`、`BoardImageRenderer`、`specs.json` からの入力スキーマ（`describe` の出力と一致）、`environment` の読み取り、`paths.py`、`jobs` の停止の手順

---

## 7. 進め方

1. **T1（開始局面の読み取り）を先に直す。** 黙ってデータが変わるもので、影響がいちばん大きい。例はどれもそのまま回帰テストにできる。次の順に小さなコミットに分けられる。
   - F1（読む行の範囲）: 仕様の判断は要らない。
   - F2（持駒の見出しの表）: 関数を1つ置くだけ。
   - F4・F3: Q3・Q4 を決めてから。
2. **小さく直せて影響の大きいものを次に直す。** どれも1〜2ファイルで済む。
   - F5（投了。Q1 を決めてから）
   - F10（定跡の「着手」）
   - F11（1行）
   - F9（ツールチップ）
   - F8（MCP の failed）
   - F20（再帰）
   - F16・F17（詰将棋の入口。`specs.json` の作り直しが要る）
3. **T2（保存の規則）を進める。** F6（Q2 を決めてから）→ F7 → F19 → F18 → F14 の順。F19 は CLI・自動化 API・MCP の出力を1コミットで足す。
4. **T3 の窓口の部品（`setActiveChannel` の解消）と T4 の残りを進める。** どちらも部品の形が変わるので、テストの書き直しが要る。T4 の F12 は、CSA の開始局面を値の関数にしてから。
5. **F13・F15・F21〜F24 と T6、3章は、いつでも進められる。** M1 は `JosekiMoveBoard` の周りを次に触るときに一緒に直す。
6. **4章の改名は、関わる項目を直すときに一緒に行う。** 改名だけのコミットは翻訳の文脈が変わるので、`update_translations` と訳の埋め直しまでを1コミットにする。
7. **5章は、関わる項目を直すときに片付ける。** 単独で直すなら、CLAUDE.md・`mcp-server.md`・ヘッダーのコメントを1コミットにまとめる。

**監査を続ける効果について。** 構造の問題はこの5回でほぼ解消し、今回の指摘の多くは「修正が隣の入口に届いていない」形である。次の監査の前に、修正のときに「同じ操作の入口はほかにないか」を確かめる習慣を CLAUDE.md の Design Principles に足すと、同じ形の指摘を減らせる。次に監査するなら、範囲ごとに読むより、T1・T2 のように同じ操作の入口を表にして比べるほうが、この形の問題を見つけやすい。
