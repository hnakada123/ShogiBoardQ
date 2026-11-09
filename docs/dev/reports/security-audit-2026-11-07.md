# セキュリティ監査（2026-11-07）

「セキュリティ監査」の依頼で、`src/`（約8.8万行）・`mcp/`・ビルドと CI と配布・同梱エンジン Hayanagi の入口を監査した。

- 対象のコミットは 85c88928。本文の行番号はこの時点のもの。
- 前回の監査 `docs/dev/reports/security-audit-2026-10-23.md`（25c34973）から257コミット。重点は次の順。
  1. 前回の修正（S1〜S21）が今も効いているか
  2. 前回より後に足された・書き換えられたコード（特に `src/network/`）
  3. 前回見ていない観点
- 想定した攻撃者
  - A: 細工した棋譜・定跡・局面集の作者
  - B: 悪意のある CSA サーバー、または平文の CSA 通信の中間者（平文が既定）
  - C: プロンプトインジェクションで操られた AI エージェント（MCP の制限モード）
  - D: 同じマシンの別ユーザー
  - E: ビルド・配布の供給網
  - 利用者が登録した USI エンジンは信頼する側に置いた
- 「確認」の欄の意味
  - 「再現＋」: 区画の担当が実行で再現し、主担当がもう一度実行して同じ結果を確かめた
  - 「再現」: 区画の担当が実行で再現した
  - 「コード」: 経路を読んで確かめた
  - 「疑い」: 実行もしておらず、経路も確かめきれていない
- 使った手段
  - Release 版と ASan+UBSan 版（CLI・GUI）を HEAD で作り直して実行
  - ループバックの偽 CSA サーバー、Xvfb 上のアプリと自動化 API
  - プロセス内の変異ファズ（6ワーカー40分、完了した入力だけで約40万件以上。KIF・KI2・CSA・JKF・USI・USEN・SFEN・局面図・定跡・局面集・Game_Summary・USI の受信行と、書き出しの読み直し）
- 監査ではソースを変更していない。

> **修正の進み具合**: 2026-11-09 に V1〜V35 を修正し、main へ push した（3e4ef04e〜581618cf）。各項目の内容と確かめた範囲は「7. 修正の記録」にある。Info は、そこに書いたものを除いて未修正である。
>
> **この文書の置き場所**: リポジトリは公開で、`docs/` は GitHub Pages で公開される。そのため、直すまでは git 管理外に置き、修正を push してから `docs/dev/` に移した。公開にあたり、手元のパスと実験の記録を除いた。

## 全体の評価

**High はなく、任意コード実行に届く経路は確認していない。前回の修正に退行はなかった。** ただし、修正が同じ操作の別の入口に届いていない箇所が8つあり（S2・S4/S12・S8・S9・S13・S18・S20・S21）、新しい問題の型が2つ見つかった。

1. **CSA の状態機械が、モーダルのダイアログの入れ子のイベントループを考えていない。** 今回いちばん重い問題である。
   - サーバーの1行ごとにモーダルが入れ子になり、接続しただけで落ちる（V1）。
   - モーダルをまたいで持つポインタが、次の対局の開始で解放される（V2）。
   - 前回の「再入は問題なし」は自動化の要求の処理中だけを見たもので、この経路を含んでいなかった。
2. **S5・S6 の計算量の修正が CLI の読み込みまでで、GUI のメインスレッドの後段に届いていない。**
   - 読み込みは別スレッドで速くなったが、結果を表示する部品が手数・変化・コメントの長さの二乗で動く（V4〜V6）。
   - 100KB の棋譜で GUI が10〜20秒止まる。

MCP の閉じ込めは、パスの検査そのものは試した全部の手口で破れなかった。残っているのは、既定の許可ディレクトリがホーム全体のままであることから来る穴である（V7・V8）。

メモリ安全性は、変異ファズと狙った入力では符号付き整数のオーバーフローしか出なかった。解放後の使用（V2）だけが例外で、これはパーサではなく GUI の流れの問題である。

---

## 1. 指摘一覧

| # | 深刻度 | 指摘 | 攻撃者 | 確認 | 前回との関係 |
|---|---|---|---|---|---|
| V1 | **Medium** | サーバーの1行ごとにモーダルが入れ子になり、スタックを使い切って落ちる（ログイン前に届く） | B | 再現＋ | 新規 |
| V2 | **Medium** | モーダルをまたいで持つ `KifuBranchNode*` の解放後の使用 | B・C | 再現＋ | 新規 |
| V3 | **Medium** | CSA の手数に上限がなく、210KB の対局条件で GUI が5分止まる | B | 再現 | 新規（S6 と同型） |
| V4 | **Medium** | 評価値グラフの X 軸が手数の二乗で GUI を止める | A・C | 再現＋ | S6 の GUI 側 |
| V5 | **Medium** | 長いコメントの表示で GUI が止まり、数 GB を使い、落ちる | A・C | 再現＋ | 新規 |
| V6 | **Medium** | 分岐ツリーの段の割り当てが変化の数の二乗 | A・C | 再現＋ | S6 の GUI 側 |
| V7 | **Medium** | 制限モードで棋譜でない `.json` の先頭80文字を読める。`AppData`・`Library` にも届く | C | 再現＋（Windows・macOS は未実行） | S2 の修正漏れ |
| V8 | **Medium** | 制限モードで利用者の画像を上書きできる | C | 再現＋ | S1 の残り |
| V9 | **Medium** | ヘッドレスのスクリプトの Xvfb に認証がなく、別ユーザーが画面を読み、入力を送れる | D | 再現 | 新規 |
| V10 | **Medium** | AppImage に埋め込むランタイムを `continuous` から検証なしで取得している | E | 確認済（ログと実行ファイル） | S18 の修正漏れ |
| V11 | Low | 求めていない `LOGOUT:` で接続が残り、後からサーバーが対局を始められる。棋譜が確認なしに置き換わる | B | 再現＋ | 新規 |
| V12 | Low | 確認ダイアログと dialogs の静的 `QMessageBox` に外部の文字列が自動判定のまま入る | A | 再現＋（画像とコード） | S4・S12 の修正漏れ |
| V13 | Low | CSA の `$TIME_LIMIT` に改行が残り、保存した棋譜の開始局面と手順が変わる | A | 再現＋ | S9 の修正漏れ |
| V14 | Low | 対局情報のキーが KIF・KI2 で別の行になる | A | 再現＋ | S9 の別の入口 |
| V15 | Low | 通信ログの上限が行数だけで、最大約 3.8GB まで増える | B | 再現 | 新規 |
| V16 | Low | 自動保存のファイル名が全角の名前で 255 バイトを超えて保存に失敗する | B | 再現 | S21 の修正漏れ |
| V17 | Low | 対局数「無制限」のとき、同意なしの START で局を高速に回せる | B | 再現 | 新規 |
| V18 | Low | 読み込めるのに `fromSfen` が拒む局面へ移ると、自動化 API が止まったままになる | A・C | 再現＋ | S13 が作った不整合 |
| V19 | Low | 64MiB の上限の中でメモリが数 GB になり abort する | A | 再現 | S14 の限界 |
| V20 | Low | 大量の警告を1つの `QMessageBox` に出して固まる | A | 再現 | 新規 |
| V21 | Low | 対局者名と独自の終局語の長さに表示の上限がない | A | 再現 | 新規 |
| V22 | Low | 符号付き整数のオーバーフローの残り | A | 再現＋（ファズ） | S13 の修正漏れ |
| V23 | Low | 定跡のマージが同じ局面の登録数の二乗 | A | 再現 | S6 の別の入口 |
| V24 | Low | KIF の書き出しが兄弟の数の二乗 | A | 再現 | 新規 |
| V25 | Low | 局面集の読み込みがメインスレッドで、上限 256MiB まで | A | 再現 | S14 の範囲 |
| V26 | Low | 非制限モードで USI コンソールから選んだ場所にファイルを書ける | C（非制限） | 再現 | S8 の修正漏れ |
| V27 | Low | Windows で UNC パスを検査の前に開く | C | コード＋判定の単体実行 | 新規 |
| V28 | Low | `python3 -m` が現在のディレクトリのコードを先に読み込む | 細工したリポジトリ | 再現 | 新規 |
| V29 | Low | `max_chars` が値の長さに効かない | A・C | 再現 | S20 の修正漏れ |
| V30 | Low | 上書きするツールの注釈が `destructiveHint=false` | C | 確認済 | 新規 |
| V31 | Low | 外部の文字列が、信頼できないデータである印なしでエージェントに届く | A→C | 確認済 | 新規 |
| V32 | Low | 取り消しで CLI を強制終了すると、EOF を無視するエンジンが残る | C | 再現 | S20 の範囲外 |
| V33 | Low | Windows の名前付きパイプの先取り | D | 疑い | 前回 Info の残り |
| V34 | Low | 非公開の脆弱性報告が無効なのに、テンプレートが Security Advisories へ案内している | ― | 確認済 | 新規 |
| V35 | Low | `.claude/settings.local.json` が追跡され、広い実行許可が公開されている | 細工した PR | 確認済 | 新規 |

Info は3章にまとめる。

---

## 2. 詳細

### V1【Medium】受信行ごとにモーダルが入れ子になり、スタックを使い切って落ちる

- **場所**
  - `src/network/csagamecoordinator.cpp:322-328`（`onLoginFailed`。切断より先に `errorOccurred` を出す）、`:294-302`（`onClientError`）
  - `src/network/csaclient.cpp:495-498`（`LOGIN:incorrect`）、`:462-470`（不正な `REJECT:`）、`:54-55`（readyRead はキュー接続）
  - `src/app/mainwindowcomposer_wire.cpp:177`
  - `src/ui/presenters/notificationpresenter.cpp:78-81, 102`（`critical()` は `exec()` で待つ）
- **流れ**
  1. サーバーが `LOGIN:incorrect` を送ると、`onLoginFailed` が先に `errorOccurred` を出し、`QMessageBox::exec()` の入れ子のイベントループに入る。
  2. この間、段階は変わらず接続も残る。
  3. 次の行が届くと `onReadyRead` が入れ子の中で動き、また `exec()` に入る。
  4. 1行で1段ずつ深くなり、スタックを使い切る。
  - `GameReady` では不正な `REJECT:<x>` の行でも同じ経路を通る（60行でウィンドウ62個まで確認）。
  - ログイン前に届くので、利用者は接続するだけで影響を受ける。
- **確認**: 実行で再現＋。
  - 担当: 2953窓で VmStk 6696 kB・RSS 1156MB、3634行以内に SIGSEGV（約90秒）。
  - 主担当: 3762行で SIGSEGV（rc=139、約95秒）。
  - Windows のメインスレッドのスタックは 1MB なので、もっと少ない段で落ちると考えられる（疑い）。
- **修正案**
  1. `onReadyRead` に再入の防止を入れる（処理中の呼び出しはすぐ戻り、外側のループが続きを読む）。
  2. `onLoginFailed` は段階の変更と切断を先に行ってから知らせる。
  3. 通信対局のエラーは `Wait::No` で1つに差し替える（結果のダイアログの `replaceInformation` と同じ）。
  4. 不正な `REJECT:` はログだけにする。

### V2【Medium】モーダルをまたいで持つ `KifuBranchNode*` の解放後の使用

- **場所**
  - `src/ui/controllers/commentcontroller.cpp:82`（node の取得）、`:94`（`dialog.exec()`）、`:97`（`setBookmark`）
  - `src/kifu/kifudocument.cpp:234-236`（解放済みのノードを読み、値が違えば書く）
  - `src/ui/controllers/kifunavigationcontroller.cpp:143-146`（`selectRecordRow`）、`:152-163`（`selectBranchCandidateRow`）、`:169-171`（`selectBranchTreeNode`）
  - `src/ui/controllers/branchtreeeditcontroller.cpp:127-146`（`apply`）
  - `src/app/mainwindowcomposer_create.cpp:242-243`（`canBrowseRecord` は確認の問いの前にしか見ない）
  - 入口の側: `src/automation/automationcommands_kifu.cpp:53-67`（`requireReplaceableRecord`）・`:190-213`（`kifu.goto`）・`automationcommands_position.cpp:29-45` はモーダルを確かめない
- **前提（CSA の経路）**: 1つの接続で複数の対局を続ける設定（既定は1局で、この場合は起きない）。利用者が、局が替わる瞬間にしおりの入力ダイアログか未更新のコメントの確認を開いている。
- **前提（MCP の経路）**: 制限モードの `load_kifu`・`set_position` で届く。エージェントが自分でモーダルを開く手段はないので、利用者が開いている必要がある。非制限モードならエージェントだけで起こせる。
- **流れ（しおり）**
  1. 利用者がしおりのボタンを押す。`node` を持ったまま `QInputDialog::exec()` に入る。ボタンは対局中も押せる。
  2. サーバーが `#TIME_UP`・`#WIN`、次の Game_Summary、`START` を送る。入れ子のループの中で `SessionResetCoordinator::prepareGameStart` → `KifuDocument::startNew` → `KifuBranchTree::clear` が走り、ノードが解放される。
  3. 利用者が OK を押す。解放済みの `node` を読み、値が違えば書く。
- **流れ（移動）**: 局と局の間でコメントが未更新のまま棋譜の行を押す。確認の間に `START` が届く。「破棄」を押すと、確認の前に取った `path.at(row)` で `setCurrentNode` する。この時点の状態は対局中だが `canBrowseRecord` はもう見ない。
- **確認**: 実行で再現＋。主担当も隔離した環境の ASan 版で両方を再現した。
  - `heap-use-after-free READ of size 8`。`KifuBranchNode::bookmark()` ← `KifuDocument::setBookmark` ← `CommentController::onBookmarkEditRequested`
  - `heap-use-after-free READ of size 8`。`KifuBranchNode::parent()` ← `KifuNavigationState::rememberPath` ← `selectRecordRow`
  - 解放のスタックは `CsaGameCoordinator::onGameStarted` → `GameSession::beginRecord` → `KifuDocument::startNew`。
  - 通常版は移動の経路で7回中3回 SIGSEGV。しおりの経路（1回）は落ちなかった。
  - `selectBranchCandidateRow` は自動化の経路で再現。`selectBranchTreeNode`・`BranchTreeEditController::apply` はコードで確認（右クリックのメニューを自動化から出せない）。
- **影響**: 確認できたのはクラッシュと解放後の読み出し。解放後の書き込みの経路はコード上ある（`kifudocument.cpp:235`）が、ASan は読み出しで止まるので実行では確かめていない。任意コード実行に届くかは確かめていない。ローカルのエンジン同士の連続対局でも同じことが起きるはずである（疑い）。
- **修正案**
  - モーダルをまたぐときはポインタでなくノード ID を持ち、戻ってから `tree->nodeAt(id)` で引き直す。ID は `clear()` で戻らないので（`kifubranchtree.cpp:44`）、置き換え後は nullptr になる。
  - `confirmLeave()` の後に `path`・`candidates` を取り直し、`canBrowseRecord` も確かめ直す。
  - 定跡の側は同じ問題を写しで避けている（`josekieditcontroller.cpp:27`）。棋譜の側にも同じ規則を置く。
  - 自動化の3つの入口は `QApplication::activeModalWidget()` があれば `invalid_state` にする（`docs/dev/mcp-server.md` 2章の「モーダルで遮られた対象を拒否」と合わせる）。

### V3【Medium】CSA の手数に上限がなく、210KB の対局条件で GUI が5分止まる

- **場所**
  - `src/network/csagamesummaryreader.cpp:125-141`（再開局面の手）、`:174`（`Max_Moves` は読むだけ）
  - `src/network/csagameprogress.cpp:74-105`、`:107-118`（`initialMoveTime` は手ごとに先頭から足す）
  - `src/network/csagamecoordinator.cpp:352-354`、`csagamecoordinator_moves.cpp:255-271`
  - `src/network/csaclient.cpp:268-293`（受信を一度に処理する）
  - 1手ごとの O(手数): `src/kifu/kifunavigationstate.cpp:105-110`、`src/widgets/branchtreeview.cpp:425`、`src/models/kifurecordlistmodel.cpp:38`
- **流れ**: 20000行の上限の中に合法な玉の往復を入れた Game_Summary と `START:` を送ると、全手の追記が GUI スレッドの1回の呼び出しで走る。その間はキャンセルできない。
- **確認**: 実行で再現。
  - 19000手（209,569バイト）で **309.6秒、GUI が完全に停止**。5000手で 44.8秒。
  - 対局中も1手の処理が 3.98ms（1000手）から 13.49ms（6000手）に増える。メモリは1手あたり約 8.3KB で、10バイトの行の約800倍。
- **修正案**
  1. 再開局面の手数に上限を置き、超えたら REJECT する。
  2. 対局中は `Max_Moves` と絶対の上限を見る。
  3. `onReadyRead` は一定の行数ごとにイベントループへ返す。
  4. `initialMoveTime` を O(n) にし、1手ごとの処理を O(1) にする。

### V4【Medium】評価値グラフの X 軸が手数の二乗で GUI を止める

GUI と パーサの担当が別々に見つけた。

- **場所**: `src/widgets/evaluationchartwidget.cpp:224-231`（`applyXAxis`。同じ並びが `:76-77`）。呼び出しは `src/ui/presenters/evaluationgraphpresenter` の `showCurrentPly`。
- **流れ**
  1. 手数の多い棋譜を開く（形式は問わない）。
  2. `applyXAxis` が `setRange(0, xLimit)` を先に、`setTickInterval` を後に呼ぶ。
  3. Qt Charts は古い間隔（既定 5）のまま、手数 / 5 個の目盛りの部品を作る。`CartesianChartAxis::createItems` の中の `childItems()` で時間が二乗になる。
  4. グラフのドックが隠れていても起きる。
- **確認**: 実行で再現＋（USI は1手5バイト）。

  | 手数 | ファイル | GUI が止まった時間 |
  |---|---|---|
  | 1万手 | 50KB | 2.8秒（主担当）〜4.3秒 |
  | 2万手 | 100KB | 10.0秒（主担当）〜19.9秒 |
  | 4万手 | 200KB | 80秒超 |
  | 7万手 | 350KB | 約208秒（RSS 1.49GB。同じ入力が CLI なら 0.9秒） |

  - `load_kifu` は60秒で時間切れを返すが、GUI は止まったままになる。
  - 原因の判断はスタックのサンプルとコードの順序からで、順序を直した版では試していない。
- **修正案**: 広げるときは先に間隔、狭めるときは先に範囲を変える。目盛りの数に上限を置く。

### V5【Medium】長いコメントの表示で GUI が止まり、数 GB を使い、落ちる

GUI・MCP・パーサの3つの担当が別々に見つけた。原因は2つで、どちらもコメント欄に長さの上限がないことから来る。

- **場所**: `src/widgets/commenteditorpanel.cpp:44, 94, 107, 173-195`（`setHtml`）、`src/ui/controllers/commentcontroller.cpp:35-36`
- **流れ**: 開始局面のコメントなら棋譜を開くだけで、手のコメントならその手へ移ったときに起きる。ドックがタブの裏でも起きる。
- **空白のない1語（`QTextLine::layout_helper` が1行ごとに走査し直す）**: 実行で再現＋

  | 長さ | 1語 | 空白あり |
  |---|---|---|
  | 32KB | 1.27秒 | 0.05秒 |
  | 64KB | 4.35秒 | 0.09秒 |
  | 128KB | 16.5秒 | 0.18秒 |
  | 200KB | 67.9秒 | 0.24秒 |

  上は主担当の実測（200KB だけ MCP の担当）。2MiB は180秒超で打ち切り、32MiB は CPU 450秒超・RSS 2.9GB で打ち切り。ハーネスでは `QTextOption::WrapAnywhere` で 128KB が 78ms、`NoWrap` で 36ms だった。
- **行の多いコメント（改行を `<br/>` にして `setHtml` へ渡す）**: 実行で再現。
  - 3MB で 20秒・RSS 2.48GB。
  - 24MB でアドレス空間 8GB の制限下で `std::bad_alloc` の未捕捉で abort。
- **修正案**: 表示するコメントの長さと行数に上限を置き、超えた分は省略の表示にする（全文は編集のときだけ読み込む）。折り返しは `WrapAnywhere` にする。行数が多いときは `setHtml` を使わず平文で出す。

### V6【Medium】分岐ツリーの段の割り当てが変化の数の二乗

- **場所**: `src/widgets/branchtreelayout.cpp:147-150`（`assignLanes`。既定で有効）。`KifuBranchTree::reset` の中でメインスレッドで動く。
- **流れ**: 同じ手数に変化を多数置くと、変化ごとに新しい段が要り、各変化が既存の全段を調べる。USEN なら変化1つが6〜7バイト。
- **確認**: 実行で再現＋。
  - `usen_var100k.usen`（700KB）: 20.5秒（主担当 13.5秒）。
  - `var100k.kif`（3.4MB）: 36.8〜75.8秒。
  - 100万個（7MB）は未測定で、二乗から1時間規模と推定。
- **修正案**: 手数の区間で空いている段を引ける構造にする。または変化の数が多いときは詰めた配置をやめる。

### V7【Medium】制限モードで棋譜でない `.json` の先頭80文字を読める

- **場所**
  - `mcp/shogiboardq_mcp/paths.py:27-31`（既定はホーム全体）
  - `src/kifu/formats/usitosfenconverter.cpp:82`（`content.left(80)` をエラー文に入れる）
  - `mcp/shogiboardq_mcp/handlers_cli.py:53-54`（`.json` にも `input_format` を指定できる）
- **前提**: `SHOGIBOARDQ_ALLOWED_DIRS` が未設定（既定）。読む先がドットで始まらない場所にあり、名前が分かる。
- **流れ**: `convert_kifu(input_path=<x.json>, input_format="usi", output_format="kif")` を呼ぶと、`parse_error` の本文に最初の空でない行の先頭80文字が入る。
- **確認**: 実行で再現＋（偽の値）。
  - 1行の JSON: `Unknown USI position format: {"api_key":"sk-FAKE-minified-EEEE5555"}`
  - 整形された複数行の JSON は `{` だけ。
  - `AppData/Roaming/Claude/claude_desktop_config.json`、`AppData/Roaming/gcloud/application_default_credentials.json`、`Library/Application Support/Claude/...` と同じ名前の偽ファイルは、Linux 上ですべて検査を通った。Windows の判定を単体で動かしても許可された。実機の Windows・macOS では動かしていない。
  - `not_found` と `parse_error` の違いからファイルの有無も分かる。
- **修正案**（上から優先）
  1. 既定の許可ディレクトリを専用のフォルダにする（S1 の修正案1で未実施）。
  2. `.json` は `auto`・`jkf` のときだけ読む。
  3. エラー文に入力の中身を入れない（行番号だけにする）。

### V8【Medium】制限モードで利用者の画像を上書きできる

- **場所**: `mcp/shogiboardq_mcp/handlers_cli.py:159-162`、`paths.py:106-122`（`IMAGE_WRITE` は png・jpg・jpeg・bmp・webp）
- **流れ**: `render_board_image(output_path=~/Pictures/family.jpg, overwrite=true)`。`overwrite` はエージェントが渡す引数で、運用者の許可ではない。`file_exists` の応答で有無も確かめられる。
- **確認**: 実行で再現＋。既存の `family.jpg` が 720x566 の盤面の JPEG に置き換わった。`overwrite` なしは拒否された。書ける拡張子のファイルが実行される経路は見つからなかった。
- **修正案**: 画像の上書きは、そのサーバーが作ったファイルだけ、または専用の出力フォルダの中だけにする。運用者の環境変数で上書きを許す形にする。V7 の修正で影響も狭まる。

### V9【Medium】ヘッドレスのスクリプトの Xvfb に認証がない

- **場所**: `scripts/mcp/shogiboardq-mcp-headless.sh:55`（`-auth` がない）
- **前提**: 同じマシンに別ユーザーがいて、利用者がこのスクリプトを使っている。
- **流れ**: Xvfb は認証なしで起動し、`/tmp/.X11-unix/X<n>` は 0777 なので、別の UID も接続できる。XTEST で入力を送れるので、アプリを直接操作できる。MCP の制限モードも許可ディレクトリも効かない。
- **確認**: 実行で再現（主担当はログとコードを確認）。`docker --user 54321 --network none` の別 UID から接続し、GetImage で画面を読み、XTEST のクリックと Ctrl+O で「棋譜ファイルを開く」のダイアログを開いた。
  - 確かめたのはダイアログを開くところまで。GUI ができることはすべてできるはずで、エンジン登録を経由すれば被害者の権限でプログラムを実行できると考えられる（未実行）。
- **修正案**: `mcookie` で cookie を作り `-auth` を渡す。担当が手順を試し、cookie なしは同じ UID も別 UID も拒否されることを確かめた。
  ```
  xauth -f F add :0 . $(mcookie)
  Xvfb -displayfd 9 -auth F ...
  # 表示番号が分かったら
  xauth -f F add :N . <cookie>
  export XAUTHORITY=F
  ```

### V10【Medium】AppImage に埋め込むランタイムを検証なしで取得している

- **場所**: `scripts/build-linux.sh`（`"$APPIMAGETOOL" "$APPDIR" "$APPIMAGE_NAME"`。`--runtime-file` を渡さない）
- **内容**
  - S18 の修正は `appimagetool` 自体を SHA-256 で固定した。
  - しかし `appimagetool` は `--runtime-file`（または `APPIMAGETOOL_RUNTIME_FILE`）がなければ、`https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64` を毎回取得して AppImage の先頭に埋め込む。
  - このランタイムは、配布物を起動したときに最初に動くコードである。
- **確認**
  - `appimagetool` の実行ファイルの文字列に取得先がある。
  - 2026-11-04 のリリースのビルドログに `Downloading runtime file from …continuous/runtime-x86_64` がある。
  - 今の配布物に埋め込まれたランタイムの SHA-256 は `0c499a076941e49fb2f78b2cf3454473dd1be3b21e2e32f0ac2c210cbd214f0e`（944,632バイト）。
- **深刻度**: 上流の侵害が前提で、影響は Linux の全利用者のコード実行。S18 は Low としたが、S7 と同じ「上流の侵害で利用者か成果物に届く」型なので Medium にした。
- **修正案**: ランタイムをリポジトリ外のキャッシュに固定した版で取得し、SHA-256 を確かめて `--runtime-file` で渡す。

### V11【Low】求めていない `LOGOUT:` で接続が残り、後からサーバーが対局を始められる

- **場所**: `src/network/csaclient.cpp:325-334`（`handleLogoutResponse` は `m_logoutPending` のときだけ閉じる）、`:503-509`、`src/network/csagamecoordinator.cpp:330-334`（Phase::Off にする）、`:336-356`（`onGameStarted` は段階を見ない）、`csagamecoordinator_setup.cpp:70-115`
- **流れ**: ログイン直後にサーバーが `LOGOUT:completed` を送ると、画面は待機に戻るがソケットは残る。後から Game_Summary と `START:` を送ると、利用者の操作なしに AGREE が送られ対局が始まる。`confirmDiscardUnsaved` を通らない。
- **確認**: 実行で再現＋。開いていた棋譜が置き換わり（`kifu_file` が空、手数 3→1）、`ui_state` が `csa_game` になった。進行中のローカル対局も置き換わる。「対局キャンセル」の後の LOGOUT の応答待ち（最大3秒）でも同じ。
- **修正案**: `LOGOUT:` を受けたら常に閉じる。`m_logoutPending` の間は Game_Summary を読まない。司令塔は Phase::Off・Failed で届いた対局条件を REJECT し、START を無視する。

### V12【Low】確認ダイアログと dialogs の静的 `QMessageBox` に外部の文字列が自動判定のまま入る

| 部品 | 入る値 | 場所 |
|---|---|---|
| `DialogUtils::confirmAction` | 独自の終局語 | `src/widgets/dialogutils.cpp:124-132`、`src/ui/controllers/branchtreeeditcontroller.cpp:116-123` |
| 同上 | 定跡の読めない手の語 | `src/ui/controllers/josekieditcontroller.cpp:71-74`、`src/kifu/kifupresentation.cpp:55-60` |
| `UnsavedChangesPrompt` の `setInformativeText` | 定跡のファイル名 | `src/widgets/unsavedchangesprompt.cpp:12-14`、`src/ui/controllers/josekifilecontroller.cpp:155-159` |
| `QMessageBox::warning` の静的関数 | 局面集のファイル名・パス | `src/dialogs/sfencollectiondialog.cpp:173, 181, 227`、`src/dialogs/tsumecollectiondialog.cpp:296` |

- **流れ**: `Qt::mightBeRichText` は1行目のどこかにタグがあれば真になるので、前に文があっても防げない。独自の終局語は、細工した KIF を開いてその手で「この手以降を削除する…」を選ぶ。定跡は手の欄にタブ区切りの `<img	src=...>` を置いて「削除」する。ファイル名は `/` を `&#47;` で書ける（Windows は `<` `>` が使えないのでこの経路はない）。
- **確認**: 実行で再現＋（担当が Release 版で実行、主担当はコードと画像を確認）。
  - ローカルファイルの画像が「手順の削除」のダイアログに描かれた。棋譜欄と分岐ツリーは同じ文字列を文字のまま出している。
  - `<img src="/dev/zero">` で RSS 4.4GB、約7秒で `std::bad_alloc` により異常終了。未保存の棋譜は失われる。
  - FIFO を指すと GUI が止まる。
  - `<a href="file:///etc/passwd">` は押せるリンクになり、押すと偽の xdg-open に `file:///etc/passwd` が渡った（スキームの許可はない）。S4 で「確かめきれていない」とした点が確定した。KDE のテーマでは外部の起動を確認できなかった（原因は未調査）。
  - Windows の UNC パスの `<img>` は疑い。
- **修正案**: `confirmAction` と `UnsavedChangesPrompt` を `Qt::PlainText` にする（補足文にも効くことをテストで確かめる）。dialogs・widgets の静的 `QMessageBox` を PlainText の共通関数に置き換え、`tst_structural_kpi` の検査対象に広げる。エンジンの出力が入る部品（Info）も一緒に直す。

### V13【Low】CSA の `$TIME_LIMIT` に改行が残る

- **場所**: `src/kifu/formats/csaformatter.cpp:113`（`singleLine` を通さない）、`csaexporter.cpp:65`
- **流れ**: 持ち時間の値が既知の書式でないと `$TIME_LIMIT:<生の値>` と書く。KIF の値の `\n` は読み込みで本物の改行になり、JKF は `\r` を直接持てる。
- **確認**: 実行で再現＋。平手・7g7f 3c3d の棋譜を CSA に保存して読み直すと、開始局面が飛車落ち相当（`PI82HI22KA`）、手順は 2g2f だけになった。
- **修正案**: `:113` の値を `KifuExportCommon::singleLine` に通す。

### V14【Low】対局情報のキーが KIF・KI2 で別の行になる

- **場所**: `src/kifu/formats/kifexporter.cpp:198-204`（`appendHeaderLines`）
- **流れ**: JKF の header と CSA の `$キー:値` は任意のキーを持てる。KIF として保存すると、読み手が別の意味に解釈する行になる。
- **確認**: 実行で再現＋。

  | 入力 | 読み直した結果 |
  |---|---|
  | キー `1 ２六歩(27)` | 手順が 7g7f 3c3d から 2g2f だけに変わる |
  | `手合割：飛車落ち`（実際は平手） | 開始局面が飛車落ちになる |
  | `先手の持駒：飛` | 持ち駒に飛が増える |
  | `配置：` | 棋譜全体が読めなくなる |

- **修正案**: 手合割は開始局面から書き、見出しの値を使わない。指し手の行・局面図の行に読めるキーは書かない。

### V15【Low】通信ログの上限が行数だけで、最大約 3.8GB まで増える

- **場所**: `src/widgets/logpanel.cpp:84`（5000行）、`src/network/csaclient.cpp:287`、`csagamecoordinator.cpp:465-469`、`src/dialogs/csawaitingdialog.cpp:253-256`
- **確認**: 実行で再現。64KB の行がドックと待機ダイアログの2つのパネルに入り、4589行で 2843MB、最大 3825MB。約490MB の通信で届く。
- **修正案**: 表示する1行を切り詰める（例: 4KiB）か、パネルの上限を文字数でも持つ。

### V16【Low】自動保存のファイル名が 255 バイトを超える

- **場所**: `src/kifu/kifusavefile.cpp:18-19, 38-42`（上限は UTF-16 の40文字）、`src/ui/controllers/kifufilecontroller.cpp:303-309`
- **確認**: 実行で再現。Name+ に「あ」41字、Name- に「い」41字で「ファイル名が長すぎます」のダイアログが出て、保存されない。テストは ASCII だけ（`tests/tst_kifusavedialogs.cpp:339`）。
- **修正案**: 名前の部分を UTF-8 のバイト数で切る（例: 各100バイト）。

### V17【Low】同意なしの START で局を高速に回せる

- **前提**: 既定ではない「無制限」を選んでいる。
- **場所**: `src/network/csaclient.cpp:455-461`（AGREE を送る前の START も受ける）、`csagamecoordinator_setup.cpp:105-110`、`src/app/gamesession.cpp:125-126`、`src/engine/engineprocessmanager.cpp:163-190`
- **確認**: 実行で再現。人間: 17秒で648ファイル。エンジン: 16秒で自動保存454件・子プロセス180個。
- **修正案**: AGREE を送っていない START は無視して切断する。局と局の間に最短の間隔を置く。

### V18【Low】読み込めるのに `fromSfen` が拒む局面へ移ると、自動化 API が止まったままになる

- **場所**: `src/core/shogiposition.cpp:345`（手数を `kMaxMoveNumber` で止めない）、`:72`（読むときは超えた値を拒む）、`src/kifu/formats/kiflexer.cpp:374`（局面図の手数をちょうど上限に丸める）、`src/core/shogiboard.cpp:48-53`（読めない SFEN を `ErrorBus` へ流す）
- **流れ**: 「手数＝99999999999」の局面図は、1手指すと手数が 10000001 になる。`P*5a` のような緩い駒打ちでも読めない局面になる。その局面へ `kifu.goto` すると、`ErrorBus` → `NotificationPresenter::critical` のモーダルで止まる（原因はコードからの判断）。
- **確認**: 実行で再現＋。`kifu.goto` が応答せず、別の接続の `app.ping` も `-32002 Another request is still running` になる。API からは復旧できない。
- **修正案**: `apply` の手数も上限で止める。緩い駒打ちの結果を読める局面に保つか、読み込みで断る。自動化の処理中は `ScopedCapture` で受ける。

### V19【Low】64MiB の上限の中でメモリが数 GB になり abort する

- **場所**: `src/common/boundedfile.h`、`src/kifu/recordentry.h`
- **確認**: 実行で再現。USEN は1手3バイトで、1手あたり約 1.6KB を使う（約550倍。増え方は線形で S6 の再発ではない）。200万手（6MB）で 26秒・3.3GB、2000万手（60MB）で SIGABRT。
- **修正案**: 手数と変化の数に上限を置く。バイト数の上限だけでは足りない。

### V20【Low】大量の警告を1つの `QMessageBox` に出して固まる

- **場所**: `src/kifu/kifuloader.cpp:120`、`src/ui/presenters/notificationpresenter.cpp`
- **確認**: 実行で再現（貼り付けの経路）。持っていない駒を打つ手ごとに1行増える。100KB で 10.7秒、500KB で 51.6秒。箱の高さは 32767 ピクセルで OK ボタンが画面の外になる。
- **修正案**: 先頭の数十行と「ほか N 件」にする。

### V21【Low】対局者名と独自の終局語の長さに表示の上限がない

- **場所**: `src/widgets/elidelabel.cpp:33-52, 57-80`、`src/widgets/recordpane.cpp:316-326`
- **確認**: 実行で再現。対局者名 4MiB で 6.9秒止まり、ドックの切り替えに15秒超。独自の終局語 90文字でウィンドウの幅が 1300 から 1937 に広がる。1MiB で 32767 になる。次の起動で戻る。
- **修正案**: 表示に使う長さに上限を置く。`RecordPane` の最小幅に上限を置く。

### V22【Low】符号付き整数のオーバーフローの残り

リリース版では値が折り返すだけで、メモリの問題は観測していない。

| 場所 | 確認 |
|---|---|
| `src/kifu/josekibook.cpp:442`（マージの出現頻度の和） | 再現＋（主担当のファズと担当の入力で別々に検出） |
| `src/kifu/josekibook.cpp:380` | コード |
| `src/kifu/formats/kiflinebuilder.cpp:102`（時間の累計） | UBSan で再現 |
| `src/kifu/formats/csaformatter.cpp:136`（`$TIME_LIMIT`） | UBSan で再現 |
| `src/kifu/timecontroltext.cpp:43, 45`（持ち時間） | UBSan で再現 |

定跡の出現頻度の読み手は `qint64` の合計と `qMax(…, 0)` で受けるので、負の値でも問題は起きない（コードで確認）。

- **修正案**: 飽和する足し算と、読むときの上限。

### V23〜V25【Low】残りの計算量

- **V23 定跡のマージ**: `src/kifu/josekibook.cpp:428`（手ごとの `resolve`）。各 4.4MB で 27秒、各 8.9MB は200秒で打ち切り。ワーカースレッドなので GUI は止まらないが、取り消せない。範囲の一覧を局面ごとに1度だけ作る。
- **V24 KIF の書き出し**: `src/kifu/formats/kifexporter.cpp:66-69`（`hasNextSibling` が線形）。100万個の変化で200秒超（ほかの形式は14〜46秒）。親の子を順にたどるときに番号を渡す。
- **V25 局面集の読み込み**: `src/dialogs/sfencollectiondialog.cpp:165-201`、`src/dialogs/tsumecollectiondialog.cpp`、`src/analysis/tsumecollectionsession.cpp`。64MiB で約20秒・RSS 0.6〜1.2GB。上限の 256MiB なら約80秒（推定）。ワーカースレッドで読み、行数に上限を置く。

### V26【Low】非制限モードで USI コンソールから選んだ場所にファイルを書ける

- **場所**: `src/widgets/logpanel.cpp:171-172`、`src/widgets/usilogpanel.cpp:21-26`（印がない）、`src/ui/controllers/usicommandcontroller.cpp:27-50`
- **前提**: `SHOGIBOARDQ_ALLOW_UNRESTRICTED_UI=1`、任意のパスをオプションで受けるエンジン。
- **確認**: 実行で再現（YaneuraOu 9.10 の `DebugLogFile`）。ログの1行目を `>> x; touch …` にでき、その写しを `sh` で実行すると `touch` が動いた。シェルの起動ファイルには書いていない。エンジン設定ダイアログの文字欄はコードで確認。
- **修正案**: `automationSensitive` だけでは足りない（`widget.setValue` は機密の `QLineEdit` への書き込みを許す）。`submit` と送信ボタンを止めるか、書き込みも拒む印を設ける。文書に「非制限モードは任意のコード実行に等しい」と書く。`mcp/README.md:25` の「エンジン登録」も今の動作に合わせる。

### V27〜V33【Low】MCP と自動化の残り

- **V27 UNC パス**: `paths.py:49`。`path.resolve()` が許可の判定より先なので、Windows では `\\attacker\share\x.kif` が拒否される前に `_getfinalpathname` へ渡る。SMB 接続と NTLM の応答の漏洩の可能性がある。偽の `nt` モジュールで判定を動かして確かめた。実機は未確認。`resolve()` の前に字面で比べる。
- **V28 `python3 -m`**: `scripts/mcp/shogiboardq-mcp-headless.sh:96` と README・ガイドの設定例。`sys.path[0]` が現在のディレクトリになるので、開いたプロジェクトの `shogiboardq_mcp/` や `typing_extensions.py` が先に読み込まれる。実行で再現。Windows の `shutil.which`（`cli_backend.py:68`）も同じ型（コード）。スクリプトは起動前に `cd` し、文書の例は `cwd` の指定か `pip install` したコマンドにする。
- **V29 `max_chars`**: `handlers_cli.py:85-99`、`handlers_app.py:94-101`。`convert_kifu(max_chars=1000)` で `structuredContent` が 1,900,850 文字になった。値ごとに上限を置き、切ったことを示す。
- **V30 注釈**: `tooldefs.py:197, 387, 472`。`convert_kifu`・`render_board_image`・`save_kifu` は上書きするのに `destructiveHint=false`。クライアントの動作までは確かめていない。`destructive=True` にする。
- **V31 信頼できないデータの印**: `handlers_app.py:92-101`、`handlers_cli.py:122-126`、`server.py:79`。棋譜のコメント・対局情報が、印なしでツールの結果の本文に入る（`get_kifu` の本文に `IGNORE ALL PREVIOUS INSTRUCTIONS…` がそのまま入ることを確認）。文書にも注意がない。完全には防げないが、`INSTRUCTIONS` とツールの説明への注意書き、`openWorldHint=true`、区切りの印、文書の「専用フォルダを指定する」「書き込みのツールを自動承認しない」で緩和できる。
- **V32 エンジンの残り**: `cli_backend.py:148-151`（SIGKILL だけ）、`jobs.py:34-35, 257-267`。EOF を無視する偽エンジンが親が init に変わって残った。Hayanagi は終わる。CLI を新しいプロセスグループで起動してグループごと終わらせる。
- **V33 名前付きパイプ**: `src/headless/automationendpoint.cpp:17-18`（名前が予測できる）、`src/automation/automationserver.cpp:86-107`（検査がすべて `#ifndef Q_OS_WIN` の中）。Windows も Qt のソースもなく確かめていない。起動ごとに乱数を含む名前にしてエンドポイントファイルで渡す。

### V34【Low】非公開の脆弱性報告が無効

- **確認**: `gh api repos/hnakada123/ShogiBoardQ/private-vulnerability-reporting` は `{"enabled":false}`。
- **内容**: `.github/ISSUE_TEMPLATE/config.yml` と `security_report.yml` は Security Advisories へ案内するが、無効だと外部の人は報告を作れない。公開の Issue しか手段がない。`SECURITY.md` もない。
- **修正案**: Settings で Private vulnerability reporting を有効にする。`SECURITY.md` を足す。

### V35【Low】`.claude/settings.local.json` が追跡されている

- **内容**: 本来は手元だけの設定が git に入っている。許可に `Bash(bash:*)`・`Bash(python3:*)`・`Bash(git:*)`・`Bash(tee:*)` などがある。clone した人が Claude Code でこのフォルダを信頼すると適用され得る。細工した PR を手元で読ませるときに、プロンプトインジェクションからの実行が確認なしに通りやすくなる。`.qtcreator/CMakeLists.txt.user.*` も追跡されていて、手元のパスが見える（秘密はない）。
- **修正案**: `git rm --cached` して `.gitignore` に足す。

---

## 3. Info

**CSA・USI**

- パスワード欄がダイアログに残り、接続先とポートを手で変えて開始すると、前の接続先のパスワードが新しい接続先へ送られる（`csagameuicontroller.cpp:37-39`、`csagamedialog.cpp:185-199`。実行で再現）。終了後も `m_options.password` が残る（コード）。開始の後に欄を消し、接続先を変えたら消す。
- `BEGIN Position` の不正な局面がエンジンに渡る件は前回から変わらない（81マスに玉を並べた局面も送られる。Hayanagi は断る）。

**MCP**

- `SHOGIBOARDQ_ALLOWED_DIRS` の `""`・`" "`・`":"` はホーム全体に戻る。相対パスも受け付ける。設定されていて有効な値がないときは起動エラーにする。
- 文書の食い違い: `docs/dev/mcp-server.md:354` の「引数は inputSchema で検証され（SDK）」は S19 の直し漏れ。`.` で始まる名前を「資格情報の置き場所」とする説明は `AppData`・`Library` に当たらない。`docs/guide/mcp-server.html:470` の「明示的な許可が原則」は `overwrite`・`discard_unsaved` に合わない。
- `load_kifu(path="")` が `internal_error: KeyError: 'text'` になる（`minLength` がない）。
- `analyze_kifu`（最大 10,001局面 × 600秒）と `generate_tsume` に全体の時間の上限がない。どちらも取り消せる。
- 既定の出力先 `/tmp/shogiboardq-output-*` はサーバーが終わっても消えない（0700）。
- 依存（`mcp>=2,<3`、`jsonschema>=4`）にロックファイルとハッシュの固定がない。既知の脆弱性の照合はしていない。

**自動化 API**

- 読まないクライアントでアプリのメモリが増える（`automationserver.cpp:201` の `socket->write` に上限がない。接続数の上限もない）。同じユーザーだけが届く。
- `screenshot.capture` の `output_dir` は `OutputPath` を通らない（`automationcommands_screenshot.cpp:32-35`）。MCP は `resolve_output_dir` で検査する。
- 非制限モードでは、接続先を変えて「開始」を押すと入力済みのパスワードがその先に届く。
- `AutomationInteractions::run` は `AutomationError` しか捕まえない（`automationinteractions.cpp:96`）。
- `paramspec.cpp` の `isInteger` は範囲外の double を `qint64` にキャストする（厳密には未定義動作。実行では正しく拒否）。

**GUI・ファイル**

- エンジンの出力が自動判定の部品に入る（`pvboarddialog.cpp:37, 82`、`changeenginesettingsdialog.cpp:49`、`enginesettingsoptionhandler.cpp:146, 157`、`engineregistrationdialog.cpp:191`、`tsumeshogigeneratordialog.cpp:256`）。エンジンは信頼する側だが、V12 と一緒に PlainText にするのが簡単。
- 細工した設定ファイルで起動時に落ちる（`[Engines] size=2147483647`。`engineprofilestore.cpp:77-79`）。書き換えられるのは同じ利用者だけ。
- 設定ファイルは 0644、フォルダは 0755。中身は CSA の接続先・ID・履歴と最近のファイルのパス。パスワードはない。
- 通信ログの折り返しをオンにすると長い行で遅くなる（既定はオフ）。
- Windows の疑い: `SetDefaultDllDirectories` がない。`JosekiImport::localPaths` は `file://host/...` も通す。`.bat` のエンジンのパスの `&`。
- JKF の独自の終局語が指し手として読み直される（special が「３四歩(33)」）。KIF の読み手の `isCustomTerminal` の検査が JKF にない。

**配布・リポジトリ**

- **Hayanagi**: `position` の手順が長いと `HistoryNode` の `shared_ptr` の連鎖の再帰解放でスタックを使い切り SIGSEGV になる（100万手・5MB で再現。20万手では落ちない）。落ちるのはエンジンのプロセスだけで、アプリに同じ長さの棋譜を読ませる経路は実行していない。SFEN の読み取りは範囲検査が丁寧だった。
- **Qt 6.7.2**: AppImage の Qt はサポートが終わった版のまま（前回から変わらない）。OpenSSL 3.6.5 は新しい。外部の入力が届く範囲は小さい。画像の読み込みは qrc だけで、SVG も自前の資源だけ。リッチテキストは V12 を直せば届かなくなる。CVE ごとの照合はしていない（公開の一覧は第三者のもので、6.7 の版の範囲を確かめられなかった）。
- **配布物の署名**: リポジトリに Actions の secrets がなく、Windows は Authenticode なし、macOS は公証なしのまま。README は `xattr -dr com.apple.quarantine` を案内する。チェックサムは Release に添付しない方針（GitHub の digest は API で読める）。
- **CI**: `ci.yml` は手動で無効。リリースは手元のコンテナで作る（`release.yml` の `v*` のタグは今のタグの形式に合わず起動しない）。Dependabot のアラートは無効で、`dependabot.yml` は github-actions だけ。`allowed_actions: all`・`sha_pinning_required: false`。`ci.yml:116, 505` は `${{ github.base_ref }}` をシェルに直接埋める（ブランチを作れる人だけが届く）。`brew install create-dmg` は版を固定しない。Windows の署名は `*.exe` だけで DLL に付かない。

---

## 4. 調べて問題がなかった点

### 前回の修正

| 前回 | 結果 |
|---|---|
| S1・S2・S3 | 効いている（`paths.py` は下記）。残りは V7・V8 |
| S4・S12 | `NotificationPresenter`・待機ダイアログ・ツールチップ（`plainToolTip`）は効いている。残りは V12 |
| S5・S6 | CLI では効いている。7万手 0.91秒、変化10万個 2.8秒、「,」100万個 0.08秒、1行3万手 0.59秒。GUI の後段が V4〜V6 |
| S7・S16 | リリース CI の固定と権限は効いている。`readelf` でアプリ・CLI・Hayanagi に BIND_NOW・PIE・IBT/SHSTK を確認。AppDir の OpenSSL は 3.6.5 |
| S8 | 「追加」ボタンは塞がっている。残りは V26 |
| S9 | 対局情報の値・コメント・しおり・CSA の受信は効いている。残りは V13・V14 |
| S10・S11・S15 | ソケット・スクリーンショット・`OutputPath` は効いている（実行で確認。Linux） |
| S13 | 大部分は効いている。残りは V22、副作用が V18 |
| S14 | FIFO・`/dev/zero`・ディレクトリ・65MiB は拒否。残りは V19・V25 |
| S17 | OpenSSL・curl は新しい版。Qt は Info |
| S18 | ツール3つの SHA-256 は効いている。ランタイムだけが V10 |
| S19・S20 | jsonschema の検査、CLI の同時実行、標準出力の上限は効いている。残りは V29・V32 |
| S21 | ASCII の範囲は効いている。残りは V16 |

### MCP の境界（実行で確認）

- シンボリックリンク（途中の要素・ファイル・`/proc/self/root`・ループ・`/dev/zero`）、`..`、`.SSH`、Unicode の紛らわしい名前、NUL、長いパス、FIFO、`~`、`x.kif.sh`、リンクの先の拡張子を、すべて拒否。
- Windows の判定の単体実行: 代替データストリーム、末尾のドットと空白、`\\?\`、8.3 形式、予約デバイス名を拒否。
- `--` で始まる値は CLI のオプションにならない。USI の注入はできない。`engine="/bin/sh"` は `engine_not_found`。
- ツールは全45個で、前回から増えていない。`unrestricted` は8個（`get_clipboard` が加わった）。パスを受けるツールはすべて `paths.py` を通る。
- 書ける拡張子のファイルが実行・自動読み込みされる経路は見つからなかった。制限モードのツールの組み合わせで許可ディレクトリの外に作用する経路も見つからなかった。

### 自動化 API

- 枠組み: 9MiB の改行なしは切断、5000段のネスト、バッチ、`1e400` を拒否。エンドポイントファイル 0600、ソケット `srwx------`。
- 許可リストは前回から増えていない。`actionQuit`・`actionSave`・`actionOpenWebSite` は not_allowed。パスワードは読めない。
- 31メソッドすべてに引数の表がある（コード）。結果の記録は256件まで。
- 主なモーダルを開いたままの `app.quit`、許可リストの操作の総当たり（約555回）で ASan の報告なし。

### パーサ

- 変異ファズ（約40万入力以上）と狙った入力47種以上で、ASan の報告はなし。UBSan は V22 だけ。
- `JosekiImport::expandPaths` はリンクの循環・FIFO・`/dev/zero` へのリンクを飛ばす。`JosekiLines::expand` は循環する定跡で止まる。
- 正規表現は40万文字で 0.35秒以内。
- 添字・`QStringView` の寿命・`fromSfen` の未確認の参照・USEN の番号・定跡の16ビットの番号はコードで確認した。

### CSA・USI

- 受信の上限（65537バイトの行で切断、Game_Summary 2MB・20000行）は再現で確認。TLS の上でも効く。
- TLS: 信頼できない証明書・ホスト名の不一致・期限切れを拒否し、LOGIN を送らない。TLS 1.1 を拒否。再接続も平文に落ちない。
- パスワード: カナリアを使い、通信ログ・設定・履歴・自動保存のどこにも出ないことを確認。再接続は同じ接続先にだけ送る。
- 状態機械のファズ（ASan+UBSan の GUI で18回）と20種の不正な `BEGIN Position` で報告なし。
- S4・S9・S13 の修正は `CsaGameSummary::Time` への書き換えで戻っていない。
- USI: `position` は型付きの値から作り直し、`sendCommand` が CR・LF を拒否し、`QProcess::start(path, {})` でシェルを通さない。

### GUI・ファイル

- `openUrl` は固定の URL とコメント欄の2か所だけで、コメント欄は http・https・ftp だけを Ctrl+クリックで開く。
- `system`・`popen`・`QLibrary`・`QPluginLoader`・`addLibraryPath` はない。`QProcess` はエンジンだけ。翻訳をカレントディレクトリから読まない。
- スタイルシートに外部の値は入らない。SQL は `prepare`・`bindValue`。棋譜・定跡・詰将棋集の保存は `QSaveFile`。
- Release は `QT_NO_DEBUG_OUTPUT` と空のハンドラで何も出さない。

### 秘密情報・公開サイト

- リポジトリの追跡ファイルと全履歴に、実際のトークンや鍵は見つからなかった。`tests/fixtures/tls/server-key.pem` はテスト専用（CN=ShogiBoardQ test only）で、配布物に入らない。
- GitHub の secret scanning と push protection は有効。`main` は force push と削除を禁止し、管理者にも適用。
- `docs/` の HTML は外部のスクリプトを読み込まず、`http://` の参照もない。
- アプリは CSA 以外の通信をしない（`QNetworkAccessManager` なし）。`.desktop` はファイルを引数に取らない。

---

## 5. 調べきれなかった範囲

- 実機の Windows・macOS は未実行（V27・V33・Windows の疑い・schannel・SecureTransport・APFS の正規化）。
- V2 が悪用でクラッシュより先に進むか。V9 のプログラム実行まで。
- 実際の shogi-server（すべて偽サーバー）。Hayanagi 以外のエンジンに不正な局面を送った場合。
- 定跡の数 GB のファイルの実測。詰将棋問題一覧の大きなファイル。英語表記の GUI の計算量。
- 設定の `@Variant` の読み込み。証明書の失効の確認。
- Hayanagi は USI と SFEN の入口と履歴だけを読み、探索と定跡の全体は見ていない。
- 依存ライブラリの CVE の照合。

---

## 6. 直す順序の提案

1. **V1・V2・V11（CSA の状態機械とモーダル）**: 悪意のサーバーに接続するだけで落ちる問題から。再入の防止とノード ID への置き換えが中心。
2. **V4（1行の順序の入れ替え）・V13（`singleLine` を通す）・V12（PlainText）**: 小さい変更で塞げる。
3. **V9（Xvfb の認証）・V10（ランタイムの固定）・V34・V35**: 数行か設定の変更で済む。次のリリースの前に。
4. **V7・V8（MCP の既定）**: 既定の許可ディレクトリを専用のフォルダにする。文書も直す。互換性の判断が要る。
5. **V3・V5・V6（GUI の計算量と上限）**: 手数・変化・コメントの長さの上限の方針を決めてから。V15・V19〜V21 も同じ方針で片付く。
6. **V14・V16〜V18・V22〜V33**: 残りの値の検査と文書。

---

## 7. 修正の記録（2026-11-09）

V1〜V35 を修正し、4つのコミットで main へ push した（3e4ef04e・c84a59ca・2bb4e94c・581618cf）。修正前の基準の実行では `tst_mcp_python` が落ちた（どのテストかはログが上書きされて分からない）。修正後は全143件が通り、`tests/gui` の `commentsAndBookmark`・`branchNavigation`・`tst_ponder_flow` も通った。

| # | 直した内容 | 確かめた結果 |
|---|---|---|
| V1 | `onReadyRead` の再入防止。ログイン失敗は切断の後に知らせる。通信対局のエラーは待たずに1つに差し替える。不正な `REJECT:` はログだけ | 再現スクリプト: 最初の `LOGIN:incorrect` で接続を閉じるので次の行が届かず、生存（VmStk 132kB、窓2個。修正前は3762行で SIGSEGV） |
| V2 | しおり・移動・分岐編集はノードを番号で持ち引き直す。確認の後に移ってよいかを確かめ直す（`setBrowseCheck`）。`kifu.goto`・`kifu.load`・`position.set` はモーダルがあれば `invalid_state` | ASan 版で解放後の使用の再現6本すべてに報告なし。単体テストあり |
| V3 | 再開局面 512 手・対局 2000 手の上限と `Max_Moves` の監視。`onReadyRead` は64行ごとにイベントループへ返す。`initialMoveTime` を廃止して線形に。**1手ごとの O(手数) はそのまま**（上限で抑えた） | 19000手は REJECT。512手で1.1秒（修正前 19000手で 309秒） |
| V4 | X 軸の範囲と間隔を、途中で目盛りが増えない順で変える | 2万手 0.89秒（修正前 10秒）。7万手 3.06秒（修正前 208秒） |
| V5 | 10万字・5000行を超えるコメントは先頭だけを読み取り専用で出す。長い語は `WrapAnywhere` | 128KB の1語 0.10秒（修正前 16.5秒）。2MB 0.59秒。3MB の行 1.04秒（修正前 20秒） |
| V6 | 5000行を超えると詰めた配置をやめる。隠れた行の判定を線形に | 変化10万個の USEN 3.7秒（修正前 13.5〜20.5秒）。同じく KIF 4.2秒（修正前 36〜75秒） |
| V7 | 既定の許可先を `~/ShogiBoardQ-MCP` に。`.json` は JKF としてだけ読む。USI のエラー文に中身を入れない。`ALLOWED_DIRS` の空・相対はエラー | CLI の210通りの変換で、棋譜として読める行のほかに中身は出なかった。`.json` を JKF に限る動作は MCP の単体テスト。実機の Windows・macOS は未実行 |
| V8 | 画像の上書きは、このサーバーが書いたものだけ。`SHOGIBOARDQ_ALLOW_IMAGE_OVERWRITE=1` で許す | 単体テスト |
| V9 | Xvfb に `-auth`。鍵はコマンドラインに載せない | 鍵なしの接続は `Authorization required`、鍵ありは接続できた（別 UID は未実行） |
| V10 | ランタイムを SHA-256 で固定して `--runtime-file` で渡す | 実物の appimagetool で、取得せずに固定の版を埋め込むことを確認（16バイトの埋め込み情報だけ違う）。コンテナでの全ビルドは未実行 |
| V11 | 頼んでいない `LOGOUT:` でも閉じる。応答待ちの間は対局条件を受けない。段階が Off・Failed の対局条件と START を断る | 再現スクリプト: 棋譜が置き換わらず、接続が閉じた |
| V12 | `confirmAction`・`UnsavedChangesPrompt`（補足の文も）・`DialogUtils::warning` などを PlainText に。dialogs・widgets の静的 `QMessageBox` を置き換え、`tst_structural_kpi` を広げた。Info のエンジン出力の部品も PlainText に | 単体テスト |
| V13 | `$TIME_LIMIT` の値を1行にする | 単体テスト |
| V14 | 手合割は開始局面から書く。指し手・局面図・手番・コメントとして読まれるキーは書かない | 単体テスト（読み直しで手順が同じ） |
| V15 | 表示は1行 4096 字まで。パネルの合計も100万字まで | 6万字の行の洪水で RSS 262MB（修正前 3825MB）、応答 3ms |
| V16 | 名前を UTF-8 で100バイトまでに切る | 単体テスト |
| V17 | AGREE を送っていない START は切断する。前の局の終わりから1秒は同意を遅らせる | 再現スクリプト: 同意なしの START で切断 |
| V18 | 手数を上限で止める。行き所のない段への駒打ちを断る。`kifu.goto` は `ScopedCapture` で受ける | 単体テスト |
| V19 | 1回の読み込みの手数を30万に制限 | 単体テスト |
| V20 | 警告は30行と「ほか N 件」 | 単体テスト |
| V21 | `ElideLabel` の描画とツールチップは2000字まで。棋譜欄の列は幅の計算に上限 | 単体テスト |
| V22 | 飽和する足し算と上限（定跡・時間の累計・`$TIME_LIMIT`・持ち時間） | 単体テスト |
| V23 | 定跡のマージを線形に | 既存のテスト |
| V24 | `hasNextSibling` を O(1) に | 既存のテスト |
| V25 | 局面集は別スレッドで読み、20万行まで | 既存のテストと単体テスト |
| V26 | USI コンソールとエンジンのファイルの置き場所の欄を、自動化 API から操作できないようにした（`automationForbidden`）。文書も修正 | 単体テスト |
| V27 | Windows のネットワークパスを開く前に文字列で比べる | 判定の単体テストのみ。実機は未実行 |
| V28 | ヘッドレスのスクリプトは `mcp/` から起動。設定例に `PYTHONSAFEPATH=1`（Python 3.11 以上。3.10 は `pip install` のコマンドを案内） | ヘッドレスのスクリプトの起動でエラーなし。import の順序の再現は未実行 |
| V29 | 作者が書いた文字列（コメント・対局情報・警告）を `max_chars` の合計に収める | 500手×3000字で `structuredContent` 約9万字（修正前は 190万字規模） |
| V30 | `convert_kifu`・`render_board_image`・`save_kifu` を `destructive` に | テスト |
| V31 | `<untrusted-data>` の印、`INSTRUCTIONS` と説明の注意、`openWorldHint` | テスト |
| V32 | CLI を別のプロセスグループで起動し、強制終了のときはグループごと終わらせる（POSIX のみ） | 既存のテスト。EOF を無視するエンジンの再現は未実行 |
| V33 | Windows のパイプ名を起動ごとの乱数入りにする | Windows は未実行（コードのみ） |
| V34 | Private vulnerability reporting を有効化。`SECURITY.md` とテンプレートの案内 | API で `{"enabled":true}` |
| V35 | `.claude/settings.local.json` と `.qtcreator/*.user.*` を追跡から外して `.gitignore` に追加 | コミット 3e4ef04e で追跡から外した |

Info は V12 と一緒に直したエンジン出力の部品と、`SHOGIBOARDQ_ALLOWED_DIRS` の空の値・相対パス以外は手を付けていない。
