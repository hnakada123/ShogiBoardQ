# MainWindow 責務外処理 移譲メソッド詳細

作成日: 2026-02-26  
対象: `src/app/mainwindow.h`, `src/app/mainwindow.cpp`

## 1. 目的

`MainWindow` を「UIイベント受信と委譲の入口」に限定し、責務外コードを既存クラスまたは新規クラスへ移譲する。
本書は、移譲候補ごとに実装方法を具体化した実行手順書である。

## 2. 現状ベースライン

- `MainWindow` 実装行数: `2838` (`mainwindow.cpp`)
- `MainWindow::` メソッド数: `181`
- `ensure*` メソッド数: `47`

既に分離済み:
- `MainWindowCompositionRoot`
- `MainWindowDepsFactory`
- `MainWindowWiringAssembler`
- `MainWindowUiBootstrapper`
- `MainWindowRuntimeRefsFactory`
- `MainWindowGameStartService`
- `MainWindowResetService`
- `GameRecordLoadService`
- `GameRecordUpdateService`
- `UndoFlowService`
- `TurnStateSyncService`
- `SessionLifecycleCoordinator`
- `KifuNavigationCoordinator`

## 3. 最終責務の定義

### 3.1 MainWindow に残す責務

- Qtのトップレベルウィンドウ管理
- QAction/Signal の受信窓口
- 遅延初期化のトリガ
- 「どこへ委譲するか」のルーティング

### 3.2 MainWindow から追い出す責務

- 配線詳細と Deps 組み立て
- コメント未保存確認や行復元などの編集ワークフロー
- Dock生成後の細かいUI調整
- プレイヤー名解決時の時間情報組み立て
- 連続対局開始パラメータ組み立て
- 終了処理シーケンスの重複実装

## 4. 移譲対象ごとの詳細手順

## 4.1 Analysis タブ配線群

対象:
- `MainWindow::setupEngineAnalysisTab`
- `MainWindow::connectAnalysisTabSignals`
- `MainWindow::configureAnalysisTabDependencies`

現状問題:
- `MainWindow` が UI生成、signal接続、依存設定を同時に持っている。
- `m_analysisTab` と `PlayerInfoWiring` と `CommentCoordinator` の接続順序知識が `MainWindow` に集中している。

移譲先:
- 既存 `AnalysisTabWiring` を拡張する。
- 必要なら薄い新規 `AnalysisTabCoordinator` を `src/app` に追加し、`MainWindow` は coordinator の `ensureInitialized()` だけ呼ぶ。

実装方法:
1. `AnalysisTabWiring::Deps` に以下を追加する。  
   `QObject* mainWindowReceiver`, `CommentCoordinator*`, `UsiCommandController*`, `ConsiderationWiring*`, `PlayerInfoWiring*`, `ShogiEngineThinkingModel* considerationModel`
2. `AnalysisTabWiring::buildUiAndWire()` で、以下を一括接続する。  
   `branchNodeActivated`, `commentUpdated`, `pvRowClicked`, `usiCommandRequested`, `startConsiderationRequested`, `engineSettingsRequested`, `considerationEngineChanged`
3. `configureAnalysisTabDependencies` 相当の処理を wiring 側メソッドに移し、`setAnalysisTab`, `addGameInfoTabAtStartup`, `setConsiderationThinkingModel` も移管する。
4. `MainWindow::setupEngineAnalysisTab()` は「生成と結果受け取り」のみにする。

受け入れ条件:
- 上記3メソッドから `connect()` 呼び出しが消える。
- `MainWindow` 側の分岐と null-check が大幅に減る。

回帰確認:
- 起動時に `対局情報` タブが正しく挿入される。
- コメント編集、PVクリック、検討開始、USIコマンド送信が従来通り動作する。

## 4.2 コメント未保存確認と行復元フロー

対象:
- `MainWindow::onRecordRowChangedByPresenter`

現状問題:
- 未保存コメント確認、キャンセル時の行復元、コメント表示更新が混在。
- `QSignalBlocker` と `QTableView` 操作が `MainWindow` に残っている。

移譲先:
- 既存 `CommentCoordinator` へ集約する。

実装方法:
1. `CommentCoordinator` に新規メソッドを追加する。  
   `bool handleRecordRowChangeRequest(int row, EngineAnalysisTab* analysisTab, RecordPane* recordPane)`
2. 内部で以下を実施する。  
   - `hasUnsavedComment()` 判定  
   - `confirmDiscardUnsavedComment()` 判定  
   - キャンセル時の `QSignalBlocker` + 行復元
3. `MainWindow::onRecordRowChangedByPresenter` は以下だけにする。  
   - `ensureCommentCoordinator()`  
   - `if (!m_commentCoordinator->handleRecordRowChangeRequest(...)) return;`  
   - `broadcastComment(...)`
4. 可能なら `comment.trimmed().isEmpty() ? tr("コメントなし") : comment` も `CommentCoordinator` に寄せる。

受け入れ条件:
- `MainWindow::onRecordRowChangedByPresenter` が 10 行以内になる。
- `QTableView` と `QSignalBlocker` 依存が `MainWindow` から消える。

回帰確認:
- 未保存コメントありで別行選択時に確認ダイアログが出る。
- 「キャンセル」で行選択が元に戻る。
- コメント表示が棋譜ペインとコメントパネルで同期する。

## 4.3 Dock 生成後のレイアウト調整

対象:
- `MainWindow::setupBoardInCenter`
- `MainWindow::onBoardSizeChanged`
- `MainWindow::performDeferredEvalChartResize`
- `MainWindow::createJosekiWindowDock` の visibility 接続

現状問題:
- ボードサイズ同期 (`m_central->setFixedSize`) が分散。
- dock 作成後の副作用接続が `MainWindow` に残っている。

移譲先:
- 既存 `DockCreationService` と `MainWindowUiBootstrapper` を拡張する。
- 必要なら新規 `BoardViewportLayoutService` を追加する。

実装方法:
1. `DockCreationService` に後処理フックを追加する。  
   例: `setJosekiVisibilityReceiver(QObject* receiver, const char* slot)` は使わず、型安全のため `setJosekiVisibilityHandler(std::function<void(bool)>)` よりも `QObject* + member function pointer` 方式の専用 setter を定義する。
2. `setupBoardInCenter` と `onBoardSizeChanged` の重複ロジックを `BoardViewportLayoutService::syncCentralSizeToBoard()` に移す。
3. `performDeferredEvalChartResize` は上記サービス1呼び出しだけにする。
4. `MainWindowUiBootstrapper` で初期レイアウト調整を呼ぶ。

受け入れ条件:
- ボードサイズ同期の本体実装が `MainWindow` から消える。
- Joseki ドック可視化時更新の接続責務が `DockCreationService` 側に移る。

回帰確認:
- 盤サイズ変更時に中央領域サイズが追従する。
- Joseki ドック表示時に内容が更新される。

## 4.4 対局者名解決と対局情報反映

対象:
- `MainWindow::onPlayerNamesResolved`

現状問題:
- 時間制御情報 (`TimeControlInfo`) の組み立てを `MainWindow` が保持している。
- `m_timeController` 有無で処理分岐しており、UI配線責務を越えている。

移譲先:
- 既存 `PlayerInfoWiring::resolveNamesAndSetupGameInfo` を統一入口として使う。

実装方法:
1. `PlayerInfoWiring` に `resolveNamesWithTimeController(...)` を追加する。  
   `TimeControlController*` を受け、内部で `TimeControlInfo` 構築まで完結させる。
2. `MainWindow::onPlayerNamesResolved` は以下に縮小する。  
   - `ensurePlayerInfoWiring()`  
   - `m_playerInfoWiring->resolveNamesWithTimeController(human1, ..., playMode, m_state.startSfenStr, m_timeController);`
3. `clearGameEndTime()` 呼び出しも `PlayerInfoWiring` 側へ移す。

受け入れ条件:
- `onPlayerNamesResolved` から `TimeControlInfo` 構築コードが消える。
- `MainWindow` は data pass-through のみになる。

回帰確認:
- 新規対局開始時に開始日時/持ち時間/先手後手名が従来通り表示される。
- `m_timeController == nullptr` のケースでクラッシュしない。

## 4.5 連続対局開始リクエストの組み立て

対象:
- `MainWindow::ensureConsecutiveGamesController` 内の `requestStartNextGame` 接続ラムダ

現状問題:
- `MainWindow` が `GameStartCoordinator::StartParams` を組み立てている。
- イベント受信者なのに次局開始の詳細手順まで持っている。

移譲先:
- 既存 `ConsecutiveGamesController` に「次局起動委譲」責務を寄せる。

実装方法:
1. `ConsecutiveGamesController` 側の signal を変更する。  
   現在: `requestStartNextGame(opt, tc)`  
   変更案: `requestStartNextGame(const GameStartCoordinator::StartParams& params)`
2. `ConsecutiveGamesController::startNextGame()` で `StartParams` を組み立て、`autoStartEngineMove = true` もそこで設定する。
3. `MainWindow` 側接続は lambda をやめ、専用スロット `onConsecutiveStartRequested(const StartParams&)` を追加して `m_gameStart->start(params)` のみ行う。

受け入れ条件:
- `ensureConsecutiveGamesController` から `StartParams` 組み立てコードが消える。
- `MainWindow` 側で lambda 接続が1つ減る。

回帰確認:
- 連続対局で次局が自動開始される。
- 先後入れ替え設定が維持される。

## 4.6 終了処理の重複解消

対象:
- `MainWindow::saveSettingsAndClose`
- `MainWindow::closeEvent`

現状問題:
- 設定保存 + エンジン終了が二重管理。
- 今後の終了フロー変更で不整合を起こしやすい。

移譲先:
- 新規 `AppShutdownService` か、既存 `SessionLifecycleCoordinator` へ「アプリ終了前処理」を追加する。

実装方法:
1. 共通メソッド `performShutdownSequence()` を作成する。  
   中身は `saveWindowAndBoardSettings()`, `m_match->destroyEngines()`。
2. `saveSettingsAndClose` は  
   `performShutdownSequence(); QCoreApplication::quit();`
3. `closeEvent` は  
   `performShutdownSequence(); QMainWindow::closeEvent(e);`
4. 将来的には `SessionLifecycleCoordinator` に `shutdownApp()` を生やして `MainWindow` は委譲のみでもよい。

受け入れ条件:
- 終了時処理の実体が1か所になる。

回帰確認:
- メニュー終了、ウィンドウクローズ双方で設定保存とエンジン終了が必ず実行される。

## 4.7 巨大 Deps 組み立てメソッドの分離

対象:
- `MainWindow::ensureKifuNavigationCoordinator`
- `MainWindow::ensureSessionLifecycleCoordinator`

現状問題:
- Deps 構築だけで長く、変更差分が `MainWindow` に集中する。
- 依存項目の追加時に `MainWindow` の責務境界を壊しやすい。

移譲先:
- 新規 `KifuNavigationDepsFactory`
- 新規 `SessionLifecycleDepsFactory`

実装方法:
1. `src/app/kifunavigationdepsfactory.h/.cpp` を追加し、`KifuNavigationCoordinator::Deps build(MainWindow&)` を実装。
2. `src/app/sessionlifecycledepsfactory.h/.cpp` を追加し、`SessionLifecycleCoordinator::Deps build(MainWindow&)` を実装。
3. `MainWindow` は
   - `auto deps = KifuNavigationDepsFactory::build(*this);`
   - `m_kifuNavCoordinator->updateDeps(deps);`
   だけにする。
4. `friend class` は必要最小限にし、可能なら `MainWindowRuntimeRefs` 経由に揃える。

受け入れ条件:
- 対象2メソッドが 10〜15 行程度に縮小する。
- `MainWindow` 内の `std::bind` 密度がさらに減る。

回帰確認:
- 棋譜ナビ同期、終局後処理、時間設定適用が従来通り動作する。

## 5. 追加で推奨する共通実装ルール

- `ensure*` は「生成と `updateDeps` 呼び出し」だけにする。
- Deps 構築は factory に集約する。
- `connect()` は専用 wiring/coordinator に寄せ、`MainWindow` 内では最小化する。
- UI文言やフォーマット組み立ては wiring/service 側へ寄せる。
- `MainWindow` メソッド1つあたり目安 3〜10 行を上限とする。

## 6. 実施順序

1. `4.6` 終了処理重複解消  
2. `4.4` 対局者名解決の移譲  
3. `4.5` 連続対局開始組み立て移譲  
4. `4.2` コメント未保存確認移譲  
5. `4.1` Analysis タブ配線移譲  
6. `4.3` Dock/ボードレイアウト調整移譲  
7. `4.7` Deps factory 化

理由:
- 小差分で安全に進められる順序を優先する。
- 早い段階で重複除去とイベント入口の整理を終える。

## 7. 進捗管理チェックリスト

- [ ] `onPlayerNamesResolved` から `TimeControlInfo` 組み立てが除去された
- [ ] `onRecordRowChangedByPresenter` が CommentCoordinator 委譲のみになった
- [ ] `ensureConsecutiveGamesController` の start 次局ラムダが除去された
- [ ] 終了処理が1実装に統合された
- [ ] `setupEngineAnalysisTab` 系から signal 接続詳細が除去された
- [ ] `ensureKifuNavigationCoordinator` / `ensureSessionLifecycleCoordinator` が factory 呼び出し主体になった
- [ ] `MainWindow::` メソッド数と平均行数が削減された

## 8. 手動回帰テスト観点

- 新規対局開始、投了、中断、連続対局、次局自動開始
- 棋譜行選択、分岐遷移、コメント編集キャンセル
- Joseki ドック表示切替時の更新
- 終了操作（メニュー終了/ウィンドウクローズ）時の設定保存とエンジン停止

