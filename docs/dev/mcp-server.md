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
   └── 共通の非 GUI クラス ──┘  src/headless/ の部品、kifu/formats、engine/UsiEngineSession・MateOracle、analysis/Tsume*
```

| 層 | 役割 | 実装 |
|---|---|---|
| MCP サーバー | ツール定義（JSON Schema）、引数検証、パス方針、ジョブ管理、アプリの自動起動・再接続、エラー変換 | `mcp/shogiboardq_mcp/`（Python 3.10+、`mcp>=2,<3`。SDK 1.x では版を示して終了する） |
| CLI | アプリを起動せずに使える機能（棋譜変換、SFEN 検証、エンジン一覧、解析、詰み探索、詰将棋生成・余詰検査、盤面画像） | `src/cli/` + `src/headless/` の共通部品（盤面画像は `src/cli/` の `BoardImageRenderer`） |
| 自動化 API | 起動中のアプリの状態取得・操作（局面、棋譜、メニュー動作、スクリーンショット、ダイアログ内容） | `src/automation/`（`--automation` 指定時のみ有効） |

設計上の原則:

- MCP 仕様の最新版 2026-07-28（リクエストごとに版を示すステートレスな方式。`server/discover` で交渉）と、`initialize` で交渉する 2024-11-05〜2025-11-25 の両方に対応する。交渉は `mcp` SDK 2.x の低レベル Server が行い、`mcp/tests/test_protocol.py` で両方式を確認する。ベンダー固有機能に依存しない。トランスポートは stdio のみ。
- ツールは `inputSchema` / `outputSchema` を厳密な JSON Schema で定義し、結果は必ず人が読めるテキストと、`structuredContent` の両方を返す。
- 長時間処理（解析・詰み探索・局面生成）はジョブ方式（開始ツールがジョブ ID を返し、状態・結果ツールで取得）。1 回のツール呼び出しは数秒以内に返す。
- `sampling` / `elicitation` / `roots` / ログ通知（2026-07-28 で非推奨）に依存しない。進捗はジョブの状態ツールで返し、`notifications/progress` は送らない。
- UI 操作はすべてメインスレッドで行う。自動化 API は `QLocalServer` のシグナルからメインスレッドで既存コントローラを呼ぶだけで、ワーカースレッドを持たない。

## 2. 提供経路とツール一覧

名前は snake_case、説明は英語。段階 1 はアプリ起動不要（CLI 経由）、段階 2 は起動中のアプリを操作する（自動化 API 経由）。

### 段階 1（CLI 経由）

| ツール | 内容 | 主な引数 | CLI コマンド |
|---|---|---|---|
| `convert_kifu` | 棋譜ファイル／文字列を KIF・KI2・CSA・JKF・USI・USEN・SFEN 列に変換 | `input_path` または `text`、`input_format`（既定 auto）、`output_format`、`output_path`、`overwrite`、`max_chars`、`max_moves`（`usi_moves`・`moves`・`sfens` に並べる手の数。既定 2000。超えたら `moves_truncated`） | `convert-kifu` |
| `validate_sfen` | SFEN の妥当性、手番、持駒、王手の有無、合法手数 | `sfen` | `validate-sfen` |
| `list_engines` | 設定済み USI エンジン（名前・パス・作者・実行ファイルの有無） | なし | `list-engines` |
| `analyze_position` | 登録エンジンで局面を解析するジョブを開始 | `engine`、`sfen`、`moves`、`seconds`(1-600)、`multipv`(1-10) | `analyze`（JSON Lines） |
| `analysis_status` | 解析ジョブの状態と最新の読み筋 | `job_id`、`max_pv_moves` | （ジョブ管理） |
| `analysis_result` | 解析ジョブの最終結果（実行中なら途中結果） | `job_id`、`max_pv_moves` | （ジョブ管理） |
| `analyze_kifu` | 棋譜の本譜を全手または指定範囲で連続解析 | `engine`、`input_path` または `text`、`from_ply`、`to_ply`、`seconds_per_position`、`multipv` | `analyze-kifu` |
| `kifu_analysis_status` / `kifu_analysis_result` | 手数別の評価・読み筋・進捗・部分結果 | `job_id`、`offset`、`max_positions`、`max_pv_moves` | （ジョブ管理） |
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
| `get_kifu` | 現在の棋譜（表示中の手順の構造化リスト、または形式指定のテキスト） | `format`、`line`、`from_ply`、`max_moves`、`max_chars` | `kifu.get` |
| `goto_ply` | 指定手数へ移動 | `ply` | `kifu.goto` |
| `list_actions` | `trigger_action` で実行できる動作の一覧（有効・チェック状態付き） | なし | `action.list` |
| `trigger_action` | `QAction` の objectName を許可リスト内で実行（応答後に実行するのでモーダルダイアログでも返る） | `name` | `action.trigger` |
| `click_board_square` | 盤面・駒台へマウスクリックを送る（回転に依存しない論理座標） | `target`、`file`、`rank`、`button` | `board.click` |
| `click_dialog_button` | ダイアログのボタンを押す。問題カード・成り選択にも対応 | `target`、`widget` または `text`、`index` | `dialog.clickButton` |
| `list_docks` | 非表示を含む全ドックの配置・タブ・表示領域・固定状態 | なし | `dock.list` |
| `configure_dock` | 表示、閉じる、浮動化、四方向配置、タブ化、タイトルバードラッグ | `widget`、`operation`、`area?`、`relative_to?`、`geometry?`、`x?`、`y?` | `dock.configure` |
| `capture_screenshot` | メインウィンドウまたは指定ダイアログを PNG 保存 | `target`、`output_dir` | `screenshot.capture` |
| `list_dialogs` | 開いているトップレベルウィンドウ／ダイアログ | なし | `dialog.list` |
| `close_dialog` | ダイアログを閉じる（`QDialog::reject`）。エラーのメッセージボックスの片付けにも使う | `target` | `dialog.close` |
| `get_widget_text` | ダイアログ（`target` 省略時はメインウィンドウ）内のラベル・入力欄・テーブル内容 | `target`（既定 `main`）、`widget`、`max_rows`、`max_widgets`、`include_children` | `widget.text` |
| `set_widget_value` | コンボ・スピン・スライダー・チェック・グループ・リスト・タブ・色・テキスト欄の変更。単一行入力は `submit=true` で Enter も送信 | `target`、`widget`、`value`、`submit?` | `widget.setValue` |
| `list_menu_actions` / `select_menu_action` | 定跡・局面集の履歴、定跡マージ、保存済みレイアウトの動的メニュー | `target?`、`widget`、選択時 `path` | `menu.items` / `menu.select` |
| `menu_favorites` | お気に入りの取得、登録・解除・並べ替えと保存 | `actions?`（省略時は取得） | `menu.favorites` |
| `click_branch_node` | 分岐ツリーのノードへ移動 | `target?`、`widget`、`id` | `branch.click` |
| `get_clipboard` | クリップボードのテキストと画像の有無・サイズ（ほかのアプリと共有するので、汎用GUI操作と同じく `SHOGIBOARDQ_ALLOW_UNRESTRICTED_UI=1` が要る） | `max_chars` | `clipboard.get` |
| `edit_table_cell` | 編集可能なテーブルセルに入力し、必要に応じて確定 | `target?`、`widget`、`row`、`column`、`text`、`commit?` | `widget.editCell` |
| `widget.showDock` | `{widget}` | `{object_name, queued:true, interaction}`（ドック表示・前面化） | `-32602` 型違い、`-32002` 操作不可、`-32005` 対象なし |
| `widget.click` | `{target?, widget}` | `{object_name, queued:true, interaction}`（ボタンクリック） | 同上 |
| `widget.setValue` | `{target?, widget, value}` | `{object_name, queued:true, interaction}`（コンボ・整数スピン・チェック・タブ・プレーンテキスト入力） | 同上、範囲外・型違いは `-32602` |
| `widget.clickCell` | `{target?, widget, row, column}` | `{object_name, queued:true, interaction}`（テーブルの通常マウス入力） | 同上 |
| `widget.editCell` | `{target?, widget, row, column, text, commit?}` | `{object_name, queued:true, interaction}`（通常のセル入力欄を使用。`commit` の既定はtrue） | 同上。読み取り専用セルは `-32002` |

`widget.text` はコンボ／タブの `current_index`、フォントの `font_point_size`、盤面の `arrows`
（`from_file`, `from_rank`, `to_file`, `to_rank`, `priority`, `drop_piece`）も返す。
UI操作は非表示・無効・モーダルで遮られた対象を拒否し、予約実行時にも再確認する。
予約実行時の確認で止まった操作や実行中に失敗した操作は `ui.interaction` に `failed`（分類つきの `error`）として残り、
MCP のツールはその分類の語のエラーを返す（4 章「応答の後に行う UI 操作」）。
テキスト入力は読み取り専用欄を拒否する。パスワードは書き込みのみ可能で、表示状態に関係なく
読み取りを伏せ、APIからの表示切り替えを拒否する。機密欄は `automationSensitive` プロパティで識別する。
MCPの汎用GUI操作は `SHOGIBOARDQ_ALLOW_UNRESTRICTED_UI=1` の明示指定が必要であり、
有効化時にはGUI操作へのファイル制限を保証しない。専用ファイルツールの制限は常に適用する。`kifuPasteText` は棋譜貼り付け欄、
`kifuTable` / `kifuBranchTable` は棋譜・分岐候補テーブルを指定する名前。
`gameInfoTable` は対局情報テーブル。`commit=false` は入力中の状態を維持する。
`get_widget_text` は省略表示ラベルの `fullText` プロパティにも対応するため、
`blackNameLabel` / `whiteNameLabel` で盤面の対局者名を全文取得できる。
`get_position` と `get_app_state.total_plies`、`goto_ply` の上限は閲覧中の分岐に従い（スキーマの上限は 100000）、
`get_kifu` は本譜を返す。`get_kifu.truncated` は指定範囲の続きが省略されている場合に true。
`widget.showDock` は `dock.configure` の `operation=show` と同じ処理を呼ぶ（引数1つで選びやすいので名前を残している）。どちらもメインウィンドウのドックだけを対象にし、メインウィンドウがモーダルで塞がれていないことと、ドックが有効なことを確かめる。

`dock.list` の `hidden` は明示的な非表示、`exposed` は表示領域の有無を表す。
タブの裏側は `visible=true` でも `exposed=false` となる。`geometry` はグローバル論理座標の
クライアント矩形、`tabified_with` は同じタブグループのobjectName、`area` は浮動時には最後の配置エリア。
`dock.configure` の操作は `show / hide / float / dock / tabify / drag`。
`float` は任意で `geometry={x,y,width,height}`、`dock` は `area=left/right/top/bottom`、
`tabify` は `relative_to`、`drag` はマウスの終点 `x,y` を指定する。
ドラッグはドッキング中のQtタイトルバーへのマウスイベント、それ以外はQtのドックAPIを使う。
移動不可・許可エリア外・モーダルで遮られた操作は拒否し、予約後も再確認する。
`actionResetDockLayout` と `actionSaveDockLayout` も許可リストに含む。
`mcp/tests/test_docks.py` は独立した設定ディレクトリで実アプリを起動し、これらをstdio MCP経由で検証する。


### 無名の部品と拡張操作

`get_widget_text` / `list_dialogs` は `selector`（例: `@widget-42`）を返す。
`widget` と `target` に指定でき、無名・同名の部品を区別する。
セレクターは対象の寿命中だけ有効で、ダイアログ再生成・アプリ再起動後は再取得する。
`get_widget_text(widget=..., include_children=true)` で対象内に絞って列挙できる。
`max_widgets` は最大3000（既定300）、`max_rows` は表・リスト・分岐ノードの取得上限。
`parent_selector`、`accessible_name`、`tooltip`、メニューボタンの `action_name` も返す。

メニューは `list_menu_actions(widget=...)` で読み取り、0始まりの階層インデックス
`select_menu_action(widget=..., path=[0, 1])` で選択する。
`josekiRecentMenu`、`josekiMergeMenu`、`sfenCollectionRecentMenu`、`menuSavedLayouts` が公開済み。
`menu_favorites(actions=[...])` はアクション名を表示順で指定し、空配列なら全解除する。
分岐ツリーは `branchTreeView` の `nodes` を取得し、`click_branch_node` に `id` を渡す。

`analyze_kifu` は本譜の開始局面（0手目）から終局面までを既定で解析する。
`from_ply` / `to_ply` は両端を含む。分岐を含む入力では `has_branches` を返す。
CLI の `analyze-kifu` がエンジンを1回だけ起動し、各局面へ元の開始局面と着手履歴を渡すため、千日手判定に必要な履歴を維持する。
`lines` の評価は手番側、`score_cp_black` / `score_mate_black` は先手側に統一した値。
`cancel_job` で中断でき、完了済み局面と停止時に返った部分結果を取得できる。
GUIの棋譜解析結果や評価値グラフへの書き込みは行わない。

新たに接続した機能と実機テストは [MCP連携の補完記録](mcp-coverage-implementation-2026-09-28.md) を参照。

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
| `describe` | - | `{ok, commands:[{name, summary, input_schema}], methods:[{name, summary, input_schema}]}`（CLI の全コマンドと自動化 API の全メソッドの引数の表を JSON Schema で出す） |
| `environment` | - | `{ok, version, socket_path, endpoint_file, settings_file, max_message_bytes, error_kinds:[{slug, rpc_code}], kifu_read_suffixes, kifu_write_suffixes, image_write_suffixes}`（自動化 API の既定のソケット・エンドポイントファイル・設定ファイルの場所と1メッセージの上限を、アプリと同じ計算（`AutomationEndpoint::defaultSocketPath()`・`endpointFilePath()`・`SettingsCommon::settingsFilePath()`・`AutomationEndpoint::kMaxMessageBytes`）で出し、エラーの分類の表（`ErrorKinds::toJson()`）と、MCP サーバーが読み書きを許すファイルの拡張子（`KifuFormats::readableSuffixes()`・`writableSuffixes()`・`BoardImageRenderer::fileSuffixes()`）を添える。MCP サーバーはこれを読んで値を写さない） |
| `convert-kifu` | `--input-path PATH` / `--text STR`、`--input-format auto\|kif\|ki2\|csa\|jkf\|usi\|usen`、`--output-format kif\|ki2\|csa\|jkf\|usi\|usen\|sfen`、`--output-path PATH`、`--overwrite`（旧名 `--input`・`--output`） | `{ok, input_format, output_format, text \| output_path, initial_sfen, ply_count, usi_moves[], sfens[], moves[], game_info[], has_branches, line_count, warnings[]}`。`moves[]` は本譜の各行（`{ply, text, usi, time, comment, bookmark}`。終局の行は `usi` なし、`time`・`comment`・`bookmark` は空なら省く）で、`KifuConversionService::Result::mainline`（`RecordLine`。`KifuBranchTree::exportLine(0)`）から作る。`text` は GUI の保存・コピーと同じ書き出しの表（`KifuExport::lines(tree, metadata, format)`）で作る |
| `validate-sfen` | `--sfen SFEN` | `{ok, valid, normalized_sfen, turn, hands{black,white}, in_check, legal_move_count, kings{black,white}, errors[]}` |
| `list-engines` | - | `{ok, engines:[{name,path,author,exists}]}` |
| `analyze-kifu` | `--engine NAME (--input-path PATH \| --text STR) [--input-format auto\|...] [--from-ply N] [--to-ply N] [--seconds-per-position N] [--multipv M] [--stdin-control]`（本譜を1つのエンジンで順に解析。`--from-ply` 既定 0（開始局面）、`--to-ply` 既定は本譜の最後、1局面 1-600 秒 既定 1） | JSON Lines: `started{total_positions, from_ply, to_ply, has_branches, warnings}` → `progress{current_ply, completed}`（局面を始めるごと）→ `position{ply, sfen, played_move, bestmove, ponder, lines[], score_cp_black \| score_mate_black, partial}`（局面を終えるごと。評価値は先手から見た値）→ `finished{completed, stopped}`。範囲が本譜の外なら `invalid_argument` |
| `analyze` | `--engine NAME [--sfen SFEN] [--moves "7g7f 3c3d"] [--seconds N] [--multipv M] [--stdin-control]`（`--sfen` 既定 `startpos`、`--seconds` 1-600 既定 10、`--multipv` 1-10 既定 1） | JSON Lines: `started` → `info`（読み筋更新ごと）→ `result{bestmove, ponder, lines[]}` |
| `mate` | `--engine NAME --sfen SFEN [--moves ...] [--seconds N] [--stdin-control]`（`--sfen` 必須、`--seconds` 1-600 既定 10） | JSON Lines: `started` → `result{status: mate\|nomate\|unknown\|notimplemented, pv[], plies}` |
| `generate-tsume` | `--engine NAME [--target-moves N] [--max-positions K] [--timeout-ms T] [--max-attack-pieces A] [--max-defend-pieces D] [--attack-range R] [--no-add-remaining-to-defender-hand] [--no-allow-final-move-alternatives] [--stdin-control]`（既定: 手数 3（1-19）、件数 5（1-100）、5000 ms（500-60000）、攻め方 4（1-10）、玉方 1（0-10）、範囲 3（1-8）、余り駒を玉方の持ち駒にする・最終手の別解を許す。旧名 `--max-attack`・`--max-defend`・`--no-remaining-to-hand`・`--no-final-alternatives`） | JSON Lines: `started` → `progress`（約 1 秒ごと）→ `position{sfen,pv}`（発見ごと）→ `finished{found,generated,elapsed_ms}`。stdin に `stop` を受け取ると停止 |
| `verify-tsume` | `--engine NAME --sfen SFEN --target-moves N [--timeout-seconds T] [--no-allow-final-move-alternatives]`（手数 1-39、`--timeout-seconds` 1-120 既定 15。旧名 `--timeout-ms`（秒に切り上げ）・`--no-final-alternatives`） | `{ok, status: unique\|multiple\|nomate\|wrong_length\|unknown\|invalid, pv[], queries, elapsed_ms}` |
| `render-board` | `[--sfen SFEN] --output-path PATH.png [--square-size 50] [--flip] [--last-move 7g7f] [--black-name NAME] [--white-name NAME] [--overwrite]`（`--square-size` 20-150。旧名 `--output`） | `{ok, output_path, width, height}` |

共通仕様:

- オプションは引数の表 `CliCommandSpecs`（`src/cli/clicommandspecs.{h,cpp}`）に1か所で宣言する。引数名は MCP のツールの引数名と同じで、オプション名は引数名の `_` を `-` にしたもの（`max_positions` → `--max-positions`）。`CliCommands::run()` が表（`main.cpp` の (名前, 実装, 出力の種類) の1行と、同じ名前の `CliCommandSpecs`）からコマンドを選び、コマンドラインを検査済みの JSON（既定値を補ったもの）にしてからコマンドに渡す（使い方の誤りもコマンドの出力の種類で書く。表の片方にしか無いコマンドは `unknown_operation`）。真偽値はフラグ（指定すれば true。既定値が true のものは `--no-<名前>` で false）、文字列の配列（`--moves`）は空白区切りの1つの値。全部の引数を `--params-json '<JSON オブジェクト>'` で渡すこともでき（MCP サーバーはこれを使う）、コマンドラインのオプションが優先する。以前の名前は旧名（`ParamSpec::aliases`。JSON Schema には出さない）として受け付ける。指し手（`moves` の要素・`last_move`）は USI の形の正規表現（`ParamSpecs::usiMovePattern()`。JSON Schema の `pattern`）で検査する。知らないオプション・必須の欠け・範囲外・選択肢外は `invalid_argument`。各コマンドはこの JSON を読み、局面は `CliArgs::resolvePosition(params)`、エンジンは `CliArgs::resolveEngine(params)` で解決する。引数なし・`--help` のときの使い方の文（`CliCommandSpecs::usage()`）も同じ表から作る。範囲と既定値の一覧は `shogiboardq-cli describe` で確かめる。
- エラーは `CliOutput::fail(ErrorKind, message)` が `{"ok":false,"error":{"code":語,"message":"..."}}` を標準出力に書いて終了コード 1。JSON Lines コマンドでは `CliOutput::failEvent(ErrorKind, message)` が `{"event":"error","code":語,"message":"..."}` を出してから終了する。`code` はエラーの分類（[エラーの分類](#エラーの分類)）の語で、CLI が使うのは `invalid_argument`（引数の誤り・読めない局面）、`unknown_operation`（知らないコマンド）、`engine_not_found`、`engine_error`、`parse_error`（`convert-kifu` の棋譜が読めない・開始局面が不正）、`not_found`（`convert-kifu` の入力ファイルがない）、`render_error`（盤面を描けない。局面・`--last-move` の誤りは `invalid_argument`）、出力ファイルの問題（`OutputPath::check()` が分類を返す。`kifu.save` と MCP の `paths.py` も同じ分類。相対パスとディレクトリを指すときは `invalid_argument`、既存ファイルで `--overwrite` なしは `file_exists`（`error.hint` に対処）、親ディレクトリがないときは `not_found`、書き込みの失敗は `file_error`）。
- エンジンは設定ファイル（`SettingsCommon::settingsFilePath()`）の `[Engines]` に登録済みの名前だけを受け付ける（`EngineListSettings::loadEngines()`）。任意パスは受け付けない。エンジンのオプションは GUI と同じく設定ファイルの値を適用する（`UsiEngineSession::startAsync()` が `EngineProfileStore::launchOptions()` で読み、`UsiProtocolHandler::setLaunchOptions()` に渡す）。
- 局面は `--sfen`（`startpos` も可）と `--moves`（USI 手を空白区切り）で指定し、`CliArgs::resolvePosition(params)` が合法手だけを指して探索させる局面（`UsiPosition`。開始局面・手順・最後の局面もこの値から返す）にする（読めない手・指せない手は `invalid_argument`）。エンジンには `UsiPosition::command()`（平手なら `position startpos moves ...`、それ以外は `position sfen ... moves ...`）を送る。
- `--output-path` の既存ファイルは `--overwrite` が無ければ拒否する。ディレクトリ制限は MCP サーバー側で行う（CLI 単体では行わない）。
- 長時間コマンドは `SIGTERM`/`SIGINT` でも停止処理（エンジンへの `quit`）を行って終了する（Windows は stdin の `stop` のみ）。
- JSON の出力と棋譜の形式は表示言語に依存しない。`render-board` だけは、GUI と同じ表示言語・棋譜表記・書体の設定（`ApplicationTranslations`・`ApplicationFonts`）を描画前に適用し、「手番」などの表示と段の表記（一〜九／a〜i）を GUI に合わせる。

## 4. アプリ側自動化 API（--automation）

### 起動

```
ShogiBoardQ --automation [--automation-socket PATH]
```

既定では無効。有効時は `QLocalServer` でローカルソケットを開き、所有者だけがアクセスできる（`QLocalServer::UserAccessOption`）。

| 項目 | 値 |
|---|---|
| ソケットパス（Linux/macOS） | `--automation-socket` → 環境変数 `SHOGIBOARDQ_AUTOMATION_SOCKET` → `QStandardPaths::RuntimeLocation`（`$XDG_RUNTIME_DIR`）`/shogiboardq/automation.sock`（`AutomationEndpoint::defaultSocketPath()`）。RuntimeLocation が取れなければ設定のフォルダの `automation.sock`（共有の一時ディレクトリの固定名は使わない）。絶対パスに限り、Unix ドメインソケットの制限と Qt が bind に使う一時フォルダの分で Linux は 99 バイト、macOS は 95 バイト以内。ソケットのフォルダは、無ければ所有者専用（0700）で作り、あれば自分の持ち物でほかの利用者が書けないことを確かめる（権限は変えない）。前回の残骸は、ソケットで待ち受けがないときだけ消し、ほかのアプリが待ち受けていれば起動しない。終了時のエンドポイントファイルは自分の pid のものだけ消す |
| ソケット名（Windows） | 環境変数 `SHOGIBOARDQ_AUTOMATION_SOCKET` → `shogiboardq-automation-<ユーザー名>`（名前付きパイプ） |
| エンドポイント情報 | 設定ファイル（`SettingsCommon::settingsFilePath()`）と同じフォルダの `automation-endpoint.json`（`AutomationEndpoint::endpointFilePath()`。`{"socket":..., "pid":..., "version":...}`）。起動時に書き、終了時に削除する。MCP サーバーはこれを読んで接続先を決める |
| プロトコル | 改行区切りの JSON-RPC 2.0（1 行 1 メッセージ、UTF-8）。バッチは未対応（`-32600`） |
| メッセージの上限 | 1 メッセージ 8 MiB（`AutomationEndpoint::kMaxMessageBytes`。棋譜テキストを含む）。超えると `-32600`（`invalid_request`）を返して切断する |
| スレッド | すべてメインスレッド。1 リクエストを同期処理して応答する |

### メソッド一覧

| メソッド | params | result | 主なエラー |
|---|---|---|---|
| `app.ping` | - | `{pong:true}` | - |
| `app.version` | - | `{version, qt, api:1}` | - |
| `app.state` | - | `{ui_state, play_mode, current_ply, total_plies, sfen, kifu_file, dirty, board_flipped, dialogs:[...], engines:{black,white}, csa?:{...}}` | - |
| `app.quit` | - | `{ok:true}`（応答後に終了。テストハーネス用で MCP ツールには出さない） | - |
| `position.get` | - | `{sfen, start_sfen, ply, moves[]}` | - |
| `position.set` | `{sfen, discard_unsaved?}` | `{sfen}` | `-32602` 不正 SFEN、`-32004` 未保存 |
| `kifu.load` | `{path?, text?, discard_unsaved?}` | `{total_plies, start_sfen, kifu_file, warnings?}` | `-32602` 相対パス・`path` と `text` の一方だけを渡していない、`-32005` ファイルなし、`-32004` 未保存、`invalid_state` 対局・検討・解析・局面編集の間、`-32006`（`parse_error`）読込失敗（理由をメッセージに含め、エラーダイアログは出さない） |
| `kifu.save` | `{path, overwrite?}` | `{path, format}` | `-32602` 相対パス・ディレクトリを指す（`OutputPath::check`）、`-32003`（`file_exists`）既存ファイル、`-32005` ディレクトリなし、`-32003`（`file_error`）書き込みの失敗・.kif・.ki2 に Shift_JIS で表せない文字（確認ダイアログは出さず、文字を示して `.kifu`・`.ki2u` を案内） |
| `kifu.get` | `{format?, line?: "current"\|"main", from_ply?, max_moves?, max_chars?}` | `{format, total_plies, line, moves:[{ply,text,usi,time,comment}], text?, truncated}`（`moves` は表示中の手順。`line="main"` で本譜） | - |
| `kifu.goto` | `{ply}` | `{ply, sfen}` | `-32602` 範囲外 |
| `action.list` | - | `{actions:[{name,text,enabled,checked,checkable}]}` | - |
| `action.trigger` | `{name}` | `{name, triggered:true, queued:true, interaction}`（応答後に実行） | `-32001` 許可リスト外、`-32002` 無効状態、`-32005` 存在しない |
| `board.click` | `{target?, file, rank, button?: "left"\|"right"}` | `{target, file, rank, queued:true, interaction}`（応答後に実行） | `-32602` 不正座標、`-32002` 操作不可、`-32005` 盤なし |
| `dialog.clickButton` | `{target, widget? または text?, index?}` | `{object_name, text, queued:true, interaction}`（応答後に実行） | `-32602` 不正引数、`-32002` 操作不可、`-32005` ボタンなし |
| `screenshot.capture` | `{target?: "main"\|objectName\|タイトル, output_dir?}` | `{path, width, height}` | `-32602` 相対の `output_dir`、`-32005` 対象なし |
| `dialog.list` | - | `{windows:[{object_name,class,title,visible,modal,active,main,width,height}]}` | - |
| `dialog.close` | `{target}` | `{closed:true, object_name, title, queued:true, interaction}`（応答後に閉じる） | `-32005` 対象なし |
| `widget.text` | `{target?, widget?, max_rows?, max_widgets?, include_children?}` | `{widgets:[{object_name,class,text?,items?,rows?}]}` | `-32005` 対象なし |
| `ui.interaction` | `{id}` | `{interaction, state: "pending"\|"running"\|"done"\|"failed"\|"cancelled", error?:{code, message, data:{slug, hint?}}}`（予約した UI 操作の状態） | `-32005` 知らない番号・古くて消えた番号 |

引数はメソッドごとの表 `AutomationMethodSpecs`（`src/headless/automationmethodspecs.{h,cpp}`）に宣言する。`AutomationDispatcher::handleRequest()` はメソッドを呼ぶ前に `ParamSpecs::validate()` で表と照らし、知らない引数・必須の欠け・型違い・範囲外・選択肢外を `-32602`（`error.data.hint` は `shogiboardq-cli describe` の案内）で返し、既定値を補った引数をメソッドに渡す。各メソッドは補われた値を読むだけで、`AutomationParams`（`requireString`・`optionalInt`・`requireInt`）は範囲が画面の状態で決まる引数（表の行・列、コンボやスピンの値など）や操作によって必須になる引数（`dock.configure` の `area`・`relative_to`）にだけ使う。表は `shogiboardq-cli describe` の `methods` に JSON Schema として出る。

`action.trigger` の許可リストは `src/automation/automationactionpolicy.cpp` で管理する。終了（`actionQuit`）、上書き保存（`actionSave`）、Web サイトを開く動作は除外する。言語切替（システム・日本語・英語・簡体字・繁体字の `actionLanguage*`）とドックレイアウト保存・初期化は許可する。言語はアプリ再起動後に反映する。

#### 応答の後に行う UI 操作

UI を操作するメソッド（`board.click`・`dialog.clickButton`・`action.trigger`・`dialog.close`・`widget.showDock`・`widget.click`・`widget.setValue`・`widget.clickCell`・`widget.editCell`・`dock.configure`・`menu.select`・`branch.click`）は、応答を書いた後のイベントループで操作する。これによりモーダルダイアログを開く操作（`QDialog::exec()` の入れ子ループ）でも呼び出し側が応答を受け取れ、続けて `dialog.list` / `widget.text` / `screenshot.capture` で内容を確認できる。

- 予約は `AutomationInteractions::queue(QueuedInteraction{target, check, perform}, response)`（`src/automation/automationinteractions.h`）1つで行う。`check` は予約の時点（失敗すればその場でエラーの応答）と、実行の直前（応答の後に対象が隠れた・無効になった・別のダイアログに遮られた・メニューが閉じた、など）の2回走る。メソッドは同じ検査を予約の前に書かない。
- 応答には `"queued": true` と操作の番号 `"interaction"` が付く。`ui.interaction {id}` で状態を引ける: `pending`（未実行）、`running`（実行中。開いたモーダルダイアログが閉じるまで）、`done`、`failed`（`error` に `code`・`message`・`data.slug`。直前の検査や操作が投げた `AutomationError` の分類）、`cancelled`（実行前に対象が消えた。`invalid_state`）。
- 実行時の失敗は握りつぶさずに記録する。記録は新しいものから `AutomationInteractions::kHistoryLimit`（256）件まで残し、それより古い番号は `-32005`（`not_found`）になる。
- `app.quit` だけはこの仕組みを使わず、`AutomationDeferredCall` で応答の後に終了する。
- マウス入力（盤・表のセル・分岐ツリーのクリック、ドックのタイトルバーのドラッグ）は `AutomationInput::sendClick()`・`sendDrag()`（`src/automation/automationinput.h`）で、通常の移動・押す・離すのイベントとして送る。
- 部品の読み（`widget.text`）と書き（`widget.setValue`）は、型ごとのアダプタの表 `AutomationWidgetAdapters::describe()`・`writer()`（`src/automation/automationwidgetadapters.h`）で行う。表は上の行から当てはめる（派生クラスの行を基底クラスより先に置く）。`writer()` は型・値・範囲を受付の時点で確かめ、返した操作は実行時に書けなくなっていれば（読み取り専用になった・項目が隠れたなど）失敗する。型を足すときは表に1行足す。

MCP サーバーは `AppClient.call_queued()` で操作の実行を待つ（5 章）。

#### 未保存の変更・読み込みの失敗・入れ子の要求

`kifu.load` / `position.set` は対局・検討・解析・局面編集の間は棋譜を置き換えず `invalid_state` を返す（GUI の「開く」と同じく待機中だけ。`AutomationCommands::requireReplaceableRecord`）。未保存の変更があるとき `discard_unsaved` 無しでは `-32004` を返し、確認ダイアログは出さない。読み込み失敗の通知も `UiNotificationService::ScopedCapture` でダイアログを出さずに記録し、`-32006`（`parse_error`）の理由として返す。ハンドラの実行中（入れ子のイベントループ）に届いた別の要求は実行せず `-32002` を返し、処理中に切断したソケットはハンドラが戻ってから削除する。

### エラーの分類

エラーの分類は `ErrorKind`（`src/headless/errorkinds.{h,cpp}`）の表1つで決め、CLI・自動化 API・MCP サーバーで同じ失敗は同じ語（slug）にする。`ErrorKinds::slug(kind)` が語、`ErrorKinds::rpcCode(kind)` が JSON-RPC のコード（`AutomationErrorCode` の値）を返し、`ErrorKinds::toJson()` が全分類を `shogiboardq-cli environment` の `error_kinds` に出す。

| `ErrorKind` | 語（slug） | JSON-RPC のコード | 意味 |
|---|---|---|---|
| `InvalidJson` | `invalid_json` | `-32700` | 要求が JSON でない |
| `InvalidRequest` | `invalid_request` | `-32600` | JSON-RPC の形が正しくない・バッチ・大きすぎる |
| `UnknownOperation` | `unknown_operation` | `-32601` | 知らないメソッド・コマンド |
| `InvalidArgument` | `invalid_argument` | `-32602` | 引数の誤り（相対パスを含む） |
| `InternalError` | `internal_error` | `-32603` | 予期しない失敗 |
| `NotAllowed` | `not_allowed` | `-32001` | 許可されない操作 |
| `InvalidState` | `invalid_state` | `-32002` | 今の状態ではできない |
| `FileError` | `file_error` | `-32003` | ファイルの読み書きの失敗 |
| `FileExists` | `file_exists` | `-32003` | 上書きの許可なしに既存のファイルへ書こうとした |
| `UnsavedChanges` | `unsaved_changes` | `-32004` | 未保存の変更がある |
| `NotFound` | `not_found` | `-32005` | 対象（ファイル・ディレクトリ・ウィンドウ・部品など）が見つからない |
| `InvalidContent` | `parse_error` | `-32006` | 棋譜や局面の中身が読めない |
| `EngineNotFound` | `engine_not_found` | `-32005` | 登録エンジンが見つからない |
| `EngineError` | `engine_error` | `-32007` | エンジンの起動・探索の失敗 |
| `RenderError` | `render_error` | `-32603` | 盤面画像を描けない |

メソッドの実装は `AutomationError(ErrorKind, message, hint)` を投げ、`AutomationDispatcher::makeError(id, kind, message, hint)` が `error.code` に分類のコード、`error.data.slug` に分類の語（常に付ける）、`error.data.hint` に対処方法（あれば）を入れる。コードは複数の分類で共有することがある（`-32003`・`-32005`・`-32603`）ので、分類は `error.data.slug` で見分ける。

### アプリ側のクラス構成

| クラス | ファイル | 責務 |
|---|---|---|
| `AutomationServer` | `src/automation/automationserver.{h,cpp}` | `QLocalServer` の待ち受け、接続ごとの行バッファ、応答の書き出し、エンドポイント情報の作成・削除 |
| `AutomationEndpoint` | `src/headless/automationendpoint.{h,cpp}` | 既定のソケットの場所・エンドポイントファイル・1メッセージの上限（サーバーと CLI の `environment` で共有） |
| `AutomationDispatcher` | `src/automation/automationdispatcher.{h,cpp}` | JSON-RPC 2.0 の解析・検証、メソッド表、呼ぶ前の引数の検査（`AutomationMethodSpecs`）、エラー応答の生成（`makeError()` が `error.data.slug` を付ける。GUI 非依存で単体テスト可能） |
| `ErrorKind` / `ErrorKinds` | `src/headless/errorkinds.{h,cpp}` | エラーの分類と、語・JSON-RPC のコードの表（`slug()`・`rpcCode()`・`toJson()`）。CLI の `CliOutput::fail()`・`failEvent()` と `AutomationError` が使う |
| `ParamSpec` / `OperationSpec` | `src/headless/paramspec.{h,cpp}` | 引数1つの仕様（名前・型・必須・範囲・既定値・選択肢・説明）と操作1つの仕様。`ParamSpecs::inputSchema()`・`toJson()`（JSON Schema）と `validate()`（知らない引数の拒否・必須・型・範囲・選択肢の検査と既定値の補完）。CLI・自動化 API・MCP で共有する |
| `AutomationMethodSpecs` | `src/headless/automationmethodspecs.{h,cpp}` | 自動化 API の全メソッドの引数の表（`all()`・`find()`） |
| `AutomationContext` | `src/automation/automationcontext.h` | MainWindow 側の依存を注入する Deps 構造体。部品と状態（動作状態 `AppActivityState`・棋譜の文書 `KifuDocument`・`KifuNavigationState`）は非所有ポインタで受け取り、関数で受け取るのはメニューウィンドウ（widgets 層）のお気に入りと終了要求だけ |
| `AutomationCommands*` | `src/automation/automationcommands.cpp`・`automationcommands_{app,position,kifu,board,branch,menu,action,dialog,widget,dock,screenshot,clipboard,ui}.cpp` | 各メソッドの実装（メソッド `x.y` は `automationcommands_x.cpp`。登録の一覧と共有の読み取りは `automationcommands.cpp`）。既存の `KifuFileController` / `KifuNavigationController` / `UiStatePolicyManager` / `ScreenshotService` を呼ぶ。`kifu.get` の形式指定のテキストは `KifuExport::lines(*document, format)`、`moves` は分岐ツリーから作り、`app.state` の `kifu_file`・`engines`・`play_mode` は `KifuDocument::filePath()`・`players()` を読む |
| `AutomationActionPolicy` | `src/automation/automationactionpolicy.{h,cpp}` | `QAction` の許可リスト |
| `AutomationInteractions` / `QueuedInteraction` | `src/automation/automationinteractions.{h,cpp}` | 応答の後に行う UI 操作の予約（`queue()`）・実行の直前の再検査・結果の記録と `ui.interaction` の答え（`status()`）。次のイベントループで呼ぶ `AutomationDeferredCall`（直接使うのは `app.quit` だけ）もここ |
| `AutomationInput` | `src/automation/automationinput.{h,cpp}` | 通常のマウス入力（`sendClick()`・`sendDrag()`） |
| `AutomationWidgets` / `AutomationWidgetAdapters` | `src/automation/automationwidgets.{h,cpp}`・`automationwidgetadapters.{h,cpp}` | ウィンドウ・部品の検索と操作可否の検査（`requireInteractive()`）／部品の型ごとの読み（`describe()`）と書き（`writer()`）の表 |
| `ScreenshotService` | `src/ui/controllers/screenshotservice.{h,cpp}` | ウィンドウの PNG 保存。リリースビルドでも使える。F12 のショートカット（`DebugScreenshotShortcut`）はデバッグビルド専用 |

MainWindow には `startAutomationServer(socketPath)` の委譲メソッドだけを追加し、生成と依存注入は `MainWindowComposer::startAutomationServer()`（`src/app/mainwindowcomposer_automation.cpp`）で行う（[composition-root.md](composition-root.md)）。`KifuFileController` には確認ダイアログを出さない非対話 API（`loadKifuFile`、`loadKifuText`、`applySfenPosition`、`saveKifuToPath`、`hasUnsavedChanges`）と `exportLines(format)` を置く。

## 5. MCP サーバー（Python）

```
mcp/
  pyproject.toml            パッケージ定義（shogiboardq-mcp スクリプト）
  README.md                 利用手順と各クライアントの設定例
  shogiboardq_mcp/
    __main__.py             python -m shogiboardq_mcp → stdio サーバー
    server.py               低レベル Server。tools/list・tools/call・resources
    tooldefs.py             ツール定義（name / description / inputSchema / outputSchema / annotations）
    specs.py・specs.json    CLI と自動化 API の引数の表（shogiboardq-cli describe の出力を保存したもの）と、それからツールの inputSchema を作る関数
    handlers_cli.py         段階 1 ツール（CLI 呼び出し）
    handlers_kifu_analysis.py 棋譜全体解析のジョブ開始・結果ページ取得
    handlers_app.py         段階 2 ツール（自動化 API 呼び出し）
    cli_backend.py          CLI の探索と実行（同期コマンド／ジョブ用サブプロセス）
    jobs.py                 ジョブ管理（状態、イベント、停止、期限切れ）
    app_client.py           自動化 API クライアント（接続、再接続、アプリの自動起動。アプリのエラーの語は data.slug）
    environment.py          shogiboardq-cli environment の結果（環境変数の組ごとに1回読む）
    paths.py                パス方針（絶対パス、許可ディレクトリ、上書き）とエンドポイント・既定のソケットの解決
    formatting.py           人が読むテキスト要約の生成
  tests/                    pytest（mcp SDK のクライアントで stdio 経由にツールを呼ぶ）
```

- 低レベル `mcp.server.lowlevel.Server` を使い、`CallToolResult` に `content`（要約テキスト）と `structuredContent` を両方入れる。`outputSchema` は SDK が検証する。`structuredContent` を無視するクライアントでもテキストだけで使える。
- ツール定義は `annotations`（`readOnlyHint` / `destructiveHint` / `idempotentHint` / `openWorldHint`）を付ける。
- 引数は `inputSchema` で検証され（SDK）、追加で `paths.py` がパス方針を検査する。SDK はスキーマの既定値を補わないので、`server.with_defaults()` が呼び出し前に補い、ハンドラは既定値のリテラルを持たない。
- 汎用GUI操作のツールは `tooldefs._tool(..., unrestricted=True)` で宣言し（`UNRESTRICTED_TOOLS` に入る）、`dispatch_tool` が `server.require_unrestricted_ui()` でまとめて検査する（`SHOGIBOARDQ_ALLOW_UNRESTRICTED_UI=1` でなければ `not_allowed`）。
- UI を操作するツール（`trigger_action`・`click_board_square`・`click_dialog_button`・`close_dialog`・`show_dock`・`configure_dock`・`click_widget`・`set_widget_value`・`click_table_cell`・`edit_table_cell`・`select_menu_action`・`click_branch_node`）は `AppClient.call_queued(method, params)` で呼ぶ。応答の `interaction` の番号で `ui.interaction` を引き、操作が実行されるまで最大 `QUEUED_WAIT`（3 秒）待つ。`failed`・`cancelled` ならアプリの分類の語の `AppError` を投げ（実行時の失敗もツールのエラーになる）、`done` か `running`（開いたモーダルダイアログがまだ開いている）か待ち切れなければその `state` を結果に足して返す。出力スキーマに `queued` を持つツールには `tooldefs._tool()` が `interaction`・`state`（`QUEUED_OUTPUT`）と説明の注記（`QUEUED_NOTE`）を足す。
- 引数を CLI のコマンドや自動化 API のメソッドに渡すツールは、`inputSchema` を C++ の表から作る（`tooldefs._spec(kind, 名前, drop=..., override=..., extra=...)`。表は `shogiboardq-cli describe` の出力を保存した `specs.json`。引数名は表と同じ）。ツールが変えるのは、出さない引数（ジョブが付ける `stdin_control`）、狭めた範囲や MCP の許可ディレクトリに触れる説明、MCP だけの引数（`max_chars` など）だけ。説明の中の自動化 API のメソッド名・CLI のコマンド名はツール名に置き換える。CLI には `--params-json` で引数をそのまま渡し（`CliTools._command()`）、フラグを組み立てない。表を変えたら `python3 -m shogiboardq_mcp.specs <cli>` で `specs.json` を作り直す（`mcp/tests/test_param_specs.py` が古さを検査する）。ジョブの開始で CLI がエラーのイベントを出したら、その語（`invalid_argument` など）をそのまま返す。
- 環境変数:

| 変数 | 意味 |
|---|---|
| `SHOGIBOARDQ_CLI` | `shogiboardq-cli` のパス。未指定なら `SHOGIBOARDQ_EXECUTABLE` と同じディレクトリ、次に `PATH` を探す |
| `SHOGIBOARDQ_EXECUTABLE` | `ShogiBoardQ` のパス。段階 2 ツールでアプリが起動していないときに `--automation` 付きで起動する |
| `SHOGIBOARDQ_AUTOMATION_SOCKET` | 自動化ソケットのパス（アプリ側と共通） |
| `SHOGIBOARDQ_ALLOWED_DIRS` | 読み書きを許可するディレクトリ（`os.pathsep` 区切り。既定はホームディレクトリ） |
| `SHOGIBOARDQ_OUTPUT_DIR` | スクリーンショットなど出力先未指定時のディレクトリ（既定は一時ディレクトリ配下の `shogiboardq-mcp`） |
| `SHOGIBOARDQ_QUIT_APP_ON_EXIT` | `1` のとき、サーバーが起動したアプリを終了時に閉じる（既定は残す） |
| `SHOGIBOARDQ_CONFIG_HOME` | 設定の置き場所の親フォルダ（アプリ・CLI と共通）。指定するとその下の `ShogiBoardQ/` を使い、`ShogiBoardQ.ini` と `automation-endpoint.json` もそこに置く。既定は Qt の標準の場所 |
| `SHOGIBOARDQ_DATA_HOME` / `SHOGIBOARDQ_CACHE_HOME` | データ（詰将棋の進捗）・キャッシュ（駒音・詰将棋）の置き場所の親フォルダ（アプリ側の変数。意味は `SHOGIBOARDQ_CONFIG_HOME` と同じ） |

`XDG_CONFIG_HOME` などは Linux の Qt だけが見る。macOS・Windows では効かないので、テストや撮影で普段の設定を
書き換えないよう、`SHOGIBOARDQ_*_HOME` も指定して隔離する（`mcp/tests/conftest.py`・`tests/gui/run.py`・`scripts/docs/launch.sh`）。

- ソケット・エンドポイントファイルの場所と1メッセージの上限は Python で計算せず、`environment.environment()` が `shogiboardq-cli environment` を実行して読む（結果は、場所を変える環境変数 `SHOGIBOARDQ_CONFIG_HOME`・`SHOGIBOARDQ_AUTOMATION_SOCKET`・`XDG_CONFIG_HOME`・`XDG_RUNTIME_DIR`・`HOME`・`LOCALAPPDATA`・`USERNAME`・`SHOGIBOARDQ_CLI`・`SHOGIBOARDQ_EXECUTABLE` の値の組ごとに1回だけ実行して覚える）。`paths.endpoint_file()` はその `endpoint_file`、`paths.default_socket_path()` は `SHOGIBOARDQ_AUTOMATION_SOCKET` があればその値、なければ `socket_path` を返す。自動化 API のソケットの読み込みの上限は `max_message_bytes`。
- アプリへの接続は `SHOGIBOARDQ_AUTOMATION_SOCKET` → `automation-endpoint.json` の `socket` → 既定パスの順に試し、失敗したら `SHOGIBOARDQ_EXECUTABLE` を起動して最大 30 秒待つ。エンドポイントファイルと既定パスの場所、読み込みの上限は `shogiboardq-cli environment` から得るので、段階 2 ツールにも `shogiboardq-cli` が要る（実行できないときは、エンドポイントファイルと既定パスを候補から外す）。接続断は次の呼び出しで再接続する。
- ツールのエラーは `ToolError(code, message, data)` で、`isError: true` の結果のテキスト `Error (code): message`（`data.hint` があれば `Hint: ...` を続ける）にする。`code` は [エラーの分類](#エラーの分類) と同じ語を使う（パスの誤りは `invalid_argument`、許可ディレクトリの外は `not_allowed`、ファイル・ディレクトリ・ジョブがないときは `not_found`、既存ファイルは `file_exists`）。CLI のエラーは CLI の `code` を、アプリのエラーは `error.data.slug`（なければ `environment()` の `error_kinds` でコードから引いた語。表にないコードは `app_error`）をそのまま返す。MCP サーバーだけが持つ失敗には `app_unavailable`・`app_disconnected`・`app_response_too_large`・`app_error`・`cli_not_found`・`cli_failed`・`job_failed`・`timeout`（アプリの応答待ちの時間切れ）・`too_many_jobs` を使う（`mcp/tests/test_environment.py` が、ソースの `ToolError` の語が分類の表かこの一覧にあることを検査する）。
- 1 回の JSON-RPC 呼び出しには 30 秒のタイムアウトを置く（GUI がモーダルダイアログで止まっている場合に備える）。

## 6. ジョブの状態遷移

ジョブは MCP サーバーのプロセス内に保持する（サーバー再起動で消える）。局面解析・棋譜全体の解析・詰み探索・生成は、CLI のサブプロセス 1 つが 1 ジョブに対応する。棋譜全体の解析は CLI の `analyze-kifu` が1つのエンジンで局面を順に解析し、局面ごとの結果（先手から見た評価値つき）を `position` イベントで返す。`text` で渡した棋譜は一時ファイルにし、CLI が読み込んだ（`started` を出した）後で消す（`JobManager.start(..., until="started")`）。

```
            start tool            CLI が result/finished を出力
 (none) ───────────────▶ running ───────────────────────────▶ finished
                           │  │
                           │  └── CLI が error を出力 / 異常終了 ─▶ failed
                           │
                           └── stop / cancel ─▶ stopping ─▶ stopped
                                 （stdin に stop、6 秒待って terminate、さらに 3 秒で kill）

 finished / failed / stopped は 30 分後に一覧から消える。同時実行は 4 ジョブまで（超過は error）。
```

| ジョブ種別 | 状態ツールが返す内容 |
|---|---|
| analysis | `state, elapsed_ms, engine, depth, nodes, nps, lines:[{multipv, score_cp, score_mate, depth, pv[], pv_text}], bestmove, ponder` |
| kifu_analysis | `state, completed, total_positions, current_ply, positions:[{ply,sfen,played_move,lines,bestmove,score_cp_black,score_mate_black,partial}], offset,next_offset,truncated,partial` |
| mate | `state, elapsed_ms, engine, status, pv[], plies` |
| tsume_generation | `state, elapsed_ms, generated, found, rejected, inconclusive, positions:[{sfen,pv}]` |

## 7. 安全性

- 自動化ソケットは所有者限定（Unix ドメインソケットの権限 / 名前付きパイプの UserAccess）。TCP や HTTP は開かない。
- ファイル引数は絶対パスのみ。`SHOGIBOARDQ_ALLOWED_DIRS`（既定: ホーム配下）の外は拒否する。上書きは `overwrite: true` が無ければ拒否する。検査は MCP サーバー側（`paths.py`）で行い、CLI/アプリ側は絶対パスの要求と既存ファイルの拒否だけを行う。
- 許可ディレクトリより下に `.` で始まる名前があるパス（シンボリックリンクをたどった先で判定）は `not_allowed` にする。既定の許可ディレクトリはホーム全体なので、これがないと操られたエージェントが資格情報（`~/.netrc` など）を読み、起動時に実行されるファイル（シェルの起動ファイルなど）に書ける（2026-10-23 のセキュリティ監査の S1・S2）。隠しディレクトリを使うときは、そのディレクトリ自体を許可ディレクトリに入れる。
- 拡張子は、棋譜の読み込みが `kifu_read_suffixes`、棋譜の書き出しが `kifu_write_suffixes`（読むだけの `.json` を含まない）、盤面画像が `image_write_suffixes` に限り、それ以外は `invalid_argument` にする。一覧は `shogiboardq-cli environment` から読む（`paths.allowed_suffixes()`）。
- クリップボードはほかのアプリと共有するので、`get_clipboard` も汎用GUI操作と同じく `SHOGIBOARDQ_ALLOW_UNRESTRICTED_UI=1` が要る。
- エンジンは `[Engines]` に登録済みの名前だけ。実行ファイルパスは受け付けない。汎用GUI操作を許したときも、エンジン登録ダイアログの「追加」（`addEngineButton`。選んだ実行ファイルを起動して確かめる）は `automationSensitive` で押せない（2026-10-23 のセキュリティ監査の S8）。それ以外の GUI 操作（通信対局の接続先の指定など）はアプリと同じ権限で行える。
- 自動化 API は既定で無効。`--automation` を付けたときだけ有効になり、起動時に標準出力へソケットパスを出す。
- `trigger_action` は許可リスト方式。終了・上書き保存・Web サイトを開く動作は除外する。動的メニューは作成元が明示的に公開したものだけ操作できる。
- 出力サイズは既定で抑える（棋譜テキスト 30,000 文字、手順 200 手、`convert_kifu` の手の一覧 2000 手・対局情報と警告 200 件、読み筋 20 手、生成局面 20 件など）。
- ツールの引数は、ハンドラを呼ぶ前に inputSchema で検査する（`server.check_arguments`。mcp の lowlevel Server は検査しない）。知らない引数、MCP で狭めた範囲、`oneOf` の外れは `invalid_argument`。
- 1回で終わる CLI の呼び出しは同時に4つまで（`cli_backend.MAX_CONCURRENT_COMMANDS`。ジョブとは別）。標準出力は 64 MiB まで読み、超えたら止める。要求が取り消されたら CLI（とエンジン）を終わらせる。

## 8. ビルド構成とソース配置

CLI が GUI 本体と同じクラスを使えるように、`CMakeLists.txt` のソースを OBJECT ライブラリ `shogiboardq_objects` にまとめ、`ShogiBoardQ` はこれを直接リンクする（従来と同じく全オブジェクトを含む）。`shogiboardq-cli` は同じオブジェクトから作る静的ライブラリ `shogiboardq_static` をリンクし、参照したオブジェクトだけが取り込まれる。新しいテストもこの静的ライブラリをリンクできる。

| ディレクトリ | 内容 |
|---|---|
| `src/headless/` | CLI・自動化 API・MCP が共有する GUI 非依存の部品: 引数の表（`ParamSpec`・`AutomationMethodSpecs`）、エラーの分類の表（`ErrorKind`・`ErrorKinds`・`Failure`）、書き出し先の検査（`OutputPath`）、自動化 API の接続先（`AutomationEndpoint`）、共通サービス（`KifuConversionService`、`EngineCatalog`、`MateSearchRunner`、`TsumeVerificationRunner`。局面の検証は `src/core/` の `SfenValidator`、その JSON は `SfenValidationJson`。局面の列の解析 `EngineAnalysisRunner` は GUI の棋譜解析と共通なので `src/analysis/`） |
| `src/automation/` | 自動化 API（`AutomationServer` ほか） |
| `src/cli/` | `shogiboardq-cli` のエントリーポイントとコマンド実装（`CliCommands*`）、盤面画像（`BoardImageRenderer`）、コマンドの引数の表 `CliCommandSpecs` と引数の解釈 `CliArgs`。JSON 出力と stdin の `stop` 監視 |
| `mcp/` | Python パッケージとテスト |

`src/headless/` は core〜analysis だけを使い（GUI にも通信対局にも依存しない）、`src/automation/` は `app/`・`dialogs/`・`widgets/` に依存しない（`views/ShogiView` は `board.click` などで盤を探すために使う）。`src/cli/` は GUI の層と `src/automation/` に依存しない（盤面画像の `boardimagerenderer.cpp` だけは盤を描くために `views/` を使う）。どれもレイヤー依存テスト（`tst_layer_dependencies`）が検査する。

## 9. テスト

| テスト | 種別 | 内容 |
|---|---|---|
| `tst_automation_dispatcher` | C++（Qt Test） | JSON-RPC の解析・エラーコードと `error.data.slug`・メソッド表、予約した UI 操作の結果の記録（実行・直前の検査での失敗・実行前の取り消し） |
| `tst_param_specs` | C++ | 引数の表の検査（既定値の補完・知らない引数・範囲・型の拒否）、JSON Schema の出力、CLI のオプションの変換、登録した全メソッドに表があること、ディスパッチャが呼ぶ前に検査すること、エラーの分類の語が重ならず表の並びが `ErrorKind` と同じこと |
| `tst_kifu_conversion_service` | C++ | fixtures の KIF/KI2/CSA/JKF/USI/USEN を相互変換し、手順・初期局面・SFEN 列を検証 |
| `tst_sfen_validator` | C++ | 妥当／不正な SFEN、王手検出、合法手数 |
| `tst_usilineparser` | C++ | `info` 行などの受信行の解析（`src/engine/usilineparser`。multipv、score cp/mate、bound、pv、info string、崩れた行）と JSON 出力 |
| `tst_board_image_renderer` | C++ | offscreen で PNG を生成しサイズと非空を検証 |
| `mock_mate_engine` | 補助実行ファイル | `go mate <ms>` に Hayanagi の詰み探索で答える USI エンジン（生成・余詰検査・詰み探索の結合テスト用） |
| `tst_mcp_python` | pytest（CTest から実行） | `mcp` SDK のクライアントで stdio サーバーを起動し、段階 1 ツール（CLI）と段階 2 ツール（`QT_QPA_PLATFORM=offscreen` で起動したアプリ）を呼ぶ。エンジンは `hayanagi`（解析）と `mock_mate_engine`（詰み）を一時設定ファイルに登録して使う |

`tst_mcp_python` は Python3 と `mcp`・`pytest` が見つかるときだけ登録される。`mcp/tests/test_param_specs.py` は `specs.json` が `shogiboardq-cli describe` の出力と同じことと、表から作ったツールが表の引数（型・選択肢・既定値・pattern・範囲（狭めるのは可）・必須）を出していることを確かめ、ハンドラが呼ぶメソッドとコマンドが表にあることも確かめる。`mcp/tests/test_environment.py` は `shogiboardq-cli environment` の出力（場所・上限・語の重ならない分類の表）と、MCP サーバーの語が分類の表と一致すること、`AppError` が `data.slug`（なければコード）から語を決めることを確かめる。`mcp/tests/test_app_client.py` は `call_queued()` が操作の実行を待つこと、`running` を返すこと、`failed`・`cancelled` をアプリの語のエラーにすることを確かめる。実エンジンでの確認は `SHOGIBOARDQ_TEST_KOMORING` があれば追加で行う。

`mcp/tests/test_csa_game.py` は実際の shogi-server を使い、接続ダイアログ・盤面・終局・棋譜保存を stdio MCP 経由で検証する。
`SHOGIBOARDQ_TEST_CSA_SERVER` にサーバースクリプトを指定する。実エンジン対局は `SHOGIBOARDQ_TEST_USI_ENGINE`、
ShogiHome との相互接続は `SHOGIBOARDQ_TEST_SHOGIHOME_CDP` も指定する。
環境構築・実行例・確認範囲は [CSA検証記録](csa-game-mcp-audit-2026-09-27.md) を参照。

テストでは、`list_dialogs` で見つけたダイアログを `selector` で指す。メッセージボックス（`QMessageBox`）は
オブジェクト名が無く、macOS の Qt はタイトルも無視する（空になる）ため、タイトルでは探せない。
`mcp/tests/conftest.py` の `message_box(session, text)` で本文から探し、返るセレクタを `click_dialog_button`・
`close_dialog` に渡す。

## 10. 対応クライアント

設定例（`claude_desktop_config.json`、`claude mcp add`、`.cursor/mcp.json`、`.vscode/mcp.json`、Gemini CLI の `settings.json`、Codex CLI の `config.toml`）は [mcp/README.md](../../mcp/README.md) と利用ガイドに記載する。いずれも stdio で `python3 -m shogiboardq_mcp` を起動し、環境変数で実行ファイルの場所を渡す。

## 11. 制限事項と拡張候補

- Streamable HTTP は実装しない（必要になれば localhost 限定・トークン必須で追加する）。
- CSA 通信対局の専用API、対局開始・USI形式の着手専用API、評価値グラフ画像の取得は未対応。
  CSA対局は接続ダイアログと盤面のクリックで操作できる。`get_app_state` の `csa` は通信対局を一度始めた後（司令塔 `CsaGameCoordinator` が待機状態でないとき）に返り、
  `state`、`is_my_turn`、`is_black`、`is_human`、先後の `remaining_ms`・`consumed_ms` を含む。
  残時間はサーバー確認済みの最新着手時点（開始時を含む）の値で、思考中の経過時間を差し引く表示時計とは異なる。
- Windows の名前付きパイプ接続は実装済みだが未検証。
- 拡張候補: 解析結果のグラフ画像専用API、`prompts/list` による定型手順、局面集の一括操作専用API。GUIの画像保存・局面集読込と履歴選択にはUI操作ツールを利用できる。
