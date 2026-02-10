# developer-guide.md 作成用プロンプト集

各プロンプトを1セッションずつ実行してください。
セッション間は `/clear` で区切ります。

---

## Session 2: 第2章 将棋のドメイン知識

```
docs/dev/developer-guide.md に第2章「将棋のドメイン知識」を作成してください。

<!-- chapter-2-start --> と <!-- chapter-2-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/core/sfenutils.h
- src/core/shogiutils.h
- src/core/shogimove.h
- src/core/shogiboard.h

記述してほしい内容:
- 盤面座標系の説明（筋1-9、段1-9、内部インデックスとの対応）
- SFEN文字列フォーマットの完全な解説（盤面・手番・持ち駒・手数）
- 駒の表現方法（大文字=先手、小文字=後手、成駒の文字）。sfenutils.hやshogiutils.hの実際の定数・変換関数を参照して記述
- USI指し手表記の解説（通常移動 7g7f、成り 2d2c+、駒打ち P*5e）
- KIF/KI2/CSA/JKF/USEN各フォーマットの概要と特徴の比較表
- コード例は実際のソースから引用

注意:
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 3: 第3章 アーキテクチャ全体図

```
docs/dev/developer-guide.md に第3章「アーキテクチャ全体図」を作成してください。

<!-- chapter-3-start --> と <!-- chapter-3-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/app/mainwindow.h
- src/core/playmode.h
- CLAUDE.md（Architecture セクション）

記述してほしい内容:
- レイヤー構造図（Mermaid図）: core → game/kifu/engine → ui/views/widgets の依存関係
- 各レイヤーの責務の1行説明
- MainWindowの「ハブ/ファサード」としての位置づけ（各層を接続する役割）
- PlayMode列挙型の全値とその意味、PlayModeによるアプリ全体の動作分岐の説明
- データフロー概要図（Mermaid図）: ユーザ操作 → 指し手 → 盤面更新 → 表示反映
- 層間の依存ルール（どの層がどの層を参照してよいか）

注意:
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 4: 第4章 設計パターンとコーディング規約

```
docs/dev/developer-guide.md に第4章「設計パターンとコーディング規約」を作成してください。

<!-- chapter-4-start --> と <!-- chapter-4-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/ui/wiring/considerationwiring.h（Deps構造体の実例）
- src/game/matchcoordinator.h（Hooks構造体の実例）
- src/common/errorbus.h
- src/app/mainwindow.h（ensure*() メソッド群）
- src/ui/presenters/boardsyncpresenter.h（Presenterの実例）
- docs/dev/commenting-style-guide.md

記述してほしい内容:
- Deps構造体パターン: 目的、構造、updateDeps()メソッド、実際のコードから引用した例
- ensure*()遅延初期化パターン: ガードパターン（if already exists, return）、MainWindowでの使用例
- Hooks/std::functionパターン: MatchCoordinator::Hooksの実例、コールバック注入の目的
- Wiringクラスパターン: シグナル/スロット接続の整理手法、connect()でラムダ禁止の理由と代替
- MVPパターン: Presenter/Controller/Coordinatorの使い分けと命名規則
- ErrorBus: グローバルエラー通知の仕組み
- コーディング規約: コンパイラ警告、std::as_const()、connect()の書き方ルール
- commenting-style-guide.mdの要約と参照リンク

注意:
- コード例は実際のソースファイルから引用してください
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 5: 第5章 core層

```
docs/dev/developer-guide.md に第5章「core層：純粋なゲームロジック」を作成してください。

<!-- chapter-5-start --> と <!-- chapter-5-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/core/shogiboard.h
- src/core/shogimove.h
- src/core/movevalidator.h
- src/core/shogiclock.h
- src/core/playmode.h
- src/core/legalmovestatus.h
- src/core/sfenutils.h

記述してほしい内容:
- ShogiBoard: 盤面データ構造（QVector<QChar>の81マス）、QMap駒台、SFEN変換メソッド、主要publicメソッド一覧
- ShogiMove: 移動元/先、駒種、成/不成の表現、USI文字列との相互変換
- MoveValidator: 合法手判定のアルゴリズム概要、LegalMoveStatusとの関係
- ShogiClock: 持ち時間/秒読み/フィッシャーの3モード、時間管理の仕組み
- core層がQt GUI非依存であることの意義（テスタビリティ、再利用性）

注意:
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 6: 第6章 game層

```
docs/dev/developer-guide.md に第6章「game層：対局管理」を作成してください。

<!-- chapter-6-start --> と <!-- chapter-6-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/game/matchcoordinator.h
- src/game/shogigamecontroller.h
- src/game/gamestartcoordinator.h
- src/game/gamestatecontroller.h
- src/game/turnmanager.h
- src/game/turnsyncbridge.h
- src/game/promotionflow.h
- src/game/prestartcleanuphandler.h
- src/game/consecutivegamescontroller.h

記述してほしい内容:
- MatchCoordinator: 司令塔クラスの全責務、StartOptions/GameEndInfo/GameOverState等の内部型、Hooksによるコールバック注入
- 対局開始シーケンス（Mermaid図）: GameStartCoordinator → PreStartCleanupHandler → MatchCoordinator
- ShogiGameController: ボードとルールの橋渡し、指し手実行の流れ
- TurnManager/TurnSyncBridge: 手番管理の仕組み、人間/エンジンの手番切替
- PromotionFlow: 成/不成選択ダイアログの表示フロー
- PreStartCleanupHandler: 対局前に何をクリーンアップするか
- ConsecutiveGamesController: 連続対局の制御
- GameStateController: ゲーム状態（対局中、終了等）の管理

注意:
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 7: 第7章 engine層

```
docs/dev/developer-guide.md に第7章「engine層：USIエンジン連携」を作成してください。

<!-- chapter-7-start --> と <!-- chapter-7-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/engine/usi.h
- src/engine/usiprotocolhandler.h
- src/engine/engineprocessmanager.h
- src/engine/thinkinginfopresenter.h
- src/engine/shogienginethinkingmodel.h
- src/engine/shogiengineinfoparser.h
- src/engine/enginesettingscoordinator.h
- src/engine/engineoptions.h
- src/engine/usicommlogmodel.h

記述してほしい内容:
- 3層構造の解説: EngineProcessManager（プロセス管理） → UsiProtocolHandler（プロトコル処理） → Usi（ファサード）
- USIプロトコルの状態遷移図（Mermaid図）: 起動 → usi → isready → readyok → position/go → bestmove
- Usiクラスの主要publicメソッドとシグナル一覧
- ThinkingInfoPresenter: info行のパース、読み筋(PV)の漢字変換処理
- ShogiEngineThinkingModel: Qtモデルとしての役割、TableViewへのバインド
- EngineSettingsCoordinator: エンジン登録・設定変更のフロー
- UsiCommLogModel: 通信ログの記録と表示

注意:
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 8: 第8章 kifu層

```
docs/dev/developer-guide.md に第8章「kifu層：棋譜管理」を作成してください。

<!-- chapter-8-start --> と <!-- chapter-8-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/kifu/kifubranchnode.h
- src/kifu/kifubranchtree.h
- src/kifu/kifubranchtreebuilder.h
- src/kifu/gamerecordmodel.h
- src/kifu/livegamesession.h
- src/kifu/kifuloadcoordinator.h
- src/kifu/kifusavecoordinator.h
- src/kifu/kifuioservice.h
- src/kifu/kifuexportcontroller.h
- src/kifu/kifunavigationstate.h
- src/kifu/kifutypes.h
- src/kifu/kifdisplayitem.h

記述してほしい内容:
- KifuBranchTree/KifuBranchNode: 分岐ツリーのデータ構造図（Mermaid図で木構造を図示）、ノードの持つ情報
- KifuBranchTreeBuilder: ツリー構築の流れ
- GameRecordModel: 棋譜データモデルの責務、コメント管理、エクスポート機能
- LiveGameSession: リアルタイム記録セッション、commit/discardパターンの解説
- KifuLoadCoordinator: ファイル読み込みフロー（ファイル選択 → パース → ツリー構築 → UI反映）
- KifuSaveCoordinator/KifuExportController: 保存・エクスポートの流れ
- KifuNavigationState: 現在位置の追跡、分岐の記憶
- フォーマット変換器一覧表（KIF, KI2, CSA, JKF, USEN, USI → SFEN）

注意:
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 9: 第9章 analysis層

```
docs/dev/developer-guide.md に第9章「analysis層：解析機能」を作成してください。

<!-- chapter-9-start --> と <!-- chapter-9-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/analysis/analysiscoordinator.h
- src/analysis/analysisflowcontroller.h
- src/analysis/considerationflowcontroller.h
- src/analysis/considerationmodeuicontroller.h
- src/analysis/tsumesearchflowcontroller.h
- src/analysis/analysisresultspresenter.h
- src/analysis/kifuanalysislistmodel.h

記述してほしい内容:
- 3モードの比較表: 検討(Consideration) / 棋譜解析(KifuAnalysis) / 詰将棋探索(TsumeSearch) — 目的、UIの違い、使用するUSIコマンド、結果の表示方法
- AnalysisCoordinator: Idle/SinglePosition/RangePositionsモードの状態遷移
- ConsiderationFlowController: 検討モードの開始・停止フロー、MatchCoordinator経由での検討
- AnalysisFlowController: 棋譜解析の範囲指定と逐次解析の仕組み
- TsumeSearchFlowController: go mate コマンドによる詰将棋探索
- AnalysisResultsPresenter: 解析結果の表示
- KifuAnalysisListModel: 解析結果リストのQtモデル

注意:
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 10: 第10章 UI層

```
docs/dev/developer-guide.md に第10章「UI層：プレゼンテーション」を作成してください。

<!-- chapter-10-start --> と <!-- chapter-10-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/ui/presenters/boardsyncpresenter.h
- src/ui/presenters/evalgraphpresenter.h
- src/ui/presenters/navigationpresenter.h
- src/ui/presenters/gamerecordpresenter.h
- src/ui/presenters/timedisplaypresenter.h
- src/ui/controllers/boardsetupcontroller.h
- src/ui/controllers/playerinfocontroller.h
- src/ui/controllers/replaycontroller.h
- src/ui/wiring/uiactionswiring.h
- src/ui/wiring/analysistabwiring.h
- src/ui/wiring/considerationwiring.h
- src/ui/coordinators/dialogcoordinator.h
- src/ui/coordinators/gamelayoutbuilder.h
- src/ui/coordinators/kifudisplaycoordinator.h

記述してほしい内容:
- UI層の4カテゴリの役割分担図（Mermaid図）: Presenters / Controllers / Wiring / Coordinators
- Presenters（5つ）: 各Presenterの責務を1〜2行で説明、データフローの方向（Model→View）
- Controllers（10個）: 各Controllerの一覧表（名前、責務、主な操作対象）
- Wiring（8つ）: 各Wiringクラスが接続するシグナル/スロットの概要、なぜWiringクラスに分離するのか
- Coordinators（6つ）: 各Coordinatorの役割、複数オブジェクト間の調整パターン

注意:
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 11: 第11章 views/widgets/dialogs層

```
docs/dev/developer-guide.md に第11章「views/widgets/dialogs層：Qt UI部品」を作成してください。

<!-- chapter-11-start --> と <!-- chapter-11-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/views/shogiview.h
- src/widgets/recordpane.h
- src/widgets/engineanalysistab.h
- src/widgets/evaluationchartwidget.h
- src/widgets/kifudisplay.h
- src/widgets/kifubranchdisplay.h
- src/widgets/branchtreewidget.h
- src/widgets/gameinfopanecontroller.h
- src/dialogs/startgamedialog.h
- src/dialogs/considerationdialog.h

記述してほしい内容:
- ShogiView: QGraphicsViewベースの描画、レンダリングレイヤー順序（背景→盤→駒→ハイライト→矢印→駒台→ラベル）、主要メソッド
- 主要ウィジェット一覧表（名前、親ウィジェット/配置場所、機能の1行説明）
- ダイアログ一覧表（名前、.uiファイルの有無、用途の1行説明）
- .uiファイルとC++クラスの統合方法（AUTOUICの動作、ui->setupUi()の呼び出し）
- カスタムウィジェットの設計指針（CollapsibleGroupBox、ElideLabel等の汎用部品）

注意:
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 12: 第12章 network層とservices層

```
docs/dev/developer-guide.md に第12章「network層とservices層」を作成してください。

<!-- chapter-12-start --> と <!-- chapter-12-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/network/csaclient.h
- src/network/csagamecoordinator.h
- src/services/settingsservice.h
- src/services/settingsservice.cpp
- src/services/timekeepingservice.h
- src/services/playernameservice.h
- src/services/timecontrolutil.h

記述してほしい内容:
- CsaClient: TCP/IP通信の実装、ConnectionState列挙型の全状態、状態遷移図（Mermaid図）
- CsaGameCoordinator: CsaClientとゲームシステム（MatchCoordinator等）の橋渡し役割
- SettingsService: INI設定ファイルの構造、設定項目の追加方法（getter/setter のコード例）、CLAUDE.mdのSettingsService Update Guidelinesとの関連
- TimekeepingService: 時間管理の仕組み
- PlayerNameService: 対局者名の解決ロジック
- TimeControlUtil: 時間制御のユーティリティ関数

注意:
- SettingsServiceのコード例は実際のsettingsservice.cppから引用してください
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 13: 第13章 navigation層とboard層

```
docs/dev/developer-guide.md に第13章「navigation層とboard層」を作成してください。

<!-- chapter-13-start --> と <!-- chapter-13-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/navigation/kifunavigationcontroller.h
- src/navigation/recordnavigationhandler.h
- src/board/boardinteractioncontroller.h
- src/board/positioneditcontroller.h
- src/board/boardimageexporter.h
- src/board/sfenpositiontracer.h

記述してほしい内容:
- KifuNavigationController: 6つのナビゲーションボタン（先頭/前/次/末尾/前分岐/次分岐）の動作、棋譜クリック・分岐クリックの処理
- RecordNavigationHandler: RecordPaneの行変更イベントの処理
- ナビゲーションフロー図（Mermaid図）: ユーザ操作 → Controller → KifuNavigationState → UI更新
- BoardInteractionController: 2クリック方式（選択→移動先）の実装、合法手ハイライトの管理
- PositionEditController: 局面編集モードの操作（駒配置、駒削除、手番切替）
- BoardImageExporter: 盤面画像のエクスポート処理
- SfenPositionTracer: SFEN局面の追跡と管理

注意:
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 14: 第14章 MainWindowの役割と構造

```
docs/dev/developer-guide.md に第14章「MainWindowの役割と構造」を作成してください。

<!-- chapter-14-start --> と <!-- chapter-14-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/app/mainwindow.h
- src/app/mainwindow.cpp（冒頭〜200行程度 + ensure*()メソッドのパターン確認）
- src/app/testautomationhelper.h
- src/app/dockcreationservice.h
- src/app/commentcoordinator.h

記述してほしい内容:
- 設計思想: "MainWindow should stay lean" の原則
- メンバ変数のグループ分け（状態変数、コントローラ/コーディネータ、モデル、UIウィジェット、ドック等）
- ensure*()による遅延生成パターン: 代表的なensureメソッドの実装例（実際のコードから引用）、生成されるオブジェクトの依存関係
- コンストラクタの処理フロー
- リファクタリング経緯の概要（5,368行 → 4,180行、何をどこに移譲したか）
- TestAutomationHelper: std::functionコールバックパターンによるMainWindow機能の外部化
- DockCreationService: ドックウィジェット生成の分離
- CommentCoordinator: コメント機能の委譲

注意:
- mainwindow.cppは大きいため、全体を読む必要はありません。ensure*()のパターンを数例確認すれば十分です
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 15: 第15章 機能フロー詳解

```
docs/dev/developer-guide.md に第15章「機能フロー詳解」を作成してください。

<!-- chapter-15-start --> と <!-- chapter-15-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/game/gamestartcoordinator.h
- src/game/matchcoordinator.h
- src/game/shogigamecontroller.h
- src/board/boardinteractioncontroller.h
- src/kifu/kifuloadcoordinator.h
- src/ui/wiring/considerationwiring.h
- src/analysis/considerationflowcontroller.h
- src/network/csaclient.h
- src/network/csagamecoordinator.h
- src/engine/usi.h

記述してほしい内容:
以下の5つのユースケースについて、それぞれMermaidシーケンス図と簡潔な説明文を記述してください。

1. 対局開始フロー: MenuWindow → StartGameDialog → GameStartCoordinator → PreStartCleanupHandler → MatchCoordinator → Usi
2. 指し手実行フロー: BoardClick → BoardInteractionController → MoveValidator → ShogiBoard → ShogiView → GameRecordModel → Usi（エンジン手番の場合）
3. 棋譜読み込みフロー: File選択 → KifuLoadCoordinator → KifReader → フォーマット変換 → KifuBranchTree構築 → UI反映
4. 検討モードフロー: 検討ボタン → ConsiderationWiring → ConsiderationFlowController → MatchCoordinator → Usi → ThinkingInfoPresenter → UI表示
5. CSA通信対局フロー: CsaGameDialog → CsaClient → サーバ接続 → ログイン → 対局開始 → 指し手交換 → 対局終了

注意:
- 各シーケンス図はMermaid sequenceDiagram 記法で記述してください
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 16: 第16章 国際化（i18n）と翻訳

```
docs/dev/developer-guide.md に第16章「国際化（i18n）と翻訳」を作成してください。

<!-- chapter-16-start --> と <!-- chapter-16-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/ui/controllers/languagecontroller.h
- src/ui/controllers/languagecontroller.cpp
- resources/translations/ShogiBoardQ_ja_JP.ts（冒頭部分の構造確認のみ）
- CLAUDE.md（Internationalization セクション）

記述してほしい内容:
- tr() → lupdate → .ts編集 → lrelease → .qm のワークフロー図
- LanguageController: 実行時言語切替の仕組み
- 翻訳追加の手順（新しいtr()文字列を追加してから翻訳が反映されるまで）
- .tsファイルの構造（XML形式の簡単な説明）

注意:
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 17: 第17章 新機能の追加ガイド

```
docs/dev/developer-guide.md に第17章「新機能の追加ガイド」を作成してください。

<!-- chapter-17-start --> と <!-- chapter-17-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- src/ui/wiring/considerationwiring.h/.cpp（Wiringクラスの実例として）
- src/services/settingsservice.h/.cpp（設定追加の実例として）
- src/app/mainwindow.h（ensure*()パターンの参照）
- CLAUDE.md（SettingsService Update Guidelines、Code Style セクション）
- docs/dev/commenting-style-guide.md

記述してほしい内容:
実践チュートリアル形式で、仮の新機能「棋譜にタグを付ける機能」を例に以下の手順を解説:

1. ファイル配置先の選定: どのディレクトリに置くべきか判断基準
2. Deps構造体を持つ新クラスの作成: ヘッダとソースのテンプレートコード
3. MainWindowにensure*()メソッドを追加: 遅延初期化の実装例
4. Wiringクラスでシグナル/スロット接続: connect()の書き方
5. SettingsServiceに設定項目を追加: getter/setterの実装例
6. tr()による翻訳対応: 翻訳可能文字列の書き方とlupdate実行
7. コメントの記述: commenting-style-guide.mdに従ったドキュメント
8. チェックリスト: 新機能追加時に確認すべき項目（コンパイラ警告、ラムダ禁止、std::as_const()等）

注意:
- テンプレートコードは本プロジェクトの実際のパターンに合わせてください
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```

---

## Session 18: 第18章 用語集・索引

```
docs/dev/developer-guide.md に第18章「用語集・索引」を作成してください。

<!-- chapter-18-start --> と <!-- chapter-18-end --> の間のプレースホルダを
実際の内容で置き換えてください。

主要参照ファイル:
- docs/dev/developer-guide.md（既存の全章を参照して索引を作成）
- src/core/shogiutils.h（将棋用語の参照）

記述してほしい内容:
- 将棋用語の日英対応表（先手/後手、成/不成、持ち駒、王手、詰み、千日手、入玉、持将棋など主要20語以上）
- クラス名逆引き一覧: 全主要クラス名のアルファベット順リスト（クラス名、所属ディレクトリ、1行説明、解説章番号）
- ファイル→章の対応表: 主要ファイルがどの章で解説されているかの逆引き
- 略語一覧: SFEN、USI、CSA、PV、KIF、KI2、JKF、USEN等の正式名称と意味

注意:
- 他の章の内容は変更しないでください
- 改訂履歴テーブルに日付と作成した章を追記してください
```
