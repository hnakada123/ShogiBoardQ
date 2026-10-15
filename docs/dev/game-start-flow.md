# 対局ダイアログから対局開始までのフロー

対局の開始処理は、開始位置を値として確定してから検証し、その後に棋譜・盤・時計・表示へ適用する。部品の生成と配線は [MainWindowComposer](composition-root.md) が起動時に行う。

## 開始要求

`GameSessionOrchestrator::initializeGame()` はダイアログの結果と `KifuNavigationState::currentRecordLine()` を取得する。この時点では対局用の履歴を書き換えない。

`GameStartCoordinator::Ctx` は次を渡す。

| 値 | 用途 |
|---|---|
| `dialogData` | 対局者、手合割、時間設定など |
| `recordLine` | 選択した分岐の開始局面・指し手・現在手数 |
| `startNodeId` | 分岐ツリーで選択したノードのID |
| `clock` | 対局の時計。省略時は司令塔の時計 |
| `startSfenStr`, `currentSfenStr` | 検証後に更新する表示用の局面文字列 |
| `bottomSide` | 人間を手前に表示するときの盤の向き |

`recordLine` の手順は、同じ局面へ到達する別の手順と区別する。過去の対局をSFENで検索したり、SFEN末尾の手数から棋譜を切り詰めたりしない。棋譜を持たない単独局面は、局面1件・指し手0件の手順になる。

## 検証と適用の順序

1. `GameStartCoordinator::initializeGame()` が現在局面またはプリセットから開始する手順を決定する。現在局面では `recordLine.currentPly` までを取り、後続の手を除く。
2. 開始局面の玉と対局モードを検証する。不正なら、クリーンアップ・時計設定・エンジン起動を行わずエラーを返す。
3. `requestPreStartCleanup(GameStartPreparation)` を通知する。`GameStartPreparation::Mode` は `NewGame`、`CurrentPosition`、`Reset` のいずれかであり、表示用SFENから推測しない。
4. `SessionLifecycleCoordinator` が `PreStartCleanupHandler::performCleanup()` を呼ぶ。新規ならツリーを作り直し、現在局面なら指定ノードまでの棋譜表示を作る。終局ノードが選択されていれば親を起点とし、末尾の中断ノードは取り除く。
5. クリーンアップが返した起点から、`SessionLifecycleCoordinator` がライブセッションを開始する。`Reset` では開始しない。クリーンアップハンドラ自体は対局を開始しない。
6. 表示用SFENと盤の向きを反映し、連続対局設定を通知する。
7. `GameStartCoordinator::start()` が時計設定を一度適用し、`requestApplyTimeControl` で表示層へ通知する。`TimeControlController::presentTimeControl()` は設定と開始時刻の記録・表示を担当し、時計を再設定しない。
8. 対局者名を通知し、`MatchCoordinator::configureAndStart()` に確定した `MatchStartOptions::recordLine` を渡す。
9. 司令塔が対局用の局面列・指し手列を更新する。`GameStartOrchestrator` が `MatchSessionState::begin()`、盤・名前・手番の反映、対局モード別Strategyの開始を行う。
10. `finishStart()` が開始通知、棋譜の開始行の選択、時計の始動、エンジン初手の要求を行う。初期化待ちのエンジンは準備完了後に思考を始める。

## 対局中と再開

`MatchSessionState` は対局設定、モード、終了状態、エンジンの先読み局面を所有する。終了状態の変更は `finish()`、`markTerminalRecorded()`、`clearGameOver()` を通し、フィールドへのポインタを他のハンドラへ渡さない。

`MatchStrategyContext` は盤・状態・時計・エンジン管理・終局処理・履歴を依存として受け取る独立したクラスである。司令塔のprivateメンバーにはアクセスしない。エンジンへの `position` は、その時点の確定済み局面列・指し手列から `RecordLine::positionCommand()` で生成する。人間対エンジン、エンジン同士、待ったの後で同じ生成経路を使う。

中断再開は `InterruptedGameState` の局面列・指し手列・時計スナップショットを復元する。以前の `position` 文字列の履歴は保持しない。先読みは破棄し、復元した確定手順から新しい探索を始める。時計は保存時の値を復元し、通常開始の設定適用は通さない。

連続対局の次局は独立した棋譜として準備し、開始手順のスナップショットを空にして同じ開始局面から始める。時間設定は通常の `start()` が一度適用する。

## 回帰テスト

- `tst_game_start_orchestrator`: 選択した分岐とSFEN手数の異なる開始局面を、実際のUSI通信経路で検証する。
- `tst_game_start_flow`: 不正な開始要求で状態が変わらないこと、クリーンアップと時計設定が一度だけ実行されることを検証する。
- `tst_preset_gamestart_cleanup`, `tst_positionedit_gamestart`: 新規・現在局面・局面編集・中断からの開始を検証する。
- `tst_wiring_contracts`: 本体ウィンドウの配線で、開始・終局時の棋譜、ライブセッション、時計、UI状態を検証する。
- `tst_gamestrategy`, `tst_undo_flow`, `tst_background_tasks`: 着手確定、待った、中断時のプロセス終了を検証する。

これらは本体の `shogiboardq_static` をリンクする。本体クラスのメソッドを空実装に差し替えるテスト用シンボル定義は使用しない。外部エンジンとの通信にはテスト用のUSIプロセスを用いる。
