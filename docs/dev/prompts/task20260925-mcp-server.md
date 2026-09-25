# 依頼：ShogiBoardQ を複数の AI クライアントから使える MCP サーバーにする

この依頼は、過去の会話を参照できない状態でも実行できるようにまとめています。作業前に `CLAUDE.md`、`AGENTS.md`、`docs/dev/threading-guidelines.md`、`docs/dev/ownership-guidelines.md`、`docs/dev/deps-hooks-refs-pattern.md`、`docs/dev/settings-service.md` を読み、`git status` で作業ツリーがクリーンなことを確認してください。説明・報告・コミットメッセージは日本語、AI クライアントが読むツール説明は英語です。

## 目的

ShogiBoardQ（Qt6/C++17 の将棋GUI。USI エンジン連携、棋譜管理、解析、詰将棋の探索・対局・局面生成、CSA 通信対局）の機能を、**Model Context Protocol（MCP）** 経由で AI クライアントから使えるようにします。利用するクライアントは Claude Desktop / Claude Code に限らず、Cursor、VS Code（GitHub Copilot）、Gemini CLI、Codex CLI、その他 MCP 対応のエージェントを想定します。したがって次を守ってください。

- MCP 仕様（実装時点の最新安定版）に忠実に実装し、特定ベンダー専用の機能に依存しない。基本トランスポートは **stdio**（全クライアントが対応）。Streamable HTTP は任意の追加機能とし、有効化した場合は localhost 限定にする。
- ツールは `inputSchema` を厳密な JSON Schema で定義し、`description` は英語で簡潔に、引数の単位・形式（SFEN、USI 手、パス）を明記する。結果はテキスト（人が読める要約）を必ず返し、可能なら `structuredContent`（`outputSchema` 付き）も返す。`structuredContent` を無視するクライアントでも使えること。
- 長時間処理（解析、詰み探索、局面生成）は、ジョブ ID を返す開始ツールと、状態・結果を返すツールに分ける。クライアントによってはツール呼び出しが約60秒で打ち切られるため、1回の呼び出しは数秒以内に返す。進捗通知（`notifications/progress`）は対応クライアント向けの補助に留める。
- 出力は大きくしない（棋譜や解析結果は件数・手数で絞り込めるようにし、既定で先頭の一定件数だけ返す）。エラーは `isError: true` と対処方法が分かるメッセージで返す。
- サンプリング（`sampling/createMessage`）や `elicitation` に依存しない。`roots` があれば尊重するが無くても動く。

## 推奨する構成（2層）

MCP のプロトコル実装を Qt アプリ本体に入れず、次の2層にします。

1. **アプリ側の自動化 API（C++、Qt）**：ShogiBoardQ に `--automation` オプション（既定は無効）で起動するローカルサーバーを追加し、改行区切りの JSON-RPC 2.0 で機能を公開する。トランスポートは `QLocalServer`（Unix ドメインソケット／Windows 名前付きパイプ。Qt Network は既にリンク済み。`Qt6HttpServer` はこの環境に無いので使わない）。ソケットのパスは `$XDG_RUNTIME_DIR/shogiboardq/automation.sock` 相当（無ければ `QStandardPaths::RuntimeLocation`）とし、所有者のみアクセス可にする。
   - 新しいコードは `src/automation/` に置く（例：`automationserver.{h,cpp}` 接続管理、`automationdispatcher.{h,cpp}` メソッド表と JSON 変換、`automationcommands_*.cpp` 機能別のコマンド）。MainWindow に処理を足さず、`MainWindowCompositionRoot` / `ServiceRegistry` / 既存の Wiring・Coordinator を Deps 構造体で注入する。
   - すべての UI 操作はメインスレッドで行う。`QLocalServer` のシグナルはメインスレッドで届くので、そこから既存のコントローラを呼ぶ。ワーカーには値オブジェクトだけを渡す。
   - 既存の入口を使う：メニュー動作は `mainwindow.ui` の `QAction` の objectName（例：`actionOpenKifuFile`、`actionAnalyzeKifu`、`actionTsumeShogiSearch`、`actionTsumeshogiGenerator`、`actionTsumePlay`、`actionSfenCollectionViewer`、`actionCopySFEN`、`actionSaveBoardImage`）、ダイアログ起動は `src/ui/wiring/dialoglaunchwiring.h`、棋譜入出力は `src/kifu/kifufilecontroller.h` と `src/kifu/formats/`（KIF/KI2/CSA/JKF/USI/USEN）、盤面画像は `src/board/boardimageexporter.h`、エンジン一覧は `EngineListSettings::loadEngines()`（`src/services/enginelistsettings.h`）、USI 通信は `src/engine/usi.h`（`Usi(nullptr, nullptr, nullptr, parent)` でモデル無しでも動く。`src/analysis/tsumeshogigenerator.cpp` が例）、詰み探索は `src/engine/tsumemateengine.h`、詰将棋生成は `src/analysis/tsumeshogigenerator.h`、余詰検査は `src/analysis/tsumeshogiverifier.h`、事前選別は `src/analysis/tsumeshogicandidatescreener.h`、問題集は `src/analysis/tsumecollection.h`。デバッグビルドの `src/services/debugscreenshotservice.h` はリリースでも使える形に一般化してよい。
2. **MCP サーバー（Python、公式 `mcp` SDK）**：`mcp/` ディレクトリに `pyproject.toml` 付きのパッケージ `shogiboardq_mcp` を作り、`python -m shogiboardq_mcp` で stdio サーバーとして起動できるようにする。各ツールは自動化 API への JSON-RPC 呼び出しに変換する薄い層に留める。アプリが起動していなければ、環境変数 `SHOGIBOARDQ_EXECUTABLE` で指定された実行ファイルを `--automation` 付きで起動する（未指定なら分かりやすいエラー）。接続断からの再接続、アプリ側エラーの変換、引数検証をここで行う。この環境には Python 3.14.7 と `mcp` 1.29.0 が入っている（`uv`/`pipx` は無い。Node 26 と npm もあるが Python を推奨）。

GUI が無くても使える機能（棋譜の形式変換、SFEN 検証、詰将棋の生成と余詰検査、エンジンによる解析・詰み探索）は、アプリ本体を起動しなくても使えるように、同じ非GUIクラスから作る **CLI 実行ファイル**（例：`shogiboardq-cli`、`QCoreApplication` か `QT_QPA_PLATFORM=offscreen` の `QApplication`）を追加し、MCP サーバーはこれらのツールを CLI 経由で提供してよい。どちらで提供するかはツールごとに設計文書に明記する。

## 公開するツール（最小セット）

名前は snake_case、英語説明、引数は JSON Schema。段階1（アプリ起動不要）と段階2（起動中のアプリを操作）に分けます。

段階1：
- `convert_kifu`：棋譜ファイルまたは文字列を KIF/KI2/CSA/JKF/USI/USEN/SFEN 列に変換して返す。
- `validate_sfen`：SFEN の妥当性と手番・持駒・王手の有無を返す。
- `list_engines`：設定済み USI エンジン（名前・パス・作者）を返す。
- `analyze_position` / `analysis_status` / `analysis_result`：指定エンジンで局面（SFEN と手順）を秒数・MultiPV 指定で解析する（ジョブ方式）。
- `search_mate` / `mate_status`：`go mate` に対応するエンジンで詰みを探索し、PV を返す。
- `generate_tsume` / `tsume_generation_status` / `stop_tsume_generation`：`TsumeshogiGenerator` の設定（目標手数、攻め駒・守り駒上限、配置範囲、探索時間、生成上限、最終手の複数解の許容）で局面を生成する。
- `verify_tsume`：局面と目標手数を受け取り、`TsumeshogiVerifier` の判定（Unique / Multiple / NoMate / WrongLength / Unknown）と主手順を返す。
- `render_board_image`：SFEN から PNG を書き出し、パスを返す。

段階2（起動中のアプリ）：
- `get_app_state`：モード（待機・対局中・解析中・詰将棋対局中など）、開いているダイアログ、現在の棋譜ファイル名。
- `get_position` / `set_position`：現在局面の SFEN と手順、局面の設定。
- `load_kifu` / `save_kifu` / `get_kifu`（形式指定、手数範囲指定）/ `goto_ply`。
- `trigger_action`：`QAction` の objectName を指定して実行（許可リスト方式。終了や上書き保存など危険な動作は除外）。
- `capture_screenshot`：メインウィンドウまたは指定ダイアログを PNG に保存しパスを返す。
- `list_dialogs` / `get_widget_text`：開発時の検証用に、ダイアログのラベル・テーブル内容を取得。

リソースとして `shogiboardq://position/current`（SFEN）、`shogiboardq://kifu/current`（KIF テキスト）、`shogiboardq://engines`（JSON）を提供する。プロンプト（`prompts/list`）は任意で、対応しないクライアントがあることを前提に必須機能にしない。

## 安全性

- 自動化ソケットとローカル HTTP は localhost・所有者限定。HTTP を実装する場合は起動時に生成するトークンを必須にする。
- ファイル引数は絶対パスのみ受け付け、設定で許可したディレクトリ（既定：ホーム配下）の外は拒否する。上書き保存は明示フラグが無ければ拒否する。
- エンジン起動は `EngineListSettings` に登録済みのものだけに限定し、任意の実行ファイルパスは受け付けない。
- 自動化 API は既定で無効。有効化方法とリスクを README と `docs/guide` に記載する。

## 実装の進め方

1. **設計文書を先に書く**：`docs/dev/mcp-server.md` に、2層構成、JSON-RPC メソッド一覧（名前・引数・戻り値・エラー）、MCP ツール一覧（名前・スキーマ・提供経路）、ジョブの状態遷移、安全性、対応クライアントとその設定例を書く。既存の `docs/dev/*.md` の書式に合わせる。
2. **段階1**：CLI と MCP サーバーの段階1ツールを実装する。`tests/` に Python の結合テスト（`mcp` SDK のクライアントセッションで stdio 経由にツールを呼ぶ）と、CLI の C++ 単体テスト（`tests/CMakeLists.txt` の `add_shogi_test`）を追加する。
3. **段階2**：`src/automation/` と MCP の段階2ツールを実装する。`QT_QPA_PLATFORM=offscreen` で起動したアプリに対する結合テストを追加する（`docs/dev/live-game-verification.md` の Xvfb 手順は不要にするのが目標）。
4. **文書と配布**：README、`docs/guide`（日本語）と `docs/en/guide`（英語）に利用手順を追加し、Claude Desktop（`claude_desktop_config.json`）、Claude Code（`claude mcp add`）、Cursor（`.cursor/mcp.json`）、VS Code（`.vscode/mcp.json`）、Gemini CLI、Codex CLI の設定例を載せる。各形式は公式文書で確認してから書く。
5. **検証**：`cmake --build build`、`ctest --test-dir build`、`python3 -m pytest mcp/tests`（または同等）、Claude Code からの実操作（`claude mcp add` で登録し、`convert_kifu` と `generate_tsume` を呼ぶ）。可能なら Claude 以外のクライアントを1つ以上で動作確認し、できなかった場合はその旨を報告する。

## 守るべき既存の規約

- MainWindow に処理を足さない。新しいクラスは `src/automation/`・`src/services/` などに置き、Deps/Hooks/Refs パターンで依存を注入する。
- `connect()` にラムダを使わない（メンバ関数ポインタ）。`signals:`/`slots:` の引数型は完全修飾。範囲 for は `std::as_const()`。所有権は親子関係か `std::unique_ptr`。メンバ初期化は NSDMI とコンストラクタ初期化子リストの使い分け。
- `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` と clazy level0/1 で警告を出さない。
- CMake のソース一覧は明示列挙（`CMakeLists.txt`、`tests/CMakeLists.txt`。`scripts/update-sources.sh` が参考）。
- `tr()` の文字列を追加・変更したら `cmake --build build --target update_translations` を実行し、`resources/translations/ShogiBoardQ_en.ts` に英訳を入れ、`resources/translations/baseline-unfinished.txt` を更新する。ツール説明やログは `tr()` の対象外。
- ダイアログを追加・変更したら `SettingsService`（`docs/dev/settings-service.md`）で設定を永続化する。
- デバッグログは `qCDebug(lc...)`（`src/common/logcategories.h`）を使う。
- テストを追加したら `scripts/update-test-summary.sh` で `docs/dev/test-summary.md` を更新する。

## 成果物と報告

- フェーズごとに日本語でコミットする（push はしない）。
- 最後に、実装したツール一覧、対応を確認したクライアント、未対応・制限事項、今後の拡張候補（CSA 対局、解析結果のグラフ画像など）を日本語で報告する。

## 環境

- リポジトリ：`/home/nakada/GitHub/ShogiBoardQ`（Qt 6.11.2、Widgets/Network/Concurrent/Multimedia/Sql/Charts/WebSockets が利用可能、`Qt6HttpServer` は無い）。ビルドは `cmake -B build -S . -DBUILD_TESTING=ON && cmake --build build`。
- Hayanagi サブモジュール（`Hayanagi/`）は詰将棋対局・余詰検査・事前選別で使う内蔵探索。変更しない。
- 詰み探索エンジンの実機テストには `SHOGIBOARDQ_TEST_KOMORING=/home/nakada/shogi/KomoringHeights-kh-v1.1.0/source/KomoringHeights-by-gcc` を使える（無い環境ではスキップされるように書く）。
- GUI の結合テストは `QT_QPA_PLATFORM=offscreen`、必要なら Xvfb（`docs/dev/live-game-verification.md`）。
