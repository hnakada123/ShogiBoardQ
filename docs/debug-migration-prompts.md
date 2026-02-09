# デバッグ文移行プロンプト集

`docs/debug-logging-guidelines.md` に従って、各モジュールの `qDebug()` を `QLoggingCategory` ベースに移行するためのプロンプト集。
各プロンプトをそのままコピーして Claude Code に貼り付けることで、モジュール単位で移行を実行できる。

---

## 共通ルール（全プロンプト共通）

以下のルールはすべてのプロンプトに適用される。プロンプト内で繰り返さない。

1. **規約ファイルを必ず参照**: `docs/debug-logging-guidelines.md` を読んでから作業すること
2. **レベル判断**:
   - 開発者トレース（変数値、処理フロー確認） → `qCDebug()`
   - 運用ログ（接続、起動/終了、設定変更） → `qCInfo()`
   - リカバリ可能なエラー（ファイル未検出、想定外値） → `qCWarning()`
   - 処理続行困難なエラー → `qCCritical()`
   - **迷ったら `qCDebug()` を選択する**（`qCInfo()` はリリースビルドで出力されるため）
   - **`qCDebug` にすべき具体パターン**:
     - 関数の入口/出口トレース（`"enter"`, `"exit"`, `"called"`, `"START"`, `"leave"`）
     - ポインタアドレスの出力（`static_cast<const void*>(ptr)`）
     - 毎手/毎tick 呼ばれるSFEN・盤面出力（`"setSfen:"`, `"last sfen ="`）
     - 処理フロー分岐の確認（`"forwarding to"`, `"skip"`, `"delegate"`）
     - 変数値・内部状態のダンプ（`"modeNow="`, `"recSize="`, `"m_running="`）
     - UI操作イベントの詳細（`"onMoveRequested"`, `"onClicked"`）
     - タイマー遅延の診断（`"elapsed=66ms"`）
   - **`qCInfo` にすべきパターン**（低頻度の運用情報のみ）:
     - アプリ起動時の設定（`"Language setting:"`, `"Application dir:"`）
     - エンジン/サーバーの起動・終了（`"エンジン起動完了:"`）
     - ファイル読み込み成功（ファイル名つき）
     - 入玉宣言・棋譜自動保存など重要ゲームイベント
3. **プレフィックス除去**: `[CSA]`, `[USI]`, `[GM]` 等の手動プレフィックスは除去する（カテゴリが同等の役割を果たす）
4. **不要なデバッグ文の削除**: 意味のないメッセージ（`"ここに来た"`, `"OK"`）、getter/setter の単純呼び出しログ、高頻度メソッドでの出力は削除する
5. **Q_LOGGING_CATEGORY の定義場所**: モジュール内の主要ソースファイル（最初に列挙したファイル）に `Q_LOGGING_CATEGORY` を定義し、他ファイルのヘッダーでは `Q_DECLARE_LOGGING_CATEGORY` で参照する
6. **ヘッダーの `#include <QLoggingCategory>`**: カテゴリを使用するファイルのヘッダーに追加する
7. **ビルド確認**: 移行後に `cmake --build build` でビルドが通ることを確認する
8. **コードスタイル**: `connect()` でラムダを使わない。`std::as_const()` でコンテナのdetach回避

---

## プロンプト一覧

### 1. src/core/ — shogi.core

```
src/core/ のデバッグ文を QLoggingCategory に移行してください。

## 規約
docs/debug-logging-guidelines.md を読んで従ってください。

## 対象ファイル（qDebug 箇所数）
- shogiboard.cpp（50箇所）
- movevalidator.cpp（32箇所）
- shogiutils.cpp（2箇所）

※ shogiclock.cpp は移行済み（lcShogiClock, "shogi.clock"）。変更不要。

## カテゴリ定義
- カテゴリ名: "shogi.core"
- 変数名: lcCore
- 定義場所: shogiboard.cpp に Q_LOGGING_CATEGORY(lcCore, "shogi.core")
- shogiboard.h に Q_DECLARE_LOGGING_CATEGORY(lcCore)
- movevalidator.cpp, shogiutils.cpp からは shogiboard.h を include して参照

## 既存プレフィックス例
- `[ShogiUtils]` — shogiutils.cpp
- プレフィックスなし — movevalidator.cpp, shogiboard.cpp

## レベル判断の目安
- 盤面パース中の座標・駒情報ダンプ → qCDebug（開発時のみ必要）
- SFEN文字列パース結果 → qCDebug
- 不正な座標値の検出（sentinel value -1 等） → qCWarning
- movevalidator の変数値出力 → 高頻度のものは削除を検討、残すなら qCDebug

## ビルド確認
移行後に cmake --build build でビルドが通ることを確認してください。
```

---

### 2. src/views/ — shogi.view

```
src/views/shogiview.cpp のデバッグ文を QLoggingCategory に移行してください。

## 規約
docs/debug-logging-guidelines.md を読んで従ってください。

## 対象ファイル（qDebug 箇所数）
- shogiview.cpp（19箇所）

## 現状
既に Q_LOGGING_CATEGORY(ClockLog, "clock") が定義されていますが、
命名規則に合っていません。以下のように変更してください:
- ClockLog → lcView に変数名を変更
- "clock" → "shogi.view" にカテゴリ名を変更
- shogiview.h の Q_DECLARE_LOGGING_CATEGORY(ClockLog) も lcView に変更
- 既存の qCDebug(ClockLog) 呼び出しも qCDebug(lcView) に更新

残っている plain qDebug() も全て qCDebug(lcView) 等に移行してください。

## 既存プレフィックス例
- `[DEBUG]`, `[SHOGIVIEW-DEBUG]` — 盤面操作のトレース
- `[ShogiView]` — プレイヤー名設定

## レベル判断の目安
- setBoard の呼び出しトレース → qCDebug（高頻度なら削除検討）
- mouseReleaseEvent のデバッグ座標出力 → 削除（高頻度・一時デバッグ用）
- プレイヤー名設定 → qCDebug

## ビルド確認
移行後に cmake --build build でビルドが通ることを確認してください。
```

---

### 3. src/network/ — shogi.network

```
src/network/ のデバッグ文を QLoggingCategory に移行してください。

## 規約
docs/debug-logging-guidelines.md を読んで従ってください。

## 対象ファイル（qDebug 箇所数）
- csaclient.cpp（16箇所）
- csagamecoordinator.cpp（31箇所）

## カテゴリ定義
- カテゴリ名: "shogi.network"
- 変数名: lcNetwork
- 定義場所: csaclient.cpp に Q_LOGGING_CATEGORY(lcNetwork, "shogi.network")
- csaclient.h に Q_DECLARE_LOGGING_CATEGORY(lcNetwork)
- csagamecoordinator.cpp からは csaclient.h を include して参照

## 既存プレフィックス例
- `[CSA]` — 送受信ログ（Sent/Recv）
- `[CSA-DEBUG]` — 詳細デバッグ（状態、指し手処理）

## レベル判断の目安
- CSAプロトコル送受信内容 → qCDebug（通信トレース）
- 接続・切断イベント → qCInfo（運用ログ）
- 指し手拒否（not in game, not my turn） → qCDebug
- 接続エラー → qCWarning
- 消費時間ログ → qCDebug

## ビルド確認
移行後に cmake --build build でビルドが通ることを確認してください。
```

---

### 4. src/engine/ — shogi.engine

```
src/engine/ のデバッグ文を QLoggingCategory に移行してください。

## 規約
docs/debug-logging-guidelines.md を読んで従ってください。

## 対象ファイル（qDebug 箇所数）
- usi.cpp（21箇所）
- thinkinginfopresenter.cpp（14箇所）
- shogiengineinfoparser.cpp（8箇所）
- engineprocessmanager.cpp（5箇所）
- usiprotocolhandler.cpp（5箇所）
- usicommlogmodel.cpp（1箇所）

## カテゴリ定義
- カテゴリ名: "shogi.engine"
- 変数名: lcEngine
- 定義場所: usi.cpp に Q_LOGGING_CATEGORY(lcEngine, "shogi.engine")
- usi.h に Q_DECLARE_LOGGING_CATEGORY(lcEngine)
- 他ファイルからは usi.h を include して参照

## 既存プレフィックス例
- `[Usi]`, `[Usi::メソッド名]` — usi.cpp の構造化ログ
- `[USI]` — プロトコルレベルのログ
- `logPrefix()` — engineprocessmanager.cpp の動的プレフィックス（エンジン名入り）

## 注意: logPrefix() の扱い
engineprocessmanager.cpp の logPrefix() は動的にエンジン名を含むプレフィックスを生成しています。
カテゴリ移行時には、logPrefix() の情報（エンジン名）はメッセージ本文に含めてください。
例: `qCDebug(lcEngine).nospace() << logPrefix() << " > " << command;`
→ `qCDebug(lcEngine) << engineName() << "送信:" << command;`
（logPrefix() がエンジン識別に必要な場合はメッセージに残す）

## レベル判断の目安
- USIコマンド送受信 → qCDebug（通信トレース）
- エンジン起動・終了 → qCInfo
- bestmove のドロップ（op-id-mismatch等） → qCDebug
- 不正なUSI応答 → qCWarning
- エンジンプロセス異常終了 → qCWarning
- タイマー関連のトレース → qCDebug

## ビルド確認
移行後に cmake --build build でビルドが通ることを確認してください。
```

---

### 5. src/kifu/ — shogi.kifu

```
src/kifu/ のデバッグ文を QLoggingCategory に移行してください。

## 規約
docs/debug-logging-guidelines.md を読んで従ってください。

## 対象ファイル（qDebug 箇所数）
- kifuloadcoordinator.cpp（70箇所）
- gamerecordmodel.cpp（29箇所）
- kifuclipboardservice.cpp（20箇所）
- kifuexportcontroller.cpp（11箇所）
- kifubranchtreebuilder.cpp（10箇所）
- livegamesession.cpp（10箇所）
- formats/kiftosfenconverter.cpp（7箇所）
- kifunavigationstate.cpp（4箇所）
- formats/ki2tosfenconverter.cpp（4箇所）
- formats/usentosfenconverter.cpp（2箇所）
- kifreader.cpp（1箇所）

## カテゴリ定義
- カテゴリ名: "shogi.kifu"
- 変数名: lcKifu
- 定義場所: kifuloadcoordinator.cpp に Q_LOGGING_CATEGORY(lcKifu, "shogi.kifu")
- kifuloadcoordinator.h に Q_DECLARE_LOGGING_CATEGORY(lcKifu)
- 他ファイルからは kifuloadcoordinator.h を include して参照
  （循環依存が生じる場合は、別途共通ヘッダーに宣言を移動してよい）

## 既存プレフィックス例
- `[GM]` — kifuloadcoordinator.cpp（旧GameManager由来）
- `[GameRecordModel]` — gamerecordmodel.cpp
- `[KifuClipboard]` — kifuclipboardservice.cpp
- `[KNS]` — kifunavigationstate.cpp

## レベル判断の目安
- 棋譜パース結果のダンプ → qCDebug
- 棋譜読み込み成功 → qCInfo（ファイル名含む）
- 棋譜ファイル読み込み失敗 → qCWarning
- パース警告（parseWarn） → qCWarning
- 分岐ツリー操作のトレース → qCDebug
- クリップボード操作ログ → qCDebug
- ナビゲーション状態変更 → qCDebug

## ビルド確認
移行後に cmake --build build でビルドが通ることを確認してください。
```

---

### 6. src/analysis/ — shogi.analysis

```
src/analysis/ のデバッグ文を QLoggingCategory に移行してください。

## 規約
docs/debug-logging-guidelines.md を読んで従ってください。

## 対象ファイル（qDebug 箇所数）
- analysisflowcontroller.cpp（67箇所）
- considerationmodeuicontroller.cpp（14箇所）
- analysiscoordinator.cpp（6箇所）
- tsumesearchflowcontroller.cpp（3箇所）
- analysisresultspresenter.cpp（2箇所）

## カテゴリ定義
- カテゴリ名: "shogi.analysis"
- 変数名: lcAnalysis
- 定義場所: analysisflowcontroller.cpp に Q_LOGGING_CATEGORY(lcAnalysis, "shogi.analysis")
- analysisflowcontroller.h に Q_DECLARE_LOGGING_CATEGORY(lcAnalysis)
- 他ファイルからは analysisflowcontroller.h を include して参照

## 既存プレフィックス例
- `[ANA]`, `[ANA::メソッド名]` — analysiscoordinator.cpp, analysisflowcontroller.cpp
- `[AnalysisFlowController::メソッド名]` — 完全クラス名プレフィックス
- `[TsumeSearch]` — tsumesearchflowcontroller.cpp

## レベル判断の目安
- 解析コマンド送信（sendAnalyzeForPly, sendGoCommand） → qCDebug
- 解析モード開始/終了 → qCInfo
- エンジン情報行の処理トレース → qCDebug（高頻度のものは削除検討）
- 解析結果の状態更新 → qCDebug
- 詰将棋探索の開始/完了 → qCInfo

## ビルド確認
移行後に cmake --build build でビルドが通ることを確認してください。
```

---

### 7. src/game/ — shogi.game

```
src/game/ のデバッグ文を QLoggingCategory に移行してください。

## 規約
docs/debug-logging-guidelines.md を読んで従ってください。

## 対象ファイル（qDebug 箇所数）
- matchcoordinator.cpp（85箇所）
- gamestartcoordinator.cpp（35箇所）
- prestartcleanuphandler.cpp（21箇所）
- shogigamecontroller.cpp（18箇所）
- consecutivegamescontroller.cpp（5箇所）
- gamestatecontroller.cpp（2箇所）

## カテゴリ定義
- カテゴリ名: "shogi.game"
- 変数名: lcGame
- 定義場所: matchcoordinator.cpp に Q_LOGGING_CATEGORY(lcGame, "shogi.game")
- matchcoordinator.h に Q_DECLARE_LOGGING_CATEGORY(lcGame)
- 他ファイルからは matchcoordinator.h を include して参照

## 既存プレフィックス例
- `[MC]` — matchcoordinator.cpp（MatchCoordinator）
- `[PreStartCleanupHandler]` — prestartcleanuphandler.cpp
- `[CGC]` — consecutivegamescontroller.cpp（ConsecutiveGamesController）
- `[GameState]` — gamestatecontroller.cpp

## レベル判断の目安
- 対局開始/終了 → qCInfo
- 投了・入玉宣言 → qCInfo
- 棋譜自動保存 → qCInfo
- 手番処理の詳細トレース → qCDebug
- 連続対局の進行状況 → qCDebug
- クリーンアップ処理のフロー → qCDebug
- ゲームオーバー重複検出（already game over） → qCDebug

## ビルド確認
移行後に cmake --build build でビルドが通ることを確認してください。
```

---

### 8. src/board/ + src/navigation/ — shogi.board, shogi.navigation

```
src/board/ と src/navigation/ のデバッグ文を QLoggingCategory に移行してください。

## 規約
docs/debug-logging-guidelines.md を読んで従ってください。

## 対象ファイル（qDebug 箇所数）

### src/board/ — shogi.board
- boardinteractioncontroller.cpp（1箇所）

### src/navigation/ — shogi.navigation
- kifunavigationcontroller.cpp（39箇所）
- recordnavigationhandler.cpp（6箇所）

## カテゴリ定義

### shogi.board
- カテゴリ名: "shogi.board"
- 変数名: lcBoard
- 定義場所: boardinteractioncontroller.cpp に Q_LOGGING_CATEGORY と Q_DECLARE_LOGGING_CATEGORY の両方
  （ファイルが1つしかないため、.cpp 内に定義し .h に宣言）

### shogi.navigation
- カテゴリ名: "shogi.navigation"
- 変数名: lcNavigation
- 定義場所: kifunavigationcontroller.cpp に Q_LOGGING_CATEGORY(lcNavigation, "shogi.navigation")
- kifunavigationcontroller.h に Q_DECLARE_LOGGING_CATEGORY(lcNavigation)
- recordnavigationhandler.cpp からは kifunavigationcontroller.h を include して参照

## 既存プレフィックス例
- `[BoardInteraction]` — boardinteractioncontroller.cpp
- `[KNC]` — kifunavigationcontroller.cpp（KifuNavigationController）
- `[RNH]` — recordnavigationhandler.cpp（RecordNavigationHandler）

## レベル判断の目安
- クリック無視の理由（not human's turn） → qCDebug
- ナビゲーション操作（goForward, goBack 等） → qCDebug
- 行変更ハンドリング → qCDebug

## ビルド確認
移行後に cmake --build build でビルドが通ることを確認してください。
```

---

### 9. src/ui/ + src/widgets/ + src/dialogs/ + src/models/ + src/services/ — shogi.ui

```
UI層全体のデバッグ文を QLoggingCategory に移行してください。

## 規約
docs/debug-logging-guidelines.md を読んで従ってください。

## 対象ファイル（qDebug 箇所数）

### src/ui/（21ファイル、260箇所）
- coordinators/kifudisplaycoordinator.cpp（50箇所）
- coordinators/dialogcoordinator.cpp（35箇所）
- controllers/pvclickcontroller.cpp（32箇所）
- controllers/evaluationgraphcontroller.cpp（32箇所）
- wiring/josekiwindowwiring.cpp（16箇所）
- presenters/boardsyncpresenter.cpp（16箇所）
- wiring/csagamewiring.cpp（15箇所）
- controllers/playerinfocontroller.cpp（14箇所）
- presenters/evalgraphpresenter.cpp（10箇所）
- controllers/timecontrolcontroller.cpp（10箇所）
- wiring/playerinfowiring.cpp（8箇所）
- wiring/considerationwiring.cpp（6箇所）
- controllers/replaycontroller.cpp（4箇所）
- controllers/boardsetupcontroller.cpp（3箇所）
- controllers/usicommandcontroller.cpp（3箇所）
- coordinators/positioneditcoordinator.cpp（1箇所）
- coordinators/docklayoutmanager.cpp（1箇所）
- presenters/recordpresenter.cpp（1箇所）
- presenters/timedisplaypresenter.cpp（1箇所）
- wiring/menuwindowwiring.cpp（1箇所）
- wiring/recordpanewiring.cpp（1箇所）

### src/widgets/（6ファイル、84箇所）
- engineanalysistab.cpp（40箇所）
- evaluationchartwidget.cpp（26箇所）
- recordpane.cpp（12箇所）
- engineinfowidget.cpp（3箇所）
- gameinfopanecontroller.cpp（2箇所）
- branchtreewidget.cpp（1箇所）

### src/dialogs/（5ファイル、92箇所）
- josekiwindow.cpp（57箇所）
- pvboarddialog.cpp（23箇所）
- csawaitingdialog.cpp（10箇所）
- engineregistrationdialog.cpp（1箇所）
- josekimergedialog.cpp（1箇所）

### src/models/（1ファイル、7箇所）
- kifubranchlistmodel.cpp（7箇所）

### src/services/（2ファイル、3箇所）
- playernameservice.cpp（2箇所）
- timecontrolutil.cpp（1箇所）

## カテゴリ定義
- カテゴリ名: "shogi.ui"
- 変数名: lcUi
- 定義場所: src/ui/coordinators/kifudisplaycoordinator.cpp に Q_LOGGING_CATEGORY(lcUi, "shogi.ui")
- src/ui/coordinators/kifudisplaycoordinator.h に Q_DECLARE_LOGGING_CATEGORY(lcUi)
- 他ファイルからは kifudisplaycoordinator.h を include して参照
  （循環依存が生じる場合は、src/ui/ 直下に loggingcategory.h を新設し宣言を置いてよい）

## 既存プレフィックス例
- `[PvClick]` — pvclickcontroller.cpp
- `[PlayerInfo]` — playerinfocontroller.cpp
- `[CHART]` — evaluationchartwidget.cpp
- `[ENGINE_INFO]` — engineinfowidget.cpp
- `[JosekiWindow]` — josekiwindow.cpp
- `[BRANCH-MODEL]` — kifubranchlistmodel.cpp
- `[PlayerNameService]` — playernameservice.cpp

## レベル判断の目安
- UIイベント処理のトレース（クリック、行選択等） → qCDebug
- ダイアログ表示/操作 → qCDebug
- チャートの描画トレース → qCDebug（高頻度のものは削除検討）
- 定石ファイル読み込み成功 → qCInfo
- ファイルI/Oエラー → qCWarning
- 不正なフォーマット検出 → qCWarning

## 注意
ファイル数が多いため、サブディレクトリごとに作業しても構いません
（src/ui/ → src/widgets/ → src/dialogs/ → src/models/ → src/services/ の順）。

## ビルド確認
移行後に cmake --build build でビルドが通ることを確認してください。
```

---

### 10. src/app/ — shogi.app

```
src/app/ のデバッグ文を QLoggingCategory に移行してください。

## 規約
docs/debug-logging-guidelines.md を読んで従ってください。

## 対象ファイル（qDebug 箇所数）
- mainwindow.cpp（108箇所）
- main.cpp（13箇所）
- commentcoordinator.cpp（4箇所）

## カテゴリ定義
- カテゴリ名: "shogi.app"
- 変数名: lcApp
- 定義場所: mainwindow.cpp に Q_LOGGING_CATEGORY(lcApp, "shogi.app")
- mainwindow.h に Q_DECLARE_LOGGING_CATEGORY(lcApp)
- main.cpp, commentcoordinator.cpp からは mainwindow.h を include して参照

## 既存プレフィックス例
- `[MW]`, `[MainWindow]`, `[MainWindow::メソッド名]` — mainwindow.cpp
- `[CONNECT]` — mainwindow.cpp のシグナル/スロット接続ログ
- `[i18n]` — main.cpp の国際化関連
- `[CommentCoordinator]` — commentcoordinator.cpp

## 注意: main.cpp の messageHandler
main.cpp には messageHandler（カスタムログハンドラ）が定義されています。
messageHandler 自体は移行対象外です（フォーマッタとして残す）。
messageHandler 内の qDebug() は使用しないでください（再帰呼び出しになる）。
main.cpp 内の他の qDebug()（i18n 関連等）のみ移行対象です。

## レベル判断の目安
- 初期化・コンポーネント生成トレース → qCDebug
- シグナル/スロット接続ログ → qCDebug（接続失敗時は qCWarning）
- 国際化（i18n）の翻訳ファイルロード → qCInfo（起動時1回の運用情報）
- コメント変更ログ → qCDebug
- MainWindow のメソッド呼び出しトレース → qCDebug

## 注意
mainwindow.cpp は最もqDebug箇所が多く、他モジュールの移行で
参照パターンが確立した後に実施するのが効率的です（そのためこの順番にしています）。

## ビルド確認
移行後に cmake --build build でビルドが通ることを確認してください。
```

---

## 移行状況トラッキング

| # | モジュール | カテゴリ | ファイル数 | qDebug数 | 状態 |
|---|-----------|---------|-----------|---------|------|
| 1 | src/core/ | shogi.core | 3 | 84 | 未着手 |
| 2 | src/views/ | shogi.view | 1 | 19 | 未着手 |
| 3 | src/network/ | shogi.network | 2 | 47 | 未着手 |
| 4 | src/engine/ | shogi.engine | 6 | 54 | 未着手 |
| 5 | src/kifu/ | shogi.kifu | 11 | 168 | 未着手 |
| 6 | src/analysis/ | shogi.analysis | 5 | 92 | 未着手 |
| 7 | src/game/ | shogi.game | 6 | 166 | 未着手 |
| 8 | src/board/ + src/navigation/ | shogi.board, shogi.navigation | 3 | 46 | 未着手 |
| 9 | src/ui/ 他 | shogi.ui | 35 | 446 | 未着手 |
| 10 | src/app/ | shogi.app | 3 | 125 | 未着手 |
| — | **合計** | — | **75** | **1,247** | — |

> 既に移行済み: `src/core/shogiclock.cpp`（shogi.clock）、`src/views/shogiview.cpp`（部分的、要リネーム）
