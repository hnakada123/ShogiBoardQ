# shogiboardq-mcp

ShogiBoardQ の機能を [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) 経由で AI クライアント
（Claude Desktop / Claude Code / Cursor / VS Code / Gemini CLI / Codex CLI など）から使うためのサーバーです。
トランスポートは stdio で、`python -m shogiboardq_mcp` で起動します。

設計の詳細は [docs/dev/mcp-server.md](../docs/dev/mcp-server.md)、利用手順は
[docs/guide/mcp-server.html](../docs/guide/mcp-server.html) を参照してください。

## 必要なもの

- Python 3.10 以上と `mcp` パッケージ（`pip install mcp`、または `pip install -e mcp` でこのパッケージごと）
- ビルド済みの ShogiBoardQ（`build/shogiboardq-cli` と `build/ShogiBoardQ`）
- エンジンを使うツールには、ShogiBoardQ の「エンジン設定」で登録した USI エンジン

## 環境変数

| 変数 | 意味 |
|---|---|
| `SHOGIBOARDQ_CLI` | `shogiboardq-cli` のパス。未指定なら `SHOGIBOARDQ_EXECUTABLE` と同じディレクトリ、次に `PATH` を探します |
| `SHOGIBOARDQ_EXECUTABLE` | `ShogiBoardQ` のパス。アプリ操作系ツールで起動中のアプリが無いとき `--automation` 付きで起動します |
| `SHOGIBOARDQ_AUTOMATION_SOCKET` | 自動化ソケットのパス（アプリ側の `--automation-socket` と共通） |
| `SHOGIBOARDQ_ALLOWED_DIRS` | 読み書きを許可するディレクトリ（`:` 区切り、Windows は `;`）。既定はホームディレクトリ |
| `SHOGIBOARDQ_OUTPUT_DIR` | スクリーンショットなど出力先未指定時のディレクトリ。既定は一時ディレクトリ配下の `shogiboardq-mcp` |
| `SHOGIBOARDQ_QUIT_APP_ON_EXIT` | `1` のとき、サーバーが起動したアプリをサーバー終了時に閉じ、終了を待ちます。応答しない場合はその子プロセスだけを強制終了します（既定は残す） |
| `SHOGIBOARDQ_MCP_DEBUG` | `1` で詳細ログを標準エラーに出します |

## ツール一覧

アプリを起動しなくても使えるもの（`shogiboardq-cli` 経由）:

| ツール | 内容 |
|---|---|
| `convert_kifu` | 棋譜ファイル／文字列を KIF・KI2・CSA・JKF・USI・USEN・SFEN 列に変換 |
| `validate_sfen` | SFEN の妥当性、手番、持駒、王手、合法手数 |
| `list_engines` | 登録済み USI エンジン |
| `analyze_position` / `analysis_status` / `analysis_result` | エンジン解析（ジョブ方式、MultiPV 対応） |
| `analyze_kifu` / `kifu_analysis_status` / `kifu_analysis_result` | 棋譜の本譜を連続解析。範囲指定・進捗・手数別評価と読み筋・結果のページ取得・取消に対応 |
| `search_mate` / `mate_status` | `go mate` による詰み探索（ジョブ方式） |
| `generate_tsume` / `tsume_generation_status` / `stop_tsume_generation` | 詰将棋局面の自動生成（ジョブ方式） |
| `verify_tsume` | 余詰検査（unique / multiple / nomate / wrong_length / unknown） |
| `render_board_image` | SFEN から盤面 PNG を生成 |
| `list_jobs` / `cancel_job` | ジョブの一覧と取消 |

起動中のアプリ（`ShogiBoardQ --automation`）を操作するもの:

| ツール | 内容 |
|---|---|
| `get_app_state` | UI 状態・現在手数・棋譜ファイル・未保存・開いているダイアログ。CSAは接続状態・手番・サーバー確認済みの残時間も取得 |
| `get_position` / `set_position` | 現在局面の取得と設定 |
| `load_kifu` / `save_kifu` / `get_kifu` / `goto_ply` | 棋譜の読み込み・保存・取得・手数移動 |
| `list_actions` / `trigger_action` | メニュー動作の一覧と実行（許可リスト方式） |
| `click_board_square` | 盤面・駒台のクリック。回転・盤サイズに追従し、通常のマウス入力として処理 |
| `show_dock` | 検討・思考などのドックを表示し、タブ化されている場合も前面へ移動 |
| `list_docks` / `configure_dock` | 全ドックの状態確認、切り離し、四方向への再配置、タブ化、表示・非表示、タイトルバーのドラッグ |
| `click_widget` / `set_widget_value` / `click_table_cell` | 名前またはselectorで部品を指定。ボタン、コンボ・数値・スライダー・チェック・リスト・色・テキスト、棋譜・読み筋表の操作 |
| `list_menu_actions` / `select_menu_action` | 定跡・局面集の履歴、マージ、保存済みレイアウトのメニューを取得・実行 |
| `menu_favorites` | お気に入りの取得・登録・解除・並べ替えと保存 |
| `click_branch_node` | 分岐ツリーのノードへ移動 |
| `edit_table_cell` | 対局情報などの編集可能なセルを通常の入力欄から編集。`commit=false` で入力中の状態も検証可能 |
| `get_clipboard` | コピーされたテキスト・画像の有無・画像サイズを取得（テキストの上限指定可） |
| `click_dialog_button` | ダイアログのボタン操作。問題選択、成り選択、再生・復帰など |
| `capture_screenshot` | メインウィンドウ／ダイアログのスクリーンショット |
| `list_dialogs` / `get_widget_text` | 開いているダイアログ、テキスト・テーブル・盤面内容とウィジェット座標 |

リソース: `shogiboardq://position/current`（SFEN）、`shogiboardq://kifu/current`（KIF）、`shogiboardq://engines`（JSON）。

詰将棋対局では `click_board_square(target="tsumePlayDialog", file=3, rank=3)`、
続けて `file=5, rank=3` と指定すると３三から５三へクリックします。座標は盤の回転に依存しません。
成り選択は `click_dialog_button(dialog="成りの選択", text="成る")` で回答します。
駒台は先手が `file=10`・`rank=1..8`（歩・香・桂・銀・金・角・飛・玉）、
後手が `file=11`・`rank=9..2`（同順）です。`button="right"` は選択を取り消します。
ボタンは `widget`（objectName）か `text`（表示文字列の完全一致）で指定し、
同名の問題カードは `index`（0始まり）で選択できます。
クリックは予約後に応答するため、着手の成否や探索完了は `get_widget_text` などで確認してください。
非表示・無効な対象や、別のモーダルダイアログに遮られた対象は操作しません。

CSA待機画面の「通信ログ」を開いた後、次の操作でコマンドを送信できます。
`submit=true` は単一行のテキスト入力欄だけに対応し、通常の Enter キー入力として処理します。
パスワード表示を無効にした入力欄は読み取り時に伏せ字となり、書き込みも拒否します。

```text
set_widget_value(target="csaWaitingLogWindow", widget="csaWaitingCommandInput", value="LOGOUT", submit=true)
```

検討タブは次のように操作できます。値変更・クリックは通常のUIシグナルを通り、
処理を予約して応答します。開始・停止・エンジン切替の完了は `get_widget_text` で確認してください。

```text
show_dock(widget="ConsiderationDock")
set_widget_value(widget="considerationEngine", value="登録したエンジン名")
set_widget_value(widget="considerationMultiPV", value=2)  # 0始まり: 3候補
set_widget_value(widget="considerationUnlimited", value=true)
click_widget(widget="considerationStartStop")
get_widget_text(widget="considerationView")
click_table_cell(widget="considerationView", row=0, column=4)  # 読み筋の盤面
```

時間制限を使う場合は開始前に `considerationTimed=true`、`considerationSeconds=秒数` を設定します。
`considerationArrows` は矢印表示、`considerationFontIncrease` / `considerationFontDecrease` は文字サイズです。
`get_widget_text` の盤面データには矢印の移動元・移動先・順位・駒種も含まれます。

ドック配置は次のように検証できます。`list_docks` は閉じたドックも返します。
`hidden` は閉じた状態、`exposed` は実際に表示領域を持つ状態です。
タブの裏にあるパネルは `visible=true` でも `exposed=false` になります。

```text
list_docks()
configure_dock(widget="EvalChartDock", operation="float",
               geometry={"x": 100, "y": 100, "width": 700, "height": 400})
capture_screenshot(target="EvalChartDock")
configure_dock(widget="EvalChartDock", operation="dock", area="bottom")
configure_dock(widget="EvalChartDock", operation="tabify", relative_to="ThinkingDock")
configure_dock(widget="EvalChartDock", operation="hide")
show_dock(widget="EvalChartDock")
trigger_action(name="actionResetDockLayout")
```

`operation="drag", x=..., y=...` はドッキング中の表示されたパネルのタイトルバーから、
画面全体での論理座標へマウスイベントを送ります。OSが描画する浮動ウィンドウのタイトルバーは対象外です。
それ以外の配置操作はQtのドックAPIを使います。移動はドック固定・許可エリアの制約を守り、
モーダルダイアログ表示中は操作しません。処理は予約されるため、完了は `list_docks` で確認します。
`actionSaveDockLayout` で保存ダイアログも開けます。

対局情報は `show_dock(widget="GameInfoDock")` で表示し、
`get_widget_text(widget="gameInfoTable")` で行を確認できます。
`edit_table_cell(widget="gameInfoTable", row=1, column=1, text="対局者名")` は0始まりの行・列を編集します。
`gameInfoApply`、`gameInfoUndo`、`gameInfoRedo`、`gameInfoCut`、`gameInfoCopy`、`gameInfoPaste`、
`gameInfoAddRow`、`gameInfoFontIncrease`、`gameInfoFontDecrease` は `click_widget` で操作できます。
浮動ウィンドウでは `target="GameInfoDock"` を指定します。
`get_widget_text(widget="blackNameLabel")` / `whiteNameLabel` は盤面に表示される対局者名の全文を返します。

## 無名の部品・メニュー・棋譜全体解析

`get_widget_text` と `list_dialogs` が返す `selector` を `widget` / `target` / `dialog` に指定すると、
名前のないボタンや入力欄も操作できます。同名の部品が複数ある場合も selector を使ってください。
ダイアログ再生成・再起動後には取り直します。`include_children=true` で指定部品内を列挙できます。

```text
get_widget_text(widget="josekiTable")
list_menu_actions(widget="josekiMergeMenu")
select_menu_action(widget="josekiMergeMenu", path=[0])  # 0始まりの階層インデックス
menu_favorites(actions=["actionStartGame", "actionAnalyzeKifu"])
get_widget_text(widget="branchTreeView")
click_branch_node(widget="branchTreeView", id=取得したノードID)
analyze_kifu(engine="登録したエンジン名", input_path="/absolute/path/game.kif", seconds_per_position=1)
kifu_analysis_status(job_id="返されたID", offset=0, max_positions=100)
kifu_analysis_result(job_id="返されたID", offset=100, max_positions=100)
```

全体解析は本譜の開始局面から終局面まで（`from_ply` / `to_ply` で両端を含む範囲を指定可能）。
`lines` は手番側の評価、`score_cp_black` / `score_mate_black` は先手側の評価です。
`cancel_job` で中断しても、それまでの結果を取得できます。GUIへの結果反映は行いません。

言語は `actionLanguageSystem` / `actionLanguageJapanese` / `actionLanguageEnglish` で変更し、再起動後に反映されます。
名前付きレイアウトは `actionSaveDockLayout` の入力欄を selector で指定して保存し、
`menuSavedLayouts` から復元・削除・起動時指定を操作します。
定跡・コメント・しおり・駒音・配色・エンジン選択などの対応表は
[実装と検証の記録](../docs/dev/mcp-coverage-implementation-2026-09-28.md) を参照してください。

## クライアント設定例

いずれも「`python3 -m shogiboardq_mcp` を stdio で起動し、環境変数で実行ファイルの場所を渡す」だけです。
`PYTHONPATH` は `mcp/` ディレクトリ（このファイルのある場所）を指すか、`pip install -e mcp` でインストールしてください。
以下は Linux の例で、`/path/to/ShogiBoardQ` はリポジトリの場所に置き換えます。

### Claude Code

```bash
claude mcp add shogiboardq \
  --env SHOGIBOARDQ_EXECUTABLE=/path/to/ShogiBoardQ/build/ShogiBoardQ \
  --env PYTHONPATH=/path/to/ShogiBoardQ/mcp \
  -- python3 -m shogiboardq_mcp
```

### Claude Desktop（`claude_desktop_config.json`）

```json
{
  "mcpServers": {
    "shogiboardq": {
      "command": "python3",
      "args": ["-m", "shogiboardq_mcp"],
      "env": {
        "SHOGIBOARDQ_EXECUTABLE": "/path/to/ShogiBoardQ/build/ShogiBoardQ",
        "PYTHONPATH": "/path/to/ShogiBoardQ/mcp"
      }
    }
  }
}
```

### Cursor（`.cursor/mcp.json`）

```json
{
  "mcpServers": {
    "shogiboardq": {
      "command": "python3",
      "args": ["-m", "shogiboardq_mcp"],
      "env": {
        "SHOGIBOARDQ_EXECUTABLE": "/path/to/ShogiBoardQ/build/ShogiBoardQ",
        "PYTHONPATH": "/path/to/ShogiBoardQ/mcp"
      }
    }
  }
}
```

### VS Code（`.vscode/mcp.json`）

```json
{
  "servers": {
    "shogiboardq": {
      "type": "stdio",
      "command": "python3",
      "args": ["-m", "shogiboardq_mcp"],
      "env": {
        "SHOGIBOARDQ_EXECUTABLE": "/path/to/ShogiBoardQ/build/ShogiBoardQ",
        "PYTHONPATH": "/path/to/ShogiBoardQ/mcp"
      }
    }
  }
}
```

### Gemini CLI

```bash
gemini mcp add -s user \
  -e SHOGIBOARDQ_EXECUTABLE=/path/to/ShogiBoardQ/build/ShogiBoardQ \
  -e PYTHONPATH=/path/to/ShogiBoardQ/mcp \
  shogiboardq python3 -m shogiboardq_mcp
```

`~/.gemini/settings.json`（プロジェクトなら `.gemini/settings.json`）に直接書く場合:

```json
{
  "mcpServers": {
    "shogiboardq": {
      "command": "python3",
      "args": ["-m", "shogiboardq_mcp"],
      "env": {
        "SHOGIBOARDQ_EXECUTABLE": "/path/to/ShogiBoardQ/build/ShogiBoardQ",
        "PYTHONPATH": "/path/to/ShogiBoardQ/mcp"
      }
    }
  }
}
```

### Codex（CLI / アプリ / IDE 拡張）

同じホストで動く Codex クライアントは `~/.codex/config.toml` の MCP 設定を共有します。
まず、`mcp` パッケージをインストールした Python で登録してください。
仮想環境を使う場合は、`python3` をその環境の Python の絶対パスに置き換えます。

```bash
codex mcp add shogiboardq \
  --env SHOGIBOARDQ_EXECUTABLE=/path/to/ShogiBoardQ/build/ShogiBoardQ \
  --env PYTHONPATH=/path/to/ShogiBoardQ/mcp \
  -- python3 -m shogiboardq_mcp
```

Linux で GUI の自動起動も使う場合は、登録後に `[mcp_servers.shogiboardq]` へ
下記の `env_vars` を追加してください。Codex からディスプレイ・セッションの環境変数を
引き継ぎます。値を固定しないため、ログインし直した後もそのセッションの値が使われます。
`XDG_CONFIG_HOME` は設定ファイルと起動中アプリの接続先を見つけるために引き継ぎます。

コマンドの代わりに `~/.codex/config.toml` に直接書く場合も、次の設定を使えます
（既に同じテーブルがある場合は追記せず、そのテーブルを編集します）:

```toml
[mcp_servers.shogiboardq]
command = "python3"
args = ["-m", "shogiboardq_mcp"]
startup_timeout_sec = 30
env_vars = ["DISPLAY", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS", "XDG_CONFIG_HOME"]

[mcp_servers.shogiboardq.env]
SHOGIBOARDQ_EXECUTABLE = "/path/to/ShogiBoardQ/build/ShogiBoardQ"
PYTHONPATH = "/path/to/ShogiBoardQ/mcp"
```

登録を `codex mcp get shogiboardq` で確認した後、Codex を再起動して新しい会話を開始してください。
CLI の `/mcp` で接続を確認し、「ShogiBoardQ の validate_sfen で startpos を検証して」と依頼すると、
平手初期局面の合法手数 `30` が返ります。`codex mcp list` / `get` は登録内容の確認であり、
ツールの実行成功までは確認しません。

画面のない環境でアプリ操作も試す場合は、`[mcp_servers.shogiboardq.env]` に
`QT_QPA_PLATFORM = "offscreen"` を追加できます。この場合、アプリのウィンドウは画面に表示されません。
棋譜変換・局面検証など CLI 経由のツールだけなら、この追加設定は不要です。

設定項目の詳細は [OpenAI 公式の MCP ドキュメント](https://developers.openai.com/codex/mcp) を参照してください。

Windows では `python3` を `python` に、パスを `C:\\Users\\...\\ShogiBoardQ\\build\\ShogiBoardQ.exe` のように置き換えてください（JSON では `\\` でエスケープします）。

## 安全性

- 自動化 API は `ShogiBoardQ --automation` を付けたときだけ有効で、ソケットは所有者のみアクセスできます。ネットワークには公開しません。
- ファイル引数は絶対パスのみ受け付け、`SHOGIBOARDQ_ALLOWED_DIRS`（既定はホーム）の外は拒否します。既存ファイルは `overwrite: true` が無ければ上書きしません。
- エンジンは ShogiBoardQ に登録済みの名前だけを受け付け、任意の実行ファイルパスは受け付けません。
- `trigger_action` は許可リスト方式で、終了・上書き保存・Webサイトを開く動作は実行しません。言語切替と名前付きレイアウト管理には対応しています。

## テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON && cmake --build build
ctest --test-dir build -R tst_mcp_python --output-on-failure
```

`ctest` は `SHOGIBOARDQ_CLI` などの環境変数を自動で渡します。pytest を直接実行する場合:

```bash
SHOGIBOARDQ_CLI=$PWD/build/shogiboardq-cli \
SHOGIBOARDQ_EXECUTABLE=$PWD/build/ShogiBoardQ \
SHOGIBOARDQ_TEST_USI_ENGINE=$PWD/build/Hayanagi/hayanagi \
SHOGIBOARDQ_TEST_MATE_ENGINE=$PWD/build/tests/mock_mate_engine \
python3 -m pytest -q mcp/tests
```

実際の shogi-server と ShogiHome を使うCSA対局テストの環境構築・実行方法は、
[CSA検証記録](../docs/dev/csa-game-mcp-audit-2026-09-27.md) を参照してください。
