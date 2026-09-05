# GUI自動テスト・不具合修正結果（2026-09-05）

初回のGUIテストで報告した5種類の不具合を修正した。修正確認中に見つかったBODの誤判定、編集中の手番同期漏れ、検討再開時の二重起動、初期局面のUSIコマンドの誤りも修正対象に含めた。

## 修正内容

| 報告した不具合 | 原因と修正 | 回帰テスト |
|---|---|---|
| 閲覧中のSFEN/BODコピーが初期局面になる | 対局用SFEN履歴ではなく、表示中の盤面・手番・持ち駒と選択手数を使用 | 初期局面・1手目・4手目、分岐、本譜復帰、対局中、局面編集 |
| コメント更新が保持されない | 生成したGameRecordModelをCommentCoordinatorへ渡して保存経路を接続 | コメントとしおりを更新し、別の手へ移動、KIFコピーと再読込 |
| 検討中止でエンジンが再起動する | ボタン生成時の接続を管理対象に含める。開始要求の重複経路も削除 | 開始・中止を3回繰り返し、各回の要求数とUSIプロセス起動数を検証 |
| 棋譜解析ドックが固定されない | DockLayoutManagerへAnalysisResultsを登録 | 全12ドックの固定・解除 |
| 「使い方」が無反応 | メニューを利用ガイドURLの起動処理へ接続 | URLハンドラで利用ガイドへの遷移要求を検証 |

関連する追加修正:

- BOD末尾の「手数＝4 △８四歩 まで」をKI2と誤判定していた。指し手行の先頭で判定し、BODの再貼り付けを可能にした。BOD付きのKIF/KI2も単体テストで確認した。
- 局面編集の手番変更時に盤面データも同期するようにした。編集中のコピー結果にも後手番が反映される。
- 検討で送信されていた`position sfen startpos`を正しい`position startpos`にした。局面・指し手列・履歴・開始局面の各入力経路をテストした。

## 修正後の検証

| 検証 | 結果 |
|---|---|
| アプリ・GUIテストのビルド | 成功。コンパイラ警告なし |
| CTest | **76/76成功**。4.76秒 |
| Qt Test詳細集計 | **1,813成功、0失敗、0スキップ**。初期化・終了処理を含む |
| 実MainWindowのGUIテスト | **59/59成功**。36シナリオ群、初期化・終了処理を除く |
| GUIテスト実行時間 | Qt Test報告値の合計47.183秒 |
| メニュー操作 | 名前付き54アクション、ドック表示切替12項目 |
| GUIのクラッシュ・タイムアウト・スキップ | いずれも0件 |

[修正後の集計](../../build/gui-audit/summary.json)、[GUIシナリオ別結果](../../build/gui-audit/run-results.json)、[CTestログ](../../build/gui-audit/ctest-after-fixes.log)、[CTest詳細ログ](../../build/gui-audit/ctest-after-fixes-details.log)、[JUnit XML](../../build/gui-audit/ctest-after-fixes.xml)。実エンジンの棋力・評価精度と実CSAサーバーへの接続は検証対象外。

GUI回帰テストは[tests/gui/](../../tests/gui/README.md)に保存した。通常のCTestにはフォーマット判定テストを追加し、検討局面解決のケースも拡充した。GUI検証はLinuxのRelease/NinjaビルドとXvfbを使う別実行のテストである。

以下は初回テストの記録。初回は75 CTestが成功した一方、GUIは56件中50件成功・6件失敗であり、その失敗を起点に上記の修正を行った。

## 対象と実行方法

| 項目 | 内容 |
|---|---|
| 対象コミット | 初回: `32c4169c`。修正後: 同コミットに本作業の変更を適用 |
| アプリケーション | ShogiBoardQ 2026.09.05 |
| 機能一覧 | [公開ドキュメントのソース](../index.html)の16機能、`mainwindow.ui`、実行中のメニューバー |
| 環境 | Linux / Qt 6.11.2 / GCC 16.2.1 / Release / Ninja |
| 既存テスト | CTest、`QT_QPA_PLATFORM=offscreen` |
| GUIテスト | Qt Test + Xvfb、1600×1200、Fusionスタイル |
| 設定・保存先 | テスト専用のXDG設定領域と`build/gui-audit/` |
| USI連携 | 決まった合法手・評価情報・投了・詰みなし応答を返す模擬エンジン |

GUIテストは、ビルド済みアプリケーションの実際のオブジェクトファイルをリンクし、`main.cpp`だけをテスト用エントリーポイントに置き換えた。MainWindow、各ドック、コントローラ、棋譜処理、USI通信はアプリケーションの実装を使用しており、これらをスタブに置き換えていない。

Qt Testでメニューを表示して項目をクリックし、ダイアログ入力、盤面クリック、棋譜選択などを行った。判定には盤面SFEN、棋譜モデル、クリップボード内容、保存ファイルの再読み込み、画像の読み込み、ボタンの状態、送信USIコマンドを使用した。画面画像も保存した。

各シナリオ群は別プロセスで実行し、タイムアウトを設定した。最終実行にはクラッシュ・タイムアウト・スキップはない。ホームページへのリンクはURLハンドラで送信先を検証し、ブラウザの起動と外部アクセスは行っていない。

## 初回テストの集計（修正前）

| 検証 | 結果 |
|---|---|
| アプリケーションのビルド | 成功。実行時のビルドログにコンパイラ警告なし |
| 既存CTest | **75/75成功**、4.55秒 |
| 既存Qt Test詳細集計 | **1,799成功、0失敗、0スキップ**。初期化・終了処理を含む |
| 追加GUIテスト | **50成功、6失敗、0スキップ**。初期化・終了処理を除く56件 |
| GUIテストの構成 | 36シナリオ群。データ別テストを含め56件 |
| GUIテストの実行時間 | Qt Test報告値の合計53.742秒。準備・調査・再実行時間を除く |
| 名前付きメニューアクション | **54/54項目を操作** |
| 動的なドック表示切替 | **12項目を操作** |

操作した項目数は、その機能の全設定・全データで正常動作することを示すものではない。例えば、CSAは設定ダイアログの起動まで、詰将棋生成は模擬エンジンでの開始・停止までをGUIで検証している。

## 初回テストで確認した不具合（修正前）

### 1. 閲覧中の局面をSFEN/BODコピーできない

再現手順:

1. 「編集 → 棋譜貼り付け」で以下を取り込む。
   ```text
   position startpos moves 7g7f 3c3d 2g2f 8c8d
   ```
2. 棋譜の1手目または4手目を選ぶ。
3. 「編集 → 局面コピー → SFEN形式」または「BOD形式」を実行する。

期待結果: 選択中の局面をコピーする。

実際の結果: 盤面と棋譜選択は進んでいるが、**コピー結果は平手初期局面**になる。BODを新規画面へ貼り付けても初期局面が表示される。USIの「現在の指し手まで」は選択手数を反映できた。また、局面編集で作成した詰将棋初期配置のSFENコピーは成功しているため、全ての局面コピーが失敗するわけではない。

関連処理: [`KifuExportClipboard::currentPly()` / `currentPositionData()`](../../src/kifu/kifuexportclipboard.cpp)。通常時の局面選択に`activePly`を使用する箇所が調査対象となる。

証跡: [SFENテスト](../../build/gui-audit/before-fixes/currentPositionCopy.log)、[BODテスト](../../build/gui-audit/before-fixes/bodCurrentPosition.log)、[画面](../../build/gui-audit/before-fixes/screenshots/failure-currentPositionCopy.png)。

### 2. コメント更新が保持されない

再現手順:

1. 上記の4手の棋譜を取り込み、1手目を選ぶ。
2. 「棋譜コメント」ドックのテキストを`Audit comment at ply one`に変更し、「コメント更新」を押す。
3. 4手目へ移動してから1手目へ戻る。
4. KIF形式でコピーする。

期待結果: 1手目のコメントが再表示され、KIFにもコメントが含まれる。

実際の結果: **「コメントなし」に戻り、KIFにも入力したコメントが含まれない**。テストでは編集パネルが`commentUpdated(1, "Audit comment at ply one")`を発行したことも確認した。表示更新待ちを入れて再実行しても再現した。

関連処理: [`CommentCoordinator`](../../src/app/commentcoordinator.cpp)、[解析タブのコメント接続](../../src/ui/wiring/analysistabwiring.cpp)。更新シグナルから保存モデル・表示モデルまでの経路が調査対象となる。

証跡: [テストログ](../../build/gui-audit/before-fixes/commentsAndBookmark.log)、[画面](../../build/gui-audit/before-fixes/screenshots/failure-commentsAndBookmark.png)。

### 3. 「検討中止」でエンジンが再起動する

再現手順:

1. テスト用USIエンジンを登録して棋譜を取り込む。
2. 「検討」ドックで「検討開始」を押す。
3. 思考情報の表示後、「検討中止」を押す。

期待結果: エンジンが停止し、ボタンが「検討開始」に戻る。

実際の結果: **ボタンが「検討中止」のままとなり、エンジンプロセスの終了と再起動が発生する**。コマンドログには、異なるPIDで`usi → isready → go infinite → quit`が繰り返されている。

ソースでは、[ボタン作成時](../../src/widgets/considerationtabmanager_ui.cpp)の開始シグナルへの接続が`m_stopButtonConnection`に保存されていない。一方、[状態切替処理](../../src/widgets/considerationtabmanager.cpp)は保存済みの接続だけを切断して開始／中止ハンドラを付け替えており、開始ハンドラの残存が原因候補となる。

なお、同じログの初期局面では`position sfen startpos`も観測した。実エンジンでの受理可否は未検証。4手目を選択した別テストでは、表示中の局面が具体的なSFENとして正しく送信されることを確認した。

証跡: [テストログ](../../build/gui-audit/before-fixes/consideration.log)、[USIコマンド](../../build/gui-audit/before-fixes/consideration.usi.log)、[画面](../../build/gui-audit/before-fixes/screenshots/failure-consideration.png)。

### 4. 「ドックを固定」で棋譜解析ドックが固定されない

再現手順: 「表示 → ドックを固定」をチェックする。

期待結果: 各ドックが移動・フローティングできなくなる。

実際の結果: 他のドックは固定されるが、**棋譜解析ドックには`DockWidgetMovable`と`DockWidgetFloatable`が残る**。

ソースでも、[ドック管理への登録](../../src/app/mainwindowuiregistry.cpp)に`DockType::AnalysisResults`の登録がないことを確認した。

証跡: [各ドックの状態を記録したログ](../../build/gui-audit/before-fixes/dockVisibilityAndLock.log)。

### 5. 「ヘルプ → 使い方」が無反応

再現手順: 「ヘルプ → 使い方」をクリックする。

期待結果: 利用ガイドなどの説明画面・URLが開く。

実際の結果: **画面もURLも開かない**。同じURLハンドラを使った「ホームページ」テストでは、期待するプロジェクトURLが渡されている。

`actionUsage`は[UI定義](../../src/app/mainwindow.ui)に存在するが、ソース内に動作へ接続するコードが見つからない。[ファイル・ヘルプ関連の接続](../../src/ui/wiring/fileactionswiring.cpp)にも含まれていない。

証跡: [使い方テスト](../../build/gui-audit/before-fixes/usageLink.log)、[ホームページテスト](../../build/gui-audit/before-fixes/websiteLink.log)。

## ドキュメントの16機能との対応（初回テスト）

ここでの「成功」は、記載した確認範囲での結果を示す。

| 機能 | 今回の確認範囲と結果 |
|---|---|
| 対局機能 | 人間同士の開始・名前表示・2手着手・待った・中断・投了が成功。人間対模擬エンジン、模擬エンジン同士の対局と投了、即時着手も成功 |
| CSA通信対局 | 設定ダイアログの起動・終了が成功。既存のCSAプロトコルテストも成功。実サーバーとのログイン・対局は未実施 |
| 棋譜表示 | 先頭・次手・最終手の盤面同期、分岐選択と「本譜へ戻る」が成功。しおりダイアログの起動成功。コメント更新に不具合 |
| 棋譜管理 | KIFファイルを開く・名前付き保存・上書き保存・再読込が成功。KIF/KI2/CSA/USI/JKF/USENのコピー→再取込で指し手と最終局面を保持。SFEN/BODの現在局面コピーに不具合 |
| 棋譜解析 | 模擬エンジンで解析結果が追加され完了すること、解析中止が成功。実エンジンの評価精度や長時間解析は未検証 |
| 検討モード | 開始・思考情報表示・4手目のSFEN送信を確認。中止に不具合。MultiPV変更など全設定のGUI確認は未実施 |
| 詰み探索 | ダイアログ起動、模擬エンジンの詰みなし応答、探索中止が成功。実際の詰み手順探索は未検証 |
| 詰将棋局面生成 | 模擬エンジンで開始・停止とボタン状態遷移が成功。既存の生成ロジックテストも成功。実エンジンによる問題発見・トリミングの全工程は未検証 |
| 局面編集 | 開始・終了、平手配置、詰将棋配置、全駒を駒台へ、手番変更が成功。編集局面のSFENコピーも成功 |
| 局面集ビューア | SFEN局面集ファイルを開き、次の局面へ移動し、メイン画面へ取り込む操作が成功 |
| 定跡機能 | テスト用やねうら王形式ファイルを読み込み、人間対局中に定跡手をダブルクリックして着手する操作が成功。編集・削除・保存は既存のリポジトリ／ウィンドウテストで確認 |
| 入玉宣言 | 持将棋点数ダイアログと、初期局面で条件未達の宣言を取り消して対局を継続できることを確認。点数・条件判定の既存テストも成功。条件成立時のGUI終局処理は未検証 |
| 画像エクスポート | 将棋盤と評価値グラフの画像コピー、PNGファイル保存・再読込が成功 |
| ドック機能 | 12ドックの表示切替、レイアウトリセットが成功。レイアウト保存ダイアログ起動と既存の保存・復元テストが成功。棋譜解析ドックの固定に不具合 |
| メニュー機能 | 実行時メニュー一覧を取得し、名前付き54アクションを操作。メニュードックのタブ構成とボタンからの盤面回転が成功。お気に入りの既存テストも成功。「使い方」に不具合 |
| 多言語対応 | 日本語・英語・システム設定の保存、メニューの排他選択が成功。英語翻訳をロードしたMainWindowの表示も成功 |

加えて、ツールバーの表示切替、盤面回転・拡大縮小、バージョン情報、Qtについて、終了アクションによるアプリケーションイベントループの終了を確認した。

## 証跡と再実行

GUI検証コードは`tests/gui/`、実行結果は`build/gui-audit/`に保存した。**実行結果はGit管理外であり、buildディレクトリの削除で失われる。** 初回のログは`build/gui-audit/before-fixes/`に退避してある。

- [集計JSON](../../build/gui-audit/summary.json)
- [シナリオ別結果JSON](../../build/gui-audit/run-results.json)
- [実行時メニュー一覧](../../build/gui-audit/menus.json)
- [操作したアクション一覧](../../build/gui-audit/all-clicked-actions.json)
- [GUI検証コード](../../tests/gui/tst_gui_functional.cpp)
- [模擬USIエンジン](../../tests/gui/mock_usi.py)
- [既存CTestログ](../../build/gui-audit/ctest.log) / [詳細ログ](../../build/gui-audit/ctest-details.log) / [JUnit XML](../../build/gui-audit/ctest.xml)
- [起動画面](../../build/gui-audit/screenshots/startup.png) / [人間対局](../../build/gui-audit/screenshots/human-game.png) / [英語表示](../../build/gui-audit/screenshots/english.png)

今回と同じRelease/NinjaビルドでのGUIテスト再実行:

```bash
python3 tests/gui/prepare.py
xvfb-run -a -s '-screen 0 1600x1200x24' python3 tests/gui/run.py
```

修正対象のシナリオだけの再実行例:

```bash
xvfb-run -a -s '-screen 0 1600x1200x24' python3 tests/gui/run.py \
  currentPositionCopy bodCurrentPosition commentsAndBookmark \
  consideration dockVisibilityAndLock usageLink
```

再実行すると対応するログと`run-results.json`は更新される。`summary.json`は今回の最終全件実行に対する集計スナップショットである。失敗を検出した場合、実行スクリプトの終了コードは非0となる。

初回は既存テストの成功だけではGUI不具合を検出できなかったため、修正後も実画面の操作テストで回帰を確認する。
