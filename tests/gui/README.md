# 実MainWindowのGUI回帰テスト

ダイアログ共通の外観・文字サイズ監査は CTest の `tst_dialog_appearance` でも実行します。
独自ダイアログ全22種類と補助入力画面を日本語・英語で開き、A−/A＋の上下限、
操作ボタンの欠け、フォーカス、設定の復元、駒音の数値入力、エンジン選択を検証します。
色選択・CSA通信ログ・評価値グラフ設定も対象です。OS／Qt標準のファイル選択・
通知ダイアログは標準の操作を維持し、実メニューからの起動は `dialogs` で確認します。

```bash
ctest --test-dir build --output-on-failure -R '^tst_dialog_appearance$'
xvfb-run -a -s '-screen 0 1600x1200x24' env QT_QPA_PLATFORM=xcb \
  SHOGIBOARDQ_DIALOG_SCREENSHOTS=/tmp/shogiboardq-dialog-audit \
  build/tests/tst_dialog_appearance
```

スクリーンショットを指定した場合、通常・最大文字サイズの各画面をそのディレクトリへ保存します。
下部操作列の横並び・二段への折り返し、拡大後の決定ボタンの可視性、エンジン一覧が
空になった際の案内更新、定跡の登録済み状態と一括登録の有効・無効も検証します。
見た目の確認では、主操作を青、補助操作を淡い色とし、Tabフォーカスの枠、
未読込時の案内、通常サイズと文字拡大時の余白・文字切れを確認します。

Qt Testで実際のメニュー、ボタン、棋譜欄を操作する。アプリケーション本体のオブジェクトファイルを再利用し、`main.cpp`だけをテスト用の起動処理へ置き換える。MainWindowやコーディネーターのスタブは使用しない。

対象環境はLinux、Qt6、Python3、Xvfb、Release/Ninjaビルド。通常のCTestとは別に実行する。既存の`build/`が別の構成の場合、この手順の対象外となる。

`legalMoveHighlights` は対局中の駒選択、表示メニューでのON/OFF、盤の反転、着手・キャンセル時の消去、再起動時の設定復元を検証し、`legal-moves-selected.png`・`legal-moves-flipped.png` を保存します。合法手の制約（王手、ピン、成り必須、二歩、行き所のない駒打ち、打ち歩詰め）と選択状態の破棄は CTest の `tst_legal_move_highlights` で検証します。

```bash
# 初回のビルド設定
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

# アプリとGUIテストのビルド
python3 tests/gui/prepare.py

# 全シナリオ
xvfb-run -a -s '-screen 0 1600x1200x24' python3 tests/gui/run.py

# 対局ダイアログ・時間切れ・連続対局の回帰テスト
xvfb-run -a timeout 90s build/gui-audit/test-build/tst_start_game_flow

# 先読み・成り・即時着手・CSAエンジン制御の回帰テスト
xvfb-run -a env QT_QPA_PLATFORM=xcb timeout 60s build/gui-audit/test-build/tst_ponder_flow

# 修正対象のシナリオだけを実行
xvfb-run -a -s '-screen 0 1600x1200x24' python3 tests/gui/run.py \
  currentPositionCopy bodCurrentPosition commentsAndBookmark \
  consideration dockVisibilityAndLock usageLink branchNavigation
```

外観の切替は `boardThemes` で「榧と畳」「墨と榧」「琥珀」の選択、木目・影・駒倍率の変更、複数盤面への反映、再起動後の復元、反転を検証します。`board-appearance-main.png`、`board-appearance-flipped.png`、`board-appearance-dialog.png` と各テーマの盤面画像を保存します。
`boardFlipKeepsLayout` は盤サイズ30・53・100で「盤面を反転」を各12回クリックし、将棋盤・駒台・対局者情報・ウィンドウの位置と大きさ、および81マスのクリック判定を検証します。`board-flip-normal.png`・`board-flip-flipped.png` を保存します。
標準の駒の選択メニュー、反転表示、別の盤面への反映、設定復元は `pieceStyles` で検証します。
`tst_start_game_flow` は設定の保存・復元、先後入れ替え、初期設定への復帰、開始局面の選択、時間切れ負け設定と終局理由、最大手数での終局通知、連続対局の継続と各局の棋譜保存を検証します。王・玉の不足・重複がある編集局面では、両手番と人同士・人対エンジン・エンジン同士の各設定で対局開始を拒否し、盤面・棋譜・時計を維持すること、および局面修正や平手・駒落ちの選択後に開始できることも確認します。
`dialogPresentation` は日本語・英語の対局設定画面と文字拡大時の配置を確認し、`start-game-dialog.png`・`start-game-dialog-en.png` と文字拡大版を保存します。狭い画面でも「対局開始」ボタンが見えることを検証します。
`preparedGameInfoSurvivesStart` は現在局面・平手からの対局開始で、事前入力した棋戦・場所・入力途中の備考を保持し、開始日時・対局者・持ち時間を更新することを検証します。
`tst_ponder_flow` はUSI_Ponderを報告しない模擬エンジンで、先後両方の成りと予測一致・不一致、人間とエンジンそれぞれの手番での「すぐ指させる」、エンジン同士の先読み対局を検証します。CSAは実際のCsaEngineControllerを使い、予測局面の保持とエンジン再初期化・終了を確認します（CSAサーバーには接続しません）。
標準の駒と反転時のスクリーンショットも保存します。
追加21種類は `pieceVariants:<style>`（例: `pieceVariants:torafu_light`、戦国文字は `pieceVariants:sengoku`）で種類ごとに実行します。各種類の選択、別盤面への反映、再起動後の復元、反転時の王・玉、成駒・持駒・駒打ち矢印、配色ダイアログの表示名と画像を検証します。`pieceStyleMenuBar` と `pieceStyleMenuDock` は旧メニュー・ボタンがなくなり、統合ウィンドウを開けることを確認します。
人間対エンジンの投了後の棋譜操作は `engineHumanResignNavigation` で検証します。人間が先手・後手の両方で、投了直後も淡い若草色の対局者カードと深緑の手番バッジ・カード全周の枠線を保つことを確認します。その後、指し手列・消費時間列から開始局面・途中の手・投了行を繰り返し選び、盤面と手番表示が選択と一致することを確認します。
局面編集開始時の棋譜クリアは `boardEditingClearsRecord` で検証します。読込棋譜の先頭・途中・最終局面と対局終了後から編集を開始し、盤面・持ち駒・手番を保持したまま棋譜欄が起動時と同じ1行に戻ることを確認します。メニューと盤上ボタンの両方から編集を終了し、棋譜操作・USI出力・再編集にも編集後の開始局面が使われることを検証します。
局面編集の直接操作は `boardEditingPieceTransfers` で通常・反転表示それぞれの両側7種類について、駒箱・駒台・盤・相手駒台の間の移動と枚数を検証します。「全ての駒を駒箱へ」は王・玉を含む全40枚を先後共通の生駒として収納し、盤上と両駒台を空にします。`boardEditingKingsStayOnBoard` は王・玉を通常の駒台へ移す操作の拒否を計8ケースで検証します。
`boardEditingPieceBox` は編集開始での駒箱表示、空の箱からの取り出し拒否、王2枚の取り出しと先後変更、片玉局面、金の先後変更、成駒・持ち駒を含む局面の編集終了・再開・KIF再読込と収納状態の復元を通常・反転表示で確認します。`boardEditingPieceBoxLayout` は盤サイズ30・50・100で時計や駒台と重ならず、8種類すべてのクリック位置が一致することを検証します。`board-edit-box-full.png`・`board-edit-box-full-flipped.png`・`board-edit-box-tsume.png`・`board-edit-box-tsume-flipped.png` を保存します。
`boardEditingPieceBoxSide` は通常・反転表示で先後の切替、同じ筋への両側の歩の配置と二歩の拒否、持ち上げ中の切替、手番の保持、反転・編集再開時の選択保持を検証します。`board-edit-box-white.png`・`board-edit-box-white-flipped.png` と移動中の `board-edit-box-white-drag.png`・`board-edit-box-white-drag-flipped.png` を保存します。`boardEditingPieceBoxLayout` では切替ボタンと駒のクリック領域が重ならないことも確認します。
`boardEditingPromotionAndCapture`・`boardEditingForcedPromotion`・`boardEditingRejectedMoves` は成り・不成・先後の巡回、成駒の駒箱への収納、成駒の取り込み、行き所のない駒の自動成り、二歩・味方駒・玉取り・空の駒台・不正座標の拒否を確認します。`boardEditingStandMargins` は駒台の外側がクリック対象にならないことを検証します。
`boardEditingTurn` は配置操作による手番の保持と初期配置への切替時の手番同期、`boardEditingSelectionReset` は配置変更・編集終了時の選択解除、`boardEditingFromBranch` は表示中の分岐からの編集開始を検証します。`boardEditingGameAndExport` は編集後の後手番と持ち駒を保った対局開始・着手・投了・開始局面への復帰・KIF再読込を確認します。

```bash
xvfb-run -a -s '-screen 0 1600x1200x24' python3 tests/gui/run.py \
  boardEditing boardEditingClearsRecord boardEditingTurn boardEditingSelectionReset \
  boardEditingPieceTransfers boardEditingPieceBox boardEditingPieceBoxSide boardEditingPieceBoxLayout boardEditingKingsStayOnBoard \
  boardEditingPromotionAndCapture boardEditingStandMargins \
  boardEditingForcedPromotion boardEditingRejectedMoves boardEditingFromBranch boardEditingGameAndExport
```

盤面の配色は `boardColors` で4色の選択、キャンセル、画像コピー、反転、別盤面への反映、標準色への復帰、配色とダイアログサイズの復元を検証します。
対局者情報の配色は `boardInformationColors` で追加14色の選択、透明度、複数盤面への即時反映、警告文字色、おすすめ配色による表示色の保持、設定・選択タブの復元、全配色の初期化を検証します。
おすすめ配色は `boardColorPresets` で標準の駒に合う配色5種類の選択・設定復元、個別調整との同期を検証します。
横幅の固定は `windowWidthFollowsContent` で盤の左右の余白、縦方向のリサイズ、棋譜フォント・盤サイズ・表示列の変更への追従、消費時間の全体表示を検証します。通常サイズと棋譜フォント拡大時のスクリーンショットも保存します。
`branchPanelCollapse` は分岐候補の開閉、幅の追従、開閉状態の復元、折りたたみ中の棋譜移動を確認し、長い対局者名を含む `branch-panel-collapsed.png` を保存します。
`engineInfoLayout` はエンジン情報帯の幅、狭い画面での横スクロール、フォント拡大、手動列幅の保持と全文ツールチップを確認します。
`engineConsiderationLayout` は検討操作部の折り返し、時間設定の切替、読み筋の表示幅と全文ツールチップ、盤面表示操作、文字拡大、列幅の保存・復元を確認し、`consideration-layout.png` と `consideration-narrow.png` を保存します。
`boardZoomPreservesWindowState` は全画面表示・最大化それぞれで「将棋盤の拡大」「将棋盤の縮小」を繰り返しクリックし、ウィンドウの表示状態と位置・大きさの維持、通常表示に戻した後の横幅の追従を検証します。

`engineAnalysisSettings` は局面数・所要時間の更新、範囲指定、開始局面のみの設定復元、文字拡大とエンジン未登録時の表示を確認します。
`engineAnalysisLayout` は進捗・完了表示、列幅の保存、横スクロール、読み筋の全文ツールチップ、Enterによる盤面表示要求を検証します。
`engineAnalysisRange` は途中の手から解析し、結果の選択が正しい棋譜の手数へ移動することを確認します。
`enginePostGameAnalysis` は実GUIで先手／後手として対局・投了した後、全局面と最終局面の再解析を実行します。終局行の除外、結果とグラフの評価値一致、盤面・棋譜・グラフ・ツリー・移動ボタンの同期を確認し、Hayanagiを使うケースも実行します。
`engineAnalysisBranch` は分岐の解析対象・指し手と局面の一致、本譜へ移動後の結果選択、キャンセル後のグラフから元の分岐への復帰を検証し、`analysis-branch-synchronized.png` を保存します。
`engineAnalysisCancelPreservesGraph` は条件ダイアログをキャンセルしても既存の2系列と選択手数が保持されることを確認します。
`engineAnalysisRecord` はビルド済みのHayanagiと棋王戦の棋譜で開始局面から14手目までを解析し、局面移動と読み筋盤面の表示を確認します。`ANALYSIS_AUDIT_KIF` で入力棋譜を指定できます。`analysis-settings.png`、`analysis-settings-large.png`、`analysis-layout.png`、`analysis-narrow.png`、`analysis-record.png` を保存します。

各シナリオは独立したプロセスで実行し、35秒でタイムアウトする。通常のアプリ設定は使用せず、テスト用一時ディレクトリに隔離する。結果は`build/gui-audit/`に保存される。

`menuPresentation` はメニューパネルの全文ラベル、USI形式の検索、狭いドックでの折り返し、お気に入りの登録を確認し、`menu-panel.png`・`menu-search.png`・`menu-narrow.png`・`menu-favorites.png` を保存します。チェック状態・表示状態の同期、ボタン上からのドラッグ開始、並べ替え・削除、設定の即時保存は CTest の `tst_menu_window` で検証します。

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
`applicationFont` は表示メニューから共通書体を変更し、メニュー・棋譜・時計・通信ログ・
分岐ツリー・評価値グラフ・新しく開くダイアログへの反映と、個別の文字サイズ・選択局面の保持を検証します。
`application-font-dialog.png` と `application-font-main.png` を保存します。
CTest の `tst_fontsettingsdialog` はプレビュー・キャンセル・標準への復元・設定の保存と読込を検証します。
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
`paginationAndPersistence` は日本語・英語・中国語（簡体字・繁体字）で実行し、
表示確認用のスクリーンショットを `tsume-collection-<言語>.png` に保存する。

```bash
xvfb-run -a env QT_QPA_PLATFORM=xcb build/gui-audit/test-build/tst_tsume_collection_gui
```

外観統合ウィンドウは `appearanceWindow` で組み合わせ、絞り込み、成駒・持駒プレビュー、反転、開始時への復元、カスタム表示を検証します。`appearanceComponents:boards`・`appearanceComponents:backgrounds`・`appearanceComponents:stands`・`appearanceComponents:information` はそれぞれ20見本を実際にクリックし、他部品の保持、描画、別盤面への反映、再起動後の復元を確認します。`appearanceBoardBackground` は将棋盤と背景の双方向の独立性、カスタム背景、両方の選択状態の保存を確認します。

`evaluationGraphAppearance` は実MainWindow内で2系列・詰み・選択手数を表示し、`evaluation-graph-main.png` とグラフ単体の `evaluation-graph-export.png` を保存します。自動範囲・手動固定・目盛りの間引き・詰みの先後・ホバー・設定保存・待ったの詳細は CTest の `tst_evaluationchart` で検証します。

`commentPresentation` は棋王戦の棋譜で段落・開始局面のコメント列・全文ツールチップ・Ctrl＋クリックでのリンク操作・空行の保存と再読込を確認し、`comment-presentation.png` を保存します。`commentEditing` は空コメントの表示、書式付きクリップボードからのテキスト貼り付け、Ctrl＋Enterでの更新、記号・URL・段落を含む本文の保存と再読込、コメント削除を検証します。

`gameInfoPresentation` は対局情報の起動時の9項目と未開始・未設定の案内、新規作成時の初期化、案内文を含めないKIF出力、棋譜読込後の表示、入力途中からの更新、備考のKIF出力と再読込、項目列幅の保持、狭いドックでの操作ボタンの折り返し、文字拡大を検証します。`game-info-startup.png`・`game-info-loaded.png`・`game-info-narrow.png`・`game-info-large-font.png` を保存します。編集状態に応じたボタンの有効化、Ctrl＋Enter、行削除のUndo/Redo、全文ツールチップ、設定復元は CTest の `tst_game_info_pane` で検証します。

`josekiLoadAndPlay` は定跡ファイルの読込・合法手の追加・予想応手の編集・保存内容・盤上への着手を検証し、`joseki-window.png`・`joseki-add.png`・`joseki-edit.png` を保存します。`josekiVisibleDuringDestruction` は定跡ドックを表示したままメイン画面を破棄してもクラッシュしないことを確認します。

詰将棋局面生成は `tst_tsumeshogi_generator_gui` で設定欄と一覧の配置、未登録時の案内、
複数選択とCtrl+C、手順を含む保存、盤面表示、閲覧位置の保持、進捗・停止・再開始・エラー表示、
Escでの停止、設定復元、文字拡大時の表示を検証します。開始・停止には模擬USIエンジンを使用します。
`tsumeshogi-generator-empty.png`・`tsumeshogi-generator-results.png`・
`tsumeshogi-generator-help.png`・`tsumeshogi-generator-large-font.png` を保存します。

```bash
xvfb-run -a env QT_QPA_PLATFORM=xcb build/gui-audit/test-build/tst_tsumeshogi_generator_gui
```

詰み探索の条件設定は `tst_tsume_search_dialog` でエンジン未登録時の開始禁止、
時間指定・無制限の切替、秒数の直接入力とEnterによる開始、検討設定との独立性、
エンジンの登録順変更後の選択復元、キャンセル時の条件保持、文字・画面サイズの保存を検証します。
日本語・英語の通常表示と24pt表示、幅を狭めたときの折り返しも確認し、
`tsume-search-ja.png`・`tsume-search-en.png` と文字拡大版・未登録時の画像を保存します。
探索開始・結果表示・中止の実MainWindow経由の確認には `engineMateNoMate`・`engineStopMate` を使用します。

```bash
xvfb-run -a -s '-screen 0 1600x1200x24' env QT_QPA_PLATFORM=xcb build/gui-audit/test-build/tst_tsume_search_dialog
```
