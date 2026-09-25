# MCP サーバー（AI クライアント連携）設計

ShogiBoardQ の機能を Model Context Protocol（MCP）経由で AI クライアント（Claude Desktop / Claude Code / Cursor / VS Code / Gemini CLI / Codex CLI など）から使えるようにする仕組みの設計文書。
利用手順は [docs/guide/mcp-server.html](../guide/mcp-server.html)（英語版: [docs/en/guide/mcp-server.html](../en/guide/mcp-server.html)）、Python パッケージの README は [mcp/README.md](../../mcp/README.md) を参照。

---

## 目次

1. [全体構成](#1-全体構成)
2. [提供経路とツール一覧](#2-提供経路とツール一覧)
3. [CLI（shogiboardq-cli）](#3-clishogiboardq-cli)
4. [アプリ側自動化 API（--automation）](#4-アプリ側自動化-api--automation)
5. [MCP サーバー（Python）](#5-mcp-サーバーpython)
6. [ジョブの状態遷移](#6-ジョブの状態遷移)
7. [安全性](#7-安全性)
8. [ビルド構成とソース配置](#8-ビルド構成とソース配置)
9. [テスト](#9-テスト)
10. [対応クライアント](#10-対応クライアント)
11. [制限事項と拡張候補](#11-制限事項と拡張候補)

---

## 1. 全体構成

MCP のプロトコル実装は Qt アプリ本体に入れず、次の 2 層＋CLI で構成する。

```
AI クライアント（Claude Desktop / Claude Code / Cursor / VS Code / Gemini CLI / Codex CLI ...）
   │  MCP（stdio, JSON-RPC 2.0）
   ▼
shogiboardq_mcp（Python, 公式 mcp SDK）           mcp/shogiboardq_mcp/
   │                        │
   │ サブプロセス（JSON / JSON Lines）│ QLocalSocket（改行区切り JSON-RPC 2.0）
   ▼                        ▼
shogiboardq-cli            ShogiBoardQ --automation
（QApplication offscreen）  （GUI 本体。src/automation/ のサーバーが受け付ける）
   │                        │
   └── 共通の非 GUI クラス ──┘  src/automation/ のサービス、kifu/formats、engine/Usi、analysis/Tsume*
```

| 層 | 役割 | 実装 |
|---|---|---|
| MCP サーバー | ツール定義（JSON Schema）、引数検証、パス方針、ジョブ管理、アプリの自動起動・再接続、エラー変換 | `mcp/shogiboardq_mcp/`（Python 3.10+、`mcp>=1.10`） |
| CLI | アプリを起動せずに使える機能（棋譜変換、SFEN 検証、エンジン一覧、解析、詰み探索、詰将棋生成・余詰検査、盤面画像） | `src/cli/` + `src/automation/` の共通サービス |
| 自動化 API | 起動中のアプリの状態取得・操作（局面、棋譜、メニュー動作、スクリーンショット、ダイアログ内容） | `src/automation/`（`--automation` 指定時のみ有効） |

設計上の原則:

- MCP 仕様（2025-11-25 版。実装時点の `mcp` SDK 1.29 が対応する最新安定版）に忠実に実装し、ベンダー固有機能に依存しない。トランスポートは stdio のみ。
- ツールは `inputSchema` / `outputSchema` を厳密な JSON Schema で定義し、結果は必ず人が読めるテキストと、`structuredContent` の両方を返す。
- 長時間処理（解析・詰み探索・局面生成）はジョブ方式（開始ツールがジョブ ID を返し、状態・結果ツールで取得）。1 回のツール呼び出しは数秒以内に返す。
- `sampling` / `elicitation` / `roots` に依存しない。`notifications/progress` は `progressToken` が渡された場合だけ補助的に送る。
- UI 操作はすべてメインスレッドで行う。自動化 API は `QLocalServer` のシグナルからメインスレッドで既存コントローラを呼ぶだけで、ワーカースレッドを持たない。

## 2. 提供経路とツール一覧

名前は snake_case、説明は英語。段階 1 はアプリ起動不要（CLI 経由）、段階 2 は起動中のアプリを操作する（自動化 API 経由）。

### 段階 1（CLI 経由）

| ツール | 内容 | 主な引数 | CLI コマンド |
|---|---|---|---|
| `convert_kifu` | 棋譜ファイル／文字列を KIF・KI2・CSA・JKF・USI・USEN・SFEN 列に変換 | `input_path` または `text`、`input_format`（既定 auto）、`output_format`、`output_path`、`overwrite`、`max_chars` | `convert-kifu` |
| `validate_sfen` | SFEN の妥当性、手番、持駒、王手の有無、合法手数 | `sfen` | `validate-sfen` |
| `list_engines` | 設定済み USI エンジン（名前・パス・作者・実行ファイルの有無） | なし | `list-engines` |
| `analyze_position` | 登録エンジンで局面を解析するジョブを開始 | `engine`、`sfen`、`moves`、`seconds`(1-600)、`multipv`(1-10) | `analyze`（JSON Lines） |
| `analysis_status` | 解析ジョブの状態と最新の読み筋 | `job_id`、`max_pv_moves` | （ジョブ管理） |
| `analysis_result` | 解析ジョブの最終結果（実行中なら途中結果） | `job_id`、`max_pv_moves` | （ジョブ管理） |
| `search_mate` | `go mate` 対応エンジンで詰みを探索するジョブを開始 | `engine`、`sfen`、`moves`、`seconds`(1-600) | `mate`（JSON Lines） |
| `mate_status` | 詰み探索ジョブの状態と PV | `job_id` | （ジョブ管理） |
| `generate_tsume` | 詰将棋局面生成ジョブを開始 | `engine`、`target_moves`、`max_positions`、`timeout_ms`、`max_attack_pieces`、`max_defend_pieces`、`attack_range`、`add_remaining_to_defender_hand`、`allow_final_move_alternatives` | `generate-tsume`（JSON Lines、stdin `stop`） |
| `tsume_generation_status` | 生成ジョブの進捗と発見局面 | `job_id`、`max_positions` | （ジョブ管理） |
| `stop_tsume_generation` | 生成ジョブの停止 | `job_id` | stdin へ `stop` |
| `verify_tsume` | 余詰検査（Unique / Multiple / NoMate / WrongLength / Unknown / Invalid）と主手順 | `engine`、`sfen`、`target_moves`、`timeout_seconds`(1-50)、`allow_final_move_alternatives` | `verify-tsume` |
| `render_board_image` | SFEN から PNG を書き出し | `sfen`、`output_path`、`square_size`、`flip`、`last_move`、`overwrite` | `render-board` |
| `list_jobs` / `cancel_job` | ジョブ一覧と取消（補助） | `job_id` | （ジョブ管理） |

### 段階 2（自動化 API 経由）

| ツール | 内容 | 主な引数 | JSON-RPC メソッド |
|---|---|---|---|
| `get_app_state` | UI 状態（待機・対局中・解析中・詰将棋対局中など）、プレイモード、現在手数、棋譜ファイル名、未保存の有無、開いているダイアログ | なし | `app.state` |
| `get_position` | 現在局面の SFEN と開始局面からの USI 手順 | なし | `position.get` |
| `set_position` | SFEN で局面を設定 | `sfen`、`discard_unsaved` | `position.set` |
| `load_kifu` | 棋譜ファイル（または文字列）を読み込む | `path` または `text`、`discard_unsaved` | `kifu.load` |
| `save_kifu` | 棋譜を保存（形式は拡張子で決定） | `path`、`overwrite` | `kifu.save` |
| `get_kifu` | 現在の棋譜（構造化した手順、または形式指定のテキスト） | `format`、`from_ply`、`max_moves`、`max_chars` | `kifu.get` |
| `goto_ply` | 指定手数へ移動 | `ply` | `kifu.goto` |
| `trigger_action` | `QAction` の objectName を許可リスト内で実行 | `name` | `action.trigger`（一覧は `action.list`） |
| `capture_screenshot` | メインウィンドウまたは指定ダイアログを PNG 保存 | `target`、`output_dir` | `screenshot.capture` |
| `list_dialogs` | 開いているトップレベルウィンドウ／ダイアログ | なし | `dialog.list` |
| `get_widget_text` | ダイアログ内のラベル・入力欄・テーブル内容 | `dialog`、`widget`、`max_rows` | `widget.text` |

### リソース

| URI | 内容 | 経路 |
|---|---|---|
| `shogiboardq://position/current` | 現在局面の SFEN（text/plain） | `position.get` |
| `shogiboardq://kifu/current` | 現在の棋譜（KIF テキスト） | `kifu.get` |
| `shogiboardq://engines` | 設定済みエンジン（application/json） | `list-engines` |

プロンプト（`prompts/list`）は提供しない（対応しないクライアントがあるため必須機能にしない）。

## 3. CLI（shogiboardq-cli）

`QApplication` を `QT_QPA_PLATFORM=offscreen`（未設定時に自動設定）で起動する単発コマンド。標準出力に JSON（1 文書）または JSON Lines（長時間処理）を書き、標準エラーにはログだけを書く。終了コードは成功 0、失敗 1。

```
shogiboardq-cli <command> [options]
```

| コマンド | 主なオプション | 出力 |
|---|---|---|
| `version` | - | `{"ok":true,"version":"2026.09.25"}` |
| `convert-kifu` | `--input PATH` / `--text STR`、`--input-format auto\|kif\|ki2\|csa\|jkf\|usi\|usen`、`--output-format kif\|ki2\|csa\|jkf\|usi\|usen\|sfen`、`--output PATH`、`--overwrite` | `{ok, output_format, text \| output_path, initial_sfen, ply_count, usi_moves[], sfens[], warnings[]}` |
| `validate-sfen` | `--sfen SFEN` | `{ok, valid, normalized_sfen, turn, hands{black,white}, in_check, legal_move_count, kings{black,white}, errors[]}` |
| `list-engines` | - | `{ok, engines:[{name,path,author,exists}]}` |
| `analyze` | `--engine NAME --sfen SFEN [--moves "7g7f 3c3d"] --seconds N --multipv M` | JSON Lines: `started` → `info`（読み筋更新ごと）→ `result{bestmove, ponder, lines[]}` |
| `mate` | `--engine NAME --sfen SFEN [--moves ...] --seconds N` | JSON Lines: `started` → `result{status: mate\|nomate\|unknown\|notimplemented, pv[], plies}` |
| `generate-tsume` | `--engine NAME --target-moves N --max-positions K --timeout-ms T --max-attack A --max-defend D --attack-range R [--no-remaining-to-hand] [--no-final-alternatives]` | JSON Lines: `started` → `progress`（約 1 秒ごと）→ `position{sfen,pv}`（発見ごと）→ `finished{found,generated,elapsed_ms}`。stdin に `stop` を受け取ると停止 |
| `verify-tsume` | `--engine NAME --sfen SFEN --target-moves N [--timeout-ms T] [--no-final-alternatives]` | `{ok, status: unique\|multiple\|nomate\|wrong_length\|unknown\|invalid, pv[], queries, elapsed_ms}` |
| `render-board` | `--sfen SFEN --output PATH.png [--square-size 50] [--flip] [--last-move 7g7f] [--overwrite]` | `{ok, output_path, width, height}` |

共通仕様:

- エラーは `{"ok":false,"error":{"code":"invalid_argument"|"engine_not_found"|"engine_error"|"io_error"|"internal","message":"..."}}` を標準出力に書いて終了コード 1。JSON Lines コマンドでは `{"event":"error",...}` を出してから終了する。
- エンジンは設定ファイル（`SettingsCommon::settingsFilePath()`）の `[Engines]` に登録済みの名前だけを受け付ける（`EngineListSettings::loadEngines()`）。任意パスは受け付けない。エンジンのオプションは GUI と同じく設定ファイルの値を適用する（`Usi::startAndInitializeEngine`）。
- 局面は `--sfen`（`startpos` も可）と `--moves`（USI 手を空白区切り）で指定し、`position sfen ... moves ...` を組み立てる。
- `--output` の既存ファイルは `--overwrite` が無ければ拒否する。ディレクトリ制限は MCP サーバー側で行う（CLI 単体では行わない）。
- 長時間コマンドは `SIGTERM`/`SIGINT` でも停止処理（エンジンへの `quit`）を行って終了する（Windows は stdin の `stop` のみ）。

## 4. アプリ側自動化 API（--automation）

### 起動

```
ShogiBoardQ --automation [--automation-socket PATH]
```

既定では無効。有効時は `QLocalServer` でローカルソケットを開き、所有者だけがアクセスできる（`QLocalServer::UserAccessOption`）。

| 項目 | 値 |
|---|---|
| ソケットパス（Linux/macOS） | 環境変数 `SHOGIBOARDQ_AUTOMATION_SOCKET` → `--automation-socket` → `QStandardPaths::RuntimeLocation`（`$XDG_RUNTIME_DIR`）`/shogiboardq/automation.sock` |
| ソケット名（Windows） | `shogiboardq-automation-<ユーザー名>`（名前付きパイプ） |
| エンドポイント情報 | `QStandardPaths::AppConfigLocation/automation-endpoint.json`（`{"socket":..., "pid":..., "version":...}`）。起動時に書き、終了時に削除する。MCP サーバーはこれを読んで接続先を決める |
| プロトコル | 改行区切りの JSON-RPC 2.0（1 行 1 メッセージ、UTF-8）。バッチは未対応（`-32600`） |
| スレッド | すべてメインスレッド。1 リクエストを同期処理して応答する |

### メソッド一覧

| メソッド | params | result | 主なエラー |
|---|---|---|---|
| `app.ping` | - | `{pong:true}` | - |
| `app.version` | - | `{version, qt, api:1}` | - |
| `app.state` | - | `{ui_state, play_mode, current_ply, total_plies, kifu_file, dirty, board_flipped, dialogs:[...], engines:{black,white}}` | - |
| `app.quit` | - | `{ok:true}`（応答後に終了。テストハーネス用で MCP ツールには出さない） | - |
| `position.get` | - | `{sfen, start_sfen, ply, moves[]}` | - |
| `position.set` | `{sfen, discard_unsaved?}` | `{sfen}` | `-32602` 不正 SFEN、`-32004` 未保存 |
| `kifu.load` | `{path?, text?, discard_unsaved?}` | `{total_plies, start_sfen, kifu_file}` | `-32003` パス、`-32004` 未保存、`-32005` 読込失敗 |
| `kifu.save` | `{path, overwrite?}` | `{path, format}` | `-32003` パス／既存ファイル |
| `kifu.get` | `{format?, from_ply?, max_moves?, max_chars?}` | `{format, total_plies, moves:[{ply,text,usi,time,comment}], text?, truncated}` | - |
| `kifu.goto` | `{ply}` | `{ply, sfen}` | `-32602` 範囲外 |
| `action.list` | - | `{actions:[{name,text,enabled,checked,checkable}]}` | - |
| `action.trigger` | `{name}` | `{name, triggered:true}` | `-32001` 許可リスト外、`-32002` 無効状態 |
| `screenshot.capture` | `{target?: "main"\|objectName\|タイトル, output_dir?}` | `{path, width, height}` | `-32005` 対象なし |
| `dialog.list` | - | `{windows:[{object_name,class,title,visible,modal,active}]}` | - |
| `widget.text` | `{dialog?, widget?, max_rows?}` | `{widgets:[{object_name,class,text?,items?,rows?}]}` | `-32005` 対象なし |

エラーコード: JSON-RPC 標準（`-32700` parse、`-32600` invalid request、`-32601` method not found、`-32602` invalid params、`-32603` internal）に加え、`-32001` not allowed、`-32002` invalid state、`-32003` file error、`-32004` unsaved changes、`-32005` not found。`error.data.hint` に対処方法を入れる。

`action.trigger` の許可リストは `src/automation/automationactionpolicy.cpp` で管理する。終了（`actionQuit`）、上書き保存（`actionSave`）、言語切替、Web サイトを開く動作、ドックレイアウトの保存は除外する。

### アプリ側のクラス構成

| クラス | ファイル | 責務 |
|---|---|---|
| `AutomationServer` | `src/automation/automationserver.{h,cpp}` | `QLocalServer` の待ち受け、接続ごとの行バッファ、応答の書き出し、エンドポイント情報の作成・削除 |
| `AutomationDispatcher` | `src/automation/automationdispatcher.{h,cpp}` | JSON-RPC 2.0 の解析・検証、メソッド表、エラー応答の生成（GUI 非依存で単体テスト可能） |
| `AutomationContext` | `src/automation/automationcontext.h` | MainWindow 側の依存を注入する Deps 構造体（非所有ポインタとコールバックのみ） |
| `AutomationCommands*` | `src/automation/automationcommands_{app,position,kifu,ui}.cpp` | 各メソッドの実装。既存の `KifuFileController` / `KifuExportController` / `KifuNavigationController` / `UiStatePolicyManager` / `ScreenshotService` を呼ぶ |
| `AutomationActionPolicy` | `src/automation/automationactionpolicy.{h,cpp}` | `QAction` の許可リスト |
| `ScreenshotService` | `src/services/screenshotservice.{h,cpp}` | 旧 `DebugScreenshotService` をリリースでも使える形にしたもの。F12 配線（`DebugScreenshotWiring`）はデバッグビルド専用のまま |

MainWindow には `startAutomationServer(socketPath)` の委譲メソッドだけを追加し、生成と依存注入は `MainWindowServiceRegistry::ensureAutomationServer()`（`src/app/mainwindowautomationregistry.cpp`）で行う。`KifuFileController` には確認ダイアログを出さない非対話 API（`loadKifuFile`、`loadKifuText`、`applySfenPosition`、`saveKifuToPath`、`hasUnsavedChanges`）を追加し、`KifuExportController` には `exportLines(format)` を追加する。

## 5. MCP サーバー（Python）

```
mcp/
  pyproject.toml            パッケージ定義（shogiboardq-mcp スクリプト）
  README.md                 利用手順と各クライアントの設定例
  shogiboardq_mcp/
    __main__.py             python -m shogiboardq_mcp → stdio サーバー
    server.py               低レベル Server。tools/list・tools/call・resources
    tooldefs.py             ツール定義（name / description / inputSchema / outputSchema / annotations）
    handlers_cli.py         段階 1 ツール（CLI 呼び出し）
    handlers_app.py         段階 2 ツール（自動化 API 呼び出し）
    cli_backend.py          CLI の探索と実行（同期コマンド／ジョブ用サブプロセス）
    jobs.py                 ジョブ管理（状態、イベント、停止、期限切れ）
    app_client.py           自動化 API クライアント（接続、再接続、アプリの自動起動）
    paths.py                パス方針（絶対パス、許可ディレクトリ、上書き）とエンドポイント解決
    formatting.py           人が読むテキスト要約の生成
  tests/                    pytest（mcp SDK のクライアントで stdio 経由にツールを呼ぶ）
```

- 低レベル `mcp.server.lowlevel.Server` を使い、`CallToolResult` に `content`（要約テキスト）と `structuredContent` を両方入れる。`outputSchema` は SDK が検証する。`structuredContent` を無視するクライアントでもテキストだけで使える。
- ツール定義は `annotations`（`readOnlyHint` / `destructiveHint` / `idempotentHint` / `openWorldHint`）を付ける。
- 引数は `inputSchema` で検証され（SDK）、追加で `paths.py` がパス方針を検査する。
- 環境変数:

| 変数 | 意味 |
|---|---|
| `SHOGIBOARDQ_CLI` | `shogiboardq-cli` のパス。未指定なら `SHOGIBOARDQ_EXECUTABLE` と同じディレクトリ、次に `PATH` を探す |
| `SHOGIBOARDQ_EXECUTABLE` | `ShogiBoardQ` のパス。段階 2 ツールでアプリが起動していないときに `--automation` 付きで起動する |
| `SHOGIBOARDQ_AUTOMATION_SOCKET` | 自動化ソケットのパス（アプリ側と共通） |
| `SHOGIBOARDQ_ALLOWED_DIRS` | 読み書きを許可するディレクトリ（`os.pathsep` 区切り。既定はホームディレクトリ） |
| `SHOGIBOARDQ_OUTPUT_DIR` | スクリーンショットなど出力先未指定時のディレクトリ（既定は一時ディレクトリ配下の `shogiboardq-mcp`） |
| `SHOGIBOARDQ_QUIT_APP_ON_EXIT` | `1` のとき、サーバーが起動したアプリを終了時に閉じる（既定は残す） |

- アプリへの接続は `automation-endpoint.json` → 既定パスの順に試し、失敗したら `SHOGIBOARDQ_EXECUTABLE` を起動して最大 30 秒待つ。接続断は次の呼び出しで再接続する。
- 1 回の JSON-RPC 呼び出しには 30 秒のタイムアウトを置く（GUI がモーダルダイアログで止まっている場合に備える）。

## 6. ジョブの状態遷移

ジョブは MCP サーバーのプロセス内に保持する（サーバー再起動で消える）。CLI のサブプロセス 1 つが 1 ジョブに対応する。

```
            start tool            CLI が result/finished を出力
 (none) ───────────────▶ running ───────────────────────────▶ finished
                           │  │
                           │  └── CLI が error を出力 / 異常終了 ─▶ failed
                           │
                           └── stop / cancel ─▶ stopping ─▶ stopped
                                 （stdin に stop、5 秒待って terminate、さらに 3 秒で kill）

 finished / failed / stopped は 30 分後に一覧から消える。同時実行は 4 ジョブまで（超過は error）。
```

| ジョブ種別 | 状態ツールが返す内容 |
|---|---|
| analysis | `state, elapsed_ms, engine, depth, nodes, nps, lines:[{multipv, score_cp, score_mate, depth, pv[], pv_text}], bestmove, ponder` |
| mate | `state, elapsed_ms, engine, status, pv[], plies` |
| tsume_generation | `state, elapsed_ms, generated, found, rejected, inconclusive, positions:[{sfen,pv}]` |

## 7. 安全性

- 自動化ソケットは所有者限定（Unix ドメインソケットの権限 / 名前付きパイプの UserAccess）。TCP や HTTP は開かない。
- ファイル引数は絶対パスのみ。`SHOGIBOARDQ_ALLOWED_DIRS`（既定: ホーム配下）の外は拒否する。上書きは `overwrite: true` が無ければ拒否する。検査は MCP サーバー側（`paths.py`）で行い、CLI/アプリ側は絶対パスの要求と既存ファイルの拒否だけを行う。
- エンジンは `[Engines]` に登録済みの名前だけ。実行ファイルパスは受け付けない。
- 自動化 API は既定で無効。`--automation` を付けたときだけ有効になり、起動時に標準出力へソケットパスを出す。
- `trigger_action` は許可リスト方式。終了・上書き保存・言語切替など、ユーザーの意図なしに実行すべきでない動作は除外する。
- 出力サイズは既定で抑える（棋譜テキスト 30,000 文字、手順 200 手、読み筋 20 手、生成局面 20 件など）。

## 8. ビルド構成とソース配置

CLI が GUI 本体と同じクラスを使えるように、`CMakeLists.txt` のソースを OBJECT ライブラリ `shogiboardq_objects` にまとめ、`ShogiBoardQ` はこれを直接リンクする（従来と同じく全オブジェクトを含む）。`shogiboardq-cli` は同じオブジェクトから作る静的ライブラリ `shogiboardq_static` をリンクし、参照したオブジェクトだけが取り込まれる。新しいテストもこの静的ライブラリをリンクできる。

| ディレクトリ | 内容 |
|---|---|
| `src/automation/` | 共通サービス（`KifuConversionService`、`SfenValidationService`、`BoardImageRenderer`、`UsiInfoLineParser`、`EngineCatalog`、`EngineAnalysisRunner`、`MateSearchRunner`、`TsumeVerificationRunner`）と自動化 API（`AutomationServer` ほか） |
| `src/cli/` | `shogiboardq-cli` のエントリーポイントとコマンド実装（`CliCommands*`）。JSON 出力と stdin の `stop` 監視 |
| `mcp/` | Python パッケージとテスト |

`src/automation/` は `app/`・`dialogs/`・`widgets/` に依存しない（`views/ShogiView` は盤面画像のために使う）。レイヤー依存テスト（`tst_layer_dependencies`）にルールを追加する。

## 9. テスト

| テスト | 種別 | 内容 |
|---|---|---|
| `tst_automation_dispatcher` | C++（Qt Test） | JSON-RPC の解析・エラーコード・メソッド表 |
| `tst_kifu_conversion_service` | C++ | fixtures の KIF/KI2/CSA/JKF/USI/USEN を相互変換し、手順・初期局面・SFEN 列を検証 |
| `tst_sfen_validation_service` | C++ | 妥当／不正な SFEN、王手検出、合法手数 |
| `tst_usi_info_line_parser` | C++ | `info` 行の解析（multipv、score cp/mate、bound、pv） |
| `tst_board_image_renderer` | C++ | offscreen で PNG を生成しサイズと非空を検証 |
| `mock_mate_engine` | 補助実行ファイル | `go mate <ms>` に Hayanagi の詰み探索で答える USI エンジン（生成・余詰検査・詰み探索の結合テスト用） |
| `tst_mcp_python` | pytest（CTest から実行） | `mcp` SDK のクライアントで stdio サーバーを起動し、段階 1 ツール（CLI）と段階 2 ツール（`QT_QPA_PLATFORM=offscreen` で起動したアプリ）を呼ぶ。エンジンは `hayanagi`（解析）と `mock_mate_engine`（詰み）を一時設定ファイルに登録して使う |

`tst_mcp_python` は Python3 と `mcp`・`pytest` が見つかるときだけ登録される。実エンジンでの確認は `SHOGIBOARDQ_TEST_KOMORING` があれば追加で行う。

## 10. 対応クライアント

設定例（`claude_desktop_config.json`、`claude mcp add`、`.cursor/mcp.json`、`.vscode/mcp.json`、Gemini CLI の `settings.json`、Codex CLI の `config.toml`）は [mcp/README.md](../../mcp/README.md) と利用ガイドに記載する。いずれも stdio で `python3 -m shogiboardq_mcp` を起動し、環境変数で実行ファイルの場所を渡す。

## 11. 制限事項と拡張候補

- Streamable HTTP は実装しない（必要になれば localhost 限定・トークン必須で追加する）。
- CSA 通信対局、対局の開始・指し手の実行、評価値グラフ画像の取得は未対応。
- Windows の名前付きパイプ接続は実装済みだが未検証。
- 拡張候補: 解析結果のグラフ画像、棋譜解析（全手）のジョブ化、`prompts/list` による定型手順、局面集ファイルの操作。
