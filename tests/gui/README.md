# 実MainWindowのGUI回帰テスト

Qt Testで実際のメニュー、ボタン、棋譜欄を操作する。アプリケーション本体のオブジェクトファイルを再利用し、`main.cpp`だけをテスト用の起動処理へ置き換える。MainWindowやコーディネーターのスタブは使用しない。

対象環境はLinux、Qt6、Python3、Xvfb、Release/Ninjaビルド。通常のCTestとは別に実行する。既存の`build/`が別の構成の場合、この手順の対象外となる。

```bash
# 初回のビルド設定
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

# アプリとGUIテストのビルド
python3 tests/gui/prepare.py

# 全シナリオ
xvfb-run -a -s '-screen 0 1600x1200x24' python3 tests/gui/run.py

# 対局ダイアログ・時間切れ・連続対局の回帰テスト
xvfb-run -a timeout 45s build/gui-audit/test-build/tst_start_game_flow

# 先読み・成り・即時着手・CSAエンジン制御の回帰テスト
xvfb-run -a env QT_QPA_PLATFORM=xcb timeout 60s build/gui-audit/test-build/tst_ponder_flow

# 修正対象のシナリオだけを実行
xvfb-run -a -s '-screen 0 1600x1200x24' python3 tests/gui/run.py \
  currentPositionCopy bodCurrentPosition commentsAndBookmark \
  consideration dockVisibilityAndLock usageLink branchNavigation
```

外観の切替は `boardThemes` で「榧と畳」「墨と榧」「琥珀」の選択、木目・影・駒倍率の変更、複数盤面への反映、再起動後の復元、反転を検証します。`board-appearance-main.png`、`board-appearance-flipped.png`、`board-appearance-dialog.png` と各テーマの盤面画像を保存します。
標準の駒の選択メニュー、反転表示、別の盤面への反映、設定復元は `pieceStyles` で検証します。
`tst_start_game_flow` は設定の保存・復元、先後入れ替え、初期設定への復帰、開始局面の選択、時間切れ負け設定と終局理由、最大手数での終局通知、連続対局の継続と各局の棋譜保存を検証します。
`tst_ponder_flow` はUSI_Ponderを報告しない模擬エンジンで、先後両方の成りと予測一致・不一致、人間とエンジンそれぞれの手番での「すぐ指させる」、エンジン同士の先読み対局を検証します。CSAは実際のCsaEngineControllerを使い、予測局面の保持とエンジン再初期化・終了を確認します（CSAサーバーには接続しません）。
標準の駒と反転時のスクリーンショットも保存します。
追加20種類は `pieceVariants:<style>`（例: `pieceVariants:torafu_light`）で種類ごとに実行します。各種類の選択、別盤面への反映、再起動後の復元、反転時の王・玉、成駒・持駒・駒打ち矢印、配色ダイアログの表示名と画像を検証します。`pieceStyleMenuBar` と `pieceStyleMenuDock` は旧メニュー・ボタンがなくなり、統合ウィンドウを開けることを確認します。
人間対エンジンの投了後の棋譜操作は `engineHumanResignNavigation` で検証します。人間が先手・後手の両方で、投了直後も淡い若草色の対局者カードと深緑の手番バッジ・カード全周の枠線を保つことを確認します。その後、指し手列・消費時間列から開始局面・途中の手・投了行を繰り返し選び、盤面と手番表示が選択と一致することを確認します。
局面編集開始時の棋譜クリアは `boardEditingClearsRecord` で検証します。読込棋譜の先頭・途中・最終局面と対局終了後から編集を開始し、盤面・持ち駒・手番を保持したまま棋譜欄が起動時と同じ1行に戻ることを確認します。メニューと盤上ボタンの両方から編集を終了し、棋譜操作・USI出力・再編集にも編集後の開始局面が使われることを検証します。
盤面の配色は `boardColors` で4色の選択、キャンセル、画像コピー、反転、別盤面への反映、標準色への復帰、配色とダイアログサイズの復元を検証します。
対局者情報の配色は `boardInformationColors` で追加14色の選択、透明度、複数盤面への即時反映、警告文字色、おすすめ配色による表示色の保持、設定・選択タブの復元、全配色の初期化を検証します。
おすすめ配色は `boardColorPresets` で標準の駒に合う配色5種類の選択・設定復元、個別調整との同期を検証します。
横幅の固定は `windowWidthFollowsContent` で盤の左右の余白、縦方向のリサイズ、棋譜フォント・盤サイズ・表示列の変更への追従、消費時間の全体表示を検証します。通常サイズと棋譜フォント拡大時のスクリーンショットも保存します。
`branchPanelCollapse` は分岐候補の開閉、幅の追従、開閉状態の復元、折りたたみ中の棋譜移動を確認し、長い対局者名を含む `branch-panel-collapsed.png` を保存します。
`engineInfoLayout` はエンジン情報帯の幅、狭い画面での横スクロール、フォント拡大、手動列幅の保持と全文ツールチップを確認します。
`engineConsiderationLayout` は検討操作部の折り返し、時間設定の切替、読み筋の表示幅と全文ツールチップ、盤面表示操作、文字拡大、列幅の保存・復元を確認し、`consideration-layout.png` と `consideration-narrow.png` を保存します。
`boardZoomPreservesWindowState` は全画面表示・最大化それぞれで「将棋盤の拡大」「将棋盤の縮小」を繰り返しクリックし、ウィンドウの表示状態と位置・大きさの維持、通常表示に戻した後の横幅の追従を検証します。

各シナリオは独立したプロセスで実行し、35秒でタイムアウトする。通常のアプリ設定は使用せず、テスト用一時ディレクトリに隔離する。結果は`build/gui-audit/`に保存される。

`usiLogPresentation` はUSI通信ログの通常表示・狭いフローティング表示・折り返し表示を確認し、`usi-log-main.png`・`usi-log-narrow.png`・`usi-log-wrapped.png` を保存します。閲覧位置と選択範囲の保持、コピー・消去、送信先別のEnter／ボタン送信、設定復元は CTest の `tst_usilogpanel` で検証します。

- `run-results.json`: シナリオ別結果。失敗・タイムアウト時は実行スクリプトも非0で終了する。
- `*.log` / `*.xml`: Qt TestのテキストログとJUnit XML。
- `*.usi.log`: 模擬USIエンジンへ送信したコマンド。
- `menus.json` / `all-clicked-actions.json`: 実行時メニューと操作済みアクション。
- `screenshots/`: 起動画面、各機能の画面、失敗時の画面。

`mock_usi.py`は決まった思考情報と指し手を返す検証用エンジン。実エンジンの評価精度・棋力は検証しない。通常のGUIシナリオではCSAは設定画面までを確認する。WebリンクはURLハンドラで宛先を検査するため、ブラウザ起動・外部通信は行わない。

現在局面のSFEN/BODコピー、分岐からのコピー、コメント・しおりの保存と再読込、検討開始・中止の繰り返し、全ドックの固定、「使い方」のURL表示を回帰テストに含む。加えて棋譜形式の相互変換、対局、解析、詰み探索、画像保存などを検証する。

詰将棋対局は次のコマンドで検証します。実際の盤クリック、成り、持ち駒の打ち込み、
Hayanagiの応手、正解・不詰の通知、問題切替を確認し、`tsume-play.png` と
`tsume-refutation.png` を `screenshots/` に保存します。
`promotionKeepsDraggedPiece` は３三飛から３四への移動について、成り選択中の描画が
移動先で維持されること、成る／成らないの確定、王手でない着手を拒否した後の表示復元を確認します。
詰将棋・問題一覧・一般機能の GUI テストは、本体と同じ日本語フォントの初期化を行います。
`tst_applicationfonts` は「判定時間」などに使用される実描画フォントと等幅表示を検証します。
`solveByBoardClicks` は盤の拡大・縮小ボタン、Ctrl＋ホイール、回転後の盤での着手も確認します。
`boardLayoutDuringMove` は通常・縦長・文字拡大の各表示で、着手から玉方の応手までの全描画フレームを記録し、探索中止ボタンの表示切替で盤面の位置・大きさが変わらないことを確認します。

```bash
xvfb-run -a env QT_QPA_PLATFORM=xcb build/gui-audit/test-build/tst_tsume_play_gui
```

問題一覧は `tst_tsume_collection_gui` で1003問のページ表示、10/20/50/100件への変更、
履歴の絞り込み、閲覧時に挑戦回数が増えないこと、正答後の一覧復帰・再挑戦時の履歴保持を確認する。
`menuModalRoundtrip` は実MainWindowのメニューから起動し、「一覧に戻る」・Esc・閉じる操作・
正答後の復帰で一覧が生存すること、ページ移動・別問題の選択・一覧終了後のメニュー操作が可能なことを確認する。
`tst_tsume_play_gui` の `solutionPlaybackAndResume` は正解手順の先頭・前・次・詰み局面への移動、
ボタンを押す前後の対局操作・再生操作の切替、日本語の棋譜、閲覧中の着手禁止、
正答履歴を増やさず途中対局へ戻れることを確認する。
`solutionLayoutDuringToggle` は10・12・16・24ptで正解手順と対局を繰り返し切り替え、
共通ボタン・切替ボタン・ウィンドウの寸法と位置、および全描画フレームの盤面矩形が変わらないことを確認する。
`cancelPendingSolution` は探索中止・再判定・手順取得中の復帰でも盤面が動かず、古い結果が適用されないことを確認する。
スクリーンショットは `tsume-collection.png`。

```bash
xvfb-run -a env QT_QPA_PLATFORM=xcb build/gui-audit/test-build/tst_tsume_collection_gui
```

外観統合ウィンドウは `appearanceWindow` で組み合わせ、絞り込み、成駒・持駒プレビュー、反転、開始時への復元、カスタム表示を検証します。`appearanceComponents:boards`・`appearanceComponents:backgrounds`・`appearanceComponents:stands`・`appearanceComponents:information` はそれぞれ20見本を実際にクリックし、他部品の保持、描画、別盤面への反映、再起動後の復元を確認します。`appearanceBoardBackground` は将棋盤と背景の双方向の独立性、カスタム背景、両方の選択状態の保存を確認します。

`evaluationGraphAppearance` は実MainWindow内で2系列・詰み・選択手数を表示し、`evaluation-graph-main.png` とグラフ単体の `evaluation-graph-export.png` を保存します。自動範囲・手動固定・目盛りの間引き・詰みの先後・ホバー・設定保存・待ったの詳細は CTest の `tst_evaluationchart` で検証します。

`commentPresentation` は棋王戦の棋譜で段落・開始局面のコメント列・全文ツールチップ・Ctrl＋クリックでのリンク操作・空行の保存と再読込を確認し、`comment-presentation.png` を保存します。`commentEditing` は空コメントの表示、書式付きクリップボードからのテキスト貼り付け、Ctrl＋Enterでの更新、記号・URL・段落を含む本文の保存と再読込、コメント削除を検証します。

`josekiLoadAndPlay` は定跡ファイルの読込・合法手の追加・予想応手の編集・保存内容・盤上への着手を検証し、`joseki-window.png`・`joseki-add.png`・`joseki-edit.png` を保存します。`josekiVisibleDuringDestruction` は定跡ドックを表示したままメイン画面を破棄してもクラッシュしないことを確認します。
