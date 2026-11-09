# MCP連携の補完と検証（2026-09-28）

`docs/index.html` と MCP 関連の git log を照合した調査（基準コミット `11caf28e`）で見つかった
10項目の未対応・不完全な経路を補完した。公開ツールは38から45に増加した。

## 対応した操作

| 調査項目 | 実装した経路 |
|---|---|
| 定跡の新規・読込・追加・編集・削除・保存・マージ | 無名のボタン・入力欄を `selector` で操作。`josekiTable`、`josekiRecentMenu`、`josekiMergeMenu` を公開 |
| コメント・しおり・表示列・分岐ツリー | `kifuCommentEdit` / `kifuCommentApply`、`kifuBookmarkEdit`、`kifuToggleTime` / `kifuToggleBookmark` / `kifuToggleComment`。`branchTreeView` のノード一覧と `click_branch_node` |
| メニューのカスタマイズ | `menu_favorites` で登録・解除・順序変更・保存。`menuTabs`、`menuCustomize`、ボタン・文字サイズ調整ボタンも公開 |
| 駒音の5つの調整 | `set_widget_value` のスライダー対応。`pieceSoundVolume` / `pieceSoundPitch` / `pieceSoundLow` / `pieceSoundMid` / `pieceSoundHigh` |
| 盤面配色 | コンボ選択時に `activated`、テキスト編集時に `textEdited` を通す。`QColorDialog` 自体への色指定にも対応 |
| 表示言語 | `actionLanguageSystem` / `actionLanguageJapanese` / `actionLanguageEnglish` を許可。反映はアプリ再起動後 |
| 名前付きドックレイアウト | 保存名入力を `selector` で指定。`menuSavedLayouts` から復元・削除・起動時指定 |
| エンジン一覧の選択 | `set_widget_value` で `engineListWidget` の0始まり行番号を選び、設定・削除を実行 |
| 後手の個別時間設定 | チェック可能な `QGroupBox` を `set_widget_value` の真偽値で切替 |
| 棋譜解析 | `analysisResultsTable` の行・盤面列操作。`analyze_kifu` / `kifu_analysis_status` / `kifu_analysis_result` を追加 |

局面集ビューアの履歴メニュー `sfenCollectionRecentMenu` も公開した。
定跡・局面集・名前付きレイアウトの動的メニューは `list_menu_actions` で階層を読み取り、
`select_menu_action` の `path` に0始まりのインデックス列を指定する。

## 操作と結果の扱い

- 無名・同名の部品には `get_widget_text` / `list_dialogs` が返す `selector` を使う。
  対象が破棄された場合やアプリ再起動後は取得し直す。
- 部品一覧が64 KiBを超えても読めるよう、ローカル通信の受信上限を8 MiBに設定する。
  超過時は接続を閉じ、件数を減らす案内を返す。
- 操作の応答は予約完了を示す。実行後の値・ファイル・画面で完了を確認する。
- 非表示・無効・別のモーダルダイアログに遮られた対象は操作しない。
  子ダイアログ表示中に親を閉じてクラッシュする経路も拒否する。
- ファイル名などの全文入力では補完ポップアップを閉じ、続くボタン操作を可能にする。
- メニュードックとツールバーのボタンにもアクション許可リストを適用する。
- 全体解析は本譜の0手目から最終局面まで。範囲は両端を含み、各局面に着手履歴を渡す。
  分岐の有無は返すが、分岐の解析は行わない。
- 結果の `lines` は手番側、`score_cp_black` / `score_mate_black` は先手側の評価。
  取消・エンジン異常時にも取得済み結果を保持する。GUIの結果表には自動反映しない。
- 連続解析は局面間も同時実行枠を1つ使用する。停止中・起動中の子プロセスも回収する。

## 検証

`mcp/tests/test_feature_coverage.py` は一時設定・専用ソケットで実アプリを起動し、stdio MCP経由で操作する。
配色の実際の値、設定の再起動後の保持、保存棋譜のコメント・しおり、分岐移動後の局面、
定跡ファイルの内容、局面集の再読込、解析結果の評価方向・ページ取得・中断を確認する。
解析にはビルド済みHayanagiを使用する。

`mcp/tests/test_jobs.py` は子プロセス起動中の取消、同時実行枠の予約、プロセス回収、
連続解析の途中エラーと既存結果の保持を検証する。
`tests/tst_menu_window.cpp` は設定を一時領域に隔離し、お気に入りの通知・再生成後の保持も検証する。
既存の検討テストでは、局面切替時のモデル更新で予約クリックが無効になった場合に備えて、
読み筋ダイアログの表示を待ち、必要なら表を取得し直して再操作する。表示された局面・再生結果の検証は維持する。

実行コマンド:

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
ctest --test-dir build --rerun-failed --output-on-failure
```

結果:

- ビルド成功。コンパイラ警告なし。
- CTestは失敗分の修正・再実行を含め100件すべて成功。
- MCPテスト群は99件成功、16件スキップ。
- 最終の受信容量・ボタン許可リスト修正後、追加機能と接続処理の回帰テスト11件が成功。

検証環境はLinux / Qt offscreen。外部CSAサーバーやShogiHomeを必要とするテストは、
接続先を指定していない場合にスキップする。Windows / macOSの実機確認は行っていない。
