# コード監査：抜本的な書き直しが見合う箇所（2026-10-23）

「本来はもっと綺麗に実装できるのにコードが汚く、抜本的に書き直した方が良い箇所はあるか」という依頼で、`src/` 全体（約8.6万行）と `mcp/` を監査した。前回の監査（`code-audit-2026-10-16.md`、54ca37d7）の R1〜R10・3章・4章をすべて直した後の状態が対象である。その後の104コミットで src の約2.3万行が書き換わっている。

- 対象のコミットは a10775c3。本文の行番号はこの時点のもの。
- 6つの範囲（core・kifu／engine・game・analysis・network／app・ui・navigation・board・common／widgets・views・models・style／dialogs／services・automation・cli・mcp）に分けてコードを読み、呼び出し元と重複箇所を grep で確かめた。前回の文書と同じ指摘は除き、書き直しの後に残った名残（新旧の仕組みの併存・転送だけの層・新しく生まれた正本の二重化）と、新しく書かれたコード（定跡まわり）を中心に見た。
- 不具合は、`shogiboardq-cli` で再現できるものは再現した。
- 監査ではソースを変更していない。

> **修正の進み具合**: 6章の順に直し、各項目に「状態: 済（コミット）」を書く。途中で止まったら、状態のない項目から再開する。

## 全体の傾向

- 前回のような層をまたぐ大きな構造の問題（局面の値の型がない、対局の流れが2本ある、など）は、ほぼ解消している。
- 文字どおりの重複もほぼない。コメント・空行・括弧だけの行を除いて6行以上同じ並びが別ファイルにあるのは4組だけだった。
  - `position … moves …` の前処理（`shogiposition.cpp:133`・`usiposition.cpp:37`）
  - `relaxToolbarWidth`（`commenteditorpanel.cpp:34`・`csalogpanel.cpp:23`・`usilogpanel.cpp:57`）
  - 定跡の条件ダイアログ2つの決定ボタン
  - 評価値グラフの設定の setter の宣言
- 残っている問題は次の4つに分けられる。
  1. **状態に持ち主がない**: 動作状態（`AppActivityState`）は6つの部品が直接書き換え、取り上げられた側に知らせない。定跡の表示・登録の状態は画面部品（`JosekiWindow`）が持ち、ほかの部品がそれに問い合わせる。
  2. **文字列で受け渡す名残**: R1・R2 で値の型は入ったが、棋譜の表記（`displayText`）・読み筋・局面（SFEN）・消費時間を文字列で持ち、使う側が正規表現や `split` で読み戻している。
  3. **同じ流れを場所ごとに組み合わせている**: 文書の置き換えでの表示の消去、探索の止め方と次の開始、読み筋の漢字表記（4つのループ）、ダイアログの大きさの保存（2系統）など。
  4. **C++ と Python の表の二重管理**: MCP のツール定義が CLI・自動化 API の表の写しのまま残っている。
- 複数の範囲の監査で別々に見つかった問題がある。動作状態の書き手、定跡の状態の持ち方、読み筋の文字列と漢字表記がそれで、確度が高い。

---

## 1. 見つかった不具合

書き直しとは別に、個別に直せる。

### 1.1 再現・確認したもの

「CLI で再現」は `shogiboardq-cli` で出力を確かめたもの、「コードで確認」は経路を読んで確かめたもの（アプリでは実行していない）。

| # | 不具合 | 原因 | 確認 |
|---|---|---|---|
| B1 | KIF の消費時間の分が3桁（100分以上）だと読めず、警告なしに0秒にする。アプリが書いた KIF も読み戻せない。CSA の `T6000` を KIF にすると `(100:00/01:40:00)` と書き、それを CSA に戻すと `T0` になる | `kiflexer.cpp:48` の正規表現が分を `{1,2}` 桁に限っている。書き出し（`kifexporter.cpp:54`）は3桁を書く | CLI で再現<br>**状態: 済（b02cb1f0）** |
| B2 | KI2 の「まで3手で先手の反則負け」を読んで KI2 に書き出すと「後手の反則負け」になる。反則勝ちも同じ。KIF は「まで」の行を読み飛ばすので、出力が入力と食い違う | `Ki2Lexer::parseResultLine`（`ki2lexer.cpp:57`）が「先手の」「後手の」を捨てる。読む側は手番側が終局を記録したとみなす（`kiftextparser.cpp:108`） | CLI で再現<br>**状態: 済（b02cb1f0）**。結果行が名指しした側を読み、書き出しと同じ規則で記録する側を決める（`Ki2Lexer::ResultLine::endingSide`） |
| B3 | `convert-kifu` で対局情報のない棋譜（USI など）を変換すると、変換した時刻を「対局日」「開始日時」（CSA では `$START_TIME`）として書き、「先手：先手」「後手：後手」の行も作る。GUI の保存ではこうならない | 書き出しの文脈の `gameInfoProvided` が偽のとき、対局情報を自動で作る。この経路を通るのは CLI だけ（`kifuconversionservice.cpp:67-70`）。`csaexporter.cpp:115-116` のコメント（変換時刻は書かない）とも食い違う | CLI で再現<br>**状態: 済（b02cb1f0）** |
| B4 | 局面編集中に「検討開始」を押すと、編集を終えないまま検討が始まる。盤は編集の表示のまま残り、「編集終了」は効かず、編集メニューは隠れる。検討するのは編集前の局面になる。検討を止めて編集を始め直すと、編集した内容は消える | 検討開始ボタンは対局中以外のすべての状態で押せる（`uistatepolicymanager.cpp:101`、テスト `tst_app_ui_state_policy.cpp:566-576` もそれを期待）。`AnalysisSession::start`（`analysissession.cpp:50`）が動作状態を「検討中」に上書きし、`PositionEditSession::finish()`（`positioneditsession.cpp:72`）は `isEditing()` が偽なので何もしない | コードで確認<br>**状態: 済（1cc6f94b）**。つなぎとして検討開始ボタンを局面編集中は無効にした。根本は R1 |
| B5 | コメント欄で書きかけて「コメント更新」を押していない内容が、保存に含まれず、終了・新規・開くでも確認なしに消える。対局中に相手が指すと、`showCurrentComment` が書きかけの内容を上書きする | 確認が出るのは利用者の局面移動のときだけ（`kifunavigationcontroller.cpp:129-134`）。`KifuFileController::hasUnsavedChanges()`（`kifufilecontroller.cpp:40-43`）は文書の `isModified()` しか見ない | コードで確認<br>**状態: 済（1cc6f94b）**。未更新の本文を文書の未保存に数え、書き出す前に書いていた手に確定する。表示中の手が対局の進行などで変わったときも、書いていた手に確定してから切り替える |
| B6 | エンジン登録で option 行が不正なエンジンを登録すると、そのエンジンがハンドラの一覧にだけ残る。以後、画面の行とハンドラの一覧の番号がずれ、ツールチップ・「設定」・「削除」が別のエンジンを指す。次の登録が成功すると、失敗したエンジンも設定に保存される | `EngineRegistrationHandler::registerEngine`（`engineregistrationhandler.cpp:170`）が一覧に足してからオプションを解析し、失敗しても取り除かない。画面は行番号でハンドラの一覧を引く | コードで確認<br>**状態: 済（ec32d120）** |
| B7 | マージで登録した定跡手を表から消しても、マージのダイアログでは「✓登録済」のままで押せない。「全て登録」もその手を飛ばす | 「登録済」は定跡に手があるかではなく、保存されない印（`JosekiBook::m_mergeRegistered`）で決める。`JosekiBook::removeMove`（`josekibook.cpp:336-349`）は印を消さない | コードで確認<br>**状態: 済（ec32d120）**。つなぎとして `removeMove` で印も消す。印ごとの廃止は R2（52c25f32 で廃止し、「登録済」は定跡に手があるかで決める） |
| B8 | 定跡ドックを閉じるときの確認で「破棄」を選んでも、定跡は捨てられない。未保存のまま残って盤の矢印・対局に使われ、アプリの終了時にもう一度聞かれる | `JosekiFileController::confirmDiscardChanges`（`josekifilecontroller.cpp:152-177`）は「破棄」で true を返すだけで、セッションを元に戻さない。セッションをウィンドウの外へ出す前の名残 | コードで確認<br>**状態: 済（ec32d120）** |
| B9 | 定跡ウィンドウの「自動読込」をオフにして起動すると、前回のファイルの記録が消える。後でオンにしても、次の起動では読み込まない | 起動時の `setRecentFiles`（`josekiwindow.cpp:453`）が `recentFilesChanged` → `saveSettings` を呼び、まだファイルを開いていないので空のパスを保存する（`josekiwindow.cpp:469-470`、`josekibooksession.cpp:174-177`） | コードで確認<br>**状態: 済（ec32d120）** |
| B10 | 通信対局を始めても、前の対局・棋譜解析の評価値グラフの点、思考の表、解析結果が消えない。同じ接続の次局も同じ | `GameSession::onCsaGameStarted`（`gamesession.cpp:268`）は `resetToNewRecord` だけを呼ぶ。表示を消すのは `prepareGameStart` と `clearBeforeKifuLoad` だけ（`sessionresetcoordinator.cpp:38-41,56-60`） | コードで確認<br>**状態: 済（ea3264f4）** |
| B11 | 局面集ビューア・棋譜貼り付け（どちらもモードレス）を開いたまま対局・通信対局・棋譜解析・局面編集を始め、そこから反映すると文書が置き換わる。対局中ならライブ記録が捨てられ、以後の手は棋譜に残らず、司令塔は前の手順のまま続ける。自動化 API の `kifu.load`・`position.set` も同じ | 操作してよいかの判定は `UiStatePolicyManager` のメニュー・ボタンの有効/無効だけで、文書を置き換える入口（`kifufilecontroller.cpp:95,102`）は動作状態を見ない | コードで確認<br>**状態: 済（ea3264f4）**。待機中でなければ置き換えずに知らせる（`KifuFileController::canReplaceRecord`。自動化 API は `invalid_state`）。根本は R1 |
| B12 | 検討タブのエンジン一覧が、エンジン登録ダイアログで追加・削除しても更新されない。削除したエンジンを選んだまま開始すると「検討エンジンの選択が不正です」になる | 一覧は起動時に1回だけ読む（`considerationtabmanager.cpp:114`）。`DialogLauncher::displayEngineSettingsDialog`（`dialoglauncher.cpp:45-49`）は閉じた後に何も知らせない | コードで確認<br>**状態: 済（ea3264f4）** |
| B13 | 駒数の上限を超える SFEN（歩20枚など）を `validate-sfen` と `render-board` は拒否し、`convert-kifu` は警告なしで KIF に書く | 局面の検査が3系統ある（R4） | CLI で再現<br>**状態: 済（b853918c）**。convert-kifu は render-board と同じく盤と持駒として成り立たない局面を `parse_error` にする（表記の説明の図のような、手番でない側に王手がかかっている局面は受け付ける） |
| B14 | CLI のエラーの分類が文書と違う。`convert-kifu` で入力の指定がない・両方ある・ファイルがない → `parse_error`（`invalid_argument`・`not_found` であるべき）。`render-board` で読めない SFEN → `render_error`。`verify-tsume` で受方の玉がない SFEN → `engine_error`（エンジンは起動していない）で、status `invalid` に届かない | サービスが bool と文字列で失敗を返し、CLI が分類を1つに決め打ちしている（R9） | CLI で再現<br>**状態: 済（b853918c）**。分類つきの失敗 `Failure`（`errorkinds.h`）を足し、変換・盤面描画が返す分類を CLI がそのまま写す |
| B15 | ドックレイアウトの名前に「/」を含めると、保存はされるが一覧に出ず、選ぶことも消すこともできない | `saveDockLayout`（`docksettings.cpp:40-44`）が名前をそのままキーにするので、入れ子のグループになり `childKeys()` に出ない。保存ダイアログは名前を検査しない。前回の B8 と同じ種類 | コードで確認<br>**状態: 済（b853918c）** |
| B16 | 通信対局の終局で、エンジンへの知らせ方がローカル対局と違う。引き分けでも `gameover draw` を送らず `quit` だけを送る。先読み中でも `stop` を送らずに `gameover` を送る | `csagamecoordinator.cpp:340-351`。ローカル対局は cancel → stop → gameover → quit の順 | コードで確認<br>**状態: 済（b853918c）** |
| B17 | 読み筋盤は「閉じる」でも Esc でもウィンドウの大きさを保存しない。局面集ビューアは Esc で閉じると、ウィンドウと盤の大きさを保存しない | 大きさを closeEvent でだけ保存している。accept/reject では closeEvent が呼ばれない（Qt 6.12 の最小のプログラムで確認） | コードで確認<br>**状態: 済（b853918c）**。（確認: 読み筋盤・局面集ビューアのウィンドウは開くたびに盤に合わせていて、保存した大きさは使われていなかった。その設定を消し、局面集ビューアの盤の大きさを `done` で保存する） |

### 1.2 疑い（コードから追った推定で、実行では確かめていない）

| # | 疑い | 根拠 |
|---|---|---|
| U1 | 局面編集の後に0手のまま CSA・JKF・USEN で保存した棋譜を GUI で開くと「指し手を取得できませんでした」で失敗する（KIF の局面図と USI は開ける） | `kifuloadcoordinator.cpp:120` が手合割の名前の文字列（`teaiLabel`）で局面図の有無を決めている（R5）。CLI では読み込めることを確認<br>**状態: 済（a08a5886・c6a53e53）**。平手の開始局面を手合割の名前だけで書いた0手の棋譜は、開くものが無いので今までどおり読まない |
| U2 | 英語表記で棋譜解析を始め、後で日本語表記に切り替えると、結果の表の指し手の列が英語のまま残る（逆は作り直される） | 棋譜解析が表示用の行を `RecordEntry::displayText` に書き戻している（`kifuanalysiscontroller.cpp:90`、`kifupresentation.cpp:92`）（R3）<br>**状態: 済（759b737b）** |
| U3 | 棋譜解析の後に分岐を足す・本譜にする・並べ替えると、結果の行の選択やグラフの点のクリックで別の手順へ移る | 解析した手順を `allLines()` の番号で覚えている（`kifuanalysiscontroller.cpp:110,194,235,242,252`、`evaluationchartwidget.cpp:316`）。番号は深さ優先の並び <br>**状態: 済（f119793c）**。解析した手順を末尾の手のノード ID（`AnalysisRequest::lineEndNodeId`）で覚え、`KifuNavigationController::goToPlyOnLine()`（`KifuBranchTree::nodeOnLine()`）で移る。グラフの見出しの手順の名前は分岐ツリーの形が変わるたびに付け直す（テスト `resultsKeepAnalyzedLineAfterBranchEditing`・`lineEndSurvivesBranchEditing`） |
| U4 | 人間対エンジンで人間が先に指すと、エンジンの初期化を待つ間もエンジンの持ち時間が減る（最大で初期化の期限の5秒） | 初手がエンジンなら初期化中は時計を止める（`humanvsenginestrategy.cpp:63`）が、人間が指した後の依頼（`:36`）では止めない<br>**状態: 済（58eadff5）**。人間の手の後も初期化中なら時計を止めて待つ（`HumanVsEngineStrategy::requestEngineMoveWhenReady`。テスト `tst_gamestrategy` の `humanMoveWaitsForInitializingEngine`） |
| U5 | 局面を読めないと position を送らずに go を送り、前の局面の結果が返る（棋譜解析なら別の手数の結果として記録される）。今の呼び出し元は読める局面だけを渡すはず | 空の `UsiPosition` が「position を送らない」の意味になっていて、`value_or(UsiPosition{})` が6か所ある（R8）<br>**状態: 済（81887c15）**。空の局面では何も送らない。6か所は `std::optional` で受け、棋譜解析はその手数を飛ばす（`tst_analysisflow` の `jobSkipsUnreadablePositions`）、詰みの問い合わせは判定できなかったとし、検討・対局・通信対局は知らせて送らない |
| U6 | 中断して再開を待つ間、ライブ記録が有効のまま残る。盤で変化を指せない、分岐を編集できない、グラフの縦線が追わない。前回 B5 と同じ症状（意図の確認が要る） | `gamesession.cpp:320` <br>**状態: 済（87b95924）**。再開は中断の行を除いてライブ記録を始め直す（`resumeInterruptedGame`）ので、中断の時点で記録を確定しておいてよいと判断した。中断でもライブ記録を確定し、自動保存だけしない。待つ間は変化の手・分岐の編集・グラフの追従ができ、棋譜を変えたら（以前からの規則どおり）再開できなくなる（テスト `interruptedGameResumesFromCoordinatorState`） |
| U7 | 終了処理の後に盤のクリックが届くと、解放済みの `HumanTurnPolicy` を読む（経路は狭い） | `mainwindowcomposer.cpp:103` で破棄するが、`BoardMoveController` は生ポインタを持ったまま（`boardmovecontroller.cpp:31`） <br>**状態: 済（f119793c）**。終了処理では `HumanTurnPolicy` を破棄せず `disable()` で何も受け付けなくした |
| U8 | 詰将棋問題一覧のプレビューの駒台が、盤の木目の設定で描かれる | `tsumepositionpreview.cpp:40-41` が駒台の木目の設定（standWoodGrain）を見ていない <br>**状態: 済（f119793c）**。プレビューを `BoardStaticLayer` で描くようにして、駒台は駒台の木目の設定で描く |
| U9 | `kifu.get` の `total_plies` が、`format=moves` では終局の行を数えず、ほかの形式では数える | `automationcommands_kifu.cpp:139` と `:153` <br>**状態: 済（f119793c）**。`format=moves` も手順の行数（終局の行も1手）で数え、ほかの形式・`get_app_state` と揃えた |

---

## 2. 書き直す価値がある箇所（重要度順）

### R1. 動作状態に持ち主がない（app・ui・board・analysis・game・network）。B4・B11 の原因

**問題**

- `AppActivityState::setActivity`（`appactivitystate.h:22`）を6つの部品が直接書く。
  - `positioneditsession.cpp:63,81`
  - `kifuanalysiscontroller.cpp:153,182`
  - `csagamesession.cpp:73,83,103,114`
  - `gamesession.cpp:307`
  - `analysissession.cpp:50,100`
  - `matchcoordinator.cpp:78,384,536`
- どれも始めるときに上書きし、終えるときに Idle に戻す。終えるときに自分の状態かを確かめるのは `KifuAnalysisController` だけで、`AnalysisSession::stop` は無条件に Idle に戻す。上書きしても前の持ち主には知らせない。エンジンの枠には `EngineLease`＋`engineRevoked` があるが、動作状態には同じ仕組みがない。
- 操作してよいかの判定は `UiStatePolicyManager` の表（メニュー・ボタンの有効/無効）だけにある。次の入口は表を引かない。
  - `AnalysisSession::start`
  - `PositionEditSession::begin`
  - モードレスのダイアログ（局面集ビューア・棋譜貼り付け）からの文書の置き換え
  - 自動化 API の `kifu.load`・`position.set`
- `PositionEditSession` は盤の描き方を begin/finish の中で切り替えるだけで、動作状態の変化には従わない。前回 R7 の「ビューの描き方は動作状態から決める」が直りきっていない。
- `EngineDisplayPresenter::clearAll` が検討タブの RunState を直接 Idle にする（`enginedisplaypresenter.cpp:88`）。前回 R6 の「実行の状態は `AnalysisSession` の知らせだけから決める」の例外になっている。
- `MatchCoordinator::start` は動作状態を DuringGame にしてから枠を破棄するので、取り上げの知らせを受けた側が状態を書き戻せる。

**書き直し案**

- 動作状態は `begin(Activity, owner)` で受け取る move-only の `ActivityLease` を通してだけ変える。終えられるのは持ち主だけにし、取り上げたら前の持ち主に知らせる（`EngineLease` と同じ形）。遷移を表で拒否してもよい。
- 文書の置き換え・`AnalysisSession::start`・`PositionEditSession::begin` の入口でも、`UiStatePolicyManager` と同じ可否の表を引く。モードレスのダイアログの決定ボタンも同じ表で無効にする。
- `PositionEditSession` は動作状態の変化を受けて盤の描き方を合わせる。
- 当面の止血として、検討開始ボタンの許可から DuringPositionEdit を外す（テストの期待も直す）。

**影響**: 約12ファイル、300行前後。

**状態: 済（2f255623）**。案から変えた点: 取り上げの知らせ `revoked` は、始めた部品の準備が済んでから受け手が処理するよう次のイベントループで届ける。局面編集は取り上げられたら編集した局面で編集を終える（編集を捨てない）。欄が1つだけの `AppState` を廃止して合成ルートが `AppActivityState` を持つ（3章の「合成ルートの関数注入と AppState」のうち AppState の部分）。`UiStatePolicyManager` の別名 `AppState`・テストだけが使う既定のコンストラクタと `stateChanged`・何もしない `enableNavigationIfAllowed` も消した。入口の可否の検査は、棋譜の置き換え（B11）と局面編集の開始（待機中だけ）に入れ、検討の開始はリースの取り上げで扱う

### R2. 定跡まわりの状態と登録の経路（dialogs・ui・kifu・app）。B7・B8・B9 の原因

前回の後に増えた定跡の機能で、状態の正本が画面部品にあり、登録の経路が2系統ある。

**問題**

- **画面部品が正本を持つ**: `JosekiWindow`（951行。1000行の上限に近い）が、表示の停止・盤の手の登録・前回のファイルと履歴の保存・自動読込の待ちを持ち、ほかの部品がウィジェットに問い合わせる。
  - 盤の矢印を出すか: `mainwindowcomposer.cpp:77-84` が `isDisplayEnabled` を読む。
  - 読み筋を登録できるか: `mainwindowcomposer_create.cpp:339` が `canRegisterMoves` を bind する。
  - 盤の手の登録: `mainwindowcomposer_wire.cpp:188-189` がウィンドウの `recordBoardMove` に直接つなぐ。読み筋の登録は `JosekiRegistrationController` なので、登録の入口が2層に分かれている。
  - 自動読込は表示更新 `updateJosekiDisplay` の副作用になっている（`josekiwindow.cpp:780-785`）。
- **同じ値が3か所にある**: 「盤に定跡手を表示」は `CandidateArrowPresenter::m_josekiArrowsVisible`・`actionShowJosekiArrows`・ウィンドウのチェックボックスの3つ。合成ルートが初期値を2回入れ（`_create.cpp:313,334`）、配線3本（`_wire.cpp:184-185`、`menuactionswiring.cpp:92`）で手で揃えている。
- **「人間が指せるか」の伝え方が3通り**: フック（`josekiwindowcontroller.h:44`）、引き出し型の provider（`.cpp:41`）、押し込みの `setHumanCanPlay`（`:144`）。写しはウィンドウとモデルにある。
- **棋譜から定跡への登録が2系統**
  - 「現在の棋譜／棋譜ファイルから」（`JosekiMergeDialog` → `startRegisterMergeMoves`）は1手ごとにファイル全体を保存し、保存先が必須。
  - 「複数の棋譜から」（`JosekiImportDialog` → `importMoves`）は保存しない。
  - 棋譜の読み取りも2つある。`JosekiMergeEntries::fromKifuFile`（`josekimergeentries.cpp:31`）は `JosekiImport::Collector::addLine`（`josekiimport.cpp:109`）の部分集合で、UI スレッドで読む。
  - 表示中の棋譜は、`josekiwindowcontroller.cpp:166` で `RecordLine` を SFEN・USI・表記の並行リスト3本に分け、ウィンドウ → `JosekiMergeController` → `JosekiMergeEntries::fromLine` で組み直す。R2 で消した「並行リストを件数で突き合わせる」形がまた現れている。
  - `KifuMergeEntry` は kifu の値型なのに、表示文字列 `japaneseMove` と画面の状態 `isCurrentMove` を持つ。
- **「登録済」が別の正本**: 定跡に手があるかではなく、保存されない印 `m_mergeRegistered` で決める。各ダイアログが印の写しを持ち、`findChildren` で4回配り直す。`removeMove` は印を消さない（B7）。
- **読み書き中の変更を `JosekiBookSession` が拒否しない**: `isBusy` を見るのは `importMoves` だけ（`josekibooksession.cpp:102`）。`addMove`・`updateMove`・`removeMove`・`moveMoveTo`・`sortMoves`・`registerEnginePv` は呼び出し側の4クラスがそれぞれ確かめている。確認を忘れると、保存が終わったときに編集前の写しで上書きされて編集が消える（`josekibooksession.cpp:316`）。今は全経路が確かめている。
- **表の作り直しが重なる**: 編集では `contentChanged` で作り直した後、ウィンドウがもう一度作り直す（`josekiwindow.cpp:602-618,669-670`）。読み込みでは `filePathChanged`・`loaded`・`contentChanged` で3回作り直す。

**書き直し案**

- 前回のファイル・履歴の保存と遅延読込はセッション側へ移し、`canRegister()` もセッションが答える。
- 表示の停止・盤の手の登録・盤の矢印は、`JosekiWindowController` が持つ checkable な `QAction` を正本にする。盤の手の登録は `JosekiRegistrationController` へ移す。人間が指せるかは引き出し1本にする。
- 現在の手順・棋譜ファイルも `Collector` に通し（`RecordLine` を受ける入口を足す）、`importMoves` で登録する。「登録済」は `hasMove` で出す。次を削除する。
  - `startRegisterMergeMoves`・`m_pendingMerge`・`mergeFinished`
  - 印 `m_mergeRegistered` と `JosekiMoveKey`
  - `KifuMergeEntry`・`JosekiMergeEntries` と、`findChildren` での配布
- `JosekiBookSession` の変更系は、読み書き中なら false を返す。
- 表の再表示は `contentChanged` だけにする。
- 仕様の判断が要る: マージの「1手ずつ登録して即時保存」を残すか。残すなら `importMoves` と `startSave` を組み合わせる。

**影響**: dialogs 4〜6・kifu 5・ui 2・app 3。差し引き約500行減（`josekiwindow.cpp` は250〜300行減）。`tst_josekiwindow` は friend でのアクセスが19か所ある。

**状態: 済（52c25f32・ca627f15）**。仕様の判断: マージの「1手ずつ登録して即時保存」はやめ、ほかの編集と同じく未保存にして「保存」で書き込む（ガイドも直した）。案から変えた点: 「登録済」は定跡に手があるか（`hasMove`）で決めるので、すでにある手は「登録済」で押せない（出現頻度を数えるのは「複数の棋譜から...」）。マージのダイアログは開いている定跡が替わっても閉じず、登録先の表示と登録済みの印を合わせる。定跡を変える関数は読み書き中だけでなく、前回のファイルの読み込みを待っている間も何もしない（`canEdit()`）。人間が指せるかは定跡ウィンドウの引き出し1本（`setHumanCanPlayProvider`）にし、押し込みの `setHumanCanPlay` を消した。`josekiwindow.cpp` は951行から926行（`JosekiMergeEntries` などを合わせ src 全体で約310行減）

### R3. 指し手と読み筋の表記を文字列で受け渡している（kifu・engine・analysis・models・widgets・dialogs）。U2 の原因

3つの範囲の監査で別々に見つかった。

**問題（棋譜の表記）**

- `RecordEntry::displayText`（`recordentry.h:20`）に3種類の値が入る。
  - KIF の指し手の表記（`KifMoveText` で作る）
  - 開始局面の訳語 `tr("開始局面")`
  - 棋譜解析が書き戻す表示用の行（手数・分岐の印つき、その時点の表示言語。`kifuanalysiscontroller.cpp:88-91`）
- そのため `KifuPresentation::label`・`status`（`kifupresentation.cpp:90-135`）が、手数（`^\s*[0-9]+\s+`）・末尾の `+`・終局語・「（定跡）」を正規表現で読み戻す。ノードは `TerminalType` を持っているのに、終局の種類を語から引き直している。
- `BranchTreeView` も手数と `+` を別の正規表現で削り（`branchtreeview.cpp:51-65`）、手番は `RecordEntry::mover` があるのに SFEN の2語目で決める（`:97-98`）。
- 呼び出し側の5か所が、それぞれ (displayText, 親の SFEN, USI の指し手) の文字列3つを組み立てて渡し、`KifuPresentation` は表示のたびに SFEN と USI を解析し直す。
  - `kifurecordlistmodel.cpp:46-48`
  - `kifubranchlistmodel.cpp:34-36`
  - `branchtreeview.cpp:55-63`
  - `branchtreeeditcontroller.cpp:125-127`
  - `josekimergeentries.cpp:23,49`
- ほかにも displayText に頼る所がある。指し手を持たないノードの書き出しは displayText から `(xy)` を正規表現で消す（`ki2exporter.cpp:43-44`）。`gamesession.cpp:102` は displayText が空かで手の有効性を判定する。

**問題（読み筋）**

- 読み筋を空白区切りの文字列で持ち、使う側が分け直す。例えば、`AnalysisJob` は `QStringList` で作り（`analysisjob.cpp:71`）、`AnalysisResultHandler` が join して行に入れ（`:48,75`）、同じハンドラが `section` で初手を取り（`:86`）、また split する（`:141`）。`CandidateArrowPresenter:183`・`JosekiRegistrationController:21-23,55,89`・`PvClickController:28-29` も分けている。
- 思考の表の1行は、同じ読み筋を文字列5つ（`pvKanji`・`usiPv`・`enginePv`・`baseSfen`・`lastUsiMove`）で持つ。
- 読み筋を局面の上で指しながら漢字の表記にするループが4つあり、指せない手の扱いがそれぞれ違う。
  - `PvNotationFormatter`（`pvnotationformatter.cpp:88-148`）: そこで止め、USI 形式の読み筋を空にする。
  - `PvLine`（`pvline.cpp:25-46`）: 緩く指して打ち切る。
  - `TsumeshogiKanjiBuilder`（`tsumeshogikanjibuilder.cpp:17-38`）: 読めない手を飛ばして続ける。GUI に依存しないのに dialogs にある。
  - `KifuPresentation`（`kifupresentation.cpp:74-88`）: 緩く指す。
- `PvNotationFormatter` は setter と文字列の getter からなる状態つきの API なので、`AnalysisJob` は偽の `UsiInfo` を作って渡し、返った文字列を split している。
- 表記の設定ごとの表示キャッシュが2つある（`ShogiInfoRecord` と `KifuAnalysisListModel::Entry`）。無効化の方法（revision）は R9-5 で揃ったが、実装は2つのまま。

**書き直し案**

- 段階1（棋譜の表記）
  - `KifuPresentation` の入口を型付きにする（例: `label(const RecordEntry&, const Position& before, BoardSquare previousTo)`）。終局と定跡は `TerminalType`・bool で渡す。
  - `displayText` は「読み込んだ修飾語つきの表記」のキャッシュに限るか、廃止する。
  - `KifuAnalysisRow` は手数・分岐の有無・`UsiMove` を値で持ち、表示文字列を `RecordEntry` に書き戻さない。
- 段階2（読み筋）
  - 値 `PvText{開始局面 Position, 直前の移動先, 指せた手 QList<UsiMove>, 残りの語}` を engine で1回だけ作る。
  - 表記は `KifuPresentation` の1つの関数が、その時の設定で描く。
  - 行の型は `QList<UsiMove>` と `firstMove()` を持つ。漢字表記の4つのループと表示キャッシュ2つは1つにする。

**影響**: 段階1は約12ファイル・300〜400行。段階2は約12ファイルで、差し引き約150行減。

**状態: 済（段階1 759b737b、段階2 235f4a20）**。U2 も直った。案から変えた点: 入口は `entryLabel(RecordEntry, 指す前の局面 SFEN)` にした（局面は英語表記のときだけ読む。`RecordEntry` が局面を持つのは R4 の段階3）。棋譜解析の結果の行は棋譜の表記・分岐の有無（`AnalysisRequest::branchPlies`）・定跡かを値で持つ。読み筋の値は `PvText`（core。`PvText::play` が1回だけ指し、指せた手と指せなかった語を持つ）で、思考の表の行の文字列5つは `PvText` 1つと至った手になった。表示の覚え方は `NotationTextCache` 1つ。詰将棋の正解手順も読み筋と同じ表記（「同」を使う）にし、`TsumeshogiKanjiBuilder` を消した。KI2 の書き出しで指せない手は、表記の文字列から移動元を削らずに指し手の値から作る（`KifMoveText::Notes::withoutSource`）

### R4. 局面を SFEN 文字列で受け渡す名残と、局面の検査・指し手の適用の3系統（core・kifu・analysis・engine・cli）。B13 の原因

**問題**

- 棋譜のデータが局面を `QString` で持つ。`RecordEntry::sfen`・`RecordLine::startSfen`・`KifBranchPoint::sfen`・`KifuMergeEntry` と `JosekiImport::Move` の sfen・`JosekiBook` の全 API がそう。`Position::fromSfen*`・`assignSfen` の呼び出しは src で約40か所（kifu で約20か所）、SFEN を `section(' ',1,1)` で切って手番を読む所が十数か所ある。
- 文字列と局面を往復している例:
  - `KifLineBuilder` が局面を `toSfen` で文字列にし（`kiflinebuilder.cpp:37,52`）、変化を読み始めるときに `fromSfen` で局面に戻す（`kiftextparser.cpp:240`、`usentosfenconverter.cpp:112`、`jkftosfenconverter.cpp:207`）。
  - `JosekiLines` は1局面ごとに Position → toSfen → keyOf → normalizeSfenKey → fromSfenLike → positionKey をたどる（`josekilines.cpp:29,43,73`、`josekibook.cpp:89-95`）。
  - `findMatchingChild` は子ごとに、比べる2つの SFEN を両方とも解析し直す（`kifubranchtree.cpp:437-448`）。
- 前回 R1 の案「Bod・CsaPosition・SfenReplay・差分計算は Position を受け取る」は実施されていない。`bodtextgenerator.cpp:11`・`sfencsapositionconverter.h:12`・`sfendiffutils.h:37`・`sfenreplay.h:20`・`jkfformatter.h:204`・`kifunodereplay.h:188` は今も `QString` を受け取る。型付きの `Position::after` は呼び出しが0で、文字列版の `SfenReplay::positionAfter` が5か所で使われている。
- `position … moves …` の文字列の読み取りが4つある: `Position::fromSfenLike`（`shogiposition.cpp:131`）、`UsiPosition::parse`（`usiposition.cpp:35`）、`SfenUtils::normalizePositionLikeSfen`（`sfenutils.h:53`）、`UsiToSfenConverter`（`usitosfenconverter.cpp:57-120,233-300`。`UsiMove::parse` の写しの `isValidUsiMove` を持つ）。
- 局面の検査が3系統、指し手の適用も3系統ある。
  - 検査
    - `Position::fromSfen` は構文だけを見る。
    - `SfenValidationService`（CLI・自動化・局面集）は Hayanagi で、駒数の上限・玉の重複・手番でない側への王手まで見る。
    - 対局開始は SFEN の文字 'K' と 'k' を数える（`gamestartoptionsbuilder.cpp:43-58`）。
    - 詰将棋は Hayanagi で検査し直す（`tsumecollection.cpp:67-111`）。
  - 指し手の適用
    - `UsiPosition::fromMoves` は緩い `Position::apply`（駒の色も持ち駒も見ない）。
    - `applyUsiMoves` は Hayanagi の合法手。
    - 対局は fmv。
    - CLI の `resolvePosition` は同じ手を Hayanagi と core で2回指す（`cliargs.cpp:28-63`）。
- 局面のキー（`TsumeCollection::positionId` と `Position::positionKey`）と、USI の指し手の正規表現（`usiprotocolhandler.cpp:329`・`positioncollectionfile.cpp:22`）も2つずつある。

**書き直し案**

- 段階1（kifu の中で閉じる）
  - `KifBranchPoint`・`KifuNodeReplay`・`JosekiLines` と、BOD・CSA・JKF の局面の変換を `Position` で受け取る形にする。
  - `JosekiBook` に `Position` で引く版を足す。
  - position 文字列の読み取りは `UsiPosition::parse` 1つにする。
- 段階2（検査と適用）
  - core に `Position::validate()`（駒数・玉・手番でない側への王手）と、合法手だけを指す適用（fmv）を置く。
  - `SfenValidationService` はその結果を JSON にするだけにし、Hayanagi は詰み探索だけに使う。
- 段階3: `RecordEntry` が `Position` を持ち、`sfen()` はそこから作る。

**影響**: 段階1は約10ファイル・200行。段階2は7〜9ファイル・追加と削除がそれぞれ約150行。段階3は約40ファイル（`node->sfen()` だけで28か所）なので、段階1・2の後で効果を見て決める。

**状態: 段階1・2は済（段階1 fbfe89d6、段階2 5b1e1b32）、段階3は見送り**。分岐元 `KifBranchPoint` は `std::optional<Position>` を持ち、`JosekiBook::movesAt(Position)` を足し、`findMatchingChild` は指し手で比べる。局面図と CSA の局面の行の書き出しは `Position` を受け取る。position 文字列の読み取りは `UsiPosition::parse` 1つ（`Position::fromSfenLike` はそれに任せる）。`KifuNodeReplay`・`JkfFormatter::sfenToJkfData`・`SfenDiffUtils` は書き出しや表示の入口で文字列を受け取るので、段階1では変えていない

段階2: core に `PositionValidator`（`check()`・`boardProblem()`）と `EngineMoveValidator::applyIfLegal` を置き、`SfenValidationService` は結果を JSON にするだけにした。対局の開始（`validatePosition`。旧 `validateKings`）・詰将棋問題一覧（`positionId`・`parse`・`validMateLine`）・CLI の `resolvePosition`（1回だけ指す）も同じ検査を使い、Hayanagi は詰み探索（`TsumeSearch` を使う詰将棋の部品）だけに使う。USI の指し手の正規表現2つは `UsiMove::isExact` にした。局面のキーは `Position::positionKey` 1つ（`positionId` もそれを返す）。

段階3（`RecordEntry` が `Position` を持つ）は見送る。段階1・2の後に残る SFEN 文字列は、分岐ツリーのノード・棋譜の書き出し・表示の入口が受け取るものだけで、局面を何度も読み直す処理（定跡の引き・分岐の照合・読み筋の表記）は値で受け渡すようになった。残る読み直しは英語表記の棋譜欄で表示中の行の数だけで、測れるほどの負荷ではない。一方で `node->sfen()` を使う28か所を含む約40ファイルを書き換える必要があり、利点に見合わない。読み直しが目立つ場面が出たら、ノードに `Position` を足す形で再検討する

### R5. 棋譜の読み込みの字句解析と読み込み結果（kifu/formats）。B1・B2・B3・U1 の原因

**問題**

- **消費時間を文字列で中継している**: 正規表現で取った数値を `normalizeTimeMatch` で "mm:ss/HH:MM:SS" の文字列に戻し（`kiflexer.cpp:66,97`）、`RecordTime::fromText` でもう一度読む（`kiftextparser.cpp:138,140`）。`stripTimeAndNextMove` は出力引数が4つあり、2つは使わない（`kiflexer.cpp:112-113` の Q_UNUSED）。分を2桁までに限る正規表現（`kiflexer.cpp:48`）が B1 の原因。
- **結果行の情報を捨てている**: `Ki2Lexer::parseResultLine` は終局の種類と手数しか返さず、「先手の」「後手の」を捨てる（B2）。
- **開始局面の正本が3つある**: `KifuParseOutcome` の `initialSfen`（コメントに「mainline.startSfen と同じ」とある）、`record.mainline.startSfen`、`positionOnly`。
  - `KifuLoadResult` が全項目を写し（`kifuloadparser.cpp:84-91`）、空の欄を互いに補う（`:101-102`）。
  - `kifuconversionservice.cpp:44-46` は3つを手で揃える。
  - `KifuDocument::replaceWithRecord` は record とは別に startSfen を受け取る。
- **`teaiLabel` 1つが5つの意味を兼ねる**: 手合割の名前、局面図の有無（"BOD"・"配置"）、既定値の印（"平手(既定)"）、"局面指定"、"任意局面"。`kifuloadcoordinator.cpp:120` はこの文字列を比べて局面図の有無を決める（U1）。
- **書き出しの文脈の旗**: `gameInfoProvided` で「渡された対局情報を使う」か「自動で作る」かを切り替える。自動で作る経路を通るのは CLI だけ（B3）。`HeaderStyle::Compact` は呼び出し0。項目名が "対局日" などのリテラルで、`GameInfoKeys` と二重になっている（`kifudocument.cpp:46`、`kifexporter.cpp:200-202`）。
- **持ち時間の書式**: `KifuExportMetadataBuilder::timeControlText` が作る独自の書式を、`CsaFormatter::convertToCsaTime`（`csaformatter.cpp:79-157`）と `KifuPresentation::infoValue`（`kifupresentation.cpp:161-175`）がそれぞれ正規表現で読む。

**書き直し案**

- 字句解析は型の値を返す: `NumberedMove{手数, 表記, optional<RecordTime>, コメント}`、`ResultLine{TerminalType, 手数, optional<Turn>}`。
- 開始局面は `mainline.startSfen` の1か所だけにする。手合割は `StartKind{Handicap, Diagram, Specified, Default}` と名前で持ち、「局面だけ」は手の有無から求める。
- 書き出しで対局情報を自動で作るのをやめ、項目名は `GameInfoKeys` を使う。
- 持ち時間は値の型で持ち、書式の読み書きを1組にする。

**影響**: 約16ファイル、約250行。

**状態: 済（3813d6ef・a08a5886・f30dd3b1・cb5b8b7e）**。案から変えた点: 字句解析は `KifLexer::readNumberedMoves` が `NumberedMove{表記, optional<RecordTime>, コメント}` を返す（手数は返さない。行内のコメントは行末までとし、コメントの中の数字で手順を打ち切らない）。結果行の `ResultLine` は B2 で入れた。開始局面の書き方は `StartKind` にし、手合割の名前は持たない（使う所がない）。`positionOnly` は bool にした。持ち時間は値の型 `TimeControl` を `TimeControlText`（`format`・`parse`・`parseSide`）で読み書きする

### R6. 入力中の編集の確定と未保存の判定が2通り（widgets・models・kifu・ui）。B5 の原因

**問題**

- **対局情報**: R3 の後、表が変わるたびに `gameInfoEdited` → `KifuDocument::editHeader` で文書に入る（`mainwindowcomposer_wire.cpp:103`）。それでも R3 前の「反映」の仕組みが丸ごと残っている。
  - 残っているもの: `GameInfoEditModel::m_applied`・`markApplied`、`GameInfoPane::applyChanges`・`gameInfoUpdated`、「対局情報更新」ボタン、「未反映の変更があります」の表示。
  - ボタンの説明「編集した対局情報を棋譜に反映する」（`gameinfopane.cpp:370`）とガイド（`docs/guide/game-play.html:238`）は実際と違う。ボタンの実際の効果は盤の対局者名の更新だけ（`PlayerInfoPresenter::onGameInfoUpdated` は items を使わない。`playerinfopresenter.cpp:47-51`）。
  - 未保存の基準が「文書の保存時点」と `m_applied` の2つあり、保存のたびに `saved` から `applyChanges` を呼んで揃えている（`_wire.cpp:102`）。
- **コメント**: 対局情報とは逆に、「コメント更新」を押すまで文書に入れない。未確定の確認は利用者の局面移動のときだけ（B5）。

**書き直し案**

- 対局情報の `m_applied` と更新ボタンをやめ、未保存の判定は文書に一本化する。盤の名前は header の変更で合わせる。
- `KifuDocument::Hooks` を「入力中の編集を確定する・未確定があるか」の汎用の入口にし、コメント欄も登録する。保存の前と、終了・新規・開くの確認（`KifuFileController::confirmDiscardUnsaved`）で両方を数える。

**影響**: widgets 2・models 1・kifu 1・ui 3・app 1、±150行。`tst_game_info_pane`・GUI テスト・ガイド1か所。

**状態: 済（3b008cdb）**。「対局情報更新」ボタン・`m_applied`・`gameInfoUpdated`・未反映の表示を消し、盤の対局者名は `KifuDocument::headerEdited` で合わせる。`KifuDocument::Hooks` は `commitPendingEdits`・`hasPendingEdits` の2つにし、合成ルートが対局情報欄の入力中のセルとコメント欄の未更新の本文をまとめて渡す。案から変えた点: 対局情報 `header()` は入力中のセルを確定しなくなった（読むだけで文書を変えない）。確定は保存・書き出し・対局の開始・対局情報を残す新しい棋譜（`startNew(keepHeader)`）の前。入力中のセルの確定には Ctrl+Enter も使え、欄の下に案内を出す

### R7. 文書の置き換えと、文書の外の表示の消去（app・ui）。B10 の原因

**問題**

- `SessionResetCoordinator` は部品を3つ出している: `prepareGameStart`（表示の消去＋文書）、`resetToNewRecord`（文書だけ）、`clearBeforeKifuLoad`（表示だけ）。これを流れごとに別々に組み合わせている。

| 流れ | 組み合わせ |
|---|---|
| 新規 | `clearBeforeKifuLoad`＋`resetToNewRecord`（`gamesession.cpp:304-306`） |
| 局面編集 | `resetToNewRecord`＋配線で `EvaluationGraphPresenter::clear`（`_wire.cpp:137`）。エンジンの表示は消さない |
| 読み込み | フック `clearUiBeforeKifuLoad` を5か所で呼ぶ（`kifufilecontroller.cpp:105,111,166,175,183`） |
| 連続対局の次局 | `resetToNewRecord` の直後に `prepareGameStart` がもう一度 `startNew`（`gamesession.cpp:224` → `:174`。ツリーの reset が2回） |
| 通信対局の開始 | `resetToNewRecord` と `startFromRoot` だけ（`gamesession.cpp:268-269`）。表示を消さない（B10） |

- 前回 R3・R4 の「文書の置き換えで消し忘れをなくす」が、文書の外の表示（評価値グラフ・思考の表・解析結果）には及んでいない。

**書き直し案**: 文書の置き換えを1つの知らせにし、評価値グラフとエンジンの表示がそれを受けて自分で消す（「現在局面から続ける」ときだけ切り詰める）。通信対局の開始も `prepareGameStart` を通す。`SessionResetCoordinator` は `prepareGameStart` だけになり、`KifuFileController` のフックと配線1本も消える。

**影響**: 6〜7ファイル、約150行。

**状態: 済（3b008cdb）**。`KifuDocument::replaced` を受けて評価値グラフ（`EvaluationGraphPresenter::clear`）とエンジンの表示（`EngineDisplayPresenter::clearAll`）が自分で消す。`clearBeforeKifuLoad`・`KifuFileController::Deps::clearUiBeforeKifuLoad`・局面編集の配線を消し、連続対局の次局と通信対局の開始は `prepareGameStart` だけを通す（ツリーの作り直しは1回）。案から変えた点: `resetToNewRecord` は新規・局面編集の入口として残した（対局の準備を伴わない）。局面編集でもエンジンの表示が消えるようになった。読み込みに失敗したときは文書を置き換えないので、前の表示が残る

### R8. エンジンの枠が対局の仲介を兼ね、探索の切り替えを使う側が書いている（engine・game・network・analysis）。B16・U5 の原因

**問題（枠と仲介）**

- `EngineLifecycleManager` の Seat が手番を持ち、手番のあるエンジンにだけ対局時計を渡し、投了・宣言勝ち・エラーを中継する（`enginelifecyclemanager.h:76-135`、`.cpp:81-86,96-152`）。指し手は Strategy が `Usi::matchMoveReady` から直接受ける。通信対局は `CsaEngineController` が、検討・棋譜解析は各自が同じ知らせを直接受ける。エンジンからの知らせが3つの経路で届く。
- 名残が4つある。
  - `disconnectArbiterSignals`: 中断の後の `bestmove resign` は、通番・`cancelAsync`・`isGameOver` の3か所で既に捨てられる。
  - `slotFor`: 呼び出し0。
  - `startEngine(Usi*, …)`: 転送するだけで、使うのは Strategy だけ。
  - `m_shutdownInProgress`: `AnalysisSession::stop` の処理中だけ真になり、読むのは `AnalysisSession` と `considerationuicontroller.cpp:38` だけ。
- `CsaEngineController`（約230行）は知らせを `ThinkingResult` に詰め替えて転送するだけ。`ThinkingResult::resign` はどこでも値が入らないので、`csagamecoordinator_moves.cpp:209` の分岐は通らない。
- 終局をエンジンに知らせる手順が2通りある（B16）。

**問題（探索の切り替え）**

- `UsiProtocolHandler::startSearch` は探索中でも position と go を送る（`usiprotocolhandler.cpp:129-149`）。探索が重ならないようにしているのは使う側で、それぞれ違う方法を使っている。
  - `UsiMatchHandler`: `Pending::PonderStop` を持ち、結果を受けた処理の中で同期的に次を始める（`usimatchhandler.cpp:44-50,111-116`）。
  - `AnalysisSession`: `StoppingSearch`・`RestartQueued` と0ミリ秒のタイマーで次を遅らせる（`analysissession.cpp:167-207`）。
  - `MateOracle`: 呼ぶ側の順序に頼る（`mateoracle.cpp:58`）。
- 空の `UsiPosition` が「position を送らない」の意味になっていて、読めない局面を `value_or(UsiPosition{})` で空にする所が6か所ある（U5）。

**書き直し案**

- 枠の部品は、リース・取り上げの知らせ・表示の消去だけにする。game に `MatchEngineSeat`（リース・手番・時間の出どころ・知らせの接続・終局の手順）を作り、人間対エンジン・エンジン同士・通信対局で共有する。投了などは `MatchStrategyHost` 経由で司令塔へ届ける。`CsaEngineController` は不要になる。
- `UsiEngineSession::search` を「探索中なら stop を送り、最新の依頼だけを覚え、前の結果を捨ててから始める」にし、空の局面は拒否する。`AnalysisSession` の状態は Searching と Waiting だけになり、`PonderStop` は要らなくなる。

**影響**: 約15ファイル、差し引き約280行減。`tst_background_tasks`・`tst_match_end`・`tst_gamestrategy`・`tst_csagamecoordinator`・`tst_usimatchhandler`・`tst_analysisflow`・`tst_usiprotocolhandler` を書き換える。

**状態: 前半（枠と仲介）は済（58eadff5）、後半（探索の切り替え）は済（81887c15）**。前半: `MatchEngineSeat`（game）を作り、人間対エンジン・エンジン同士の Strategy と `CsaGameCoordinator` が持つ。終局の知らせは `MatchEngineSeat::finish()` の1通り。`EngineLifecycleManager` は `acquire(owner, slot, name)`・`engineInSlot`・`holds`・`destroyEngines` と知らせ（`enginesChanged`・`engineRevoked`・`displayClearRequested`）だけになり、名残4つと `CsaEngineController` を消した（`m_shutdownInProgress` は `AnalysisSession` 自身の印にした）。U4 も直した。案から変えた点: 投了・入玉宣言勝ち・エラーは `MatchStrategyHost` を通さず、司令塔が Strategy を作ったときに `engineSeat(side)` のシグナルへ直接つなぐ（Strategy は中継しない）。対局時計の `ClockTimeSource` は司令塔が持ち、`MatchStrategyHost::timeSource()` で Strategy に渡す。ほかの持ち主に枠を取られたときは `MatchEngineSeat::errorOccurred` で知らせるので、ローカル対局も通信対局と同じく中断になる。後半: `UsiEngineSession::search` は最後の依頼だけを行い、探索中なら `UsiProtocolHandler::abandonSearch()`（stop を送り、結果と info を捨てる）の後、`searchEnded` を受けて始める。空の局面は受け付けない（false）。`UsiMatchHandler` の `Pending::PonderStop`、`AnalysisSession` の `StoppingSearch`・`RestartQueued`・再開のタイマーを消し、`MateOracle` も頼み直すだけにした。案から変えた点: 捨てる探索が stop に `kStopReplyMs` 答えなければエラーにする（次の探索を始められないため）。`stop()` は最後の依頼に効く（まだ始めていなければ始めてすぐ止め、結果は届く）ようにし、結果を知らせない取りやめは `cancelSearch()` に分けた。対局の go の時間は送るときの残り時間で決める（`UsiSearchRequest::goAtStart`）。検討の候補手の数は前の探索を止めた後に送る（`UsiSearchRequest::options`）。待機中の検討のやり直しはすぐ始まる（以前は次のイベントループまで待って、同じ回の変更をまとめていた。探索中の変更は今も最後の1つだけになる）。テスト用エンジン（`mock_usi_match`）は、探索中の stop に bestmove を2回返していたのを1回にした

### R9. MCP・CLI・自動化 API の表の二重管理と失敗の分類（mcp・cli・automation）。B14 の原因

**問題**

- **MCP のツール定義が C++ の表を手で写したまま（R10-3 の名残）**
  - 45ツールのうち20は引数を自動化 API にそのまま渡すのに、型・範囲・既定値を `tooldefs.py:458-817` に手で写している。
  - テスト（`test_param_specs.py:23-76`）が比べる属性は7つだけ。`describe` と突き合わせると、比べていない属性が22か所ずれていた。
  - CLI 経由のツールは、CLI と引数の名前・真偽・単位が違う。そのため `handlers_cli.py` がフラグを手で組み立て、テストの `MAPPED` が3つ目の対応表になっている。
    - `input_path` ↔ `--input`
    - `max_attack_pieces` ↔ `--max-attack`
    - `add_remaining_to_defender_hand` ↔ `--no-remaining-to-hand`（真偽が逆）
    - `timeout_seconds` ↔ `--timeout-ms`（1000倍）
  - CLI 自身も、`generate-tsume` の started イベントでは MCP 側の名前を出す（`clicommands_tsume.cpp:179`）。1つの引数に名前が3つある。
  - 結局、1つのツールの定義が4か所にある（C++ の表・`tooldefs.py`・`server.py` の handler 表とフラグの組み立て・テストの対応表）。手数が奇数かの検査も Python（`handlers_cli.py:259`）と C++（`clicommands_tsume.cpp:144,201`）にある。
- **失敗に分類がない**: `KifuConversionService`・`BoardImageRenderer`・`TsumeVerificationRunner` は bool と文字列で失敗を返し、CLI が分類を1つに決め打ちする（B14）。出力先の検査も3か所にあり、出力先がディレクトリのときの語がそれぞれ違う。
  - `cliargs.cpp:211-231`: `file_error`
  - `kifu.save`（`automationcommands_kifu.cpp:107-115`）: `file_exists`
  - `paths.py:142-155`: `invalid_argument`
- **棋譜全体の解析を MCP が Python で別に実装している**: 1局面ごとに `shogiboardq-cli analyze` を起動し直すので、局面の数だけエンジンの起動と usi/isready が走る（`handlers_kifu_analysis.py:18-45`、`jobs.py:138-191`）。評価値を先手側の向きに直す処理（`jobs.py:174`）も Python で書いていて、`EvaluationSeries::blackView` と別の系統になる。
- **`automation/` の同居**: GUI に依存しない共有部品（`paramspec`・`errorkinds`・メソッドの表・変換・`EngineCatalog`・Runner 3つ）と、GUI を操作する自動化 API が同じディレクトリにある。
  - 層の検査（`tst_layer_dependencies.cpp:61`）は app・dialogs・widgets・cli しか禁じていないので、CLI と共有する部品が ui/ を include しても通る。
  - `errorkinds.cpp` は数値を得るためだけに `automationdispatcher.h` を include している。

**書き直し案**

- CLI が引数を JSON（`--params-json` か stdin）でも受け、同じ `ParamSpec` で検査する。CLI の引数名を MCP に揃える（コマンドラインの旧名は別名として残す）。MCP は `describe` から生成して同梱する specs.json からスキーマを作り、テストはその生成物が最新かだけを確かめる。ツール1つは「名前・行き先（cli/app/queued）・MCP だけの引数・説明・要約の書式」の1行で宣言する。
- `errorkinds.h` に `Failure{ErrorKind kind; QString message; QString hint;}` を置く。GUI に依存しないサービスは分類つきで失敗を返し、`CliOutput::fail` と `AutomationError` はそれを写すだけにする。出力先の検査は1つの関数にまとめる。
- CLI に `analyze-kifu` を足し、1つの `UsiEngineSession` で局面を順に解析して JSON Lines で返す。MCP 側は普通のジョブにする。
- GUI に依存しない共有部品を別ディレクトリに分け、core〜analysis だけを参照できる規則を層の検査に足す。

**影響**: Python 4ファイルで約600行減、C++ は約20ファイルの移動と約250行の追加。

**状態: 段階1は済（1a283b33）、段階2は済（ad0f0d3a）、段階3は済（905f1c80）**。段階1: GUI に依存しない共有部品（`paramspec`・`errorkinds`・`automationmethodspecs`・`kifuconversionservice`・`enginecatalog`・`engineanalysisrunner`・`matesearchrunner`・`tsumeverificationrunner`）を `src/headless/` に移し、core〜analysis だけを使う規則を層の検査に足した（下の層も headless を使わない）。JSON-RPC のコード `AutomationErrorCode` は `errorkinds.h` に移し、`errorkinds.cpp` は `automationdispatcher.h` を使わない。出力先の検査は `OutputPath::check` 1つにして CLI と `kifu.save` が使い、ディレクトリを指すときは3経路とも `invalid_argument`（MCP の `paths.py` は前から同じ分類）。`Failure` に対処 `hint` を足し、CLI は `error.hint`、自動化 API は `AutomationError(Failure)` で `data.hint` に出す。案から変えた点: `BoardImageRenderer` は `ShogiView` で描くので views を使い、`src/automation/` に残した（CLI も使う）。段階2: CLI の引数名を MCP のツールに揃えた（`input_path`・`output_path`・`max_attack_pieces`・`max_defend_pieces`・`add_remaining_to_defender_hand`・`allow_final_move_alternatives`・`timeout_seconds`）。以前のオプション名は旧名（`ParamSpec::alias`・`aliasFlag`。ミリ秒の `--timeout-ms` は秒に切り上げ）で受け、既定値が true の真偽値は `--no-<名前>` で false にする。CLI は全部の引数を `--params-json` でも受け（同じ `ParamSpec` で検査）、MCP サーバーはフラグを組み立てずにこれで渡す。MCP は `describe` の出力を保存した `specs.json` からツールの `inputSchema` を作り（`tooldefs._spec`。変えるのは出さない引数・狭める範囲・MCP の許可ディレクトリの説明・MCP だけの引数）、テストは `specs.json` が最新かと、ツールが表の引数を出しているかだけを見る（対応表 `PASS_THROUGH`・`MAPPED` は消した）。MCP にしかなかった制約（USI の指し手の形・`action.trigger` の名前の形・`widget.text` の説明）は C++ の表に移した（`ParamSpec::pattern`）。偶数の手数の検査は CLI だけにし、ジョブの開始で CLI がエラーのイベントを出したらその語を返す。案から変えた点: ツール1つを1行で宣言するところまではしていない（説明と出力のスキーマ・ハンドラは今の形のまま）。段階3: CLI に `analyze-kifu` を足した。`EngineAnalysisRunner` が局面の列を1つの `UsiEngineSession` で順に解析し（`positionStarted`・`positionFinished`・`finished(stopped)`）、局面ごとに `position` イベント（先手から見た `score_cp_black`・`score_mate_black` を C++ で付ける）を出す。MCP の `analyze_kifu` は普通のジョブ1つになり、局面ごとに CLI を起動し直す `start_sequence` と評価値の向きを直す Python の処理を消した。`text` の一時ファイルは CLI が読み込んだ（`started`）後で消す

### R10. 画面部品の名残（widgets・dialogs・services）。B6・B17 の原因

それぞれ独立した小〜中の書き直しで、いつでも進められる。

| 箇所 | 問題 | 書き直し案 |
|---|---|---|
| ダイアログの大きさと入力欄の保存（R9-1 の移行が途中） | Hide で保存する `DialogFontScale::Options::size` を使うのは7ダイアログだけで、残りの17は `DialogUtils` をデストラクタ・`done`・`closeEvent` に各自で書く。closeEvent だけで保存するダイアログは取りこぼし（B17）、二重に保存するダイアログもある（`JosekiMergeDialog`・`KifuPasteDialog`・生成ダイアログ・問題一覧）。`SettingsFormBinding` にラジオボタンの組と `QDoubleSpinBox` がないため、6ダイアログが入力欄を手で読み書きしている | 大きさは `Options::size` に一本化する。`SettingsFormBinding` に2つの型を足す <br>**状態: 済（433dc612）**。15のダイアログを `Options::size` にし、詰将棋の2つの画面は大きさを構造体の設定から分けた（キーは同じ）。`SettingsFormBinding` に小数・ラジオの組（位置と2択）を足し、CSA 通信対局・定跡の展開・登録・マージのダイアログを表にした（文字の欄は前後の空白を除いて保存、コンボは範囲外を端に寄せる。テスト `tst_settings_form_binding`）。棋譜解析のダイアログは3章の「値を1つで返す」と一緒に直す。定跡ウィンドウはダイアログでないので `DialogUtils` のまま |
| エンジン登録の一覧（B6） | 画面の `QListWidget` が、ハンドラの一覧と行番号でつながる2つ目の正本になっている（`engineregistrationdialog.cpp:151-170,191,196`） | 正本はハンドラの一覧だけにし、画面は変更の知らせで作り直す。登録は成功してから一覧に足す <br>**状態: 済（433dc612）**。`EngineRegistrationHandler::enginesChanged`（読み込み・登録・削除）で画面の一覧を作り直し、選んでいたエンジンは名前で選び直す。登録は B6 で成功してから一覧に足すようにしてある |
| 盤の18色が5か所 | `boardcolors.h:9-44`（フィールド・`members()`・`supportsTransparency`）、`appsettings.cpp:40-81`（キーとメンバの対応表）、`settingskeys.h:84-107`（キー18個。どれも1回しか使われない）、`boardcolordialog.cpp:122-147`（表示名と objectName）、`boardappearancecatalog.cpp:158`（「4番目以降が対局者情報」という数字）。色を1つ足すと5か所を直す | `{member, key, label, objectName, transparent, group}` の表を1つにし、保存・ダイアログ・見本の適用の範囲をそこから作る <br>**状態: 済（433dc612）**。`BoardColors::Field{member, key, label, objectName, group, transparent}` の表 `BoardColors::fields()` 1つから、保存（`AppSettings::boardColors`）・配色ダイアログのタブとボタン・`supportsTransparency`・外観の見本の範囲（分類が盤でない色）を作る。`settingskeys.h` の18個のキーは消した（キーの文字列は変えていない） |
| 棋譜欄（`RecordPane`＋`RecordPaneAppearanceManager`） | マネージャは `QTableView*` を受ける関数の寄せ集めで、`RecordPane` の状態を `bool&` で書き戻す。文字の大きさを2か所で持つ。スタイルシートを2つの関数が別々に作って上書きする。列番号 1・2・3 を直書きしている（`KifuRecordListModel::Column` があるのに）。モデル差し替えの名残の Connection が3つ（1つは一度も接続されない） | マネージャを廃し、値 `Appearance{fontSize, visibleColumns, navigationEnabled}` と `applyAppearance()` の1か所にする。約860行が約550行に <br>**状態: 済（433dc612）**。`RecordPaneAppearanceManager` を消し、`RecordPane::Appearance` と `applyAppearance()` の1か所にした（列は `KifuRecordListModel::Column`。Connection の名残3つも消した）。約860行が約650行に |
| 通信ログのパネル（`UsiLogPanel`・`CsaLogPanel`） | 等幅のログ（5000行上限）・A-/A+・コピー・消去・コマンド欄という同じ構成を2回書き、挙動もずれている（USI は折り返し・「最新へ」・閲覧位置を保つ追記があり、CSA は `appendPlainText` だけ）。`relaxToolbarWidth` はコメント欄と合わせて3つ | 共通の `LogPanel` を作り、送信先と色・接続状態だけを足す。約590行が約420行に <br>**状態: 済（433dc612）**。`LogPanel`（ログ・文字の大きさ・折り返し・最新へ・全てコピー・消去・コマンドの欄・閲覧位置を保つ追記）を基底にし、USI はエンジン名・送信先・色、CSA は接続状態と「接続中だけ送れる」だけを持つ。CSA にも折り返し（`NetworkSettings::csaLogWrapLines`）と最新へが付き、コピー・消去の文言は USI に揃えた。`relaxToolbarWidth` は `ToolbarWidth::relax`（`toolbarwidth.h`）1つにした。部品の名前は、CSA のコピー・消去だけ USI に揃えて `csaLogCopy`・`csaLogClear` にした（以前の名前を参照する箇所は無かった）。ヘッダーを含めて742行が682行に（共通部分の説明を足したので見込みより減りは小さい） |
| 評価値グラフの設定（`EvaluationChartConfigurator`） | 設定の値・保存・自動目盛りの計算・部品の生成・モーダルダイアログの組み立てを1クラスが持つ。範囲（100〜100000 など）を3回書く。ダイアログの大きさを手で保存する。`EvaluationChartWidget` の get/set 11本と2つのシグナルは src からの呼び出しが0。`setFloating` は中身が空なのに合成ルートで接続している（`mainwindowcomposer_create.cpp:376-377`） | 計算は値型 `EvaluationChartRange` に、保存は `StructSetting`・`BoundedSetting` に、ダイアログは dialogs に移す <br>**状態: 済（433dc612）**。`EvaluationChartConfigurator` を消し、設定の値 `EvaluationChartSettings`（範囲の定数と `normalized()` はここだけ。保存は `StructSetting` の `AnalysisSettings::evalChart`、キーは以前と同じ）・軸の計算 `EvaluationChartRange::compute`・ダイアログ `EvaluationChartSettingsDialog`（dialogs。`settingsRequested` を受けて `DialogLauncher` が開き、大きさは `Options::size`）に分けた。get/set 11本・2つのシグナル・`setFloating` と合成ルートの接続を消した |
| 詰将棋局面生成ダイアログ | 結果の正本が `QTableWidget` の項目（UserRole）で、保存・コピー・盤の表示は項目から読み戻す。実行状態を bool 3つ（`m_running`・`m_stopRequested`・`m_runFailed`）と `m_lastRunStartedAt.isValid()` で表す（`tsumeshogigeneratordialog.h:147-151`） | 結果は値の一覧かモデルを正本にし、状態は `enum RunState` にする <br>**状態: 済（433dc612）**。採択した局面は値の一覧 `QList<Result>{sfen, pv}` を正本にし、表は表示だけにした（保存・コピー・盤の表示は一覧から読む。UserRole は使わない）。実行の状態は `RunState{NotStarted, Running, Stopping, Finished, Stopped, Failed}` 1つにした |

---

## 3. 中くらいの候補

| 候補 | 問題 | 書き直し案 |
|---|---|---|
| 棋譜解析の結果と読み筋ダイアログ（ui） | 解析した手順を `allLines()` の番号で覚える（U3）。同じ読み筋ダイアログを `AnalysisResultHandler`（表の行を作る処理と兼ねる）と `PvClickController` が別の規則で開く（対局者名を対局情報から取るか文書の対局者から取るか、盤の向き） | 解析した手順はノード ID で持つ。ダイアログを開く処理は1つの関数にする <br>**状態: 済（f119793c）**。U3 と同じく手順はノード ID で持つ。読み筋盤を開く処理は `PvClickController::showPvBoard()` 1つにし（対局者名は対局情報、盤の向きは本体の盤）、`AnalysisResultHandler` は `KifuAnalysisRowPresenter` に改名して表の行を作るだけにした |
| 盤の入力の判定（前回 R7 の名残） | 動作状態ごとの規則が `humanturnpolicy.cpp:18-36` と `boardmovecontroller.cpp:50,83,116` の3つの switch に分かれる。待機中のクリックは `recordBoardMove(QPoint&, QPoint&)` を通り、座標の写し（`boardmovecontroller.cpp:58`、`positioneditsession.cpp:99`）はもう不要（U7 も） | 規則を1つの表にし、`recordBoardMove` は `UsiMove` を受ける <br>**状態: 済（f119793c）**。規則は `HumanTurnPolicy::boardInput()`（`BoardInput{None, Edit, Variation, Game, CsaServer}`）1つにし、`BoardMoveController` は行き先で振り分けるだけにした。待機中の手もクリックを `UsiMove` にしてから `BranchTreeEditController::recordMove()` で記録する（`recordBoardMove(QPoint&, QPoint&)`・座標の写し・`canRecordBoardMove()` を削除） |
| 対局者ごとの設定（game） | `MatchPlayers` の6欄・`enginePath1/2`・`josekiOptions[2]`・`time.rules` に分かれ、連続対局で手番を入れ替えるとき4回 swap する（`consecutivegameseries.cpp:59-62`） | `std::array<SeatPlan, 2>` にして `Turn` で引く <br>**状態: 済（f119793c）**。`MatchPlayers` を `std::array<Seat, 2>`（`seat(Turn)`）、`MatchStartOptions` のエンジンのパスと定跡の条件を `std::array<EngineSeat, 2>`（`engine(Turn)`）にした。手番の入れ替えは `MatchStartOptions::swapSides()` と `TimeControl::swapSides()` の2つ |
| CSA の状態（network） | `ConnectionState` と `GameState`（どちらも8値）を並行して進める。手番を表す値が client と coordinator に分散。`m_playerType`・`m_thinkingMarginMs` は `m_options` の写し | `GameState` を `ConnectionState` と「最後の結果」から導く <br>**状態: 済（cad2da29）**。`gameState()` は持たずに求める。接続の段階はクライアントの `ConnectionState` から、対局中・終局後・失敗は司令塔だけが知る段階 `Phase`（`Off`・`Active`・`Playing`・`Ended`・`Failed`）から決め、変わったときだけ知らせる。クライアントの状態をなぞる `setGameState` の呼び出しをなくした（`CsaClient::reject` は以前から対局待ちに戻していた）。手番の写し `m_turnSide` は `CsaGameProgress::sideToMove()` に、`m_playerType`・`m_thinkingMarginMs` は `m_options` に一本化。接続を始められなかったとき（ポート番号が不正など）に接続中のまま残っていたのを `Error` にした |
| 入玉宣言の判定（core・game） | 宣言の条件と24点法・27点法の閾値が `JishogiCalculator`（`jishogicalculator.cpp:91-134`）と `NyugyokuJudgement`（`nyugyokujudgement.cpp:22-80`）の2か所にある。rule は int | core が判定を enum で返し、game は文言だけを作る <br>**状態: 済（f119793c）**。`JishogiCalculator::judge()` が `Rule` で `Verdict` を返し、閾値は core だけで決める（`result24`・`result27` を削除）。`NyugyokuJudgement` と点数のダイアログは文言だけを作り、持将棋ルールは `std::optional<JishogiCalculator::Rule>` で受ける |
| 時間の型の名残（engine・network） | `UsiTimingParams` は `UsiGoTimeInput` の一部の写し。`csagamecoordinator.cpp:364` は `FixedTimeSource` を変換関数代わりに使う | `CsaGameProgress` が `UsiGoTimeInput` を直接返す <br>**状態: 済（f119793c）**。`UsiTimingParams` を削除し、`FixedTimeSource` と `CsaGameProgress::engineTiming()` は `UsiGoTimeInput` を直接使う |
| 考慮時間の計測（game） | 人間対人間だけ Strategy の `QElapsedTimer` で上書きし、人間対エンジンの人間の手は時計で測る。待ったでは両方を巻き戻す | 人間の手は時計の値を正本にする <br>**状態: 済（f119793c）**。人間対人間の `QElapsedTimer` と時計の上書きをやめ、人間の手は人間対エンジンと同じく時計が測った値を使う。待ったは時計の `undo()` だけで戻す |
| 対局の盤に局面編集が同居（game） | `ShogiGameController` が局面編集も受け持つ。`PieceMoveRules::shouldAutoPromote` は `shogiboard.cpp:120` の `isDeadSquare` と同じ判定 | 局面編集の規則を board と `PieceRules` に移す <br>**状態: 済（f119793c）**。局面編集の駒の移動は board の `PositionEditRules`（`apply()`・`canMove()`）にし、`ShogiGameController` から編集の関数を、game から `PieceMoveRules` を消した。必成は `ShogiBoard` の `PieceRules::isDeadSquare` の1か所 |
| 開始設定を対局中も持ち続ける（game） | 再開後は `options.recordLine` が古いまま。`resumeFromBreakOff` は書くだけ。`MatchGameOverState` の4欄はテストだけが読む | 対局中に要る値だけを持つ <br>**状態: 済（f119793c）**。`MatchSessionState` は開始の手順を持たず（手順は `record()`）、`resumeFromBreakOff` を削除した。終局の状態は `std::optional<GameEndInfo>`（`gameEnd()`）1つにし、テストだけが読む欄を消した |
| 登録エンジンの扱い（services・engine） | `EngineEntry` は `EngineListSettings` にあり、保存は `EngineProfileStore` で、互いに include し合う。`EngineListSettings::loadEngines` は転送だけ。名前での検索が `indexOfName`・`pathForName`・`EngineCatalog::findByName` の3通り。option 行を `UsiOptionDeclaration` に読んだ後、`EngineOption` にもう一度読み直す | 一覧と find を持つ値を services に1つ置き、option は1つの型にする <br>**状態: 済（f119793c）**。`EngineEntry` と一覧・検索を値型 `RegisteredEngines`（services）1つにし、名前の検索は `findByName` だけにした（`EngineCatalog::findByName` は候補名の説明を添えるだけ）。`EngineListSettings` は選択の保存だけで、`EngineProfileStore` と include し合わない。オプションは `parseOption` が直接 `EngineOption` を返す（`UsiOptionDeclaration` を削除） |
| 「成れた手か」の判定と KI2 の表記（core・kifu） | 同じ判定が4つ（`PieceRules::couldPromote`（呼び出し0）、`KifMoveText::couldPromote`、`fmvconverter.cpp:242`、`Annotation::canPromote`）。`Ki2MoveNotation::format`（`ki2movenotation.cpp:172`）は `KifMoveText::append` とほぼ同じ文字列を別に組む | 判定を1つにし、KI2 の表記は `KifMoveText` を使う <br>**状態: 済（f119793c）**。判定は `PieceRules::couldPromote` 1つにし、`KifMoveText::couldPromote`（成った手・駒打ちを除く）・KI2 と JKF の「不成」はこれを使う（`Annotation::canPromote` を削除）。合法手生成（fmv）の中の判定は独自の型の上の計算なので残した。`Ki2MoveNotation::format` は `KifMoveText::format` で作る |
| USEN の符号（kifu） | `encodeMove`・`decodeMove` が USI の文字列を経由する（`usencodec.cpp:55,80`）。分けた部分を `"~0."`＋手で組み直し、`decodeMoves` がまた分ける（`usentosfenconverter.cpp:199,214`）。`parseUsenString` は `QList<QPair<int, QString>>` の出力引数 | `UsiMove` で受け渡し、分割は1回にする <br>**状態: 済（f119793c）**。符号化と復号は `UsiMove` で受け渡し、手順は `UsenCodec::decodeLines()` が "~" で1回だけ分けて `UsenLine{offset, moves, terminal}` の列にする（`parseUsenString`・`decodeMoves` を削除）。オフセットの読めない手順は、以前は黙って捨てていたが、読めない棋譜として知らせる |
| `ShogiBoard` の公開 API（core） | 補助の9関数（`addStandPiece`・`consumeStandPiece` など）は src から呼ばれずテストだけが呼ぶ。`flipSides`（`shogiboard.cpp:225`）は src から呼ばれず、局面を変えても知らせない | 公開を `setPosition`・`applyUsiMove`・編集操作3つに絞る <br>**状態: 済（f119793c）**。局面を変える公開の関数は置き換え（`setPosition`・`setSfen`）・`applyUsiMove`・局面編集の3つ（`updateBoardAndPieceStand`・`promoteOrDemotePiece`・`resetGameBoard`）にし、補助の6関数は非公開（単体テストは friend）。`flipSides` は削除した |
| CSA の文字コードの判別（kifu） | CSA だけ `readFileText`（`csatosfenconverter.cpp:23`）が別の順で判別し、UTF-16・EUC-JP を見ない | 宣言がなければ `KifReader` で判別する <br>**状態: 済（f119793c）**。宣言があればそれに従い、無ければ `KifReader::decodeAuto()`（判別だけを関数にした）で読む。UTF-16 の BOM や EUC-JP の CSA も読める |
| 盤の拡大・縮小（views・ui） | Ctrl+ホイールの処理が3か所（`ShogiView::wheelEvent` と、`MainWindowAppearanceController`・`BoardViewerWidget` の eventFilter）。`ShogiView` 側は横取りされて通らない。通知も `fieldSizeChanged`（`emitSignal` 引数つき）と `boardResized` の2つ | 操作1つ・シグナル1つにする <br>**状態: 済（f119793c）**。Ctrl+ホイールは `ShogiView::wheelEvent()` だけで扱い（横取りの `eventFilter` 2つを削除）、拡大・縮小は常に `fieldSizeChanged` で知らせる（`emitSignal` 引数と `boardResized` を削除）。盤ビューアはこの知らせでウィンドウを合わせる |
| 駒箱の位置（views） | `BoardGeometry` に入っていない。`ShogiView::pieceBoxRect`（`shogiview.cpp:532-544`）が子ウィジェットの位置と表示状態から求めるので、配置の順序に依存する | カードの高さを渡して `BoardGeometry` で計算する <br>**状態: 済（f119793c）**。`BoardGeometry::pieceBox(cardHeight, areaHeight)` がカードの配置と同じ式で求める。`ShogiView` は子ウィジェットの位置と表示状態を読まず、カードの高さ（`BoardChromeWidget::cardHeight()`）だけを渡す |
| 詰将棋問題一覧のプレビュー（widgets） | `TsumePositionPreview` が Hayanagi の `shogi::Position` を使い、駒種を1つずらして変換し、24px の座標を直書きする。core の `Position`・`BoardGeometry` と別系統（U8） | core の `Position` と `BoardGeometry` で描く <br>**状態: 済（f119793c）**。局面は core の `Position`、配置は `BoardGeometry`（マス22px）、盤・駒台の木肌は盤と同じ `BoardStaticLayer`、持ち駒は `PiecePainter::drawStack` で描く（Hayanagi の局面型・駒種のずらし・固定の座標を廃した） |
| 解析結果の実行状態・検討タブの名残（widgets） | `AnalysisResultsPanel::setStopButtonEnabled(bool)` が実行状態と状態文を兼ねる（完了時は「解析中止」を書いてから「解析完了」で上書き）。`ConsiderationTabManager` は中身が QWidget のパネルなので名前が役割と合わない。`setSettings` は呼び出し0。検討時間の既定値が3か所 | 検討タブと同じ `RunState` にし、`ConsiderationPanel` に改名して名残を消す <br>**状態: 済（f119793c）**。`AnalysisResultsPanel` は `RunState` 1つで進み具合の文・中止ボタン・選択の受け付けを決め、`finishAnalysis(completed)` で中止か完了にする。`ConsiderationTabManager` は `ConsiderationPanel` に改名し、`setSettings` を削除。検討時間の範囲と既定値は `AnalysisSettings::considerationByoyomiSec`（`BoundedSetting`）だけで決める |
| 書体の規則（widgets） | R9-5 で `MoveFont` の1か所にしたはずが、`BranchTreeView`（`:73-79`）と `EngineInfoWidget`（`:297-298`）が同じ判定を自前で書く | `MoveFont::font` を使う <br>**状態: 済（f119793c）**。`BranchTreeView`・`EngineInfoWidget`（と盤の段の文字・詰将棋のプレビュー）は `MoveFont::font` を使う |
| 定跡の値と表の表記（kifu・dialogs） | `JosekiMove` の予想応手が文字列の番兵（null は「変えない」、空と "none" は「無し」）で、dialogs の4か所と kifu の2か所で変換する。`JosekiTableModel`（`josekitablemodel.cpp:19-33`）は `KifuPresentation::move` の写しで、入力欄（「不成」を付ける）と表記が違い、指した後の局面を `data()` のたびに計算する | 予想応手は `std::optional` と「変えない」の引数に分ける。表記は `KifuPresentation` に寄せ、指した後の局面は行の値に持つ <br>**状態: 済（f119793c）**。予想応手は `std::optional<QString>` にし、「変えない」は `updateMove` の `NextMoveUpdate::Keep` に分けた（"none" はファイルの読み書きと、入力欄の「なし」の項目の値 `JosekiMoveInputWidget::kNoMove` だけ）。表は `KifuPresentation::move` で書き、指した後の局面は `setMoves` で1回だけ求める（入力欄の「不成」は、同じ升への成・不成を一覧で見分けるために残した） |
| ダイアログのロジック（dialogs） | `CsaGameDialog` が shogi-server のパスワード欄の符号化と接続履歴の MRU 処理を持つ（`csagamedialog.cpp:263-273,324-335`）。`TsumeCollectionDialog` が判定エンジンを名前に "KomoringHeights" が入るかで決める。`KifuAnalysisDialog` が値を4つの getter で返し、呼ぶ側（`kifuanalysiscontroller.cpp:77-79`）が分岐と qBound をやり直す | 符号化は network へ、MRU は `RecentFileList` を一般化して使う。エンジンの判定は登録情報で行う。解析の範囲と時間は値1つで返す <br>**状態: 済（f119793c）**。パスワード欄の組み立ては `CsaClient::loginPassword()`（network）へ、接続履歴は `RecentFileList` を一般化した `RecentList<T>` で扱う。判定エンジンは登録時に報告したオプション（`TsumeMateEngine::canVerifyShortestMate`。起動後の確認も同じ規則）で絞る。棋譜解析の範囲と時間は `KifuAnalysisDialog::options()`（`AnalysisRequest::Options`）1つで返し、範囲はダイアログで最大手数に収める |
| 棋譜ファイルの選択（dialogs・ui） | フィルタの文字列を3か所に手書きしていて、取り込みの方は `*.sfen` が抜けている。前回のフォルダの扱いが3通り。フォルダの走査は `.json` を拾わない | `KifuFormat` の表からフィルタと拡張子を作る <br>**状態: 済（f119793c）**。開ける拡張子は `KifuFormats::readableSuffixes()`（形式の表）から作り、ダイアログの絞り込みは `KifuFileFilter::forOpening()` 1つにした（どれも .sfen・.json・形式ごと・すべてのファイルを含む）。定跡に棋譜から登録するときの前回のフォルダは棋譜を開くときと同じ。フォルダの走査は .json も拾う（局面だけの .sfen は除く）。定跡の一括取り込みは取り込み用の前回のフォルダのまま |
| 盤の対局者名（ui） | `PlayerInfoPresenter` が盤の名前を2経路で書く（`gamesession.cpp:205` の `showPlayers` と `_wire.cpp:105` の `headerReset`） | 文書の知らせと動作状態から毎回求め、`GameSession` の Deps から外す（R6・R7 と一緒に） <br>**状態: 済（f119793c）**。`PlayerInfoPresenter` が文書の知らせ（対局者・対局情報）と動作状態の変化を自分で受け、そのたびに求め直す。`GameSession` の Deps と合成ルートの接続から外した |
| 合成ルートの関数注入と `AppState`（app） | 合成ルートにロジックを持つ関数3つ（`mainwindowcomposer.cpp:67-84`）とラムダ2つ（`_create.cpp:204,402`）。`AppState` は欄が1つだけの包みで、CLAUDE.md は「`AppState*` を渡す」とするが、10部品が `&m_state.activity` を受ける | 型付きのポインタで渡す。`AppState` は廃止して `AppActivityState*` に統一する（R1 と一緒に） <br>**状態: 済（f119793c）**。`AppState` は前の監査対応で廃止済み（部品は `AppActivityState*` を受ける）。合成ルートの判断を持つ関数4つとラムダ2つを消し、部品は型付きのポインタ（`HumanTurnPolicy*`・`const ConsiderationSettings*`）か部品の関数（`PositionEditSession::allowsLeavingMove`・`MatchCoordinator::hasHumanPlayer`・画面ごとの `addPendingEditor`・`addLeaveGuard`）を受ける |
| 操作の表と実装の表（cli・automation） | CLI の引数の表と `main.cpp:284` の handler 表が別で、各コマンドが自分の名前と「JSON Lines か」を渡し直す（表がずれると release ではヌルポインタ参照。`cliargs.cpp:145-148`）。UI 操作の12メソッドが、予約前の検査と `check` に同じ検査を2回書く。ドック操作は操作名の文字列を2か所で比べる。指し手の JSON を `kifu.get` と `movesToJson` が別々に作る | (spec, handler, 出力の種類) を1行で登録する。`queue()` が予約の時点でも `check` を走らせる <br>**状態: 済（f119793c）**。CLI は (名前, 実装, 出力の種類) を `main.cpp` の表の1行で登録し、`CliCommands::run()` が引数を検査してから渡す（表がずれても `unknown_operation`。`CliArgs::parse` を削除）。`AutomationInteractions::queue()` が予約の時点でも `check` を走らせ、12メソッドの予約前の重複した検査を消した。ドック操作は `DockOperation` に変えてから `switch` で振り分ける。指し手の JSON は `RecordJson`（headless）1つ |
| 設定の名残（services） | `BoundedSetting` は3項目だけで、読む側の qBound が14か所ある（`evaluationchartconfigurator.cpp:74-78` など）。接頭辞だけ違う設定を2回書く（`josekisettings.h:128-151`、`gamesettings.h:81-93`）。`UiNotificationService`（QMessageBox）・`ScreenshotService`・`loadWindowSize(QWidget*)` が kifu より下の層にある | 範囲を宣言に移す。`StructSetting` に接頭辞を渡せるようにする。GUI に依存する部品は ui か style へ移す <br>**状態: 済（cad2da29）**。メニューウィンドウのボタンの大きさ・分岐ツリーの倍率・対局情報の項目列幅を `BoundedSetting` にし、読む側の `qBound` と画面ごとの範囲の定数を消した（範囲は宣言の `minimum()`・`maximum()`。駒音の音量の定数も同じ）。詰将棋の2つのダイアログの持ち時間は入力欄の範囲に任せる。キーを実行時に組み立てられるようにし（`Setting`・`SettingField` はキーの写しを持つ）、グループ名だけ違う定跡の条件と対局者の項目は、宣言を返す関数1つ（`playOptionsSetting`・`playerSettings`）から作る。メッセージボックスは ui の `NotificationPresenter`（`UiNotificationService` を改名）、スクリーンショットは ui/controllers へ移し、ウィンドウの大きさの読み書き（`loadWindowSize`・`saveWindowAndBoard`）は合成ルートで宣言を直接読み書きする |
| 名前と役割（ui） | `KifuSaveCoordinator`・`AboutCoordinator` は関数の名前空間。`KifuLoadCoordinator` は読み込み器。`AnalysisResultHandler` の役割が名前と違う。`LanguageController` は言語5つをスロット5つで書く（表記の項目は `QActionGroup`） | 改名と説明の修正。言語も `QActionGroup`＋data にする <br>**状態: 済（cad2da29）**。`KifuLoadCoordinator` は GUI に依存しない読み込み器なので kifu の `KifuLoader` に、関数だけの `KifuSaveCoordinator` は dialogs の `KifuSaveDialogs`（翻訳の文脈も自分の名前。`QObject::tr` の借用をやめた）にした。`AboutCoordinator` は2行の転送だったので消し、`DialogLauncher` が `VersionDialog` を開く。未保存の確認の見た目は棋譜と定跡で共通の `UnsavedChangesPrompt`（widgets）にした（4章の二重の処理も解消）。言語は `QActionGroup` 1つで項目の data に言語コードを持ち、`onLanguageTriggered` 1つで受ける（5つのスロットと `changeLanguage` を削除） |

---

## 4. 小さな修正・使われていないコード

監査の報告に基づく一覧。消す前に grep で確かめること。

- **呼び出しが0**
  - core・kifu: `SfenComponents`（`shogitypes.h:33`）、`SfenUtils::isHirateBoardSfen`、`PieceRules::couldPromote`、`Position::after`、`ShogiClock::considerationTimeString`、`MatchTimeRecord::clearStart`、`ShogiNotation::movePieceName(QChar,bool)`、`Ki2Lexer::isResultLine`・`resultPatternRe`
  - engine・game・analysis: `EngineLifecycleManager::slotFor`、`MateOracle::isReady`（`m_ready` はこのためだけにある）、`LocalGameChannel::players`・`timeControl` と `m_clock`
  - app・ui: `KifuDisplayPresenter::refreshLabels`・`BranchTreeView::refreshLabels`、`MenuWindowController::displayMenuWindow` とシグナル `actionTriggered`・`favoritesChanged`、`threadtypes.h` の `JobGeneration`、`KifuAnalysisController` の `m_ownedResultsPanel` の分岐、`KifuFileController::copyRecord` の Sfen/Bod の分岐（到達しない）
  - widgets・views: `ShogiView::clearArrows`、`kWarn10Ms`/`kWarn5Ms` の別名、`CommentEditorPanel::clearCommentDirty`、`AppToolTipFilter` の `installOn`・`installOnMenuBar`・`installOnToolBar`・`installOnMenu`、`ButtonStyles` の `navigationButton`・`customizeSettings`・`tablePlayButton`・`tableEditButton`・`tableDeleteButton`
  - dialogs: `TsumeshogiKanjiBuilder::pieceCharToKanji`、`JosekiTableModel::sfen()`
  - services: `KifuPresentation::resolveLanguage`
- **テストからしか呼ばれない**
  - core・kifu: `Position::countOnBoard`、`ShogiClock::totalConsiderationTimeString`・`considerationAndTotalTime`、`ShogiNotation::findPieceName`、`NotationUtils::formatSfenMove`・`formatSfenDrop`、`KifuPresentation::configure`、`JosekiBookSession::positionKey`・`setModified`、`KifuBranchTree::setComment`・`hasBranch`、`RecordLine::fromPositions`、`LiveGameSession::moveCount`
  - engine・game・analysis・network: `ShogiGameController::validateAndMove`・`lastMove`、`MatchCoordinator::clock`、`TsumeshogiGenerator::m_screenUnknownCount`、`CsaClient::isAwaitingMoveConfirmation`（`awaitingMoveConfirmation` と同じ）
  - app・ui: `UiStatePolicyManager::stateChanged`・既定コンストラクタ、`ErrorBus::postError`・`errorOccurred`（「後方互換」とコメントにあるが src からは使われない）、`KifuNavigationController::stepActionsEnabled`、`KifuAnalysisController::Deps::engine`
  - widgets・views・models: `ShogiViewHighlighting::clearHighlights`、`ShogiEngineThinkingModel::appendRow`、`MenuButtonWidget::dragStarted`、`EvaluationChartWidget` の get/set 11本
  - dialogs: `JosekiTableView::emptyGuideLabel`、`JosekiFileController::saveTo`、`JosekiWindow::isRecordingBoardMoves`
  - mcp: `environment.error_slug`（アプリが必ず `data.slug` を付ける）
- **転送だけ・名残の層**
  - `KifToSfenConverter`・`Ki2ToSfenConverter`（4ファイル、どれも1行）、`NotationUtils::mapHandicapToSfen`・`handicapLabelForSfen`、`KifReader::readLinesAuto`（「互換API（旧名）」、呼び出し1）、`ConsiderationPositionResolver`（呼び出し1。前処理を二重にしている）
  - `MainWindowActions::resetRecordForEditedPosition`、`MainWindowActions::Deps::gameController`（使わない）、`JosekiWindowController::Deps::gameController`（Q_ASSERT 以外で使わない）
  - `CliPosition` は `UsiPosition` の写し
  - `BranchTreeEditController::canEdit` と `canRecordBoardMove` は本体が同じ
- **書くだけで読まない・何もしない処理**
  - `BranchLine::name`（`kifubranchtree.cpp:203,207`）、`LiveGameSession::m_entries`（件数しか使わない）、`KifuNodeReplay::m_mover`（board の手番の写し）
  - `enableNavigationIfAllowed`（`_wire.cpp:113`。局面送りの有効/無効は表だけが決める）
  - `EvaluationChartWidget::setFloating`（中身が空なのに接続されている）
  - `csaclient_parser.cpp:375` の `#CHUDAN`（318行で先に処理される）
  - `ThinkingResult::resign`（値が入らないので `csagamecoordinator_moves.cpp:209` の分岐は通らない）
  - `PlayerCard` の HTML のツールチップ（eventFilter が先に横取りするので表示されない）
  - `josekiwindow.cpp:53,302` の 950x600（設定の既定値 800x500 で必ず上書きされる）。`JosekiWindow` の `Qt::Window`・`closeEvent`・`updateWindowTitle` はドックの中では効かない
  - `CommentEditorPanel::commentUpdated` の moveIndex（受け手が捨てる）
- **二重の処理**
  - `DockLayoutManager` の `actionLockDocks` の `setChecked` と `setSavedLayoutsMenu` を、合成ルート（`_create.cpp:382,385`）と `wireMenuActions` の両方で行う
  - `BoardInteractionController` が選択のたびに盤へ接続し直す（`boardinteractioncontroller.cpp:201-205`）。盤は起動から終了まで1つなので、コンストラクタで1回つなげば足りる
  - `BranchTreeView::rebuild` で `allLines()` を2回作る
  - 矢印の寸法を `arrowsPaintRect` と `drawArrows` で別々に計算する
  - `AnalysisResultHandler` が「同　」を文字の位置で差し替える（`:94`）。`KifMoveText` の `previousTo` で作れる
  - `JosekiImport::winnerOf`（`josekiimport.cpp:28`）が勝敗の表を `RecordEntry::endingSide` と別に持ち、記録した側ではなく SFEN の手番を使う
  - `isTurnMark` が `ki2lexer.cpp` と `kifmovegrammar.cpp` の2か所
  - `position.set`（`automationcommands_position.cpp:247-251`）が kifu の `requireSavedOrDiscard` と同じ処理を写す
  - 未保存の確認ダイアログが `KifuSaveCoordinator` と `JosekiFileController::confirmDiscardChanges` の2つ
  - `PieceSoundPlayer` の `kMin/Max/DefaultVolume` が `BoundedSetting` の値を二重に持つ
  - `BoardImageRenderer` が `square_size` の範囲を表とは別に再検査する。読めない `last_move` は黙って無視する（`--last-move zzzz` でも成功する）
  - 詰将棋のワーカー（`TsumePositionAnalyzer`・`TsumeGameSession`）が `CancelFlag` とは別に `shared_ptr<atomic_bool>` を持ち、メインスレッドで `waitForFinished` し、`unique_ptr` と親を併用して `release()->deleteLater` する
  - `ChangeEngineSettingsDialog` がハンドラを `unique_ptr` と QObject の親の両方で持つ（`changeenginesettingsdialog.cpp:24`）
- **表記・語の不統一**
  - 詰みの表記が3通り: 思考の表は「3手詰」、解析の表は「mate 3」（訳なし）、グラフは「先手勝ち・詰みまで3手」
  - `server.py:129` の `unknown_tool` はエラーの分類表にない語。`unknown_operation` にする
  - `QObject::tr`（`AnalysisResultHandler:22`）や "MainWindow" の文脈の借用（`dialoglauncher.cpp:61`）
  - `describePosition`（`josekiwindow.cpp:87`）が SFEN を split で読み、手合を表示名 "平手" の文字列比較で判定する（`:898`）
  - `KifuMergeEntry` の `ply`・`isCurrentMove` に初期値がない
- **古いコメント**
  - 設定ヘッダーの「呼び出し元」に、存在しないファイルが5つ載っている（`mainwindowuibootstrapper.cpp` など）。`enginedialogsettings.h` が存在しない `enginesettingscoordinator.h` に触れる
  - `EngineAnalysisRunner::start` の説明が「false を返す」だが、実際は void
  - `engineregistrationhandler.h:5,15`（ワーカースレッド）、`pvnotationformatter.h:16`（UsiInfoParser）、`csaclient.cpp:135`（入れ子イベントループ）
  - 中身のない節見出し（`enginelifecyclemanager.cpp:21`、`matchcoordinator.cpp:240-250,454-460`）
  - `kifubranchtree.cpp:305` の「ResolvedRow互換」、`jkfformatter.h` の `timeToJkf` の説明
  - `josekiwindow.cpp:549`、宣言のないコメント（`engineregistrationdialog.h:48,50`、`csagamedialog.h:169,171`、`josekimergedialog.h:90-92`）
  - `MainWindowAppearanceController` の説明に消えた責務が残る
  - `shogiview.h:24` の `shogigamecontroller.h` の include は使っていない（ほかのファイルがこの include 経由の型に頼っていないかは確かめていない）

**状態: 済（87b95924）**。一覧を1件ずつ grep で確かめて直した。それまでの対応で既に消えていたもの（`Position::after`・`MatchTimeRecord::clearStart`・`Ki2Lexer::isResultLine`・`EngineLifecycleManager::slotFor`・`TsumeshogiKanjiBuilder::pieceCharToKanji`・`CommentEditorPanel::clearCommentDirty`・`EvaluationChartWidget` の get/set・`KifReader::readLinesAuto`・`ThinkingResult`・`enableNavigationIfAllowed`・`EvaluationChartWidget::setFloating`・`KifuMergeEntry`・`commentUpdated` の moveIndex・定跡ウィンドウの 950x600 と `closeEvent`・`JosekiWindowController::Deps::gameController`・`canRecordBoardMove`・「同　」の差し替え・`position.set` の写し・`describePosition`・`EngineAnalysisRunner::start` の説明・`last_move` を黙って無視する処理）は確かめただけ。

- 消したもの: `SfenComponents`、`SfenUtils::isHirateBoardSfen`、`ShogiNotation::movePieceName(QChar, bool)`・`findPieceName`（テストは `pieceFromName` に）、`ShogiClock::considerationTimeString`・`totalConsiderationTimeString`・`considerationAndTotalTime`（テストは `recordTime()` で読む）、`MateOracle::isReady` と `m_ready`、`LocalGameChannel` の `players`・`timeControl` と時計（通信対局の窓口だけが持つ）、`KifuDisplayPresenter`・`BranchTreeView` の `refreshLabels`、`MenuWindowController::displayMenuWindow` と転送だけのシグナル2つ、`JobGeneration`（スレッドの指針も今のやり方に書き直した）、`KifuAnalysisController` が結果の表を自分で作る分岐（表は必須の依存にし、作るときに1回だけ結ぶ）、`ShogiView::clearArrows`・`ShogiViewHighlighting::clearArrows`・`clearHighlights`、時計の閾値の別名、`AppToolTipFilter` の `installOn*` とメニューバーの分岐、使わないボタンのスタイル5つ、`JosekiTableModel::sfen()`、`KifuPresentation::resolveLanguage`・`configure`（`NotationStyle` を直接呼ぶ）、`JosekiBookSession::positionKey`、`RecordLine::fromPositions`（テストの補助 `TestReplay::recordLine` へ）、`ShogiGameController::validateAndMove`・`lastMove`、`MatchCoordinator::clock`（テストは部品の時計を読む）、`CsaClient::isAwaitingMoveConfirmation`、`ErrorBus::postError`・`errorOccurred`、`KifuNavigationController::stepActionsEnabled`、`JosekiTableView::emptyGuideLabel`（オブジェクト名で引く）、MCP の `environment.error_slug`（アプリは必ず `data.slug` を付ける）、`NotationUtils`（転送だけ。呼び出し側は `Handicaps` を直接使う）、`ConsiderationPositionResolver`（`KifuNavigationState::currentUsiPosition()` にした）、`MainWindowActions::Deps::gameController`、`CliPosition`（`UsiPosition` を返す）、`BranchLine::name`、`LiveGameSession::m_entries`（件数だけ持つ）、`#CHUDAN` の2つ目の分岐、`PlayerCard` の HTML のツールチップ、定跡ウィンドウの `updateWindowTitle`、`KifuSaveCoordinator`・`AboutCoordinator`（3章で改名・削除）、設定ヘッダーの「呼び出し元」と存在しないファイルへの言及、中身のない節見出し、宣言のないコメント、`shogiview.h` の不要な include
- 二重の処理: ドックの固定と保存したレイアウトのメニューは `wireMenuActions()` だけで初期化する（使わない `clearStartupLayout` 引数も削除）。盤の操作は盤の知らせにコンストラクタで1回だけつなぐ。分岐ツリーは作り直しのたびに `allLines()` を1回だけ作る。矢印の寸法と始点・終点は描画と描き直す範囲で同じ関数から取る。定跡の取り込みの勝敗は `RecordEntry::winner()`（`endingSide` の隣に置いた逆の規則）で決める。`isTurnMark` は `ShogiNotation` の1つ。未保存の確認は `UnsavedChangesPrompt`（3章）。駒音の音量の範囲は設定の宣言だけ（3章）。`BoardImageRenderer` の範囲は盤の定数（`BoardGeometry`）から取る。詰将棋のワーカーは `CancelFlag` を使い、止めるときにメインスレッドで終わるのを待たず、`unique_ptr` と QObject の親を併用しない（止めたワーカーは終わったら自分で消える）。エンジン設定のダイアログのハンドラは `unique_ptr` だけで持つ
- 表記: 棋譜解析の表の詰みは思考の表と同じ「3手詰」（手数が無ければ「詰み」。訳のない「mate 3」をやめた）。評価値グラフのツールチップは先後を名指しする文なので「先手勝ち・詰みまで3手」のまま。MCP の未知のツールは分類表の `unknown_operation`。`DialogLauncher` は自分の翻訳の文脈を使う
- ガイドの画像の撮り直しで見つけた退行: 定跡のマージのダイアログで、文字の大きさに合わせる処理（R2 で列幅の設定をまとめた）が最後の「状態」の列まで内容の幅に縮め、表の右に空きが出ていた。最初の3列だけ内容に合わせ、最後の列は右端まで伸ばす（テスト `emptyStatesAndMergeSelection`）。詰将棋問題一覧のプレビュー（3章で盤と同じ描画部品にした）は、盤と駒台の周りを主画面の背景色（灰緑）で塗り、マス22pxでは持ち駒の枚数が読みにくかった。周りはカードの地のままにし、マスを26pxにした（テスト `tsumePreviewKeepsCardBackground`）
- 残したもの（理由つき）: `KifToSfenConverter`・`Ki2ToSfenConverter`（形式ごとの `parse` の表を `KifuLoadParser` で揃える入口）、`MainWindowActions::resetRecordForEditedPosition`（QObject でない `SessionResetCoordinator` へのシグナルの受け口）、`KifuNodeReplay::m_mover`（局面を取り直せないときも手番を交互に進めるので盤の手番の写しではない）、`LiveGameSession::moveCount`・`ShogiEngineThinkingModel::appendRow`・`MenuButtonWidget::dragStarted`・`TsumeshogiGenerator::m_screenUnknownCount`・`JosekiFileController::saveTo`・`JosekiBookSession::setModified`（テストが状態を作る・確かめるのに使う。`setModified` は接続しないので slots から外した）、`KifuAnalysisController::Deps::engine`（テストでエンジンを差し込む）、ファイルの読み書きなどの `QObject::tr`（関数だけの名前空間の文言。指摘の2か所だけ直した）

---

## 5. 書き直し不要と判断した部分

- **core**: `Position` と駒の符号化（`shogitypes`・`piecerules`）、`UsiMove`・`UsiPosition` の値型（parse の重複を除く）、fmv（合法手生成）と `EngineMoveValidator`、`ShogiClock`（表示用の文字列関数を除く）、`JishogiCalculator`。
- **kifu**
  - `KifuBranchTree`・`KifuBranchNode` の所有と型付きの変更通知、`KifuNavigationState`（ノード ID だけを持つ）。
  - `KifuDocument` の置き換えと未保存の判定、`LiveGameSession`（`m_entries` を除く）。
  - `KifLineBuilder`・`ParseReport`、`KifTextParser` による KIF と KI2 の共有。
  - `KifuExport` の形式ごとの表、`KifuNodeReplay`、KI2・JKF・CSA の書き出し本体。
  - `JosekiBook` の詰めた表と `JosekiBookFile`、`JosekiMovePicker`、`JosekiLines` の展開（文字列の API を除く）。
  - 対応表のファイル（`TerminalTypes`・`TerminalCodes`・`Handicaps`・`ShogiNotation`）、`kifuformat` の判定、`KifuSaveFile`。
- **engine**: `UsiEngineSession`（非同期の起動失敗・`whenReady`）と `EngineProcessManager`、`UsiProtocolHandler` の状態の enum と通番、`UsiLineParser`、`UsiGoTimeCalculator`・`ClockTimeSource`、`UsiMatchHandler` の先読みの当たり外れと定跡手（`Pending`）。
- **game**: `MatchCoordinator` の一手の確定・終局・待った・中断と再開、`SennichiteDetector`、`GameChannel` による対局中の操作の振り分け。
- **analysis**: `MateOracle`・`MateQueryLoop`・`TsumeCandidateJob`・`TsumeVerificationJob`・`TsumeshogiGenerator`、`AnalysisJob`・`PlyAnalysisCollector`（読み筋の整形を除く）、`TsumeCollectionSession` の判定の待ち行列と `TsumeProgressStore`。
- **network**: `CsaGameProgress`（値型）と、`CsaGameCoordinator` の裁定待ち・時間の出どころ。
- **app・ui**
  - 合成ルートの生成の順と Deps の組み立て、`DockRegistry`、`DockLayoutManager` の本体。
  - `UiStatePolicyManager` の表と static_assert（R1 は、操作の入口でもこの表を引くという話）。
  - `KifuNavigationController`・`KifuDisplayPresenter`・`BoardSyncPresenter`・`CommentCoordinator`。
  - `GameSession` の対局の棋譜（`MatchRecordSink`・`GameChannel`・中断と再開）、`CsaGameSession`。
  - `KifuFileController::finishLoad`（`Source` の enum）、`KifuLoadCoordinator`。
  - `EvaluationGraphPresenter`、`EngineDisplayPresenter` の本体、`ConsiderationUiController`、`PositionEditSession` の編集メニューと着手。
- **widgets・views・models・style**
  - `BranchTreeView`・`BranchNodeItem`・`BranchTreeLayout`（ノード ID だけでやり取りし、末尾の1手は描き足すだけ）。
  - `BoardGeometry`・`PlayerCard`・`BoardChromeWidget`・`BoardStaticLayer`・`PiecePainter`、`ShogiViewHighlighting` の層の表、`ShogiViewInteraction`。
  - `EngineSelector`・`PlayerSetupWidget`・`TimeControlEditor`・`SettingsFormBinding`・`EditActions`・`RecentFilesMenu`、`FontSizeHelper`・`FontSizeControl`・`DialogFontScale`。
  - `EvaluationSeries`・`KifuRecordListModel`・`KifuBranchListModel`・`GameInfoEditModel` の履歴部分。
  - `PvTableView`・`ThinkingTableView`・`ThinkingPanel`・`EngineInfoWidget`・`BoardViewerWidget`、style/ 全体。
- **dialogs**
  - `JosekiTableView`・`JosekiTableModel`（列と操作が enum、ボタンはデリゲートで描く、行は差分で挿入）、`JosekiEditController`・`JosekiFileController`（ドックを閉じるときの確認を除く）、`JosekiMoveDialog`・`JosekiMoveInputWidget`、定跡の条件ダイアログ4つ、`JosekiImportDialog`。
  - `StartGameDialog`・`TsumePlayDialog`・`PvLine`・`BoardColorDialog`・`ChangeEngineSettingsDialog`/`EngineSettingsOptionHandler`・`MenuWindow`・`CsaWaitingDialog`・`JishogiScoreDialog`・`KifuPasteDialog`・`SfenCollectionDialog`・`TsumeCollectionDialog` と小さなダイアログ。
  - 層の依存: dialogs から ui・app を include している箇所は0件。
- **services・automation・cli・mcp**
  - `ParamSpec` の検査とスキーマ出力、`CliCommandSpecs::parse`、`ErrorKind` の表（`AutomationErrorCode` の名残を除く）。
  - `AutomationInteractions` の記録、`AutomationWidgetAdapters` の表、`AutomationInput`、`AutomationServer`・`AutomationDispatcher`。
  - `setting.h` の土台と大半の1行宣言、`EngineProfileStore`、`PieceStyleCatalog`・`BoardAppearanceCatalog`、`NotationStyle`・`ApplicationTranslations`。
  - CLI の Reporter と `CliStopWatcher`、MCP の `app_client.py`・`paths.py`・`environment.py` と `jobs.py` の単体ジョブの部分。

---

## 6. 進め方

1. **不具合 B1〜B17 を先に個別に直し、それぞれテストを足す。** 多くは小さな修正で済む。
   - B1: 分の正規表現を `{1,}` 桁にする（R5 で型の値にするまでのつなぎ）。
   - B2: 結果行の「先手の」「後手の」を読み、終局を記録した側に使う。
   - B3: CLI の変換で対局情報を作らない。
   - B4: 検討開始ボタンの許可から DuringPositionEdit を外す（R1 までのつなぎ）。
   - B5: 終了・新規・開くの確認で、コメント欄の未確定の編集も数える（R6 までのつなぎ）。
   - B6: オプションの解析が成功してから一覧に足す。
   - B7: `removeMove` で印も消す（R2 で印ごと廃止するまでのつなぎ）。
   - B8: 「破棄」でセッションを読み直すか空にする。仕様の判断が要る。
   - B9: 自動読込を待っていないときは、前回のファイルの記録を上書きしない。
   - B10: 通信対局の開始も表示を消す（R7 までのつなぎ）。
   - B11: モードレスのダイアログの決定ボタンを、待機中以外は無効にする（R1 までのつなぎ）。
   - B12: エンジン登録ダイアログを閉じた後、`EngineSelector` を持つ部品に再読み込みを知らせる。
   - B13: `convert-kifu` も `SfenValidationService` で局面を確かめる（R4 までのつなぎ）。
   - B14・B15・B16・B17: それぞれ分類・名前の検査・終局の手順・大きさの保存を直す。
2. **R1（動作状態の持ち主）を最初に進める。** B4・B11 の根本で、R6・R7 の土台になる。
3. **R5（棋譜の字句解析）と R4 の段階1を一緒に進める。** 変換器のテストが揃っていて、CLI で再現した不具合の根本を消せる。
4. **R2（定跡）は、定跡の機能を足す前に進める。** 新しいコードなので、今のうちに正本をセッションへ移す方が安い。マージの即時保存を残すかを先に決める。
5. **R3（表記）は R4 の段階1の後に進める。** 局面を `Position` で受け取る入口があると、型付きの `KifuPresentation` を作りやすい。
6. **R6・R7・R8・R9・R10 は、ほかと独立していつでも進められる。** R8 の探索の切り替えは、エンジンのテストの書き換えが多いので1つのコミットにまとめる。
