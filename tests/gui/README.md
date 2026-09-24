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

駒の種類の切り替え、反転表示、別の盤面への反映、設定復元は `pieceStyles` で検証します。
`tst_start_game_flow` は設定の保存・復元、先後入れ替え、初期設定への復帰、開始局面の選択、時間切れ負け設定と終局理由、最大手数での終局通知、連続対局の継続と各局の棋譜保存を検証します。
`tst_ponder_flow` はUSI_Ponderを報告しない模擬エンジンで、先後両方の成りと予測一致・不一致、人間とエンジンそれぞれの手番での「すぐ指させる」、エンジン同士の先読み対局を検証します。CSAは実際のCsaEngineControllerを使い、予測局面の保持とエンジン再初期化・終了を確認します（CSAサーバーには接続しません）。
標準・太字・反転時のスクリーンショットも保存します。
木目・白・黒の追加バリエーションは `pieceVariants` で切り替え・反転・設定復元を検証します。
人間対エンジンの投了後の棋譜操作は `engineHumanResignNavigation` で検証します。人間が先手・後手の両方で、投了直後も淡い若草色の対局者カードと深緑の手番バッジ・カード全周の枠線を保つことを確認します。その後、指し手列・消費時間列から開始局面・途中の手・投了行を繰り返し選び、盤面と手番表示が選択と一致することを確認します。
局面編集開始時の棋譜クリアは `boardEditingClearsRecord` で検証します。読込棋譜の先頭・途中・最終局面と対局終了後から編集を開始し、盤面・持ち駒・手番を保持したまま棋譜欄が起動時と同じ1行に戻ることを確認します。メニューと盤上ボタンの両方から編集を終了し、棋譜操作・USI出力・再編集にも編集後の開始局面が使われることを検証します。
盤面の配色は `boardColors` で4色の選択、キャンセル、画像コピー、反転、別盤面への反映、標準色への復帰、配色とダイアログサイズの復元を検証します。
対局者情報の配色は `boardInformationColors` で追加14色の選択、透明度、複数盤面への即時反映、警告文字色、おすすめ配色による表示色の保持、設定・選択タブの復元、全配色の初期化を検証します。
おすすめ配色は `boardColorPresets` で駒5種類×配色5種類の選択・設定復元、駒変更時の候補更新、個別調整との同期を検証します。
横幅の固定は `windowWidthFollowsContent` で盤の左右の余白、縦方向のリサイズ、棋譜フォント・盤サイズ・表示列の変更への追従、消費時間の全体表示を検証します。通常サイズと棋譜フォント拡大時のスクリーンショットも保存します。

各シナリオは独立したプロセスで実行し、35秒でタイムアウトする。通常のアプリ設定は使用せず、テスト用一時ディレクトリに隔離する。結果は`build/gui-audit/`に保存される。

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

```bash
xvfb-run -a env QT_QPA_PLATFORM=xcb build/gui-audit/test-build/tst_tsume_play_gui
```

問題一覧は `tst_tsume_collection_gui` で1003問のページ表示、10/20/50/100件への変更、
履歴の絞り込み、閲覧時に挑戦回数が増えないこと、正答後の一覧復帰・再挑戦時の履歴保持を確認する。
`menuModalRoundtrip` は実MainWindowのメニューから起動し、「一覧に戻る」・Esc・閉じる操作・
正答後の復帰で一覧が生存すること、ページ移動・別問題の選択・一覧終了後のメニュー操作が可能なことを確認する。
`tst_tsume_play_gui` の `solutionPlaybackAndResume` は正解手順の先頭・前・次・詰み局面への移動、
ボタンを押す前後の棋譜・再生操作の表示切替、日本語の棋譜、閲覧中の着手禁止、
正答履歴を増やさず途中対局へ戻れることを確認する。
`cancelPendingSolution` は手順取得中の復帰で古い結果が盤面へ適用されないことを確認する。
スクリーンショットは `tsume-collection.png`。

```bash
xvfb-run -a env QT_QPA_PLATFORM=xcb build/gui-audit/test-build/tst_tsume_collection_gui
```
