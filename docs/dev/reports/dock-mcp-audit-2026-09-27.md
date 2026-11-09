# ドック機能のMCP検証（2026-09-27）

対象: `docs/guide/dock.html` のドッキング、フローティング、タブ化と、関連する表示・固定・初期化。
Qt 6.11.2 / Linux の実アプリを、stdio MCPサーバー経由で操作した。
検証用の設定とソケットを一時ディレクトリに分離し、通常のユーザー設定は変更していない。

## 再現して修正した不具合

| 問題 | 再現手順 | 修正 |
|---|---|---|
| 棋譜解析ドックが初期化から漏れる | `AnalysisResultsDock` を浮動化し、`actionResetDockLayout` を実行すると浮いたまま残る | 全登録ドックを初期化対象とし、棋譜解析を下部・非表示へ戻す |
| ドック固定が再起動で解除される | `actionLockDocks` を有効化し、終了・再起動するとチェックと移動禁止が失われる | 固定切替時に `DockSettings::setDocksLocked` で保存 |
| MCPからモーダル画面の背後を操作できる | `actionSaveDockLayout` で入力ダイアログを開き、`actionResetDockLayout` を実行できる | QAction実行の予約時と実行時に、メインウィンドウの操作可否を確認 |

修正前のMCPテストでは、上の3件がそれぞれ失敗することを確認した。
修正後は同じ検査を通過した。

## MCPに追加した機能

- `list_docks` (`dock.list`): 非表示を含む全ドックの名前、配置エリア、浮動状態、タブ関係、表示領域、移動可否、表示メニューのチェック、グローバル座標を取得。
- `configure_dock` (`dock.configure`): 表示、閉じる、浮動化と位置・サイズ指定、上下左右への配置、タブ化、ドッキング中のタイトルバーへのドラッグ入力。
- `actionResetDockLayout` / `actionSaveDockLayout` を既存の `trigger_action` の対象へ追加。

`hidden` は閉じた状態、`exposed` はQtが報告する表示領域の有無を表す。
タブの裏側では `visible=true` のまま `exposed=false` となる。
`tabified_with` はQtが列挙する現在のタブであり、閉じたタブは含まれない。
ドラッグはQtのタイトルバーへのマウスイベント、その他の配置変更はQtのドックAPIを使用する。
操作は固定・許可エリア・モーダル状態を確認した上で予約し、完了を一覧取得で検査する。

## 検証範囲と結果

`mcp/tests/test_docks.py` は5シナリオを実行する。

1. 全12ドックを浮動化し、閉じる・再表示・四方向への再配置を検査。検討パネルの浮動中のコントロール操作とスクリーンショット、タブ化と前面切替も確認。
2. 全12ドックを浮動化して初期化。棋譜解析も含めた配置・表示状態と、繰り返し初期化後のタブ構成を確認。
3. 思考ドックのタイトルバーへマウスイベントを送り、ウィンドウ外へのドラッグで浮動化することを確認。
4. 固定中の浮動化・ドラッグ・配置・タブ化を拒否し、閉じる・再表示は可能なことを確認。再起動後も固定が残り、解除すると移動できることを確認。
5. 存在しないドック、自己タブ化、不完全な引数、非表示の相手へのタブ化、モーダル画面の背後の操作を拒否することを確認。

Qt Testの `tst_dock_layout` でも、保存レイアウトの浮動状態・座標・非表示・タブ関係と、起動時レイアウトの復元を実際のウィジェットで検査した。

| 検査 | 結果 |
|---|---|
| `cmake --build build -j 4` | 成功、コンパイラ警告なし |
| `ctest --test-dir build --output-on-failure -j 4` | 99 / 99 成功（MCPテストを含む） |
| ドックのstdio MCPテスト（offscreen） | 5 / 5 成功 |
| 同じMCPテスト（Xvfb / xcb、1920×1600） | 5 / 5 成功 |
| `git diff --check` | 成功 |

ローカルの検証ログと画面は `build/dock-audit/` に保存した。
`floating-consideration.png` は独立した検討パネル、`dock-stress-layout.png` は全パネルの配置操作後の画面。

再実行例:

```bash
SHOGIBOARDQ_CLI="$PWD/build/shogiboardq-cli" \
SHOGIBOARDQ_EXECUTABLE="$PWD/build/ShogiBoardQ" \
python3 -m pytest -q -p no:cacheprovider mcp/tests/test_docks.py

xvfb-run -a -s '-screen 0 1920x1600x24' env \
  SHOGIBOARDQ_TEST_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1 \
  SHOGIBOARDQ_CLI="$PWD/build/shogiboardq-cli" \
  SHOGIBOARDQ_EXECUTABLE="$PWD/build/ShogiBoardQ" \
  python3 -m pytest -q -p no:cacheprovider mcp/tests/test_docks.py
```

実際の複数モニター間移動、Wayland、Windows/macOSのウィンドウ装飾、OSネイティブの浮動タイトルバーのドラッグは今回の検証範囲外。
保存ダイアログの名前入力・保存済みメニューの操作はMCPテストには含めず、保存データの復元をQt Testで検証した。

## 残存テストプロセスの追加調査

利用者からCPU使用率の高いプロセスの報告を受け、ホスト側の起動引数を確認した。
6件は今回のドック検証とは別の `pytest-50` 配下の検討機能テストで使った
自動化ソケットを持ち、親がデスクトップのセッション管理に移っていた。
PIDは276513、276561、276609、276669、276756、277413で、約6時間48分経過しCPUを各約100%使用していた。
検証用と特定した6件を終了し、通常の2件がCPU使用率0%で残ることを確認した。
今回サンドボックス内で停止したテストランナー2件も終了した。

MCPの `AppClient.shutdown()` は終了RPCを送るだけで、終了完了を待たず、失敗時もプロセスを残していた。
`SHOGIBOARDQ_QUIT_APP_ON_EXIT=1` の場合は、自分が起動した `Popen` の終了を待ち、
タイムアウト時にterminate、続いてkillで回収するよう修正した。
終了時には接続し直さず、アプリの再起動や別アプリへの誤接続も避ける。
既存アプリへの接続や終了設定のないアプリは終了しない。
`test_app_client.py` は無応答、SIGTERM無視、終了設定なし、外部アプリへの接続を検査する。
過去の6プロセスの高CPUループ内部はptraceの制限によりスタックを取得できていない。
後始末の修正後にもビルドと `ctest --test-dir build -R '^tst_mcp_python$' --output-on-failure`
を実行し、MCPテスト39件がすべて成功した。
