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
| `SHOGIBOARDQ_QUIT_APP_ON_EXIT` | `1` のとき、サーバーが起動したアプリをサーバー終了時に閉じます（既定は残す） |
| `SHOGIBOARDQ_MCP_DEBUG` | `1` で詳細ログを標準エラーに出します |

## ツール一覧

アプリを起動しなくても使えるもの（`shogiboardq-cli` 経由）:

| ツール | 内容 |
|---|---|
| `convert_kifu` | 棋譜ファイル／文字列を KIF・KI2・CSA・JKF・USI・USEN・SFEN 列に変換 |
| `validate_sfen` | SFEN の妥当性、手番、持駒、王手、合法手数 |
| `list_engines` | 登録済み USI エンジン |
| `analyze_position` / `analysis_status` / `analysis_result` | エンジン解析（ジョブ方式、MultiPV 対応） |
| `search_mate` / `mate_status` | `go mate` による詰み探索（ジョブ方式） |
| `generate_tsume` / `tsume_generation_status` / `stop_tsume_generation` | 詰将棋局面の自動生成（ジョブ方式） |
| `verify_tsume` | 余詰検査（unique / multiple / nomate / wrong_length / unknown） |
| `render_board_image` | SFEN から盤面 PNG を生成 |
| `list_jobs` / `cancel_job` | ジョブの一覧と取消 |

起動中のアプリ（`ShogiBoardQ --automation`）を操作するもの:

| ツール | 内容 |
|---|---|
| `get_app_state` | UI 状態・現在手数・棋譜ファイル・未保存・開いているダイアログ |
| `get_position` / `set_position` | 現在局面の取得と設定 |
| `load_kifu` / `save_kifu` / `get_kifu` / `goto_ply` | 棋譜の読み込み・保存・取得・手数移動 |
| `list_actions` / `trigger_action` | メニュー動作の一覧と実行（許可リスト方式） |
| `capture_screenshot` | メインウィンドウ／ダイアログのスクリーンショット |
| `list_dialogs` / `get_widget_text` | 開いているダイアログと、その中のテキスト・テーブル内容 |

リソース: `shogiboardq://position/current`（SFEN）、`shogiboardq://kifu/current`（KIF）、`shogiboardq://engines`（JSON）。

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
- `trigger_action` は許可リスト方式で、終了・上書き保存・言語切替などは実行しません。

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
