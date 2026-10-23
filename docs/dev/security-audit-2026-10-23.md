# セキュリティ監査（2026-10-23）

「ソースコードのセキュリティ監査」の依頼で、`src/`（約8.5万行）・`mcp/`・ビルドと CI（`.github/workflows/`・`scripts/`）を監査した。

- 対象のコミットは 58113bb6。本文の行番号はこの時点のもの。
- 攻撃面を5つに分けて読んだ。
  - 自動化 API
  - MCP サーバーと CLI の境界
  - 棋譜・定跡・局面集のパーサ
  - USI エンジンと CSA 通信対局
  - GUI の表示・ファイルの書き込み・ビルドと配布
- 想定した攻撃者は次のとおり。
  - 細工した棋譜・定跡・局面集の作者。利用者が開くか、MCP 経由で読み込まれる。
  - 悪意のある CSA サーバー、または平文の CSA 通信の中間者。
  - プロンプトインジェクションで操られた AI エージェント。MCP の制限モードの中に被害を閉じ込められるかを見た。
  - 同じマシンの別ユーザー。
  - ビルド・配布の供給網。
  - 利用者が登録した USI エンジンは信頼する側に置いた。もともと利用者の権限で動くプログラムだからである。
- 「確認」の欄の意味は次のとおり。
  - 「実行で再現」: 監査中に次のいずれかで確かめた。CLI、Xvfb 上のアプリ、偽の CSA サーバー、偽のエンジン、ASan+UBSan 版の CLI（`build-asan/` に残してある）。
  - 「コードで確認」: 経路を読んで確かめた。
  - 「疑い」: 実行もしておらず、経路も確かめきれていない。
- 監査ではソースを変更していない。実験は、利用者の設定とホームに触れない隔離した設定ディレクトリで行った。

> **修正の進み具合**: S1 から順に直し、各項目の見出しの下に「状態」を書いた。すべて済。
>
> **この文書の置き場所**: リポジトリは公開で、`docs/` は GitHub Pages で公開される。そのため、直すまでは git 管理外に置き、High・Medium を直したコミットを push してから `docs/dev/` に移した。

## 全体の傾向

守られていたもの（詳しくは4章）:

- **USI**: 外部の値からエンジンへのコマンドの注入はできない。position は型付きの `UsiPosition` から作り直し、エンジンは登録済みの名前でしか選べない。
- **CSA**:
  - パスワードは保存せず、ログでは伏せる。
  - TLS は証明書を検証する。
  - 受信は1行 64KB・Game_Summary 2MB まで。
  - 送る値は正規表現で検査する。
- **自動化ソケット**:
  - 既定の場所では所有者だけが使える。
  - エンドポイントファイルは 0600。
  - 1メッセージは 8MiB まで。
  - 全メソッドの引数を表で検査する。
- **MCP の `paths.py`**: シンボリックリンク・`..`・前方一致の誤り・存在確認のオラクルを防いでいる。
- **パーサ**: ASan+UBSan で試した入力では、ヒープの範囲外アクセスも解放後の使用も出なかった。
- **表示と保存**: 棋譜コメント・対局情報・読み筋などの表示はエスケープ済み。保存は `QSaveFile` で行う。

残っている問題は次の5つに分けられる。

1. **MCP の閉じ込めが既定では効かない**。許可ディレクトリの既定がホーム全体で、書き出し先の名前も中身も絞らない。そのため、操られたエージェントがホームの資格情報を読み、起動時に実行されるファイルを書ける（S1・S2）。クリップボードも制限モードで読める（S3）。
2. **外部の文字列を Qt の自動判定（`Qt::AutoText`）の部品に入れている**。CSA サーバーの受信行がリッチテキストとして描かれ、リンクやローカルファイルの画像を出せる（S4）。定跡のコメントのツールチップも同じ（S12）。
3. **棋譜の読み込みに、再帰の深さ・計算量・大きさの上限がない**。数百 KB〜数 MB のファイルで、クラッシュ・数 GB のメモリ消費・数十秒の停止が起きる（S5・S6）。
4. **共有の `/tmp` の固定名を、所有者を確かめずに使う**（S10）。複数利用者の環境で問題になる。
5. **配布の供給網が弱い**: リリース CI の `curl | sh`、検証なしのツール取得、古い同梱ライブラリ、堅牢化フラグの不足（S7・S16〜S18）。

---

## 1. 指摘一覧

| # | 深刻度 | 指摘 | 攻撃者 | 確認 |
|---|---|---|---|---|
| S1 | **High** | MCP の制限モードで、ホームの任意の場所に、中身の一部を決められるファイルを書ける（起動ファイルに書けばコード実行に届く） | 操られた AI エージェント | 実行で再現 |
| S2 | Medium | MCP の制限モードで、ホームの棋譜でないファイル（資格情報など）の中身を読み出せる | 操られた AI エージェント | 実行で再現 |
| S3 | Medium | MCP の制限モードで、システムのクリップボードを読める | 操られた AI エージェント | コードで確認 |
| S4 | Medium | CSA サーバーの受信行がリッチテキストとして描かれる。エラーダイアログではリンクとして押せる。待機ダイアログではローカルファイルを画像として読み込み、`/dev/zero` で落ちる | 悪意の CSA サーバー・中間者 | 実行で再現 |
| S5 | Medium | 長い手順（7万手・350KB）の棋譜で、再帰がスタックを使い切ってクラッシュする | 細工した棋譜 | 実行で再現 |
| S6 | Medium | 棋譜の読み込みの計算量とメモリが爆発する（1.9MB で 7.2GB、1.5MB で 19秒、1MB で 27秒） | 細工した棋譜 | 実行で再現 |
| S7 | Medium | リリース CI が、版を固定しない `curl … main/install.sh \| sh` を `contents: write` の権限で実行する | 供給網 | コードで確認 |
| S8 | Low | MCP の非制限モードでは、許可リストのエンジン設定から任意の実行ファイルを起動できる。文書の「実行ファイルパスは受け付けない」と食い違う | 操られた AI エージェント（非制限モード） | コードで確認 |
| S9 | Low | 対局情報の値（とキー）の改行がそのまま書き出され、保存した棋譜に別の行が入る。CSA の Name+ から開始局面を変えたり手を足したりできる | 中間者・細工した棋譜 | 実行で再現 |
| S10 | Low | 共有の `/tmp` の固定名を所有者を確かめずに使う（自動化ソケットの退避先・スクリーンショット・MCP の出力先） | 同じマシンの別ユーザー | 一部を実行で再現 |
| S11 | Low | ソケットパスの前処理が破壊的。既存のファイルの種類を問わず消し、既存ディレクトリを chmod し、2つ目のインスタンスがソケットを奪う | 利用者自身の設定の誤り | 実行で再現 |
| S12 | Low | 定跡ファイルのコメントがツールチップで HTML として描かれる | 配布された定跡ファイル | 一部を実行で再現 |
| S13 | Low | 符号付き整数のオーバーフロー（UB）。手数・持ち駒・時間・変化の番号・CSA の持ち時間 | 細工した棋譜・CSA サーバー | 実行で再現（UBSan） |
| S14 | Low | 入力の大きさに上限がなく、特殊ファイルも読む。`kifu.load` に FIFO を渡すと GUI が止まる | 細工したファイル・同じユーザー | 実行で再現 |
| S15 | Low | `kifu.save` が、リンク先のないシンボリックリンクをたどって、上書きの許可なしに別の場所へ書く。拡張子を問わず KIF で保存する | 保存先にリンクを置ける者 | 実行で再現 |
| S16 | Low | 実行ファイルの堅牢化フラグ（BIND_NOW・FORTIFY・CET）がない | ― | 実行で確認 |
| S17 | Low | AppImage に古い OpenSSL 3.3.1・libcurl 8.8.0・Qt 6.7.2 を同梱している | 供給網 | 実行で確認 |
| S18 | Low | Linux のビルドが、linuxdeploy・appimagetool を `continuous` から検証なしで取って実行する | 供給網 | コードで確認 |
| S19 | Low | MCP の引数が inputSchema で検査されない（文書と食い違い）。MCP だけの上限（`max_chars` など）を外せる | 操られた AI エージェント | 実行で再現 |
| S20 | Low | MCP の同期の CLI 呼び出しに同時実行の上限がなく、キャンセルで子プロセスが残る。配列の出力と stdout に上限がない | 操られた AI エージェント | コードで確認 |
| S21 | Low | 自動保存のファイル名で、NUL・制御文字・双方向制御文字を除かず、長さも制限しない | 悪意の CSA サーバー | 実行で再現 |

Info は5章にまとめる。

---

## 2. 詳細

### S1【High】MCP の制限モードで、ホームの任意の場所に中身を決められるファイルを書ける

**状態: 済（582a74ce）**。許可ディレクトリより下の `.` で始まる名前を拒否し、拡張子を棋譜（読み込みは `.json` も）と盤面画像に限った。一覧は `shogiboardq-cli environment` の `kifu_read_suffixes`・`kifu_write_suffixes`・`image_write_suffixes`（`paths.py`、`KifuFormats::writableSuffixes()`、`BoardImageRenderer::fileSuffixes()`）。既定の許可ディレクトリ（ホーム）は変えていない。テスト `mcp/tests/test_paths.py`・`test_environment.py`。対局情報の改行は S9 で直す

- **場所**
  - `mcp/shogiboardq_mcp/paths.py:22-31`: 許可ディレクトリの既定はホーム全体。
  - `paths.py:68-82`: `resolve_write_path` は、拡張子もドットで始まる要素も見ない。
  - `mcp/shogiboardq_mcp/handlers_cli.py:67-75`（`convert_kifu`）と `handlers_app.py:64-67`（`save_kifu`）: どちらも制限モードで使える。
  - `src/kifu/formats/kifexporter.cpp:196-201`: 対局情報のキーと値を、そのまま1行の `キー：値` として書く。
- **前提**
  - `SHOGIBOARDQ_ALLOWED_DIRS` を設定していない。README・ガイドの既定どおりの使い方がこれに当たる。
  - エージェントが、読み込んだ棋譜のコメントなどに仕込まれた指示に操られている。
- **流れ**
  1. `convert_kifu` の `text` に、キーの部分が任意の文になっている対局情報の行（例: `<任意のコマンド> #：x`）を含む KIF を渡す。
  2. キーはそのまま出力の独立した1行になる。S9 の改行を使えば、値の側からも行を差し込める。
  3. `output_path` には、ホームの中のどのファイル名でも指定できる。まだないファイルなら `overwrite` も要らない。
  4. シェルの起動ファイル、Python の `.pth`、デスクトップの自動起動など、読み込まれると実行されるファイルに書けば、利用者の権限でコードが実行される。
  5. `load_kifu`（text）と `save_kifu` の組み合わせでも同じ KIF の書き出しを通る（コードで確認）。
- **再現**: 次の KIF を `convert-kifu` で `out.bashrc` に書き出した。
  ```
  先手：A
  echo INJECTED_MARKER #：x
  手数----指手---------消費時間--
     1 ７六歩(77)
  ```
  出力に `echo INJECTED_MARKER #：x` の行がそのまま入り、`bash out.bashrc` で `INJECTED_MARKER` が表示された。MCP の担当は、許可ディレクトリを scratchpad に限ったうえで、MCP のハンドラ経由でも同じことを確かめた。Python の site の仕組みで処理する経路も同様に確かめた。
- **修正案**（上から優先）
  1. 既定の許可ディレクトリをホーム全体から専用のフォルダ（例: `~/Documents/ShogiBoardQ`）にする。または明示の設定を必須にする。
  2. パスの要素に `.` で始まるものがあれば拒否する。
  3. 書き出しの拡張子を形式に合うもの（`.kif` `.kifu` `.ki2` `.csa` `.jkf` `.usi` `.usen` `.sfen` `.png`）に限る。アプリ側の `kifu.save`（S15）も同じにする。
  4. 多重の防御として、書き出しで対局情報のキーと値の制御文字を除く（S9）。

### S2【Medium】MCP の制限モードで、棋譜でないファイルの中身を読み出せる

**状態: 済（582a74ce）**。S1 と同じ変更で、隠しファイルと棋譜以外の拡張子を読めなくした。パーサのエラー文に行を入れる点は、GUI で原因を示すのに使うので残した

- **場所**
  - `mcp/shogiboardq_mcp/handlers_cli.py:44-51, 77-79`: `game_info`・`text`・エラー文をそのまま返す。
  - `src/kifu/formats/usitosfenconverter.cpp:130, 219`: エラー文に先頭行を入れる。
  - KIF・KI2 のヘッダーの解釈: ASCII の `:` の行も対局情報として扱う。
- **流れ**: `convert_kifu`・`analyze_kifu`・`load_kifu` の入力パスに、許可ディレクトリの中の任意のファイルを渡すと、中身の一部が返る。
  - 自動判定・KIF・KI2 として読ませると、`key: value` の行がすべて `game_info` と出力のテキストに入る。
  - `input_format=usi` で読ませると、先頭行が parse_error の文に入る。
- **再現**（偽の値のファイル）
  - gh の `hosts.yml` 形式のファイルでは、`game_info` に `{"key":"oauth_token","value":"gho_FAKE…"}` が入った。
  - `.netrc` 形式のファイルを `usi` で読ませると、エラーに `Unknown USI position format: machine example.com login me password FAKEPW` が出た。
  - MCP の担当は、`.git-credentials`・`.npmrc` の `_authToken`・`.env` の1行目も回収できることを確かめた。AWS の credentials（`=` 区切り）と SSH 鍵の本体は出なかった。
- **修正案**
  - S1 の 1〜2（許可ディレクトリとドットで始まる要素）。
  - 入力の拡張子を棋譜の形式に限る。
  - エラー文に入力行の中身を入れず、行番号だけにする。
  - 指し手が0手のときは、知らないキーの `game_info` を返さない。

### S3【Medium】MCP の制限モードで、システムのクリップボードを読める

**状態: 済（582a74ce）**。`get_clipboard` を `unrestricted=True` にした（`test_security.py`）

- **場所**
  - `src/automation/automationcommands_widget.cpp:110-120`（`clipboard.get`）: `QApplication::clipboard()->text()` を、最大100万文字までそのまま返す。
  - `mcp/shogiboardq_mcp/tooldefs.py:572-581`（`get_clipboard`）: `unrestricted=True` が付いていない。
- **流れ**: 利用者がほかのアプリでコピーした内容（パスワード管理ソフトの値など）を、エージェントがいつでも読める。許可ディレクトリの方針の外にある情報である。
- **修正案**: `get_clipboard` を非制限モードのツールにする。または、アプリ自身が最後にコピーした内容と一致するときだけ返す。

### S4【Medium】CSA サーバーの受信行がリッチテキストとして描かれる

**状態: 済（96e4096e）**。`NotificationPresenter` は `QMessageBox` を自分で作って `Qt::PlainText` にする。待機ダイアログの `csaWaitingDetail` と通信対局の終局のダイアログも PlainText（`tst_csagamesession` の `serverTextIsShownAsPlainText`）

- **場所**
  - 知らせ: `src/ui/presenters/notificationpresenter.cpp:44-53`
    - `QMessageBox::information/warning/critical` に文をそのまま渡す。
    - `QMessageBox` は文を自動判定し、最初の行にタグがあれば HTML として描く。
    - リンクは Qt の既定（`openExternalLinks`）で外部のアプリで開く。
  - 受信行を知らせに入れる所
    - `src/network/csaclient_parser.cpp:70`（不正な `REJECT:` 行）
    - `src/network/csagamecoordinator_setup.cpp:50`（Game_Summary の指し手）
    - `src/network/csagamecoordinator_moves.cpp:158`
  - 待機ダイアログ: `src/dialogs/csawaitingdialog.cpp:86-90, 229-239`
    - `m_detailLabel` は既定の自動判定の `QLabel` である。
    - 受信行はすべて `"[RECV] %1"`（`src/network/csagamecoordinator.cpp:423`）の形でここに出る。対局者名・Protocol_Version も同じ経路を通る。
- **前提**: 悪意の CSA サーバー、または平文 CSA（既定）の中間者。
- **再現**: Xvfb 上のアプリを偽の CSA サーバーにつないで確かめた。
  - (a) 不正な REJECT 行に `<a href=…>` を入れると、エラーダイアログに大きな字のリンク「ここをクリックして更新してください」が描かれた。
  - (b) ログイン後に `<img src="<ローカルの PNG>">` を送ると、待機ダイアログにその画像が描かれた。Qt の `QTextDocument::loadResource` はローカルパスを読む。
  - (c) `<img src="/dev/zero">` を送ると、メモリを使い続けて `std::bad_alloc` でアプリが落ちた。アドレス空間は 7.2GB に制限していた。
  - (d) `<img src>` に FIFO を指すと、書き手が離すまで GUI が止まった（`app.ping` が 9.4秒かかった）。
  - Windows で UNC パスを指したときの SMB 接続（NTLM の漏洩）は疑いで、確かめていない。
  - クリックで開くことは、Qt の既定の設定（`openExternalLinks=true`）を実行時に確かめたところまで。押した後の起動は、記録が残っておらず確かめきれていない。
- **修正案**
  - `NotificationPresenter` で `QMessageBox` を自分で作り、`setTextFormat(Qt::PlainText)` にする。これで全部の知らせをまとめて塞げる。
  - `m_detailLabel->setTextFormat(Qt::PlainText)` にする。
  - 方針として、外部の値を出す `QLabel` は PlainText にする。

### S5【Medium】長い手順の棋譜でスタックを使い切ってクラッシュする

**状態: 済（7271f6bd）**。`KifuBranchTree::collectLines()` を明示のスタックにした。JKF の書き出しは変化の入れ子を300段まで書く（`kMaxForkDepth`。Qt の JSON の読み込みの上限 1024 段に収まる）。テスト `longLineIsListedWithoutDeepRecursion`・`deeplyNestedVariationsAreCapped`。7万手・10万手の USI を変換できることを確認

- **場所**: `src/kifu/kifubranchtree.cpp:195-241` の `collectLinesRecursive`。手数の深さだけ再帰する。
- **呼び出し元**
  - `allLines()` の CLI の変換（`src/headless/kifuconversionservice.cpp:64`）
  - 評価値グラフ（`src/ui/presenters/evaluationgraphpresenter.cpp:37`）
  - ナビゲーション（`src/kifu/kifunavigationstate.cpp:63`）
  - 書き出し（`src/kifu/formats/kifexporter.cpp:166` など）
- **再現**: `position startpos moves` の後に `5i5h 5a5b 5h5i 5b5a` を繰り返して7万手（350KB）にしたファイルを使った。
  - Release 版の `convert-kifu` が SIGSEGV（rc=139）で落ちた。5万手では落ちない。
  - ASan 版は stack-overflow を報告した。
  - GUI ではメインスレッドで呼ぶので同じく落ちるはずである。Windows のメインスレッドのスタックは 1MB なので、もっと短い手順で落ちると考えられる（推測）。
- **同じ型（疑い）**: `src/kifu/formats/jkfexporter.cpp:83` の `buildJkfMoves` は、分岐の入れ子の深さだけ再帰する。KIF・USEN から読んだ深い入れ子を JKF で保存するときに上限がない。
- **修正案**: 明示のスタックを使う反復に書き換える。あわせて手数と分岐の深さに上限を置く。

### S6【Medium】棋譜の読み込みの計算量とメモリが爆発する

**状態: 済（7271f6bd）**。`BranchLine` はノードの列を持たず終端 `leaf` から作る（`nodes()`・`nodesFrom()`・`length()`）。組み立ては変化ごとに経路を複製しない。`latestBranchPoint` は手数ごとの表（`lastVariationAtPly`）。`splitStatements`・`readNumberedMoves` は位置で進め、正規表現の UTF-16 の検査を1回にした。`UsiPosition` は手の列を共有し（`alongMoves`）、`fromMoves`・`parse` はその場で足す。定跡の読み込みは同じ局面の登録を読み終えてからまとめる。実測: 1.9MB の KIF 7.2GB・9.5秒 → 132MB・2.0秒、変化10万個の KIF 19秒 → 0.4秒、CSA の「,」100万個 27秒 → 0.07秒、1行3万手の KIF 15.5秒 → 0.37秒。修正前の CLI と全フィクスチャの変換結果が一致

| 箇所 | 原因 | 入力と実測（Release 版） |
|---|---|---|
| `kifubranchtreebuilder.cpp:21-26`、`kifubranchtree.cpp:206` | 変化ごとに根からの全経路を複製して持つ（手数×変化数） | 本譜2万手＋1手の変化4万個（1.9MB の KIF）で、最大 RSS 7.2GB・9.5秒。大きさを倍にすると約4倍 |
| `kifparsetypes.h:93-102` の `latestBranchPoint`（呼び出しは `kiftextparser.cpp:225`・`usentosfenconverter.cpp:269`） | 変化1つごとに、それまでの全変化を走査する | KIF の「変化：1手」10万個（1.5MB）で 19秒（監査者も再現）。USEN の空の変化10万個（300KB）で 18秒 |
| `csatosfenconverter.cpp:67-82` の `splitStatements` | 「,」1つごとに残り全体を `mid()` で複製する | 「,」100万個（1MB）で 27秒。自動化 API の `kifu.load`（text）に80万個を渡すと、GUI のメインスレッドが 16.9秒応答しない（制限モードの `load_kifu` でも届く） |
| `kiflexer.cpp:91-127` の `readNumberedMoves` | 手ごとに行の残りを複製し、正規表現を先頭から当て直す | 3万手を1行に並べた KIF（420KB）で 15.5秒 |
| `usiposition.cpp:70-79` の `withMove` | 手ごとに手順全体を複製する（O(N²)）。analyze-kifu（`clicommands_engine.cpp:255`）は手数ごとに呼ぶので O(N³) | コードで確認 |
| `josekibook.cpp:505` | 同じ局面の手数違いの登録で、flush のたびに全登録を走査する（O(N²)） | コードで確認 |

**修正案**
- ビルダーは変化ごとに分岐元のノードだけを覚える。`BranchLine` は葉と分岐点だけを持ち、経路は親をたどって作る。
- `latestBranchPoint` は、手数ごとに最後の手順を表で持つ。
- `splitStatements`・`readNumberedMoves` は `QStringView` と位置で走査する。
- `UsiPosition` は手を1つずつその場で足す。
- 変化の数・手数・1行の長さに上限を置く（S14 の大きさの上限と合わせる）。

### S7【Medium】リリース CI が版を固定しないスクリプトを書き込み権限で実行する

**状態: 済（ed949d24）**。syft は v1.54.1 を SHA-256 で確かめて使う。ワークフローの既定の権限を `contents: read` にし、`contents: write` は publish-release だけ。`download-artifact`・`cache` を SHA で固定し、`inputs.tag` は環境変数で渡す。`ci.yml` にも読み取りだけの `permissions` を付けた

- **場所**
  - `.github/workflows/release.yml:329-332`: `curl -sSfL https://raw.githubusercontent.com/anchore/syft/main/install.sh | sh`
  - ワークフロー全体が `permissions: contents: write`（`release.yml:19-20`）。
- **影響**: 上流（syft の `main`）が侵害されると、ランナーからトークンを取られる。2025年の tj-actions と同じ手口である。後続の `publish-release` は `pattern: release-package-*` と `merge-multiple: true` で成果物を集めるので、成果物を足されれば正規のファイルが置き換わる可能性もある（疑い）。
- **修正案**
  - syft は版とハッシュで固定するか、SHA で固定した action を使う。
  - `permissions` はジョブごとに最小にし、`contents: write` は publish のジョブだけに付ける。
  - publish で集める成果物の名前を明示する。

### S8【Low】MCP の非制限モードでは任意の実行ファイルを起動できる

**状態: 済（582a74ce）**。エンジン登録の「追加」ボタンに `automationSensitive` を付け、文書（README・開発文書・4言語のガイド）に書いた（`test_security.py` の `test_engine_executable_cannot_be_chosen`）

- **場所**
  - `src/automation/automationactionpolicy.cpp:66`: `actionEngineSettings` が許可リストにある。
  - 起動までの経路
    1. エンジン登録ダイアログの追加ボタン（`src/dialogs/engineregistrationdialog.cpp:99-108`）が `QFileDialog` を開く。
    2. その `fileNameEdit` に `widget.setValue` で書き込む。
    3. `src/engine/engineregistrationhandler.cpp:110` の `probeAsync` が、選んだファイルを起動する。
- **前提**: `SHOGIBOARDQ_ALLOW_UNRESTRICTED_UI=1`。
- **内容**
  - MCP サーバーの説明（「GUI operations then have full app permissions」）とは合っている。
  - しかし `docs/dev/mcp-server.md` 7章の「エンジンは登録済みの名前だけ。実行ファイルパスは受け付けない」は、このモードでは成り立たない。
  - `actionCSA` で任意のホストに接続できることも同様である。
- **修正案**: 文書に「非制限モードは任意のプログラムの実行に等しい」と明記する。さらに次のどちらかを行う。
  - `actionEngineSettings` を許可リストから外す。
  - 実行ファイルを選ぶ部品に、自動化からは操作できない印（`automationSensitive` と同じ形）を付ける。

### S9【Low】対局情報の改行がそのまま書き出され、保存した棋譜に別の行が入る

**状態: 済（7271f6bd）**。`CsaClient` は受信行の制御文字を空白にする。書き出しは `KifuExportCommon::singleLine`（対局情報のキーと値、CSA の N+/N-/$ 行、定跡のコメント）と `splitLines`（コメント・しおり。CR だけの改行も行に分ける）を使う。あわせて、KIF の読み込みが行の途中の「手合割：」も手合割の指定として読んでいた不具合を直した（行頭が「手合割：」「手合：」の行だけ。対局者名やコメントで開始局面が変わっていた）。テスト `lineBreaksInValuesDoNotAddLines`・`csaClient_controlCharactersInLinesBecomeSpaces`

- **場所**
  - 受信: `src/network/csaclient.cpp:369` は行末の `\r` を1つだけ除く。値の途中の `\r` は残り、`csaclient_parser.cpp:258-261` で対局者名になる。
  - 書き出し: `src/kifu/formats/kifexporter.cpp:196-201` と `csaexporter.cpp:76-79` は、キーと値を改行の検査なしで1行に書く。
  - 読み込み: 単独の `\r` を改行として読む（`kifreader.cpp:17-21`、`csatosfenconverter.cpp:62`）。
- **再現**
  - JKF の header の先手を `"Alice\r手合割：香落ち"` にして KIF に変換し、それを読み直すと、開始局面が香落ち（後手番）になった（監査者が CLI で再現）。
  - 偽の CSA サーバーで Name+ に `PI82HI22KA\r+2726FU` を入れ、CSA で書き出して読み直した。飛角落ちの局面になり、指していない手 2g2f が記録に入った。
- **影響**: 中間者が、保存される棋譜（自動保存を含む）の開始局面と手順を書き換えられる。S1 の書き出しの中身を行単位で決める手段にもなる。
- **修正案**
  - `CsaClient` で、受信値の制御文字（`\x00-\x1f`）を除くか置き換える。
  - 全形式の書き出しで、対局情報のキーと値の CR・LF を空白にする。定跡のコメントの書き出し（`josekibook.cpp:657-658`）も同じにする。

### S10【Low】共有の `/tmp` の固定名を、所有者を確かめずに使う

**状態: 済（582a74ce）**。自動化ソケットは `RuntimeLocation` が無ければ設定のフォルダに置く。ソケットのフォルダは、無ければ 0700 で作り、あれば自分の持ち物でほかの利用者が書けないことを確かめる（変えない）。スクリーンショットの既定は `AppPaths::cacheDirectory()/screenshots`（デバッグの F12 も）。MCP の既定の出力先は `tempfile.mkdtemp` で作る自分専用のフォルダ。ヘッドレスのスクリプトは `XDG_RUNTIME_DIR` の下を使い、`/tmp` に作るときは自分の持ち物でリンクでないことを確かめる

| 場所 | 内容 |
|---|---|
| `src/automation/automationserver.cpp:44-47, 71-77` | `XDG_RUNTIME_DIR` が使えないと、`QDir::tempPath()/shogiboardq/automation.sock` に退避する。既にあるディレクトリの所有者を確かめず、`setPermissions` の失敗も無視する |
| `src/ui/controllers/screenshotservice.cpp:20-23, 58-76` | `output_dir` を省くと `/tmp/shogiboardq-screenshots` に書く。ディレクトリは `drwxr-xr-x`、PNG は `-rw-r--r--` で作られる（実行で確認） |
| `mcp/shogiboardq_mcp/paths.py:94-98` | `SHOGIBOARDQ_OUTPUT_DIR` がなければ `/tmp/shogiboardq-mcp` を `exist_ok=True` で使う |
| `scripts/mcp/shogiboardq-mcp-headless.sh:26, 35-36` | `/tmp/shogiboardq-mcp-headless-<uid>` を使う。`set -e` がないので、chmod の失敗を見逃す |

- **前提**: 同じマシンに別ユーザーがいて、被害者の `XDG_RUNTIME_DIR` が使えない（su・cron・一部の ssh やコンテナ）か、出力先を省いている。
- **影響**: 別ユーザーがディレクトリを先に作れば、次のことができる。
  - 自動化 API を止める。
  - 自分のソケットで待ち受け、MCP サーバーをつながせる。MCP は `shogiboardq-cli environment` で同じ退避先を計算する。その結果、送られる棋譜とパスを受け取り、偽の応答を返せる。
  - スクリーンショットを読む。
- **確認**: 退避先の計算は、`XDG_RUNTIME_DIR` を外し `TMPDIR` を変えて再現した。他人の所有のディレクトリでの挙動は、コードで確認した。
- **修正案**
  - 退避先に `/tmp` を使わない。`RuntimeLocation` が空なら、設定ディレクトリの下を使う。
  - 既にあるディレクトリは lstat で「自分の所有・ディレクトリ・0700」かを確かめ、違えば中止する。
  - Python 側は `os.lstat` の st_uid を確かめるか、`tempfile.mkdtemp` を使う。

### S11【Low】ソケットパスの前処理が破壊的

**状態: 済（582a74ce）**。絶対パスに限り、ソケットでない既存のファイルは消さず、待ち受け中のソケットがあれば起動しない（2つ目のアプリが奪わない）。既存フォルダの chmod をやめた。終了時のエンドポイントファイルは自分の pid のものだけ消す。長さの上限を Qt の一時フォルダの分だけ下げた（Linux 99、macOS 95 バイト）。テスト `listenDoesNotTouchOtherFiles`

- **場所**: `src/automation/automationserver.cpp:59-78, 92-94`、`src/app/main.cpp:83-109`。
- **再現**: どれも利用者自身の指定の誤りで起きる。外部からの攻撃経路ではない。
  - `--automation-socket <dir>/v` に既存の通常ファイルを指すと、`QLocalServer::removeServer` がそのファイルを消した。0755 の `<dir>` は 0700 に変わった。
  - 相対パスを渡すと、カレントディレクトリが 0700 に変わった。Qt は相対名を一時ディレクトリの下として扱う。
  - A が待ち受けている間に同じパスで B を起動すると、ソケットが置き換わり、A に届かなくなった。
- **長さの上限のずれ**: Qt 6.12 は同じ親の下に一時ディレクトリを作り、その中で bind してから rename する。そのため、実効の上限はアプリの検査（100バイト）より短い（99 は成功、100 は失敗）。
- **修正案**
  - 絶対パスを必須にする。
  - ソケット（`S_ISSOCK`）でなければ消さない。消す前に接続を試み、使用中なら中止する。
  - chmod は自分で作ったディレクトリにだけ行う。
  - 終了時は、自分の pid のエンドポイントファイルだけを消す。

### S12【Low】定跡ファイルのコメントがツールチップで HTML として描かれる

**状態: 済（96e4096e）**。`JosekiTableModel` のツールチップを `<qt>` とエスケープで出す（テスト `tableModel_showsDerivedReplyInGray`）

- **場所**: `src/dialogs/josekitablemodel.cpp:87-95`。`Qt::ToolTipRole` が、`move.comment`・`move.nextMove`・`move.move` をエスケープせずに返す。
- **入力の出どころ**
  - やねうら王形式の定跡ファイルのコメント（`josekibook.cpp:546, 588`）
  - 読めない指し手の文字列（`josekibook.cpp:142-150`）
  - 棋譜からの一括登録で取り込むコメント（`josekiimport.cpp:109`）
- **確認**
  - コメントの `<img src=…>` が加工されずにモデルに入ることは、実行で確かめた。
  - ツールチップを実際に出す実演はしていない。ツールチップも自動判定の `QLabel` なので、S4 の (b)〜(d) と同じ影響になると判断した。
- **修正案**: `GameInfoEditModel`・`KifuRecordListModel` と同じく、`"<qt>" + toHtmlEscaped() + "</qt>"` にする。

### S13【Low】符号付き整数のオーバーフロー（UB）

**状態: 済（7271f6bd）**。`numberFromDigits` は int の最大値で止める。`Position` に `kMaxHandCount`（99）・`kMaxMoveNumber`（1000万）を設け、SFEN はこれを超えると読まない。持ち駒の足し算・手数の加算は頭打ち。BOD の手数・JKF の時間・USEN のオフセット・CSA の T の秒数（1000万秒まで）を桁あふれさせない。通信対局の時間は `TimeControl::toMs`（7日で頭打ち）と飽和する足し算。`*Position::fromSfen` の参照は結果を確かめてから。UBSan 版の CLI で監査の入力を読み直し、報告が出ないことを確認

UBSan 版で実行して確かめた箇所は次のとおり。

| 場所 | 入力 |
|---|---|
| `src/core/shogiposition.cpp:327`（`++m_moveNumber`） | `position sfen <平手> b - 2147483647 moves 7g7f 3c3d`。後続の SFEN が `-2147483648` になり、読み直せない |
| `src/core/shogiposition.cpp:252`（`addToHand`） | BOD の「先手の持駒：歩2147483647 歩1」 |
| `src/kifu/formats/kiflexer.cpp:361` | BOD の「手数＝2147483647」の `+ 1` |
| `src/core/shoginotation.h:96`（`numberFromDigits`） | 「変化：99999999999手」、桁の多い消費時間 |
| `src/kifu/formats/csalexer.cpp:155` | `T9223372036854775807` の ×1000 |
| `src/kifu/formats/jkfmoveparser.cpp:131` | time の `m: 2147483647` の ×60 |
| `src/kifu/formats/usentosfenconverter.cpp:90` | 変化のオフセット 2147483647 の +1 |
| `src/network/csaclient.h`（`totalTime * unitMs()` など）、`csagameprogress.cpp:23-55` | Time_Unit:1min・Total_Time:100000 で `totalTimeMs()` が 1705032704 になる。Increment:2147483647 で −60000 になる（偽サーバーで再現） |

**疑い（コードで確認）**: `src/core/positionvalidator.cpp:37` の `totals[kind] += count`。

- SFEN の持ち駒は「99P」を繰り返して足せるので、合計が 2^31 を超えると負になり、駒数の上限の検査をすり抜ける。
- その後、`sfenvalidationservice.cpp:134`・`cliargs.cpp:26` の `*Position::fromSfen(...)` が空の optional を参照する。
- ただし約6500万文字の1行が要るので、現実性は低い。

**修正案**
- 読み込むところで上限を決めて拒否する（手数 ≤ 10万、持ち駒は1種 ≤ 99、時間は qint64 で24時間までなど）。
- `numberFromDigits` の桁数を制限する。
- `PositionValidator` の合計は qint64 で数える。
- `fromSfen` の結果は確かめてから参照する。
- 上限を超える Game_Summary は REJECT する。

### S14【Low】入力の大きさに上限がなく、特殊ファイルも読む

**状態: 済（7271f6bd）**。`BoundedFile::read`（`src/common/boundedfile.h`）で、棋譜は 64 MiB、局面集・詰将棋集は 256 MiB まで、通常のファイルだけを読む。定跡は大きなものも読めるよう大きさは制限せず、通常のファイルかだけ確かめる。自動化 API の `kifu.load` も通常のファイルに限る

- **場所**
  - `src/kifu/kifreader.cpp:140-150, 266` の `readAll()`
  - `csatosfenconverter.cpp:32`
  - `JosekiBookFile::load`
  - `src/automation/automationcommands_kifu.cpp:89-97`: 存在しか確かめない。
- **再現**: 自動化 API の `kifu.load` に FIFO の `.kif` を渡すと、GUI のメインスレッドが止まり、別の接続の `app.ping` も応答しなかった。ここに渡せるのは API を直接使う同じユーザーだけで、MCP は通常ファイルかを検査する。
- **その他**: iconv で変換するときは、入力の3倍のバッファを確保する（`src/kifu/shiftjiscodec.cpp:34`）。
- **修正案**
  - 通常ファイル以外を拒否する。
  - 棋譜は数十 MB、定跡はそれより大きい上限を置く。
  - 自動化からの読み込みもワーカースレッドで行う。

### S15【Low】`kifu.save` がリンク先のないシンボリックリンクをたどる

**状態: 済（7271f6bd・582a74ce）**。`OutputPath::check` はシンボリックリンク（リンク先が無いものも）を拒む。`kifu.save` は棋譜の拡張子の名前だけに保存する（`test_kifu_save_refuses_other_files`）

- **場所**: `src/headless/outputpath.cpp:13-18`（`exists` がリンクをたどる）、`src/kifu/kifuioservice.cpp:106`（`QSaveFile` もたどる）。
- **再現**: `sv/link.kif` → `sv/other/created.kif`（まだない）のリンクを置き、`overwrite` なしで `kifu.save` した。成功して、リンクの先にファイルができた。
- **MCP 経由の場合**: MCP は `resolve()` 後のパスを検査して渡す。そのため残るのは、検査と書き込みの間に差し替えられる場合（TOCTOU）だけである。
- **修正案**: lstat でリンクを拒否する。保存の拡張子を棋譜の形式に限る（現状は任意の名前を KIF で保存する）。

### S16【Low】実行ファイルの堅牢化フラグがない

**状態: 済（ed949d24）**。`ENABLE_HARDENING`（既定 ON）で `-fstack-protector-strong`・`-D_FORTIFY_SOURCE=3`・`-fcf-protection=full`・PIE・`-z relro -z now -z noexecstack`、MSVC は `/guard:cf`・`/CETCOMPAT`。同梱の Hayanagi にも付くよう `add_subdirectory(Hayanagi)` の前で設定する。`readelf` でアプリと Hayanagi に BIND_NOW・PIE・IBT/SHSTK・`*_chk` があることを確認。Windows・macOS は未確認

- **場所**: `CMakeLists.txt` に堅牢化のフラグがない。
- **確認**: `readelf` で `build/ShogiBoardQ` と AppImage の中の実行ファイルを見た。
  - PIE・NX スタック・`__stack_chk_fail` はある。
  - BIND_NOW（完全な RELRO）・`*_chk`（FORTIFY）・IBT/SHSTK（CET）はない。
  - 比較: Arch のパッケージは makepkg のフラグで、これらが付いている。
- **修正案**: GCC/Clang の Release に `-D_FORTIFY_SOURCE=3 -fstack-protector-strong -fcf-protection -Wl,-z,relro,-z,now` を付ける。MSVC は `/guard:cf`・`/CETCOMPAT` を付ける。

### S17【Low】AppImage に古いライブラリを同梱している

**状態: 済（ccf2a774）**。コンテナのイメージで、同梱する OpenSSL・libcurl と、libcurl が使う通信のライブラリを新しい版に替えた（OpenSSL 3.6.5・curl 8.23.0・nghttp2 1.70.0・nghttp3 1.18.0・ngtcp2 1.25.0・libssh2 1.11.1）。OpenSSL などは今の Arch のパッケージ（GLIBC_2.38 まで）を `scripts/linux-container/packages.sha256` の SHA-256 と署名で確かめて入れ、curl は今の Arch のパッケージが glibc 2.43 の関数を使うので、同じ版の Arch の PKGBUILD でイメージの中で作り直す（タグの署名は `curl-signing-key.asc` で確かめる）。OpenSSL をシステムのものに替えなかったのは、Qt 6.7 が既定でシステムのプロキシ設定を使って libproxy を読み、その libcurl が OpenSSL 3.5 以降の関数を使うため（Ubuntu 24.04 の OpenSSL 3.0 では起動できない）。配布物の glibc の要求は 2.38 のまま。あわせて次の2つを直した（どちらも前回のリリースからの問題）。
- Fedora では、同梱の上流版 OpenSSL が Fedora の設定ファイル（暗号ポリシー）を読めずに初期化に失敗し、通信対局の TLS が使えなかった。AppRun が `OPENSSL_CONF` で同梱の空の設定（`usr/share/ShogiBoardQ/openssl.cnf`）を読ませる。
- 動作条件の記述を、実際に要る GCC 14 の libstdc++（`CXXABI_1.3.15`）に合わせ、「Fedora 39 以降」を「Fedora 40 以降」に直した（Fedora 39 の libstdc++ は GCC 13 で起動できない）。

確認: 作った ZIP の AppImage を、Ubuntu 24.04（OpenSSL 3.0.13）・Debian 13（`libcom-err2` を入れたもの）・Fedora 40 のコンテナと手元の Arch で Xvfb 上で起動し、自己署名の TLS サーバーへの接続で証明書のエラーが出ること、その証明書をシステムの信頼ストアに入れるとハンドシェイクが通りログインの行が届くことを確かめた。読み込まれるのが同梱の OpenSSL・libcurl であることは `/proc/<pid>/maps` で確かめた。最小構成の Debian 13 のコンテナでは、krb5 が使う `libcom_err.so.2`（linuxdeploy が同梱しない）が無くて起動しない。これも前回のリリースからで、通常のデスクトップには e2fsprogs と一緒に入っている。Qt 6.7.2（サポート終了）は替えていない。

- **場所**: `scripts/linux-container/Dockerfile`。Arch の 2024-07-15 のスナップショットに固定している。
- **確認**: `build-container/AppDir/usr/lib` を調べた。
  - `libcrypto.so.3` は `OpenSSL 3.3.1 4 Jun 2024`、`libcurl.so.4` は `libcurl/8.8.0`、Qt は 6.7.2 だった。
  - RUNPATH `$ORIGIN/../lib` により、システムの OpenSSL ではなく同梱のものが使われる。
- **影響**: 3.3.2 以降の修正が入っていない（例: CVE-2024-6119。X.509 の名前検査の DoS）。CSA の TLS 接続で到達するかは確かめていない。Qt 6.7 はサポートが終わっている。
- **修正案**: スナップショットを定期的に更新する。glibc を古いままにする必要があれば、OpenSSL と Qt だけ新しくビルドする。

### S18【Low】ビルドのツールを検証なしで取得している

**状態: 済（ed949d24）**。`build-linux.sh` に3つのツールの SHA-256 を書き、取得した物もキャッシュも実行前に照らし合わせる（値は前回のリリースで使い、上流の continuous の digest と一致した版）

- **場所**: `scripts/build-linux.sh:41-43, 226-238`。linuxdeploy・linuxdeploy-plugin-qt・appimagetool を、GitHub の `continuous` リリースから `curl` で取り、チェックサムを見ずに実行する。
- **影響**: appimagetool のランタイムは、配布する AppImage に埋め込まれる。
- **修正案**: 版を固定し、SHA-256 で確かめる。

### S19【Low】MCP の引数が inputSchema で検査されない

**状態: 済（582a74ce）**。`server.check_arguments` が jsonschema で検査する（`pyproject.toml` に `jsonschema>=4` を足した）。テスト `test_arguments_are_checked_against_the_input_schema`

- **場所**: `mcp/shogiboardq_mcp/server.py:126-139`。mcp 2.3.0 の lowlevel Server は、ツールの引数を JSON Schema で検査しない。`docs/dev/mcp-server.md` §5 には「引数は inputSchema で検証され（SDK）」とある。
- **再現**（stdio のセッション）
  - `validate_sfen` に知らない引数を足しても受け付けた。
  - `convert_kifu` に `input_path: ""` を渡すと、`internal_error: KeyError` になった。
- **影響**
  - パスの検査は迂回できない。
  - MCP だけの上限（`max_chars`・`max_positions`・`max_pv_moves`）や、`verify_tsume` の `timeout_seconds` を50に狭めた指定は効かない。
  - CLI の表で検査される型の誤りは、invalid_argument になる。
- **修正案**: `dispatch_tool` で jsonschema による検査を行う。

### S20【Low】MCP の資源の上限

**状態: 済（582a74ce）**。`run_cli` は同時に4つまで、標準出力は 64 MiB まで読み、取り消し・時間切れ・読み過ぎで CLI を終わらせる。`convert_kifu` は手の一覧を `max_moves`（既定 2000）で切り `moves_truncated` を返し、対局情報と警告は200件まで

- **同時実行**: `cli_backend.py:59-84` の `run_cli` は、ジョブの上限（4）に入らない。`verify_tsume` などを並列に呼ぶと、CLI とエンジンが数の制限なしに起動する。
- **キャンセル**: `asyncio.wait_for(proc.communicate())` は、CancelledError で子プロセスを kill しない。
- **出力**: `handlers_cli.py:77-79` の `usi_moves`・`sfens`・`moves` は `max_chars` の対象外で、全件を返す。`cli_backend.py:73` は stdout を上限なしで読む。
- **修正案**
  - `run_cli` を共通のセマフォで制限する。
  - キャンセルされても kill して wait する。
  - 配列は件数で切り、`truncated` を返す。

### S21【Low】自動保存のファイル名の検査が足りない

**状態: 済（7271f6bd）**。制御文字（NUL を含む）・文字の向きを変える制御文字を置き換え、名前の部分を40文字で切る（テスト `defaultSaveFileNameIsSafe`）

- **場所**: `src/kifu/kifuioservice.cpp:17-27`（`sanitizeForFileName`）、`src/ui/coordinators/kifufilecontroller.cpp:254-280`。
- **再現**
  - 名前が300文字だと、ファイル名が 327 バイトになり、自動保存が失敗した。
  - NUL を含む名前では、NUL の位置でパスが切れ、拡張子のない `20261023_120000_` ができた。
  - U+202E（RLO）や、Windows で使えない制御文字もそのまま通る。
  - パストラバーサルは起きない（`/` `\` などを置き換え、先頭に日時を付ける）。
- **修正案**: 制御文字と双方向制御文字を除き、名前の部分を例えば64文字で切る。

---

## 3. Info

**状態**: 次の3つは直した（エンジンへの送信と旧版のパスワードは 96e4096e、パイプ名は 582a74ce）。ほかは記録だけ。
- エンジンへの送信: `EngineProcessManager::sendCommand` は改行を含むコマンドを送らない。
- Windows のパイプ名: MCP サーバーは名前に `\\.\pipe\` を付けてつなぐ（`AppClient.pipe_address`。Windows では未確認）。
- 旧版の平文パスワード: 起動時に `CsaGameSettings/password` を消す（`main.cpp`）。

- **エンジンへの送信の多層防御**: `src/engine/engineprocessmanager.cpp:27-43`（`sendCommand`）は改行を検査しない。外部の入力からは届かないが、エンジン設定の文字欄と USI コンソールは、貼り付けた改行をそのまま送る。`\r` と `\n` を含むコマンドを拒否するとよい。
- **CSA の開始局面**: `PositionValidator` で検査していないので、不正な局面がエンジンに送られ得る。
- **Windows のパイプ名**（疑い・未検証）: `automationserver.cpp:41-43` は `\\.\pipe\` を付けない名前を返し、`app_client.py:74-80` はそれをそのまま `create_pipe_connection` に渡す。既定で段階2が動かない可能性がある。名前が予測できるので、他ユーザーに先取りされないかも確かめが要る。
- **TOCTOU・ハードリンク**
  - MCP の検査とアプリ・CLI の書き込みの間に、パスの途中を差し替えられる余地がある。制限モードにはリンクを作るツールがないので、エージェント単独では起こせない。
  - 許可ディレクトリの中の、外を指すハードリンクは検出できない。
- **`kifu.load` の応答**: 警告つきで読めたときに、棋譜を置き換えたうえで parse_error を返す（`automationcommands_kifu.cpp:98-104`）。
- **制限モードで読めるもの**: `widget.text` は名前を指定すれば隠れた部品も読める。`app.state`・`menu.items` からは、最近使ったファイルのパスが分かる。
- **旧版の平文パスワード**: 旧版が保存した `CsaGameSettings/password` は、CSA のダイアログを開くまで設定ファイル（0644）に残る（`csagamedialog.cpp:375, 385` で消す）。起動時に消すとよい。
- **盤面画像の保存**: `boardimageexporter.cpp:77-92` は原子的に書かない。拡張子を補った後のパスは、上書きの確認を通らない。
- **CI・配布のその他**
  - `release.yml` の `TAG="${{ inputs.tag }}"` は式をシェルに直接埋め込む。起動できるのは書き込み権限のある人だけなので、影響は小さい。
  - `ci.yml` にトップレベルの `permissions` がない。
  - `actions/cache@v6`・`actions/download-artifact@v8` はタグでの指定（ほかは SHA で固定）。
  - ベースイメージはダイジェストで固定していない。
  - 配布物は署名していない。Windows は Authenticode なし、macOS はアドホック署名で公証なし。
  - `packaging/arch/PKGBUILD` は `sha256sums=('SKIP')`。

---

## 4. 調べて問題がなかった点

### 自動化 API

- **ソケットの権限**（実行で確認）
  - 既定の場所は `/run/user/1000/shogiboardq/automation.sock`。
  - Qt 6.12 の `UserAccessOption` は 0700 の一時ディレクトリの中で bind→chmod→rename するので、作った時点から `srwx------` である。
- **エンドポイントファイル**（実行で確認）: 0600 で、中身は socket・pid・version だけ。
- **有効化**（コードで確認）: `--automation` のときだけ有効になり、TCP は開かない。
- **メッセージの上限**（実行で確認）
  - 改行なしの 9MiB は -32600 で切断する。
  - 5000段のネストは -32700、バッチは拒否する。
- **引数の検査**（突き合わせで確認）: 31 のメソッドがすべて引数の表で検査される。行・列は、実行の直前に `QPersistentModelIndex` で確かめ直す。
- **許可リスト**（実行で確認）: `actionQuit` と、ツールバーの上書き保存ボタンへのクリックは not_allowed になる。
- **外部 URL**（コードで確認）: `openUrl` を呼ぶのはヘルプ（許可リスト外）と、コメントの Ctrl+クリックだけ。
- **パスワード**（コードで確認）: CSA のパスワード欄は読めない。表示の切り替えも、平文表示中のスクリーンショットも拒否する。
- **再入**（コードで確認）: 処理中の別の要求は拒否する。処理中に切断したソケットは、処理が戻ってから消す。

### MCP サーバー

- **パスの拒否**（実行で確認）
  - 相対パス・`~`・空白で始まるパス。
  - `..` を含むパスと、前方一致だけの別ディレクトリ。
  - ファイル・ディレクトリへのシンボリックリンク、壊れたリンク。
  - ディレクトリ・FIFO・親のないパス。
- **エラー文**（実行で確認）: 許可の外は、存在するかどうかに関係なく同じ not_allowed になり、リンク先は出さない。
- **パスの検査の漏れ**（コードで総当たり）: パスを受けるツールはすべて `paths.py` を通る。CLI には検査済みの値を `--params-json` で渡し、shell は使わない。
- **USI の注入**（実行で確認）: SFEN・手・エンジン名に改行を入れると、invalid_argument か engine_not_found になる。偽エンジンの stdin に余計な行は届かなかった。
- **制限モード**（コードで確認）: 汎用の7ツールは、ハンドラの前に `require_unrestricted_ui` で検査される。
- **ジョブ**（コードで確認）: stdout の1行は 64KiB まで、イベントは200件まで。止めるときは stop → terminate → kill の順。アプリの応答は 8MiB まで。
- **text 入力の一時ファイル**（コードで確認）: `mkstemp`（0600）で作り、必ず消す。

### パーサ（ASan+UBSan 版で実行）

- **SFEN**: 筋の数・空きマスの数・成れない駒の `+`・行き所のない駒・持ち駒の2桁を検査している。盤の外への `setPieceAt`・`pieceAt` は無視される。
- **USI の手**: マスの範囲と打てる駒を検査する。空のマスや手番でない駒を動かす手は拒否する。
- **fmv との境界**: 筋・段を検査し、`MoveList` は600件で止まり、玉のない局面も扱える。
- **KIF・KI2・BOD・CSA・JKF・USEN**: 移動元・段・P 行・PI・JKF の座標と色・USEN の番号（46655 以下）を検査している。JKF の再帰は Qt の JSON の深さ上限（1024）で抑えられている。
- **定跡**: 16ビットの番号は 0x8000 未満に収まり、読めない文字列の表も 0x7FFF 件で止まる。
- **分岐ツリーの解放**: 反復で行うので、深い木でも再帰しない。
- **文字コード**: Shift_JIS の iconv は出力の残りを守る。BOM・UTF-16・UTF-32 も判別する。
- **正規表現**: 対局情報・形式の判定とも、長い行で爆発しなかった。
- **ASan の結果**: 上の入力（オーバーフロー・深い手順・大量の変化・長い行）で、スタックオーバーフロー（S5）以外の報告はなかった。

### USI・CSA

- **position**: すべて型付きの `UsiPosition` から作り直す（`usiposition.cpp:81-93`）。
- **setoption**: 値の出どころは、設定ファイル・数値・固定の定数だけ。BookFile のようなパスのオプションを外部入力から設定する経路はない。
- **エンジンの起動**: `QProcess::start(path, {})` でシェルを通さない。パスの出どころは登録済みの設定だけ。
- **bestmove・ponder**: `UsiMove::parse` を通す。CSA への送信も型付きの値から作る。
- **CSA の受信**: 1行 64KB、Game_Summary は 2MB・20000行まで。
- **CSA の送信**: ID・パスワード・ゲーム名は、正規表現（空白なし・ASCII・32文字まで）で検査する。
- **パスワード**（実行で確認）: 保存しない。通信ログ・待機ダイアログ・debug.log では伏せる。
- **TLS**: VerifyPeer・TLS1.2 以上で、`ignoreSslErrors` はない。暗号化が済むまで LOGIN を送らない。平文接続の警告をダイアログに出す。

### GUI・ファイル

- **棋譜コメント欄**: 全体を `toHtmlEscaped` し、リンクは http・https・ftp だけを Ctrl+クリックで開く。`setAcceptRichText(false)` である。
- **エスケープ済みのツールチップ**: 対局情報、棋譜欄のコメント、分岐ツリー、読み筋、盤の対局者名。
- **プレーンテキストで描くもの**: 評価値グラフ、USI・CSA の通信ログ、ステータスバー、貼り付けダイアログ。
- **保存**: 棋譜・定跡・詰将棋集は `QSaveFile` で書き、失敗したときは元のファイルを壊さない。
- **ログ**: Release ビルドはファイルにも stderr にもログを出さない。
- **CI**: `pull_request_target`・`workflow_run`・`issue_comment` はない。Qt のソースのキャッシュは SHA-256 で確かめ直す。
- **AppImage**: RUNPATH はすべて `$ORIGIN` 基準で、`LD_LIBRARY_PATH` を設定しない。
- **公開サイト**: 外部のスクリプトを読み込まない。

---

## 5. 直す順序の提案

1. **S1・S2・S3（MCP の閉じ込め）**
   - 既定の許可ディレクトリ、ドットで始まる要素の拒否、入出力の拡張子の制限、クリップボードの非制限化を行う。
   - High はこれだけで、外部の利用者に影響する。ガイドと README の既定の記述も直す。
2. **S4・S12（自動判定の表示）**: `NotificationPresenter` と待機ダイアログを PlainText にし、定跡のツールチップをエスケープする。小さい変更で塞げる。
3. **S7（リリース CI）**: 次のリリースの前に直す。
4. **S5・S6・S14（棋譜の読み込みの上限と計算量）**: 再帰を反復にし、ビルダーと `latestBranchPoint`・`splitStatements` の計算量を直し、大きさの上限を置く。
5. **S9・S13・S21（値の検査）**: 書き出しで改行を除き、数の上限を決め、ファイル名の制御文字を除く。
6. **S10・S11・S15（ファイルとソケットの扱い）**、**S16〜S18（配布）**、**S8・S19・S20（文書の食い違いと MCP の資源）**。
