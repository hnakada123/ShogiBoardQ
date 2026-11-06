# Windows・macOS の画面の撮り直しの依頼

クロスプラットフォームのガイド（`guide/multi-os.html`）の Windows・macOS の画像は、思考タブの情報欄が見出し付きの表だった頃の
表示のままになっている（2026-11-06 の変更で、手番の印・エンジン名と「項目名 値」を並べた1行の帯になった）。
Linux の画像は Linux の PC で撮り直し済みで、この2種類だけは各 OS の PC でないと撮れない。

## 依頼のしかた

Windows・macOS の PC で、次の順に行う。2台の作業は別々の日でも、どちらが先でもよい。

1. リポジトリを最新にする。

   ```
   git pull --recurse-submodules
   ```

2. Claude Code に次の依頼を貼り付ける。

   **Windows の PC:**

   ```
   /guide-page docs/dev/multi-os-screenshot-prompts.md を読み、「共通の手順」と「Windows の手順」に従って、multi-os.html の Windows の画像を4言語で撮り直してください。
   ```

   **macOS の PC:**

   ```
   /guide-page docs/dev/multi-os-screenshot-prompts.md を読み、「共通の手順」と「macOS の手順」に従って、multi-os.html の macOS の画像を4言語で撮り直してください。
   ```

3. 報告と画像を確認してから、「コミットとプッシュをお願いします」と依頼する。

## 共通の手順

ここから下は、依頼を受けた Claude Code への指示。この文書は Linux の PC で書いた。Linux の PC にあるメモリはこの PC では読めないので、
必要なことはここにまとめてある。返答とコミットメッセージは日本語で書く。

### 作るもの

4言語それぞれの画面を撮り、同じ名前で上書きする。ページの文章は変えない。

| 言語 | UI の言語（`language`） | Windows | macOS | ページ |
|---|---|---|---|---|
| 日本語 | `ja_JP` | `docs/images/multi-os/Windows.png` | `docs/images/multi-os/macOS.png` | `docs/guide/multi-os.html` |
| 英語 | `en` | `docs/images/en/multi-os/Windows.png` | `docs/images/en/multi-os/macOS.png` | `docs/en/guide/multi-os.html` |
| 簡体字 | `zh_CN` | `docs/images/zh-cn/multi-os/Windows.png` | `docs/images/zh-cn/multi-os/macOS.png` | `docs/zh-cn/guide/multi-os.html` |
| 繁体字 | `zh_TW` | `docs/images/zh-tw/multi-os/Windows.png` | `docs/images/zh-tw/multi-os/macOS.png` | `docs/zh-tw/guide/multi-os.html` |

### 撮る画面

作業の前に、今の画像を開いて構図を確かめる。次の状態のウィンドウを、OS のタイトルバーごと撮る。

- 人間（先手）と Hayanagi（後手）の平手の対局。持ち時間は10分、秒読みなし。人間の名前は既定のまま（日本語は「あなた」）
- 8〜10手ずつ指した後の、**Hayanagi の手番で思考している最中**（ページの説明が「Hayanagiが思考中」なので）
- 下のドックは「思考」タブ
- 情報帯に「△」の印とエンジン名、探索手・深さ・ノード数が出ている
- 思考の表が6行以上あり、読み筋が数手ぶん以上の長さで並んでいる

幅が足りないと情報帯は2段に折り返す。これは正常な表示なので、そのまま撮る。

### 手順

1. 今の `main` をビルドする。手順は「各 OS の手順」のとおり。`git log -1` で、`a295290c` 以降であることを確かめる。

2. 言語ごとに、空の設定フォルダを作業フォルダ（scratchpad）に作り、`<設定フォルダ>/ShogiBoardQ/ShogiBoardQ.ini` を次の内容で書く。
   普段の設定を書き換えないよう、普段の場所は使わない。

   ```ini
   [%General]
   language=ja_JP

   [SizeRelated]
   mainWindowSize=@Size(1300 980)

   [Engines]
   1\author=hnakada123
   1\name=Hayanagi 1.5.1
   1\path=<hayanagi の実行ファイルの絶対パス>
   size=1
   ```

   - General グループは `[%General]` と書く（`[General]` だと言語の設定が効かない）
   - 英語はウィンドウの項目名が長いので `mainWindowSize=@Size(1400 1000)` にする。横幅は実際には中身に合わせて決まる
   - パスの区切りは `/` にする（Windows でも）

3. アプリを `--automation --automation-socket <接続先>` 付きで起動する。環境変数 `SHOGIBOARDQ_CONFIG_HOME`・`SHOGIBOARDQ_DATA_HOME`・
   `SHOGIBOARDQ_CACHE_HOME` を言語ごとの作業フォルダに向ける（`XDG_CONFIG_HOME` などは Linux の Qt しか見ない）。
   起動してから接続できるようになるまで数秒待つ。

4. 自動化 API（改行区切りの JSON-RPC 2.0）で対局を始める。使い方は `scripts/docs/rpc.py`、手順の見本は
   `scripts/docs/shoot_tables.py` の `scenario_game`、メソッドの一覧は `docs/dev/mcp-server.md`。

   ```
   widget.showDock {"widget":"ThinkingDock"}
   action.trigger  {"name":"actionStartGame"}
   widget.setValue {"target":"StartGameDialog","widget":"comboBoxPlayer1","value":0}    先手は人間
   widget.setValue {"target":"StartGameDialog","widget":"comboBoxPlayer2","value":1}    後手は Hayanagi
   widget.setValue {"target":"StartGameDialog","widget":"basicTimeHour1","value":0}
   widget.setValue {"target":"StartGameDialog","widget":"basicTimeMinutes1","value":10}
   widget.setValue {"target":"StartGameDialog","widget":"byoyomiSec1","value":0}
   dialog.clickButton {"target":"StartGameDialog","text":"対局開始"}
   ```

   ボタンの文字は日本語「対局開始」、英語「Start Game」、簡体字「开始对局」、繁体字「開始對局」。

5. 人間の手は `scripts/docs/humanplay.py <接続先> <手数> <1手の思考時間 ms>` で指す（別に起動した Hayanagi に手を選ばせ、
   `board.click` で盤に指す）。環境変数 `SBQ_HAYANAGI` に hayanagi の実行ファイルのパスを渡す。まず10手指す。

6. Hayanagi の手番の間、`widget.text {"widget":"thinkingView1","max_rows":3}` を短い間隔で読む。`app.state` の `sfen` の手番が `w` で、
   `row_count` が6以上、読み筋が40文字以上になったら、すぐに撮る。読み筋は `rows` の各行の最後の列で、`rows` の1行目は見出しなので
   2行目から見る。

   Hayanagi は定跡を持っていて、序盤は読み筋が短いまま、または表が空のまま指すことがある。条件を満たす前に Hayanagi が指したら、
   `humanplay.py` で人間の手を1手足して、もう一度待つ。

7. 撮った画像を開いて、「撮る画面」のとおりかを自分の目で確かめる。指している途中の状態や別の言語が写っていたら撮り直す。

8. 4言語が揃ったら、画像の大きさが変わった分だけ、4つのページの `<img>` の `width`・`height` を実際の大きさに直す（ほかの箇所は変えない）。

9. 確認する。

   ```
   python3 scripts/docs/check_links.py guide/multi-os.html
   ```

   ブラウザで4言語のページを開き、画像が正しい言語で表示されること、スマートフォン幅でもはみ出さないことを見る。

10. 起動したアプリとエンジンをすべて止める。普段の設定に書き込みが無いことを確かめる。場所は `shogiboardq-cli environment` の
    `settings_file` で分かる（環境変数 `SHOGIBOARDQ_CONFIG_HOME` を付けずに実行する）。作業の前に更新時刻を控えておき、比べる。

11. 変えたことと確認の結果を報告する。**コミットはしない。** 頼まれてから行う。

### 注意

- `scripts/docs/launch.sh`・`shoot_tables.py`・`kwin_capture.sh`・`kwin_x_capture.sh` は Linux 専用（Xvfb と KDE を使う）なので使わない。
  アプリは直接起動する。`shoot_tables.py` は手順の見本として読むだけにする
- 自動化 API の `screenshot.capture` はクライアント領域だけを撮るので、タイトルバーが写らない。撮影は OS の機能で行う
- 画像に写るエンジンの数値は撮るたびに変わる。前回の画像と同じにする必要はない
- `APP_VERSION` は変えない（リリースの日にだけ上げる）
- 確認中に不具合を見つけたら、その場で直してテストを足し、画像とは別に報告する。コードを変えたら `ctest` も実行する
- どうしても自動で動かないとき（権限が無い、接続できないなど）は、原因を報告して、手動で撮る方法を提案する。自動の手順を無理に
  続けて、普段の設定や普段の画面を書き換えない

### コミットを頼まれたら

- OS ごとに1つのコミットにする。件名は「クロスプラットフォームのガイドの Windows の画面を4言語で撮り直す」（macOS も同じ形）
- メッセージは「件名 → 空行 → 本文 → 空行 → Co-Authored-By」の順に日本語で書く
- `main` に直接コミットしてよい。`main` は force push できないので、プッシュの前に `git log -1 --format='[%s]'` で件名が1行に
  なっていることを確かめる
- もう1台の PC の作業が先にプッシュされていることがあるので、プッシュの前に `git pull --rebase` する（同じ `multi-os.html` でも、
  直す行が別なので衝突しないはず）

## Windows の手順

ビルドは `docs/dev/windows-build-and-release.md` のとおり。リポジトリのルートで PowerShell から実行する。

```powershell
.\scripts\build-windows.ps1 -SkipZip
```

- **アプリ:** `build` の下の `ShogiBoardQ.exe`（出力先は生成器によって `build\Release` などになる）
- **Hayanagi:** `build\Hayanagi\hayanagi.exe` か `build\Hayanagi\Release\hayanagi.exe`
- **接続先:** `--automation-socket sbq-shot` のように名前だけを渡す。実際の接続先は名前付きパイプ `\\.\pipe\sbq-shot` になる

`scripts/docs/rpc.py`・`humanplay.py` は Unix ソケット（`socket.AF_UNIX`）専用で、Windows では動かない。リポジトリのファイルは変えず、
作業フォルダに写して、名前付きパイプに接続するように直して使う。

```python
# 1回の要求ごとに開き、JSON を1行書いて、改行まで読む
with open(r"\\.\pipe\sbq-shot", "r+b", buffering=0) as pipe:
    pipe.write((json.dumps(request) + "\n").encode())
    ...
```

Windows の名前付きパイプでの接続は `mcp/shogiboardq_mcp/app_client.py` に実装があるが、実機では未検証。動かなければ `app_client.py` の
`pipe_address` と `_open` を参考にする。

撮影:

- 前回の画像は Windows 11、表示スケール150%で、大きさは日本語 1938×1517、英語 2059×1547、中国語 1933×1517。スケールが違えば
  大きさも変わるので、その場合はページの `width`・`height` を実際の大きさに直す
- ウィンドウを最前面に出してから、タイトルバーを含むウィンドウの範囲を切り抜く。PowerShell なら、プロセスを DPI 対応にし
  （`SetProcessDPIAware`）、`DwmGetWindowAttribute` の `DWMWA_EXTENDED_FRAME_BOUNDS`（値は 9）で範囲を取り、`Graphics.CopyFromScreen`
  で撮る。`GetWindowRect` の範囲は見えない枠を含むので使わない
- 撮影の間はほかのウィンドウを重ねない。Windows Terminal などがウィンドウに重なっていると、そのまま写る

## macOS の手順

ビルドは `docs/dev/macos-build-and-release.md` のとおり。

```bash
./scripts/build-macos.sh --skip-dmg
```

- **アプリ:** `build/ShogiBoardQ.app/Contents/MacOS/ShogiBoardQ`
- **Hayanagi:** `build/Hayanagi/hayanagi`
- **接続先:** `--automation-socket <作業フォルダ>/sbq.sock`。Unix ソケットのパスは約100バイトまでなので、作業フォルダが長ければ
  `/tmp` の下の短いパスにする
- `scripts/docs/rpc.py`・`humanplay.py` はそのまま使える

撮影:

- 前回の画像は Retina（2倍）で、影なし、四隅は透過。大きさは日本語 1942×1560、英語 2070×1560、簡体字 1925×1561、繁体字 1925×1564
- 画面の高さに収まらないと、ウィンドウは指定より小さくなる（前回の高さは約780pt）。それでよい
- `screencapture -o -l <ウィンドウ ID> <出力先>.png` で撮る。`-o` で影を付けない。ウィンドウ ID は `CGWindowListCopyWindowInfo`
  （swift か pyobjc）で、所有者が ShogiBoardQ のウィンドウから取る
- 画面収録の権限が無いと、`screencapture` は壁紙だけを写すか失敗する。そのときは「システム設定 → プライバシーとセキュリティ →
  画面収録」で、実行しているターミナルの許可を利用者に頼む
