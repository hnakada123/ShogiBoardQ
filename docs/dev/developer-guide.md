<!-- chapter-0-start -->
# ShogiBoardQ 開発者向けソースコード解説マニュアル

## 本マニュアルについて

**対象読者**: ShogiBoardQプロジェクトに新規参加する開発者

**前提知識**:
- C++17 の基本的な文法（`std::function`、構造化束縛、`std::optional` 等）
- Qt 6 の基礎（シグナル/スロット、`QWidget`、`QObject` の親子関係）
- CMake によるビルド経験
- 将棋のルールについては第2章で解説するため、事前知識は不要

**関連ドキュメント**:
- [CLAUDE.md](../CLAUDE.md) — プロジェクトの開発ガイドライン（ビルド手順、アーキテクチャ概要、コーディング規約）
- [commenting-style-guide.md](commenting-style-guide.md) — コメント記述のスタイルガイド

---

## 目次

| 章 | タイトル | 概要 |
|----|---------|------|
| [第0章](#第0章-表紙目次) | 表紙・目次 | 本ページ |
| [第1章](#第1章-プロジェクト概要とビルド手順) | プロジェクト概要とビルド手順 | 機能紹介、ビルド方法、ディレクトリ構造 |
| [第2章](#第2章-将棋のドメイン知識) | 将棋のドメイン知識 | SFEN、USI記法、駒表現、座標系 |
| [第3章](#第3章-アーキテクチャ全体図) | アーキテクチャ全体図 | レイヤー構造、データフロー、MainWindowの位置づけ |
| [第4章](#第4章-設計パターンとコーディング規約) | 設計パターンとコーディング規約 | Deps構造体、ensure*()、Wiring、MVP、コード規約 |
| [第5章](#第5章-core層純粋なゲームロジック) | core層：純粋なゲームロジック | ShogiBoard、ShogiMove、EngineMoveValidator |
| [第6章](#第6章-game層対局管理) | game層：対局管理 | MatchCoordinator、TurnManager、GameStateController |
| [第7章](#第7章-engine層usiエンジン連携) | engine層：USIエンジン連携 | Usiファサード、プロトコルハンドラ、思考情報表示 |
| [第8章](#第8章-kifu層棋譜管理) | kifu層：棋譜管理 | 分岐ツリー、棋譜モデル、フォーマット変換 |
| [第9章](#第9章-analysis層解析機能) | analysis層：解析機能 | 検討・棋譜解析・詰将棋探索の3モード |
| [第10章](#第10章-ui層プレゼンテーション) | UI層：プレゼンテーション | Presenters/Controllers/Wiring/Coordinators |
| [第11章](#第11章-viewswidgetsdialogs層qt-ui部品) | views/widgets/dialogs層：Qt UI部品 | ShogiView描画、ウィジェット、ダイアログ |
| [第12章](#第12章-network層とservices層) | network層とservices層 | CSA通信、設定サービス、時間管理 |
| [第13章](#第13章-navigation層とboard層) | navigation層とboard層 | 棋譜ナビゲーション、盤面操作 |
| [第14章](#第14章-mainwindowの役割と構造) | MainWindowの役割と構造 | ensure*()群、委譲パターン、リファクタリング経緯 |
| [第15章](#第15章-機能フロー詳解) | 機能フロー詳解 | 5大ユースケースのシーケンス図 |
| [第16章](#第16章-国際化i18nと翻訳) | 国際化（i18n）と翻訳 | Qt Linguist、LanguageController |
| [第17章](#第17章-新機能の追加ガイド) | 新機能の追加ガイド | 実践チュートリアル |
| [第18章](#第18章-用語集索引) | 用語集・索引 | 将棋用語日英対応、クラス名逆引き |

---

## 改訂履歴

| 日付 | 内容 |
|------|------|
| 2026-02-08 | 初版作成（第0章・第1章） |
| 2026-02-08 | 第2章「将棋のドメイン知識」作成 |
| 2026-02-08 | 第3章「アーキテクチャ全体図」作成 |
| 2026-02-08 | 第4章「設計パターンとコーディング規約」作成 |
| 2026-02-08 | 第5章「core層：純粋なゲームロジック」作成 |
| 2026-02-08 | 第6章「game層：対局管理」作成 |
| 2026-02-08 | 第7章「engine層：USIエンジン連携」作成 |
| 2026-02-08 | 第8章「kifu層：棋譜管理」作成 |
| 2026-02-08 | 第9章「analysis層：解析機能」作成 |
| 2026-02-08 | 第10章「UI層：プレゼンテーション」作成 |
| 2026-02-08 | 第11章「views/widgets/dialogs層：Qt UI部品」作成 |
| 2026-02-08 | 第12章「network層とservices層」作成 |
| 2026-02-08 | 第13章「navigation層とboard層」作成 |
| 2026-02-08 | 第14章「MainWindowの役割と構造」作成 |
| 2026-02-08 | 第15章「機能フロー詳解」作成 |
| 2026-02-08 | 第16章「国際化（i18n）と翻訳」作成 |
| 2026-02-08 | 第17章「新機能の追加ガイド」作成 |
| 2026-02-08 | 第18章「用語集・索引」作成 |
| 2026-10-13 | 配線層の整理に合わせて第1章・第4章・第6章・第8章・第14章・第18章を更新（司令塔と棋譜読み込み担当は一度だけ作る。`MainWindowCompositionRoot` などの中継層を削除し、生成はレジストリに集約） |
| 2026-10-13 | 設定の保存を宣言の表（`Setting<T>`）に移したのに合わせて第1章・第7章・第9章・第11章・第12章・第16章・第17章・第18章を更新（設定項目はドメイン別ヘッダーに1行で宣言し、getter/setter の組を書かない。§12.5・§16.6・§17.5 を書き直し） |
| 2026-10-13 | 盤（`ShogiBoard`）が駒を `Piece`、駒台を `QMap<Piece, int>` で持つのに合わせて §2.1〜§2.3・§5.2 を更新（`QChar` の盤と、削除した関数・手数のメンバーの記述を除く。駒台の0枚の表を共有することと、SFEN の盤の欄で内部の成駒の文字を読まないことを追記） |
| 2026-10-14 | 合法手判定が `EngineMoveValidator`（内部は `fmv*`）になっているのに合わせて、`FastMoveValidator` の記述を直し、§2.2 の後半・§5.4・§6.6 を書き直し（§6.6 は今の `validateAndMove()`・`applyMove()`・`commitMove()` の流れとシグナル） |
| 2026-10-14 | §15.2 指し手実行フローの図・役割の表・補足を今の流れに書き直し（着手要求は `BoardSetupController` が振り分け、盤の再描画は `ShogiBoard` のシグナル、棋譜の追記は `GameRecordUpdateService`。存在しない `boardChanged` シグナル・違法手の「エラー通知」・`decidePromotion()` などを削除） |
| 2026-10-14 | 盤の着手の流れの記述を今のコードに合わせて更新（§3.5 のデータフロー概要、§6.2 の司令塔の責務の表、§6.7 の `TurnManager` の変換メソッド・`TurnSyncBridge` の接続・手番切替の図、§6.8 の PromotionFlow の呼ばれ方とソースの場所、§10.3 の BoardSetupController の処理フロー、§10.6 の UI層全体のデータフロー。存在しない `engineMoveOnce()`・`onHumanMove_HvE()`・`decidePromotion()`・`setMandatoryPromotionFlag()`・`squareClicked`・`TurnManager::setFromGc()` などを削除） |
| 2026-10-14 | 対局者（先手・後手）を `Turn` にまとめたのに合わせて §4・§5.5・§6.2・§6.7・§6.9・§10 を更新（`MatchPlayer`（P1/P2）を削除。時計の API は `Turn` で受け、時間切れは `playerTimeOut(Turn)` の1つのシグナル。エンジンの投了・勝ち宣言・反則はエンジンの手番で受ける。`TurnManager::toClockPlayer()` を削除） |
| 2026-10-14 | 先後を `bool` で持っていたもの（`engineIsP1`・`loserIsP1`・`lastLoserIsP1`・`bottomIsP1`・`appendEvalP1/P2`・`p1turn`）を `Turn` にしたのに合わせて §4・§6.2・§6.3・§10 を更新（§6.2 の `configureAndStart()` の流れと §6.3 の図を、Strategy を作る今の流れに書き直し。盤の反転で時計の表示を描き直す `TimeDisplayPresenter::refresh()` を追記） |
| 2026-10-14 | 使われていなかった時間切れの投了シグナル（`ShogiClock::resignationTriggered`）と、編集開始の局面の候補の `resumeSfenStr` を削除したのに合わせて §5.5・§13 を更新 |
| 2026-10-14 | 司令塔と `GameStartCoordinator` のシグナルを受け手へ直接つなぐようにしたのに合わせて §6.2（シグナルの接続先の表、UI・Game のフックをシグナルにしない理由）・§6.9・§6.10・§6.11・§14.4 を更新 |
| 2026-10-14 | 古い記述を今のコードに合わせて更新（§1.2・§1.4・§17.1 のソースの列挙、§1.6 の規模の表、§4.1 の ConsiderationWiring の例、§4.4・§10.4 の UiActionsWiring の例、§13.6 の局面編集の API、§14.2 のメンバーの表、§14.5.1 を TestAutomationHelper から自動化 API に、§14.6 のスロット構造、§14.7 の行数、第16章を4言語と `ApplicationTranslations` に、§17.3 をレジストリへの追加手順に） |
| 2026-10-14 | §1.5 のディレクトリの木を今の構成に合わせて更新（プロジェクトルートに `Hayanagi/`・`mcp/`・`data/`・`packaging/` など、`src/` に `automation/`・`cli/` と主要なファイルを追加。存在しない `enginesettingscoordinator`・`navigationpresenter`・`recordpresenter`・`gamelayoutbuilder`・`branchrowdelegate` を削除） |
| 2026-10-14 | `prestartcleanuphandler.h` を `src/app/`、`promotionflow.h` を `src/ui/coordinators/` に移し、ヘッダーとソースを同じディレクトリにしたのに合わせて §1.5・§6.1・§6.5・§6.8 を更新。§1.8 のテスト構成を今の `tests/` に合わせて書き直し（存在しない `fakes/`・`tst_prestartcleanuphandler.cpp` などを削除） |

<!-- chapter-0-end -->

---

<!-- chapter-1-start -->
## 第1章 プロジェクト概要とビルド手順

### 1.1 ShogiBoardQとは

ShogiBoardQは、Qt 6とC++17で構築された将棋盤アプリケーションである。以下の主要機能を持つ。

| 機能カテゴリ | 内容 |
|-------------|------|
| **対局** | 人間 vs エンジン、エンジン vs エンジン、人間同士の対局。連続対局にも対応 |
| **棋譜管理** | KIF/KI2/CSA/JKF/USEN/USI形式の読み込み・保存、分岐棋譜の管理 |
| **エンジン解析** | USI対応エンジンによる局面検討、棋譜解析、詰将棋探索 |
| **CSA通信対局** | CSAプロトコルによるネットワーク対局 |
| **局面編集** | 任意の局面をGUIで作成、SFEN文字列からの読み込み |
| **盤面画像出力** | 現在の局面を画像ファイルとしてエクスポート |
| **定跡管理** | 定跡データの閲覧、結合、移動 |
| **国際化** | 日本語・英語・簡体字中国語・繁体字中国語の4言語対応（メニューで切り替え、再起動で反映） |

### 1.2 技術スタック

| 項目 | バージョン/仕様 |
|------|---------------|
| C++規格 | C++17（必須） |
| GUIフレームワーク | Qt 6（Qt 5 非対応） |
| Qt モジュール | Widgets, Charts, Network, LinguistTools |
| ビルドシステム | CMake 3.16以上 |
| エンジン通信 | USI（Universal Shogi Interface）プロトコル |
| ネットワーク対局 | CSA（Computer Shogi Association）プロトコル |

### 1.3 ビルド手順

#### 前提条件

- CMake 3.16 以上
- Qt 6 の開発パッケージ（Widgets, Charts, Network, LinguistTools）
- C++17 対応コンパイラ（GCC, Clang）

#### 基本ビルド

```bash
# 構成
cmake -B build -S .

# ビルド
cmake --build build

# Ninja使用（高速）
cmake -B build -S . -G Ninja
ninja -C build

# 実行
./build/ShogiBoardQ
```

#### 静的解析付きビルド

```bash
# clang-tidy（未使用コード検出）
cmake -B build -S . -DENABLE_CLANG_TIDY=ON
cmake --build build

# cppcheck（未使用関数検出）
cmake -B build -S . -DENABLE_CPPCHECK=ON
cmake --build build
```

clang-tidyは以下のチェックを実行する:
- `clang-diagnostic-unused-*` — 未使用の警告
- `misc-unused-*` — 未使用コードの検出
- `clang-analyzer-deadcode.*` — デッドコード解析
- `readability-redundant-*` — 冗長なコードの検出

cppcheckは `--enable=warning,unusedFunction` で未使用関数を検出する。

#### テストビルド

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build
```

#### 翻訳ファイルの更新

`tr()` でラップされた文字列を追加・変更した後に実行する:

```bash
lupdate src -ts resources/translations/ShogiBoardQ_ja_JP.ts resources/translations/ShogiBoardQ_en.ts
```

### 1.4 CMakeビルド構成の概要

`CMakeLists.txt` の主要な設定:

```cmake
# Qt自動処理（.ui, MOC, リソース）
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ソースファイルはディレクトリ別に列挙する（一覧の更新: ./scripts/update-sources.sh）
set(SRC_APP
    src/app/applicationlogging.cpp
    src/app/applicationlogging.h
    # ...
)
# ... SRC_CORE から SRC_CLI まで、ディレクトリごとに1つ（全18）

# 本体のオブジェクトライブラリ（SRC_CLI 以外）。CLI とテストは同じオブジェクトの静的ライブラリを使う
qt_add_library(shogiboardq_objects OBJECT ${SRC_APP} ${SRC_CORE} ... ${SRC_AUTOMATION} ${UI_FILES})
add_library(shogiboardq_static STATIC $<TARGET_OBJECTS:shogiboardq_objects>)

qt_add_executable(ShogiBoardQ src/app/main.cpp resources/shogiboardq.qrc ...)
qt_add_executable(shogiboardq-cli ${SRC_CLI} resources/shogiboardq.qrc)
```

ソースファイルは `GLOB` で集めず、`CMakeLists.txt` に列挙している。新しいファイルを追加したら、ディレクトリに対応する `SRC_*` に `.cpp` と `.h` を書き足す（`scripts/update-sources.sh` がディレクトリ別の一覧を出力する）。`.ui` ファイルは `UI_FILES` に足す。テストのソースは `tests/CMakeLists.txt` に別に列挙する。`shogiboardq_static` にリンクするテストは、本体に足したファイルをそのまま使える。

**コンパイラ警告**: 以下の警告オプションが有効化されている:

```
-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
-Wnon-virtual-dtor -Woverloaded-virtual -Wformat=2 -Wimplicit-fallthrough
-Wunused -Wunused-function -Wunused-variable -Wunused-parameter
```

### 1.5 ディレクトリ構造

主なファイルとディレクトリだけを載せる（2026-10-14 時点）。ソースの全体は `CMakeLists.txt` の `SRC_*`（[1.4節](#14-cmakeビルド構成の概要)）、規模は [1.6節](#16-コード規模) を参照。

#### プロジェクトルート

```
ShogiBoardQ/
├── CMakeLists.txt          # トップレベルビルド構成（ソースは SRC_* に列挙）
├── CMakePresets.json       # CMake のプリセット
├── CLAUDE.md               # 開発ガイドライン（AGENTS.md・GEMINI.md は他の AI 向けの同様の文書）
├── README.md               # プロジェクト概要
├── LICENSE                 # ライセンス
├── .clang-tidy             # 静的解析設定
├── .github/                # CI（workflows/）、Issue・PR のテンプレート
├── cmake/                  # CMake の補助ファイル
├── Hayanagi/               # 詰将棋エンジン（git submodule。USI エンジンと、詰将棋対局の相手として組み込む）
├── src/                    # ソースコード（後述）
├── tests/                  # テストコード（tst_*.cpp と、スタブの test_stubs_*.cpp）
│   ├── fixtures/           #   テスト用の棋譜など
│   └── gui/                #   GUI シナリオ・GUI フロー・エンジン終了のハーネス（xvfb-run の上で実行）
├── mcp/                    # MCP サーバー（Python。shogiboardq-cli と自動化 API を使う）
├── resources/              # リソースファイル
│   ├── shogiboardq.qrc     # Qt リソース定義ファイル
│   ├── tsumeshogi.qrc      # 同梱の詰将棋問題集の検査結果のリソース定義
│   ├── icons/              # アプリケーションアイコン
│   ├── images/
│   │   ├── actions/        # メニュー/ツールバーアイコン
│   │   └── pieces/         # 駒画像
│   ├── sounds/             # 駒音
│   ├── licenses/           # 同梱するライセンス文書
│   ├── platform/           # プラットフォーム固有ファイル（app.rc・.desktop）
│   ├── translations/       # 翻訳ファイル（4言語の .ts と、未翻訳数のベースライン）
│   └── include-baseline.txt # include 数の上限（scripts/check-include-counts.sh）
├── data/tsumeshogi/        # 詰将棋問題集（3〜13手詰、各1000題）と検査結果
├── docs/                   # 公開サイト（index.html・guide/・en/・zh-cn/・zh-tw/）と開発文書（dev/）
├── packaging/arch/         # Arch Linux のパッケージ定義
├── scripts/                # ビルド・配布・検査のスクリプト
└── build/                  # ビルド出力（git管理外）
```

#### ソースコード（`src/`）

ヘッダファイル（`.h`）は対応するソースファイル（`.cpp`）と同じディレクトリに配置される。`foo.cpp/.h` は `foo.cpp` と `foo.h`、`foo*.cpp` は `foo.cpp` と `foo_*.cpp` に分割したファイルを表す。

```
src/
├── app/                            # アプリケーションエントリーポイントと部品の生成・起動手順
│   ├── main.cpp                    #   エントリーポイント
│   ├── mainwindow.cpp/.h/.ui       #   メインウィンドウ（部品を持ち、処理はレジストリへ委譲）
│   ├── mainwindowstate.h           #   MainWindow の状態の構造体（モデル・ドック・分岐ナビ・棋譜・対局）
│   ├── mainwindowserviceregistry.cpp/.h # ensure*（部品の生成と依存更新）と主要操作の集約
│   ├── gamesubregistry*.cpp        #   同レジストリの対局系（時計・対局開始・セッション・司令塔の配線）
│   ├── mainwindowuiregistry.cpp    #   同レジストリの UI 系（同じく analysis・board・automation の *registry.cpp）
│   ├── mainwindowuibootstrapper.cpp #  同レジストリの起動時の UI 構築（ドックは mainwindowdockbootstrapper.cpp）
│   ├── mainwindowlifecyclewiring.cpp # 同レジストリの起動・終了の配線
│   ├── mainwindowfoundationregistry.cpp/.h # 複数の領域が使う部品の ensure*
│   ├── kifusubregistry.cpp/.h      #   棋譜系の ensure*（読み込み・保存・分岐ナビ・定跡）
│   ├── mainwindowlifecyclepipeline.cpp/.h # 起動・終了の手順
│   ├── mainwindowmatchadapter.cpp/.h #  司令塔のフックの受け先（盤の描画・ハイライト・終局の表示）
│   ├── gamesessionorchestrator*.cpp/.h # UI 起点の対局操作（開始・投了・中断・再開）
│   ├── sessionlifecyclecoordinator.cpp/.h # 対局の開始・終局・リセットの後処理
│   ├── prestartcleanuphandler.cpp/.h # 対局開始前のクリーンアップ（棋譜欄・分岐・ログ・時計表示・評価値グラフ）
│   ├── playmodepolicyservice.cpp/.h #  プレイモード依存の判定
│   ├── turnstatesyncservice.cpp/.h #   手番同期（TurnManager/GC/View/時計）
│   ├── gamerecordupdateservice.cpp/.h # 棋譜追記・ライブセッション更新
│   ├── boardloadservice.cpp/.h     #   SFEN盤面適用・ハイライト同期
│   ├── kifunavigationcoordinator.cpp/.h # 棋譜ナビゲーション同期
│   ├── commentcoordinator.cpp/.h   #   コメント機能の調整
│   └── dockcreationservice.cpp/.h  #   ドックウィジェット生成
│
├── core/                           # 純粋なゲームロジック（Qt GUI非依存）
│   ├── shogitypes.h                #   手番（Turn）と駒（Piece）
│   ├── shogiboard*.cpp/.h          #   盤面状態・駒台
│   ├── usimove.h                   #   USI と1対1に対応する指し手（UsiMove）
│   ├── shogimove.cpp/.h            #   指した手の記録（動かした駒・取った駒を含む）
│   ├── boardcoordinates.h          #   盤面表示の座標と USI の指し手の対応
│   ├── enginemovevalidator.cpp/.h  #   合法手判定（内部は fmv*.cpp/.h のビットボード）
│   ├── shogiclock.cpp/.h           #   対局時計
│   ├── timecontrol.h               #   持ち時間の決まり（持ち時間・秒読み・加算・遅延）
│   ├── shoginotation.h             #   棋譜の表記（筋・段・駒の名前・CSA の駒種）
│   ├── kifmovetext.h               #   KIF 形式の指し手の文字列
│   ├── sfenutils.h                 #   SFEN文字列操作
│   ├── playmode.h                  #   プレイモード定義
│   └── legalmovestatus.h           #   合法手判定の状態
│
├── game/                           # 対局管理
│   ├── matchcoordinator*.cpp/.h    #   対局全体の司令塔
│   ├── matchcoordinatorhooks.h     #   司令塔から上位層へのフック
│   ├── matchturnhandler.cpp/.h     #   手番の進行と、対局モード別の Strategy の管理
│   ├── gamemodestrategy.h          #   対局モード別の Strategy（実装は humanvs*strategy・enginevsenginestrategy）
│   ├── gamestartorchestrator.cpp/.h #  司令塔の対局開始の段取り
│   ├── gameendhandler*.cpp/.h      #   終局処理
│   ├── enginelifecyclemanager.cpp/.h # エンジンの生成と破棄
│   ├── matchtimekeeper.cpp/.h      #   時計の配線と残り時間の通知
│   ├── sennichitedetector.cpp/.h   #   千日手の検出
│   ├── shogigamecontroller.cpp/.h  #   ゲームフロー制御（指し手の検証と盤への適用）
│   ├── gamestartcoordinator*.cpp/.h #  対局開始フロー（ダイアログから開始まで）
│   ├── gamestatecontroller.cpp/.h  #   ゲーム状態制御
│   ├── turnmanager.cpp/.h          #   手番管理
│   ├── turnsyncbridge.cpp/.h       #   手番同期ブリッジ
│   └── consecutivegamescontroller.cpp/.h # 連続対局制御
│
├── kifu/                           # 棋譜管理
│   ├── gamerecordmodel.cpp/.h      #   棋譜データモデル
│   ├── kifubranchnode.cpp/.h       #   分岐ノード
│   ├── kifubranchtree*.cpp/.h      #   分岐ツリー（棋譜の正本）
│   ├── kifubranchtreebuilder.cpp/.h #  分岐ツリー構築
│   ├── livegamesession.cpp/.h      #   対局中の記録セッション
│   ├── kifuloadcoordinator.cpp/.h  #   棋譜読み込み調整
│   ├── kifufilecontroller.cpp/.h   #   棋譜ファイルの操作（開く・保存・上書き・貼り付け・自動保存）
│   ├── kifusavecoordinator.cpp/.h  #   棋譜保存調整
│   ├── kifuioservice.cpp/.h        #   棋譜ファイルI/O
│   ├── shiftjiscodec.cpp/.h        #   Shift_JIS（CP932）の変換
│   ├── kifuexportcontroller.cpp/.h #   棋譜エクスポート制御
│   ├── kifuexportmetadata.cpp/.h   #   棋譜出力のヘッダー（対局者・手合割など）
│   ├── kifuclipboardservice.cpp/.h #   クリップボード操作
│   ├── kifucontentbuilder.cpp/.h   #   棋譜コンテンツ構築
│   ├── kifunavigationstate.cpp/.h  #   ナビゲーション状態（棋譜のカーソル）
│   ├── recordline.h                #   分岐ツリーの1本の手順の値（RecordLine）
│   ├── kifreader.cpp/.h            #   棋譜ファイル読み込み
│   ├── shogiinforecord.cpp/.h      #   対局情報レコード
│   ├── kifdisplayitem.h            #   棋譜表示アイテム定義
│   ├── kifparsetypes.h             #   棋譜パース型定義
│   ├── kifutypes.h                 #   棋譜共通型定義
│   └── formats/                    #   フォーマット変換器
│       ├── kiftosfenconverter.cpp/.h   #   KIF → SFEN
│       ├── ki2tosfenconverter.cpp/.h   #   KI2 → SFEN
│       ├── csatosfenconverter.cpp/.h   #   CSA → SFEN
│       ├── jkftosfenconverter.cpp/.h   #   JKF → SFEN
│       ├── usentosfenconverter*.cpp/.h #   USEN → SFEN
│       ├── usitosfenconverter.cpp/.h   #   USI → SFEN
│       ├── kiflexer*.cpp/.h            #   KIF の字句解析（ki2lexer・csalexer も同様）
│       ├── kifexporter.cpp/.h          #   KIF の書き出し（ki2・csa・jkf・usen・usi の *exporter も同様）
│       └── bodtextgenerator.cpp/.h     #   BOD（局面図）の文字列
│
├── analysis/                       # 解析機能
│   ├── analysiscoordinator.cpp/.h  #   解析オーケストレーション
│   ├── analysisflowcontroller*.cpp/.h # 棋譜解析のフロー制御
│   ├── considerationflowcontroller.cpp/.h # 検討モード制御
│   ├── considerationmodeuicontroller.cpp/.h # 検討UI制御
│   ├── tsumesearchflowcontroller.cpp/.h # 詰み探索制御
│   ├── tsumeshogigenerator*.cpp/.h #   詰将棋局面の自動生成
│   ├── tsumecollection.cpp/.h      #   詰将棋問題集のファイルの読み込みと検証
│   ├── tsumegamesession.cpp/.h     #   詰将棋対局（応手は Hayanagi）
│   ├── analysisresultspresenter.cpp/.h # 解析結果表示
│   └── kifuanalysislistmodel.cpp/.h # 棋譜解析リストモデル
│
├── engine/                         # USIエンジン連携
│   ├── usi*.cpp/.h                 #   エンジンファサード（Usi）
│   ├── usiprotocolhandler*.cpp/.h  #   USIプロトコル処理
│   ├── usimatchhandler.cpp/.h      #   対局・解析での USI のやり取り（position/go/bestmove）
│   ├── usigotime.cpp/.h            #   go の時間の計算
│   ├── usienginesession.cpp/.h     #   GUI モデルを持たない USI 接続（自動化・CLI 用）
│   ├── engineprocessmanager*.cpp/.h #  エンジンプロセス管理
│   ├── tsumemateengine.cpp/.h      #   詰み判定エンジン（KomoringHeights）
│   ├── shogiengineinfoparser.cpp/.h # エンジン情報パース
│   ├── shogienginethinkingmodel.cpp/.h # 思考情報Qtモデル
│   ├── thinkinginfopresenter.cpp/.h # 思考情報表示
│   ├── usicommlogmodel.cpp/.h      #   USI通信ログモデル
│   ├── engineoptiondescriptions.cpp/.h # エンジンオプション説明
│   ├── engineoptions.h             #   エンジンオプション定義
│   └── enginesettingsconstants.h   #   エンジン設定定数
│
├── network/                        # CSA通信対局
│   ├── csaclient*.cpp/.h           #   CSAプロトコルクライアント
│   ├── csagamecoordinator*.cpp/.h  #   CSA対局調整
│   ├── csaenginecontroller.cpp/.h  #   通信対局で指すエンジンの制御
│   ├── csamoveprogresshandler.cpp/.h # 指し手の進行
│   └── csamoveconverter*.cpp/.h    #   CSA・USI・SFEN の指し手の変換
│
├── navigation/                     # ナビゲーション
│   ├── kifunavigationcontroller*.cpp/.h # 棋譜ナビゲーション制御
│   └── recordnavigationhandler.cpp/.h # レコードペイン行変更ハンドラ
│
├── board/                          # 盤面操作
│   ├── boardinteractioncontroller.cpp/.h # 盤面インタラクション制御
│   ├── positioneditcontroller.cpp/.h # 局面編集制御
│   ├── boardimageexporter.cpp/.h   #   盤面画像エクスポート
│   └── sfenpositiontracer.cpp/.h   #   SFEN局面追跡
│
├── ui/                             # UI層
│   ├── sfendiffutils.h             #   2つの局面（SFEN）の盤上の差分
│   ├── presenters/                 #   MVPプレゼンター層
│   │   ├── boardsyncpresenter.cpp/.h   # 盤面同期
│   │   ├── evalgraphpresenter.cpp/.h   # 評価グラフ
│   │   ├── gamerecordpresenter.cpp/.h  # 棋譜表示
│   │   ├── kifudisplaypresenter*.cpp/.h # 棋譜表示データの構築
│   │   ├── engineanalysispresenter.cpp/.h # 思考ビュー
│   │   └── timedisplaypresenter.cpp/.h # 時間表示
│   ├── controllers/                #   UIイベントハンドラ
│   │   ├── boardsetupcontroller.cpp/.h     # 盤面セットアップ（着手要求の振り分け）
│   │   ├── mainwindowappearancecontroller.cpp/.h # 盤の拡大・縮小・反転とウィンドウの外観
│   │   ├── evaluationgraphcontroller.cpp/.h # 評価グラフ制御
│   │   ├── candidatearrowcontroller.cpp/.h # 候補手の矢印
│   │   ├── branchtreeeditcontroller.cpp/.h # 分岐ツリーの編集
│   │   ├── languagecontroller.cpp/.h       # 言語・棋譜表記の切替
│   │   ├── playerinfocontroller.cpp/.h     # 対局者情報制御
│   │   ├── pvclickcontroller.cpp/.h        # PVクリック制御
│   │   ├── replaycontroller.cpp/.h         # リプレイ制御
│   │   ├── timecontrolcontroller.cpp/.h    # 時間制御
│   │   ├── usicommandcontroller.cpp/.h     # USIコマンド制御
│   │   ├── settingsresetcontroller.cpp/.h  # 設定の初期化
│   │   ├── nyugyokudeclarationhandler.cpp/.h # 入玉宣言処理
│   │   └── jishogiscoredialogcontroller.cpp/.h # 持将棋スコア
│   ├── wiring/                     #   シグナル/スロット接続
│   │   ├── mainwindowsignalrouter.cpp/.h # MainWindow まわりの配線の集約
│   │   ├── uiactionswiring.cpp/.h      # UIアクション接続（file・game・edit・view の *actionswiring）
│   │   ├── dialoglaunchwiring.cpp/.h   # ダイアログ起動
│   │   ├── dialogcoordinatorwiring.cpp/.h # DialogCoordinator の生成と配線
│   │   ├── analysistabwiring.cpp/.h    # 解析タブ接続
│   │   ├── considerationwiring.cpp/.h  # 検討モード接続
│   │   ├── csagamewiring*.cpp/.h       # CSA対局接続
│   │   ├── playerinfowiring.cpp/.h     # 対局者情報接続
│   │   ├── recordpanewiring.cpp/.h     # 棋譜ペイン接続
│   │   ├── recordnavigationwiring.cpp/.h # 棋譜ナビゲーション接続
│   │   ├── branchnavigationwiring.cpp/.h # 分岐ナビゲーション接続
│   │   ├── menuwindowwiring.cpp/.h     # メニューウィンドウ接続
│   │   ├── matchcoordinatorwiring.cpp/.h # 司令塔の生成と配線
│   │   └── josekiwindowwiring.cpp/.h   # 定跡ウィンドウ接続
│   └── coordinators/               #   UI調整
│       ├── uistatepolicymanager.cpp/.h  # 状態ごとの UI の有効・無効（状態表）
│       ├── dialogcoordinator*.cpp/.h    # ダイアログ調整
│       ├── docklayoutmanager.cpp/.h     # ドックレイアウト管理
│       ├── kifudisplaycoordinator*.cpp/.h # 棋譜表示調整
│       ├── kifuselectionsync.cpp/.h     # 棋譜の選択状態の同期
│       ├── positioneditcoordinator.cpp/.h # 局面編集調整
│       ├── promotionflow.cpp/.h         # 成/不成の選択ダイアログ
│       └── aboutcoordinator.cpp/.h      # バージョン情報調整
│
├── views/                          # Qt Graphics View
│   ├── shogiview*.cpp/.h           #   将棋盤描画ビュー（描画・イベント・駒台などに分割）
│   ├── shogiviewlayout.cpp/.h      #   盤面のレイアウト計算
│   ├── shogiviewinteraction.cpp/.h #   盤面の操作（クリック・ドラッグ）
│   ├── shogiviewhighlighting.cpp/.h #  ハイライト・矢印・手番表示
│   ├── piecepainter.cpp/.h         #   駒の描画（倍率と影）
│   └── boardsurfacepainter.cpp/.h  #   盤・駒台の木肌と縁の描画
│
├── widgets/                        # カスタムQtウィジェット
│   ├── recordpane*.cpp/.h          #   棋譜表示ペイン
│   ├── engineanalysistab*.cpp/.h   #   エンジン解析タブ
│   ├── branchtreemanager*.cpp/.h   #   分岐ツリーの表示
│   ├── considerationtabmanager*.cpp/.h # 検討タブ
│   ├── evaluationchartwidget*.cpp/.h #  評価値グラフウィジェット
│   ├── kifudisplay.cpp/.h          #   棋譜表示ウィジェット
│   ├── kifubranchdisplay.cpp/.h    #   分岐表示ウィジェット
│   ├── gameinfopanecontroller*.cpp/.h # 対局情報ペイン制御
│   ├── engineinfowidget.cpp/.h     #   エンジン情報ウィジェット
│   ├── usilogpanel.cpp/.h          #   USI通信ログパネル（CSA は csalogpanel）
│   ├── commenteditorpanel.cpp/.h   #   棋譜コメント編集パネル
│   ├── kifuanalysisresultsdisplay.cpp/.h # 棋譜解析結果表示
│   ├── menubuttonwidget.cpp/.h     #   メニューボタン
│   ├── collapsiblegroupbox.cpp/.h  #   折りたたみグループボックス
│   ├── elidelabel.cpp/.h           #   省略ラベル
│   ├── flowlayout.cpp/.h           #   フローレイアウト
│   ├── globaltooltip.cpp/.h        #   グローバルツールチップ
│   ├── apptooltipfilter.cpp/.h     #   ツールチップフィルタ
│   ├── longlongspinbox.cpp/.h      #   long long対応スピンボックス
│   └── numericrightaligncommadelegate.h # 数値右寄せデリゲート
│
├── dialogs/                        # ダイアログ実装
│   ├── startgamedialog*.cpp/.h/.ui #   対局開始ダイアログ
│   ├── kifuanalysisdialog.cpp/.h/.ui # 棋譜解析ダイアログ
│   ├── csagamedialog*.cpp/.h/.ui   #   CSA対局接続ダイアログ
│   ├── engineregistrationdialog.cpp/.h/.ui # エンジン登録ダイアログ
│   ├── changeenginesettingsdialog.cpp/.h/.ui # エンジン設定変更
│   ├── promotedialog.cpp/.h/.ui    #   成/不成選択ダイアログ
│   ├── versiondialog.cpp/.h/.ui    #   バージョン情報ダイアログ
│   ├── tsumeshogisearchdialog.cpp/.h/.ui # 詰み探索ダイアログ
│   ├── tsumeshogigeneratordialog*.cpp/.h # 詰将棋局面生成ダイアログ
│   ├── tsumecollectiondialog*.cpp/.h #  詰将棋問題集（一覧から選んで解く）
│   ├── tsumeplaydialog*.cpp/.h     #   詰将棋を解く画面
│   ├── boardcolordialog*.cpp/.h    #   外観（駒・盤・背景・駒台・対局者情報）
│   ├── fontsettingsdialog.cpp/.h   #   GUI 共通の書体
│   ├── piecesoundsettingsdialog.cpp/.h # 駒音の設定
│   ├── jishogiscoredialog.cpp/.h   #   持将棋の点数
│   ├── sfencollectiondialog*.cpp/.h #  SFEN局面集ビューア
│   ├── pvboarddialog.cpp/.h        #   読み筋盤面ダイアログ
│   ├── kifupastedialog.cpp/.h      #   棋譜貼り付けダイアログ
│   ├── csawaitingdialog.cpp/.h     #   CSA待機ダイアログ
│   ├── menuwindow.cpp/.h           #   メニューウィンドウ
│   ├── josekiwindow*.cpp/.h        #   定跡ウィンドウ
│   ├── josekimergedialog.cpp/.h    #   定跡結合ダイアログ
│   └── josekimovedialog.cpp/.h     #   定跡移動ダイアログ
│
├── models/                         # Qtモデル
│   ├── abstractlistmodel.h         #   抽象リストモデル基底
│   ├── kifurecordlistmodel.cpp/.h  #   棋譜レコードリストモデル
│   └── kifubranchlistmodel.cpp/.h  #   分岐リストモデル
│
├── services/                       # クロスカッティングサービス
│   ├── setting.h                   #   設定1項目の宣言（Setting<T>: キーと既定値）
│   ├── settingscommon.cpp/.h       #   設定ファイルのパスと共有QSettings
│   ├── appsettings.h, gamesettings.h, ... # ドメイン別の設定の宣言の表
│   ├── settingsservice.h           #   ドメイン別ヘッダーをまとめて取り込むだけのヘッダー
│   ├── apppaths.h                  #   設定・データ・キャッシュの置き場所
│   ├── applicationtranslations.cpp/.h # 起動時の言語と翻訳の読み込み
│   ├── boardappearance.cpp/.h      #   全盤面に共通する配色
│   ├── pieceimageprovider.cpp/.h   #   全盤面で共有する駒画像
│   ├── piecesoundplayer.cpp/.h     #   駒音の再生
│   ├── enginelistsettings.cpp/.h   #   登録エンジン一覧の読み込み
│   ├── uinotificationservice.cpp/.h #  エラー通知ダイアログ
│   ├── playernameservice.cpp/.h    #   対局者名解決サービス
│   └── timecontrolutil.cpp/.h      #   時間制御ユーティリティ
│
├── common/                         # 共有ユーティリティ
│   ├── kifupresentation.cpp/.h     #   表示用の指し手・座標の表記（言語と表記の設定に合わせる）
│   ├── applicationfonts.cpp/.h     #   表示言語に応じた書体
│   ├── dialogutils.cpp/.h          #   ダイアログの大きさの保存・復元など
│   ├── fontsizehelper.cpp/.h       #   A-/A+ のフォントサイズ調整
│   ├── logcategories.cpp/.h        #   ログカテゴリ
│   ├── boardconstants.h            #   盤の座標・サイズの定数
│   ├── errorbus.cpp/.h             #   エラー通知バス
│   ├── jishogicalculator.cpp/.h    #   持将棋点数計算
│   └── tsumepositionutil.cpp/.h    #   詰将棋局面ユーティリティ
│
├── automation/                     # 自動化 API（--automation）と、CLI と共有する GUI 非依存のサービス
│   ├── automationserver.cpp/.h     #   QLocalServer で改行区切りの JSON-RPC 2.0 を待ち受け
│   ├── automationdispatcher.cpp/.h #   JSON-RPC の解析とメソッド表（GUI 非依存）
│   ├── automationcommands*.cpp/.h  #   app・position・kifu・ui などのメソッド
│   ├── automationactionpolicy.cpp/.h # 実行してよい QAction の許可リスト
│   ├── kifuconversionservice.cpp/.h #  棋譜の形式変換
│   ├── sfenvalidationservice.cpp/.h #  SFEN の妥当性検査
│   ├── boardimagerenderer.cpp/.h   #   SFEN から盤面画像を描画
│   ├── engineanalysisrunner.cpp/.h #   登録エンジンで1局面を解析
│   ├── matesearchrunner.cpp/.h     #   登録エンジンで詰み探索
│   └── tsumeverificationrunner.cpp/.h # 余詰の検査
│
└── cli/                            # shogiboardq-cli（GUI を起動しない JSON 出力コマンド。mcp/ が使う）
    ├── main.cpp                    #   エントリーポイント
    ├── clicommands*.cpp/.h         #   各コマンド
    ├── cliargs.cpp/.h              #   引数の解釈（局面・エンジン・数値）
    └── clioutput.cpp/.h            #   JSON 出力
```

### 1.6 コード規模

`src/` 配下の `.cpp` と `.h` の数と行数（2026-10-14 時点）。

| ディレクトリ | ファイル数 | 行数 | 概要 |
|-------------|-----------|------|------|
| `app/` | 68 | 9,067 | メインウィンドウ、エントリーポイント、レジストリ |
| `core/` | 32 | 5,119 | 純粋ゲームロジック |
| `game/` | 61 | 8,862 | 対局管理 |
| `kifu/` | 54 | 7,772 | 棋譜管理（formatsを除く） |
| `kifu/formats/` | 51 | 8,722 | フォーマット変換器 |
| `engine/` | 32 | 5,436 | USIエンジン連携 |
| `analysis/` | 40 | 5,951 | 解析機能 |
| `network/` | 14 | 4,077 | CSA通信対局 |
| `navigation/` | 5 | 1,040 | ナビゲーション |
| `board/` | 8 | 1,285 | 盤面操作 |
| `ui/`（直下） | 1 | 84 | 局面の差分（`sfendiffutils.h`） |
| `ui/presenters/` | 13 | 1,897 | MVPプレゼンター |
| `ui/controllers/` | 30 | 4,413 | UIコントローラ |
| `ui/wiring/` | 37 | 5,197 | シグナル/スロット接続 |
| `ui/coordinators/` | 22 | 3,762 | UI調整 |
| `views/` | 19 | 4,220 | 将棋盤描画 |
| `widgets/` | 82 | 12,644 | カスタムウィジェット |
| `dialogs/` | 80 | 16,308 | ダイアログ |
| `models/` | 5 | 788 | Qtモデル |
| `services/` | 50 | 3,881 | 横断サービス・設定の宣言 |
| `common/` | 31 | 2,440 | 共有ユーティリティ |
| `automation/` | 34 | 3,979 | 自動化 API と、CLI と共有するサービス |
| `cli/` | 11 | 1,206 | `shogiboardq-cli` |
| **合計** | **780** | **約118,200** | |

### 1.7 リソース構成

```
resources/
├── shogiboardq.qrc             # Qtリソース定義（画像・翻訳のバンドル）
├── icons/
│   ├── shogiboardq.icns        # macOS用アイコン
│   ├── shogiboardq.ico         # Windows用アイコン
│   └── shogiboardq.png         # 汎用アイコン
├── images/
│   ├── actions/                # ツールバー・メニューのアクションアイコン
│   └── pieces/                 # 駒画像（先手/後手、各駒種）
├── platform/
│   └── app.rc                  # Windows用リソーススクリプト
└── translations/
    ├── ShogiBoardQ_ja_JP.ts    # 日本語翻訳ソース
    └── ShogiBoardQ_en.ts       # 英語翻訳ソース
```

### 1.8 テスト構成

```
tests/
├── CMakeLists.txt                  # テスト用ビルド構成（add_shogi_test で1本ずつ登録する）
├── tst_*.cpp                       # 単体テスト。多くは shogiboardq_static にリンクする
├── test_stubs_*.cpp                # 一部の部品だけを組み立てるテスト用のスタブ
├── kifu_test_helper.h ほか          # テスト共通の補助
├── mock_*.cpp                      # テスト用の偽エンジン（USI・詰み探索）
├── test_*.py                       # scripts/ の Python スクリプトのテスト
├── fixtures/                       # テスト用の棋譜（KIF・KI2・CSA・JKF・USEN・USI）と局面集
├── tsumeshogi_stubs/               # 詰将棋の生成器のテスト用のスタブ
└── gui/                            # GUI シナリオ・GUI フロー・エンジン終了のハーネス
```

2026-10-14 時点で、`tst_*.cpp` は127本、`ctest` に登録したテストは135件（MCP サーバーの Python テスト `tst_mcp_python` を含む）。実行方法は次のとおり。

- 単体テスト: `ctest --test-dir build -j8 -E tst_mcp_python`
- MCP サーバーのテスト: `python3 -m pytest -q mcp/tests`（`mcp/tests/`）
- GUI のテスト: `python3 tests/gui/prepare.py` でビルドしてから、`xvfb-run` の上で `tests/gui/run.py` などを実行する（`tests/gui/README.md`）

### 1.9 開発環境

- **IDE**: Qt Creator を使用
- **ビルドジェネレータ**: Ninja 推奨（Makeより高速）
- **compile_commands.json**: `CMAKE_EXPORT_COMPILE_COMMANDS=ON` で自動生成（clang-tidy、IDE連携に利用）

<!-- chapter-1-end -->

---

<!-- chapter-2-start -->
## 第2章 将棋のドメイン知識

本章では、ShogiBoardQの開発に必要な将棋のデータ表現を解説する。将棋のルール自体の説明は省略し、コード上の座標系・文字列フォーマット・駒表現に焦点を当てる。

### 2.1 盤面座標系

将棋盤は9×9の81マスで構成される。座標は**筋**（file, 縦列）と**段**（rank, 横行）で表現される。

#### 筋と段の番号

```
  9   8   7   6   5   4   3   2   1    ← 筋（file）: 右から左へ 1〜9
┌───┬───┬───┬───┬───┬───┬───┬───┬───┐
│   │   │   │   │   │   │   │   │   │ 一段（rank 1）
├───┼───┼───┼───┼───┼───┼───┼───┼───┤
│   │   │   │   │   │   │   │   │   │ 二段（rank 2）
├───┼───┼───┼───┼───┼───┼───┼───┼───┤
│   │   │   │   │   │   │   │   │   │ 三段（rank 3）
├───┼───┼───┼───┼───┼───┼───┼───┼───┤
│   │   │   │   │   │   │   │   │   │ ...
├───┼───┼───┼───┼───┼───┼───┼───┼───┤
│   │   │   │   │   │   │   │   │   │ 九段（rank 9）
└───┴───┴───┴───┴───┴───┴───┴───┴───┘
```

- **筋（file）**: 1〜9。盤を先手側から見て**右から左**へ番号が増える
- **段（rank）**: 1〜9。盤を先手側から見て**上から下**へ番号が増える
- 先手の陣地は7〜9段目、後手の陣地は1〜3段目

#### 座標表記の変換

日本語の棋譜表記では筋を全角数字、段を漢数字で表す。`src/core/shoginotation.h`（`ShogiNotation` 名前空間、ヘッダーだけ）に変換関数がある。

```cpp
// src/core/shoginotation.h

// 筋番号（1〜9）→ 全角数字（"１"〜"９"。範囲外は空文字列）
inline QString fileZenkaku(int file)
{
    if (file < 1 || file > 9) return {};
    return QString(QChar(u'０' + file));
}

// 段番号（1〜9）→ 漢数字（"一"〜"九"。範囲外は空文字列）
inline QString rankKanji(int rank)
{
    static const char16_t kanji[] = {u'一', u'二', u'三', u'四', u'五', u'六', u'七', u'八', u'九'};
    if (rank < 1 || rank > 9) return {};
    return QString(QChar(kanji[rank - 1]));
}
```

例: 7筋六段 → `"７六"` (7筋=`"７"`, 6段=`"六"`)

#### 内部インデックスとの対応

コード内部では、`ShogiMove` の座標値は **0-indexed**（0〜8）で管理される。

```cpp
// src/core/shogimove.h
struct ShogiMove {
    QPoint fromSquare{0, 0};            // 移動元（盤上:0-8、駒台:9=先手,10=後手）
    QPoint toSquare{0, 0};              // 移動先（0-8）
    Piece movingPiece = Piece::None;    // 動かした駒
    Piece capturedPiece = Piece::None;  // 取った駒（なければ Piece::None）
    bool isPromotion = false;           // 成りフラグ
};
```

| 表記系 | 筋の範囲 | 段の範囲 | 用途 |
|--------|---------|---------|------|
| 外部（SFEN/USI/棋譜） | 1〜9 | 1〜9（USIでは a〜i） | ファイルI/O、エンジン通信 |
| 内部（`ShogiMove`） | 0〜8 | 0〜8 | コード内の座標計算 |
| 駒台 | 9=先手, 10=後手 | 駒種別に割当 | 持ち駒管理 |

**変換式**: 内部値 + 1 = 外部値

```cpp
// src/core/shogimove.cpp — ShogiMove::toUsiMove() より
move.to = BoardSquare{toSquare.x() + 1, toSquare.y() + 1};  // 0-indexed → 1-indexed
// 段の 'a'〜'i' への変換は UsiMove::toString()（src/core/usimove.h）が行う
```

`ShogiBoard` の盤面データは1次元配列 `QList<Piece> m_boardData`（81要素）で保持される。

```cpp
// src/core/shogiboard.cpp — インデックス計算
int index = (rank - 1) * files() + (file - 1);  // 1-indexed の file, rank から算出
```

| マス | file（筋） | rank（段） | 配列インデックス |
|------|-----------|-----------|---------------|
| 9一 | 9 | 1 | (1-1)*9 + (9-1) = 8 |
| 1一 | 1 | 1 | (1-1)*9 + (1-1) = 0 |
| 5五 | 5 | 5 | (5-1)*9 + (5-1) = 40 |
| 1九 | 1 | 9 | (9-1)*9 + (1-1) = 72 |

> **注意**: SFEN文字列の盤面パートは9一（左上）から1一（右上）へ左から右に読む。つまり筋が**9→1**の降順で並ぶ。`trySetSfen()` が盤の欄を読むとき（`shogiboard_sfen.cpp` の `readBoardField`）も、各段を9筋から1筋へ読む。

### 2.2 駒の表現方法

#### SFEN駒文字

駒は1文字のアルファベットで表現される。**大文字=先手（Black）**、**小文字=後手（White）**。

| 駒名 | 日本語 | 先手 | 後手 | 最大枚数 |
|------|--------|------|------|---------|
| 歩兵 | Pawn | `P` | `p` | 各9枚 |
| 香車 | Lance | `L` | `l` | 各2枚 |
| 桂馬 | Knight | `N` | `n` | 各2枚 |
| 銀将 | Silver | `S` | `s` | 各2枚 |
| 金将 | Gold | `G` | `g` | 各2枚 |
| 角行 | Bishop | `B` | `b` | 各1枚 |
| 飛車 | Rook | `R` | `r` | 各1枚 |
| 玉将 | King | `K` | `k` | 各1枚 |

#### 成駒の表現

SFEN標準では成駒を `+` 接頭辞で表す。コード内部では、駒を列挙型 `Piece`（`src/core/shogitypes.h`）で持ち、成駒もそれぞれ1つの値にする。
`Piece` の値は1文字（`char`）で、成駒には SFEN にない文字を当てている。

| 成駒名 | 日本語 | SFEN標準 | `Piece`（先手） | `Piece`（後手） | 値の文字 |
|--------|--------|---------|----------------|----------------|---------|
| と金 | Promoted Pawn | `+P` / `+p` | `BlackPromotedPawn` | `WhitePromotedPawn` | `Q` / `q` |
| 成香 | Promoted Lance | `+L` / `+l` | `BlackPromotedLance` | `WhitePromotedLance` | `M` / `m` |
| 成桂 | Promoted Knight | `+N` / `+n` | `BlackPromotedKnight` | `WhitePromotedKnight` | `O` / `o` |
| 成銀 | Promoted Silver | `+S` / `+s` | `BlackPromotedSilver` | `WhitePromotedSilver` | `T` / `t` |
| 馬 | Horse (Promoted Bishop) | `+B` / `+b` | `BlackHorse` | `WhiteHorse` | `C` / `c` |
| 龍 | Dragon (Promoted Rook) | `+R` / `+r` | `BlackDragon` | `WhiteDragon` | `U` / `u` |

> **金将と玉将には成駒がない。**

値の文字は内部だけのもので、SFEN では使わない。`pieceToChar()`・`charToPiece()` が `Piece` と文字を変換し、
`promote()`・`demote()` が成駒と元の駒を、`toBlack()`・`toWhite()` が先後を変換する。

SFEN から `Piece` への変換は、`ShogiBoard::trySetSfen()` が盤の欄を読むときに行う（`shogiboard_sfen.cpp` の `boardPieceAt`）。
`+` の次の駒を `promote()` で成駒にする。成れない駒（金・玉）や駒でない文字に `+` が付いているとき、
内部の文字（`Q`・`c` など）が盤の欄にあるときは、不正な SFEN として読まない。

逆変換（`Piece` → SFEN文字列）は `convertPieceToSfen()` が担う。

```cpp
// src/core/shogiboard_sfen.cpp
QString ShogiBoard::convertPieceToSfen(const Piece piece) const
{
    static const QMap<Piece, QString> sfenMap = {
        {Piece::BlackPromotedPawn, "+P"}, {Piece::BlackPromotedLance, "+L"},
        {Piece::BlackPromotedKnight, "+N"}, {Piece::BlackPromotedSilver, "+S"},
        {Piece::BlackHorse, "+B"}, {Piece::BlackDragon, "+R"},
        {Piece::WhitePromotedPawn, "+p"}, {Piece::WhitePromotedLance, "+l"},
        {Piece::WhitePromotedKnight, "+n"}, {Piece::WhitePromotedSilver, "+s"},
        {Piece::WhiteHorse, "+b"}, {Piece::WhiteDragon, "+r"}
    };

    return sfenMap.contains(piece) ? sfenMap.value(piece) : QString(pieceToChar(piece));
}
```

#### 駒台（持ち駒）

駒台は `QMap<Piece, int>` で管理される。キーは駒、値は枚数。局面編集で玉も駒台に置けるため、玉を含む先後16種を常に持つ。

16種を0枚で持つ表は、すべての盤で共有する。`QMap` は暗黙の共有なので、枚数を書き換えたときに初めてその盤の表が作られる。
盤を作るたびに16個を挿入しないための工夫で、一時的な盤を作る処理（`SfenDiffUtils::diffBoards()` など）を軽くしている。

```cpp
// src/core/shogiboard.cpp — initStand()
void ShogiBoard::initStand()
{
    m_pieceStand = emptyStand();  // 16種を0枚で持つ、共有の表
}
```

駒を取ったとき、成駒は元の駒に変換して相手の駒台に加える。

```cpp
// src/core/shogiboard.cpp — convertPieceChar()
// 成駒は相手の生駒に変換し、それ以外は先後を反転する
Piece ShogiBoard::convertPieceChar(const Piece c) const
{
    if (isPromoted(c)) {
        Piece base = demote(c);
        return isBlackPiece(base) ? toWhite(base) : toBlack(base);
    }
    return isBlackPiece(c) ? toWhite(c) : toBlack(c);
}
```

#### 合法手判定の中の駒種

合法手判定（`EngineMoveValidator`、5.4 節）は、`ShogiBoard` の盤の配列（`boardData()`）と駒台（`pieceStand()`）を受け取り、内部の局面（`fmv::EnginePosition`）に変換して判定する。内部では先後を `fmv::Color`、駒種を先後の区別のない `fmv::PieceType`、持駒の種類を `fmv::HandType` で表す。

```cpp
// src/core/fmvtypes.h
enum class Color : std::uint8_t { Black = 0, White = 1, ColorNb = 2 };

enum class PieceType : std::uint8_t {
    Pawn = 0, Lance, Knight, Silver, Gold, Bishop, Rook, King,
    ProPawn, ProLance, ProKnight, ProSilver, Horse, Dragon,
    PieceTypeNb    // 駒種の数 = 14
};

enum class HandType : std::uint8_t {
    Pawn = 0, Lance, Knight, Silver, Gold, Bishop, Rook, HandTypeNb
};
```

- **盤**: 各マスの `Piece` の値の文字（`'P'`、`'q'` など。空きは `' '`）をそのまま `EnginePosition::board` に写し、先後と駒種ごとのビットボード（`pieceOcc[2][14]`）と玉の位置を作り直す。マスの並びは `ShogiBoard` と同じ（添字 = (段-1)×9 + (筋-1)）。
- **持駒**: 先後と `HandType` ごとの枚数（`hand[2][7]`）にする。駒台の玉は数えない。

### 2.3 SFEN文字列フォーマット

SFEN（Shogi Forsyth-Edwards Notation）は局面を1行の文字列で表現するフォーマットである。チェスのFEN記法を将棋向けに拡張したもの。

#### 構造

SFEN文字列は**スペース区切り**の4つのフィールドからなる。

```
<盤面> <手番> <持ち駒> <手数>
```

**例**: 初期配置

```
lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1
```

| フィールド | 値 | 説明 |
|-----------|-----|------|
| 盤面 | `lnsgkgsnl/1r5b1/...` | 駒配置（後述） |
| 手番 | `b` | `b`=先手（Black）、`w`=後手（White） |
| 持ち駒 | `-` | `-`=なし、それ以外は駒文字＋枚数 |
| 手数 | `1` | 次の手の番号（1以上の正整数） |

```cpp
// src/core/sfenutils.h — "startpos" を初期配置SFENに展開
inline QString normalizeStart(const QString& startPositionStr)
{
    if (startPositionStr == QStringLiteral("startpos")) {
        return QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    }
    return startPositionStr;
}
```

#### 盤面フィールド

盤面は `/` で9段に区切られ、各段は9一（左上、筋9）から1一（右上、筋1）へ**左から右**に読む（筋は9→1の順）。

- アルファベット文字: 駒（大文字=先手、小文字=後手）
- 数字（1〜9）: 連続する空マスの数
- `+`: 直後の駒文字が成駒であることを示す（`+P` = と金）。成れる駒（歩・香・桂・銀・角・飛）にだけ付けられる

```
lnsgkgsnl    ← 1段目: 後手の 香桂銀金玉金銀桂香
/1r5b1/      ← 2段目: 空1 飛 空5 角 空1
ppppppppp    ← 3段目: 後手の歩×9
/9/9/9/      ← 4〜6段目: 全て空
PPPPPPPPP    ← 7段目: 先手の歩×9
/1B5R1/      ← 8段目: 空1 角 空5 飛 空1
LNSGKGSNL    ← 9段目: 先手の 香桂銀金玉金銀桂香
```

SFEN文字列を4つの欄に分けて確かめるのは、`shogiboard_sfen.cpp` の `splitSfenFields()` である
（`ShogiBoard::parseSfen()` と `ShogiBoard::trySetSfen()` が使う）。欄が4つでない、手番が `b`・`w` でない、
手数が1以上の整数でない SFEN は読まない。手数は確かめるだけで、`ShogiBoard` には残さない
（必要なら `parseSfen()` の結果の `SfenComponents::moveNumber` で読める）。

```cpp
// src/core/shogiboard_sfen.cpp
struct SfenFields
{
    QStringView board;          // 盤の欄（元の文字列を指す）
    QStringView stand;          // 持駒の欄
    Turn turn = Turn::Black;    // 手番
    int moveNumber = 0;         // 手数
};

std::optional<SfenFields> splitSfenFields(const QString& sfenStr);
```

#### 持ち駒フィールド

持ち駒がない場合は `-`。ある場合は枚数＋駒文字を連結する（1枚の場合は数字を省略）。

| 持ち駒文字列 | 意味 |
|-------------|------|
| `-` | 持ち駒なし |
| `P` | 先手の歩1枚 |
| `2P` | 先手の歩2枚 |
| `RB2S3P2p` | 先手: 飛1 角1 銀2 歩3、後手: 歩2 |
| `10P` | 先手の歩10枚（2桁にも対応） |

出力は先手の駒をすべて書いてから後手の駒を書く。どちらも `R B G S N L P`（飛・角・金・銀・桂・香・歩）の順。

```cpp
// src/core/shogiboard_sfen.cpp — convertStandToSfen()
static const QList<Piece> keys = {
    Piece::BlackRook, Piece::BlackBishop, Piece::BlackGold,
    Piece::BlackSilver, Piece::BlackKnight, Piece::BlackLance, Piece::BlackPawn
};
for (const bool white : {false, true}) {
    for (const auto& blackKey : keys) {
        const Piece key = white ? toWhite(blackKey) : blackKey;
        const int value = m_pieceStand.value(key, 0);
        if (value > 0) {
            handPiece += (value > 1 ? QString::number(value) : QString()) + convertPieceToSfen(key);
        }
    }
}
return handPiece.isEmpty() ? "-" : handPiece;
```

### 2.4 USI指し手表記

USI（Universal Shogi Interface）プロトコルで使用される指し手表記。座標は **筋=1〜9**（数字）、**段=a〜i**（アルファベット）で表す。

#### 座標系

| 段（rank） | 1段 | 2段 | 3段 | 4段 | 5段 | 6段 | 7段 | 8段 | 9段 |
|-----------|-----|-----|-----|-----|-----|-----|-----|-----|-----|
| USI表記 | a | b | c | d | e | f | g | h | i |

#### 指し手の3パターン

| 種類 | 書式 | 例 | 意味 |
|------|------|-----|------|
| 通常移動 | `<from_file><from_rank><to_file><to_rank>` | `7g7f` | 7七(7g)の駒を7六(7f)へ |
| 成り | `<from><to>+` | `8h2b+` | 8八(8h)から2二(2b)へ移動し成る |
| 駒打ち | `<PIECE>*<to_file><to_rank>` | `P*5e` | 歩を5五(5e)に打つ |

- 駒打ちの駒文字は常に**大文字**（先手・後手問わず）
- 駒打ちでは `fromSquare.x()` が `9`（先手駒台）または `10`（後手駒台）

`ShogiMove` から USI 文字列への変換は、`ShogiMove::toUsiMove()` で `UsiMove`（USI と1対1の値）にしてから `UsiMove::toString()` で文字列にする。USI の読み書きの実装は `UsiMove::parse()` と `toString()` だけである。

```cpp
// src/core/shogimove.cpp — ShogiMove::toUsiMove()
UsiMove ShogiMove::toUsiMove() const
{
    UsiMove move;
    move.to = BoardSquare{toSquare.x() + 1, toSquare.y() + 1};
    if (fromSquare.x() >= 9) {                       // 駒台（9=先手、10=後手）からの駒打ち
        move.drop = toBlack(demote(movingPiece));
    } else {
        move.from = BoardSquare{fromSquare.x() + 1, fromSquare.y() + 1};
        move.promote = isPromotion;
    }
    return move;
}

// src/core/usimove.h — UsiMove::toString()
QString UsiMove::toString() const
{
    if (!isValid()) return {};
    if (isDrop()) return pieceToChar(drop) + QStringLiteral("*") + UsiNotation::squareText(to);
    QString text = UsiNotation::squareText(from) + UsiNotation::squareText(to);
    if (promote) text += QLatin1Char('+');
    return text;
}
```

#### 変換の具体例

| 指し手 | 日本語表記 | USI表記 | ShogiMove の値 |
|--------|-----------|---------|---------------|
| ７六歩 | ▲７六歩 | `7g7f` | from=(6,6) to=(6,5) piece='P' |
| ８八角成 | ▲８八角成 | `2b8h+` | from=(1,1) to=(7,7) piece='b' promo=true |
| 歩打ち５五 | ▲５五歩打 | `P*5e` | from=(9,0) to=(4,4) piece='P' |

> **注意**: `ShogiMove` の座標は0-indexedなので、USI出力時に+1する。

### 2.5 棋譜フォーマットの比較

ShogiBoardQは6種類の棋譜フォーマットを読み込める。いずれも内部的にSFEN＋USI指し手列に変換される。変換器は `src/kifu/formats/` に配置されている。

#### 各フォーマットの概要

| フォーマット | 正式名称 | 種別 | 主な用途 |
|-------------|---------|------|---------|
| **KIF** | KIF（棋譜ファイル） | テキスト（日本語） | プロ棋譜、一般的な棋譜保存 |
| **KI2** | KI2（棋譜ファイル簡略版） | テキスト（日本語） | 書籍・雑誌向け簡略表記 |
| **CSA** | CSA（コンピュータ将棋協会） | テキスト（機械向け） | コンピュータ将棋大会 |
| **JKF** | JSON Kifu Format | JSON | Webアプリ、現代的なツール |
| **USEN** | URL Safe sfen-Extended Notation | Base36エンコード | Shogi Playground等のURL共有 |
| **USI** | Universal Shogi Interface | テキスト（プロトコル） | エンジン通信、解析記録 |

#### 指し手表記の比較

同じ指し手「先手が7七の歩を7六に進める」を各フォーマットで表すと:

| フォーマット | 表記例 | 特徴 |
|-------------|--------|------|
| KIF | `▲７六歩(77)` | 移動元を括弧内に記載 |
| KI2 | `▲７六歩` | 移動元なし（盤面から推定が必要） |
| CSA | `+7776FU` | `+`=先手、座標4桁、駒種2文字 |
| JKF | `{"to":{"x":7,"y":6},"piece":"FU"}` | JSON形式 |
| USEN | 3文字のBase36コード | `code = (from*81+to)*2 + promo` |
| USI | `7g7f` | 座標4文字（筋=数字、段=a-i） |

#### 詳細比較表

| 特性 | KIF | KI2 | CSA | JKF | USEN | USI |
|------|-----|-----|-----|-----|------|-----|
| **可読性** | 高（日本語） | 高（日本語） | 中 | 中（JSON） | 低 | 中 |
| **移動元情報** | あり | **なし** | あり | あり | あり | あり |
| **対局情報** | あり | あり | あり | あり | **なし** | **なし** |
| **時間情報** | あり | あり | あり | あり | **なし** | **なし** |
| **コメント** | あり | あり | あり | あり | **なし** | **なし** |
| **分岐** | あり | あり | なし | あり | あり | あり |
| **文字エンコーディング** | Shift-JIS/UTF-8 | Shift-JIS/UTF-8 | Shift-JIS/UTF-8 | UTF-8 | ASCII | ASCII |
| **パース難易度** | 中 | **高** | 中 | 中 | **高** | 低 |
| **拡張子** | `.kif` | `.ki2` | `.csa` | `.jkf` | (URL) | (なし) |
| **変換器** | `kiftosfenconverter` | `ki2tosfenconverter` | `csatosfenconverter` | `jkftosfenconverter` | `usentosfenconverter` | `usitosfenconverter` |

#### 各フォーマットの特記事項

**KIF**: 最も広く使われる棋譜フォーマット。移動先は全角数字＋漢数字（`７六`）、移動元は括弧内に半角数字（`(77)`）で記す。同じマスへの移動は「同」と表記する。コメントは `*` で始まる行。ヘッダにはKIFフォーマット固有の情報（対局日、棋戦名、対局者名等）を含む。

**KI2**: KIFの簡略版。移動元座標が省略されるため、パーサは現在の盤面を追跡し合法手から移動元を決める（`Ki2MoveNotation::resolve()`）。複数の駒が同じマスに移動できる場合は「右」「左」「上」「引」「寄」「直」等の修飾語で区別する。

**CSA**: コンピュータ将棋大会の標準フォーマット。駒は2文字コード（`FU`=歩, `KY`=香, `KE`=桂, `GI`=銀, `KI`=金, `KA`=角, `HI`=飛, `OU`=玉, `TO`=と, `NY`=成香, `NK`=成桂, `NG`=成銀, `UM`=馬, `RY`=龍）。初期配置は `PI` 行または `P1`〜`P9` 行で記述。終了コードは `%TORYO`（投了）、`%SENNICHITE`（千日手）等。ネットワーク対局（CSAプロトコル）でも使用される。

**JKF**: JSON形式の現代的フォーマット。駒台（handicap）プリセット名 `HIRATE`（平手）、`KY`（香落ち）等をサポート。分岐もJSON配列で自然に表現できる。仕様は [json-kifu-format](https://github.com/na2hiro/json-kifu-format) を参照。

**USEN**: URLに埋め込み可能な超コンパクトフォーマット。指し手をBase36で3文字にエンコードする。計算式: `code = (from_sq * 81 + to_sq) * 2 + (promotion ? 1 : 0)`。マス番号は `(rank-1)*9 + (file-1)` で0〜80。駒打ちは `from_sq = 81 + 駒種番号`。バージョンプレフィックス `~0.` または `~.` で始まる。

**USI**: エンジン通信プロトコルの指し手部分をそのまま棋譜として保存する形式。`position startpos moves 2g2f 3c3d 7g7f` または `position sfen <SFEN> moves ...` の形式。終了コードは `resign`、`win`、`draw` 等。

#### 変換器の共通API

全変換器は以下の共通メソッドを持つ:

| メソッド | 説明 |
|---------|------|
| `detectInitialSfenFromFile()` | 初期局面のSFEN文字列を取得 |
| `convertFile()` | USI指し手列を抽出 |
| `extractMovesWithTimes()` | 時間情報付き指し手を抽出 |
| `parseWithVariations()` | 分岐を含む完全な棋譜ツリーを取得 |
| `extractGameInfo()` | 対局情報（対局者名、日時等）を抽出 |

### 2.6 座標変換の早見表

開発中に頻出する座標変換をまとめる。

| 変換 | 方法 | コード例 |
|------|------|---------|
| 内部(0-8) → USI筋 | +1 | `file + 1` → `1`〜`9` |
| 内部(0-8) → USI段 | 'a'+値 | `QChar('a' + rank)` → `a`〜`i` |
| 内部(0-8) → 日本語筋 | `ShogiNotation::fileZenkaku(file+1)` | → `"１"`〜`"９"` |
| 内部(0-8) → 日本語段 | `ShogiNotation::rankKanji(rank+1)` | → `"一"`〜`"九"` |
| 全角数字 → 数値 | `ShogiNotation::fileFromZenkaku(ch)` | `"１"` → `1` |
| 漢数字 → 数値 | `ShogiNotation::rankFromKanji(ch)` | `"一"` → `1` |
| `ShogiMove` → USI | `move.toUsiMove().toString()` | → `"7g7f"` |
| file,rank → 配列index | `(rank-1)*9 + (file-1)` | 1-indexed入力 |

<!-- chapter-2-end -->

---

<!-- chapter-3-start -->
## 第3章 アーキテクチャ全体図

本章では、ShogiBoardQのレイヤー構造、各層の責務、MainWindowの位置づけ、PlayModeによる動作分岐、およびデータフローの全体像を解説する。

### 3.1 レイヤー構造

ShogiBoardQは以下の5つのレイヤーで構成される。下位レイヤーは上位レイヤーに依存しない。

```mermaid
graph TB
    subgraph "プレゼンテーション層"
        MW["MainWindow<br/>(ハブ/ファサード)"]
        UI["ui/<br/>presenters / controllers<br/>wiring / coordinators"]
        VWD["views/ widgets/ dialogs/"]
    end

    subgraph "ドメインサービス層"
        GAME["game/<br/>対局管理"]
        KIFU["kifu/<br/>棋譜管理"]
        ANALYSIS["analysis/<br/>解析機能"]
        ENGINE["engine/<br/>USIエンジン連携"]
        NETWORK["network/<br/>CSA通信対局"]
        NAV["navigation/<br/>ナビゲーション"]
        BOARD["board/<br/>盤面操作"]
    end

    subgraph "横断サービス層"
        SVC["services/<br/>設定・時間管理"]
        COMMON["common/<br/>共有ユーティリティ"]
        MODELS["models/<br/>Qtモデル"]
    end

    subgraph "コア層"
        CORE["core/<br/>純粋なゲームロジック"]
    end

    MW --> UI
    MW --> VWD
    MW --> GAME
    MW --> KIFU
    MW --> ANALYSIS
    MW --> ENGINE
    MW --> NETWORK

    UI --> VWD
    UI --> GAME
    UI --> KIFU
    UI --> ANALYSIS
    UI --> ENGINE

    GAME --> CORE
    KIFU --> CORE
    ANALYSIS --> ENGINE
    ANALYSIS --> CORE
    BOARD --> CORE
    NAV --> KIFU
    NETWORK --> ENGINE
    NETWORK --> CORE

    GAME --> SVC
    KIFU --> SVC
    ENGINE --> SVC
    UI --> MODELS

    GAME --> COMMON
    ANALYSIS --> COMMON
```

#### 各レイヤーの責務

| レイヤー | ディレクトリ | 責務 |
|---------|------------|------|
| **コア層** | `core/` | 将棋のルール・盤面・駒・指し手を純粋なC++で表現する（Qt GUI非依存） |
| **ドメインサービス層** | `game/`, `kifu/`, `analysis/`, `engine/`, `network/`, `navigation/`, `board/` | 対局進行・棋譜管理・解析・エンジン通信などのビジネスロジックを提供する |
| **横断サービス層** | `services/`, `common/`, `models/` | 設定永続化・時間管理・エラー通知など、層を横断して利用される共通機能を提供する |
| **プレゼンテーション層** | `app/`, `ui/`, `views/`, `widgets/`, `dialogs/` | ユーザー操作の受付、画面描画、シグナル/スロット配線を行う |

### 3.2 層間の依存ルール

依存は**上位→下位**の一方向のみ許可される。同一レイヤー内の横方向依存は限定的に許可される。

| 参照元（依存する側） | 参照可能な層 | 参照禁止 |
|-------------------|------------|---------|
| **コア層** (`core/`) | なし（外部依存なし） | 他の全層 |
| **ドメインサービス層** (`game/`, `kifu/` 等) | コア層、横断サービス層 | プレゼンテーション層 |
| **横断サービス層** (`services/`, `common/`) | コア層（必要時のみ） | ドメインサービス層、プレゼンテーション層 |
| **プレゼンテーション層** (`ui/`, `views/` 等) | 全層 | — |

**同一レイヤー内の依存例**:

- `analysis/` → `engine/`: 解析機能がUSIエンジンを利用する
- `network/` → `engine/`: CSA通信対局がUSIエンジンを利用する
- `navigation/` → `kifu/`: ナビゲーションが棋譜データにアクセスする
- `ui/presenters/` → `ui/controllers/`: プレゼンターがコントローラを参照する

**禁止される依存の例**:

- `core/` → `game/`: コア層がドメインサービス層のクラスを参照してはならない
- `game/` → `ui/`: ドメインサービス層がUI部品を直接操作してはならない
- `services/` → `game/`: 横断サービスがドメインロジックに依存してはならない

> **例外**: MainWindowは全層を横断するハブであるため、全てのレイヤーへの参照を持つ。これは設計上の意図的な選択であり、MainWindowがファサードとして各層を接続する役割を担うためである（3.3節参照）。

### 3.3 MainWindowの「ハブ/ファサード」としての位置づけ

MainWindow（`src/app/mainwindow.h/.cpp`）は、ShogiBoardQ全体のUI起点であると同時に、各レイヤーのコンポーネントを接続するハブとして動作する。

```mermaid
graph LR
    subgraph "MainWindow (ファサード)"
        MW_CORE["ensure*() 群<br/>遅延初期化"]
        MW_WIRE["シグナル/スロット<br/>接続ハブ"]
        MW_STATE["PlayMode<br/>状態管理"]
    end

    USER["ユーザー操作<br/>(メニュー/ボタン)"] --> MW_CORE
    MW_CORE --> MC["MatchCoordinator<br/>(対局司令塔)"]
    MW_CORE --> KLC["KifuLoadCoordinator<br/>(棋譜読込)"]
    MW_CORE --> GSC["GameStartCoordinator<br/>(対局開始)"]
    MW_CORE --> AC["AnalysisCoordinator<br/>(解析)"]
    MW_CORE --> DC["DialogCoordinator<br/>(ダイアログ)"]

    MW_WIRE --> SV["ShogiView<br/>(盤面描画)"]
    MW_WIRE --> RP["RecordPane<br/>(棋譜欄)"]
    MW_WIRE --> EC["EvalChart<br/>(評価値グラフ)"]
    MW_WIRE --> EAT["EngineAnalysisTab<br/>(解析タブ)"]

    MC --> SGC["ShogiGameController"]
    MC --> USI["Usi (エンジン)"]
```

#### MainWindowの設計原則

1. **直接ロジックは最小化**: MainWindow自身にはビジネスロジックを書かず、`ensure*()` で遅延生成した専用クラスへ委譲する
2. **接続ハブ**: 各コントローラ/コーディネータ間のシグナル/スロット接続を MainWindow に集約する
3. **同期点**: 盤面、棋譜行、手番、評価値グラフの同期処理を MainWindow で行う
4. **遅延初期化**: 約40個の `ensure*()` メソッドで、必要になるまでオブジェクトを生成しない

#### MainWindowが接続する主要コンポーネント

| カテゴリ | コンポーネント | 役割 |
|---------|-------------|------|
| **対局進行** | `MatchCoordinator` | 対局全体の司令塔（手番管理、勝敗判定） |
| **対局開始** | `GameStartCoordinator` | ダイアログ表示→設定→対局開始の一連のフロー |
| **棋譜読込** | `KifuLoadCoordinator` | 棋譜ファイルの読み込みと盤面反映 |
| **ゲーム制御** | `ShogiGameController` | 指し手の適用、成り判定、合法手チェック |
| **盤面操作** | `BoardInteractionController` | マウスクリック→駒選択→移動の変換 |
| **UI配線** | `*Wiring` 群（8種） | 機能別のシグナル/スロット接続をカプセル化 |
| **プレゼンター** | `*Presenter` 群（5種） | Model→View の表示更新を仲介 |
| **UI調整** | `*Coordinator` 群（6種） | 複数ウィジェットにまたがるUI操作を調整 |

### 3.4 PlayMode列挙型

`PlayMode`（`src/core/playmode.h`）はアプリケーション全体の動作モードを定義する列挙型で、UIの有効/無効状態、エンジン通信の開始/停止、ナビゲーションの可否など、アプリ全体の振る舞いを切り替える最も重要な状態変数である。

#### 全PlayMode値

| PlayMode値 | 意味 | 分類 |
|------------|------|------|
| `NotStarted` | 起動直後、対局未開始。棋譜閲覧・編集のみ可能 | 初期状態 |
| `HumanVsHuman` | 人間 vs 人間（平手・駒落ち） | 対局モード |
| `EvenHumanVsEngine` | 平手・先手:人間 vs 後手:エンジン | 対局モード |
| `EvenEngineVsHuman` | 平手・先手:エンジン vs 後手:人間 | 対局モード |
| `EvenEngineVsEngine` | 平手・駒落ち・エンジン vs エンジン | 対局モード |
| `HandicapEngineVsHuman` | 駒落ち・下手:エンジン vs 上手:人間 | 対局モード |
| `HandicapHumanVsEngine` | 駒落ち・下手:人間 vs 上手:エンジン | 対局モード |
| `HandicapEngineVsEngine` | 駒落ち・エンジン vs エンジン | 対局モード |
| `AnalysisMode` | 棋譜解析モード（全指し手を自動評価） | 解析モード |
| `ConsiderationMode` | 検討モード（任意の局面をリアルタイム解析） | 解析モード |
| `TsumiSearchMode` | 詰将棋探索モード（詰み手順の探索） | 解析モード |
| `CsaNetworkMode` | CSA通信対局モード（ネットワーク越しの対局） | 通信モード |
| `PlayModeError` | エラー状態（不正な設定値の検出用） | エラー |

#### PlayModeによる動作分岐

PlayModeはアプリケーション全体の振る舞いを決定する中心的な状態変数であり、以下の領域に影響する。

**1. 盤面操作の制御**

| PlayMode | 人間の駒操作 | 備考 |
|----------|------------|------|
| `NotStarted` | 不可 | 棋譜閲覧モード |
| `HumanVsHuman` | 両手番で可能 | — |
| `*HumanVsEngine` | 人間側の手番のみ | エンジン側の手番では操作不可 |
| `*EngineVsHuman` | 人間側の手番のみ | エンジン側の手番では操作不可 |
| `*EngineVsEngine` | 不可 | エンジン同士の自動対局 |
| `ConsiderationMode` | 不可 | 棋譜ナビゲーションのみ |
| `CsaNetworkMode` | 自分の手番のみ | ネットワーク対戦 |

**2. エンジンの使用**

| PlayMode | エンジン1 | エンジン2 | 用途 |
|----------|----------|----------|------|
| `HumanVsHuman` | 不使用 | 不使用 | — |
| `EvenHumanVsEngine` | 不使用 | 対局用 | 後手エンジン |
| `EvenEngineVsHuman` | 対局用 | 不使用 | 先手エンジン |
| `*EngineVsEngine` | 対局用 | 対局用 | 両方対局用 |
| `ConsiderationMode` | 解析用 | — | 局面評価 |
| `AnalysisMode` | 解析用 | — | 全局面自動評価 |
| `TsumiSearchMode` | 探索用 | — | 詰み手順探索 |
| `CsaNetworkMode` | 対局用 | — | 通信対局 |

**3. ナビゲーション・UI状態**

| PlayMode | 棋譜ナビゲーション | 対局開始メニュー | 棋譜読込 |
|----------|-----------------|----------------|---------|
| `NotStarted` | 有効 | 有効 | 有効 |
| 対局モード全般 | **無効** | **無効** | **無効** |
| `ConsiderationMode` | 有効（局面移動で再解析） | **無効** | **無効** |
| `AnalysisMode` | **無効**（自動進行中） | **無効** | **無効** |

**4. PlayModeの遷移パターン**

```
NotStarted ──→ HumanVsHuman / *VsEngine / *VsHuman / *VsEngine
    ↑              │
    └──── 対局終了 ←┘

NotStarted ──→ ConsiderationMode / AnalysisMode / TsumiSearchMode
    ↑              │
    └──── 解析終了 ←┘

NotStarted ──→ CsaNetworkMode
    ↑              │
    └──── 通信終了 ←┘
```

全てのモードは終了時に `NotStarted` に戻る。PlayModeの遷移は `MainWindow::m_playMode` で管理され、`GameStartCoordinator` や `MatchCoordinator` が遷移をトリガーする。

### 3.5 データフロー概要

以下は、ユーザーが盤面上で駒を動かしてから表示が更新されるまでの主要なデータフローである。

```mermaid
sequenceDiagram
    participant User as ユーザー
    participant SV as ShogiView<br/>(盤面描画)
    participant BIC as BoardInteraction<br/>Controller
    participant BSC as BoardSetup<br/>Controller
    participant SGC as ShogiGame<br/>Controller
    participant SB as ShogiBoard<br/>(盤面状態)
    participant MC as MatchCoordinator<br/>(司令塔)
    participant USI as Usi<br/>(エンジン)
    participant RP as RecordPane<br/>(棋譜欄)
    participant EC as EvalChart<br/>(評価値)

    User->>SV: マウスクリック<br/>(駒選択→移動先)
    SV->>BIC: clicked(QPoint)
    BIC->>BIC: 駒の選択と合法な移動先の表示<br/>(EngineMoveValidator)
    BIC->>BSC: moveRequested(from, to)<br/>(MainWindow 経由)
    BSC->>SGC: validateAndMove()
    SGC->>SGC: 合法手判定・成り/不成の選択<br/>(EngineMoveValidator・PromotionFlow)
    SGC->>SB: applyUsiMove()
    SB-->>SV: dataChanged / standChanged<br/>(変わったマスを再描画)
    SGC-->>BSC: 成功
    BSC->>BIC: onMoveApplied()<br/>(移動元・移動先のハイライト)
    BSC->>MC: onHumanMove()
    MC->>RP: 棋譜行追加<br/>(GameRecordUpdateService)

    MC->>USI: position + go<br/>(エンジンへ送信)
    USI-->>MC: bestmove<br/>(エンジンの応手)
    MC->>SGC: applyMove(UsiMove)
    SGC->>SB: applyUsiMove()
    MC->>RP: 棋譜行追加
    MC->>EC: 評価値グラフ更新
```

#### フロー解説

1. **ユーザー操作**: `ShogiView` がマウスクリックを受け、`clicked` シグナルで `BoardInteractionController` に盤の座標を通知する。1回目のクリックで駒を選び、設定により `EngineMoveValidator` で合法な移動先を光らせる
2. **着手要求**: 2回目のクリックで `moveRequested` を発行する。`MainWindow` は `MainWindowServiceRegistry` を通して `BoardSetupController` に渡すだけである
3. **合法手判定と適用**: `ShogiGameController::validateAndMove()` が `EngineMoveValidator` で合法手かを判定し、成り・不成の両方が合法なら `PromotionFlow` のダイアログで選ばせ、`ShogiBoard::applyUsiMove()` で盤を更新する
4. **表示の同期**:
   - `ShogiView`: `ShogiBoard` の `dataChanged`／`standChanged` シグナルで、変わったマスと駒台だけを描き直す。移動元・移動先のハイライトは `BoardInteractionController` が付ける
   - `RecordPane`: 対局モードの Strategy が、考慮時間とともに `GameRecordUpdateService` で棋譜行を追加する
   - `moveCommitted` シグナルでは、現在の局面・定跡ウィンドウ・駒音を更新する
5. **エンジン応答**: 対エンジンの対局では、`MatchCoordinator` の Strategy がエンジンに `position` + `go` を送り、`bestmove` を `ShogiGameController::applyMove()` で指して、棋譜と評価値グラフを更新する

#### 棋譜読み込みのデータフロー

```mermaid
sequenceDiagram
    participant User as ユーザー
    participant MW as MainWindow
    participant KLC as KifuLoad<br/>Coordinator
    participant CONV as Format<br/>Converter
    participant SB as ShogiBoard
    participant GRM as GameRecord<br/>Model
    participant SV as ShogiView
    participant RP as RecordPane

    User->>MW: ファイル選択<br/>(chooseAndLoadKifuFile)
    MW->>KLC: loadFromFile(path)
    KLC->>CONV: detectInitialSfen()<br/>+ parseWithVariations()
    CONV-->>KLC: SFEN + USI指し手列<br/>+ 分岐データ
    KLC->>SB: setSfenString(initialSfen)
    KLC->>GRM: 棋譜データ設定
    KLC-->>MW: loadCompleted

    par 表示更新
        MW->>SV: 初期局面描画
        MW->>RP: 棋譜行一覧表示
    end
```

棋譜ファイルは6種類のフォーマット（KIF/KI2/CSA/JKF/USEN/USI）のいずれかを自動判別し、対応する変換器（`*ToSfenConverter`）で内部表現（SFEN + USI指し手列）に統一変換される。

### 3.6 アーキテクチャの設計判断

#### なぜMainWindowがハブなのか

Qt Widgetsアプリケーションでは、`QMainWindow` がメニューバー、ツールバー、ドックウィジェット、セントラルウィジェットの所有者であるため、UI操作の起点が自然とMainWindowに集中する。ShogiBoardQでは、この構造を活かしつつ以下の戦略で複雑さを管理している:

- **Wiring クラス**: 機能別のシグナル/スロット接続をカプセル化（`CsaGameWiring`, `PlayerInfoWiring` 等）
- **Coordinator クラス**: 複数コンポーネントにまたがる操作を調整（`GameStartCoordinator`, `DialogCoordinator` 等）
- **Presenter クラス**: Model→View の表示更新ロジックを分離（`BoardSyncPresenter`, `RecordPresenter` 等）
- **ensure*() パターン**: 遅延初期化により、必要になるまでオブジェクトを生成しない

これらのパターンの詳細は[第4章](#第4章-設計パターンとコーディング規約)で解説する。

#### コア層の独立性

`core/` ディレクトリのコードは Qt GUI（`QWidget`, `QGraphicsView` 等）に依存せず、純粋なC++とQtのユーティリティクラス（`QString`, `QPoint`, `QMap` 等）のみを使用する。これにより:

- ゲームロジックの単体テストが容易になる
- UI変更がルールエンジンに影響しない
- 将来的にCLI版やWebバックエンドへの再利用が可能になる

<!-- chapter-3-end -->

---

<!-- chapter-4-start -->
## 第4章 設計パターンとコーディング規約

本章では、ShogiBoardQ全体で繰り返し使われる設計パターンと、守るべきコーディング規約を解説する。新規コードを書く際はこれらのパターンに従うこと。

### 4.1 Deps構造体パターン

#### 目的

クラスが必要とする外部オブジェクト（依存）を1つの構造体にまとめ、コンストラクタや `updateDeps()` メソッドで一括注入する。これにより以下の利点が得られる:

- **依存の明示化**: クラスが何に依存しているかがヘッダの `Deps` 定義を見るだけで分かる
- **遅延初期化との親和性**: MainWindowの `ensure*()` でオブジェクトが段階的に生成されるため、後から依存を差し替える必要がある
- **テスト容易性**: テスト時にモックやスタブを `Deps` 経由で注入できる

#### 基本構造

```cpp
class SomeClass : public QObject {
    Q_OBJECT
public:
    struct Deps {
        ObjectA* objA = nullptr;     ///< 説明（非所有）
        ObjectB* objB = nullptr;     ///< 説明（非所有）
        int*     sharedValue = nullptr;  ///< 共有状態（非所有）
    };

    explicit SomeClass(const Deps& deps, QObject* parent = nullptr);

    /// 依存オブジェクトを再設定する
    void updateDeps(const Deps& deps);

private:
    ObjectA* m_objA = nullptr;
    ObjectB* m_objB = nullptr;
    int*     m_sharedValue = nullptr;
};
```

**要点**:
- `Deps` はクラス内の `public` セクションに定義する
- 全フィールドに `nullptr` または適切なデフォルト値を設定する
- ポインタは全て**非所有**（`Deps` を受け取る側は所有権を持たない）
- `///<` でフィールドの説明を付ける

#### 実例: ConsiderationWiring

```cpp
// src/ui/wiring/considerationwiring.h

class ConsiderationWiring : public QObject {
    Q_OBJECT
public:
    struct Deps {
        QWidget* parentWidget = nullptr;
        ConsiderationTabManager* considerationTabManager = nullptr;
        EngineInfoWidget* thinkingInfo1 = nullptr;
        ShogiView* shogiView = nullptr;
        MatchCoordinator* match = nullptr;
        DialogCoordinator* dialogCoordinator = nullptr;
        ShogiEngineThinkingModel* considerationModel = nullptr;
        UsiCommLogModel* commLogModel = nullptr;
        PlayMode* playMode = nullptr;
        QString* currentSfenStr = nullptr;
        std::function<void()> ensureDialogCoordinator;  ///< DialogCoordinator 遅延初期化コールバック
    };

    explicit ConsiderationWiring(const Deps& deps, QObject* parent = nullptr);
    void updateDeps(const Deps& deps);
    // ...
};
```

> **注目**: `std::function<void()> ensureDialogCoordinator` は遅延初期化コールバックである。Wiringクラスが自分では `ensureDialogCoordinator()` を呼べないため、レジストリ側の遅延初期化関数を `std::function` 経由で注入する。このパターンについては [4.3節](#43-hooksstdfunction-コールバックパターン) で詳述する。

#### updateDeps() メソッドの実装

`updateDeps()` は全フィールドを新しい値で上書きする。遅延初期化により依存オブジェクトが後から生成された場合に呼ばれる。

```cpp
// src/ui/wiring/considerationwiring.cpp

void ConsiderationWiring::updateDeps(const Deps& deps)
{
    m_parentWidget = deps.parentWidget;
    m_considerationTabManager = deps.considerationTabManager;
    m_thinkingInfo1 = deps.thinkingInfo1;
    m_shogiView = deps.shogiView;
    m_match = deps.match;
    m_dialogCoordinator = deps.dialogCoordinator;
    m_considerationModel = deps.considerationModel;
    m_commLogModel = deps.commLogModel;
    m_playMode = deps.playMode;
    m_currentSfenStr = deps.currentSfenStr;
    m_ensureDialogCoordinator = deps.ensureDialogCoordinator;
    if (m_uiController) ensureUIController();
}
```

#### 実例: MatchCoordinator::Deps

大規模なクラスでは `Deps` に加えて `Hooks` 構造体（コールバック群）も持つ:

```cpp
// src/game/matchcoordinator.h

struct Deps {
    ShogiGameController* gc = nullptr;         ///< ゲームコントローラ（非所有）
    ShogiClock*          clock = nullptr;       ///< 将棋時計（非所有）
    Usi*                 usi1 = nullptr;         ///< エンジン1（非所有）
    Usi*                 usi2 = nullptr;         ///< エンジン2（非所有）
    UsiCommLogModel*           comm1 = nullptr;  ///< エンジン1通信ログ（非所有）
    ShogiEngineThinkingModel*  think1 = nullptr; ///< エンジン1思考情報（非所有）
    UsiCommLogModel*           comm2 = nullptr;  ///< エンジン2通信ログ（非所有）
    ShogiEngineThinkingModel*  think2 = nullptr; ///< エンジン2思考情報（非所有）
    Hooks                hooks;                  ///< UIコールバック群
    QStringList* sfenRecord = nullptr;           ///< SFEN履歴（非所有）
    QList<ShogiMove>* gameMoves = nullptr;       ///< 指し手リスト（非所有、外部共有用）
};
```

### 4.2 ensure*() 遅延初期化パターン

#### 目的

`ensure*()` メソッドは MainWindow ではなくレジストリ（`MainWindowServiceRegistry`・`MainWindowFoundationRegistry`・`KifuSubRegistry`、合わせて約50個）にあり、各コンポーネントを**初めて必要になった時点**で生成する。これにより:

- **起動時間の短縮**: 全オブジェクトを起動時に生成しない
- **循環依存の回避**: AがBに依存し、BがAに依存する場合も、使用時点で双方が存在すれば問題ない
- **メモリ節約**: 使用しない機能のオブジェクトは生成されない

#### 生成と依存更新の契約

依存先が対局・読み込み・UI生成で変わるコンポーネントは、次の順で処理する。

1. 未生成なら一度だけ生成する。
2. 既存インスタンスにも、最新の参照とコールバックを設定する。
3. シグナル接続は初回のみ、または `Qt::UniqueConnection` で重複を防ぐ。

レジストリは生成を `create*()`、依存更新を `refresh*Deps()` に分ける（[レジストリ境界仕様](registry-boundary.md) の「ensure* の2段構造パターン」）。
更新時には、対局・解析の進行状態を初期化しない。
依存を持たない `PositionEditController` などの生成専用メソッドは、生成済みなら復帰してよい。

```cpp
// src/app/gamesubregistry_wiring.cpp
void MainWindowServiceRegistry::ensureMatchCoordinatorWiring()
{
    if (!m_mw.m_matchWiring) {
        createMatchCoordinatorWiring();   // 生成と、シグナルの受け手の登録（初回だけ）
    }
    refreshMatchWiringDeps();             // 最新の部品とフックを渡し直す
}
```

#### レジストリの ensure*() 一覧（主要なもの）

| メソッド | 生成するクラス | 分類 |
|---------|-------------|------|
| `ensureBoardSyncPresenter()` | `BoardSyncPresenter` | Presenter |
| `ensureRecordPresenter()` | `GameRecordPresenter` | Presenter |
| `ensureAnalysisPresenter()` | `AnalysisResultsPresenter` | Presenter |
| `ensureGameStartCoordinator()` | `GameStartCoordinator` | Coordinator |
| `ensureDialogCoordinator()` | `DialogCoordinator` | Coordinator |
| `ensurePositionEditCoordinator()` | `PositionEditCoordinator` | Coordinator |
| `ensureConsiderationWiring()` | `ConsiderationWiring` | Wiring |
| `ensureCsaGameWiring()` | `CsaGameWiring` | Wiring |
| `ensurePlayerInfoWiring()` | `PlayerInfoWiring` | Wiring |
| `ensureGameStateController()` | `GameStateController` | Controller |
| `ensureReplayController()` | `ReplayController` | Controller |
| `ensureLanguageController()` | `LanguageController` | Controller |
| `ensureMatchCoordinatorWiring()` | `MatchCoordinatorWiring`（司令塔は `initMatchCoordinator()` で作る） | Wiring |
| `ensureKifuLoadCoordinator()` | `KifuLoadCoordinator`（最初の読み込みで作り、以後使い回す） | Coordinator |
| `ensureGameRecordModel()` | `GameRecordModel` | データモデル |

#### Wiringクラス内での ensure*() パターン

Wiringクラスが内部で所有するコントローラにも同じパターンを適用する:

```cpp
// src/ui/wiring/considerationwiring.cpp

void ConsiderationWiring::ensureUIController()
{
    // 初回だけ作り、2回目以降（updateDeps から）は最新の部品を渡し直す
    if (!m_uiController) m_uiController = new ConsiderationModeUIController(this);
    m_uiController->setConsiderationTabManager(m_considerationTabManager);
    m_uiController->setThinkingEngineInfo(m_thinkingInfo1);
    m_uiController->setShogiView(m_shogiView);
    m_uiController->setMatchCoordinator(m_match);
    m_uiController->setConsiderationModel(m_considerationModel);
    m_uiController->setCommLogModel(m_commLogModel);

    // コントローラからのシグナルを接続
    connect(m_uiController, &ConsiderationModeUIController::stopRequested,
            this, &ConsiderationWiring::handleStopRequest, Qt::UniqueConnection);
    connect(m_uiController, &ConsiderationModeUIController::multiPVChangeRequested,
            this, &ConsiderationWiring::onMultiPVChangeRequested, Qt::UniqueConnection);
}
```

#### 遅延初期化の依存ギャップ問題

コードをMainWindowからWiringクラスへ抽出する際、元のメソッド内にあった `ensure*()` 呼び出しがWiringクラスからはアクセスできなくなる。この問題は `Deps` に `std::function` コールバックを追加することで解決する:

```cpp
// Deps に遅延初期化コールバックを定義
struct Deps {
    // ...
    std::function<void()> ensureDialogCoordinator;  // 遅延初期化コールバック
};

// 使用側（Wiringクラス）
void ConsiderationWiring::displayConsiderationDialog()
{
    // m_dialogCoordinator が未初期化の場合、コールバックで遅延初期化を実行
    if (!m_dialogCoordinator && m_ensureDialogCoordinator) {
        m_ensureDialogCoordinator();  // MainWindowServiceRegistry::ensureDialogCoordinator() が呼ばれる
    }
    // ... 以降、m_dialogCoordinator を使用
}
```

レジストリ側で `Deps` を構築する際にコールバックをバインドする:

```cpp
// src/app/mainwindowanalysisregistry.cpp
ConsiderationWiring::Deps deps;
deps.ensureDialogCoordinator = [this]() {
    ensureDialogCoordinator();
    if (m_mw.m_considerationWiring)
        m_mw.m_considerationWiring->setDialogCoordinator(m_mw.m_dialogCoordinator);
};
```

> **注意**: この `std::function` は `connect()` のラムダ禁止ルールとは無関係である。`connect()` でのラムダ禁止はシグナル/スロット接続に限定されたルールであり、`Deps` 構造体内のコールバック注入は許容される。

### 4.3 Hooks/std::function コールバックパターン

#### 目的

ドメインサービス層（`game/`, `analysis/` 等）のクラスがUI操作を行う必要がある場合、直接UIクラスに依存せずに `std::function` コールバック群（Hooks）を通じてUI操作を要求する。これにより:

- **層間の依存ルールを維持**: ドメインサービス層がプレゼンテーション層に依存しない
- **柔軟な差し替え**: テスト時にUI操作をモックに差し替えられる
- **責務の分離**: 「何をすべきか」はドメイン層が決定し、「どう表示するか」はUI層が決定する

#### 実例: MatchCoordinator::Hooks

```cpp
// src/game/matchcoordinatorhooks.h（MatchCoordinator::Hooks はこの型の別名）

struct MatchCoordinatorHooks {
    /// UI更新・描画系
    struct UI {
        std::function<void(Turn cur)> updateTurnDisplay;                           ///< 手番表示の更新
        std::function<void(const QString& p1, const QString& p2)> setPlayersNames; ///< 対局者名設定
        std::function<void(const QString& e1, const QString& e2)> setEngineNames;  ///< エンジン名設定
        std::function<void()> renderBoardFromGc;                                   ///< gc→view反映
        std::function<void(const QString& title, const QString& message)> showGameOverDialog;
        std::function<bool()> usesHandicapNames;                                   ///< 対局者を下手・上手と呼ぶか
        std::function<void(const QPoint& from, const QPoint& to)> showMoveHighlights;
    };

    /// ゲーム初期化・棋譜系
    struct Game {
        std::function<void(const QString& sfenStart)> initializeNewGame;
        std::function<void(const QString& text, const QString& elapsed)> appendKifuLine;
        std::function<void(Turn side)> appendEval;                                 ///< エンジンの手のあとの評価値追記
        std::function<void(const QString& saveDir)> autoSaveKifu;                  ///< 棋譜自動保存
    };

    UI ui;
    Game game;
};
```

フックは `MainWindowServiceRegistry::buildMatchHooks()`（UNDO 用は `buildUndoHooks()`）が組み立てる。司令塔は一度だけ作るので、各フックは組み立てたときのポインタを写さない。レジストリ自身（`this`）か一度だけ作るアダプタ（`MainWindowMatchAdapter`）を捕まえ、呼ばれた時点の部品を読む。エンジンへの stop 送信はフックに置かず、`MatchTurnHandler` には司令塔が自分の `sendStopToEngine()` を渡す。

#### Hooks と Deps の使い分け

| 構造体 | 格納するもの | 特徴 |
|--------|------------|------|
| **Deps** | 依存オブジェクトのポインタ | 参照を保持するだけ。`updateDeps()` で差し替え可能 |
| **Hooks** | `std::function` コールバック | 動作を注入する。UI操作やAPI差異の吸収に使用 |

`MatchCoordinator` は `Deps` 内に `Hooks` を含む形で両方を使用する:

```cpp
struct Deps {
    ShogiGameController* gc = nullptr;
    ShogiClock*          clock = nullptr;
    // ...
    Hooks                hooks;    ///< UIコールバック群（Deps の一部として格納）
};
```

### 4.4 Wiringクラスパターン

#### 目的

シグナル/スロット接続のコードはMainWindowに集中しがちである。Wiringクラスは、**機能単位**の接続ロジックをMainWindowから分離してカプセル化する。

#### 全Wiringクラス一覧

| クラス | ファイル | 責務 |
|-------|---------|------|
| `UiActionsWiring` | `ui/wiring/uiactionswiring.h` | メニュー/ツールバーのQActionを担当クラスのスロットへ接続（ファイル・対局・編集・表示の4つの子に分ける） |
| `ConsiderationWiring` | `ui/wiring/considerationwiring.h` | 検討モードの開始/停止/設定変更フロー |
| `CsaGameWiring` | `ui/wiring/csagamewiring.h` | CSA通信対局関連の接続 |
| `PlayerInfoWiring` | `ui/wiring/playerinfowiring.h` | 対局者情報/エンジン名の設定と表示 |
| `AnalysisTabWiring` | `ui/wiring/analysistabwiring.h` | 解析タブ関連の接続 |
| `RecordPaneWiring` | `ui/wiring/recordpanewiring.h` | 棋譜ペイン関連の接続 |
| `MenuWindowWiring` | `ui/wiring/menuwindowwiring.h` | メニューウィンドウ関連の接続 |
| `JosekiWindowWiring` | `ui/wiring/josekiwindowwiring.h` | 定跡ウィンドウ関連の接続 |

#### Wiringクラスの典型的な構造

```
Wiringクラス
├── Deps 構造体（依存オブジェクト群）
├── コンストラクタ（Deps を受け取って内部に保持）
├── updateDeps()（依存の差し替え）
├── wire() メソッド（一括接続を実行）
│   └── connect() 呼び出し群
└── public slots（接続先のスロットメソッド群）
```

#### 実例: UiActionsWiring

アクションの接続だけを行うWiringクラスの例。`wire()` が、メニューの種類ごとの子（`FileActionsWiring`・`GameActionsWiring`・`EditActionsWiring`・`ViewActionsWiring`）を初回だけ作り、それぞれの `wire()` を呼ぶ:

```cpp
// src/ui/wiring/uiactionswiring.h

class UiActionsWiring : public QObject {
    Q_OBJECT
public:
    struct Deps {
        Ui::MainWindow* ui = nullptr;           ///< UIオブジェクト
        QObject*        ctx = nullptr;          ///< 受け側オブジェクト（MainWindow*）
        DialogLaunchWiring* dlw = nullptr;      ///< ダイアログ起動配線
        DialogCoordinatorWiring* dcw = nullptr; ///< DialogCoordinator配線（解析中止用）
        KifuFileController* kfc = nullptr;      ///< 棋譜ファイル操作コントローラ
        GameSessionOrchestrator* gso = nullptr; ///< 対局ライフサイクルオーケストレータ
        // ... 外観・盤・評価値グラフ・棋譜エクスポートの取得関数
    };

    explicit UiActionsWiring(Deps d, QObject* parent = nullptr);

    /// 全アクションのシグナル/スロット接続を実行する（多重接続を避けるため1回だけ呼ぶ）
    void wire();

private:
    Deps m_d;
    FileActionsWiring* m_fileWiring = nullptr;
    GameActionsWiring* m_gameWiring = nullptr;
    EditActionsWiring* m_editWiring = nullptr;
    ViewActionsWiring* m_viewWiring = nullptr;
};
```

```cpp
// src/ui/wiring/fileactionswiring.cpp（抜粋）

void FileActionsWiring::wire()
{
    // ファイル/アプリ。MainWindow に残っているのは終了だけで、ほかは担当クラスへ直接つなぐ
    QObject::connect(ui->actionQuit,         &QAction::triggered, mw,  &MainWindow::saveSettingsAndClose,          Qt::UniqueConnection);
    QObject::connect(ui->actionSaveAs,       &QAction::triggered, kfc, &KifuFileController::saveKifuToFile,        Qt::UniqueConnection);
    QObject::connect(ui->actionOpenKifuFile, &QAction::triggered, kfc, &KifuFileController::chooseAndLoadKifuFile, Qt::UniqueConnection);
}

// src/ui/wiring/gameactionswiring.cpp（抜粋）
QObject::connect(ui->actionStartGame, &QAction::triggered, gso, &GameSessionOrchestrator::initializeGame,    Qt::UniqueConnection);
QObject::connect(ui->actionResign,    &QAction::triggered, gso, &GameSessionOrchestrator::handleResignation, Qt::UniqueConnection);
```

#### 実例: ConsiderationWiring（スロットを持つWiring）

検討モードのように複雑なフローを扱うWiringクラスは、`wire()` だけでなく独自のスロットメソッドを持つ:

```cpp
// src/ui/wiring/considerationwiring.h（スロット部分）

public slots:
    void displayConsiderationDialog();           // 検討開始
    void onEngineSettingsRequested(int engineNumber, const QString& engineName);
    void onEngineChanged(int engineIndex, const QString& engineName);
    void onModeStarted();                        // 検討モード開始時
    void onTimeSettingsReady(bool unlimited, int byoyomiSec);
    void onMultiPVChangeRequested(int value);    // MultiPV変更
    void updateArrows();                         // 矢印更新
    void onShowArrowsChanged(bool checked);      // 矢印表示切替
```

このように、Wiringクラスは単なる接続コード置き場ではなく、接続のターゲットとなるスロットメソッド自体も保持できる。

### 4.5 MVPパターン: Presenter/Controller/Coordinator の使い分け

ShogiBoardQの `ui/` ディレクトリには3種類のクラスが存在する。それぞれの責務と命名規則を以下にまとめる。

#### 役割の比較

| 種類 | 命名規則 | 責務 | 例 |
|------|---------|------|-----|
| **Presenter** | `*Presenter` | **Model→View の表示更新**。データを受け取り、ウィジェットの表示を更新する | `BoardSyncPresenter`, `GameRecordPresenter` |
| **Controller** | `*Controller` | **ユーザー操作のハンドリング**。UIイベントを受け取り、適切なアクションを実行する | `EvaluationGraphController`, `LanguageController` |
| **Coordinator** | `*Coordinator` | **複数コンポーネントの調整**。複数のウィジェットやコントローラにまたがる操作を司る | `DialogCoordinator`, `KifuDisplayCoordinator` |

#### データフローの方向

```
  ユーザー操作 ──→ Controller ──→ ドメイン層
                                      │
                                      ↓
              View ←── Presenter ←── Model/データ変更
                                      │
                                      ↓
                   Coordinator ──→ 複数ウィジェット同期
```

#### Presenter の実例

**BoardSyncPresenter** — 盤面の表示同期を担当:

```cpp
// src/ui/presenters/boardsyncpresenter.h

class BoardSyncPresenter : public QObject {
    Q_OBJECT
public:
    struct Deps {
        ShogiGameController* gc = nullptr;
        ShogiView* view = nullptr;
        BoardInteractionController* bic = nullptr;
        std::function<RecordLine()> currentLine;   // 表示中の手順（KifuNavigationState::currentRecordLine）
    };

    explicit BoardSyncPresenter(const Deps& deps, QObject* parent = nullptr);

    void syncBoardAndHighlightsAtRow(int ply) const;   // 表示中の手順の ply 手目を描いてハイライト
    void loadBoardWithHighlights(const QString& currentSfen,
                                  const QString& prevSfen) const; // 指定の局面を描いてハイライト
};
```

**GameRecordPresenter** — 棋譜表示の更新を担当:

```cpp
// src/ui/presenters/gamerecordpresenter.h

class GameRecordPresenter : public QObject {
    Q_OBJECT
public:
    struct Deps {
        KifuRecordListModel* model {nullptr};
        RecordPane*          recordPane {nullptr};
    };

    explicit GameRecordPresenter(const Deps& d, QObject* parent = nullptr);
    void updateDeps(const Deps& d);

    void clear();
    void presentGameRecord(const QList<KifDisplayItem>& disp);  // 棋譜全体を描画
    void appendMoveLine(const QString& prettyMove,
                        const QString& elapsedTime);             // 1手追記
};
```

**EvalGraphPresenter** — 名前空間として実装されたPresenter（状態を持たない場合）:

```cpp
// src/ui/presenters/evalgraphpresenter.h

namespace EvalGraphPresenter {
    void appendPrimaryScore(QList<int>& scoreCp, MatchCoordinator* match);
    void appendSecondaryScore(QList<int>& scoreCp, MatchCoordinator* match);
}
```

#### Controller の実例

**EvaluationGraphController** — 評価値グラフのUIイベントを処理
**LanguageController** — 言語切替操作を処理
**BoardSetupController** — 盤面セットアップ操作を処理

#### Coordinator の実例

**DialogCoordinator** — 各種ダイアログの表示を管理:

```cpp
// src/ui/coordinators/dialogcoordinator.h

class DialogCoordinator : public QObject {
    Q_OBJECT
public:
    explicit DialogCoordinator(QWidget* parentWidget, QObject* parent = nullptr);
    // バージョン情報、エンジン設定、成り選択、検討、詰み探索、
    // 棋譜解析、ゲームオーバー表示などのダイアログを統括
};
```

**KifuDisplayCoordinator** — 棋譜表示・分岐候補の再構築を管理

#### 命名ガイドライン

| やりたいこと | 使うべきクラス種 | 命名例 |
|-------------|---------------|--------|
| データをウィジェットに反映したい | Presenter | `FooPresenter` |
| ユーザー操作に反応したい | Controller | `FooController` |
| 複数コンポーネントを同期したい | Coordinator | `FooCoordinator` |
| シグナル/スロット接続を整理したい | Wiring | `FooWiring` |

### 4.6 ErrorBus: グローバルエラー通知

#### 仕組み

`ErrorBus` はシングルトンパターンのイベントバスで、アプリケーション内のどこからでもエラーを発行し、購読者に一斉配信する。

```cpp
// src/common/errorbus.h

class ErrorBus final : public QObject {
    Q_OBJECT
public:
    static ErrorBus& instance();

    /// エラーメッセージを発行する
    void postError(const QString& message) {
        emit errorOccurred(message);
    }

signals:
    /// エラーが発生した（→ MainWindow::onErrorBusOccurred）
    void errorOccurred(const QString& message);

private:
    explicit ErrorBus(QObject* parent = nullptr) : QObject(parent) {}
    Q_DISABLE_COPY_MOVE(ErrorBus)
};
```

```cpp
// src/common/errorbus.cpp

ErrorBus& ErrorBus::instance() {
    static ErrorBus inst;  // Meyers' Singleton
    return inst;
}
```

#### 使い方

**エラーの発行**（どのレイヤーからでも可能）:

```cpp
ErrorBus::instance().postError(tr("エンジンの起動に失敗しました"));
```

**エラーの受信**（MainWindowで接続）:

```cpp
connect(&ErrorBus::instance(), &ErrorBus::errorOccurred,
        this, &MainWindow::displayErrorMessage);
```

#### 設計上のポイント

- ドメインサービス層（`engine/`, `kifu/` 等）がUIに依存せずにエラーを通知できる
- MainWindowが `errorOccurred` シグナルを受けてダイアログ表示を行う
- `Q_DISABLE_COPY_MOVE` でコピー/ムーブを禁止し、シングルトンの一意性を保証する

### 4.7 コーディング規約

#### コンパイラ警告

全てのソースコードは以下の警告オプション付きでコンパイルされる:

```
-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
-Wnon-virtual-dtor -Woverloaded-virtual -Wformat=2 -Wimplicit-fallthrough
-Wunused -Wunused-function -Wunused-variable -Wunused-parameter
```

**警告をエラーとして扱わないが、0警告を目標とする。**

#### connect() でのラムダ禁止

本プロジェクトでは `connect()` 文でのラムダ式使用を禁止している。

```cpp
// 正しい: メンバ関数ポインタ構文
connect(sender, &Sender::signal, receiver, &Receiver::slot);

// 禁止: ラムダ式
connect(sender, &Sender::signal, [this]() { /* ... */ });
```

**理由**:
- ラムダ内で `this` をキャプチャした場合、`sender` より先に `receiver` が破棄されるとダングリングポインタになる
- メンバ関数ポインタ構文では、`receiver` が破棄されると Qt が自動的に接続を切断する
- コードの追跡性（どこに接続されているかの `grep` による検索）が向上する
- デバッガでのスタックトレースが読みやすくなる

> **例外**: `Deps` 構造体内の `std::function` コールバックや `Hooks` の `std::function` はこのルールの対象外である。ラムダ禁止はあくまでも `connect()` に限定される。

#### std::as_const() の使用

Qt コンテナ（`QList`, `QMap` 等）を range-for ループで走査する際、暗黙的なデタッチ（コピー）を防ぐために `std::as_const()` を使用する:

```cpp
// 正しい: デタッチ防止
for (const auto& item : std::as_const(list)) { /* ... */ }

// 警告: clazy-range-loop-detach
for (const auto& item : list) { /* ... */ }
```

**理由**: Qt のコンテナは暗黙共有（implicit sharing）を採用しており、非const参照で走査するとコピーオンライトが発生する場合がある。`std::as_const()` で const 参照を強制することで、不要なコピーを確実に防止する。

#### 静的解析

`.clang-tidy` で以下のチェックが有効化されている:

| チェック | 検出対象 |
|---------|---------|
| `clang-diagnostic-unused-*` | 未使用の警告 |
| `misc-unused-*` | 未使用コードの検出 |
| `clang-analyzer-deadcode.*` | デッドコード解析 |
| `readability-redundant-*` | 冗長なコードの検出 |

cppcheck は `--enable=warning,unusedFunction` で未使用関数を検出する。

### 4.8 コメントスタイル

コメントの記述方針の詳細は [commenting-style-guide.md](commenting-style-guide.md) を参照。以下に要点を要約する。

#### 基本方針

| 項目 | 規約 |
|------|------|
| **言語** | 日本語 |
| **記法** | Doxygen 形式（`/** */`, `///`, `///<`） |
| **記述場所** | ヘッダファイル（`.h`）。`.cpp` には実装上の補足のみ |
| **原則** | **Why（なぜ）を書く。What（何）はコードが語る** |

#### 記法の使い分け

| 記法 | 用途 |
|------|------|
| `/** ... */` | クラス、メソッド、構造体のDoxygen複数行ドキュメント |
| `///` | 1行で収まるDoxygenドキュメント |
| `///<` | enum値・メンバ変数の行末コメント |
| `// ---` | セクション区切り内の小見出し |

#### 書かないコメント

- `// コンストラクタ`、`// デストラクタ` — 見れば分かる
- `// getter` / `// setter` — 命名規約から自明
- 変更履歴・日付コメント — Git で管理する
- コメントアウトしたコード — 削除して Git に任せる

#### 適用タグ

スタイルガイドを適用したファイルには以下のタグを付ける:

```cpp
/// @todo remove コメントスタイルガイド適用済み
```

リリース時に一括検出・除去する:
```bash
grep -rn "@todo remove コメントスタイルガイド適用済み" src/
```

詳細は [commenting-style-guide.md](commenting-style-guide.md) を参照。

<!-- chapter-4-end -->

---

<!-- chapter-5-start -->
## 第5章 core層：純粋なゲームロジック

core層（`src/core/`）は将棋のルールと盤面状態を表現する純粋なゲームロジック層である。Qt の GUI モジュール（`QtWidgets`、`QtCharts` 等）に依存せず、`QObject`・`QString`・`QVector` 等の QtCore モジュールのみを使用する。

### 5.1 core層の設計思想

#### GUI非依存の意義

core層が GUI に依存しないことで、以下のメリットが得られる。

| メリット | 説明 |
|---------|------|
| **テスタビリティ** | ウィンドウ生成なしで盤面操作・合法手判定をテストできる |
| **再利用性** | CLI ツール、別の GUI フレームワーク、エンジン内部などに転用可能 |
| **コンパイル高速化** | GUI ヘッダへの依存がないため、変更時の再コンパイル範囲が限定される |
| **関心の分離** | 「ルール」と「表示」が明確に分かれ、どちらかの変更が他方に波及しない |

#### ファイル構成

```
src/core/
├── shogiboard.h / .cpp      盤面データ管理
├── shogimove.h / .cpp        指し手データ構造
├── enginemovevalidator.h / .cpp 合法手判定の入口
├── fmv*.h / .cpp               合法手判定の内部（局面・利き・指し手の生成）
├── shogiclock.h / .cpp       対局時計
├── playmode.h                対局モード列挙
├── legalmovestatus.h         合法手存在状態
├── sfenutils.h               SFEN文字列ユーティリティ
├── usimove.h                 USI と1対1の指し手の値（UsiMove / BoardSquare）
├── shoginotation.h           棋譜の表記（筋・段・駒の名前・CSA の駒種）
└── kifmovetext.h             KIF 形式の指し手の文字列
```

### 5.2 ShogiBoard — 盤面データ管理

`ShogiBoard` は将棋盤の現在局面（駒配置・駒台・手番）を保持し、SFEN文字列との相互変換を行う中核クラスである。

**ソース**: `src/core/shogiboard.h`、`src/core/shogiboard.cpp`（盤面アクセス・駒台）、`src/core/shogiboard_sfen.cpp`（SFEN の入出力）、`src/core/shogiboard_edit.cpp`（局面編集・駒箱）

#### データ構造

```
ShogiBoard
├── m_boardData     : QList<Piece>       81マス。index = (rank - 1) * 9 + (file - 1)
│                                         （file・rank は1〜9。index 0 が1一、80 が9九）
├── m_pieceStand    : QMap<Piece, int>   駒台。玉を含む先後16種の枚数を常に持つ
│                                         （Piece::BlackPawn → 3、Piece::WhitePawn → 0、…）
├── m_currentPlayer : Turn               手番（Turn::Black / Turn::White）
└── m_ranks, m_files : int               段数・筋数（9）
```

**盤面配列 `m_boardData`** は81要素の `QList<Piece>` で、各マスの駒を列挙型 `Piece`（`src/core/shogitypes.h`）で表す。
先手の駒は `Piece::BlackPawn` など、後手の駒は `Piece::WhitePawn` など、成駒は `Piece::BlackPromotedPawn`（と金）や
`Piece::BlackHorse`（馬）などの別の値で、空きマスは `Piece::None` である。`Piece` の値は1文字（先手は大文字、後手は小文字、
成駒は `Q`・`M`・`O`・`T`・`C`・`U` とその小文字、空きマスは空白）で、`pieceToChar()`・`charToPiece()` で文字と変換できる（2.2節）。

**駒台 `m_pieceStand`** は `QMap<Piece, int>` で、駒をキーに枚数を値として持つ。局面編集で玉も駒台に置けるため、
玉を含む先後16種を常に持つ（`addStandPiece()` はこの16種以外の駒を無視する）。16種を0枚で持つ表はすべての盤で共有し
（`initStand()`）、枚数を書き換えたときに初めてその盤の表が作られる（`QMap` の暗黙の共有）。盤を作るたびに16個を挿入しないためで、
`SfenDiffUtils::diffBoards()` のように一時的な盤を作る処理を軽くしている。

**手番 `m_currentPlayer`** は `Turn` である。SFEN の手数は読むときに確かめるだけで、盤には残さない。

#### 主要メソッド一覧

| カテゴリ | メソッド | 説明 |
|---------|---------|------|
| **SFEN変換** | `setSfen(sfenStr)` | SFEN文字列から盤面全体を復元する（不正なら盤面を変えず、ErrorBus に通知する） |
| | `trySetSfen(sfenStr, error)` | 同上。不正なら盤面を変えずに false を返す（通知しない） |
| | `parseSfen(sfenStr)` | SFEN文字列を4つの欄に分ける（static。盤面は変えない） |
| | `convertBoardToSfen()` | 現在の盤面をSFEN形式の文字列に変換する |
| | `convertStandToSfen()` | 駒台をSFEN形式に変換する |
| | `addSfenRecord(...)` | 現局面のSFENレコードをリストに追加する |
| | `copyPositionFrom(other)` | 別の盤の局面（盤・駒台・手番）を写す |
| **盤面参照** | `pieceAt(square)` | 盤上のマスの駒を返す（盤外のマスは `Piece::None`） |
| | `pieceCharacter(file, rank)` | 指定マスの駒を返す（筋10・11は先手・後手の駒台の並び） |
| | `boardData()` | 81マスの配列への const 参照を返す |
| | `pieceStand()` / `pieceStandCount(piece)` | 駒台データへの const 参照／指定駒の枚数を返す |
| | `pieceBox()` / `pieceBoxCount(piece)` | 盤上・両駒台にない駒（駒箱）を数える |
| | `currentPlayer()` | 現在の手番（`Turn`）を返す |
| **駒操作** | `applyUsiMove(move, mover)` | USI の指し手を盤と駒台に適用する（合法手かどうかは確かめない） |
| | `movePieceToSquare(...)` | 盤上の駒を移動する（盤面配列のみ更新） |
| | `setPieceAt(square, piece)` | マスの駒を置き換える（局面の組み立て用） |
| | `updateBoardAndPieceStand(...)` | 盤面と駒台を同時に更新する（局面編集用） |
| | `addPieceToStand(dest)` | 取った駒を駒台に追加する（成駒→元駒に変換） |
| | `addStandPiece(piece, delta)` / `consumeStandPiece(piece)` | 駒台の枚数を増やす／1枚減らす |
| | `decrementPieceOnStand(piece)` | 駒台から1枚減らす（駒打ち時） |
| **盤面リセット** | `resetGameBoard()` | 盤上・両駒台を空にして、全駒を駒箱に戻す（局面編集用） |
| | `initStand()` | 駒台をすべて0枚にする |
| | `flipSides()` | 先後の配置を反転する |

#### シグナル

| シグナル | 接続先 | 用途 |
|---------|-------|------|
| `boardReset()` | ShogiView・BoardInteractionController | 盤面全体の再描画を要求する（`setSfen`・`copyPositionFrom` など） |
| `dataChanged(c, r)` | ShogiView・BoardInteractionController | 特定マスの再描画を要求する |
| `standChanged(piece)` | ShogiView | 駒台のその駒のセルの再描画を要求する |

#### setSfen() の処理フロー

```
setSfen("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1")
  └── trySetSfen()                 … 不正なら盤面を変えずに false（setSfen は ErrorBus に通知する）
        │
        ├── splitSfenFields()      … 空白で盤・手番・持駒・手数の4欄に分け、手番と手数を確かめる
        │                            （欄は元の文字列を指す QStringView で、文字列を作らない）
        ├── readBoardField()       … 盤の欄を1文字ずつ読み、作業用の配列（81マス）に置く
        │                            （成駒の "+"、各段9マス、行き所のない歩・香・桂を確かめる）
        ├── readStandField()       … 持駒の欄を読み、16種の枚数の配列にする
        │
        ├── m_boardData・m_pieceStand・手番を書き換える
        │     （駒台は16種がそろっていれば値だけ書き換え、作り直さない。
        │       枚数が変わらなければ書き込まないので、共有の0枚の表を複製しない）
        └── emit boardReset()      … 盤面描画の全体更新を通知
```

### 5.3 ShogiMove — 指し手データ構造

`ShogiMove` は1手分の指し手情報を保持する構造体である。

**ソース**: `src/core/shogimove.h`, `src/core/shogimove.cpp`

#### フィールド

| フィールド | 型 | 説明 |
|-----------|-----|------|
| `fromSquare` | `QPoint` | 移動元座標。盤上: x,y = 0〜8。駒台: x = 9（先手）/ 10（後手） |
| `toSquare` | `QPoint` | 移動先座標（x,y = 0〜8） |
| `movingPiece` | `QChar` | 動かした駒のSFEN文字（例: `'P'`, `'r'`） |
| `capturedPiece` | `QChar` | 取った相手の駒（なければ空白 `' '`） |
| `isPromotion` | `bool` | 成りフラグ |

#### 座標体系

```
         盤上 (0-indexed)                   駒台
   file: 0  1  2  3  4  5  6  7  8
         ↓  ↓                     ↓     file=9 : 先手駒台
rank 0 [ ][l][n][s][g][k][g][s][n][l]   file=10: 後手駒台
rank 1 [ ][ ][r][ ][ ][ ][ ][b][ ][ ]   (rankは駒種のインデックス)
  ...
rank 8 [L][N][S][G][K][G][S][N][L]

  SFEN座標とのマッピング:
  USI file = 9 - QPoint.x()    (1-9)
  USI rank = 'a' + QPoint.y()  (a-i)
```

盤上の移動は `fromSquare` と `toSquare` がともに盤上座標（0〜8）となる。駒打ちの場合は `fromSquare.x()` が 9（先手駒台）または 10（後手駒台）となり、`toSquare` に打つ先の座標が入る。

#### USI文字列との変換

USI形式への変換は `ShogiMove::toUsiMove()`（`UsiMove` にする）と `UsiMove::toString()` が担当する。

| 指し手の種類 | USI表記例 | fromSquare | toSquare |
|-------------|----------|------------|----------|
| 盤上移動（不成） | `7g7f` | (2, 6) | (2, 5) |
| 盤上移動（成り） | `8h2b+` | (1, 7) | (7, 1) |
| 駒打ち | `P*3d` | (9, ※) | (6, 3) |

`toUsiMove()` は座標系を0-indexed から1-indexed に変換し、`toString()` が段を a-i 表記にする。成りの場合は末尾に `+` を付加し、駒打ちの場合は `駒文字*移動先` の形式を生成する。逆向きは `ShogiMove::fromUsiMove()` が行う。

### 5.4 EngineMoveValidator — 合法手判定

`EngineMoveValidator` は、盤の配列（`ShogiBoard::boardData()`）と駒台（`pieceStand()`）から局面を作り、指し手が合法かどうかの判定、合法手の一覧、王手の数を求めるクラスである。王手の放置、二歩、打ち歩詰め、行き所のない駒を考慮する。メンバーを持たないので、使う場所でそのつど作る。

**ソース**: `src/core/enginemovevalidator.h`, `src/core/enginemovevalidator.cpp`（入口）、`src/core/fmv*.h/.cpp`（内部。名前空間 `fmv`）

| ファイル | 内容 |
|---------|------|
| `fmvtypes.h` | 先後・駒種・持駒の種類（2.2 節）、指し手 `fmv::Move`、指し手の一覧 `fmv::MoveList`（最大600手） |
| `fmvbitboard81.h/.cpp` | 81マスのビットボード `Bitboard81`（`uint64_t` 2つ） |
| `fmvposition.h/.cpp` | 内部の局面 `EnginePosition`（マスの駒・ビットボード・玉の位置・持駒）と、指す・戻す（`doMove()`／`undoMove()`） |
| `fmvattacks.h/.cpp` | コンパイル時に作る利きの表と、マスに利いている駒の判定 |
| `fmvconverter.h/.cpp` | 盤・駒台・`ShogiMove` を内部の型にする変換と、内部の指し手から `UsiMove` への変換 |
| `fmvlegalcore.h/.cpp`, `fmvmovegeneration.cpp` | 合法性の判定（`LegalCore`）と、指し手の生成 |
| `fmvlegalcore_internal.h` | 成れる駒・敵陣・行き所のない駒・二歩の判定 |

#### 主要API

同じ局面で何度も判定するときは、局面を `Context` に一度だけ変換してから使う。盤の操作で駒の移動先を光らせる `BoardInteractionController::refreshLegalMoveHighlights()` は、1つの `Context` で81マスを判定している。1回だけ判定するときは、盤の配列と駒台を渡す関数を使う（中で毎回 `Context` を作る）。

| メソッド | 戻り値 | 説明 |
|---------|--------|------|
| `syncContext(ctx, turn, boardData, pieceStand)` | `bool` | 局面を `Context` に変換する |
| `isLegalMove(ctx, move)`<br/>`isLegalMove(turn, boardData, pieceStand, move)` | `LegalMoveStatus` | 指し手（`ShogiMove`）を成る場合と成らない場合のそれぞれで判定する |
| `legalMoves(ctx)`<br/>`legalMoves(turn, boardData, pieceStand)` | `QList<UsiMove>` | 合法手の一覧。成り・不成のどちらも指せる手は両方を入れる |
| `pseudoLegalMoves(ctx)`<br/>`pseudoLegalMoves(turn, boardData, pieceStand)` | `QList<UsiMove>` | 駒の動きだけで決めた指し手の一覧（自玉の安全・二歩・打ち歩詰め・行き所のない駒は見ない）。反則手を書いた棋譜を読み書きするときの候補にする |
| `generateLegalMoves(ctx)`<br/>`generateLegalMoves(turn, boardData, pieceStand)` | `int` | 合法手の数 |
| `checkIfKingInCheck(ctx)`<br/>`checkIfKingInCheck(turn, boardData)` | `int` | 手番の側の玉に利いている相手の駒の数（0=王手なし、2=両王手） |

`isLegalMove()` に渡す `ShogiMove` は0始まりの座標で、駒打ちは移動元を駒台の座標（筋は 9＝先手、10＝後手。段は駒の種類で決まる）にする。`movingPiece` は移動元の駒と、`capturedPiece`（空きでなければ）は移動先の駒と一致しなければならない。

#### LegalMoveStatus

`isLegalMove()` の戻り値は `LegalMoveStatus` 構造体で、「成り」と「不成」それぞれの合法性を表す。

```cpp
// src/core/legalmovestatus.h
struct LegalMoveStatus {
    bool nonPromotingMoveExists;  // 不成での合法手が存在するか
    bool promotingMoveExists;     // 成りでの合法手が存在するか
};
```

`ShogiGameController::validateAndMove()` は、両方が `true` なら対局者に成るかどうかを選ばせ（`setPromotionChooser()` で渡された関数）、成りだけが `true` なら成り、どちらも `false` なら着手を受け付けない。エンジンの手（`applyMove()`）は、指し手の成り・不成の側が `true` かを確かめる。

#### 判定の手順

```
isLegalMove(ctx, move)
  │
  ├── [1] Converter::toEngineMove()
  │     ShogiMove を内部の指し手（fmv::Move）にする。座標が盤外、手番の駒でない、
  │     持っていない駒を打つ、取る駒の指定が盤と違う、といった手はここで不合法。
  │     成れる駒で、移動元か移動先が敵陣なら「成る手もある」と記録する
  │
  ├── [2] 局面を写す（ctx.pos は書き換えない）
  │
  └── [3] LegalCore::checkMoveVariants()（成る手もあるとき）／ checkMove()
        手を1つだけ確かめる（checkMoveVariants は、成らないと行き所のない駒になる手なら
        成る手を、それ以外なら成らない手を代表にする）
        ├── isPseudoLegal()   駒の動き・移動先の自駒・二歩・行き所のない駒・成れるか
        ├── doMove()          写した局面で指す
        ├── ownKingInCheck()  自玉に相手の駒が利いていれば不合法（王手の放置・ピン）
        ├── isPawnDropMate()  歩を打って王手し、相手に合法手がなければ打ち歩詰め
        └── undoMove()        局面を戻す
```

成る場合と成らない場合で、駒のあるマスも自玉の位置も変わらないので、自玉の安全の確認は1回で両方に使える。代表の手が合法なら、成れる位置かどうかで `promotingMoveExists` を決める。

合法手の一覧（`legalMoves()`）は、王手されていれば玉の移動・王手した駒を取る手・合駒だけを、されていなければ全ての駒の移動と駒打ちを候補にして、候補ごとに上の [3] と同じく指して戻して確かめる。

#### 利きの計算

利きの表（各マスからの歩・桂・銀・金・玉の利き、馬・龍の1マスの利き、飛・角・香の筋、2マスの間のマス）は `constexpr` 関数でコンパイル時に作る。飛・角・香・馬・龍の利きは、筋の上にある駒と「2マスの間のマス」の表から、間に駒がないかで判定する。と金・成香・成桂・成銀は金と同じ利きを使う。

| 判定 | 関数 | 内容 |
|-----|------|------|
| マスに利いている駒 | `attackersTo()` | 指定の側の駒のうち、そのマスに利いている駒のビットボード（王手の数に使う） |
| マスに利きがあるか | `isSquareAttacked()` | 利いている駒を1枚見つけたところで終える（自玉の安全の確認に使う） |
| 駒が動けるか | `pieceAttacksSquare()` | 駒種・移動元・移動先と、間の駒から、その駒が移動先へ動けるか |

#### 主な合法性チェック

| チェック項目 | 実装箇所 | 説明 |
|-------------|---------|------|
| **王手の放置・ピン** | `LegalCore::ownKingInCheck()` | 指したあとの局面で自玉に相手の駒が利いていれば不合法 |
| **二歩** | `detail::hasPawnOnFile()` | 同じ筋に自分の歩（成っていない歩）があれば歩を打てない |
| **打ち歩詰め** | `LegalCore::isPawnDropMate()` | 歩を打って玉の正面から王手し、相手に合法手がなければ不合法 |
| **行き所のない駒** | `detail::isMandatoryPromotion()`, `detail::isDropDeadSquare()` | 歩・香は1段目、桂は1〜2段目（後手は逆）へ成らずに動けず、打てない |

### 5.5 ShogiClock — 対局時計

`ShogiClock` は対局中の時間管理を行う。持ち時間制・秒読み制・フィッシャー制の3モードに対応し、着手ごとの考慮時間の記録と「待った」による巻き戻しも提供する。時間の決まり（`TimeControl`）と残り時間の両方を時計が持ち、エンジンへの `go` の時間も時計の値から求める（[時間設定の仕様](game-time-control-spec.md) の「時間の決まりの持ち方」）。

**ソース**: `src/core/timecontrol.h`, `src/core/shogiclock.h`, `src/core/shogiclock.cpp`

#### 時間の決まり（TimeControl）

```cpp
struct TimeControlSide {          // 片方の対局者の決まり（ms）
    qint64 mainMs = 0;            // 持ち時間
    qint64 byoyomiMs = 0;         // 秒読み
    qint64 incrementMs = 0;       // 1手ごとの加算
    qint64 delayMs = 0;           // CSA の Delay（通信対局だけ）
};
struct TimeControl {
    TimeControlSide black, white;
    bool isUnlimited() const;     // 双方とも全項目0
    bool usesByoyomi() const;
    TimeControl normalized() const;  // 秒読みと加算の排他（秒読み優先）
};
```

対局ダイアログの値も、CSA の対局条件（`Game_Summary`）も、この値にしてから `ShogiClock::setTimeControl()` に渡す。

#### 3つの時間制モード

| モード | 設定条件 | 動作 |
|--------|---------|------|
| **持ち時間制** | `mainMs > 0`, `byoyomiMs = 0`, `incrementMs = 0` | 持ち時間を消費し、0になった時点で時間切れ負け |
| **秒読み制** | `byoyomiMs > 0` | 持ち時間消費後、毎手 byoyomi 秒の秒読みに移行。秒読み内に着手すればリセット |
| **フィッシャー制** | `incrementMs > 0` | 開始時と着手確定時に加算される。持ち時間が0以下で時間切れ |

秒読みとフィッシャーは排他的であり、`normalized()` が両方の指定を秒読みだけにする。

#### 主なAPI（player は `Turn::Black`／`Turn::White`）

| API | 用途 |
|-----|------|
| `setTimeControl(tc)` | 対局の時間を設定する。初手の加算を与え、持ち時間0の秒読みは秒読みに入った状態で始める。「待った」用の履歴を開始状態から積み直す |
| `setRemainingTimes(blackMs, whiteMs)` | 決まりと履歴を保ったまま、残り時間だけを合わせる（CSA通信対局の着手ごとの同期） |
| `commitMove(player)` | 着手を確定する。消費時間を総消費へ加え、秒読みの再設定または加算を行い、次の手番の開始状態を履歴に積む |
| `remainingMs(player)` / `byoyomiMs(player)` / `incrementMs(player)` | 残り時間と決まりの問い合わせ |
| `timeString(player)` / `considerationAndTotalTime(player)` | 時計と棋譜の消費時間の表記 |
| `pauseAndSnapshot()` / `restoreSnapshot()` | 中断・再開用に状態（決まり・残り時間・履歴）を保存・復元する |

#### 時間管理の仕組み

```
startClock()
  │
  ├── QTimer (50ms間隔) → updateClock() を定期呼び出し
  │
  └── QElapsedTimer でモノトニック時刻を計測
        │
        updateClock()
          ├── delta = 現在時刻 - m_lastTickMs
          ├── 手番側の remainingMs -= delta、considerationMs += delta
          ├── 残り時間 ≤ 0 なら
          │     ├── 秒読みあり → 秒読みへ移行（byoyomiApplied = true）
          │     └── 秒読みなし → emit playerTimeOut(手番の側) → 時間切れ処理
          └── emit timeUpdated()              (表示更新通知)
```

`QTimer`（50ms刻み）と `QElapsedTimer`（モノトニッククロック）を組み合わせることで、システム時刻変更の影響を受けない精密な時間計測を実現している。先後の状態は `SideState`（残り時間・考慮時間・総消費時間・秒読み突入・表示用の秒）の2要素の配列で持ち、先後で同じ処理を書き分けない。

#### 着手確定時の処理

```
commitMove(player)
  │
  ├── 考慮時間を確定: totalConsiderationMs += considerationMs
  │
  ├── 秒読みモード → remainingMs = byoyomiMs   (秒読みリセット)
  ├── フィッシャーモード → remainingMs += incrementMs  (加算。使い切った後は加算しない)
  │
  └── pushHistory()  ← 次の手番の開始状態を「待った」用に積む
```

考慮時間は、次にその対局者の手番が始まるとき（`setCurrentPlayer()`）に0へ戻す。

#### undo（待った）

`ShogiClock` は手番の開始状態を `QList<HistoryEntry>` に積んでおり、`undo()` で末尾の2つ（自分と相手の1手ずつ）を取り除いて、新しい末尾の状態に戻す。各要素は先後の `SideState`（残り時間・直近手の考慮時間・総消費時間・秒読み突入・表示用の秒）を持つ。履歴は中断・再開の保存（`Snapshot`）にも含める。

#### シグナル

| シグナル | 接続先 | 用途 |
|---------|-------|------|
| `timeUpdated()` | TimeDisplayPresenter | 残り時間の表示更新 |
| `playerTimeOut(Turn)` | TimeControlController → `MatchCoordinator::handlePlayerTimeOut()` | 時間切れ通知（引数は時間が切れた側） |

### 5.6 PlayMode — 対局モード列挙

`PlayMode` はアプリケーション全体の現在の動作モードを表す列挙型（`enum class`）である。

**ソース**: `src/core/playmode.h`

```cpp
enum class PlayMode {
    NotStarted,               // 起動直後、対局未開始
    HumanVsHuman,             // 人間 vs 人間
    EvenHumanVsEngine,        // 平手 人間(先手) vs エンジン(後手)
    EvenEngineVsHuman,        // 平手 エンジン(先手) vs 人間(後手)
    EvenEngineVsEngine,       // 平手/駒落ち エンジン vs エンジン
    HandicapEngineVsHuman,    // 駒落ち エンジン(下手) vs 人間(上手)
    HandicapHumanVsEngine,    // 駒落ち 人間(下手) vs エンジン(上手)
    HandicapEngineVsEngine,   // 駒落ち エンジン vs エンジン
    AnalysisMode,             // 棋譜解析モード
    ConsiderationMode,        // 検討モード
    TsumiSearchMode,          // 詰将棋探索モード
    CsaNetworkMode,           // CSA通信対局モード
    PlayModeError             // エラー（不正な設定値の検出用）
};
```

この列挙型は game層・UI層・engine層の各所で参照され、「現在の対局形態がどれか」に応じて振る舞いを切り替えるキーとなる。たとえば、`MatchCoordinator` は着手後に次の手番がエンジンか人間かを `PlayMode` で判定し、エンジンへの思考開始指示や手番表示の切替を行う。

### 5.7 SfenUtils / ShogiNotation / KifMoveText — ユーティリティ

#### SfenUtils

**ソース**: `src/core/sfenutils.h`（ヘッダオンリー）

SFEN 文字列の小さな関数群。`normalizeStart()` は文字列 `"startpos"` を標準初期配置のSFEN文字列に展開し、既にSFEN形式であればそのまま返す。ほかに平手のSFEN（`hirateSfen()`）、平手かどうかの判定（`isHirateStart()`）、局面を比べるキー（`normalizeSfenKey()`、持ち駒を並べ直す）などがある。

```cpp
SfenUtils::normalizeStart("startpos")
// → "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
```

#### ShogiNotation

**ソース**: `src/core/shoginotation.h`（ヘッダオンリー）

棋譜の表記の対応表をまとめた名前空間である。対局の棋譜・読み筋・局面図・棋譜の読み書き・通信対局で同じ表を使う。

| 関数 | 説明 |
|------|------|
| `fileZenkaku(file)` | 筋番号（1〜9）→ 全角数字（"１"〜"９"） |
| `rankKanji(rank)` | 段番号（1〜9）→ 漢数字（"一"〜"九"） |
| `fileFromZenkaku(ch)` | 全角数字 → 1-9（それ以外は0） |
| `rankFromKanji(ch)` | 漢数字 → 1-9（それ以外は0） |
| `movePieceName(piece)` | 指し手の表記に使う駒の名前（成香・成桂・成銀は2文字） |
| `boardPieceName(piece)` | 局面図（BOD）の1文字の駒の名前（杏・圭・全） |
| `pieceFromName(name)` | 駒の名前（別表記も読む）→ 先手の駒 |
| `csaPieceCode(piece)` / `pieceFromCsaCode(code, side)` | CSA の駒種との相互変換 |

#### KifMoveText

**ソース**: `src/core/kifmovetext.h`（ヘッダオンリー）

`KifMoveText::format(mover, moving, move, previousTo)` が「▲７六歩(77)」「△同　銀(31)」「▲５五角打」の形の指し手を作る。`previousTo` が移動先と同じなら「同　」と書く。対局の棋譜・棋譜の読み込み・通信対局・定跡・詰将棋の読み筋で同じ書式を使う。

USI 形式への指し手変換は `ShogiMove::toUsiMove()` と `UsiMove::toString()`（`src/core/usimove.h`）が行う（5.3 を参照）。

### 5.8 core層の他層との関係

core層は他の全層から参照されるが、core層自身は他層を参照しない。

```
             ┌──────────┐
             │  UI 層   │
             │ views/   │
             │ widgets/ │
             └────┬─────┘
                  │ uses
    ┌─────────┐   │   ┌───────────┐
    │ game 層 │───┼───│ engine 層 │
    └────┬────┘   │   └─────┬─────┘
         │        │         │
         ▼        ▼         ▼
    ┌─────────────────────────────┐
    │         core 層             │
    │  ShogiBoard  ShogiMove      │
    │  EngineMoveValidator        │
    │  ShogiClock  PlayMode       │
    │  ShogiNotation              │
    └─────────────────────────────┘
         ↑ 依存なし（QtCore のみ）
```

| 参照元 | 使用するcore部品 | 用途 |
|--------|-----------------|------|
| **game層** | ShogiBoard, ShogiMove, EngineMoveValidator, ShogiClock, PlayMode | 対局進行・合法手判定・時間管理 |
| **engine層** | ShogiMove, PlayMode | USI コマンド生成時の指し手変換 |
| **kifu層** | ShogiBoard, ShogiMove | 棋譜の読み込み・再生時に盤面を更新 |
| **analysis層** | PlayMode | 解析モードの判定 |
| **UI層** | ShogiBoard, ShogiClock | 盤面描画データの取得・時間表示 |
| **board層** | ShogiBoard, EngineMoveValidator | 盤面編集・インタラクション |

<!-- chapter-5-end -->

---

<!-- chapter-6-start -->
## 第6章 game層：対局管理

game層（`src/game/`）は対局のライフサイクル全体を管理する層である。対局開始の準備から、指し手の実行、手番の切替、終局判定、連続対局まで、対局に関わるすべてのフローをこの層のクラス群が担う。

### 6.1 game層の全体像

```
┌─────────────────────────────────────────────────────────────────┐
│                        game 層                                  │
│                                                                 │
│  ┌───────────────────────┐    ┌──────────────────────┐         │
│  │  GameStartCoordinator │───▶│  PreStartCleanup     │         │
│  │  (対局開始フロー)      │    │  Handler             │         │
│  └───────────┬───────────┘    │  (開始前クリーンアップ)│         │
│              │                └──────────────────────┘         │
│              ▼                                                  │
│  ┌───────────────────────┐    ┌──────────────────────┐         │
│  │  MatchCoordinator     │◀──▶│  ShogiGameController │         │
│  │  (司令塔)             │    │  (盤面・ルール管理)   │         │
│  └───────────┬───────────┘    └──────────┬───────────┘         │
│              │                           │                      │
│     ┌────────┼────────┐                  │                      │
│     ▼        ▼        ▼                  ▼                      │
│  ┌──────┐┌──────┐┌──────────────┐ ┌──────────────┐             │
│  │ Usi  ││Clock ││GameState     │ │PromotionFlow │             │
│  │(E/G) ││(時計)││Controller    │ │(成/不成選択) │             │
│  └──────┘└──────┘│(終局処理)    │ └──────────────┘             │
│                  └──────────────┘                               │
│                                                                 │
│  ┌──────────────────────┐    ┌──────────────────────┐          │
│  │  TurnManager         │◀──▶│  TurnSyncBridge      │          │
│  │  (手番の単一ソース)   │    │  (手番同期配線)       │          │
│  └──────────────────────┘    └──────────────────────┘          │
│                                                                 │
│  ┌──────────────────────────────┐                               │
│  │  ConsecutiveGamesController  │                               │
│  │  (連続対局制御)              │                               │
│  └──────────────────────────────┘                               │
└─────────────────────────────────────────────────────────────────┘
```

| クラス | ソース | 責務 |
|--------|--------|------|
| **MatchCoordinator** | `matchcoordinator.h/.cpp` | 対局進行の司令塔。エンジン通信・時計・棋譜記録・終局判定を統括 |
| **ShogiGameController** | `shogigamecontroller.h/.cpp` | 盤面状態の管理、指し手の合法性検証と適用 |
| **GameStartCoordinator** | `gamestartcoordinator.h/.cpp` | 対局開始フローのオーケストレーション |
| **PreStartCleanupHandler** | `app/prestartcleanuphandler.h/.cpp` | 対局開始前のUI/状態クリーンアップ（UI の部品を扱うので `app/` に置く） |
| **GameStateController** | `gamestatecontroller.h/.cpp` | 終局処理・投了/中断・人間/エンジン判定 |
| **TurnManager** | `turnmanager.h/.cpp` | 手番の単一ソース（Single Source of Truth） |
| **TurnSyncBridge** | `turnsyncbridge.h/.cpp` | GC ↔ TurnManager ↔ UI の手番同期配線 |
| **PromotionFlow** | `ui/coordinators/promotionflow.h/.cpp` | 成り/不成の選択ダイアログ表示（ダイアログを出すので `ui/coordinators/` に置く） |
| **ConsecutiveGamesController** | `consecutivegamescontroller.h/.cpp` | 連続対局の進行管理と手番入れ替え |

### 6.2 MatchCoordinator — 司令塔

`MatchCoordinator` は対局全体のライフサイクルを管理するハブクラスであり、game層の中核を成す。`MatchCoordinatorWiring` が `std::unique_ptr` で持ち、起動時に一度だけ作る。作り直さないので、`MainWindow::m_match` は終了まで同じ司令塔を指す（[14.4節](#144-コンストラクタの処理フロー)）。

**ソース**: `src/game/matchcoordinator.h`, `src/game/matchcoordinator.cpp`

#### 司令塔の責務

| 責務 | 内容 |
|------|------|
| **対局開始** | `configureAndStart()` でモード別の初期化と開始処理を実行 |
| **エンジン管理** | USIエンジンの生成・初期化・破棄（`initializeAndStartEngineFor()`, `destroyEngines()`） |
| **手番管理** | 内部手番 `m_cur`（`Turn`）の管理と表示更新 |
| **時間管理** | ShogiClock との連携、USI go コマンド用の時間計算 |
| **指し手進行** | 人間の着手後の処理（`onHumanMove()`）とエンジンの初手（`startInitialEngineMoveIfNeeded()`）を、`MatchTurnHandler` を通して対局モードの Strategy（`HumanVsHumanStrategy`・`HumanVsEngineStrategy`・`EngineVsEngineStrategy`）に渡す。エンジンの手は Strategy が `Usi::matchMoveReady` で受け取り、`ShogiGameController::applyMove()` で指す |
| **終局判定** | 投了・時間切れ・中断・入玉宣言・持将棋の各終局パターンの処理 |
| **棋譜記録** | 着手ごとの棋譜行追記、評価値追記の委譲 |
| **UNDO** | 2手分（自分＋相手）の巻き戻し |
| **検討モード** | 検討/詰み探索エンジンの開始・停止・MultiPV変更 |

#### 内部型定義

MatchCoordinator は多くの内部型を定義しており、対局に関する設定・状態・コールバックを構造化している。終局原因・終局情報・終局状態は `src/game/matchtypes.h` にあり、`MatchCoordinator::Cause` などはその別名である。対局者（先手・後手）は、盤や対局の進行と同じ `Turn`（`Turn::Black`／`Turn::White`）で表す。

```cpp
// 終局原因
enum class Cause : int {
    Resignation    = 0,  // 投了
    Timeout        = 1,  // 時間切れ
    BreakOff       = 2,  // 中断
    Jishogi        = 3,  // 持将棋／最大手数到達
    NyugyokuWin    = 4,  // 入玉宣言勝ち
    IllegalMove    = 5,  // 反則負け（入玉宣言失敗など）
    Sennichite     = 6,  // 千日手（引き分け）
    OuteSennichite = 7   // 連続王手の千日手（反則負け）
};
```

エンジンの投了（`handleEngineResign`）・入玉宣言（`handleEngineWin`）・反則（`handleEngineIllegalMove`）も、エンジンの手番（`Turn`）で受ける。人間対エンジンでは、エンジンが後手でもエンジンは1つ目の枠（`primaryEngine()`、`EngineLifecycleManager` の `usi1`）に入る。枠は `EngineLifecycleManager` の中だけの区別で、投了・勝ち宣言の配線（`wireResignToArbiter(engine, side)`）は枠ではなくエンジンの手番で行う。

**StartOptions** — 対局開始のための設定パラメータ:

| フィールド | 説明 |
|-----------|------|
| `mode` | 対局モード（`PlayMode`） |
| `sfenStart` | 開始局面のSFEN文字列 |
| `engineName1/2`, `enginePath1/2` | エンジンの表示名と実行パス（人間対エンジンでは、エンジンが先手なら1、後手なら2を使う） |
| `maxMoves` | 最大手数（0＝無制限） |
| `autoSaveKifu`, `kifuSaveDir` | 棋譜自動保存の設定 |
| `humanName1/2` | 人間対局者名 |

**GameEndInfo** — 終局情報:

```cpp
struct GameEndInfo {
    Cause cause = Cause::Resignation;   // 終局原因
    Turn  loser = Turn::Black;          // 敗者
};
```

**GameOverState** — 直近の終局状態:

```cpp
struct GameOverState {
    bool        isOver       = false;   // 終局済みか
    bool        moveAppended = false;   // 棋譜に終局手を追記済みか
    bool        hasLast      = false;   // 直近の終局情報があるか
    GameEndInfo lastInfo;               // 直近の終局情報（敗者は lastInfo.loser）
    QDateTime   when;                   // 終局日時
};
```

`moveAppended` フラグは棋譜への終局手（「投了」「中断」等）の二重追記を防ぐガードとして機能する。

#### Hooks — コールバック注入パターン

MatchCoordinator は UI 層やその他の層への依存を `Hooks` 構造体に定義された `std::function` コールバック群で解決する。これにより司令塔自体は UI の具象型に依存しない。

```cpp
struct MatchCoordinatorHooks {   // MatchCoordinator::Hooks はこの型の別名
    struct UI {
        std::function<void(Turn cur)> updateTurnDisplay;                        // 手番表示更新
        std::function<void(const QString&, const QString&)> setPlayersNames;    // 対局者名設定
        std::function<void(const QString&, const QString&)> setEngineNames;     // エンジン名設定
        std::function<void()> renderBoardFromGc;                                // GC→盤面ビューの反映
        std::function<void(const QString&, const QString&)> showGameOverDialog; // 終局ダイアログ
        std::function<bool()> usesHandicapNames;                                // 下手・上手と呼ぶか
        std::function<void(const QPoint&, const QPoint&)> showMoveHighlights;   // 着手のハイライト
    };
    struct Game {
        std::function<void(const QString&)> initializeNewGame;                  // 新規対局GUI初期化
        std::function<void(const QString&, const QString&)> appendKifuLine;     // 棋譜行追記
        std::function<void(Turn)> appendEval;                                   // エンジンの手のあとに、その手番の評価値を追記
        std::function<void(const QString&)> autoSaveKifu;                       // 棋譜自動保存
    };
    UI ui;
    Game game;
};
```

エンジンへの送信（stop など）はフックに置かない。`MatchTurnHandler` などの子ハンドラには、司令塔が自分のメソッド（`sendStopToEngine()` など）を渡す。

Hooks はコンストラクタで `Deps` 構造体経由で注入される。`MainWindowServiceRegistry::buildMatchHooks()` が組み立て、`MatchCoordinatorWiring` が司令塔を作るときに渡す。司令塔は一度だけ作るので、各フックは組み立てたときのポインタを写さず、呼ばれた時点の部品を読む。

戻り値のない通知（`updateTurnDisplay`・`showMoveHighlights`・`appendEval` など）も、シグナルにせずフックのままにしている（2026-10-14 に検討）。理由は次の3つである。

- どのフックも受け手が1つで、呼んだ場所で処理が終わっていることを前提に次へ進む（[Deps/Hooks/Refs パターン](done/deps-hooks-refs-pattern.md) の 6.2。複数の受け手に知らせるものはシグナルにする）
- フックの多くは、QObject でない子（`MatchTurnHandler`・`GameStartOrchestrator`・各 Strategy）がそのまま受け取って呼ぶ。シグナルにしても、子に渡す関数は残る
- 受け手の側に、引数の合うスロットがないものが多い（`MainWindowMatchAdapter` は QObject でない。`appendEval` は手番でグラフを選ぶ）。`connect()` にラムダを使わない規約のもとでは、スロットを足す分だけコードが増える

> **Note**: `Hooks` の `std::function` はシグナル/スロットの `connect()` とは異なるコールバック注入パターンである。CLAUDE.md の「connect() でラムダを使わない」ルールは `connect()` の話であり、Deps/Hooks 構造体への `std::function` 注入は許容されている。

#### Deps — 依存オブジェクト

```cpp
struct Deps {
    ShogiGameController* gc = nullptr;    // ゲームコントローラ
    ShogiClock*          clock = nullptr; // 将棋時計
    Usi*                 usi1 = nullptr;   // エンジン1
    Usi*                 usi2 = nullptr;   // エンジン2
    UsiCommLogModel*     comm1 = nullptr;  // エンジン1通信ログ
    ShogiEngineThinkingModel* think1 = nullptr; // エンジン1思考情報
    UsiCommLogModel*     comm2 = nullptr;  // エンジン2通信ログ
    ShogiEngineThinkingModel* think2 = nullptr; // エンジン2思考情報
    Hooks                hooks;            // UIコールバック群
    QStringList*         sfenRecord = nullptr; // SFEN履歴（共有）
    QList<ShogiMove>*    gameMoves = nullptr;  // 指し手列（共有）
};
```

すべてのポインタは非所有（non-owning）であり、寿命は MainWindow 側で管理される。

#### configureAndStart() — モード別起動ルーティング

`configureAndStart()` は `StartOptions` を受け取り、対局モードに応じて適切な起動処理に分岐する中核メソッドである。実体は `GameStartOrchestrator::configureAndStart()` にあり、モード別の起動は `MatchTurnHandler::createAndStartModeStrategy()` が対局モードの Strategy を作って `start()` する。

```
configureAndStart(opt)                       GameStartOrchestrator
  │
  ├── 1. syncAndSearchGameHistory()      直前の position 履歴を保存し、過去の対局から開始局面の一致を探す
  ├── 2. applyStartOptionsAndHooks()
  │     ├── 終局状態をクリア、対局モード・最大手数・棋譜自動保存を設定
  │     ├── Hooks 経由で GUI 初期化（initializeNewGame・setPlayersNames・setEngineNames）
  │     └── SFEN から開始手番を決めて GC と内部手番 m_cur に設定し、手番表示を更新
  ├── 3. buildPositionStringsFromHistory()  position 文字列の初期化
  │
  └── 4. createAndStartModeStrategy(opt)    MatchTurnHandler
        ├── EvE → initEnginesForEvE() → エンジン2つを起動 → EngineVsEngineStrategy
        ├── EvH → HumanVsEngineStrategy(エンジンの手番 = Turn::Black, enginePath1)
        ├── HvE → HumanVsEngineStrategy(エンジンの手番 = Turn::White, enginePath2)
        └── HvH → HumanVsHumanStrategy
```

各起動ルートの処理内容（Strategy の `start()`）:

| 起動ルート | Strategy | 処理内容 |
|-----------|---------|---------|
| **HvH** | `HumanVsHumanStrategy` | GC の手番を内部手番に写し、盤面描画と手番表示を更新するだけ |
| **HvE / EvH** | `HumanVsEngineStrategy` | 既存エンジン破棄→USI生成（手番によらず usi1）→投了・宣言勝ちをエンジンの手番で配線→エンジン起動→手番設定 |
| **EvE** | `EngineVsEngineStrategy` | 時計開始→手番設定→position 文字列の初期化→手番の側のエンジンへ go を送信 |

#### 主要シグナル

接続先は、司令塔を作るときに `MatchCoordinatorWiring::connectMatchSignals()` が直接つなぐ（`timeUpdated` だけは `bindRuntimeDependencies()`）。接続先が複数あるものは、表に書いた順に呼ばれる。

| シグナル | 接続先 | 用途 |
|---------|-------|------|
| `gameEnded(GameEndInfo)` | SessionLifecycleCoordinator（→ GameStateController::onMatchGameEnded）、続いて UiStatePolicyManager（アイドルへ） | 終局通知 |
| `gameEndProcessed(GameEndInfo)` | SessionLifecycleCoordinator（EvE の連続対局の次局） | 終局表示・自動保存の完了通知 |
| `gameOverStateChanged(GameOverState)` | GameStateController | 終局状態変更通知 |
| `timeUpdated(blackMs, whiteMs, sideToMove, urgencyMs)` | TimeDisplayPresenter（時計表示） | 残り時間の定期更新 |
| `boardFlipped(bool)` | MainWindowAppearanceController | 盤面反転通知 |
| `requestAppendGameOverMove(GameEndInfo)` | GameStateController、続いて SessionLifecycleCoordinator（ライブセッションの確定） | 終局手の棋譜追記要求 |
| `gameInterrupted()` | GameSessionOrchestrator | 中断した対局を再開できるようにする |
| `considerationModeEnded()` | UiStatePolicyManager ほか（候補手の矢印、検討モードのUI） | 検討モード終了通知 |
| `tsumeSearchModeEnded()` | UiStatePolicyManager | 詰み探索終了通知 |

### 6.3 対局開始シーケンス

対局開始は `GameStartCoordinator` → `PreStartCleanupHandler` → `MatchCoordinator` の3段階で進行する。

```mermaid
sequenceDiagram
    participant UI as MainWindow / StartGameDialog
    participant GSC as GameStartCoordinator
    participant PCH as PreStartCleanupHandler
    participant MC as MatchCoordinator
    participant GC as ShogiGameController
    participant Clock as ShogiClock
    participant USI as Usi (Engine)

    UI->>GSC: prepare(Request)
    GSC-->>PCH: requestPreStartCleanup [signal]
    PCH->>PCH: performCleanup()
    Note over PCH: 盤面クリア, 棋譜モデルリセット,<br/>時計リセット, 評価値グラフ初期化,<br/>分岐ツリーのルート作成,<br/>ライブゲームセッション開始

    GSC-->>UI: requestApplyTimeControl(tc) [signal]
    UI->>Clock: setTimeControl(tc.rules)

    UI->>GSC: start(StartParams)
    GSC->>MC: configureAndStart(opt)

    MC->>MC: clearGameOverState()
    MC->>GC: newGame(sfenStart) [via Hooks.initializeNewGame]
    MC->>GC: setCurrentPlayer(startSide)

    alt HvH
        MC->>MC: HumanVsHumanStrategy::start()
    else HvE / EvH
        MC->>MC: HumanVsEngineStrategy::start()（エンジンの手番を受け取る）
        MC->>USI: new Usi(...)（usi1）
        MC->>USI: initializeAndStartEngineFor(エンジンの手番, path, name)
    else EvE
        MC->>USI: initEnginesForEvE()
        MC->>MC: EngineVsEngineStrategy::start()
        MC->>Clock: startClock()
        MC->>USI: go (手番の側のエンジンへ)
    end

    GSC->>MC: startInitialEngineMoveIfNeeded()
    Note over MC: 初手がエンジン手番なら go を発行

    GSC-->>UI: started(opt) [signal]
    Note over UI: ナビゲーション無効化、<br/>対局中UIに切替
```

### 6.4 GameStartCoordinator — 対局開始コーディネータ

`GameStartCoordinator` は対局開始フロー全体を司るコーディネータである。

**ソース**: `src/game/gamestartcoordinator.h`, `src/game/gamestartcoordinator.cpp`

#### 責務

- `StartOptions` を受け取り、`MatchCoordinator` へ対局開始を指示する
- 時計の初期化をシグナルで UI 層に依頼する（自身は `ShogiClock` API を直接呼ばない）
- 対局開始前の UI/内部状態クリアをシグナルで通知する
- 初手がエンジン手番の場合に go をトリガする

#### 主要な型定義

**StartParams** — 対局開始パラメータ:

```cpp
struct StartParams {
    MatchCoordinator::StartOptions opt;  // 対局設定
    TimeSettings                   tc;   // 時計設定
    bool autoStartEngineMove = true;     // 先手がエンジンなら初手 go を起動
};
```

**TimeSettings** — 時間設定:

```cpp
struct TimeSettings {
    bool        enabled = false;        // 時間制御が有効か
    TimeControl rules;                  // 先後の決まり（src/core/timecontrol.h、5.5 を参照）
    bool        loseOnTimeout = true;   // 時間切れを負けにするか
};
```

**Ctx** — 段階実行 API 用のコンテキスト:

`Ctx` は `initializeGame()` や `setTimerAndStart()` などの段階実行 API に渡される。GC、時計、ダイアログから取り出した値（`dialogData`）、SFEN文字列・局面列・棋譜欄モデルへの参照をまとめて保持する。「現在の局面から開始」のケースは `selectedPly`（表示中の手数）で扱う。棋譜読み込み担当（`KifuLoadCoordinator`）は持たない。平手・駒落ちから始めるときの分岐ツリーのリセットは `requestBranchTreeResetForNewGame` シグナルで依頼し、`BranchNavigationWiring::onBranchTreeResetForNewGame()` が行う。

#### API

| メソッド | 説明 |
|---------|------|
| `prepare(Request)` | 開始前の時計/表示ポリシーを適用。`requestPreStartCleanup` シグナルを発行 |
| `start(StartParams)` | バリデーション→TimeControl正規化→時計適用→`configureAndStart()`→完了通知 |
| `initializeGame(Ctx)` | ダイアログ表示→対局開始までの一連のフロー |
| `setTimerAndStart(Ctx)` | 時計を設定し対局を開始 |

`GameStartCoordinator` は1つだけ存在する。最初に必要になったとき（`MainWindowServiceRegistry::ensureGameStartCoordinator()` から）`MatchCoordinatorWiring::ensureMenuGameStartCoordinator()` が作り、親は `MatchCoordinatorWiring` である。司令塔は作成時に `Deps::match` で受け取る。

`start()` メソッドの時間制御正規化では、秒読みとフィッシャーが同時に指定された場合に秒読みを優先してフィッシャー加算を0にクリアする。

### 6.5 PreStartCleanupHandler — 対局開始前クリーンアップ

`PreStartCleanupHandler` は対局開始前に UI とデータモデルを初期状態にリセットする。

**ソース**: `src/app/prestartcleanuphandler.h`, `src/app/prestartcleanuphandler.cpp`

#### 接続方法

```
GameStartCoordinator::requestPreStartCleanup [signal]
    └──▶ PreStartCleanupHandler::performCleanup() [slot]
```

#### performCleanup() の処理フロー

```
performCleanup()
  │
  ├── 現在の SFEN・選択手数・選択ノードを一時保存
  │    選択手数と選択ノードは分岐ツリーのカーソル（navState->currentPly() / currentNode()）から取る
  │    （棋譜欄を作り直すと行選択の通知でカーソルが動くので、最初に控えた値を以降の判定に使う）
  │
  ├── resolveTerminalStartNode()
  │     終局手（投了など）を選んでいたら直前の局面を開始位置にする（末尾の「中断」は取り除く）
  │
  ├── clearBoardAndHighlights()
  │     盤面のハイライトをすべて消去
  │
  ├── clearClockDisplay()
  │     時計表示を "00:00:00" にリセット
  │
  ├── cleanupKifuModel(startFromCurrentPos)
  │     ├── 途中局面から開始 → 分岐ツリーのパスを辿り棋譜を再構築（保持した最後の行を返す）
  │     └── 新規対局          → 棋譜モデルを全消去し「=== 開始局面 ===」を追加
  │
  ├── 新規対局なら navState->goToRoot()
  │     表示中の手数（カーソル）を開始局面へ戻す。途中局面から始めるときは選んでいた局面のまま
  │
  ├── clearBranchModel()
  │     分岐モデルをクリア
  │
  ├── resetUsiLogModels()
  │     エンジン通信ログ・思考情報・予測手等をすべて初期化
  │
  ├── resetTimeControl()
  │     時間制御コントローラの開始時刻をクリア
  │
  ├── resetEvaluationGraph()
  │     ├── 新規対局          → 評価値グラフを全消去
  │     └── 途中局面から開始 → 指定手数でトリム
  │
  ├── selectStartRow()
  │     棋譜欄の先頭行を選択（黄色ハイライト）
  │
  ├── ensureBranchTreeRoot()
  │     分岐ツリーのルートノードが存在しなければ作成
  │
  └── startLiveGameSession()
        ├── 途中局面から → 保存したノードを分岐点としてセッション開始
        └── 新規対局     → ルートからセッション開始
        （sessionStarted を受けた KifuDisplayCoordinator がカーソルを分岐点かルートに合わせる）
```

対局の局面列・指し手列（`sfenRecord` / `gameMoves`）はここでは作らない。このハンドラより前に、`GameSessionOrchestrator::initializeGame()` が `MainWindowGameStartService::prepareForInitializeGame()` で表示中の手順（`KifuNavigationState::currentRecordLine()`）から作り直す（[対局開始フロー](game-start-flow.md) の 2 章）。

#### 「現在の局面から開始」の判定

```cpp
static bool shouldStartFromCurrentPosition(
    const QString& startSfen,
    const QString& currentSfen,
    int selectedPly)
{
    // currentSfen が空なら false
    // startSfen が空なら true（何らかの棋譜が読み込まれている）
    // selectedPly > 0 かつ currentSfen != "startpos" なら true
}
```

この判定により、棋譜を途中まで進めた局面から対局を開始する場合と、初手から新規に開始する場合を区別する。

### 6.6 ShogiGameController — 盤面とルールの橋渡し

`ShogiGameController` は `ShogiBoard`（盤面状態）と `EngineMoveValidator`（合法性検証）を統合し、指し手の実行を管理するコントローラである。

**ソース**: `src/game/shogigamecontroller.h`, `src/game/shogigamecontroller.cpp`

#### 指し手実行フロー

人間の着手（盤のクリック。定跡ウィンドウの手もこちら）では `validateAndMove()`、成り・不成まで決まったエンジンの手では `applyMove()` が呼ばれる。どちらも合法手であることを確かめてから `commitMove()` で指す。

```
validateAndMove(outFrom, outTo, record, moveNumber, sfenHistory, gameMoves)
  │
  ├── 1. クリックした2マス（表示の座標。駒台は筋 10・11）を UsiMove にする
  │     盤外・相手の駒台・空きマスからの手は受け付けない
  │
  ├── 2. legalStatus() — EngineMoveValidator::isLegalMove() で成り・不成それぞれを判定
  │
  ├── 3. 成るかどうかを決める
  │       ├── 両方とも合法 → setForcedPromotion() の指定（定跡の手）か、
  │       │                  setPromotionChooser() の関数（成りダイアログ）で決める
  │       ├── 成りだけ合法（行き所のない駒）→ 成る
  │       └── どちらも不合法 → false を返す
  │
  └── 4. commitMove()

applyMove(move, record, moveNumber, sfenHistory, gameMoves)
  ├── legalStatus() で、指し手の成り・不成の側が合法かを確かめる（反則手は盤を変えずに false）
  └── commitMove()

commitMove()
  ├── board()->applyUsiMove(move, mover)    盤を更新
  ├── gameMoves に ShogiMove を追加
  ├── board()->addSfenRecord(...)            SFEN の履歴に次の局面を追加
  ├── record = convertMoveToKanjiStr(...)    棋譜の文字列（前の手と同じマスなら「同」）
  ├── emit moveCommitted(mover, ply)         手番を切り替える「前」に出す
  └── setCurrentPlayer(次の手番)              emit currentPlayerChanged(次の手番)
```

#### 主要なシグナル

| シグナル | 接続先 | 用途 |
|---------|-------|------|
| `currentPlayerChanged(Turn)` | TurnManager（TurnSyncBridge が接続）、BoardInteractionController | 手番変更通知。盤の操作は1回目のクリックを取り消す |
| `moveCommitted(Turn, int)` | MainWindow::onMoveCommitted、PieceSoundPlayer | 着手確定通知（棋譜・表示の更新と駒音） |
| `endDragSignal()` | ShogiView::endDrag | 駒のドラッグ表示を終える |

### 6.7 TurnManager / TurnSyncBridge — 手番管理

#### TurnManager — 手番の単一ソース

`TurnManager` は手番の「Single Source of Truth」として機能し、異なるサブシステム間の手番表現を変換する。

**ソース**: `src/game/turnmanager.h`, `src/game/turnmanager.cpp`

```
  set(Turn) / setFromSfenToken("b"/"w") / toggle()
                    │
                    ▼
             ┌──────────────┐     side()          → Turn
             │ TurnManager  │──── toSfenToken()   → "b" / "w"
             │ m_side(Turn) │
             └──────┬───────┘
                    │ changed(Turn) [signal]
                    ▼
       MainWindow::onTurnManagerChanged()
```

各変換メソッド:

| メソッド | 方向 | 変換内容 |
|---------|------|---------|
| `set(Turn)` | → TM | 手番を設定する（`ShogiGameController::currentPlayerChanged` から） |
| `side()` | TM → | 今の手番（`Turn`）を返す |
| `setFromSfenToken("b"/"w")` | SFEN → TM | SFEN の手番文字から設定 |
| `toSfenToken()` | TM → SFEN | `"b"` または `"w"` を返す |
| `toggle()` | — | 手番を反転（先手 ↔ 後手） |

`set()` は後手以外を先手にそろえ、値が変わったとき（と最初の設定のとき）だけ `changed()` シグナルを発行する。

#### TurnSyncBridge — 手番同期の配線

`TurnSyncBridge` は GC・TurnManager・MainWindow 間の手番同期を一箇所に集約する静的ユーティリティクラスである。

**ソース**: `src/game/turnsyncbridge.h`, `src/game/turnsyncbridge.cpp`

```cpp
// TurnSyncBridge::wire() が構築する接続
GC::currentPlayerChanged ──▶ TurnManager::set
TurnManager::changed     ──▶ MainWindow::onTurnManagerChanged
```

`wire()` は `Qt::UniqueConnection` を使用して重複接続を防止し、呼び出し時に初期同期（`tm->set(gc->currentPlayer())`）も行う。

#### 人間/エンジンの手番切替

人間とエンジンの手番切替は、`MatchCoordinator` が対局モードごとに作る Strategy（`HumanVsHumanStrategy`・`HumanVsEngineStrategy`・`EngineVsEngineStrategy`）が制御する。

```
                         対局中の指し手進行
                              │
                   ┌──────────┴──────────┐
                   │                     │
              人間の着手              エンジンの着手
                   │                     │
          validateAndMove()     Strategy::onEngineMoveReady()
                   │               → applyMove()
                   └──────────┬──────────┘
                              │
                         commitMove()
                              │
                  setCurrentPlayer(次の手番)
                              │
                  GC::currentPlayerChanged
                              │
                     TurnManager::set()
                              │
             次の手番は誰か？（Strategy が判定）
                              │
                   ┌──────────┴──────────┐
                   │                     │
              人間の手番            エンジンの手番
                   │                     │
          盤面クリック待ち      position + go 送信
          タイマー起動          bestmove（Usi::matchMoveReady）待ち
```

### 6.8 PromotionFlow — 成り/不成の選択

`PromotionFlow` は駒の成り/不成をユーザーに問い合わせるシンプルなユーティリティクラスである。

**ソース**: `src/ui/coordinators/promotionflow.h`, `src/ui/coordinators/promotionflow.cpp`

```cpp
class PromotionFlow final {
public:
    /// @return 成る: true / 不成: false
    static bool askPromote(QWidget* parent);
};
```

実装は `PromoteDialog`（成り/不成選択ダイアログ）を表示し、ユーザーの選択結果を返す。`QDialog::Accepted`（OK/成る）なら `true`、それ以外（Cancel/不成）なら `false` を返す。

このクラスは、`ShogiGameController::validateAndMove()` が成り・不成の両方とも合法な手で、成りの選び方の関数（`setPromotionChooser()` で渡す `DialogLaunchWiring::choosePromotion()`）を呼んだときに、`DialogCoordinator::showPromotionDialog()` から呼ばれる（6.6 節）。以下の場合はダイアログを表示しない:

- **行き所のない駒**: 成りだけが合法な手（歩・香が1段目、桂が1〜2段目へ動く手。後手は逆）は成る
- **成り・不成の指定**: 定跡ウィンドウの手は、`setForcedPromotion()` で事前に成り・不成が決まっている

### 6.9 GameStateController — ゲーム状態管理

`GameStateController` は終局処理・投了/中断・人間/エンジン判定を担うコントローラである。MainWindow から終局関連の処理を分離する目的で作られた。

**ソース**: `src/game/gamestatecontroller.h`, `src/game/gamestatecontroller.cpp`

#### 責務

| 責務 | 内容 |
|------|------|
| **投了処理** | `handleResignation()` → `MatchCoordinator::handleResign()` に委譲 |
| **中断処理** | `handleBreakOffGame()` → `MatchCoordinator::handleBreakOff()` に委譲 |
| **終局後処理** | 棋譜追記、UIの閲覧モードへの遷移、矢印ボタン有効化 |
| **人間/エンジン判定** | `isHvH()`, `isHumanSide(Turn)` を `PlayMode` から判定 |
| **終局判定** | `isGameOver()` で `MatchCoordinator` の状態を参照 |

#### シグナル接続

```
MatchCoordinator::gameEnded
    └──▶ SessionLifecycleCoordinator::handleGameEnded()
            └──▶ GameStateController::onMatchGameEnded()
                    ├── gameOverStyleLock を有効化
                    ├── タイマー停止・マウス操作無効化
                    ├── setGameOverMove() で棋譜追記
                    └── 矢印ボタン有効化

MatchCoordinator::gameOverStateChanged
    └──▶ GameStateController::onGameOverStateChanged()
            ├── 終局スタイルロックを有効化（終局行の追記前に検知した時点で）
            ├── 終局行の追記を待ってから以降を行う
            ├── syncCursorToPly(棋譜欄の最終行) で表示中の手数（分岐ツリーのカーソル）を終局行へ
            ├── UIを閲覧モードへ遷移
            └── ハイライト消去

MatchCoordinator::requestAppendGameOverMove
    └──▶ GameStateController::onRequestAppendGameOverMove()
            └── setGameOverMove() で棋譜追記
```

#### コールバック設定

GameStateController は MainWindow への委譲を `Hooks` 構造体の `std::function` で行う。主なもの:

| フック | 用途 |
|-------------|------|
| `enableArrowButtons` | 矢印ボタン（前の手/次の手）の有効化 |
| `setReplayMode` | リプレイモードへの切替 |
| `syncCursorToPly(int)` | 表示中の手数（分岐ツリーのカーソル）を指定の手数に合わせる（`KifuNavigationCoordinator::syncNavStateToPly()`） |
| `updateBoardView` / `setGameOverStyleLock` / `setMouseClickMode` / `removeHighlightAllData` / `setKifuViewSingleSelection` | 盤面ビュー・棋譜欄の表示の切替 |

#### isHumanSide() の判定ロジック

```cpp
bool isHumanSide(Turn p) const {
    switch (m_playMode) {
    case HumanVsHuman:          return true;              // 両方人間
    case EvenHumanVsEngine:
    case HandicapHumanVsEngine: return (p == Turn::Black); // 先手のみ人間
    case EvenEngineVsHuman:
    case HandicapEngineVsHuman: return (p == Turn::White); // 後手のみ人間
    case EvenEngineVsEngine:
    case HandicapEngineVsEngine:return false;              // 両方エンジン
    default:                    return true;
    }
}
```

### 6.10 ConsecutiveGamesController — 連続対局制御

`ConsecutiveGamesController` はエンジン同士（EvE）の連続対局を管理する。

**ソース**: `src/game/consecutivegamescontroller.h`, `src/game/consecutivegamescontroller.cpp`

#### 仕組み

```
GameStartCoordinator::consecutiveGamesConfigured(totalGames, switchTurn)
    └──▶ ConsecutiveGamesController::configure(totalGames, switchTurn)

MatchCoordinator::gameEndProcessed（終局表示・自動保存のあと）
    └──▶ SessionLifecycleCoordinator::handleGameEndProcessed()
            └──▶ EvE で shouldStartNextGame() ? → startNextGame()

startNextGame()
  │
  ├── prepareNextGameOptions()
  │     ├── m_remainingGames--
  │     ├── m_gameNumber++
  │     └── switchTurn → swap(engineName1, engineName2), swap(enginePath1, enginePath2)
  │
  ├── emit requestPreStartCleanup()
  │
  └── QTimer::singleShot(100ms)
        ├── TimeControlUtil::applyToClock(clock, lastTimeControl, ...)
        └── emit requestStartNextGame(lastStartOptions, lastTimeControl)
```

連続対局では `m_lastStartOptions` と `m_lastTimeControl` を保存しておき、次局開始時に再利用する。手番入れ替え（`m_switchTurnEachGame`）が有効な場合、`prepareNextGameOptions()` でエンジン1とエンジン2の名前・パスを `std::swap()` で入れ替える。

次局開始前に 100ms の遅延を入れるのは、UI更新（クリーンアップ結果の描画）を確実に完了させるためである。

| メソッド | 説明 |
|---------|------|
| `configure(totalGames, switchTurn)` | 合計対局数と手番入替の設定を受け取る |
| `onGameStarted(opt, tc)` | 対局開始時のオプションを保存 |
| `shouldStartNextGame()` | 残り対局数 > 0 なら true |
| `startNextGame()` | 次の対局を開始する |
| `reset()` | 全状態をリセット（連続対局の中断時） |

### 6.11 game層のクラス間関係まとめ

```
┌──────────────────────────────────────────────────────────────────────┐
│                                                                      │
│  UI (MainWindow)                                                     │
│    │ ensure*() で生成                                                 │
│    │                                                                  │
│    ├── GameStartCoordinator                                          │
│    │     │                                                            │
│    │     ├── requestPreStartCleanup ──▶ PreStartCleanupHandler       │
│    │     │                                                            │
│    │     ├── start() ──▶ MatchCoordinator.configureAndStart()        │
│    │     │                                                            │
│    │     └── started ──▶ UiStatePolicyManager, SessionLifecycle       │
│    │                                                                  │
│    ├── MatchCoordinator  (司令塔)                                     │
│    │     ├── ShogiGameController (盤面/ルール)                        │
│    │     ├── ShogiClock (時計)                                        │
│    │     ├── Usi × 2 (エンジン)                                       │
│    │     └── gameEnded ──▶ SessionLifecycle, UiStatePolicyManager    │
│    │                                                                  │
│    ├── GameStateController (終局処理)                                  │
│    │     └── MatchCoordinator の終局シグナルをハンドリング             │
│    │                                                                  │
│    ├── TurnManager / TurnSyncBridge (手番同期)                        │
│    │     └── GC ↔ TurnManager ↔ MainWindow                           │
│    │                                                                  │
│    └── ConsecutiveGamesController (連続対局)                           │
│          └── shouldStartNextGame() で次局判定                          │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

<!-- chapter-6-end -->

---

<!-- chapter-7-start -->
## 第7章 engine層：USIエンジン連携

engine層は、外部の USI（Universal Shogi Interface）対応エンジンとのプロセス間通信を担当する。エンジンの起動・終了、USIプロトコルのコマンド送受信、思考情報の解析と表示モデルへの反映、およびエンジン設定の管理を包括的に扱う。

### 7.1 3層アーキテクチャ

engine層は **単一責任原則（SRP）** に基づき、以下の3つのクラスに分離されている。

```
┌─────────────────────────────────────────────────────┐
│                   Usi（ファサード）                    │
│  外部公開インターフェース、シグナル中継、モデル更新       │
│                                                     │
│  m_processManager ──────┐                           │
│  m_protocolHandler ─────┤  std::unique_ptr で所有    │
│  m_presenter ───────────┘                           │
├─────────────────────────────────────────────────────┤
│         ▲ シグナル/スロット接続 ▼                      │
├──────────┬──────────────────┬────────────────────────┤
│EngineProcess│UsiProtocol    │ThinkingInfo           │
│Manager      │Handler        │Presenter              │
│             │               │                       │
│QProcess管理 │USIコマンド     │info行解析             │
│起動/終了    │送受信・待機    │読み筋漢字変換         │
│標準入出力   │状態管理       │評価値計算             │
│シャットダウン│bestmove解析   │通信ログ整形           │
└──────────┴──────────────────┴────────────────────────┘
```

#### 各クラスの責務

| クラス | ソース | 責務 |
|--------|--------|------|
| `EngineProcessManager` | `src/engine/engineprocessmanager.h/.cpp` | QProcess のライフサイクル管理。コマンド送信、標準出力/エラーの行単位読み取り、シャットダウン状態の管理 |
| `UsiProtocolHandler` | `src/engine/usiprotocolhandler.h/.cpp` | USI コマンドの生成・送信、レスポンス（usiok, readyok, bestmove, info, checkmate）の解析、待機処理 |
| `ThinkingInfoPresenter` | `src/engine/thinkinginfopresenter.h/.cpp` | info 行の解析結果を GUI 向けシグナルに変換。評価値の計算、漢字読み筋の生成、通信ログの整形 |

#### シグナル伝搬の流れ

```
エンジンプロセス (stdout)
    │
    ▼
EngineProcessManager::onReadyReadStdout()
    │ emit dataReceived(line)
    ├──────────────────────────────────────┐
    ▼                                      ▼
UsiProtocolHandler::onDataReceived()    Usi::onDataReceived()
    │                                      │
    ├─ "usiok"  → emit usiOkReceived()     └─ Presenter::logReceivedData()
    ├─ "readyok"→ emit readyOkReceived()         │ emit commLogAppended()
    ├─ "bestmove"→ emit bestMoveReceived()        ▼
    ├─ "info"   → emit infoLineReceived()   UsiCommLogModel
    │              Presenter::onInfoReceived()
    │                   │ (50msバッファリング後)
    │                   ▼
    │              processInfoLineInternal()
    │                   │ emit thinkingInfoUpdated()
    │                   ▼
    │              Usi::onThinkingInfoUpdated()
    │                   ├─ ThinkingModel::prependItem()
    │                   ├─ ConsiderationModel::updateByMultipv()
    │                   └─ emit thinkingInfoUpdated() [外部通知]
    │
    └─ "checkmate" → emit checkmateSolved/NoMate/NotImplemented/Unknown()
```

### 7.2 USIプロトコルの状態遷移

USI プロトコルは、エンジンの起動から対局終了までの一連の状態遷移を定義する。以下に主要な状態遷移を示す。

```mermaid
stateDiagram-v2
    [*] --> ProcessStarted : startProcess()
    ProcessStarted --> WaitingUsiOk : sendUsi()

    WaitingUsiOk --> OptionSetting : usiok受信
    OptionSetting --> WaitingReadyOk : setoption送信後\nsendIsReady()

    WaitingReadyOk --> Ready : readyok受信
    Ready --> Ready : sendUsiNewGame()

    Ready --> Thinking : sendPosition() +\nsendGo()
    Thinking --> Ready : bestmove受信
    Thinking --> Thinking : info行受信\n（思考情報更新）
    Thinking --> Ready : bestmove resign\nbestmove win

    Ready --> Pondering : sendPosition() +\nsendGoPonder()
    Pondering --> Thinking : sendPonderHit()\n（予想手的中）
    Pondering --> Ready : sendStop()\n→ bestmove受信

    Ready --> TsumeSearch : sendPosition() +\nsendGoMate()
    TsumeSearch --> Ready : checkmate応答

    Ready --> Quitting : sendQuit()
    Quitting --> [*] : プロセス終了
```

#### 非同期初期化シーケンス

`Usi::startAndInitializeEngineAsync()` は要求の受付を返す。
`EngineProcessManager::processStarted` の後、`UsiProtocolHandler::initializeEngineAsync()` が
`usi` → `usiok` → `setoption` → `isready` → `readyok` → `usinewgame` の順に進める。
各応答はQtシグナルで受信し、usiok・readyokにはそれぞれ既定5秒のタイマーを設ける。
成功は `Usi::engineInitialized`、失敗は `errorOccurred` で通知する。
初期化中の探索要求はキューへ保管し、初期化完了後に送信する。
キャンセル時は保留要求とタイマーを破棄する。

### 7.3 EngineProcessManager — プロセス管理

**ソース**: `src/engine/engineprocessmanager.h`, `src/engine/engineprocessmanager.cpp`

QProcess を直接管理し、USI プロトコルの詳細を知らない純粋なプロセス管理クラスである。

#### シャットダウン状態

```cpp
enum class ShutdownState {
    Running,                    // 通常動作中
    IgnoreAll,                  // 全ての受信を無視
    IgnoreAllExceptInfoString   // info string以外を無視
};
```

quit コマンド送信後、エンジンが出力する残りの応答を制御するために使用される。`IgnoreAllExceptInfoString` 状態では、指定行数分の `info string` 応答のみを許可し、残行数が 0 になると `IgnoreAll` に遷移する。

#### 主要メソッド

| メソッド | 説明 |
|---------|------|
| `startProcessAsync(engineFile)` | ファイル検証後に起動要求を出す。完了は `processStarted` で通知 |
| `stopProcessAsync()` | プロセスの所有権を終了処理へ移し、quit → terminate → kill をタイマーで進める。管理元の破棄後も回収する |
| `sendCommand(command)` | コマンド文字列を UTF-8 で書き込み、`commandSent` シグナルを発行 |
| `setLogIdentity(tag, side, name)` | ログ出力用の識別情報を設定（例: `[E1/▲] YaneuraOu`） |

#### 標準出力の読み取り

`onReadyReadStdout()` はチャンク単位（最大64行）で標準出力を読み取る。1チャンクの処理後、未読データが残っていれば `QTimer::singleShot(0, ...)` でイベントループ経由で再スケジュールし、UI の応答性を維持する。

```
onReadyReadStdout()
  │
  ├── 行ごとに読み取り（最大 kMaxLinesPerChunk = 64）
  │     ├── "id name ..." → エンジン名を検出、engineNameDetected シグナル
  │     ├── ShutdownState::IgnoreAll → 破棄
  │     ├── ShutdownState::IgnoreAllExceptInfoString
  │     │     └── "info string" のみ通過、残行数をデクリメント
  │     └── Running → dataReceived シグナルを発行
  │
  └── 未読データあり → scheduleMoreReading()
```

### 7.4 UsiProtocolHandler — プロトコル処理

**ソース**: `src/engine/usiprotocolhandler.h`, `src/engine/usiprotocolhandler.cpp`

USI プロトコルのコマンド生成・レスポンス解析・非同期初期化を担当する。

#### 思考フェーズ

```cpp
enum class SearchPhase {
    Idle,    // 待機中
    Main,    // 本探索中（go送信後）
    Ponder   // 先読み中（go ponder送信後）
};
```

`sendGo()` で `Main` に、`sendGoPonder()` で `Ponder` に遷移する。`sendPonderHit()` は `Ponder` → `Main` に切り替える（予想手が的中した場合）。

#### 主要コマンド送信メソッド

| メソッド | 生成されるコマンド |
|---------|------------------|
| `sendUsi()` | `usi` |
| `sendIsReady()` | `isready` |
| `sendUsiNewGame()` | `usinewgame` |
| `sendPosition(str)` | `position startpos moves ...` |
| `sendGo(byoyomi, btime, wtime, binc, winc, useByoyomi)` | `go btime X wtime Y byoyomi Z` / `go btime X wtime Y binc A winc B` |
| `sendGoPonder()` | `go ponder` |
| `sendGoMate(timeMs, infinite)` | `go mate X` / `go mate infinite` |
| `sendGoDepth(depth)` | `go depth X` |
| `sendGoNodes(nodes)` | `go nodes X` |
| `sendGoMovetime(timeMs)` | `go movetime X` |
| `sendGoSearchmoves(moves, infinite)` | `go searchmoves 7g7f 8h2b+ infinite` |
| `sendStop()` | `stop` |
| `sendPonderHit()` | `ponderhit` |
| `sendGameOver(result)` | `gameover win` / `gameover lose` / `gameover draw` |
| `sendQuit()` | `quit`（+ シャットダウン状態遷移） |
| `sendSetOption(name, value)` | `setoption name X value Y` |

#### レスポンス処理（onDataReceived）

```
onDataReceived(line)
  │
  ├── タイムアウト宣言済み → 破棄
  │
  ├── "checkmate ..." → handleCheckmateLine()
  │     ├── "nomate"         → emit checkmateNoMate()
  │     ├── "notimplemented" → emit checkmateNotImplemented()
  │     ├── 空/"unknown"     → emit checkmateUnknown()
  │     └── 手順あり          → emit checkmateSolved(pvMoves)
  │
  ├── "bestmove ..." → handleBestMoveLine()
  │     ├── "resign" → emit bestMoveResignReceived()（重複防止付き）
  │     ├── "win"    → emit bestMoveWinReceived()（重複防止付き）
  │     └── 通常手   → m_bestMove に格納、ponder手も取得
  │     └── emit bestMoveReceived()
  │
  ├── "info ..." → emit infoLineReceived()
  │                 Presenter::onInfoReceived()
  │
  ├── "readyok" → emit readyOkReceived()
  │
  └── "usiok"   → emit usiOkReceived()
```

#### 応答待ちとキャンセル

同期の `waitFor*` APIは設けず、応答シグナルとタイマーで進行する。
初期化は `initializationFinished(bool)`、対局の着手は `Usi::matchMoveReady` で通知する。
`UsiMatchHandler` は本探索・先読み停止待ちを管理し、先読みが外れた場合は
旧探索の `bestmove` を回収してから次の `go` を送る。
時計と同じ残り時間を期限に使い、無制限対局には30分の安全上限を設ける。
`cancelCurrentOperation()` は探索の世代を更新し、取消済みの結果の採用を防ぐ。

#### 経過時間の計測

`QElapsedTimer m_goTimer` で `go` コマンド送信から `bestmove` 受信までの経過時間を計測し、`m_lastGoToBestmoveMs` に記録する。この値は時間制御で消費時間として使用される。

### 7.5 Usi クラス — ファサード

**ソース**: `src/engine/usi.h`, `src/engine/usi.cpp`

`Usi` クラスは外部（MatchCoordinator、AnalysisCoordinator 等）に対する唯一のインターフェースとして機能する。内部の3コンポーネントを `std::unique_ptr` で所有し、シグナルの中継とモデル更新を行う。

#### コンストラクタと依存関係

```cpp
Usi(UsiCommLogModel* model,              // 通信ログモデル（非所有）
    ShogiEngineThinkingModel* thinking,   // 思考情報モデル（非所有）
    ShogiGameController* gameController,  // ゲームコントローラ（非所有）
    PlayMode& playMode,                   // 対局モード参照
    QObject* parent = nullptr);
```

コンストラクタで3コンポーネントを生成し、`setupConnections()` で全シグナル/スロットを接続する。

#### setupConnections() の接続構造

```
ProcessManager → Usi
  ├── processError      → onProcessError
  ├── commandSent       → onCommandSent      → Presenter::logSentCommand
  ├── dataReceived      → onDataReceived      → Presenter::logReceivedData
  └── stderrReceived    → onStderrReceived    → Presenter::logStderrData

ProtocolHandler → Usi（シグナル再発行）
  ├── usiOkReceived     → usiOkReceived
  ├── readyOkReceived   → readyOkReceived
  ├── bestMoveReceived  → bestMoveReceived
  ├── bestMoveResignReceived → bestMoveResignReceived
  ├── bestMoveWinReceived    → bestMoveWinReceived
  ├── errorOccurred     → errorOccurred
  ├── infoLineReceived  → infoLineReceived
  └── checkmate*        → checkmate*（4種）

Presenter → Usi（モデル更新の中継）
  ├── thinkingInfoUpdated      → onThinkingInfoUpdated
  ├── searchedMoveUpdated      → onSearchedMoveUpdated  → CommLogModel
  ├── searchDepthUpdated       → onSearchDepthUpdated   → CommLogModel
  ├── nodeCountUpdated         → onNodeCountUpdated     → CommLogModel
  ├── npsUpdated               → onNpsUpdated           → CommLogModel
  ├── hashUsageUpdated         → onHashUsageUpdated     → CommLogModel
  ├── commLogAppended          → onCommLogAppended      → CommLogModel
  └── clearThinkingInfoRequested → onClearThinkingInfoRequested → ThinkingModel
```

#### 主要シグナル一覧

| シグナル | 用途 | 主な接続先 |
|---------|------|----------|
| `usiOkReceived()` | usiok 受信完了 | 初期化状態の遷移 |
| `readyOkReceived()` | readyok 受信完了 | 初期化完了の通知 |
| `bestMoveReceived()` | bestmove 受信 | MatchCoordinator |
| `bestMoveResignReceived()` | 投了 | MatchCoordinator |
| `bestMoveWinReceived()` | 入玉宣言勝ち | MatchCoordinator |
| `errorOccurred(message)` | エンジンエラー | MainWindow（エラーダイアログ） |
| `infoLineReceived(line)` | info 行受信 | AnalysisCoordinator |
| `thinkingInfoUpdated(...)` | 思考情報更新 | 評価値グラフ、UI |
| `checkmateSolved(pvMoves)` | 詰みあり | TsumeSearchFlowController |
| `checkmateNoMate()` | 不詰 | TsumeSearchFlowController |

#### onThinkingInfoUpdated() のモデル更新

```
onThinkingInfoUpdated(time, depth, nodes, score, pvKanjiStr, usiPv, baseSfen, multipv, scoreCp)
  │
  ├── 思考タブ: m_thinkingModel->prependItem(record)
  │     └── 新しい思考情報を先頭に追加（時系列の逆順）
  │
  ├── 検討タブ: m_considerationModel->updateByMultipv(record, maxMultiPV)
  │     └── MultiPV番号に基づいて行を更新/挿入
  │
  └── emit thinkingInfoUpdated(...)  ← 外部への通知
```

### 7.6 ThinkingInfoPresenter — 思考情報の解析と表示

**ソース**: `src/engine/thinkinginfopresenter.h`, `src/engine/thinkinginfopresenter.cpp`

#### info行のバッファリング

高頻度で到着する info 行を 50ms 間隔でバッチ処理し、GUI の更新頻度を抑える。

```
onInfoReceived(line)                     ← UsiProtocolHandler から呼ばれる
  │
  ├── m_infoBuffer に追加
  │
  └── フラッシュ未予約なら
        QTimer::singleShot(50ms, flushInfoBuffer)

flushInfoBuffer()
  │
  ├── バッファを swap で取り出し
  └── 各行に対して processInfoLineInternal() を実行
```

#### processInfoLineInternal() の処理フロー

```
processInfoLineInternal(line)
  │
  ├── 1. ShogiEngineInfoParser を生成
  │     ├── 前回指し手の座標を設定
  │     └── 思考開始時の手番を設定（SFENから抽出済み）
  │
  ├── 2. parseEngineOutputAndUpdateState() でinfo行を解析
  │     └── 盤面コピー上で指し手をシミュレートし、漢字読み筋を生成
  │
  ├── 3. GUI項目のシグナルを発行
  │     ├── searchedMoveUpdated  → 探索手
  │     ├── searchDepthUpdated   → 深さ（depth/seldepth）
  │     ├── nodeCountUpdated     → ノード数（カンマ区切り）
  │     ├── npsUpdated           → NPS（カンマ区切り）
  │     └── hashUsageUpdated     → ハッシュ使用率（%表示）
  │
  ├── 4. 評価値の更新
  │     └── updateEvaluationInfo()
  │           ├── score cp → 解析モードなら後手側で符号反転
  │           └── score mate → 詰み評価値（±31111）を設定
  │
  └── 5. thinkingInfoUpdated シグナルを発行
        └── multipv=1 のみ評価値グラフを更新
```

#### 通信ログの整形

送信・受信・エラーの各ログを、エンジンタグ付きのフォーマットで整形する。

```
▶ E1: position startpos moves 7g7f     （送信：▶）
◀ E1: info depth 15 score cp 120 ...   （受信：◀）
⚠ E1: warning: ...                     （エラー：⚠）
```

### 7.7 ShogiEngineInfoParser — info行パーサ

**ソース**: `src/engine/shogiengineinfoparser.h`, `src/engine/shogiengineinfoparser.cpp`

USI エンジンが出力する `info` 行のトークンを順に解析し、各サブコマンド（depth, score cp, pv 等）の値を取得するとともに、読み筋（PV）を漢字表記に変換するクラスである。

#### 解析対象のサブコマンド

| サブコマンド | アクセサ | 説明 |
|-------------|---------|------|
| `depth` | `depth()` | 探索深さ |
| `seldepth` | `seldepth()` | 選択的探索深さ |
| `multipv` | `multipv()` | MultiPV インデックス（1が最良） |
| `nodes` | `nodes()` | 探索ノード数 |
| `nps` | `nps()` | Nodes Per Second |
| `time` | `time()` | 経過時間（ms） |
| `score cp` | `scoreCp()` | 評価値（centipawn） |
| `score mate` | `scoreMate()` | 詰み手数 |
| `hashfull` | `hashfull()` | ハッシュ使用率（千分率） |
| `pv` | `pvKanjiStr()` / `pvUsiStr()` | 読み筋（漢字/USI形式） |
| `currmove` | `searchedHand()` / `searchedUsi()` | 現在探索中の手（漢字/USI形式） |

#### 評価値の境界

```cpp
enum class EvaluationBound {
    None,        // 確定値
    LowerBound,  // 下限値（lowerbound）: 表示に "++" を付加
    UpperBound   // 上限値（upperbound）: 表示に "--" を付加
};
```

#### 読み筋（PV）の漢字変換処理

読み筋は、思考を始めた局面から日本語にする。局面は `setPosition(sfen)` で設定する（`ThinkingInfoPresenter::setBaseSfen` が呼ぶ）。
パーサはその局面を `ShogiBoard` に持ち、`parseEngineOutputAndUpdateState()` の中の `parsePvAndSimulateMoves()` が
info 行ごとに作業用の盤へ写して（`ShogiBoard::copyPositionFrom`）、指し手を順に進める。

```
入力: "7g7f 3c3d 8h2b+ 3a2b B*4e"（平手）

変換処理:
  各指し手について:
    1. UsiMove::parse() で指し手を読む
    2. 盤の上で動かす駒を決める（駒打ちは指す側の持駒）。
       指す側の駒でない・持っていない・行き先が自分の駒なら、そこで変換をやめる
    3. KifMoveText::append() で文字列に足す（手番の印は局面の手番から。「同」は詰めて書く）
    4. ShogiBoard::applyUsiMove() で盤を進める（取った駒は駒台に載る）

出力: "▲７六歩(77)△３四歩(33)▲２二角成(88)△同銀(31)▲４五角打"
```

- 初手が直前の手（`setPreviousMove`）と同じマスへ行くなら「同」と書く。
- 末尾の `(57.54%)` のような指し手でない語はそのまま付ける。`resign`・`win` は「投了」「入玉勝ち」にする。
- 局面と合わない指し手で変換をやめたときは、そこまでの漢字表記を残し、USI 形式の読み筋は残さない。

### 7.8 ShogiEngineThinkingModel — 思考情報テーブルモデル

**ソース**: `src/engine/shogienginethinkingmodel.h`, `src/engine/shogienginethinkingmodel.cpp`

`AbstractListModel<ShogiInfoRecord>` を継承した Qt モデルクラスで、思考タブ・検討タブの `QTableView` にバインドされる。

#### 列構成（6列）

| 列番号 | ヘッダ | データソース | 備考 |
|--------|--------|-------------|------|
| 0 | 時間 | `ShogiInfoRecord::time()` | 経過時間 |
| 1 | 深さ | `ShogiInfoRecord::depth()` | depth/seldepth |
| 2 | ノード数 | `ShogiInfoRecord::nodes()` | カンマ区切り |
| 3 | 評価値 | `ShogiInfoRecord::score()` | cp値または詰み手数 |
| 4 | 盤面 | 固定文字列「表示」 | ボタン風セル（青背景・白文字） |
| 5 | 読み筋 | `ShogiInfoRecord::pv()` | 漢字表記の読み筋 |

列4（盤面）はクリックすると、その読み筋の局面を盤面に表示する機能に対応する。`baseSfen` と `usiPv` から局面を再現する。

#### 2つの更新モード

| モード | メソッド | 用途 |
|--------|---------|------|
| 通常モード | `prependItem(record)` | 思考タブ：新しい情報を先頭に追加（時系列の逆順表示） |
| MultiPVモード | `updateByMultipv(record, maxMultiPV)` | 検討タブ：multipv 番号に基づいて行を更新/挿入 |

#### updateByMultipv() のロジック

```
updateByMultipv(record, maxMultiPV)
  │
  ├── multipv が範囲外（< 1 or > maxMultiPV）→ 破棄
  │
  ├── 同じ multipv 値の既存行を検索
  │     ├── 見つかった → その行を上書き（dataChanged シグナル）
  │     └── 見つからない → multipv 順にソートした位置に挿入
  │
  └── 行数が maxMultiPV を超えたら末尾から削除
```

#### ShogiInfoRecord — 思考情報レコード

**ソース**: `src/kifu/shogiinforecord.h`, `src/kifu/shogiinforecord.cpp`

1行分の思考情報を保持する QObject 派生クラスである。

| フィールド | 型 | 説明 |
|-----------|-----|------|
| `m_time` | QString | 思考時間 |
| `m_depth` | QString | 探索深さ |
| `m_nodes` | QString | ノード数 |
| `m_score` | QString | 評価値（表示用文字列） |
| `m_pv` | QString | 漢字表記の読み筋 |
| `m_usiPv` | QString | USI 形式の読み筋 |
| `m_baseSfen` | QString | 読み筋の開始局面 SFEN |
| `m_lastUsiMove` | QString | 開始局面に至った最後の指し手（ハイライト用） |
| `m_multipv` | int | MultiPV 番号（1〜） |
| `m_scoreCp` | int | 評価値（整数、ソート用） |

### 7.9 EngineSettingsCoordinator — エンジン登録・設定

**ソース**: `src/engine/enginesettingscoordinator.h`, `src/engine/enginesettingscoordinator.cpp`

エンジン設定ダイアログを起動する名前空間関数で、`EngineRegistrationDialog` をモーダル表示する薄いラッパーである。

```cpp
namespace EngineSettingsCoordinator {
    void openDialog(QWidget* parent);
    // → EngineRegistrationDialog dlg(parent); dlg.exec();
}
```

#### EngineRegistrationDialog — エンジン登録ダイアログ

**ソース**: `src/dialogs/engineregistrationdialog.h`, `src/dialogs/engineregistrationdialog.cpp`

エンジンの追加・削除・設定変更を行うダイアログ。以下のフローでエンジンを登録する。

```
エンジン追加フロー:
  │
  ├── 1. addEngineFromFileSelection()
  │     └── ファイル選択ダイアログで実行ファイルを選択
  │
  ├── 2. startEngine(engineFile)
  │     └── QProcess でエンジンを起動
  │
  ├── 3. sendUsiCommand()
  │     └── "usi" を送信
  │
  ├── 4. processEngineOutput()（readyRead シグナルで繰り返し呼ばれる）
  │     ├── "id name ..." → エンジン名を取得
  │     ├── "id author ..." → 作者名を取得
  │     ├── "option name ..." → オプション行をリストに保存
  │     └── "usiok" → 解析完了
  │
  ├── 5. parseEngineOptionsFromUsiOutput()
  │     └── option行を解析し EngineOption リストを構築
  │
  ├── 6. saveEnginesToSettingsFile()
  │     └── QSettings に Engine 情報を保存
  │
  ├── 7. saveEngineOptionsToSettings()
  │     └── QSettings にオプション値を保存
  │
  └── 8. sendQuitCommand() → cleanupEngineProcess()
```

#### EngineOption 構造体

**ソース**: `src/engine/engineoptions.h`

```cpp
struct EngineOption {
    QString name;          // オプション名
    QString type;          // 型: spin, check, combo, button, filename, string
    QString defaultValue;  // デフォルト値
    QString min;           // 最小値（spin型）
    QString max;           // 最大値（spin型）
    QString currentValue;  // 現在の値
    QStringList valueList; // 選択肢（combo型）
};
```

USI プロトコルの `option` コマンドで報告される情報を格納する。型に応じてダイアログ上で適切な入力ウィジェット（スピンボックス、チェックボックス、コンボボックス等）が生成される。

#### 設定の永続化

エンジン情報とオプション値は `QSettings`（INI ファイル）に配列で保存される。配列やグループを開いたまま読み書きするため、ほかの設定のような宣言の表（§12.5）は使わず、各関数で `QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat)` を開く。登録一覧を読むときは `EngineListSettings::loadEngines()` を使う。

```
[Engines]
1\name=YaneuraOu
1\path=/path/to/yaneuraou
1\author=...

[YaneuraOu]           ← エンジン名がグループ名
1\name=Threads
1\value=4
1\type=spin
2\name=Hash
2\value=256
2\type=spin
3\name=USI_Ponder
3\value=true
3\type=check
```

`UsiProtocolHandler::loadEngineOptions()` が起動時にこの設定を読み込み、`setoption` コマンド群を構築する。`USI_Ponder` だけはここでは送らず、GUI の先読み設定（`EnginePonderSettings`。エンジン名グループの `gui/ponderEnabled` など）から初期化時に一度だけ送る（通知を省略する設定で、エンジンも報告していなければ送らない）。

### 7.10 UsiCommLogModel — 通信ログモデル

**ソース**: `src/engine/usicommlogmodel.h`, `src/engine/usicommlogmodel.cpp`

USI エンジンの思考情報と通信ログを保持し、`Q_PROPERTY` バインディングで GUI ウィジェット（`EngineInfoWidget`, `EngineAnalysisTab`）に自動反映するモデルである。

#### プロパティ一覧

| プロパティ | 変更シグナル | 説明 |
|-----------|------------|------|
| `engineName` | `engineNameChanged` | エンジン名 |
| `predictiveMove` | `predictiveMoveChanged` | 予想手（ponder手） |
| `searchedMove` | `searchedMoveChanged` | 現在探索中の手 |
| `searchDepth` | `searchDepthChanged` | 探索深さ |
| `nodeCount` | `nodeCountChanged` | ノード数 |
| `nodesPerSecond` | `nodesPerSecondChanged` | NPS |
| `hashUsage` | `hashUsageChanged` | ハッシュ使用率 |
| `usiCommLog` | `usiCommLogChanged` | 通信ログ行 |

#### 更新の流れ

```
ThinkingInfoPresenter                  Usi                    UsiCommLogModel
  │                                     │                        │
  │ emit searchedMoveUpdated("▲７六歩") │                        │
  │ ──────────────────────────────────▶ │                        │
  │                                     │ setSearchedMove(...)   │
  │                                     │ ──────────────────────▶│
  │                                     │                        │ emit searchedMoveChanged()
  │                                     │                        │ ──────────▶ GUI更新
```

各 setter は値が実際に変更された場合のみシグナルを発火する（変更検出パターン）。`clear()` メソッドは全プロパティを空にし、対局開始時のリセットに使用される。

`appendUsiCommLog()` は通信ログの1行を設定する。`ThinkingInfoPresenter` が送受信コマンドにエンジンタグ（`▶ E1:`, `◀ E1:`）を付与した文字列を渡す。

### 7.11 クラス関係図

```
                    ┌───────────────────────┐
                    │    MatchCoordinator    │
                    │  AnalysisCoordinator  │
                    │  TsumeSearchFlow...   │
                    └───────────┬───────────┘
                                │ シグナル/スロット
                                ▼
┌───────────────────────────────────────────────────────┐
│                     Usi（ファサード）                    │
│                                                       │
│  ┌─────────────────┐ ┌──────────────────┐ ┌─────────┐ │
│  │EngineProcess    │ │UsiProtocol       │ │Thinking │ │
│  │Manager          │ │Handler           │ │Info     │ │
│  │                 │ │                  │ │Presenter│ │
│  │  QProcess       │ │  SearchPhase     │ │         │ │
│  │  ShutdownState  │ │  waitFor*()      │ │  Info   │ │
│  └────────┬────────┘ └────────┬─────────┘ │  Parser │ │
│           │                   │            └────┬────┘ │
└───────────┼───────────────────┼─────────────────┼──────┘
            │                   │                 │
            ▼                   │                 ▼
    エンジンプロセス              │        ┌───────────────┐
   （標準入出力）                 │        │ShogiEngine    │
                                │        │InfoParser     │
                                │        │               │
                                │        │ PV漢字変換    │
                                │        │ 盤面シミュレート│
                                │        └───────────────┘
                                │
                    ┌───────────┴───────────┐
                    ▼                       ▼
        ┌──────────────────┐    ┌──────────────────────┐
        │UsiCommLogModel   │    │ShogiEngineThinking   │
        │                  │    │Model                 │
        │ Q_PROPERTY       │    │                      │
        │ バインディング     │    │ 思考タブTableView    │
        │ → EngineInfo     │    │ 検討タブTableView    │
        │   Widget         │    │ (MultiPV対応)        │
        └──────────────────┘    └──────────────────────┘
```

<!-- chapter-7-end -->

---

<!-- chapter-8-start -->
## 第8章 kifu層：棋譜管理

kifu層は、棋譜データの読み込み・保存・分岐管理・ナビゲーション・エクスポートを担当するレイヤーである。棋譜の「1局の記録全体」を扱い、core層の `ShogiMove` やSFEN文字列を組み合わせて分岐ツリーという上位構造を構築する。

### 8.1 全体構成

```
┌─────────────────────────────────────────────────────────────────┐
│                        kifu 層                                  │
│                                                                 │
│  ┌─────────────────────┐   ┌─────────────────────────────┐      │
│  │  KifuBranchTree     │   │  GameRecordModel            │      │
│  │  ┌───────────────┐  │   │  コメント管理（Single Source │      │
│  │  │KifuBranchNode │  │   │  of Truth）、棋譜出力        │      │
│  │  │ ply, sfen,    │  │   │  （KIF/KI2/CSA/JKF/USEN/USI）│      │
│  │  │ move, comment │  │   └──────────┬──────────────────┘      │
│  │  └───────────────┘  │              │ 参照                    │
│  └──────────┬──────────┘              │                         │
│             │ 構築                     │                         │
│  ┌──────────┴──────────┐   ┌──────────┴──────────────────┐      │
│  │KifuBranchTreeBuilder│   │  KifuNavigationState        │      │
│  │  fromKifParseResult  │   │  現在位置追跡、分岐記憶      │      │
│  │  fromResolvedRows    │   └─────────────────────────────┘      │
│  └─────────────────────┘                                        │
│                                                                 │
│  ┌─────────────────────┐   ┌─────────────────────────────┐      │
│  │KifuLoadCoordinator  │   │  LiveGameSession            │      │
│  │  ファイル読み込み     │   │  ライブ対局の一時記録        │      │
│  │  形式自動判定        │   │  commit/discard パターン    │      │
│  └─────────────────────┘   └─────────────────────────────┘      │
│                                                                 │
│  ┌─────────────────────┐   ┌─────────────────────────────┐      │
│  │KifuSaveCoordinator  │   │  KifuExportController       │      │
│  │  ダイアログ保存       │   │  クリップボード/ファイル出力  │      │
│  └─────────────────────┘   └─────────────────────────────┘      │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ formats/  フォーマット変換器                              │    │
│  │  KifToSfenConverter, Ki2ToSfenConverter,                │    │
│  │  CsaToSfenConverter, JkfToSfenConverter,                │    │
│  │  UsenToSfenConverter, UsiToSfenConverter                │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

### 8.2 KifuBranchTree / KifuBranchNode — 分岐ツリー

**ソース**: `src/kifu/kifubranchtree.h/.cpp`, `src/kifu/kifubranchnode.h/.cpp`

棋譜の分岐構造をツリーで表現するデータモデルである。UIとは独立しており、純粋なデータ層として機能する。

#### ノードが保持する情報

| フィールド | 型 | 説明 |
|-----------|-----|------|
| `m_nodeId` | `int` | ツリー内で一意のID（`QHash` で O(1) 検索可能） |
| `m_ply` | `int` | 手数（0=開始局面、1以降=指し手） |
| `m_displayText` | `QString` | 表示テキスト（例: `"▲７六歩(77)"`） |
| `m_sfen` | `QString` | この局面のSFEN文字列 |
| `m_move` | `ShogiMove` | 指し手データ（core層の構造体） |
| `m_comment` | `QString` | この手に対するコメント |
| `m_timeText` | `QString` | 消費時間テキスト |
| `m_terminalType` | `TerminalType` | 終局手の種類（投了、詰み、千日手など） |
| `m_parent` | `KifuBranchNode*` | 親ノードへのポインタ（非所有） |
| `m_children` | `QVector<KifuBranchNode*>` | 子ノードリスト（非所有、ツリーが所有） |

#### 所有権モデル

`KifuBranchTree` が全ノードを `m_nodeById`（`QHash<int, KifuBranchNode*>`）で所有する。ノード間の `parent` / `children` ポインタは非所有の参照であり、ツリーのデストラクタで全ノードが一括解放される。

#### ツリー構造図

以下は、本譜から2つの分岐が派生した棋譜の例である。

```mermaid
graph TD
    R["Root<br/>ply=0<br/>開始局面"]
    M1["ply=1<br/>▲７六歩"]
    M2["ply=2<br/>△３四歩"]
    M3["ply=3<br/>▲２六歩"]
    M4["ply=4<br/>△８四歩"]
    M5["..."]
    MT["△投了<br/>terminal"]

    B3a["ply=3<br/>▲６六歩<br/><i>分岐1</i>"]
    B4a["ply=4<br/>△８四歩"]
    B5a["..."]

    B2b["ply=2<br/>△８四歩<br/><i>分岐2</i>"]
    B3b["ply=3<br/>▲２六歩"]
    B4b["..."]

    R --> M1
    M1 --> M2
    M2 --> M3
    M3 --> M4
    M4 --> M5
    M5 --> MT

    M2 -->|"子[1]"| B3a
    B3a --> B4a
    B4a --> B5a

    M1 -->|"子[1]"| B2b
    B2b --> B3b
    B3b --> B4b

    style R fill:#e8f5e9
    style B3a fill:#fff3e0
    style B2b fill:#fff3e0
    style MT fill:#ffebee
```

**本譜**はルートから各ノードの `children[0]`（最初の子）を辿った経路である。`children[1]` 以降が分岐を表す。`isMainLine()` は「親の最初の子であるか」で判定する。

#### TerminalType — 終局手の種類

```cpp
enum class TerminalType {
    None,           // 通常の指し手
    Resign,         // 投了
    Checkmate,      // 詰み
    Repetition,     // 千日手
    Impasse,        // 持将棋
    Timeout,        // 切れ負け
    IllegalWin,     // 反則勝ち
    IllegalLoss,    // 反則負け
    Forfeit,        // 不戦敗
    Interrupt,      // 中断
    NoCheckmate     // 不詰（詰将棋用）
};
```

終局手ノードは `isTerminal() == true` であり、SFENや `ShogiMove` は持たない。`detectTerminalType()` 関数により表示テキスト（例: `"△投了"`）から種類を判定できる。

#### BranchLine — ライン情報

```cpp
struct BranchLine {
    int lineIndex;                      // 0=本譜、1以降=分岐
    QString name;                       // "本譜", "分岐1", ...
    QVector<KifuBranchNode*> nodes;     // ルートから終端までのノード列
    int branchPly;                      // 分岐開始手数
    KifuBranchNode* branchPoint;        // 分岐点のノード
};
```

`allLines()` で全ラインを取得でき、`displayItemsForLine(lineIndex)` / `sfenListForLine(lineIndex)` でライン単位のデータ抽出が可能。

#### RecordLine — 手順の値

**ソース**: `src/kifu/recordline.h`（ヘッダオンリー）

分岐ツリーの1本の手順を、ほかの機能が使う形にした値。検討・詰み探索・棋譜解析・定跡のマージ・読み筋の表示・書き出しは、局面列や指し手列を別々の一覧から探さずにこの値から取る。終局行（投了など）は含まない。

```cpp
struct RecordLine {
    QStringList sfens;          // 開始局面（[0]）と各手の後の局面
    QList<ShogiMove> moves;     // 各手（moves[i] は i+1 手目）
    QStringList displayTexts;   // 各手の棋譜の表記
    int currentPly = 0;         // 表示中の手数（0 は開始局面）

    UsiMove usiMove(int ply) const;               // ply 手目の指し手
    QStringList usiMoves(int upToPly = -1) const; // upToPly 手目までの USI の指し手
    BoardSquare destinationOf(int ply) const;     // ply 手目の移動先
    QString positionCommand(int ply) const;       // ply 手目の後の position コマンド
};
```

| 取り出し方 | 中身 |
|---------|------|
| `KifuNavigationState::currentRecordLine()` | 表示中の手順（選んでいる分岐。対局中はライブの手も含む） |
| `KifuBranchTree::mainRecordLine()` | 本譜の手順 |
| `KifuBranchTree::toRecordLine(nodes, currentPly)` | 任意のノード列（static） |

棋譜の正本は分岐ツリーとそのカーソル（8.10）で、対局の局面列・指し手列（`sfenRecord` / `gameMoves`）は対局開始時にこの値から作り直す作業用の履歴である（[指し手データ・棋譜データ構造](move-and-record-data-structures.md) の 4.4）。

#### KifuBranchTree の主要API

| カテゴリ | メソッド | 説明 |
|---------|---------|------|
| 構築 | `setRootSfen(sfen)` | ルートノードを作成 |
| 構築 | `addMove(parent, move, text, sfen)` | 指し手ノードを追加（`treeChanged` 発火） |
| 構築 | `addMoveQuiet(...)` | 指し手追加（`nodeAdded` のみ発火、ライブ対局用） |
| 構築 | `addTerminalMove(parent, type, text)` | 終局手ノードを追加 |
| 検索 | `nodeAt(nodeId)` | IDで O(1) 検索 |
| 検索 | `findByPlyOnMainLine(ply)` | 本譜のN手目を取得 |
| 検索 | `findBySfen(sfen)` | SFEN一致検索 |
| ライン | `mainLine()` | 本譜のノード列を取得 |
| ライン | `pathToNode(node)` | ルートから指定ノードまでの経路 |
| ライン | `allLines()` | 全 `BranchLine` のリスト |
| ライン | `mainRecordLine()` | 本譜の手順（`RecordLine`、終局行を除く） |
| ライン | `toRecordLine(nodes, currentPly)` | ノード列を `RecordLine` にする（static） |
| 分岐 | `hasBranch(node)` | 分岐の有無判定 |
| 分岐 | `branchesAt(node)` | 分岐候補（兄弟ノード）の取得 |
| コメント | `setComment(nodeId, comment)` | コメント設定（`commentChanged` 発火） |

#### シグナル

| シグナル | 発火タイミング |
|---------|--------------|
| `treeChanged()` | ツリー構造が変更された時（addMove等） |
| `nodeAdded(node)` | ノードが追加された時（addMoveQuietでも発火） |
| `nodeRemoved(nodeId)` | ノードが削除された時 |
| `commentChanged(nodeId, comment)` | コメントが変更された時 |

### 8.3 KifuBranchTreeBuilder — ツリー構築

**ソース**: `src/kifu/kifubranchtreebuilder.h/.cpp`

各種データソースから `KifuBranchTree` を構築する静的ビルダークラスである。コンストラクタは private で、全メソッドが static。

#### 構築フロー

```
入力データ
  │
  ├── KifParseResult（推奨パス）
  │     └── fromKifParseResult(result, startSfen)
  │           │
  │           ├── tree->setRootSfen(startSfen)
  │           ├── addKifLineToTree(tree, mainline, startPly=1)
  │           │     └── 本譜の各手を addMove() で追加
  │           └── for each variation:
  │                 └── addKifLineToTree(tree, line, startPly)
  │                       └── 分岐点を findByPlyOnMainLine() で特定
  │                           → 親ノードに addMove() で分岐追加
  │
  ├── ResolvedRows（移行用パス）
  │     └── fromResolvedRows(rows, startSfen)
  │           └── 行ごとに addLineToTree() で構築
  │
  └── KifLine（単一ライン、分岐なし）
        └── fromKifLine(line, startSfen)
```

- **`fromKifParseResult`**: フォーマット変換器の `parseWithVariations()` 結果から構築する推奨パス。`KifParseResult` は本譜（`mainline`）と変化リスト（`variations`）を持つ
- **`fromResolvedRows`**: 旧システムの `ResolvedRow` 配列からの移行用パス
- **`buildFromKifParseResult`**: 既存ツリーをクリアして再構築する in-place 版

### 8.4 KifDisplayItem / KifParseTypes — 基本データ型

#### KifDisplayItem

**ソース**: `src/kifu/kifdisplayitem.h`

棋譜の1手分の表示情報を保持する軽量構造体。UIモデル（`KifuRecordListModel`）やフォーマット変換器で広く使用される。

```cpp
struct KifDisplayItem {
    QString prettyMove;  // "▲７六歩" — 表示用指し手テキスト
    QString timeText;    // "00:05/00:00:15" — 消費時間
    QString comment;     // コメント
    int     ply = 0;     // 手数（0始まり）
};
```

`Q_DECLARE_METATYPE` でQt メタタイプに登録されており、シグナル/スロットで直接受け渡し可能。

#### KifParseResult / KifLine / KifVariation

**ソース**: `src/kifu/kifparsetypes.h`

フォーマット変換器が棋譜をパースした結果を格納する構造体群。

```cpp
struct KifLine {
    int startPly;               // 開始手数
    QString baseSfen;           // 分岐開始局面のSFEN
    QStringList usiMoves;       // USI形式の指し手列
    QList<KifDisplayItem> disp; // 表示用データ
    QStringList sfenList;       // 各手後の局面SFEN列
    QVector<ShogiMove> gameMoves; // GUI用指し手列
    bool endsWithTerminal;      // 終局手で終わるか
};

struct KifVariation {
    int startPly;   // 変化開始手数
    KifLine line;   // 変化の手順
};

struct KifParseResult {
    KifLine mainline;                  // 本譜
    QVector<KifVariation> variations;  // 変化リスト
};
```

#### ResolvedRow / LiveGameState

**ソース**: `src/kifu/kifutypes.h`

`ResolvedRow` は分岐を含む棋譜を行単位で平坦化した旧システムのデータ構造。`LiveGameState` はライブ対局の進行状態を保持する。

```cpp
struct ResolvedRow {
    int startPly;              // 開始手数
    int parent;                // 親行のインデックス（本譜は -1）
    QList<KifDisplayItem> disp; // 表示用アイテム
    QStringList sfen;          // SFENリスト
    QVector<ShogiMove> gm;    // 指し手リスト
    int varIndex;              // 変化インデックス（本譜 = -1）
    QStringList comments;      // コメント
};
```

### 8.5 GameRecordModel — 棋譜データの中央管理

**ソース**: `src/kifu/gamerecordmodel.h/.cpp`

`KifuBranchTree` を正本として、コメント・しおりの編集と各種フォーマットへの出力を担当する。ツリー未構築時のみ内部配列へ保存する。

#### 責務

1. **コメント・しおり管理**: 表示中の分岐のノードを更新し、変更シグナルで表示へ通知（棋譜欄の表示用の一覧には書かない）
2. **変更追跡**: `isDirty()` で未保存の変更を検出
3. **棋譜出力**: 6つのフォーマットへの変換メソッドを提供

#### データフロー

```
setComment(ply, comment)
  │
  ├── 表示中の分岐（KifuNavigationState::currentLineIndex()）の ply 番目のノードに保存
  │     （ツリー未構築時のみ内部配列）
  └── commentChanged(ply, comment)
        └── CommentCoordinator が棋譜欄とコメント欄を更新
```

#### ExportContext — 出力コンテキスト

`ExportContext` は `KifuExportMetadata` の別名。UIウィジェットへの参照を含まず、
`KifuExportMetadataBuilder` が通常出力と簡易KIF保存の対局情報・対局者名生成を共有する。

```cpp
struct KifuExportMetadata {
    bool gameInfoProvided;             // 空の編集結果も明示できる
    QList<KifGameInfoItem> gameInfoItems; // UIから抽出済みの値
    QString startSfen;                  // 開始局面SFEN
    PlayMode playMode;                  // 対局モード
    QString human1, human2;             // 人間プレイヤー名
    QString engine1, engine2;           // エンジン名
    // 時間制御情報（CSA出力用）
    bool hasTimeControl;
    int initialTimeMs, byoyomiMs, fischerIncrementMs;
    QDateTime gameStartDateTime, gameEndDateTime;
};
```

#### 出力フォーマット

| メソッド | 形式 | 分岐対応 | 備考 |
|---------|------|---------|------|
| `toKifLines(ctx)` | KIF | あり | 日本将棋連盟標準形式。消費時間・コメント含む |
| `toKi2Lines(ctx)` | KI2 | あり | 移動元座標省略形式。消費時間なし |
| `toCsaLines(ctx, usiMoves)` | CSA | なし | コンピュータ将棋協会形式。本譜のみ |
| `toJkfLines(ctx)` | JKF | あり | JSON棋譜フォーマット |
| `toUsenLines(ctx, usiMoves)` | USEN | あり | URLセーフ短縮形式 |
| `toUsiLines(ctx, usiMoves)` | USI | なし | position コマンド文字列。本譜のみ |

保存・コピーで渡す `usiMoves` は、`collectMainlineUsiForExport()`（分岐ツリーの本譜 `KifuBranchTree::mainRecordLine().usiMoves()`）から取る。

### 8.6 LiveGameSession — ライブ対局セッション

**ソース**: `src/kifu/livegamesession.h/.cpp`

進行中の対局を一時的に記録し、対局終了時に `KifuBranchTree` にコミットするセッション管理クラス。途中局面からの対局開始にも対応する。

#### commit/discard パターン

```
  startFromNode(branchPoint)          startFromRoot()
        │                                   │
        ▼                                   ▼
   ┌────────────────────────────────────────────┐
   │           m_active = true                  │
   │  ┌──────────────────────────────┐          │
   │  │ addMove(move, text, sfen)    │ ← 繰返し │
   │  │ addTerminalMove(type, text)  │          │
   │  └──────────────────────────────┘          │
   └────────────┬──────────────────┬────────────┘
                │                  │
         commit()             discard()
                │                  │
                ▼                  ▼
   ┌─────────────────┐  ┌──────────────────┐
   │ツリーに分岐追加   │  │セッション破棄     │
   │sessionCommitted  │  │sessionDiscarded  │
   │シグナル発火       │  │シグナル発火       │
   └─────────────────┘  └──────────────────┘
```

#### 動作の詳細

1. **`startFromNode(branchPoint)`**: 途中局面から対局を開始。`branchPoint` が分岐の起点となる
2. **`startFromRoot()`**: 開始局面から新規対局を開始
3. **`addMove(...)`**: 指し手を一時バッファ（`m_moves`）に記録しつつ、`addMoveQuiet()` でツリーにも即座にノードを追加（リアルタイムUI更新のため）
4. **`addTerminalMove(...)`**: 終局手を追加。以後 `canAddMove()` は `false` を返す
5. **`commit()`**: セッションの内容を確定。`sessionCommitted` シグナルで新しいラインの終端ノードを通知
6. **`discard()`**: セッションを破棄。ツリーには変更を加えない

#### 待った（undoMoves）

`undoMoves(count)` はセッションの履歴と現在位置（`m_liveParent`）を巻き戻すだけで、ツリーのノードは削除しない。取り消した手順は分岐として残る。
このセッションで追加した手順を取り消した場合は、その先頭ノードを `m_undoneNodeIds` に記録する。次に指した手（新規ノード・既存ノードの再利用・終局手のいずれも）が記録済みの兄弟より後ろにあれば、`KifuBranchTree::moveChildQuiet()` で前へ移す。
これにより、実際に指した手順が本譜側、取り消した手順が分岐側に並ぶ。以前の棋譜から再利用した手を取り消した場合は記録せず、以前の本譜の並びを保つ。
`canUndoMoves()` は対局開始位置（分岐起点・ルート）より前へは戻さない。

#### 途中局面からの分岐

`willCreateBranch()` は途中局面からの対局（`branchPoint != nullptr`）の場合に `true` を返す。確定時には `newLineName()` で `"分岐N"` という名前が付与される。

#### 主要シグナル

| シグナル | 発火タイミング |
|---------|--------------|
| `sessionStarted(branchPoint)` | セッション開始時 |
| `moveAdded(ply, displayText)` | 指し手追加時 |
| `terminalAdded(type)` | 終局手追加時 |
| `sessionCommitted(newLineEnd)` | 確定時（新ラインの終端ノードを通知） |
| `sessionDiscarded()` | 破棄時 |

### 8.7 KifuLoadCoordinator — 棋譜読み込みフロー

**ソース**: `src/kifu/kifuloadcoordinator.h/.cpp`, `src/kifu/kifuloadparser.h/.cpp`, `src/kifu/kifuapplyservice.h`, `src/kifu/kifuapplyservice_apply.cpp`

棋譜ファイルの読み込みからUI反映までを統括するコーディネータ。6つのファイル形式と、テキスト貼り付けによる自動判定読み込みに対応する。パース（`KifuLoadParser`）と、結果をモデル・UIに当てはめる処理（`KifuApplyService`）は別のクラスにある。

最初の読み込みのときに `KifuSubRegistry::ensureKifuLoadCoordinator()` が一度だけ作り（親は MainWindow）、以後のすべての読み込みに使う。`KifuFileController` は `Deps::ensureKifuLoadCoordinator` で取得する。読み込み中に次の読み込みを始めると、前の読み込みは `KifuLoadCoordinator` 自身が打ち切る。

#### 読み込みフロー

```
ユーザーがファイルを開く
  │
  ▼
loadFileAsync(filePath) / loadTextAsync(content)     ← KifuFileController から
  │   （同期版の loadKifuFromFile / loadKi2FromFile / loadCsaFromFile / ... も同じ applyLoadResult() を通る）
  ▼
KifuLoadParser::parseFile() / parseText()             ← 別スレッド（QtConcurrent::run）
  │   形式を判定してフォーマット変換器の parseWithVariations() を呼ぶ
  │   → KifuLoadResult { initialSfen, teaiLabel, gameInfo, record: KifParseResult（本譜 + 変化）, ... }
  ▼
onLoadFinished() → applyLoadResult(result)            ← メインスレッド
  │
  ├── 1. 対局情報
  │     ├── KifuApplyService::populateGameInfo() → 対局情報テーブルに反映
  │     └── KifuApplyService::applyPlayersFromGameInfo() → 対局者名を盤面に反映
  │
  └── 2. KifuApplyService::applyParsedResult(filePath, initialSfen, teaiLabel, record, ...)
        │
        ├── 2a. applyToRecordView()
        │     ├── displayGameRecord(disp) フック → 棋譜欄に反映（GameRecordModel にコメント・しおり）
        │     ├── 棋譜欄の先頭行を選択、navState->goToRoot()
        │     └── enableArrowButtons() フック
        │
        ├── 2b. applyBranchTree()
        │     ├── KifuBranchTreeBuilder::buildFromKifParseResult() でツリー構築
        │     │     （beginBatchUpdate()/endBatchUpdate() で treeChanged を1回にまとめる）
        │     ├── navState->goToRoot()、branchTreeBuilt() フック
        │     ├── applyBranchMarksForCurrentLine() → 棋譜欄の分岐マーク
        │     └── 分岐ツリー欄の開始局面をハイライト
        │
        └── 2c. syncBoardAndHighlightsAtRow(0) フック → 盤面を開始局面に同期
```

棋譜の正本は分岐ツリーで、読み込みは対局の局面列・指し手列（`sfenRecord` / `gameMoves`）を書かない。これらは対局開始時に表示中の手順から作り直す（[対局開始フロー](game-start-flow.md) の 2 章）。

#### テキスト貼り付け

`loadTextAsync(content)` / `loadKifuFromString(content)` は `KifuLoadParser::parseText()` で、`KifuFileReader::detectFormat()` により形式を自動判定し、適切なパーサーにディスパッチする。SFEN・BOD は局面だけを読み込む（`KifuApplyService::loadPositionFromSfen()`）。

#### 分岐マーカー表示

`KifuApplyService::applyBranchMarksForCurrentLine()` が分岐のある手数を収集し、`KifuRecordListModel::setBranchPlyMarks()` で棋譜モデルへ反映する。

### 8.8 KifuSaveCoordinator / KifuIoService — 保存

**ソース**: `src/kifu/kifusavecoordinator.h/.cpp`, `src/kifu/kifuioservice.h/.cpp`

#### KifuSaveCoordinator

棋譜の保存ダイアログ表示とファイル書き込みを担当する名前空間。対応フォーマットの組み合わせに応じて複数のダイアログ関数を提供する。

| 関数 | 対応形式 |
|------|---------|
| `saveViaDialog` | KIF |
| `saveViaDialogWithKi2` | KIF, KI2 |
| `saveViaDialogWithAllFormats` | KIF, KI2, CSA |
| `saveViaDialogWithJkf` | KIF, KI2, CSA, JKF |
| `saveViaDialogWithUsen` | KIF, KI2, CSA, JKF, USEN |
| `saveViaDialogWithUsi` | KIF, KI2, CSA, JKF, USEN, USI |
| `overwriteExisting` | 既存ファイルへの上書き |

すべての関数は保存に成功した場合にファイルパスを返し、キャンセルまたはエラーの場合は空文字を返す。

#### KifuIoService

低レベルのファイルI/Oを担当する名前空間。

- **`makeDefaultSaveFileName`**: 対局モード・プレイヤー名・日時からデフォルトファイル名を生成
- **`writeKifuFile`**: 行リストをUTF-8テキストファイルとして書き込み

### 8.9 KifuExportController — エクスポート管理

**ソース**: `src/kifu/kifuexportcontroller.h/.cpp`

`GameRecordModel` の出力メソッドと `KifuSaveCoordinator` / `KifuClipboardService` を統合し、MainWindow のアクション（メニュー項目）からエクスポートを実行するコントローラ。

#### 依存構造（Dependencies）

```cpp
struct Dependencies {
    GameRecordModel* gameRecord;
    KifuRecordListModel* kifuRecordModel;
    GameInfoPaneController* gameInfoController;
    TimeControlController* timeController;
    ShogiGameController* gameController;
    // ... 状態値（開始SFEN・対局モード・対局者名・表示中のライン・currentMoveIndex など）
};
```

`setPrepareCallback()` で設定されたコールバックが、保存・コピー操作の直前に呼ばれ、依存オブジェクトの最新状態を反映する（`KifuSubRegistry::updateKifuExportDeps()`）。`currentMoveIndex` は分岐ツリーのカーソル（`KifuNavigationState::currentPly()`）から取る。
CSA・USEN・USI の書き出しに使う USI の指し手は、対局の局面列・指し手列ではなく `GameRecordModel::collectMainlineUsiForExport()`（分岐ツリーの本譜）から取る。クリップボード操作は `KifuExportClipboard` に委ね、依存は `Dependencies` から写した `KifuExportClipboard::Deps` で渡す。

#### 主要メソッド

| カテゴリ | メソッド | 説明 |
|---------|---------|------|
| ファイル保存 | `saveToFile()` | ダイアログ表示 → 全形式で保存 |
| ファイル保存 | `overwriteFile(path)` | 上書き保存 |
| ファイル保存 | `autoSaveToDir(dir)` | 指定ディレクトリへ自動保存 |
| クリップボード | `copyKifToClipboard()` | KIF形式でコピー |
| クリップボード | `copyKi2ToClipboard()` | KI2形式でコピー |
| クリップボード | `copyCsaToClipboard()` | CSA形式でコピー |
| クリップボード | `copyJkfToClipboard()` | JKF形式でコピー |
| クリップボード | `copyUsenToClipboard()` | USEN形式でコピー |
| クリップボード | `copyUsiToClipboard()` | USI形式（全手）でコピー |
| クリップボード | `copyUsiCurrentToClipboard()` | USI形式（現在の手まで）でコピー |
| クリップボード | `copySfenToClipboard()` | SFEN（現在局面）をコピー |
| クリップボード | `copyBodToClipboard()` | BOD形式（現在局面）をコピー |

### 8.10 KifuNavigationState — ナビゲーション状態

**ソース**: `src/kifu/kifunavigationstate.h/.cpp`

`KifuBranchTree` 上の現在位置（棋譜のカーソル）と、分岐点での選択履歴を管理するクラス。表示中の手数は `currentPly()` だけが持ち、MainWindow は手数の変数を持たない。

#### 状態管理

```
KifuNavigationState
  │
  ├── m_currentNode          … 現在表示中のノード（カーソル）
  ├── m_preferredLineIndex   … 優先ライン（-1=未設定）
  └── m_lastSelectedChildAtBranch  … QHash<nodeId, childIndex>
                                     分岐点ごとに最後に選んだ子を記憶
```

#### 優先ライン（Preferred Line）

分岐を選択した時に `setPreferredLineIndex(lineIndex)` で設定される。分岐点より前に戻っても維持されるため、「進む」操作時に以前選択した分岐を辿ることができる。`resetPreferredLineIndex()` で本譜に戻る。

#### 分岐選択の記憶

```
手順例:
  1. 3手目で2番目の子（分岐）を選択
     → rememberChildSelection(node2, 1) で記憶（node2 は分岐点）
  2. 1手目に戻る
  3. 「進む」で3手目に到達
     → lastSelectedChildAt(node2) が 1 を返す
     → 自動的に同じ分岐を選択
```

`rememberPathSelections(node)` はルートから `node` までの経路上の全分岐点で、経路が通る子を記憶する。`clearLineSelectionMemory()` ですべての記憶をクリアし、ナビゲーションを本譜に戻す。記憶する添字は `childAt()` の添字で、`allLines()` のラインインデックスとは別物である。

#### 主要メソッド

| カテゴリ | メソッド | 説明 |
|---------|---------|------|
| 位置 | `currentNode()` | 現在のノードを取得 |
| 位置 | `currentPly()` | 現在の手数（表示中の手数）を取得 |
| 位置 | `currentLineIndex()` | 現在のラインインデックスを取得 |
| 位置 | `currentSfen()` | 現在位置のSFENを取得 |
| 位置 | `currentRecordLine()` | 表示中の手順（`RecordLine`。8.2 を参照） |
| 設定 | `setCurrentNode(node)` | 現在位置を設定 |
| 設定 | `goToRoot()` | ルートに移動 |
| クエリ | `isOnMainLine()` | 本譜にいるか |
| クエリ | `canGoForward()` / `canGoBack()` | 進める/戻れるか |
| 分岐 | `branchCandidatesAtCurrent()` | 現在位置の分岐候補 |
| 分岐 | `hasBranchAtCurrent()` | 現在位置に分岐があるか |
| 記憶 | `rememberChildSelection(node, childIndex)` | 分岐点で選んだ子を記憶 |
| 記憶 | `lastSelectedChildAt(node)` | 分岐点で最後に選んだ子を取得 |
| 記憶 | `rememberPathSelections(node)` | 経路上の全分岐点の選択を記憶 |

`KifuNavigationState` はシグナルを持たない。カーソルを動かすのは `KifuNavigationController`（ナビゲーション操作）、`KifuDisplayCoordinator`（棋譜欄のクリック・対局の開始・着手・待った）、`KifuApplyService`（棋譜の読み込み）、`PreStartCleanupHandler`（新規対局の開始前）などで、表示の更新は `KifuNavigationController` のシグナル（13.1）で行う。

### 8.11 フォーマット変換器一覧

`src/kifu/formats/` に配置された変換器群は、各棋譜フォーマットを統一的なインターフェースで読み込む。

| クラス | 入力形式 | 概要 | 分岐対応 |
|--------|---------|------|---------|
| `KifToSfenConverter` | KIF (.kif, .kifu) | 日本将棋連盟標準形式。移動元座標付き指し手、消費時間、コメント、BOD盤面図を解析 | あり |
| `Ki2ToSfenConverter` | KI2 (.ki2) | 移動元座標省略形式。盤面状態を追跡し「右」「左」「直」等の修飾語から移動元を推測 | あり |
| `CsaToSfenConverter` | CSA (.csa) | コンピュータ将棋協会形式。PI/P1..P9による初期局面、`+`/`-` による指し手、`$` による対局情報を解析 | なし |
| `JkfToSfenConverter` | JKF (.jkf) | JSON棋譜フォーマット。`header` / `initial` / `moves` の3セクション構造。分岐は `forks` 配列で表現 | あり |
| `UsenToSfenConverter` | USEN (.usen) | URLセーフ短縮形式。base36エンコードされた指し手列（3文字=1手）。`~(startPly).` で分岐区切り | あり |
| `UsiToSfenConverter` | USI (.usi) | USI position コマンド文字列。`startpos moves ...` または `sfen ... moves ...` 形式 | なし |

#### 共通API

すべての変換器は以下の統一インターフェースを持つ:

```cpp
// 初期局面SFENの検出
static QString detectInitialSfenFromFile(const QString& path, QString* detectedLabel = nullptr);

// USI指し手列の抽出（本譜のみ）
static QStringList convertFile(const QString& path, QString* errorMessage = nullptr);

// 表示用データの抽出（コメント含む）
static QList<KifDisplayItem> extractMovesWithTimes(const QString& path, QString* errorMessage = nullptr);

// 本譜＋全変化の一括抽出（推奨API）
static bool parseWithVariations(const QString& path, KifParseResult& out, QString* errorMessage = nullptr);

// 対局情報の抽出
static QList<KifGameInfoItem> extractGameInfo(const QString& filePath);
```

`parseWithVariations()` が最も包括的なAPIであり、`KifuLoadCoordinator` はこのメソッドを経由して棋譜を読み込む。

#### KI2形式の移動元推測

KI2形式は移動元座標がないため、`Ki2ToSfenConverter` は局面を `ShogiBoard` で追い、各指し手で以下の処理を行う:

1. `Ki2Lexer::parseMoveText()` で移動先（「同」）・駒・修飾語（「右」「左」「上」「引」「寄」「直」）・成／不成・打を分ける
2. `Ki2MoveNotation::resolve()` で、局面の合法手（`EngineMoveValidator::legalMoves()`）のうち同じ駒が移動先へ動く手を修飾語で1つに絞る。
   ピンされた駒は合法手に入らないので候補にならない。合法手で決まらない手（二歩・王手放置などの反則手を書いた棋譜）だけは、
   駒の動きだけで決めた手（`EngineMoveValidator::pseudoLegalMoves()`）から選ぶ
3. 決まった `UsiMove` を `ShogiBoard::applyUsiMove()` で盤に適用する

書き出し（`Ki2Exporter`）は、分岐ツリーのノードの手（`ShogiMove::toUsiMove()`）と局面から `Ki2MoveNotation::format()` で
KI2 の文字列を作る。修飾語は日本将棋連盟の棋譜の表記方法に合わせる（動作の上・引・寄を優先し、区別できなければ右・左。
金・銀が真っ直ぐ上がるときは直。飛・角・竜・馬は直を使わない。3枚以上は「左上」のように位置・動作の順）。

### 8.12 補助クラス

#### KifuContentBuilder

**ソース**: `src/kifu/kifucontentbuilder.h/.cpp`

棋譜データから KIF 形式の行リストを構築するユーティリティクラス。`toRichHtmlWithStarBreaksAndLinks()` でコメント中のURLをリンク化する機能も持つ。

#### KifuClipboardService

**ソース**: `src/kifu/kifuclipboardservice.h/.cpp`

クリップボード操作を担当する名前空間。各形式でのコピー、貼り付けテキストの取得、形式の自動判定（`detectFormat()`）を提供する。`KifuFormat` 列挙型で Kif / Ki2 / Csa / Usi / Jkf / Usen / Sfen の7形式を判別する。

#### KifReader

**ソース**: `src/kifu/kifreader.h/.cpp`

KIF形式ファイルの低レベル読み込みを担当するクラス。

#### ShogiInfoRecord

**ソース**: `src/kifu/shogiinforecord.h/.cpp`

USIエンジンの思考情報（時間・深さ・ノード数・評価値・読み筋）を1レコードとして保持するクラス。engine層の `ThinkingInfoPresenter` と連携して思考タブに表示される。

<!-- chapter-8-end -->

---

<!-- chapter-9-start -->
## 第9章 analysis層：解析機能

analysis層は、USIエンジンを用いた局面・棋譜の解析機能を提供するレイヤーである。**検討（Consideration）**・**棋譜解析（KifuAnalysis）**・**詰将棋探索（TsumeSearch）** の3つのモードを持ち、それぞれ異なるユースケースに対応する。

### 9.1 3モードの比較

| 項目 | 検討（Consideration） | 棋譜解析（KifuAnalysis） | 詰将棋探索（TsumeSearch） |
|------|----------------------|------------------------|--------------------------|
| **目的** | 現在の1局面をリアルタイムで深く分析する | 棋譜の指定範囲を1手ずつ自動解析し、各手の評価値と候補手を算出する | 現在の局面から詰み手順を探索する |
| **PlayMode** | `ConsiderationMode` | `AnalysisMode` | `TsumiSearchMode` |
| **USIコマンド** | `go infinite`（無制限）/ `go byoyomi`（秒読み） | `go infinite` + タイマーで `stop`（1手あたり固定時間） | `go mate <time>` / `go mate infinite` |
| **エンジン管理** | `MatchCoordinator` 経由（対局用エンジンと同じ管理） | `AnalysisFlowController` が専用 `Usi` を作成・管理 | `MatchCoordinator` 経由 |
| **結果表示** | 思考タブにリアルタイム表示、盤面に矢印オーバーレイ | 結果一覧テーブル（ドックウィジェット）、評価値グラフ | 思考タブに詰み手順を表示 |
| **MultiPV** | 対応（複数候補手を同時表示） | 対応（`setoption name MultiPV`） | 非対応（1手順のみ） |
| **フロー制御** | `ConsiderationFlowController` | `AnalysisFlowController` + `AnalysisCoordinator` | `TsumeSearchFlowController` |
| **UI制御** | `ConsiderationModeUIController` | `AnalysisResultsPresenter` | なし（思考タブのみ） |

### 9.2 全体構成

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          analysis 層                                     │
│                                                                         │
│  ┌──────────────────────────────┐  ┌──────────────────────────────┐     │
│  │ ConsiderationFlowController  │  │ ConsiderationModeUIController│     │
│  │ 検討モードの開始              │  │ 検討モードのUI状態管理        │     │
│  │ DirectParams → 即座に開始     │  │ 矢印表示、タイマー、MultiPV  │     │
│  └───────────┬──────────────────┘  └──────────────────────────────┘     │
│              │ MatchCoordinator::startAnalysis()                        │
│              ▼                                                          │
│  ┌──────────────────────────────┐                                       │
│  │    MatchCoordinator          │◄── game層（エンジン管理を委譲）        │
│  │    PlayMode に応じてエンジン   │                                       │
│  │    起動・go コマンド送信       │                                       │
│  └──────────────────────────────┘                                       │
│                                                                         │
│  ┌──────────────────────────────┐  ┌──────────────────────────────┐     │
│  │ AnalysisFlowController       │  │ TsumeSearchFlowController    │     │
│  │ 棋譜解析の全体フロー管理       │  │ 詰将棋探索の開始              │     │
│  │ ダイアログ→エンジン→結果表示   │  │ go mate コマンド             │     │
│  └───────────┬──────────────────┘  └──────────────────────────────┘     │
│              │ 所有                                                      │
│              ▼                                                          │
│  ┌──────────────────────────────┐  ┌──────────────────────────────┐     │
│  │ AnalysisCoordinator          │  │ AnalysisResultsPresenter     │     │
│  │ 局面逐次送信とinfo行解析      │  │ 解析結果テーブルのUI構築      │     │
│  │ Idle/Single/Range 状態管理   │  │ 自動スクロール、フォント管理  │     │
│  └──────────────────────────────┘  └──────────────────────────────┘     │
│                                                                         │
│  ┌──────────────────────────────┐                                       │
│  │ KifuAnalysisListModel        │                                       │
│  │ 解析結果の8列テーブルモデル    │                                       │
│  │ 形勢判断・一致判定の計算      │                                       │
│  └──────────────────────────────┘                                       │
└─────────────────────────────────────────────────────────────────────────┘
```

### 9.3 AnalysisCoordinator — エンジン解析の逐次制御

**ソース**: `src/analysis/analysiscoordinator.h`, `src/analysis/analysiscoordinator.cpp`

AnalysisCoordinator は、USIエンジンへの局面送信・思考開始・停止を逐次的に制御する低レベルコーディネータである。エンジンプロセスそのものは所有せず、`requestSendUsiCommand` シグナル経由でコマンドを外部に委譲する。

#### 解析モード（Mode）

```cpp
enum Mode {
    Idle,            // 解析停止中
    SinglePosition,  // 単一局面の解析（検討モードから利用）
    RangePositions   // 範囲解析（棋譜解析から利用）
};
```

#### 状態遷移

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> SinglePosition : startAnalyzeSingle(ply)
    Idle --> RangePositions : startAnalyzeRange()

    SinglePosition --> Idle : bestmove受信\n→ analysisFinished
    SinglePosition --> Idle : stop()

    RangePositions --> RangePositions : bestmove受信\n→ nextPlyOrFinish()\n（次の手へ進む）
    RangePositions --> Idle : 全手完了\n→ analysisFinished
    RangePositions --> Idle : stop()
```

#### Options 構造体

```cpp
struct Options {
    int startPly = 0;       // 解析開始手数
    int endPly = -1;        // 解析終了手数（-1=最終手まで）
    int movetimeMs = 1000;  // 1手あたりの思考時間（ミリ秒）
    int multiPV = 1;        // 候補手の数
    bool centerTree = true; // 分岐ツリーウィジェットの自動中央寄せ
};
```

#### 1手の解析サイクル

```
sendAnalyzeForPly(ply)
  │
  ├── sfenRecord[ply] から position コマンドを構築
  ├── m_pendingPosCmd に保存
  └── emit positionPrepared(ply, sfen)
        │  （GUI が盤面を更新する猶予を与える）
        ▼
sendGoCommand()           ←── FlowController が positionPrepared 受信後に呼ぶ
  │
  ├── send(m_pendingPosCmd)     // "position sfen ..." を送信
  ├── send("go infinite")       // 思考開始
  └── m_stopTimer.start(movetimeMs)  // タイマー開始
        │
        ▼ movetimeMs 経過
onStopTimerTimeout()
  │
  └── send("stop")              // 思考停止を要求
        │
        ▼ エンジンが応答
onEngineBestmoveReceived()
  │
  └── nextPlyOrFinish()
        ├── 次の手あり → sendAnalyzeForPly(ply + 1)
        └── 全手完了   → emit analysisFinished(Idle)
```

#### info 行の解析

`onEngineInfoLine()` でUSIの `info` 行を正規表現で解析し、`analysisProgress` シグナルで結果を伝搬する。

```cpp
// 解析される主要フィールド
struct ParsedInfo {
    int depth;      // 探索深さ
    int seldepth;   // 選択的探索深さ
    int scoreCp;    // 評価値（centipawn）
    int mate;       // 詰み手数（正=自分の勝ち、負=相手の勝ち）
    QString pv;     // 読み筋（USI形式の指し手列）
};
```

5つの `QRegularExpression` を `Q_GLOBAL_STATIC_WITH_ARGS` で静的に定義し、パフォーマンスを最適化している。

#### シグナル一覧

| シグナル | 発火タイミング | 用途 |
|---------|--------------|------|
| `requestSendUsiCommand(line)` | USIコマンド送信時 | 外部の `Usi::sendRaw()` に接続 |
| `analysisStarted(startPly, endPly, mode)` | 解析開始時 | UI の状態更新 |
| `analysisFinished(mode)` | 解析完了・中止時 | UI のリセット |
| `analysisProgress(ply, depth, seldepth, scoreCp, mate, pv, rawInfoLine)` | info 行受信時 | 結果の蓄積 |
| `positionPrepared(ply, positionCmd)` | 局面準備完了時 | 盤面表示の更新 |

### 9.4 ConsiderationFlowController — 検討モードのフロー制御

**ソース**: `src/analysis/considerationflowcontroller.h`, `src/analysis/considerationflowcontroller.cpp`

検討モード（Consideration）の開始処理を担う薄いフローコントローラである。AnalysisCoordinator は使用せず、`MatchCoordinator::startAnalysis()` に直接委譲する点が棋譜解析との大きな違いである。

#### 検討モードの開始フロー

```
ConsiderationFlowController::runDirect(deps, params, positionStr)
  │
  ├── 1. バリデーション
  │     ├── match が null → エラー通知して return
  │     └── positionStr が空 → エラー通知して return
  │
  ├── 2. エンジンパス取得
  │     └── EngineListSettings::loadEngines() の登録一覧から
  │         名前（名前が空なら番号）で実行ファイルパスを引く
  │
  ├── 3. 秒読み設定
  │     └── byoyomiSec → byoyomiMs 変換（0 = 無制限）
  │
  ├── 4. コールバック通知
  │     ├── onTimeSettingsReady(unlimited, byoyomiSec)
  │     └── onMultiPVReady(multiPV)
  │
  └── 5. startAnalysis()
        │
        └── MatchCoordinator::startAnalysis(AnalysisOptions)
              │
              ├── enginePath, engineName  ← エンジン設定
              ├── positionStr             ← "position sfen ..."
              ├── byoyomiMs              ← 思考時間
              ├── multiPV                ← 候補手数
              ├── mode = ConsiderationMode
              ├── considerationModel     ← 思考情報モデル
              ├── previousFileTo/RankTo  ← 「同」表記用の前手座標
              └── lastUsiMove            ← ハイライト用の最終手
```

#### DirectParams 構造体

```cpp
struct DirectParams {
    int engineIndex = 0;      // エンジン番号（登録一覧の位置。engineName が空のときだけ使う）
    QString engineName;        // エンジン名（登録一覧から実行ファイルを引く）
    bool unlimitedTime = true; // 無制限思考
    int byoyomiSec = 20;      // 秒読み時間（秒）
    int multiPV = 1;           // 候補手数
    int previousFileTo = 0;    // 前手の移動先の筋（1-9）
    int previousRankTo = 0;    // 前手の移動先の段（1-9）
    QString lastUsiMove;       // 最終手（USI形式、ハイライト用）
};
```

#### 検討モードと対局エンジンの関係

検討モードは `MatchCoordinator` を通じてエンジンを管理する。これは対局時と同じエンジン管理機構を使うことで、エンジンの起動・初期化・シャットダウンを一元化している。`PlayMode::ConsiderationMode` を設定することで、`MatchCoordinator` が検討専用の動作（`go infinite` / `go byoyomi` の使い分け）を行う。

### 9.5 ConsiderationModeUIController — 検討モードのUI制御

**ソース**: `src/analysis/considerationmodeuicontroller.h`, `src/analysis/considerationmodeuicontroller.cpp`

検討モードのUI状態（ボタン表示、経過時間、MultiPV、矢印表示）を管理するコントローラである。MainWindow から抽出され、`EngineAnalysisTab`（解析タブウィジェット）と `ShogiView`（盤面ビュー）を操作する。

#### UI状態遷移

```
onModeStarted()
  │ 思考モデルをタブにセット、矢印更新シグナル接続
  ▼
onTimeSettingsReady(unlimited, byoyomiSec)
  │ 時間表示設定、タイマー開始、ボタンを「停止」に切替
  │ MultiPV/停止/開始シグナル接続
  ▼
  ├── （ユーザーが棋譜ナビゲーション）
  │     └── ConsiderationPositionService::handleBuildPositionRequired(row)
  │           MatchCoordinator::updateConsiderationPosition() を呼び出し
  │
  ├── （ユーザーがMultiPV変更）
  │     └── onMultiPVChanged() → emit multiPVChangeRequested
  │
  └── onModeEnded()
        タイマー停止、ボタンを「開始」に戻す、矢印クリア
```

#### 矢印表示のロジック

`updateArrows()` は `ShogiEngineThinkingModel`（検討モデル）の各行から読み筋の最初の指し手を取得し、盤面上に矢印を描画する。

```
updateArrows()
  │
  ├── 矢印表示が無効 or モデルが空 → clearArrows() して return
  │
  └── モデルの各行をイテレーション
        │
        ├── PV文字列から最初のUSI指し手を抽出（スペース区切りの先頭）
        │
        ├── parseUsiMove() で座標に変換
        │     ├── 通常手: "7g7f" → from(7,7), to(7,6)
        │     └── 駒打ち: "P*3c" → from=なし, to(3,3), dropPiece='P'/'p'
        │           └── 手番判定: SFEN の " b " / " w " で大文字/小文字を決定
        │
        └── Arrow オブジェクト生成
              ├── row 0: QColor(255, 0, 0, 200)   — 最善手（濃い赤）
              └── row 1+: QColor(255, 100, 100, 150) — 次善手以降（薄い赤）
```

#### 棋譜ナビゲーション中の検討位置更新

ユーザーが棋譜欄で手を選択すると、`RecordNavigationHandler` の `buildPositionRequired(row)` を受けた `ConsiderationPositionService::handleBuildPositionRequired(row)` が検討対象の局面を切り替える。`ConsiderationPositionResolver::resolve(line, row, fallbackSfen)` が表示中の手順（`KifuNavigationState::currentRecordLine()`）から position コマンドと直前の手（読み筋の「同」の判定に使う移動先と USI の指し手）を決め、`MatchCoordinator::updateConsiderationPosition()` で新しい局面をエンジンに送信する。

### 9.6 AnalysisFlowController — 棋譜解析の全体フロー

**ソース**: `src/analysis/analysisflowcontroller.h`, `src/analysis/analysisflowcontroller.cpp`

棋譜解析モードの全体ワークフローを管理する最上位のフローコントローラである。ダイアログ表示からエンジン起動、`AnalysisCoordinator` による逐次解析、結果のモデルへの蓄積、完了通知までの全工程を統括する。

#### Deps 構造体（主要メンバー）

| メンバー | 型 | 必須 | 説明 |
|---------|-----|------|------|
| `sfenRecord` | `QStringList*` | 必須 | 解析する手順の局面列（`DialogCoordinator` が表示中の手順 `RecordLine::sfens` を渡す） |
| `analysisModel` | `KifuAnalysisListModel*` | 必須 | 結果蓄積用モデル |
| `usi` | `Usi*` | 任意 | 外部提供のUSIエンジン（null なら内部で作成） |
| `presenter` | `AnalysisResultsPresenter*` | 任意 | 結果表示プレゼンター |
| `displayError` | `std::function<void(const QString&)>` | 必須 | エラー表示コールバック |
| `recordModel` | `KifuRecordListModel*` | 任意 | 棋譜表示モデル（指し手ラベル抽出用） |
| `usiMoves` | `QStringList*` | 任意 | USI形式の指し手リスト（`RecordLine::usiMoves()`） |
| `activePly` | `int` | — | 解析開始時の表示手数（`RecordLine::currentPly`。分岐ツリーのカーソル） |
| `lineIndex` | `int` | — | 解析対象の分岐ライン |
| `blackPlayerName` | `QString` | — | 先手棋士名（結果ダイアログ用） |
| `whitePlayerName` | `QString` | — | 後手棋士名（結果ダイアログ用） |

#### 棋譜解析の全体シーケンス

```
runWithDialog(deps, parent)
  │
  ├── 1. KifuAnalysisDialog を表示（モーダル）
  │     ├── エンジン選択
  │     ├── 解析範囲（開始手〜終了手）
  │     ├── 1手あたりの思考時間
  │     └── MultiPV 設定
  │
  ├── 2. deps.usi が null なら専用 Usi インスタンスを作成
  │
  └── 3. start(deps, dialog)
        │
        ├── 前回の結果をクリア
        ├── AnalysisCoordinator を作成（または再利用）
        │
        ├── シグナル接続
        │     ├── AC::requestSendUsiCommand → Usi::sendRaw
        │     ├── AC::analysisProgress → this::onAnalysisProgress
        │     ├── AC::positionPrepared → this::onPositionPrepared
        │     ├── AC::analysisFinished → this::onAnalysisFinished
        │     ├── Usi::bestMoveReceived → this::onBestMoveReceived
        │     ├── Usi::infoLineReceived → AC::onEngineInfoLine
        │     ├── Usi::thinkingInfoUpdated → this::onThinkingInfoUpdated
        │     ├── Presenter::stopRequested → this::stop
        │     └── Presenter::rowDoubleClicked → this::onResultRowDoubleClicked
        │
        ├── Usi::startAndInitializeEngineAsync()
        └── AC::startAnalyzeRange()
              │
              ▼ （以下、AnalysisCoordinator の解析サイクル）
              ┌─────────────────────────────────────────┐
              │ positionPrepared(ply, sfen)              │
              │   → onPositionPrepared()                 │
              │     盤面データを Usi にセット              │
              │     前手座標を抽出                        │
              │     sendGoCommand() を呼び出し            │
              ├─────────────────────────────────────────┤
              │ info行受信 → onAnalysisProgress()        │
              │   pending結果を更新（最新の評価値を保持）  │
              ├─────────────────────────────────────────┤
              │ bestmove受信 → onBestMoveReceived()      │
              │   commitPendingResult()                  │
              │     → KifuAnalysisListModel に1行追加    │
              │   nextPlyOrFinish()                      │
              └─────────────────────────────────────────┘
              （startPly〜endPly まで繰り返し）
              │
              ▼
        onAnalysisFinished()
          └── 完了メッセージ表示、stopButton 無効化
```

#### commitPendingResult() — 解析結果の確定

`commitPendingResult()` は1手分の解析が完了した（`bestmove` 受信）タイミングで呼ばれ、蓄積した思考情報を `KifuAnalysisResultsDisplay` オブジェクトに変換して `KifuAnalysisListModel` に追加する。

| 処理 | 説明 |
|------|------|
| 定跡手判定 | info 行が0件（`m_pendingScoreCp == INT_MIN`）なら「定跡」として処理 |
| 評価値符号反転 | 後手番（奇数手）の場合、エンジン出力の評価値符号を反転して先手基準に統一 |
| 詰み表示 | `mate` 値がある場合、`"詰み N手"` 形式に変換（符号も調整） |
| 読み筋選択 | 漢字PV（`m_pendingPvKanji`）を優先、なければUSI PVを使用 |
| 候補手抽出 | 前行の読み筋の最初の指し手を候補手として設定（エンジンが推奨した手） |
| 「同」表記の正規化 | 候補手の "同" の後のスペースを除去して一致判定を正確に |
| 評価値差 | 前手の評価値との差分を計算 |

#### PV盤面ダイアログ

結果テーブルの「盤面」列（列6）をクリックすると `onResultRowDoubleClicked()` が発火し、`PvBoardDialog` を開く。このダイアログは、その局面のSFENから読み筋の指し手を順に再生できるミニ盤面ビューアである。

### 9.7 TsumeSearchFlowController — 詰将棋探索

**ソース**: `src/analysis/tsumesearchflowcontroller.h`, `src/analysis/tsumesearchflowcontroller.cpp`

詰将棋探索モードのフローコントローラである。現在の局面を `go mate` コマンドで解析し、詰み手順を探索する。

#### 探索開始フロー

```
runWithDialog(deps, parent)
  │
  ├── 1. バリデーション
  │     └── match が null → return
  │
  ├── 2. 局面文字列の構築
  │     └── buildPositionForMate(deps)
  │           ├── 途中局面（line.currentPly > 0）→ deps.line.positionCommand(ply)
  │           │     └── "position startpos moves 7g7f 3c3d ..." 形式
  │           └── 開始局面・手順なし → TsumePositionUtil::buildPositionForMate(sfen)
  │                 └── "position sfen ..." 形式（玉の有無から攻方を決めて手番を合わせる）
  │
  ├── 3. TsumeShogiSearchDialog を表示（モーダル）
  │     ├── エンジン選択
  │     └── 探索時間設定
  │
  └── 4. startAnalysis()
        │
        └── MatchCoordinator::startAnalysis(AnalysisOptions)
              ├── mode = TsumiSearchMode
              ├── enginePath, engineName
              ├── positionStr  ← 構築した局面文字列
              └── byoyomiMs   ← 探索制限時間
```

#### USIコマンドの違い

通常の解析が `go infinite` + タイマーで `stop` を送信するのに対し、詰将棋探索は `go mate <time>` コマンドを使用する。エンジンは以下の3種類の応答を返す。

| 応答 | 意味 | 対応シグナル |
|------|------|------------|
| `checkmate <moves>` | 詰み発見、手順を返す | `checkmateSolved` |
| `checkmate nomate` | 不詰（詰みなし） | `checkmateNoMate` |
| `checkmate notimplemented` | go mate 未対応 | `checkmateNotImplemented` |

#### position 文字列の構築戦略

`TsumeSearchFlowController::Deps` は `match`・`line`（表示中の手順 `RecordLine`）・`fallbackSfen`（手順が無いときの局面）・`onError` を持つ。`buildPositionForMate()` は2つの方式を持つ。

1. **moves形式（途中局面）**: `line.currentPly > 0` のとき、`RecordLine::positionCommand(ply)` で `"position startpos moves 7g7f 3c3d ..."` のように指し手列で局面を表現する。エンジンが局面の履歴情報（千日手判定等）を利用できる。

2. **SFEN形式（開始局面）**: 開始局面（詰将棋の問題など）や手順が無いときは、`TsumePositionUtil::buildPositionForMate(sfenLike)` が1つの局面（`line.startSfen()` か `fallbackSfen`）から `"position sfen <sfen>"` を作る。玉の有無から攻方を推定し、手番を合わせる。

### 9.8 AnalysisResultsPresenter — 解析結果の表示

**ソース**: `src/analysis/analysisresultspresenter.h`, `src/analysis/analysisresultspresenter.cpp`

棋譜解析結果を表示するドックウィジェットのMVPプレゼンターである。テーブルビューの構築、列幅管理、自動スクロール、フォントサイズ調整を担当する。

#### UIコンポーネント構成

```
QDockWidget
  └── QWidget (container)
        └── QVBoxLayout
              ├── QHBoxLayout
              │     ├── QPushButton "A-"  (フォント縮小)
              │     └── QPushButton "A+"  (フォント拡大)
              ├── QTableView
              │     ├── 交互行色、単一選択
              │     ├── 列 3,5: NumericRightAlignCommaDelegate（数値右寄せ）
              │     └── 列 6: BoardButtonDelegate（「表示」ボタン）
              └── QPushButton "棋譜解析中止"
```

#### 解析中の動作制御

| 状態 | テーブル選択 | 自動スクロール | 「表示」ボタン | 停止ボタン |
|------|------------|--------------|--------------|----------|
| 解析中（`m_isAnalyzing = true`） | `NoSelection` | 最新行に自動スクロール＆選択 | クリック無効 | 有効 |
| 解析完了（`m_isAnalyzing = false`） | `SingleSelection` | なし | クリック有効 | 無効 |

解析中は `NoSelection` モードでユーザーの選択操作を抑制しつつ、`onRowsInserted()` で一時的に `SingleSelection` に切り替えて最新行を選択・スクロールし、すぐに `NoSelection` に戻す。`m_autoSelecting` フラグにより、この自動選択が `rowSelected` シグナルを発火しないよう制御する。

#### 列幅の自動調整

`reflowNow()` はタイマー（`QTimer::singleShot(0)`）で遅延実行され、モデル変更のバーストに対して効率的にリフロー処理を行う。各列に最小幅（列0: 100, 列1: 100, 列2: 40, 列3: 70, 列4: 90, 列5: 50, 列6: 50）を設定し、最終列（列7: 読み筋）はストレッチモードで残りの幅を使い切る。

#### フォントサイズの永続化

A-/A+ ボタンで8pt〜24ptの範囲でフォントサイズを調整でき、変更のたびに `AnalysisSettings::kifuAnalysisFontSize`（`KifuAnalysis/fontSize`）へ保存される。保存値が範囲外（宣言の既定値 0 を含む）のときは表の現在のフォントサイズ（10pt未満なら10pt）を使う。列幅は `AnalysisSettings::kifuAnalysisColumnWidths()` / `setKifuAnalysisColumnWidths()` で保存・復元する。ドックの位置・大きさ・浮動・表示は、ほかのドックと同じく `DockSettings::kifuAnalysisResultsDockGeometry` などに保存し、起動時に `DockCreationService::restoreDockState()` が戻す。

### 9.9 KifuAnalysisListModel — 解析結果テーブルモデル

**ソース**: `src/analysis/kifuanalysislistmodel.h`, `src/analysis/kifuanalysislistmodel.cpp`

`AbstractListModel<KifuAnalysisResultsDisplay>` を継承した8列テーブルモデルである。解析結果の各行を `KifuAnalysisResultsDisplay` オブジェクトとして保持する。

#### 列定義

| 列 | ヘッダー（日本語） | ヘッダー（英語） | データソース | 配置 |
|----|------------------|-----------------|-------------|------|
| 0 | 指し手 | Move | `currentMove()` | 左寄せ |
| 1 | 候補手 | Candidate | `candidateMove()` | 左寄せ |
| 2 | 一致 | Match | `isMoveMatch()` で計算 | 中央 |
| 3 | 評価値 | Eval | `evaluationValue()` | 右寄せ |
| 4 | 形勢 | Judgment | `getJudgementString()` で計算 | 左寄せ |
| 5 | 差 | Diff | `evaluationDifference()` | 右寄せ |
| 6 | 盤面 | Board | 固定文字列 "表示" | — |
| 7 | 読み筋 | PV | `principalVariation()` | 左寄せ |

#### isMoveMatch() — 一致判定

指し手（列0）と候補手（列1）が同じかを判定するヘルパー関数。手番マーカー（▲/△）を除去し、"同" の後のスペース差異を正規化した上で文字列比較する。エンジンの最善手と実際の指し手が一致していれば "○" が表示される。

#### getJudgementString() — 形勢判断

評価値（centipawn）から日本語の形勢判断文字列を生成する。

```
|score| <= 100  → "互角"           （Even）
|score| <= 300  → "[side]やや有利"   （Slightly better）
|score| <= 600  → "[side]有利"      （Better）
|score| <= 1000 → "[side]優勢"      （Superior）
|score| > 1000  → "[side]勝勢"      （Winning）
詰み            → "[side]勝ち"      （Checkmate win）
```

`[side]` は "先手" または "後手" であり、評価値の符号で判定する（正=先手有利、負=後手有利）。

#### KifuAnalysisResultsDisplay — 結果データクラス

**ソース**: `src/widgets/kifuanalysisresultsdisplay.h`

1手分の解析結果を保持するデータオブジェクト。`QObject` を継承し、`KifuAnalysisListModel` のアイテムとして使用される。

| フィールド | 型 | 説明 |
|-----------|-----|------|
| `m_currentMove` | `QString` | 実際の指し手（漢字表記） |
| `m_candidateMove` | `QString` | エンジン推奨手（前行の読み筋の先頭手） |
| `m_evaluationValue` | `QString` | 評価値（centipawn、先手基準に統一済み） |
| `m_evaluationDifference` | `QString` | 前手との評価値差 |
| `m_principalVariation` | `QString` | 読み筋（漢字表記優先） |
| `m_usiPv` | `QString` | 読み筋（USI形式、PvBoardDialog用） |
| `m_sfen` | `QString` | この局面のSFEN文字列 |
| `m_lastUsiMove` | `QString` | 盤面ハイライト用の最終手 |
| `m_candidateUsiMove` | `QString` | 候補手のUSI形式（矢印表示等に利用可能） |

### 9.10 エンジン管理の違い：MatchCoordinator 経由 vs 直接 Usi

analysis層のフローコントローラは、エンジンとの通信経路が2つに分かれている。

```
検討モード / 詰将棋探索
  │
  └── FlowController → MatchCoordinator::startAnalysis()
                          │
                          └── 内部で Usi を起動・管理
                              go infinite / go mate を送信

棋譜解析モード
  │
  └── AnalysisFlowController
        │
        ├── 専用 Usi インスタンスを作成・所有（または外部提供）
        ├── AnalysisCoordinator::requestSendUsiCommand → Usi::sendRaw
        └── 独自の position/go infinite/stop サイクルを実行
```

**検討・詰将棋探索**: `MatchCoordinator` 経由でエンジンを管理する。対局時と同じエンジン管理機構を利用するため、エンジンの起動・初期化・シャットダウンが統一されている。

**棋譜解析**: 複数局面を逐次解析する必要があり、`AnalysisCoordinator` が独自の position/go/stop サイクルを制御する。エンジンは `AnalysisFlowController` が直接 `Usi` クラスを作成・管理するか、外部から注入されたものを使用する。

<!-- chapter-9-end -->

---

<!-- chapter-10-start -->
## 第10章 UI層：プレゼンテーション

`src/ui/` 配下には、MainWindow からビジネスロジックを分離するための4カテゴリのクラス群が配置されている。本章ではそれぞれの役割分担と、各クラスの責務を解説する。

### 10.1 4カテゴリの役割分担

UI層は **Presenters**（データ→表示変換）、**Controllers**（UIイベント処理）、**Wiring**（シグナル/スロット接続の一括管理）、**Coordinators**（複数オブジェクト間の調整）の4カテゴリで構成される。

```mermaid
graph TB
    subgraph UI層 4カテゴリ
        direction TB

        subgraph Presenters["Presenters（4クラス）"]
            direction LR
            P1[BoardSyncPresenter]
            P2[EvalGraphPresenter]
            P4[GameRecordPresenter]
            P5[TimeDisplayPresenter]
        end

        subgraph Controllers["Controllers（10クラス）"]
            direction LR
            C1[BoardSetupController]
            C2[PlayerInfoController]
            C3[EvaluationGraphController]
            C4[ReplayController]
            C5[TimeControlController]
            C6["... 他5クラス"]
        end

        subgraph Wiring["Wiring（8クラス）"]
            direction LR
            W1[UiActionsWiring]
            W2[CsaGameWiring]
            W3[ConsiderationWiring]
            W4[PlayerInfoWiring]
            W5["... 他4クラス"]
        end

        subgraph Coordinators["Coordinators（6クラス）"]
            direction LR
            Co1[DialogCoordinator]
            Co2[KifuDisplayCoordinator]
            Co3[PositionEditCoordinator]
            Co4[DockLayoutManager]
            Co5["... 他2クラス"]
        end
    end

    Model["Model層\n(GameController, MatchCoordinator,\nGameRecordModel 等)"]
    View["View層\n(ShogiView, RecordPane,\nEngineAnalysisTab 等)"]
    MW["MainWindow"]

    Model -->|データ変更| Presenters
    Presenters -->|表示更新| View
    View -->|ユーザー操作| Controllers
    Controllers -->|状態変更| Model
    Wiring -.->|connect()| Presenters
    Wiring -.->|connect()| Controllers
    MW -->|ensure*()で生成| Wiring
    MW -->|ensure*()で生成| Coordinators
    Coordinators -->|複数オブジェクト調整| Presenters
    Coordinators -->|複数オブジェクト調整| Controllers
```

#### 各カテゴリの責務

| カテゴリ | 責務 | 方向 | 典型的なパターン |
|---------|------|------|----------------|
| **Presenters** | Model のデータを View に反映する | Model → View | `applySfenAtPly()`, `appendMoveLine()` |
| **Controllers** | UI イベントを受け取り、Model の状態を変更する | View → Model | `onMoveRequested()`, `onPvRowClicked()` |
| **Wiring** | シグナル/スロット接続を一括で行う | 横断的 | `wire()`, `buildUiAndWire()` |
| **Coordinators** | 複数オブジェクト間の複雑な調整を行う | 横断的 | `showConsiderationDialog()`, `onNavigationCompleted()` |

### 10.2 Presenters — データの表示変換

Presenters は「Model → View」の単方向データフローを担当する。Model 側のデータ変更を受け取り、対応する View ウィジェットに反映する。

#### 一覧

| Presenter | ソース | 責務 |
|-----------|--------|------|
| `BoardSyncPresenter` | `boardsyncpresenter.h` | 盤面表示の同期。SFENから盤面を描画し、最終手のハイライトを表示する |
| `EvalGraphPresenter` | `evalgraphpresenter.h` | 評価値グラフのデータ収集。先手/後手エンジンの評価値をリストに蓄積する |
| `GameRecordPresenter` | `gamerecordpresenter.h` | 棋譜リスト表示の管理。棋譜全体の描画、ライブ対局中の1手追加、コメント管理を行う |
| `TimeDisplayPresenter` | `timedisplaypresenter.h` | 対局時計の表示。残り時間の表示、手番に応じたハイライト、秒読み時の緊急表示を行う |

#### BoardSyncPresenter — 盤面同期

**ソース**: `src/ui/presenters/boardsyncpresenter.h`, `src/ui/presenters/boardsyncpresenter.cpp`

棋譜の局面を盤に描き、直前の手をハイライトするプレゼンターである。コンストラクタで `Deps` 構造体（`gc`・`view`・`bic`・`currentLine`）を受け取る。局面は棋譜の正本（分岐ツリーの表示中の手順）から取る。対局中も、対局の手順は `LiveGameSession` が分岐ツリーに積んでいるので同じ経路で描ける。

| メソッド | 説明 |
|---------|------|
| `syncBoardAndHighlightsAtRow(int ply)` | 表示中の手順（`currentLine()`）の `ply` 手目の局面を描き、直前の局面との差分からハイライト（移動元・移動先のマーカー）を表示する。`ply=0`（開始局面）と終局行ではハイライトなし |
| `loadBoardWithHighlights(currentSfen, prevSfen)` | 指定した局面を描き、直前の局面との差分からハイライトする（棋譜欄のクリック・分岐ナビゲーション用）。`prevSfen` が空ならハイライトを消す |

2つのメソッドは同じハイライト処理（`showMoveHighlight()`）を通る。

```
データフロー:

currentLine().sfens[ply] → ShogiBoard::setSfen()
                             → ShogiView::applyBoardAndRender()

sfens[ply-1] と sfens[ply] の差分（SfenDiffUtils::diffBoards）
    → 移動元/移動先を求める（駒打ちは BoardCoordinates::standSquareOf() で駒台のマスを移動元にする）
    → BoardInteractionController::showMoveHighlights(from, to)
```

#### EvalGraphPresenter — 評価値グラフ

**ソース**: `src/ui/presenters/evalgraphpresenter.h`, `src/ui/presenters/evalgraphpresenter.cpp`

名前空間（namespace）として実装されたユーティリティ関数群で、クラスではない。`MatchCoordinator` から先手/後手の評価値（centipawn）を取得し、グラフ用のリストに蓄積する。

```cpp
// 使用例
EvalGraphPresenter::appendPrimaryScore(scoreCp, match);   // 先手エンジンの評価値を追加
EvalGraphPresenter::appendSecondaryScore(scoreCp, match);  // 後手エンジンの評価値を追加
```

`match` が `nullptr` の場合は 0 を追加する。`EvaluationGraphController`（後述）から呼び出される。

#### GameRecordPresenter — 棋譜リスト表示

**ソース**: `src/ui/presenters/gamerecordpresenter.h`, `src/ui/presenters/gamerecordpresenter.cpp`

棋譜リストの表示と選択状態の管理を一手に担う、Presenters の中で最も多機能なクラスである。

| メソッド群 | 説明 |
|-----------|------|
| **全体描画** | `presentGameRecord(disp)` — `KifDisplayItem` のリストから棋譜全体を描画 |
| **ライブ追加** | `appendMoveLine(prettyMove, elapsedTime, beforeSfen, usiMove)` — 対局中に1手ずつ末尾に追加 |
| **待った** | `syncCurrentMoveIndexToLastRow()` — 棋譜欄の末尾を消した後、現在の手数を最後の行に合わせる |
| **コメント取得** | `commentForRow(row)` — 棋譜欄の行のコメントを読む（コメントの正本は分岐ツリーのノード） |
| **選択通知** | `onKifuCurrentRowChanged()` → `currentRowChanged(row, comment)` シグナル発行 |
| **一括操作** | `displayAndWire()` — 棋譜反映 + コメント設定 + 選択変更配線を一度に実行 |

```
選択変更のデータフロー:

ユーザーが棋譜リストの行をクリック
  → QTableView::selectionModel()::currentChanged
     → GameRecordPresenter::onKifuCurrentRowChanged()
        → currentRowChanged(row, comment) シグナル
           → MainWindow::onRecordRowChanged()
              → 盤面更新、分岐候補更新、コメント表示
```

#### TimeDisplayPresenter — 対局時計

**ソース**: `src/ui/presenters/timedisplaypresenter.h`, `src/ui/presenters/timedisplaypresenter.cpp`

対局時計の表示を管理するプレゼンターである。`MatchCoordinator::timeUpdated` シグナルに接続され、残り時間のフォーマットと表示を行う。

| 処理 | 説明 |
|------|------|
| **時間フォーマット** | ミリ秒を `HH:MM:SS` 形式に変換して `ShogiView` に表示 |
| **手番ハイライト** | 現在の手番側のプレイヤー名・時計エリアを強調表示 |
| **秒読み緊急表示** | `ShogiClock` の秒読み状態を検出し、残り時間が少ない場合にスタイルを変更 |

```
MatchCoordinator::timeUpdated(blackMs, whiteMs, sideToMove, urgencyMs)
  → TimeDisplayPresenter::onMatchTimeUpdated()
     → fmt_hhmmss() で時間文字列に変換（最後の値と手番を覚えておく）
     → updateUrgencyStyles() で手番ハイライトと秒読み緊急表示
     → ShogiView に反映

盤を反転したとき（MainWindowAppearanceController::flipBoardAndUpdatePlayerInfo）
  → TimeDisplayPresenter::refresh() で、覚えておいた値と手番で描き直す
```

### 10.3 Controllers — UIイベント処理

Controllers は「View → Model」方向のイベント処理を担当する。ユーザー操作やUIイベントを受け取り、対応するModel層の状態を変更する。全クラスが `QObject` を継承し、setter によるDI（依存性注入）パターンを使用する。

#### 一覧

| Controller | ソース | 責務 | 主な操作対象 |
|-----------|--------|------|------------|
| `BoardSetupController` | `boardsetupcontroller.h` | 盤面操作の配線と指し手処理 | `BoardInteractionController`, `ShogiView` |
| `PlayerInfoController` | `playerinfocontroller.h` | プレイヤー名・エンジン名の管理と表示 | `GameInfoPaneController`, `ShogiView` |
| `EvaluationGraphController` | `evaluationgraphcontroller.h` | 評価値グラフの蓄積と描画 | `EvaluationChartWidget`, `MatchCoordinator` |
| `ReplayController` | `replaycontroller.h` | リプレイモード・ライブ追加モードの管理 | `ShogiClock`, `RecordPane` |
| `TimeControlController` | `timecontrolcontroller.h` | 対局時計のライフサイクルと設定 | `ShogiClock`, `TimeDisplayPresenter` |
| `LanguageController` | `languagecontroller.h` | 言語設定の変更と再起動ダイアログ | `QAction`（言語メニュー） |
| `PvClickController` | `pvclickcontroller.h` | 読み筋行クリック時のPV盤面ダイアログ表示 | `ShogiEngineThinkingModel`, `PvBoardDialog` |
| `UsiCommandController` | `usicommandcontroller.h` | 手動USIコマンドの送信 | `MatchCoordinator`, `EngineAnalysisTab` |
| `NyugyokuDeclarationHandler` | `nyugyokudeclarationhandler.h` | 入玉宣言の判定と結果表示 | `ShogiBoard`, `MatchCoordinator` |
| `JishogiScoreDialogController` | `jishogiscoredialogcontroller.h` | 持将棋スコアダイアログの表示と判定 | `ShogiBoard` |

#### BoardSetupController — 盤面操作

**ソース**: `src/ui/controllers/boardsetupcontroller.h`, `src/ui/controllers/boardsetupcontroller.cpp`

`BoardInteractionController`（`src/board/` 配下の低レベル盤面操作クラス）のライフサイクル管理と、MainWindow への配線を担当する。

主な処理フロー:

```
setupBoardInteractionController()
  ├── 前の BoardInteractionController を切り離して破棄し、作り直す（ShogiView・ShogiGameController を渡す）
  ├── connectBoardClicks()
  │     ShogiView::clicked      → BoardInteractionController::onLeftClick
  │     ShogiView::rightClicked → BoardInteractionController::onRightClick
  └── Ctrl+ホイールで盤の大きさを変えるフィルタを付ける

onMoveRequested(from, to)    ← MainWindowServiceRegistry::handleMoveRequested() から
  ├── 局面編集モード → PositionEditController::applyEditMove()
  ├── 対局していないとき → 棋譜の変化として記録（コールバック経由）
  └── それ以外 → ShogiGameController::validateAndMove()
       ├── BoardInteractionController::onMoveApplied() でハイライトを更新
       ├── HvH・HvE → MatchCoordinator::onHumanMove()
       └── 通信対局 → csaMoveRequested シグナル発行

onMoveCommitted(mover, ply)  ← MainWindowServiceRegistry::handleMoveCommitted() から
  └── EvE のとき、指した側の評価値グラフを描き直す（コールバック経由）
```

着手要求（`BoardInteractionController::moveRequested`）はこのクラスではつながない。`MainWindowSignalRouter::connectMoveRequested()` が `MainWindow::onMoveRequested` につなぎ、`MainWindowServiceRegistry::handleMoveRequested()` が `onMoveRequested()` を呼ぶ（15.2 節）。

コールバック（`std::function`）は、`MainWindowServiceRegistry::ensureBoardSetupController()` が `setEnsurePositionEditCallback()`・`setVariationInputCallbacks()`・`setRedrawEngine1GraphCallback()` 等で渡す。

#### EvaluationGraphController — 評価値グラフ

**ソース**: `src/ui/controllers/evaluationgraphcontroller.h`, `src/ui/controllers/evaluationgraphcontroller.cpp`

評価値スコアの蓄積と `EvaluationChartWidget` の再描画を管理する。`EvalGraphPresenter` から取得した評価値を `m_scoreCp` リストに保持し、タイマーを用いた遅延再描画で描画コストを抑制する。

| メソッド | 説明 |
|---------|------|
| `redrawEngine1Graph(ply)` | 先手エンジンの評価値を追加して再描画をスケジュール |
| `redrawEngine2Graph(ply)` | 後手エンジンの評価値を追加して再描画をスケジュール |
| `trimToPly(maxPly)` | 指定手数以降のスコアを切り捨て（局面戻し時に使用） |
| `clearScores()` | スコアリストを初期化 |

`redrawEngine1Graph()` / `redrawEngine2Graph()` は `QTimer::singleShot(0, ...)` で遅延実行される。連続呼び出しが発生しても、実際の再描画は1回にまとめられる。

グラフの横軸の長さは、`setRecordLengthProvider()` で渡した関数が分岐ツリーの表示中のラインの行数から求める。対局中に評価値を描く手数（`ply` に -1 を渡したとき）は、`setSfenRecord()` で渡した対局の局面列の長さから求める。

#### TimeControlController — 対局時計管理

**ソース**: `src/ui/controllers/timecontrolcontroller.h`, `src/ui/controllers/timecontrolcontroller.cpp`

`ShogiClock` のライフサイクル管理、持ち時間設定の適用、対局情報と棋譜に記録する持ち時間・開始終了日時の保持、タイムアウト処理を担当する。

```
ensureClock()
  └── ShogiClock が未作成なら作成
       └── playerTimeOut(Turn) シグナルを onClockPlayerTimeOut に接続
            （→ MatchCoordinator::handlePlayerTimeOut(時間が切れた側)）

applyTimeControl(tc, startSfen, currentSfen, view)
  ├── recordTimeControl(tc.rules)  ← 記録用の持ち時間（時間無制限なら空の TimeControl）
  ├── TimeControlUtil::applyToClock() で時計に決まりを渡す
  └── TimeDisplayPresenter の表示を更新

beginGameWithTimeControl(timeControl)
  └── CSA通信対局で、サーバーの対局条件を記録用の持ち時間にする

timeControl() / hasTimeControl()
  └── 記録用の持ち時間（TimeControl）と、時間制限があるか
```

記録用の持ち時間も、時計と同じ `TimeControl`（5.5 を参照）で持つ。

#### PvClickController — 読み筋クリック処理

**ソース**: `src/ui/controllers/pvclickcontroller.h`, `src/ui/controllers/pvclickcontroller.cpp`

`EngineAnalysisTab` 上の読み筋（PV）行をクリックした際の処理を担当する。クリックされた行から読み筋データを取得し、`PvBoardDialog`（ミニ盤面ビューア）を表示する。

```
onPvRowClicked(engineIndex, row)
  ├── ShogiEngineThinkingModel から PV データを取得
  ├── resolveCurrentSfen() で読み筋の起点の局面 SFEN を解決
  ├── resolveHighlightContext() で直前の局面と最終手を決める
  │     読み筋の起点の局面が表示中の手順（StateRefs::currentLine）の何手目かを探し、
  │     その直前の局面と、その局面に至った手（record->lastUsiMove() が空なら RecordLine::usiMove(ply)）を使う
  ├── resolvePlayerNames() で先手/後手名を解決
  └── PvBoardDialog を作成・表示
       └── ダイアログ閉じた時 → pvDialogClosed シグナル
```

`StateRefs` は対局モード・対局者名・現在／開始の SFEN・検討モデルと、表示中の手順を返す関数 `currentLine`（`KifuNavigationState::currentRecordLine()`）を持つ。

#### その他のControllers

| Controller | 説明 |
|-----------|------|
| `PlayerInfoController` | プレイヤー名（人間名・エンジン名）の保持と、プレイモードに応じた名前表示の切り替えを管理する。`onPlayerNamesResolved` スロットで名前確定イベントを受け取り、`ShogiView` と `GameInfoPaneController` に反映する |
| `ReplayController` | リプレイモードと途中局面からの再開フラグを管理する。リプレイモードの切り替え時に `ShogiClock` と盤面の手番表示を更新する |
| `LanguageController` | 言語メニュー（システム/日本語/英語）のアクション処理と、言語変更後のアプリケーション再起動ダイアログの表示を担当する |
| `UsiCommandController` | `EngineAnalysisTab` の手動コマンド入力欄からのUSIコマンドを、`MatchCoordinator` 経由でエンジンに送信する。エンジン未起動時はステータスログにエラーを表示する |
| `NyugyokuDeclarationHandler` | 入玉宣言勝ちの判定を行う。24点法・27点法の2ルールで判定し、結果をダイアログで表示した後、`declarationCompleted` シグナルを発行する |
| `JishogiScoreDialogController` | 持将棋スコアの計算結果をダイアログで表示する。各プレイヤーの駒点数・敵陣駒数・王の位置などの条件判定結果を一覧表示する |

### 10.4 Wiring — シグナル/スロット接続の一括管理

#### なぜ Wiring クラスに分離するのか

ShogiBoardQ では多数のシグナル/スロット接続が必要であり、これを MainWindow の1メソッドに記述すると以下の問題が生じる。

1. **可読性**: 数十〜数百行の `connect()` 文が並ぶと、どの接続がどの機能に関係するのか把握しにくい
2. **ライフサイクル**: 機能ごとに接続の生成・解除タイミングが異なる（例: CSA対局中のみ有効な接続）
3. **テスト性**: 接続ロジックが独立したクラスにあれば、依存オブジェクトのモック差し替えが容易になる
4. **MainWindow の肥大化防止**: CLAUDE.md の設計原則「MainWindow should stay lean」に従う

Wiring クラスは、関連するシグナル/スロット接続を機能単位でグループ化し、`wire()` または `buildUiAndWire()` メソッドで一括実行する。

#### 共通パターン

```cpp
// 典型的な Wiring クラスの構造
class SomeWiring : public QObject {
public:
    struct Deps {
        QWidget* parentWidget;
        SomeController* controller;
        SomeView* view;
        // ...
    };

    explicit SomeWiring(const Deps& d, QObject* parent = nullptr);
    void wire();           // シグナル/スロット接続を実行
    // または
    SomeWidget* buildUiAndWire();  // UI部品を生成して接続
};
```

#### 一覧

| Wiring | ソース | 接続する対象 | 概要 |
|--------|--------|------------|------|
| `UiActionsWiring` | `uiactionswiring.h` | QAction → 担当クラスのスロット | メニュー/ツールバーの全アクションを、4つの子（ファイル・対局・編集・表示）で接続 |
| `AnalysisTabWiring` | `analysistabwiring.h` | EngineAnalysisTab ↔ ThinkingModel | 解析タブの生成とモデル配線 |
| `ConsiderationWiring` | `considerationwiring.h` | 検討ダイアログ ↔ MatchCoordinator ↔ ShogiView | 検討モード開始/停止/矢印表示の全フロー |
| `CsaGameWiring` | `csagamewiring.h` | CsaGameCoordinator ↔ UI各部 | CSA通信対局のシグナル接続・解除 |
| `PlayerInfoWiring` | `playerinfowiring.h` | PlayerInfoController ↔ GameInfoPane ↔ ShogiView | プレイヤー情報・対局情報タブの管理 |
| `MenuWindowWiring` | `menuwindowwiring.h` | MenuBar → MenuWindow | メニューウィンドウの遅延生成と表示 |
| `JosekiWindowWiring` | `josekiwindowwiring.h` | JosekiWindow ↔ GameController ↔ BoardInteraction | 定跡ウィンドウの生成とUSI手→盤面操作の変換 |
| `RecordPaneWiring` | `recordpanewiring.h` | RecordPane ↔ KifuModels | 棋譜ペインの生成とモデル配線 |

#### UiActionsWiring — メニュー/ツールバー接続

**ソース**: `src/ui/wiring/uiactionswiring.h`, `src/ui/wiring/uiactionswiring.cpp`

`Ui::MainWindow` が持つ全ての `QAction`（ファイル操作、対局、盤操作、解析、表示など）を、担当するクラスのスロットに接続する。`MainWindowSignalRouter::connectAllActions()` が起動時に1回だけ作って `wire()` を呼ぶ。

```
wire()
  ├── FileActionsWiring  → KifuFileController（開く・保存）、DialogLaunchWiring、MainWindow（終了）
  ├── GameActionsWiring  → GameSessionOrchestrator（対局・投了・中断・再開）、DialogLaunchWiring、
  │                        DialogCoordinatorWiring（棋譜解析の中止）、MainWindow（新規・局面編集）
  ├── EditActionsWiring  → KifuExportController（棋譜・局面のコピー）、KifuFileController（貼り付け）
  └── ViewActionsWiring  → MainWindowAppearanceController（盤の拡大・縮小・反転）、MainWindow（待った）など
```

#### ConsiderationWiring — 検討モード配線

**ソース**: `src/ui/wiring/considerationwiring.h`, `src/ui/wiring/considerationwiring.cpp`

検討モード（局面検討）に関するUI配線を集約した、Wiring クラスの中で最も複雑なクラスである。検討ダイアログの表示、エンジン設定変更、矢印表示の制御、局面変更時の再送信など、検討モードのUIフロー全体を管理する。

```
displayConsiderationDialog()
  ├── ensureDialogCoordinator() コールバック
  ├── DialogCoordinator::startConsiderationFromContext()
  └── シグナル接続
       ├── onModeStarted() → 検討モデルクリア、矢印表示初期化
       ├── onTimeSettingsReady() → エンジンへの go コマンド準備
       ├── onDialogMultiPVReady() → MultiPV 設定反映
       ├── onEngineChanged() → エンジン変更時の再初期化
       └── updateArrows() → 矢印表示更新
```

`updatePositionIfNeeded()` は棋譜ナビゲーション中に呼ばれ、検討対象の局面をエンジンに再送信する。

#### CsaGameWiring — CSA通信対局配線

**ソース**: `src/ui/wiring/csagamewiring.h`, `src/ui/wiring/csagamewiring.cpp`

CSA通信対局に特有のシグナル/スロット接続を管理する。`wire()` で対局開始時に接続し、`unwire()` で対局終了時に解除する動的な接続ライフサイクルを持つ。

```
wire()                              unwire()
  ├── onGameStarted                   └── 全ての connect() を disconnect()
  ├── onMoveMade
  ├── onTurnChanged
  ├── onGameEnded
  └── onLogMessage

startCsaGame(dialog, parent)
  ├── CsaGameCoordinator を生成（または再利用）
  ├── CsaGameDialog の設定を Coordinator に渡す
  ├── wire() で接続
  └── Coordinator::start() で対局開始
```

**シグナル群**: `disableNavigationRequested`, `enableNavigationRequested`, `appendKifuLineRequested` など、MainWindow 側のスロットに接続されるシグナルを発行する。

#### PlayerInfoWiring — プレイヤー情報配線

**ソース**: `src/ui/wiring/playerinfowiring.h`, `src/ui/wiring/playerinfowiring.cpp`

`PlayerInfoController` と `GameInfoPaneController` の初期化・管理を担当する。対局情報タブの追加・更新、プレイヤー名・エンジン名の設定フック処理を行う。

| メソッド | 説明 |
|---------|------|
| `addGameInfoTabAtStartup()` | 起動時に対局情報タブを追加 |
| `ensureGameInfoController()` | `GameInfoPaneController` を遅延生成 |
| `resolveNamesAndSetupGameInfo(...)` | 対局者名確定時に対局情報も一括設定 |
| `updateGameInfoForCurrentMatch()` | 現在の対局状態に基づいて対局情報タブを更新 |

#### その他の Wiring クラス

| Wiring | 説明 |
|--------|------|
| `AnalysisTabWiring` | `EngineAnalysisTab`（エンジン思考情報表示タブ）とその内部モデル（`ShogiEngineThinkingModel` x2、`UsiCommLogModel` x2）の生成と配線を行う。`buildUiAndWire()` でタブUI部品を作成し、モデルを設定して返す |
| `MenuWindowWiring` | メニューウィンドウの遅延生成と表示を管理する。`ensureMenuWindow()` で初回のみ `MenuWindow` を作成し、メニューバーからアクション情報を収集してカテゴリ別に分類する |
| `JosekiWindowWiring` | 定跡ウィンドウの生成・表示と、定跡手選択時のUSI文字列→盤面座標変換を行う。`onJosekiMoveSelected()` でUSI文字列（例: `"7g7f"`）を `UsiMove::parse()` と `BoardCoordinates` で `QPoint` に変換し、`moveRequested` シグナルを発行する。着手できたかは局面が変わったかで判定する。定跡へのマージには、依存の `currentLine`（表示中の手順 `RecordLine`）の局面・USI の指し手・表記を渡す（`JosekiPresenter::buildMergeEntries()` の表記の一覧は `[i]` が i+1 手目） |
| `RecordPaneWiring` | `RecordPane`（棋譜表示パネル）の生成と、`KifuRecordListModel`・`KifuBranchListModel` のモデル配線を行う。`buildUiAndWire()` でパネルを作成し、モデルを設定する |

### 10.5 Coordinators — 複数オブジェクト間の調整

Coordinators は、複数のオブジェクトをまたぐ複雑な処理フローを調整する。単一の Controller や Presenter では対処できない、オブジェクト間の協調動作をカプセル化する。

#### 一覧

| Coordinator | ソース | 役割 |
|------------|--------|------|
| `DialogCoordinator` | `dialogcoordinator.h` | ダイアログ表示の統合窓口。検討・詰将棋探索・棋譜解析のダイアログ起動を管理 |
| `KifuDisplayCoordinator` | `kifudisplaycoordinator.h` | 棋譜表示の統合UI更新。棋譜リスト・分岐ツリー・分岐候補を同期的に更新 |
| `PositionEditCoordinator` | `positioneditcoordinator.h` | 局面編集モードの開始/終了フローと関連UIの状態管理 |
| `AboutCoordinator` | `aboutcoordinator.h` | バージョン情報ダイアログとプロジェクトWebサイトの表示 |
| `DockLayoutManager` | `docklayoutmanager.h` | ドックウィジェットのレイアウト管理、保存・復元・ロック |

#### DialogCoordinator — ダイアログ統合窓口

**ソース**: `src/ui/coordinators/dialogcoordinator.h`, `src/ui/coordinators/dialogcoordinator.cpp`

各種ダイアログの表示を統合管理する Coordinator である。MainWindow から直接ダイアログを操作する代わりに、本クラスを経由することで、ダイアログの生成・パラメータ設定・シグナル接続を一元化する。

**管理するダイアログ群:**

| ダイアログ | メソッド | 説明 |
|-----------|---------|------|
| バージョン情報 | `showVersionInformation()` | `AboutCoordinator` に委譲 |
| エンジン設定 | `showEngineSettingsDialog()` | USIエンジンの設定ダイアログ |
| 成り確認 | `showPromotionDialog()` | 駒を成るかどうかの選択 |
| 対局終了 | `showGameOverMessage(title, msg)` | 対局結果の表示 |
| 検討 | `startConsiderationFromContext()` | Context構造体から検討ダイアログを起動 |
| 詰将棋探索 | `showTsumeSearchDialogFromContext()` | Context構造体から探索ダイアログを起動 |
| 棋譜解析 | `showKifuAnalysisDialogFromContext()` | Context構造体から解析ダイアログを起動 |
| エラー | `showErrorMessage(msg)` | エラーメッセージ表示 |

**Context パターン**: 検討・詰将棋探索・棋譜解析の3つの解析ダイアログは、`set*Context()` で必要な依存オブジェクトを事前に設定し、`show*FromContext()` で起動する2段階パターンを採用している。

```
// Context パターンの使用例
dialogCoordinator->setConsiderationContext({
    .match = match,
    .currentSfen = currentSfen,
    .enginePath = enginePath,
    // ...
});
dialogCoordinator->startConsiderationFromContext();
```

#### KifuDisplayCoordinator — 棋譜表示の統合UI更新

**ソース**: `src/ui/coordinators/kifudisplaycoordinator.h`, `src/ui/coordinators/kifudisplaycoordinator.cpp`

棋譜ナビゲーション時に、棋譜リスト・分岐ツリー・分岐候補リスト・盤面表示の4つのUI要素を同期的に更新する、Coordinators の中核クラスである。

```
KifuNavigationController からのシグナル
  │
  ▼
KifuDisplayCoordinator
  ├── onNavigationCompleted(node)
  │     ├── RecordPane: 棋譜リストの選択行を更新
  │     ├── BranchTreeManager: ツリー上の強調表示を更新
  │     ├── KifuBranchListModel: 分岐候補を更新
  │     └── boardWithHighlightsRequired シグナル → 盤面更新
  │
  ├── onBranchCandidateActivated(index)
  │     └── 分岐候補リストの選択 → KifuNavigationController で分岐に移動
  │
  ├── onBranchTreeNodeClicked(lineIndex, ply)
  │     └── ツリーノードのクリック → KifuNavigationController で該当手に移動
  │
  └── ライブ対局更新
        ├── onLiveGameMoveAdded(ply, displayText) → 棋譜リストに1手追加
        ├── onLiveGameSessionStarted(branchPoint) → ライブモード開始
        ├── onLiveGameCommitted(newLineEnd) → 対局結果を棋譜ツリーに確定
        └── onLiveGameDiscarded() → 対局途中で中止
```

**整合性検証**: `verifyDisplayConsistency()` と `getConsistencyReport()` はデバッグ用メソッドで、棋譜リスト・分岐ツリー・分岐候補の表示状態が内部データと一致しているかを検証する。

#### PositionEditCoordinator — 局面編集フロー

**ソース**: `src/ui/coordinators/positioneditcoordinator.h`, `src/ui/coordinators/positioneditcoordinator.cpp`

局面編集モードの開始・終了フローを管理する。編集開始時のUI状態保存、編集用アクションの有効化、終了時の復元をコールバック注入パターンで実現する。

```
beginPositionEditing()
  ├── 表示中の手順（navState->currentRecordLine()）から BeginEditContext を作る
  │     （displayedSfen = 表示中の局面、recordStartSfen = 表示中の棋譜の開始局面）
  ├── PositionEditController::beginPositionEditing(ctx) で編集モード開始
  ├── positionEditingStarted シグナル → 棋譜を編集元の局面から作り直す（resetRecordForEditedPosition）
  ├── 編集用 QAction を有効化（EditActions 構造体）
  │     ├── actionReturnAllPiecesToStand（全駒台に戻す）
  │     ├── actionSetHiratePosition（平手初期配置）
  │     ├── actionSetTsumePosition（詰将棋配置）
  │     ├── actionChangeTurn（手番変更）
  │     └── actionSwapSides（先後入替）
  └── applyEditMenuStateCallback(true) でメニュー状態更新

finishPositionEditing()
  ├── PositionEditController::finishPositionEditing(ctx) で編集モード終了（編集後の SFEN を startSfenStr に反映）
  ├── positionEditingFinished シグナル → 棋譜（分岐ツリー・対局の局面列）を編集後の局面から作り直す（resetRecordForEditedPosition）
  ├── 編集用 QAction を無効化
  └── applyEditMenuStateCallback(false) でメニュー状態復元
```

#### DockLayoutManager — ドックレイアウト管理

**ソース**: `src/ui/coordinators/docklayoutmanager.h`, `src/ui/coordinators/docklayoutmanager.cpp`

`QMainWindow` のドックウィジェットレイアウトを管理する。レイアウトの保存・復元・ロック機能を提供し、ユーザーが自由にカスタマイズしたウィンドウ配置を永続化する。

**管理するドック種別** (`DockType` enum):

| ドック | 説明 |
|--------|------|
| `Menu` | メニューウィンドウ |
| `Joseki` | 定跡ウィンドウ |
| `Record` | 棋譜ペイン |
| `GameInfo` | 対局情報 |
| `Thinking` | エンジン思考情報 |
| `Consideration` | 検討モデル |
| `UsiLog` | USI通信ログ |
| `CsaLog` | CSA通信ログ |
| `Comment` | コメント |
| `BranchTree` | 分岐ツリー |
| `EvalChart` | 評価値グラフ |
| `AnalysisResults` | 棋譜解析結果 |

| メソッド群 | 説明 |
|-----------|------|
| `saveLayoutAs()` | 現在のレイアウトに名前をつけて保存 |
| `restoreLayout(name)` | 名前付きレイアウトを復元 |
| `resetToDefault()` | デフォルトレイアウトにリセット |
| `setAsStartupLayout(name)` | 起動時に自動適用するレイアウトを設定 |
| `setDocksLocked(locked)` | ドックの移動・リサイズを禁止/許可 |
| `wireMenuActions(...)` | メニューアクションとの接続 |

#### AboutCoordinator — バージョン情報

**ソース**: `src/ui/coordinators/aboutcoordinator.h`, `src/ui/coordinators/aboutcoordinator.cpp`

名前空間（namespace）として実装された2つのユーティリティ関数を提供する。

| 関数 | 説明 |
|------|------|
| `showVersionDialog(parent)` | バージョン情報ダイアログを表示 |
| `openProjectWebsite()` | デフォルトブラウザでプロジェクトWebサイトを開く |

### 10.6 UI層全体のデータフロー

以下に、ユーザーが「対局中に指し手を実行する」場面での、UI層各カテゴリの連携を示す。

```
ユーザーが盤面をクリック
  │
  ▼ [View層]
ShogiView::clicked(point) シグナル
  │
  ▼ [Wiring: BoardSetupController::connectBoardClicks() が接続]
BoardInteractionController::onLeftClick(point)
  │  （1回目で駒を選び、2回目で移動先を決める）
  ▼
BoardInteractionController::moveRequested(from, to) シグナル
  │
  ▼ [Wiring: MainWindowSignalRouter::connectMoveRequested() が接続]
MainWindow::onMoveRequested(from, to)
  │  └── MainWindowServiceRegistry::handleMoveRequested()
  ▼ [Controller]
BoardSetupController::onMoveRequested(from, to)
  │
  ├── ShogiGameController::validateAndMove()  ← Model層
  │     ├── ShogiBoard::applyUsiMove()
  │     │     └── [View] ShogiView が dataChanged / standChanged で変わったマスを再描画
  │     └── moveCommitted(mover, ply) シグナル
  │           └── MainWindow::onMoveCommitted() → MainWindowServiceRegistry::handleMoveCommitted()
  │                 ├── [Controller] BoardSetupController::onMoveCommitted()（EvE なら評価値グラフを再描画）
  │                 ├── [Service] GameRecordUpdateService::updateCurrentSfenAfterMove()
  │                 └── 定跡ウィンドウの更新
  │
  ├── [Controller] BoardInteractionController::onMoveApplied(from, to, ok)
  │     └── 移動元・移動先のハイライト（違法手なら駒の選択を解除）
  │
  └── MatchCoordinator::onHumanMove()  ← game層
        └── Strategy → [Service] GameRecordUpdateService::appendKifuLine()
              └── RecordPane（棋譜欄）に棋譜行追加
```

<!-- chapter-10-end -->

---

<!-- chapter-11-start -->
## 第11章 views/widgets/dialogs層：Qt UI部品

本章では、ユーザーが直接目にする Qt ウィジェット群を解説する。第10章で扱った Presenter / Controller / Wiring 層が「何を表示するか」のロジックを担うのに対し、本章のクラスは「どう描画し、どう入力を受けるか」という **ビュー実体** にあたる。

### 11.1 ShogiView — 将棋盤の描画エンジン

`src/views/shogiview.h` / `shogiview.cpp`

#### 11.1.1 概要

ShogiView は QWidget を直接継承した **カスタムペイントウィジェット** であり、盤面・駒・駒台・ハイライト・矢印・ラベル等をすべて `paintEvent()` 内の QPainter で描画する。QGraphicsView は使用しない（EngineAnalysisTab の分岐ツリーでは QGraphicsView を使用する）。

主な責務は以下のとおり。

| 責務 | 内容 |
|------|------|
| 盤面描画 | 9×9マス、星印、段筋ラベル、将棋盤の影・余白 |
| 駒描画 | 駒アイコン（QIcon → QMap\<QChar, QIcon\>）、反転表示対応 |
| 駒台描画 | 先手・後手の持ち駒と枚数（疑似座標 file=10/11 を使用） |
| ハイライト | FieldHighlight による移動可能マスの色付け表示 |
| 矢印 | Arrow 構造体によるエンジン最善手の可視化 |
| ドラッグ&ドロップ | 駒のつまみ上げ・移動先判定 |
| ラベル | 時計（QLabel）・プレイヤー名（ElideLabel）を子ウィジェットとして配置 |
| リサイズ | `fitBoardToWidget()` でウィジェットサイズに自動追従 |

#### 11.1.2 レンダリングレイヤー順序

`paintEvent()` は以下の順番で背面から前面へ描画する。

```
レイヤー 0   : drawBackground()        — 背景グラデーション（最背面）
レイヤー 0.3 : drawBoardShadow()       — 将棋盤の影（立体感）
             : drawStandShadow()       — 駒台の影（立体感）
レイヤー 0.5 : drawBoardMargin()       — 将棋盤の余白（9×9の外枠）
レイヤー 1   : drawBoardFields()       — 盤面マス（背景・枠線）
レイヤー 2   : drawNormalModeStand()   — 駒台のマス背景
レイヤー 3   : drawFourStars()         — 4隅の星印（装飾）
レイヤー 4   : drawHighlights()        — ハイライト（選択/移動可能マス）
レイヤー 5   : drawPieces()            — 盤上の駒
レイヤー 5.5 : drawArrows()            — 矢印（検討機能の最善手表示）
レイヤー 6   : drawPiecesStandFeatures() — 駒台の駒と枚数
レイヤー 7   : drawRanks() / drawFiles() — 段・筋ラベル
レイヤー 8   : drawDraggingPiece()     — ドラッグ中の駒（最前面）
```

ハイライト（レイヤー 4）が駒（レイヤー 5）より先に描画される点に注意。これにより、ハイライトの上に駒が重なり、移動先マスが半透明の色で示される設計になっている。

#### 11.1.3 座標系とアスペクト比

```
盤座標: (file, rank)  file=1..9（右→左）, rank=1..9（上→下）
駒台座標: file=10（右側=先手）, file=11（左側=後手）
```

1マスのアスペクト比は実物の将棋盤に基づく。

```cpp
static constexpr double kSquareAspectRatio = 34.8 / 31.7;  // ≈ 1.098（縦/横）
```

`m_squareSize` が横幅の基準ピクセルで、縦は `m_squareSize * kSquareAspectRatio` で算出される。反転表示（`m_flipMode`）時は `getClickedSquareInFlippedState()` で座標変換を行う。

#### 11.1.4 主要メソッド

| メソッド | 説明 |
|---------|------|
| `setBoard(ShogiBoard*)` | モデル（局面）を差し替え |
| `applyBoardAndRender(ShogiBoard*)` | ボード適用＋駒画像読込＋再描画 |
| `addHighlight() / removeHighlightAllData()` | ハイライトの追加・全クリア |
| `setArrows() / clearArrows()` | 矢印（最善手表示）の設定・クリア |
| `setFlipMode(bool)` | 反転表示の切り替え |
| `setPositionEditMode(bool)` | 局面編集モードの ON/OFF |
| `setActiveSide(bool)` | 手番表示（名前ラベルの配色切替） |
| `applyClockUrgency(qint64)` | 残時間に応じた時計配色変更（10秒/5秒警告） |
| `enlargeBoard() / reduceBoard()` | 盤の拡大/縮小（±1px） |
| `toImage(qreal)` | 現在の盤面を QImage として出力 |

#### 11.1.5 ハイライトと矢印

**ハイライト** は `Highlight` 基底クラスとその派生クラス `FieldHighlight` で実装される。`FieldHighlight` は座標 (file, rank) と色を保持し、`drawHighlights()` で半透明の矩形として描画される。

**矢印** は `Arrow` 構造体で表現される。移動元・移動先座標、優先順位（1=最善手）、色を持ち、検討機能で複数の候補手を盤上に視覚化する。駒打ちの場合は `dropPiece` フィールドに駒種を格納する。

#### 11.1.6 時計と手番表示

時計テキストは `QLabel`（`m_blackClockLabel`, `m_whiteClockLabel`）、プレイヤー名は `ElideLabel`（`m_blackNameLabel`, `m_whiteNameLabel`）として子ウィジェットに配置される。手番側のラベルは黄色背景＋青文字でハイライトされ、残時間に応じて段階的に配色が変化する。

```
Urgency::Normal → 黄色背景 + 青文字（通常）
Urgency::Warn10 → 黄色背景 + 青文字 + 青枠（残10秒）
Urgency::Warn5  → 黄色背景 + 赤文字 + 赤枠（残5秒）
```

---

### 11.2 主要ウィジェット一覧

`src/widgets/` ディレクトリに配置されたウィジェット群の一覧を示す。

| クラス名 | 基底クラス | 配置場所 | 機能 |
|---------|-----------|---------|------|
| RecordPane | QWidget | MainWindow 左ペイン | 棋譜一覧（QTableView）＋分岐候補一覧＋ナビゲーションボタン＋コメント欄 |
| EngineAnalysisTab | QWidget | MainWindow 右ペイン（QTabWidget） | 思考情報・検討・USI/CSAログ・コメント・分岐ツリーの各タブ |
| EvaluationChartWidget | QWidget | MainWindow 下部（QDockWidget） | Qt Charts による評価値グラフ（2エンジン対応、ゼロライン、カーソル線） |
| EngineInfoWidget | QWidget | EngineAnalysisTab 内（各エンジン情報エリア） | エンジン情報表示（予想手列、探索深さ、ノード数、NPS、ハッシュ使用率） |
| GameInfoPaneController | QObject | MainWindow タブ（対局情報タブ） | 対局情報テーブルの管理（QTableWidget）、編集・Undo/Redo・フォント変更 |
| KifuDisplay | QObject | モデルデータ | 棋譜1手分のデータ保持（指し手・消費時間・コメント） |
| KifuBranchDisplay | QObject | モデルデータ | 分岐候補1手分のデータ保持（指し手テキスト） |
| KifuAnalysisResultsDisplay | QObject | モデルデータ | 棋譜解析結果の保持（評価値・読み筋・局面SFEN） |
| MenuWindow | QWidget | 独立ウィンドウ | メニュー項目をボタン形式で表示（タブ付き、お気に入り、カスタマイズモード） |
| MenuButtonWidget | QWidget | MenuWindow 内 | QAction をアイコン＋テキストで表示するカスタムボタン、D&D対応 |

#### 汎用部品

| クラス名 | 基底クラス | 機能 |
|---------|-----------|------|
| CollapsibleGroupBox | QWidget | クリックで展開/折りたたみ可能なグループボックス |
| ElideLabel | QLabel | テキスト省略表示ラベル（エリプシス＋ホバースクロール＋ドラッグパン） |
| GlobalToolTip | QFrame | 軽量なカスタムツールチップポップアップ（XSS対策済み） |
| AppToolTipFilter | QObject | Qt 標準ツールチップを GlobalToolTip に置き換えるイベントフィルタ |
| FlowLayout | QLayout | 自動折り返しレイアウト（Qt サンプルコード準拠） |
| LongLongSpinBox | QAbstractSpinBox | 64bit 整数対応スピンボックス（Qt 標準 QSpinBox は int 範囲のみ） |

#### デリゲート

| クラス名 | 基底クラス | 機能 |
|---------|-----------|------|
| NumericRightAlignCommaDelegate | QStyledItemDelegate | テーブル内の数値を3桁カンマ区切り＋右寄せ表示 |

---

### 11.3 ダイアログ一覧

`src/dialogs/` ディレクトリに配置されたダイアログ群の一覧を示す。

| クラス名 | 基底クラス | .ui ファイル | 用途 |
|---------|-----------|------------|------|
| StartGameDialog | QDialog | startgamedialog.ui | 対局開始パラメータ収集（対局者・持ち時間・開始局面・連続対局設定） |
| TsumeShogiSearchDialog | QDialog | tsumeshogisearchdialog.ui | 詰み探索の設定（エンジン選択・探索時間） |
| KifuAnalysisDialog | QDialog | kifuanalysisdialog.ui | 棋譜解析パラメータ収集（解析範囲・エンジン・思考時間） |
| CsaGameDialog | QDialog | csagamedialog.ui | CSA通信対局設定（サーバー接続情報・対局者選択） |
| CsaWaitingDialog | QDialog | なし | CSA通信対局の接続待機表示（状態遷移・ログ表示） |
| EngineRegistrationDialog | QDialog | engineregistrationdialog.ui | USIエンジンの登録・削除・option自動取得 |
| ChangeEngineSettingsDialog | QDialog | changeenginesettingsdialog.ui | USI engine options の編集（テキスト/スピン/チェック/コンボ/ファイル選択） |
| PromoteDialog | QDialog | promotedialog.ui | 成り/不成の選択確認 |
| VersionDialog | QDialog | versiondialog.ui | バージョン情報表示 |
| PvBoardDialog | QDialog | なし | 読み筋（PV）再生ウィンドウ（ShogiView を内蔵、ナビゲーションボタン付き） |
| KifuPasteDialog | QDialog | なし | クリップボードからの棋譜テキスト取り込み（形式自動判定） |
| JosekiWindow | QWidget | なし | 定跡データベースの表示・編集・マージ（独立ウィンドウ） |
| JosekiMoveDialog | QDialog | なし | 定跡手の追加・編集（やねうら王形式、USIプレビュー） |
| JosekiMergeDialog | QDialog | なし | 棋譜データから定跡ファイルへのマージ（個別・一括登録） |
| MenuWindow | QWidget | なし | メニュー項目のボタン一覧表示（お気に入り・カテゴリ別タブ） |

---

### 11.4 .ui ファイルと C++ クラスの統合

#### 11.4.1 AUTOUIC の動作

本プロジェクトでは CMake の `set(CMAKE_AUTOUIC ON)` により、`.ui` ファイルの処理が自動化されている。

```
ビルド時の流れ:
  *.ui  →  uic  →  ui_*.h（ビルドディレクトリに生成）→  #include で取り込み
```

具体的には、`startgamedialog.ui` から `ui_startgamedialog.h` が生成され、`Ui::StartGameDialog` クラスが定義される。AUTOUIC が正しく動作するためには、`.ui` ファイルが対応する `.cpp` ファイルと同じディレクトリに存在する必要がある。

#### 11.4.2 ui->setupUi() の呼び出しパターン

`.ui` ファイルを使用するダイアログは、以下のパターンでUIを初期化する。

```cpp
// ヘッダー（前方宣言）
namespace Ui {
class StartGameDialog;
}

class StartGameDialog : public QDialog {
    Q_OBJECT
public:
    explicit StartGameDialog(QWidget *parent = nullptr);
    ~StartGameDialog() override;
private:
    Ui::StartGameDialog* ui;   // .ui から生成されたクラスへのポインタ
};

// ソース（コンストラクタ）
StartGameDialog::StartGameDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::StartGameDialog)
{
    ui->setupUi(this);         // UIウィジェットの生成と配置
    // ここで ui->pushButton, ui->comboBox 等にアクセス可能
    connectSignalsAndSlots();   // シグナル/スロット接続
    loadGameSettings();         // 設定の復元
}

StartGameDialog::~StartGameDialog()
{
    delete ui;                  // UIオブジェクトの解放
}
```

ポイント:
- `Ui::ClassName` はヘッダーで前方宣言し、PIMPL パターン風に `.cpp` でのみ `#include "ui_*.h"` する
- `ui->setupUi(this)` でウィジェットツリー全体が構築される
- デストラクタで `delete ui` を忘れないこと

#### 11.4.3 .ui なしのプログラマティック構築

`.ui` ファイルを使用しないダイアログ（CsaWaitingDialog、PvBoardDialog、KifuPasteDialog 等）は、コンストラクタ内またはプライベートメソッド `buildUi()` で直接ウィジェットを生成する。

```cpp
// buildUi() パターン（RecordPane、EngineAnalysisTab 等でも使用）
void CsaWaitingDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    m_statusLabel = new QLabel(this);
    m_cancelButton = new QPushButton(tr("キャンセル"), this);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_cancelButton);
}
```

`.ui` ファイルを使うか `buildUi()` を使うかの判断基準:

| 基準 | .ui ファイル | buildUi() |
|------|------------|-----------|
| レイアウトの複雑さ | 多数のウィジェットを配置する場合 | 動的にウィジェット数が変わる場合 |
| デザイン調整 | Qt Designer で視覚的に調整したい場合 | コードで制御する方が効率的な場合 |
| 既存慣例 | StartGameDialog, CsaGameDialog 等 | RecordPane, EngineAnalysisTab 等 |

---

### 11.5 カスタムウィジェットの設計指針

#### 11.5.1 CollapsibleGroupBox

```
src/widgets/collapsiblegroupbox.h / .cpp
```

折りたたみ可能なグループボックス。クリックでコンテンツエリアの展開/折りたたみを切り替える。

```
┌─ タイトル [▼] ────────┐     ┌─ タイトル [▶] ─┐
│  コンテンツエリア        │  →  └─────────────────┘
│  (QVBoxLayout)          │       （折りたたみ時）
└─────────────────────────┘
```

- `contentLayout()` でコンテンツエリアの QVBoxLayout を取得し、任意のウィジェットを追加
- `setExpanded(bool)` / `isExpanded()` で状態制御
- `expandedChanged(bool)` シグナルで外部に通知

#### 11.5.2 ElideLabel

```
src/widgets/elidelabel.h / .cpp
```

テキストがラベル幅に収まらない場合にエリプシス（...）を表示し、ホバー時に横スクロールで全文を閲覧可能にするカスタム QLabel。ShogiView のプレイヤー名表示で使用される。

機能:
- `setFullText()` でフルテキストを設定、幅超過時に自動省略
- `setElideMode(Qt::TextElideMode)` で省略位置を変更（デフォルト: 右端）
- `setSlideOnHover(true)` でマウスホバー時の自動横スクロールを有効化
- `setManualPanEnabled(true)` でドラッグによる手動パンを有効化

#### 11.5.3 GlobalToolTip と AppToolTipFilter

```
src/widgets/globaltooltip.h / .cpp
src/widgets/apptooltipfilter.h / .cpp
```

Qt 標準のツールチップは OS やテーマに依存して見た目が不統一になりやすい。本プロジェクトでは以下の構成で統一的なツールチップを実現している。

- **GlobalToolTip**: QFrame ベースの軽量ポップアップ。`showText(QPoint, QString)` でグローバル座標にプレーンテキストを表示。XSS 対策として HTML タグをエスケープする
- **AppToolTipFilter**: QObject のイベントフィルタ。`installOn(QWidget*)` でルート以下の全ウィジェットに一括装着し、`QEvent::ToolTip` を横取りして GlobalToolTip に差し替える

#### 11.5.4 LongLongSpinBox

```
src/widgets/longlongspinbox.h / .cpp
```

Qt の QSpinBox は `int` 範囲（約±21億）しか扱えないため、USI エンジンの option で `qlonglong` 範囲が必要な場合のためのカスタムスピンボックス。QAbstractSpinBox を継承し、`stepBy()` / `validate()` を自前実装している。

#### 11.5.5 FlowLayout

```
src/widgets/flowlayout.h / .cpp
```

Qt の公式サンプルコードを参考にした自動折り返しレイアウト。MenuWindow 内でメニューボタンを水平に並べ、ウィンドウ幅が不足すると自動で次の行に折り返す。`heightForWidth()` をオーバーライドして、QLayout のサイズヒント計算に対応している。

#### 11.5.6 カスタムウィジェット設計の共通パターン

本プロジェクトのカスタムウィジェットには以下の共通パターンが見られる。

**1. buildUi() による初期化**

コンストラクタで `buildUi()` を呼び出し、ウィジェットツリーの構築を分離する。RecordPane、EngineAnalysisTab、BranchTreeWidget 等が採用している。

**2. フォントサイズの永続化**

ほぼすべてのウィジェットとダイアログにフォントサイズ増減（A+/A-）機能がある。サイズは設定の宣言の表の `Setting<int>`（例: `GameSettings::kifuPaneFontSize`）で保存・復元し、`applyFontSize()` で一括反映する。ダイアログでは `FontSizeHelper` に宣言をそのまま渡すと、初期値の読み込みと変更のたびの保存をまとめて任せられる（§12.5）。

**3. イベントフィルタの活用**

`eventFilter()` をオーバーライドし、子ウィジェットのイベントを横取りするパターンが多い。ShogiView ではホイールイベントで拡大縮小、EngineAnalysisTab では分岐ツリーのクリック検出、AppToolTipFilter ではツールチップの差し替えに使用される。

**4. デリゲートによる描画カスタマイズ**

QTableView / QTreeView の表示をカスタマイズする場合は QStyledItemDelegate を継承する。`NumericRightAlignCommaDelegate` は数値の3桁区切り右寄せを担当する。

<!-- chapter-11-end -->

---

<!-- chapter-12-start -->
## 第12章 network層とservices層

network層はCSA（Computer Shogi Association）プロトコルによるネットワーク通信対局を実装し、services層はアプリケーション全体で利用される横断的な設定管理・時間管理・名前解決のサービス群を提供する。

### 12.1 全体構成

```
┌─────────────────────────────────────────────────────────────────┐
│                      network 層                                  │
│                                                                 │
│  ┌─────────────────────┐   ┌─────────────────────────────┐      │
│  │  CsaClient          │   │  CsaGameCoordinator         │      │
│  │  TCP/IP通信           │   │  ゲームシステムとの橋渡し     │      │
│  │  CSAプロトコル解析     │   │  CSA↔USI形式変換            │      │
│  │  状態管理             │   │  時計・エンジン管理          │      │
│  └──────────┬──────────┘   └──────────┬──────────────────┘      │
│             │ signals                  │ signals                │
│             └──────────┬───────────────┘                        │
│                        ↓                                        │
│              CsaGameWiring (ui/wiring層)                        │
│              MainWindow UI要素への配線                           │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                      services 層                                 │
│                                                                 │
│  ┌─────────────────────┐                                        │
│  │  Setting<T>         │                                        │
│  │  INI設定の永続化     │                                        │
│  │  (ドメイン別の宣言) │                                        │
│  └─────────────────────┘                                        │
│                                                                 │
│  ┌─────────────────────┐   ┌─────────────────────────────┐      │
│  │  PlayerNameService   │   │  TimeControlUtil            │      │
│  │  対局者名解決         │   │  持ち時間→ShogiClock変換    │      │
│  │  (static methods)    │   │  (namespace)                │      │
│  └─────────────────────┘   └─────────────────────────────┘      │
└─────────────────────────────────────────────────────────────────┘
```

### 12.2 CsaClient — CSAプロトコルクライアント

**ソース**: `src/network/csaclient.h/.cpp`

CSAサーバー（[shogi-server](https://github.com/TadaoYamaoka/shogi-server)）との通信を担当するクラス。CSAプロトコル1.2.1を実装し、TCP/IP接続の管理、メッセージの送受信、対局進行の低レベル処理を行う。

#### 責務

- TCP/IPソケット（`QTcpSocket`）による接続の確立・切断
- CSAプロトコルメッセージの解析と状態遷移
- ログイン/ログアウト処理
- Game_Summaryの受信と解析（時間設定・局面情報を含む）
- 指し手の送受信と消費時間の抽出
- 対局結果の判定（結果行は2行連続で受信: 事象行 + 結果行）

#### ConnectionState — 接続状態

CsaClientは8つの接続状態を持ち、CSAプロトコルの各フェーズに対応する。

| 状態 | 説明 |
|------|------|
| `Disconnected` | 未接続。初期状態、または切断後の状態 |
| `Connecting` | `connectToHost()` 呼び出し後、TCP接続の確立を待機中 |
| `Connected` | TCP接続完了。ログインコマンドの送信が可能 |
| `LoggedIn` | ログイン成功。Game_Summaryの受信を待機 |
| `WaitingForGame` | 対局マッチング待ち（LoggedInの後、明示的に遷移） |
| `GameReady` | Game_Summary受信完了。AGREEまたはREJECTの送信が可能 |
| `InGame` | START:受信後の対局中。指し手の送受信が可能 |
| `GameOver` | 対局終了。結果行（#WIN/#LOSE/#DRAW等）の受信完了 |

#### 状態遷移図

```mermaid
stateDiagram-v2
    [*] --> Disconnected
    Disconnected --> Connecting : connectToServer()
    Connecting --> Connected : onSocketConnected()
    Connecting --> Disconnected : timeout / error
    Connected --> LoggedIn : LOGIN: OK
    Connected --> Disconnected : LOGIN: incorrect
    LoggedIn --> WaitingForGame : Game_Summary待ち
    WaitingForGame --> GameReady : END Game_Summary
    GameReady --> InGame : START:受信
    GameReady --> WaitingForGame : REJECT:受信
    InGame --> GameOver : #WIN / #LOSE / #DRAW
    GameOver --> Disconnected : 切断
    LoggedIn --> Disconnected : LOGOUT:
    WaitingForGame --> Disconnected : LOGOUT:
```

#### 受信処理のアーキテクチャ

TCPストリームはパケット境界と行境界が一致しないため、`m_receiveBuffer` にデータを蓄積し、改行文字（`\n`）で分割して1行ずつ `processLine()` に渡す。

```
QTcpSocket::readyRead → onReadyRead() → m_receiveBuffer に蓄積
                                        → 改行で分割
                                        → processLine(line)
                                            ├── processLoginResponse()    [Connected状態]
                                            ├── processGameSummary()      [Game_Summary解析中]
                                            └── processGameMessage()      [InGame状態]
                                                ├── processResultLine()   [#で始まる行]
                                                └── processMoveLine()    [+/-で始まる行]
```

#### Game_Summary解析

Game_Summaryはセクション構造を持つ複合メッセージで、解析はフラグ管理で行われる。

```
BEGIN Game_Summary
  Protocol_Version:1.2.1
  ...
  BEGIN Time
    Time_Unit:1sec
    Total_Time:600
    Byoyomi:10
    ...
  END Time
  BEGIN Position
    P1-KY-KE-GI-KI-OU-KI-GI-KE-KY
    ...
  END Position
END Game_Summary
```

3つの解析フラグ（`m_inGameSummary`, `m_inTimeSection`, `m_inPositionSection`）で現在の解析位置を追跡する。Time+ / Time- セクションは先手/後手の個別時間設定に対応し、`m_currentTimeSection` で区別する。

#### GameSummary構造体

サーバーから受信した対局条件を保持する。

| フィールド | 型 | 説明 |
|-----------|-----|------|
| `protocolVersion` | `QString` | プロトコルバージョン（例: "1.2.1"） |
| `protocolMode` | `QString` | "Server" または "Direct" |
| `gameId` | `QString` | 対局ID |
| `blackName` / `whiteName` | `QString` | 先手/後手の対局者名 |
| `myTurn` | `QString` | 自分の手番（"+" = 先手, "-" = 後手） |
| `toMove` | `QString` | 最初の手番（通常は "+"） |
| `totalTime` | `int` | 持時間（時間単位） |
| `byoyomi` | `int` | 秒読み（時間単位） |
| `increment` | `int` | 加算時間（フィッシャー方式） |
| `timeUnit` | `QString` | 時間単位（"1sec" / "1min" / "1msec"） |
| `positionLines` | `QStringList` | 初期局面（CSA形式） |
| `moves` | `QStringList` | 途中局面からの再開用指し手リスト |

`timeUnitMs()` メソッドで `timeUnit` をミリ秒に変換できる。

#### シグナル一覧

CsaClientが発行するシグナルはすべて `CsaGameCoordinator` のスロットに接続される（1対1対応）。

| シグナル | 接続先スロット |
|---------|---------------|
| `connectionStateChanged()` | `onConnectionStateChanged()` |
| `errorOccurred()` | `onClientError()` |
| `loginSucceeded()` | `onLoginSucceeded()` |
| `loginFailed()` | `onLoginFailed()` |
| `logoutCompleted()` | `onLogoutCompleted()` |
| `gameSummaryReceived()` | `onGameSummaryReceived()` |
| `gameStarted()` | `onGameStarted()` |
| `gameRejected()` | `onGameRejected()` |
| `moveReceived()` | `onMoveReceived()` |
| `moveConfirmed()` | `onMoveConfirmed()` |
| `gameEnded()` | `onClientGameEnded()` |
| `gameInterrupted()` | `onGameInterrupted()` |
| `rawMessageReceived()` | `onRawMessageReceived()` |
| `rawMessageSent()` | `onRawMessageSent()` |

#### 対局結果の列挙型

対局結果は2つの列挙型で表現される。

**GameResult** — 自分にとっての結果:

| 値 | 説明 |
|----|------|
| `Win` | 勝利 |
| `Lose` | 敗北 |
| `Draw` | 引き分け |
| `Censored` | 打ち切り |
| `Chudan` | 中断 |

**GameEndCause** — 終局の原因:

| 値 | CSAプロトコル | 説明 |
|----|-------------|------|
| `Resign` | `#RESIGN` | 投了 |
| `TimeUp` | `#TIME_UP` | 時間切れ |
| `IllegalMove` | `#ILLEGAL_MOVE` | 反則 |
| `Sennichite` | `#SENNICHITE` | 千日手 |
| `OuteSennichite` | `#OUTE_SENNICHITE` | 連続王手の千日手 |
| `Jishogi` | `#JISHOGI` | 入玉宣言 |
| `MaxMoves` | `#MAX_MOVES` | 手数制限 |
| `Chudan` | `#CHUDAN` | 中断 |
| `IllegalAction` | `#ILLEGAL_ACTION` | 不正行為 |

### 12.3 CsaGameCoordinator — CSA通信対局コーディネータ

**ソース**: `src/network/csagamecoordinator.h/.cpp`

CsaClientとゲームシステム（ShogiGameController, ShogiView, ShogiClock等）の橋渡し役。CSA通信対局の全体フローを制御する。

#### 責務

- CsaClient のシグナルを受け取り、ゲームシステムに反映
- CSA形式 ↔ USI形式の指し手変換
- 盤面への指し手適用（`applyMoveToBoard()`）
- GUI側の時計制御（サーバーの権威的な消費時間でGUI時計を補正）
- USIエンジンの起動・思考管理（エンジン vs サーバー対局時）
- 対局結果の日本語変換と表示

#### 対局形態

CsaGameCoordinatorは2種類の対局形態をサポートする。

| PlayerType | 説明 |
|-----------|------|
| `Human` | 人間がGUI経由で指す。`onHumanMove()` スロットで座標→CSA変換→サーバー送信 |
| `Engine` | USIエンジンが自動応答。`startEngineThinking()` でエンジンに思考→結果をCSA変換→送信 |

#### GameState — 対局状態

| 状態 | 説明 |
|------|------|
| `Idle` | 待機中（初期状態） |
| `Connecting` | サーバー接続中 |
| `LoggingIn` | ログイン中 |
| `WaitingForMatch` | マッチング待ち |
| `WaitingForAgree` | 対局条件確認中（自動AGREE） |
| `InGame` | 対局中 |
| `GameOver` | 対局終了 |
| `Error` | エラー状態 |

#### CSA通信対局の全体フロー

```mermaid
sequenceDiagram
    participant UI as MainWindow/CsaGameWiring
    participant Coord as CsaGameCoordinator
    participant Client as CsaClient
    participant Server as CSAサーバー

    UI->>Coord: startGame(options)
    Coord->>Client: connectToServer(host, port)
    Client->>Server: TCP接続
    Server-->>Client: 接続完了
    Client-->>Coord: connectionStateChanged(Connected)
    Coord->>Client: login(user, pass)
    Client->>Server: LOGIN user pass
    Server-->>Client: LOGIN:user OK
    Client-->>Coord: loginSucceeded()

    Note over Server,Client: マッチング待ち
    Server-->>Client: BEGIN Game_Summary ... END
    Client-->>Coord: gameSummaryReceived()
    Coord->>Client: agree(gameId)
    Client->>Server: AGREE
    Server-->>Client: START:gameId
    Client-->>Coord: gameStarted(gameId)
    Coord->>Coord: setupInitialPosition()
    Coord->>Coord: setupClock()
    Coord-->>UI: gameStarted(black, white)

    Note over Server,Client: 対局中（指し手のやり取り）
    Server-->>Client: -3334FU,T5
    Client-->>Coord: moveReceived(move, 5000ms)
    Coord->>Coord: applyMoveToBoard()
    Coord-->>UI: moveMade() / turnChanged()

    UI->>Coord: onHumanMove(from, to, promote)
    Coord->>Client: sendMove(+7776FU)
    Client->>Server: +7776FU
    Server-->>Client: +7776FU,T3
    Client-->>Coord: moveConfirmed(move, 3000ms)

    Note over Server,Client: 対局終了
    Server-->>Client: #RESIGN / #WIN
    Client-->>Coord: gameEnded(Win, Resign)
    Coord-->>UI: gameEnded("勝ち", "投了")
```

#### 時計管理の仕組み

CSA通信対局では、サーバーが権威的な消費時間（`T` に続く数値）を通知する。CsaGameCoordinatorはこの情報をもとにGUI側の時計を補正する。

1. **残り時間の追跡**: `m_blackRemainingMs` / `m_whiteRemainingMs` でサーバーからの累積消費時間を追跡
2. **時計の補正**: 対局開始時に対局条件（`GameSummary::timeControl()`）を `ShogiClock::setTimeControl()` で渡し、着手ごとにサーバーの消費時間を反映した残り時間を `ShogiClock::setRemainingTimes()` で合わせる
3. **手番の切替**: 指し手確認後に `setCurrentPlayer()` で相手側の時計に切り替え、`startClock()` でGUI上のカウントダウンを開始

#### 指し手形式の変換

CsaGameCoordinatorは3つの指し手形式間の変換メソッドを持つ。

| メソッド | 変換元 → 変換先 | 例 |
|---------|----------------|-----|
| `csaToUsi()` | CSA → USI | `"+7776FU"` → `"7g7f"` |
| `usiToCsa()` | USI → CSA | `"7g7f"` → `"+7776FU"` |
| `boardToCSA()` | GUI座標 → CSA | `QPoint(7,7)→QPoint(7,6)` → `"+7776FU"` |
| `csaToPretty()` | CSA → 表示用 | `"+7776FU"` → `"▲７六歩(77)"` |

駒打ちの場合、CSA形式では移動元が `00`（例: `+0077FU`）となり、GUI座標では `from.x() >= 10`（10=先手駒台, 11=後手駒台）で表現される。

#### Dependencies構造体

```cpp
struct Dependencies {
    ShogiGameController* gameController = nullptr; // 対局制御（非所有）
    ShogiView* view = nullptr;                     // 盤面ビュー（非所有）
    ShogiClock* clock = nullptr;                   // 時計（非所有）
    BoardInteractionController* boardController = nullptr;
    KifuRecordListModel* recordModel = nullptr;
    QStringList* sfenRecord = nullptr;         // MainWindowと共有
    QVector<ShogiMove>* gameMoves = nullptr;   // MainWindowと共有
    UsiCommLogModel* usiCommLog = nullptr;     // USI通信ログ
    ShogiEngineThinkingModel* engineThinking = nullptr;
};
```

外部から渡されたモデル（`usiCommLog`, `engineThinking`）がある場合はそれを使用し、なければ内部で作成する（所有権フラグ `m_ownsCommLog`, `m_ownsThinking` で管理）。

### 12.4 CsaGameWiring — CSA通信対局のUI配線

**ソース**: `src/ui/wiring/csagamewiring.h/.cpp`

CsaGameCoordinatorのシグナルとMainWindowのUI要素を接続する配線クラス。第4章で解説したWiringパターンの典型例であり、MainWindowの肥大化を防ぐ。

#### 役割

- CsaGameCoordinatorの各シグナルをUIスロットに配線（`wire()` / `unwire()`）
- 指し手確定時の棋譜欄更新（`onMoveMade()`）
- 対局開始/終了時のナビゲーション無効化/有効化
- CsaGameDialogからの対局開始処理（`startCsaGame()`）
- 待機ダイアログ（CsaWaitingDialog）の管理

### 12.5 設定の永続化 — 宣言の表（`Setting<T>`）

**ソース**: `src/services/setting.h`, `src/services/settingscommon.h/.cpp`, ドメイン別ヘッダー（`appsettings.h`・`gamesettings.h`・`analysissettings.h`・`networksettings.h`・`josekisettings.h`・`tsumeshogisettings.h`・`enginedialogsettings.h`・`docksettings.h`）

アプリケーション設定をINIファイル（`ShogiBoardQ.ini`）で永続化する。設定項目は `Setting<T>` で**キーと既定値を1行で宣言**し、読み書きはその宣言を通す。getter と setter の組やキー定数は手で書かない。詳しい規約は [settings-service.md](settings-service.md) にまとめている。

`src/services/settingsservice.h` は、ドメイン別ヘッダーをまとめて include するだけのヘッダーとして残っている（関数はなく、`settingsservice.cpp` も空）。新しいコードはドメイン別ヘッダーを直接 include する。

#### INIファイルの場所と構造

INIファイルの場所は `SettingsCommon::settingsFilePath()` が `QStandardPaths::AppConfigLocation` から決める（Linux は `~/.config/ShogiBoardQ/ShogiBoardQ.ini`）。環境変数 `SHOGIBOARDQ_CONFIG_HOME` を指定すると、その下の `ShogiBoardQ` フォルダを使う。読み書きは `SettingsCommon::openSettings()` が返す、アプリ全体で1つの `QSettings` を通る。

```ini
[SizeRelated]
mainWindowSize=@Size(1100 720)
squareSize=55

[General]
lastKifuDirectory=/home/user/kifu
language=system

[FontSize]
usiLog=10
comment=10
gameInfo=10
thinking=10
kifuPane=10
csaLog=10
engineSettings=12

[UI]
lastSelectedTabIndex=0
toolbarVisible=true

[EvalChart]
yLimit=2000
xLimit=500
xInterval=10
labelFontSize=7

[Consideration]
engineName=YaneuraOu
unlimitedTime=true
byoyomiSec=0
multiPV=1

[KifuAnalysis]
fontSize=10
byoyomiSec=3
engineName=YaneuraOu
fullRange=true

[EvalChartDock]
floating=false
visible=true

[RecordPaneDock]
floating=false
visible=true

[CustomDockLayouts]
layout1=<base64エンコードされた状態>
```

主要なグループと設定項目:

| INIグループ | 設定カテゴリ | 主な項目 |
|------------|------------|---------|
| `SizeRelated` | ウィンドウサイズ | メインウィンドウ、盤のマスの大きさ、各ダイアログのサイズ |
| `General` | 一般設定 | 最後の棋譜ディレクトリ、言語、指し手の表記、GUI共通フォント |
| `FontSize` | フォントサイズ | 各タブ・ダイアログのフォントサイズ |
| `UI` | UI状態 | 最後に選択したタブ、ツールバー表示 |
| `EvalChart` | 評価値グラフ | Y軸上限、X軸上限、軸ラベルサイズ |
| `Consideration` | 検討設定 | エンジン名、時間無制限フラグ、候補手数 |
| `KifuAnalysis` | 棋譜解析設定 | 思考時間、エンジン名、解析範囲 |
| `GameSettings` | 対局開始ダイアログ | 対局者・持ち時間・開始局面・最大手数など前回の対局条件 |
| `CsaGameSettings` | CSA通信対局ダイアログ | 対局者、接続先、接続の維持・連続対局・自動保存などの詳細設定 |
| `*Dock` | ドック状態 | フローティング、ジオメトリ、表示状態 |
| `CustomDockLayouts` | カスタムレイアウト | 名前付きレイアウトの保存 |
| `MenuWindow` | メニューウィンドウ | お気に入り、サイズ、ボタンサイズ |
| `Engines`・エンジン名 | エンジン登録情報 | 登録一覧とUSIオプション（§7.9。宣言の表を使わない） |

#### 宣言と読み書き

各項目はドメイン別ヘッダーの namespace に `inline const Setting<T>` として宣言する。以下は実際のコードからの抜粋:

```cpp
// src/services/analysissettings.h より
namespace AnalysisSettings {
/// USI通信ログタブのフォントサイズ（デフォルト: 10）
inline const Setting<int> usiLogFontSize{"FontSize/usiLog", 10};
/// 読み筋表示ウィンドウのサイズ（デフォルト: 620x720）
inline const Setting<QSize> pvBoardDialogSize{"SizeRelated/pvBoardDialogSize", QSize(620, 720)};
}
```

```cpp
const int size = AnalysisSettings::usiLogFontSize();     // 読む（未保存なら宣言の既定値）
AnalysisSettings::usiLogFontSize.set(12);                // 書く
AnalysisSettings::usiLogFontSize.defaultValue();         // 宣言の既定値（「既定値に戻す」など）
AnalysisSettings::usiLogFontSize.remove();               // 保存値を消す
```

`Setting<T>` の要点:
1. 読み書きは `SettingsCommon::openSettings()` の共有 `QSettings` を通る。項目ごとの `.cpp` は要らない
2. 未保存のときは宣言の既定値を返す。既定値が翻訳文や実行時の場所で決まる項目は、宣言の既定値を空にして `valueOr()` で読む（例: `GameSettings::StartGame::player1.humanName.valueOr(tr("You"))`）
3. 同じ形の項目は `Setting<T>` をメンバに持つ構造体にまとめられる（対局開始ダイアログの `GameSettings::StartGame::Player` の `player1` / `player2`）

#### 関数として書く項目

値の範囲を絞る（`AppSettings::pieceSoundVolume` など）、複数のキーを構造体にまとめる（`AppSettings::boardColors` など）、古い保存値を引き継ぐ（`AppSettings::appearanceSection`）、リスト・配列で保存する（`AnalysisSettings::engineInfoColumnWidths`・ドックレイアウトなど）、id ごとにキーを作る（`AppSettings::dialogFontSize(id, ...)`）項目だけは、ドメイン別ヘッダーに関数を宣言し、`.cpp` で `SettingsCommon::openSettings()` を使って実装する。キーは `settingskeys.h` に置く（id から組み立てる `DialogAppearance/<id>/...` だけは関数の中に書いている）。

```cpp
// src/services/analysissettings.cpp より
QList<int> engineInfoColumnWidths(int widgetIndex)
{
    QSettings& s = SettingsCommon::openSettings();
    QString key = QString(SettingsKeys::kEngineInfoColumnWidthsFmt).arg(widgetIndex);
    QList<int> widths;

    int size = s.beginReadArray(key);
    for (int i = 0; i < size; ++i) {
        s.setArrayIndex(i);
        widths.append(s.value(SettingsKeys::kArrayWidth, 0).toInt());
    }
    s.endArray();

    return widths;
}
```

設定ファイルを `QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat)` で直接開くのは、`beginReadArray` / `beginGroup` でグループを開いたまま読み書きを続ける配列データ（エンジン登録一覧、エンジンごとのオプション配列、CSA通信対局の接続履歴）だけ。共有の `QSettings` でグループを開いたままにすると、その間に読み書きするほかの項目のキーがずれるためである。

#### 設定項目の追加方法

新しいダイアログやウィンドウを追加する際は、CLAUDE.md の「Settings Update Guidelines」に従い、以下の手順で設定を追加する。

**1. ドメイン別ヘッダーに宣言を1行ずつ足す:**

```cpp
// gamesettings.h
/// 〇〇ダイアログのウィンドウサイズ（デフォルト: 800x600）
inline const Setting<QSize> myDialogSize{"SizeRelated/myDialogSize", QSize(800, 600)};
/// 〇〇ダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> myDialogFontSize{"FontSize/myDialog", 10};
```

**2. ダイアログで読み込み/保存を行う:**

```cpp
// mydialog.cpp
MyDialog::MyDialog(QWidget* parent)
    : QDialog(parent)
    , m_fontHelper(GameSettings::myDialogFontSize, 8, 24, 1)  // 変更のたびに保存する
{
    DialogUtils::restoreDialogSize(this, GameSettings::myDialogSize);
}

MyDialog::~MyDialog()
{
    DialogUtils::saveDialogSize(this, GameSettings::myDialogSize);
}
```

`FontSizeHelper`（`src/common/fontsizehelper.h`）は宣言から初期値を読み、A-/A+ で変わるたびに同じ宣言へ保存する。`DialogUtils::restoreDialogSize` / `saveDialogSize`（`src/common/dialogutils.h`）も宣言をそのまま受け取る。保存はデストラクタのほか、`done()` や `closeEvent` で行ってもよい。実際のコードでは `KifuPasteDialog` が同じ形になっている。

永続化すべき主な設定項目:
- **ウィンドウサイズ**: ユーザーがリサイズ可能なダイアログ
- **フォントサイズ**: A-/A+ ボタンによる調整機能がある場合
- **列幅**: テーブルやリストビューの列幅
- **最後に選択した項目**: コンボボックスの選択状態、最後に開いたファイルパス

### 12.6 手番・時計表示の同期

**ソース**: `src/app/turnstatesyncservice.h/.cpp`, `src/game/matchtimekeeper.h/.cpp`

`TurnStateSyncService::updateTurnAndTimekeepingDisplay()` がゲームコントローラの手番を表示へ反映し、`ShogiClock` の現在値を `TimeDisplayPresenter` へ渡す。対局中の時間計測と手番タイマーは `MatchTimekeeper` が担当する。

### 12.7 PlayerNameService — 対局者名解決サービス

**ソース**: `src/services/playernameservice.h/.cpp`

`PlayMode`（対局モード）に応じて、エンジン名のログ用割り当てを決定する。

#### computeEngineModels() — エンジン名のログ割り当て

2つのエンジン情報表示欄（model1=上段, model2=下段）にどのエンジン名を表示するかを決定する。

| PlayMode | model1 | model2 |
|---------|--------|--------|
| `EvenHumanVsEngine` / `HandicapHumanVsEngine` | engine2 | (空) |
| `EvenEngineVsHuman` / `HandicapEngineVsHuman` | engine1 | (空) |
| `EvenEngineVsEngine` | engine1 | engine2 |
| `HandicapEngineVsEngine` | engine2 | engine1 |
| その他 | (空) | (空) |

戻り値は `EngineNameMapping` 構造体（`model1`, `model2` フィールド）。

### 12.8 TimeControlUtil — 持ち時間設定のユーティリティ

**ソース**: `src/services/timecontrolutil.h/.cpp`

`GameStartCoordinator::TimeSettings` の設定値を `ShogiClock` に適用するユーティリティ名前空間。

#### applyToClock()

```cpp
void applyToClock(ShogiClock* clock,
                  const GameStartCoordinator::TimeSettings& tc,
                  const QString& startSfenStr,
                  const QString& currentSfenStr);
```

処理の流れ:

1. **時計への適用**: `ShogiClock::setTimeControl(tc.rules)` で先後の決まりを渡す。秒読みと加算の排他、初手の加算、持ち時間0の秒読みの扱いは時計と `TimeControl::normalized()` が受け持つ
2. **時間切れ負け**: 時間制限があり、`tc.loseOnTimeout` がオンのときだけ時間切れを負けにする
3. **初期手番の推定**: 開始局面のSFEN文字列から `" b "`（先手）/ `" w "`（後手）を探索して初期手番を決定

```
TimeSettings {enabled, rules: TimeControl {black, white}, loseOnTimeout}
    ↓
ShogiClock::setTimeControl(rules)
ShogiClock::setLoseOnTimeout(limited && loseOnTimeout)
ShogiClock::setCurrentPlayer(1 or 2)  ← SFENから推定
```

<!-- chapter-12-end -->

---

<!-- chapter-13-start -->
## 第13章 navigation層とboard層

本章では棋譜ナビゲーション（`src/navigation/`）と盤面操作（`src/board/`）の2層を解説する。ナビゲーション層は棋譜の手を辿るUI操作を統一管理し、盤面層はクリックによる駒操作・局面編集・画像エクスポート・SFEN追跡を担う。

### 13.1 KifuNavigationController — 棋譜ナビゲーションコントローラ

**ソース**: `src/navigation/kifunavigationcontroller.h/.cpp`

棋譜の閲覧操作（ボタン・クリック・分岐選択）を統一的に処理するコントローラ。ナビゲーション後は `emitUpdateSignals()` で盤面・棋譜欄・分岐ツリーへの更新シグナルを一括発行する。

#### 依存オブジェクト

| オブジェクト | 設定方法 | 役割 |
|-------------|---------|------|
| `KifuBranchTree` | `setTreeAndState()` | 分岐ツリーデータ（非所有） |
| `KifuNavigationState` | `setTreeAndState()` | 現在位置・分岐選択記憶の管理（非所有） |

#### 6つのナビゲーションボタン

`connectButtons()` で6つの `QPushButton` の `clicked` シグナルを対応スロットに接続する。

| ボタン | スロット | 動作 |
|-------|---------|------|
| 先頭（`|◁`） | `onFirstClicked` → `goToFirst()` | ルートノード（0手目）へ移動 |
| 10手戻る（`◁◁`） | `onBack10Clicked` → `goBack(10)` | 親ノードを10回辿る |
| 1手戻る（`◁`） | `onPrevClicked` → `goBack(1)` | 親ノードへ移動 |
| 1手進む（`▷`） | `onNextClicked` → `goForward(1)` | 子ノードへ移動（分岐選択記憶を優先） |
| 10手進む（`▷▷`） | `onFwd10Clicked` → `goForward(10)` | 子ノードを10回辿る |
| 末尾（`▷|`） | `onLastClicked` → `goToLast()` | 現在ラインの終端まで進む |

#### goForward() の分岐選択ロジック

「1手進む」/「10手進む」/「末尾」では、分岐がある場合に `findForwardNode()` で次のノードを決定する。

```
findForwardNode():
  1. currentNode の子ノード数が 0 → null（末尾）
  2. 分岐があれば、優先ライン（なければ現在のライン）の allLines() のパス上の子を返す
  3. lastSelectedChildAt(currentNode) で以前選んだ子を取得
     → 有効なインデックスなら、その子ノードを返す
  4. フォールバック: childAt(0)（本譜）を返す
```

この仕組みにより、一度分岐を選択してから「戻る→進む」した場合、以前選択した分岐パスを辿り直すことができる。

#### goToNode() — 分岐選択記憶の更新

指定ノードへ直接移動する際、`rememberPathSelections(node)` でルートからノードまでの経路上の全分岐点での選択を記憶する。これにより後続の `goForward()` が正しいパスを辿れるようになる。

#### その他のナビゲーション操作

| メソッド | 動作 |
|---------|------|
| `goToPly(int ply)` | 現在ライン上の指定手数へ移動（内部で `goBack`/`goForward` を呼ぶ） |
| `switchToLine(int lineIndex)` | 全ライン一覧から指定ラインへ切替（同じ手数のノードを探索） |
| `selectBranchCandidate(int idx)` | 分岐候補欄から選択して移動、優先ラインを設定 |
| `goToMainLineAtCurrentPly()` | 本譜の同手数ノードへ戻る（優先ライン・選択記憶をリセット） |

#### handleBranchNodeActivated() — 分岐ツリークリック処理

分岐ツリーウィジェットのセルがクリックされたときに呼ばれるスロット。

```
handleBranchNodeActivated(row, ply):
  1. ply == 0 → ルートノードへ移動して終了
  2. 分岐前の共有ノードかチェック
     → 現在のラインの branchPly より前なら effectiveRow を現在ラインに維持
  3. effectiveRow > 0 → setPreferredLineIndex() で優先ラインを設定
  4. line.nodes から ply に一致するノードを検索
  5. goToNode() で移動
  6. branchNodeHandled シグナルで ply, sfen, 移動先座標, USI表記を通知
```

#### emitUpdateSignals() — 更新シグナルの一括発行

全ナビゲーション操作の末尾で呼ばれ、5つのシグナルを発行する。

| シグナル | 接続先 | 内容 |
|---------|-------|------|
| `navigationCompleted(node)` | MainWindow | ナビゲーション完了通知 |
| `boardUpdateRequired(sfen)` | BoardSyncPresenter | 盤面SFEN更新 |
| `recordHighlightRequired(ply)` | RecordPresenter | 棋譜欄ハイライト |
| `branchTreeHighlightRequired(lineIdx, ply)` | BranchTreeManager | 分岐ツリーハイライト |
| `branchCandidatesUpdateRequired(candidates)` | KifuBranchDisplay | 分岐候補欄更新 |

### 13.2 KifuNavigationState — ナビゲーション状態管理

**ソース**: `src/kifu/kifunavigationstate.h/.cpp`

ナビゲーションの現在位置と分岐選択履歴を保持する状態オブジェクト。KifuNavigationController の操作対象となる。

#### 主要な状態

| メンバ | 型 | 役割 |
|-------|---|------|
| `m_currentNode` | `KifuBranchNode*` | 現在位置のノード |
| `m_tree` | `KifuBranchTree*` | 分岐ツリーへの参照 |
| `m_lastSelectedChildAtBranch` | `QHash<int, int>` | 分岐点ごとに選んだ子の記憶 |
| `m_preferredLineIndex` | `int` | 優先ラインインデックス（-1=未設定） |

#### 主要メソッド

| メソッド | 説明 |
|---------|------|
| `currentPly()` | 現在の手数（表示中の手数。MainWindow は手数の変数を持たない） |
| `currentLineIndex()` | 現在のラインインデックス（0=本譜、1以上=分岐） |
| `currentRecordLine()` | 表示中の手順（`RecordLine`） |
| `isOnMainLine()` | 本譜上にいるか |
| `canGoForward()` / `canGoBack()` | 前進/後退可能か |
| `branchCandidatesAtCurrent()` | 現在位置の分岐候補ノード一覧 |
| `rememberChildSelection(node, childIndex)` | 分岐点で選んだ子を記憶 |
| `lastSelectedChildAt(node)` | 記憶された子を返す |

### 13.3 RecordNavigationHandler — 棋譜欄行変更ハンドラ

**ソース**: `src/navigation/recordnavigationhandler.h/.cpp`

棋譜欄（RecordPane）の行選択が変わったときの処理を担当する。MainWindow の `onRecordPaneMainRowChanged` から分離されたロジック。

#### Deps構造体

```cpp
struct Deps {
    KifuNavigationState* navState;          // ナビゲーション状態
    KifuBranchTree* branchTree;              // 分岐ツリー
    KifuDisplayCoordinator* displayCoordinator; // 棋譜表示調整
    KifuRecordListModel* kifuRecordModel;    // 棋譜リストモデル
    ShogiView* shogiView;                    // 盤面ビュー
    EvaluationGraphController* evalGraphController; // 評価値グラフ
    QString* currentSfenStr;                  // 現在局面SFEN
    bool* skipBoardSyncForBranchNav;          // 分岐ナビ同期スキップフラグ
    CsaGameCoordinator* csaGameCoordinator;  // CSA対局
    PlayMode* playMode;                       // 対局モード
    MatchCoordinator* match;                  // 対局調整
};
```

#### onMainRowChanged() の処理フロー

```
onMainRowChanged(row):
  1. 再入ガード（インスタンスのフラグ m_mainRowChangeInProgress でスキップ）
  2. 分岐ナビゲーション中フラグチェック → スキップ
  3. CSA対局進行中チェック → スキップ
  4. 盤面同期（対局進行中は行わない）:
     lineSfenAt(row) で表示中の手順（分岐ツリーの表示中のライン）から row 行目の局面と直前の局面を取る
     a. 取れた → branchBoardSyncRequired(sfen, prevSfen) を emit
     b. 取れない → boardSyncRequired(row) を emit
     本譜か分岐かは区別しない。対局の局面列（sfenRecord）は見ない
  5. 棋譜モデルのハイライト行を更新
  6. 手番表示・評価値グラフカーソルを更新
  7. 矢印ボタン有効化を要求
  8. m_currentSfenStr を表示中の局面に更新（対局進行中は除く）
  9. 定跡ウィンドウ更新を要求
  10. 検討モード中なら buildPositionRequired を emit
  11. KifuDisplayCoordinator::onPositionChanged() に位置変更を通知（対局進行中は除く）
      → 表示中の手数（KifuNavigationState のカーソル）がその行のノードへ動く
```

表示中の手数は手数の変数ではなく、分岐ツリーのカーソルが持つ。対局中のカーソルは `KifuDisplayCoordinator::onLiveGameMoveAdded()` が最新手に合わせる。

#### 発行するシグナル

| シグナル | 接続先 | 発火条件 |
|---------|-------|---------|
| `boardSyncRequired(ply)` | MainWindow | 本譜の盤面同期 |
| `branchBoardSyncRequired(sfen, prevSfen)` | MainWindow | 分岐ラインの盤面同期 |
| `enableArrowButtonsRequired()` | MainWindow | 矢印ボタンの有効化 |
| `turnUpdateRequired()` | MainWindow | 手番表示の更新 |
| `josekiUpdateRequired()` | MainWindow | 定跡ウィンドウの更新 |
| `buildPositionRequired(row)` | MainWindow | 検討モード用局面構築 |

### 13.4 ナビゲーションフロー図

ユーザの操作から盤面更新までの全体的なデータフローを示す。

```mermaid
flowchart TD
    subgraph "ユーザ操作"
        A1["ナビゲーションボタン<br/>（先頭/前/次/末尾/10手前/10手後）"]
        A2["棋譜欄クリック"]
        A3["分岐ツリークリック"]
        A4["分岐候補クリック"]
    end

    subgraph "Controller層"
        B1["KifuNavigationController<br/>goToFirst/goBack/goForward/goToLast"]
        B2["RecordNavigationHandler<br/>onMainRowChanged"]
        B3["KifuNavigationController<br/>handleBranchNodeActivated"]
        B4["KifuNavigationController<br/>selectBranchCandidate"]
    end

    subgraph "State層"
        C["KifuNavigationState<br/>currentNode / currentLineIndex<br/>分岐選択記憶"]
    end

    subgraph "UI更新"
        D1["BoardSyncPresenter<br/>盤面SFEN更新"]
        D2["RecordPresenter<br/>棋譜欄ハイライト"]
        D3["BranchTreeManager<br/>分岐ツリーハイライト"]
        D4["KifuBranchDisplay<br/>分岐候補欄更新"]
        D5["EvaluationGraphController<br/>カーソルライン更新"]
    end

    A1 --> B1
    A2 --> B2
    A3 --> B3
    A4 --> B4

    B1 -->|"setCurrentNode()"| C
    B3 -->|"goToNode()"| C
    B4 -->|"goToNode()"| C

    B1 -->|"emitUpdateSignals()"| D1
    B1 -->|"emitUpdateSignals()"| D2
    B1 -->|"emitUpdateSignals()"| D3
    B1 -->|"emitUpdateSignals()"| D4

    B2 -->|"boardSyncRequired"| D1
    B2 -->|"setCurrentPly()"| D5
    B2 -->|"setCurrentHighlightRow()"| D2

    B3 -->|"emitUpdateSignals()"| D1
    B3 -->|"emitUpdateSignals()"| D2
    B3 -->|"emitUpdateSignals()"| D3

    B4 -->|"emitUpdateSignals()"| D1
    B4 -->|"emitUpdateSignals()"| D2
    B4 -->|"emitUpdateSignals()"| D3
    B4 -->|"emitUpdateSignals()"| D4
```

**ポイント**: KifuNavigationController 経由の操作（ボタン・分岐ツリー・分岐候補）はすべて `emitUpdateSignals()` で統一的に更新シグナルを発行する。一方、棋譜欄クリックは RecordNavigationHandler が個別にシグナルを発行する。

### 13.5 BoardInteractionController — 盤面操作コントローラ

**ソース**: `src/board/boardinteractioncontroller.h/.cpp`

盤面上のマウスクリック操作とハイライト描画を制御するコントローラ。

#### 操作モード

```cpp
enum class Mode {
    HumanVsHuman,   // 対人対局
    HumanVsEngine,  // 対エンジン対局
    Edit             // 局面編集
};
```

モードに応じてクリック時の挙動が変わる。`Edit` モード以外では人間の手番判定コールバック（`IsHumanTurnCallback`）でガードされる。

#### 2クリック方式の駒移動

駒の移動は2段階のクリックで実行される。

```
1stクリック（onLeftClick）:
  1. 人間の手番チェック（Editモード以外）
  2. 駒台の枚数0チェック
  3. startDrag() でドラッグ開始
  4. selectPieceAndHighlight() で選択マスをオレンジハイライト
  5. m_waitingSecondClick = true

2ndクリック（onLeftClick）:
  1. m_waitingSecondClick = false
  2. 同一マスクリック → キャンセル（finalizeDrag）
  3. 異なるマス → moveRequested(from, to) シグナルを発火
```

右クリックは2クリック待ち中のキャンセル、または編集モード時の成/不成トグルに使用される。

#### ハイライト管理

3種類のハイライトを独立に管理する。

| ハイライト | 色 | 意味 | メンバ変数 |
|-----------|---|------|-----------|
| 選択中 | オレンジ（`rgba(255,128,0,70)`） | 現在選択中の駒 | `m_selectedField` |
| 移動元 | 薄い赤（`rgba(255,0,0,50)`） | 直前の着手の移動元 | `m_selectedField2` |
| 移動先 | 黄色（`rgb(255,255,0)`） | 直前の着手の移動先 | `m_movedField` |

```
onMoveApplied(from, to, success):
  成功時 → 移動元を赤、移動先を黄色で表示、選択ハイライトをクリア
  失敗時 → 全選択状態をクリア（finalizeDrag）
```

#### ダングリングポインタ防止

`ShogiView::highlightsCleared` シグナルに `onHighlightsCleared()` を接続し、`ShogiView::removeHighlightAllData()` が呼ばれた際にポインタを `nullptr` にリセットする。これにより、外部からハイライトが一括削除された場合の二重 `delete` を防止する。

#### 駒台の座標規約

| 定数 | 値 | 意味 |
|------|---|------|
| `kBlackStandFile` | 10 | 先手駒台のファイル番号 |
| `kWhiteStandFile` | 11 | 後手駒台のファイル番号 |

盤上は `file: 1-9`、`rank: 1-9` の座標を使用し、駒台は `file: 10-11` で区別する。

### 13.6 PositionEditController — 局面編集コントローラ

**ソース**: `src/board/positioneditcontroller.h/.cpp`

局面編集モードのライフサイクル（開始・終了）と盤面操作APIを提供する。

#### 編集開始 — beginPositionEditing()

`BeginEditContext` 構造体で必要な情報を受け取り、以下の手順で編集モードに入る。

```
beginPositionEditing(context):
  1. 編集開始SFENの決定:
     currentSfenStr → displayedSfen（表示中の局面）→ 盤面状態
     の優先順位で取得
  2. SFEN正規化: toMinimalSfen() で4フィールド形式に統一
  3. 手番/手数の強制上書き: forceTurnAndPly() で表示中の盤面の手番に合わせる
  4. 盤面適用: board->setSfen(adjusted)
  5. 編集モード遷移: setPositionEditMode(true), setMouseClickMode(true)
  6. BIC を Edit モードに設定、ハイライトクリア
  7. GC の手番を盤面の手番に同期
  8. 「編集終了」ボタンを表示（コールバック）
  9. startSfenStr = recordStartSfen（空なら編集する局面）
```

`BeginEditContext` の `displayedSfen`・`recordStartSfen` は、`PositionEditCoordinator` が分岐ツリーの表示中の手順（`KifuNavigationState::currentRecordLine()`）から入れる。対局の局面列や手数の変数は受け取らない。

#### 編集終了 — finishPositionEditing()

`FinishEditContext` 構造体で必要な情報を受け取り、編集結果を確定する。

```
finishPositionEditing(context):
  1. 盤面モデルから SFEN を再構成（convertBoardToSfen + GC の手番 + convertStandToSfen）
  2. board->setSfen() で反映
  3. setPositionEditMode(false) で通常モードへ復帰
  4. startSfenStr を編集後の局面に更新（編集した局面が開始局面になる）
  5. ハイライトクリア、「編集終了」ボタン非表示
```

棋譜（分岐ツリー）と対局の局面列は、このあと `PositionEditCoordinator::positionEditingFinished` を受けた `resetRecordForEditedPosition` が編集後の局面から作り直す。`FinishEditContext` は局面列を受け取らない。

#### 盤面操作API

| メソッド | 対応アクション | 動作 |
|---------|-------------|------|
| `resetPiecesToBox()` | 「全ての駒を駒箱へ」 | `ShogiView::returnAllPiecesToBox()` を呼ぶ |
| `setStandardStartPosition()` | 「平手初期配置」 | `ShogiView::initializeToFlatStartingPosition()` を呼ぶ |
| `setTsumeShogiStartPosition()` | 「詰将棋初期配置」 | `ShogiView::shogiProblemInitialPosition()` を呼ぶ |
| `onToggleSideToMoveTriggered()` | 「手番切替」 | 先手 ⇔ 後手をトグル |

各操作はメニューアクションのスロットとしても提供され、編集セッション中に保持した `m_view`/`m_gc`/`m_bic` を利用する。

#### applyEditMove() — 編集中の着手適用

局面編集モードでの駒移動を処理する。

```cpp
bool applyEditMove(from, to, view, gc, bic):
  1. gc->editPosition(from, to) で駒を移動
  2. view->endDrag() でドラッグ状態を解除
  3. bic->onMoveApplied() でハイライトを更新
  4. 成功時は view->update() で再描画
```

### 13.7 BoardImageExporter — 盤面画像エクスポータ

**ソース**: `src/board/boardimageexporter.h/.cpp`

盤面ウィジェットのキャプチャ画像をクリップボードまたはファイルに保存する静的ユーティリティ。インスタンスを持たず、全メソッドが `static` である。

#### copyToClipboard()

```cpp
static void copyToClipboard(QWidget* boardWidget);
```

`boardWidget->grab()` で `QPixmap` を取得し、`QApplication::clipboard()->setPixmap()` でクリップボードにコピーする。

#### saveImage()

```cpp
static void saveImage(QWidget* parent, QWidget* boardWidget);
```

処理フロー:

1. `QImageWriter::supportedImageFormats()` でサポートフォーマットを収集
2. PNG/TIFF/JPEG/WebP/BMP 等からフィルタリスト作成
3. `QFileDialog::getSaveFileName()` でファイル名を取得（デフォルト名: `ShogiBoard_yyyyMMdd_HHmmss.png`）
4. 拡張子からフォーマットを決定（JPEG/WebP は品質95で書き出し）
5. `QImageWriter` で画像を保存

### 13.8 SfenPositionTracer — SFEN局面トレーサ

**ソース**: `src/board/sfenpositiontracer.h/.cpp`

USI形式の手を順に適用して各手後のSFEN局面文字列を生成する軽量トレーサ。合法手チェックを行わない簡易実装で、棋譜読み込み時のSFEN列構築に使用される。

#### 内部データ構造

```
盤面: m_board[9][9] — 各マスのトークン文字列
  空白 = ""
  先手歩 = "P"、後手歩 = "p"
  先手成銀 = "+S"、後手成桂 = "+n"

持ち駒: m_handB[7], m_handW[7] — P/L/N/S/G/B/R の枚数

手番: m_blackToMove（true=先手）
手数: m_plyNext（SFENの第4フィールド）
```

#### applyUsiMove() — USI手の適用

```
applyUsiMove(usi):
  駒打ち（"P*5e"形式）:
    1. '*' の位置で判定
    2. 持ち駒から1枚減らす（subHand）
    3. 指定マスにトークンを配置

  通常手（"7g7f" / "2b3c+"形式）:
    1. 移動元・移動先の座標を解析
    2. 移動先に駒があれば捕獲（addHand）
    3. 成り指定があれば成駒トークンに変換
    4. 移動元を空、移動先にトークン配置

  共通: 手番反転、手数インクリメント
```

#### 座標変換

```
SFEN座標 → 内部配列:
  file (1-9) → col: 9 - file  （9筋が左端 = col 0）
  rank (a-i) → row: 0-8

USI座標例: "7g" → file=7, rank='g' → col=2, row=6
```

#### 静的ユーティリティメソッド

| メソッド | 用途 |
|---------|------|
| `buildSfenRecord(initialSfen, usiMoves, hasTerminal)` | 初期SFENとUSI手列から局面列を構築。`hasTerminal` が `true` のとき末尾に同一局面を複製（終局/中断表示用） |
| `buildGameMoves(initialSfen, usiMoves)` | USI手列から可視化用の `ShogiMove` 列を構築。各手の移動元/先・移動駒・取った駒を解析 |
| `dropFromSquare(dropUpper, black)` | 駒打ち時の擬似座標を返す（先手駒台: x=9、後手駒台: x=10） |
| `tokenToOneChar(tok)` | `"+P"` → `'Q'` 等、複数文字の成駒トークンを1文字にマップ |

#### buildSfenRecord() の処理フロー

```
buildSfenRecord(initialSfen, usiMoves, hasTerminal):
  1. SfenPositionTracer を作成、initialSfen をセット
  2. 開始局面の SFEN を list に追加
  3. usiMoves を順に applyUsiMove() → toSfenString() で局面列を構築
  4. hasTerminal → 末尾局面をもう1つ追加
  戻り値: [開始局面, 1手目後, 2手目後, ..., (終局複製)]
```

### 13.9 navigation層とboard層の協調

ナビゲーションと盤面操作は独立した層だが、以下の箇所で協調する。

| 協調ポイント | 関与クラス | 内容 |
|-------------|-----------|------|
| 盤面SFEN更新 | KifuNavigationController → BoardSyncPresenter | ナビゲーション完了時に `boardUpdateRequired(sfen)` で盤面を更新 |
| ハイライト同期 | RecordNavigationHandler → BoardInteractionController | 行変更時に直前手のハイライトを `showMoveHighlights()` で表示 |
| 局面編集↔通常モード切替 | PositionEditController → BoardInteractionController | 編集開始で `setMode(Edit)`、終了で通常モードに復帰 |
| SFEN追跡 | SfenPositionTracer → KifuNavigationState | 棋譜読み込み時に局面列を構築し、ナビゲーション可能にする |
| 分岐ナビ二重更新防止 | RecordNavigationHandler | `skipBoardSyncForBranchNav` フラグで分岐ツリー経由の更新と棋譜欄の更新が重複しないよう制御 |

<!-- chapter-13-end -->

---

<!-- chapter-14-start -->
## 第14章 MainWindowの役割と構造

### 14.1 設計思想: "MainWindow should stay lean"

CLAUDE.md に明記されているとおり、ShogiBoardQ の設計原則は **「MainWindow は痩せた状態を保つ」** ことである。MainWindow は UI 表示の起点であり、各コントローラ/コーディネータへの処理委譲のハブとして機能する。直接ロジックは最小限にとどめ、具体的な処理は専用クラスへ移譲する。

```
MainWindow（ハブ / ファサード）
  ├─ ensure*() で遅延生成 → 専用コントローラ/コーディネータ
  ├─ connect() でシグナル配線 → 委譲先クラスのスロット
  └─ UI骨格の構築（ドック、タブ、ツールバー）
```

この原則に基づき、当初 5,368 行あった `mainwindow.cpp` はリファクタリングにより約 170 行まで削減されている（2026-10-14 時点。詳細は [14.7節](#147-リファクタリング経緯) を参照）。

---

### 14.2 メンバ変数のグループ分け

`mainwindow.h` のメンバ変数は機能別にグループ化されている。以下に主要なカテゴリを示す。

#### 基本状態 / ゲーム状態

対局の進行状況を追跡する変数群。`src/app/mainwindowstate.h` の構造体にまとめ、`m_state`（`GameState`）と `m_kifu`（`KifuState`）として持つ。

| 変数 | 型 | 役割 |
|------|----|------|
| `m_state.startSfenStr` | `QString` | 開始局面の SFEN 文字列 |
| `m_state.currentSfenStr` | `QString` | 現在の局面の SFEN 文字列 |
| `m_state.playMode` | `PlayMode` | 現在のプレイモード（NotStarted, HvE 等） |
| `m_kifu.sfenRecord` | `QStringList` | 対局の局面列（`MatchCoordinator` にポインタで渡す。対局開始時に表示中の手順から作り直す） |
| `m_kifu.gameMoves` | `QList<ShogiMove>` | 対局の指し手列（同上） |

表示中の手数は MainWindow の変数ではなく、分岐ツリーのカーソル（`m_branchNav.navState` の `KifuNavigationState::currentPly()`）が持つ。棋譜の正本は分岐ツリー（`m_branchNav.branchTree`）である。

#### コントローラ / コーディネータ群

`ensure*()` で遅延生成される委譲先クラス群。多くは MainWindow が所有する（`&m_mw` を親に指定するか `std::unique_ptr` で持つ）。`m_match` と `m_gameStart` は `MatchCoordinatorWiring` が所有し、MainWindow は非所有のポインタだけを持つ。代表的なものを以下に示す。

| 変数 | クラス | 責務 |
|------|--------|------|
| `m_match` | `MatchCoordinator` | 対局進行の司令塔（起動時に一度だけ作る） |
| `m_gameStart` | `GameStartCoordinator` | 対局開始フロー全体の調整（1つだけ。最初に使うときに作る） |
| `m_dialogCoordinator` | `DialogCoordinator` | 各種ダイアログの表示と管理 |
| `m_kifuLoadCoordinator` | `KifuLoadCoordinator` | 棋譜読み込みフロー（最初の読み込みで作り、以後使い回す） |
| `m_gameStateController` | `GameStateController` | ゲーム状態遷移の管理 |
| `m_evalGraphController` | `EvaluationGraphController` | 評価値グラフの管理 |
| `m_boardSync` | `BoardSyncPresenter` | 盤面と内部状態の同期 |
| `m_recordPresenter` | `GameRecordPresenter` | 棋譜表示の管理 |
| `m_csaGameWiring` | `CsaGameWiring` | CSA 通信対局の UI 配線 |
| `m_considerationWiring` | `ConsiderationWiring` | 検討モードの UI 配線 |
| `m_playerInfoWiring` | `PlayerInfoWiring` | 対局者情報の UI 配線 |
| `m_matchWiring` | `MatchCoordinatorWiring` | 司令塔と `GameStartCoordinator` の生成・配線（両方を所有する） |
| `m_dockCreationService` | `DockCreationService` | ドックウィジェット生成 |
| `m_commentCoordinator` | `CommentCoordinator` | コメント機能の管理 |

#### モデル群

データバインディングに使用される Qt モデル。

| 変数 | クラス | 役割 |
|------|--------|------|
| `m_models.kifuRecord` | `KifuRecordListModel` | 棋譜レコードリスト |
| `m_models.kifuBranch` | `KifuBranchListModel` | 分岐リスト |
| `m_models.thinking1/2` | `ShogiEngineThinkingModel` | エンジン思考情報 |
| `m_models.consideration` | `ShogiEngineThinkingModel` | 検討タブ専用モデル |
| `m_models.analysis` | `KifuAnalysisListModel` | 棋譜解析結果 |
| `m_models.commLog1/2` | `UsiCommLogModel` | USI 通信ログ |
| `m_models.gameRecord` | `GameRecordModel` | 棋譜データの中央管理 |

モデルは `src/app/mainwindowstate.h` の `DataModels` にまとめて持つ。

#### UI ウィジェット / ドック

画面に表示される UI 部品とドックウィジェット。

| 変数 | 役割 |
|------|------|
| `m_shogiView` | 将棋盤ビュー |
| `m_tab` | メインタブウィジェット |
| `m_recordPane` | 棋譜欄ウィジェット |
| `m_analysisTab` | エンジン解析タブ |
| `m_evalChart` | 評価値グラフ |
| `m_docks.evalChart` | 評価値グラフ用ドック |
| `m_docks.recordPane` | 棋譜欄用ドック |
| `m_docks.gameInfo` | 対局情報ドック |
| `m_docks.thinking` | 思考ドック |
| `m_docks.comment` | 棋譜コメントドック |
| `m_docks.branchTree` | 分岐ツリードック |

ドックウィジェットは合計 12 個あり、`src/app/mainwindowstate.h` の `DockWidgets`（`m_docks`）に `QDockWidget*` としてまとめて持つ（ほかに検討・USI通信ログ・CSA通信ログ・メニューウィンドウ・定跡ウィンドウ・棋譜解析結果）。

#### 分岐ナビゲーション

棋譜の分岐構造を管理するクラス群。起動時（14.4 のステップ 5 の分岐ナビの初期化）に `KifuSubRegistry::createBranchNavigationParts()` が1回だけ作り、MainWindow が親として持つ。`BranchNavigationWiring` は作られたものを受け取って配線するだけである。

| 変数 | クラス | 役割 |
|------|--------|------|
| `m_branchNav.branchTree` | `KifuBranchTree` | 分岐ツリーデータ |
| `m_branchNav.navState` | `KifuNavigationState` | ナビゲーション状態 |
| `m_branchNav.kifuNavController` | `KifuNavigationController` | ナビゲーション操作 |
| `m_branchNav.displayCoordinator` | `KifuDisplayCoordinator` | 棋譜表示調整 |
| `m_branchNav.liveGameSession` | `LiveGameSession` | リアルタイム対局セッション |

---

### 14.3 ensure*() による遅延生成と依存更新

`MainWindowServiceRegistry`（実装は `gamesubregistry*.cpp`・`mainwindowanalysisregistry.cpp`・`mainwindowboardregistry.cpp`・`mainwindowuiregistry.cpp` などに分割）、
`MainWindowFoundationRegistry`、`KifuSubRegistry` が、利用時点の依存を集めて部品を生成し、依存を渡す。
生成済みオブジェクトを再利用し、可変の依存は呼び出しのたびに更新する。共通の契約は [4.2節](#42-ensure-遅延初期化パターン) を参照。

- `MatchCoordinator` は作り直さない。`initMatchCoordinator()` を2回目以降に呼ぶと、既存の司令塔に依存を結び直す（[14.4節](#144-コンストラクタの処理フロー)）。
- `KifuLoadCoordinator` は最初の読み込みで作り、以後の読み込みに使い回す（[8.7節](#87-kifuloadcoordinator--棋譜読み込みフロー)）。
- `GameStateController`、`BoardSetupController`、`PositionEditCoordinator` は、最新の対局・時計・局面編集コントローラを参照する。
- `CommentCoordinator` と `GameRecordPresenter` は、最新の棋譜モデル・コメント欄へ接続する。
- `DialogCoordinatorWiring` はコンテキストを更新し、初回のシグナル配線を重複させない。
- `ConsiderationWiring` は、生成済みのUIコントローラにも更新した依存を渡す。
- `PreStartCleanupHandler::updateDependencies()` は参照だけを更新する。棋譜の消去は `performCleanup()` で行う。

後から作られる部品や差し替わる値は、Deps に入れたゲッター（`std::function`）で使うときに読む。たとえば `GameSessionOrchestrator::Deps` のコントローラ取得関数は、遅延生成のもの（`gameStateController`・`gameStart`・`dialogCoordinator`）を取得時に作り、`csaGameCoordinator`・`match` はその時点の値を返す。子ハンドラの Refs でも、親が後から差し替えるポインタ（`KifuApplyService` の分岐ツリーや盤、`CsaMoveProgressHandler` のエンジンや時計）は、その時点の値を返す関数で受ける。

二重ポインタ（`T**`）で相手の変数に書き込む形は使わない。部品はレジストリが作って渡す（分岐ナビゲーションの部品、`ShogiGameController`・`ShogiView`、棋譜解析の結果のモデル）。配線クラスだけが使うダイアログ（CSA 通信対局の接続ダイアログ、局面集ビューア）は、その配線クラス（`DialogLaunchWiring`）が持つ。CSA 通信対局の司令塔は `CsaGameWiring` が作って持ち、ほかの部品は `MainWindowServiceRegistry::csaGameCoordinator()` で読む。

レジストリ内の状態消去・エンジンリセットは `MainWindowResetService` へ委譲する。
`ensure*()` は生成と接続の準備を担当し、実際の操作をサービス側に保つ。

`tst_service_initialization` は本体のMainWindowを使い、繰り返しの生成要求で
インスタンスが増えないこと、依存の差し替えと局面編集が機能することを確認する。

### 14.4 コンストラクタの処理フロー

MainWindow のコンストラクタは `MainWindowServiceRegistry` を作り、`createLifecyclePipeline()` で組み立てた `MainWindowLifecyclePipeline` の `runStartup()` を呼ぶだけである。起動手順は `MainWindowStartupSequence` が次の8段階の順に実行する（各段階の中身は `src/app/mainwindowlifecyclewiring.cpp` と `mainwindowuibootstrapper.cpp`）。この順序を崩すと null 参照や初期表示の不整合が起きやすい。

```
MainWindow::MainWindow()
 ├─ m_registry = MainWindowServiceRegistry
 ├─ m_pipeline = m_registry->createLifecyclePipeline()
 └─ m_pipeline->runStartup()
     ├─ 1. createFoundationObjects    // SignalRouter、通信ログ・検討タブ・棋譜解析の結果のモデル、ui->setupUi()
     ├─ 2. setupUiSkeleton            // 外観コントローラ、セントラルウィジェット、ツールバー
     ├─ 3. initializeCoreComponents   // GC・ShogiView を作り、MainWindowCoreInitCoordinator で盤を初期化
     ├─ 4. initializeEarlyServices    // PlayModePolicyService、TimeDisplayPresenter、ensureTimeController()
     │                                //   外観コントローラに盤と時間表示を渡す（refreshAppearanceDeps()）
     ├─ 5. buildGamePanels            // 棋譜欄・ドック・解析タブ・分岐ナビ（部品の生成と配線）の構築
     │     └─ SessionLifecycleCoordinator::startNewGame()
     │           └─ startGameSession() → initMatchCoordinator()   // ここで MatchCoordinator を作る（1回だけ）
     ├─ 6. restoreWindowAndSync       // ウィンドウサイズと起動時レイアウトの復元
     ├─ 7. connectSignals             // MainWindowSignalRouter::connectAll()、ツールチップ
     └─ 8. finalizeAndConfigureUi     // UI状態をアイドルに、言語・駒音・ドック管理、評価値グラフ用タイマー
           └─ finalizeCoordinators()
                 └─ initMatchCoordinator()   // 司令塔は作らず、依存を結び直す
```

**司令塔（MatchCoordinator）の生成**: `MainWindowServiceRegistry::initMatchCoordinator()` は `MatchCoordinatorWiring::initializeSession()` を呼ぶ。

- 初回（ステップ 5）は `createMatchCoordinator()` で司令塔を作り、司令塔のシグナル（終局・中断・盤面反転など）を受け手へ直接つなぐ（`connectMatchSignals()`、一度だけ）。受け手は `createMatchCoordinatorWiring()` が先に `MatchCoordinatorWiring::setSignalTargets()` で登録しておく。`GameStartCoordinator` のシグナルも、作るとき（`ensureMenuGameStartCoordinator()`）に同じ受け手へつなぐ（`connectGameStartSignals()`）。
- 2回目以降（ステップ 8）は司令塔を作り直さない。`MatchCoordinator::setEngineModels()` で思考情報・通信ログのモデルを渡し直し、`MatchCoordinator::resetPositionHistory()` で起動途中の仮の対局開始が残した局面の履歴を捨てる。
- どちらの場合も `bindRuntimeDependencies()` が、起動の途中で作られる部品との接続を結び直す（候補手の矢印、USIコマンド、PlayMode、評価値グラフ、UNDO、時間表示、時計の時間切れ、盤クリック、着手要求）。何度呼んでも接続が重ならないよう `Qt::UniqueConnection` を使う。
- 同じシグナルの受け手は、`connectMatchSignals()` がつないだ順に呼ばれる。`gameEnded` は `SessionLifecycleCoordinator::handleGameEnded()`（終局の記録）→ `UiStatePolicyManager::transitionToIdle()`（アイドルへの遷移）の順、`requestAppendGameOverMove` は終局手の追記 → ライブセッションの確定の順である（`tests/tst_wiring_contracts.cpp` が順を確かめる）。

以前は起動中に司令塔を2回（ステップ 5 と 8）作っていた。現在は一度だけ作り、`MainWindowAppearanceController` にも作った直後に渡す（`refreshAppearanceDeps()`）。

初期化順序の原則:
1. **UI 骨格**（central / toolbar）を最初に構築
2. **コア部品**（GameController / ShogiView / Clock）を生成
3. **ドック・各タブ**を構築
4. **設定復元とシグナル配線**を最後に実行

この順序により、シグナル配線時にはすべての UI 部品が存在することが保証される。

---

### 14.5 委譲先クラスの詳細

#### 14.5.1 AutomationServer（自動化 API）

**ファイル**: `src/automation/automationserver.h/.cpp`、`src/app/mainwindowautomationregistry.cpp`

`ShogiBoardQ --automation` で起動したときだけ、`MainWindowServiceRegistry::ensureAutomationServer()` が作る（親は MainWindow）。`QLocalServer` で改行区切りの JSON-RPC 2.0 を待ち受け、局面・棋譜・UI の操作を外部から行えるようにする。MCP サーバー（`mcp/`）と GUI のテスト（`tests/gui/`）がこれを使う。

部品は `AutomationContext` に詰めて渡し、メソッドの実装（`automationcommands_*.cpp`）は MainWindow を知らない。設計は [MCP サーバーの設計](mcp-server.md) を参照。

> 以前は MainWindow から切り出した `TestAutomationHelper`（`startTestGame()`・`makeTestMove()` など）があったが、2026-02-08 に削除した。外部からアプリを操作するときは、今はこの自動化 API を使う。

#### 14.5.2 DockCreationService

**ファイル**: `src/app/dockcreationservice.h/.cpp`

MainWindow からドックウィジェットの生成ロジックを分離したクラス。12 種類のドック（評価値グラフ、棋譜欄、対局情報、思考、検討、USI ログ、CSA ログ、コメント、分岐ツリー、メニューウィンドウ、定跡ウィンドウ、棋譜解析結果）の作成・設定復元・メニュー登録を担当する。

```
DockCreationService
  ├─ createEvalChartDock()        → 評価値グラフドック
  ├─ createRecordPaneDock()       → 棋譜欄ドック
  ├─ createAnalysisDocks()        → 解析系ドック一括生成
  │     ├─ 対局情報、思考、検討
  │     ├─ USIログ、CSAログ
  │     ├─ コメント、分岐ツリー
  │     └─ ※ 検討タブ用のモデルも設定
  ├─ createMenuWindowDock()       → メニューウィンドウドック
  ├─ createJosekiWindowDock()     → 定跡ウィンドウドック
  └─ createAnalysisResultsDock()  → 棋譜解析結果ドック
```

**依存設定**: setter メソッドで個別に設定する方式。

```cpp
m_dockCreationService->setDisplayMenu(ui->Display);
m_dockCreationService->setEvalChart(m_evalChart);
m_dockCreationService->setRecordPane(m_recordPane);
// ...
```

各 `create*()` メソッドは共通のヘルパー `setupDockFeatures()`, `addToggleActionToMenu()`, `restoreDockState()` を使い、ドック機能設定・メニュー登録・状態復元を統一的に処理する。

#### 14.5.3 CommentCoordinator

**ファイル**: `src/app/commentcoordinator.h/.cpp`

棋譜コメントの表示更新・保存・同期処理を MainWindow から分離したクラス。

**提供機能**:
- `broadcastComment()` — コメントを EngineAnalysisTab と RecordPane に配信
- `onCommentUpdated()` — UI からのコメント更新を処理
- `onGameRecordCommentChanged()` / `onBookmarkChanged()` — GameRecordModel の変更シグナルから表示を更新

編集する手数は、`setNavigationState()` で受け取った分岐ツリーのカーソル（`KifuNavigationState::currentPly()`）から決める。

**遅延初期化ギャップの解決**:

CommentCoordinator は `GameRecordModel` が必要になることがあるが、`GameRecordModel` は遅延生成される。この問題を解決するため、`ensureGameRecordModelRequested` シグナルを定義し、MainWindow 側で接続する。

```cpp
// MainWindowFoundationRegistry::ensureCommentCoordinator() 内
connect(m_mw.m_commentCoordinator, &CommentCoordinator::ensureGameRecordModelRequested,
        m_serviceRegistry->kifu(), &KifuSubRegistry::ensureGameRecordModel, Qt::UniqueConnection);
```

CommentCoordinator は `GameRecordModel` の生成方法を知らないが、シグナル発行によりレジストリの `ensure*()` を間接的に呼び出せる。

---

### 14.6 MainWindow のスロット構造

MainWindow が受けるのは、ほかの部品から呼ばれる少数の公開メソッドと、3つの private スロットだけである。どれも `MainWindowServiceRegistry` などへ委譲する短い関数である。

| カテゴリ | メソッド | 接続元 |
|----------|---------|--------|
| メニュー操作 | `resetToInitialState()`、`beginPositionEditing()`、`finishPositionEditing()`、`undoLastTwoMoves()`、`saveSettingsAndClose()` | `GameActionsWiring`・`ViewActionsWiring`・`FileActionsWiring`（`UiActionsWiring` の子） |
| 盤の操作 | `onMoveRequested()`、`onMoveCommitted()` | `MainWindowSignalRouter`、`KifuSubRegistry` |
| 棋譜欄と盤の同期 | `onRecordPaneMainRowChanged()`、`loadBoardWithHighlights()`、`setCurrentTurn()`、`updateJosekiWindow()`、`displayGameRecord()` | `RecordPaneWiring`・`RecordNavigationWiring`・`BranchNavigationWiring`・`KifuLoadCoordinator` |
| private スロット | `onTurnManagerChanged()`、`setReplayMode()`、`loadBoardFromSfen()` | `TurnManager`（`TurnSyncBridge`）、`GameStateController` のフック、`BranchNavigationWiring` |

ファイルの読み書き・対局・ダイアログなど、ほとんどのメニュー操作は MainWindow を経由しない。`UiActionsWiring` の子（`FileActionsWiring` など）が、担当するクラス（`KifuFileController`・`GameSessionOrchestrator`・`DialogLaunchWiring` など）のスロットへ直接つなぐ。矢印ボタンなどの有効・無効は `UiStatePolicyManager` が状態ごとに決める（[UI状態ポリシー](ui-state-policy.md)）。

---

### 14.7 リファクタリング経緯

MainWindow は段階的にリファクタリングされ、ロジックが専用クラスへ移譲されてきた。

#### 行数の推移

| 時期 | mainwindow.cpp 行数 | mainwindow.h 行数 |
|------|---------------------|-------------------|
| リファクタリング前 | 5,368 行 | 765 行 |
| 第1次完了後 | 4,388 行 | 721 行 |
| 第2次完了後 | 約 4,180 行 | 725 行 |
| 委譲の完了後（Task 65） | 379 行 | 545 行 |
| 現在（2026-10-14） | 171 行 | 238 行 |

部品の生成と依存の更新は `MainWindowServiceRegistry`（`gamesubregistry*.cpp`・`mainwindow*registry.cpp`）と `MainWindowFoundationRegistry`・`KifuSubRegistry` に移り、MainWindow には委譲するだけの短い関数が残っている。

#### 主な移譲先と内容

| 移譲先クラス | 移譲された責務 |
|-------------|---------------|
| `MainWindowServiceRegistry` ほか | `ensure*()` による部品の生成と依存の更新、主要な操作 |
| `DockCreationService` | ドックウィジェットの生成・設定復元・メニュー登録 |
| `CommentCoordinator` | `broadcastComment()`, コメント更新・同期処理 |
| `ConsiderationWiring` | 検討モードの UI 配線（ダイアログ↔エンジン間の接続） |
| `PlayerInfoWiring` | 対局者名の解決、ゲーム情報の設定、エンジン名タブ更新 |
| `RecordNavigationHandler` | 棋譜欄の行選択変更ハンドラ |
| `BoardSyncPresenter` | 盤面とハイライトの同期（`loadBoardWithHighlights()` 等） |
| `KifuNavigationController` | 分岐ノード活性化ハンドラ |
| `KifuLoadCoordinator` | 棋譜読み込みフローの重複排除 |

#### リファクタリングの方針

1. **ラッパーメソッドの除去**: MainWindow が単にメソッド呼び出しを中継するだけのラッパーは、`connect()` を委譲先クラスへ直接接続することで削除
2. **デッドコードの除去**: どのシグナルにも接続されておらず、直接呼び出しもないスロットを特定して削除
3. **コールバックによる間接呼び出し**: 委譲先クラスが MainWindow の `ensure*()` を呼ぶ必要がある場合、`std::function` コールバックを Deps に含めて解決
4. **状態表による UI の有効・無効**: 以前は `enableArrowButtons()` などを多数のクラスから MainWindow へつないでいたが、`UiStatePolicyManager` の状態表に置き換えて MainWindow から削除した

<!-- chapter-14-end -->

---

<!-- chapter-15-start -->
## 第15章 機能フロー詳解

本章では、ShogiBoardQの主要な5つのユースケースについて、シーケンス図と解説を通じてデータと制御の流れを追う。各フローは実際のクラス名・メソッド名に対応しており、コードリーディングの導線として活用できる。

---

### 15.1 対局開始フロー

対局開始は、ユーザーがメニューから「新規対局」を選択し、ダイアログで設定を行ってから、盤面・時計・エンジンが初期化されるまでの一連の処理である。

```mermaid
sequenceDiagram
    participant User as ユーザー
    participant MW as MainWindow
    participant SGD as StartGameDialog
    participant GSC as GameStartCoordinator
    participant PSCH as PreStartCleanupHandler
    participant MC as MatchCoordinator
    participant USI as Usi

    User->>MW: メニュー「新規対局」選択
    MW->>SGD: ダイアログ表示
    User->>SGD: 対局設定を入力しOK
    SGD->>MW: ダイアログ結果を返却
    MW->>GSC: prepare(Request)
    GSC-->>PSCH: requestPreStartCleanup シグナル
    PSCH->>PSCH: performCleanup()
    Note over PSCH: 盤面/ハイライト初期化<br/>棋譜モデルクリア<br/>USIログリセット<br/>時計リセット<br/>評価値グラフ初期化
    MW->>GSC: start(StartParams)
    GSC->>MC: configureAndStart(StartOptions)
    MC->>MC: initializePositionStringsForStart(sfen)
    MC->>USI: initializeAndStartEngineCommunication()
    Note over USI: エンジンプロセス起動<br/>usi → usiok<br/>isready → readyok
    MC->>MC: startInitialEngineMoveIfNeeded()
    alt エンジンが先手の場合
        MC->>USI: go (時間パラメータ)
    end
    GSC-->>MW: started シグナル
    Note over MW: ナビゲーション無効化<br/>対局中UI状態へ遷移
```

**主要クラスの役割:**

| クラス | 責務 |
|--------|------|
| `StartGameDialog` | 対局モード・エンジン・時間設定のUI入力を収集 |
| `GameStartCoordinator` | 対局開始フロー全体のオーケストレーション |
| `PreStartCleanupHandler` | 前回の対局状態をクリーンアップ（盤面・棋譜・時計・評価値等） |
| `MatchCoordinator` | 対局進行の司令塔。エンジン初期化、position文字列管理、初手goを制御 |
| `Usi` | USIプロトコル通信ファサード。エンジンプロセスの起動と初期化を実行 |

**補足:**
- `GameStartCoordinator::prepare()` は時計の適用と`requestPreStartCleanup`シグナルの発行を行う。クリーンアップの実体は`PreStartCleanupHandler`に完全委譲されている
- `configureAndStart()` 内部では対局モード（HvH / HvE / EvE）に応じて `startHumanVsHuman()` / `startHumanVsEngine()` / `startEngineVsEngine()` に分岐する
- EvEモードでは2つの`Usi`インスタンスが生成され、交互にgo/bestmoveを繰り返す

---

### 15.2 指し手実行フロー

人間が盤面をクリックして駒を移動し、合法手判定・盤面更新・棋譜記録・エンジン応答までを含む一連の処理。

```mermaid
sequenceDiagram
    participant User as ユーザー
    participant SV as ShogiView
    participant BIC as BoardInteractionController
    participant MW as MainWindow
    participant BSC as BoardSetupController
    participant SGC as ShogiGameController
    participant MV as EngineMoveValidator
    participant SB as ShogiBoard
    participant MC as MatchCoordinator
    participant GRU as GameRecordUpdateService
    participant USI as Usi

    User->>SV: 盤面クリック（1回目: 駒選択）
    SV-->>BIC: clicked シグナル → onLeftClick()
    BIC->>BIC: selectPieceAndHighlight()
    Note over BIC: オレンジ色でハイライト表示<br/>（設定により合法な移動先も表示）

    User->>SV: 盤面クリック（2回目: 移動先指定）
    SV-->>BIC: clicked シグナル → onLeftClick()
    BIC-->>MW: moveRequested(from, to) シグナル
    MW->>BSC: onMoveRequested()<br/>（MainWindowServiceRegistry 経由）

    BSC->>SGC: validateAndMove()
    SGC->>MV: isLegalMove()（盤の配列・駒台・指し手）
    MV-->>SGC: 成り・不成それぞれの合法性

    alt 合法手の場合
        SGC->>SB: applyUsiMove()
        SB-->>SV: dataChanged / standChanged シグナル
        SV->>SV: 変わったマス・駒台を再描画
        SGC-->>MW: moveCommitted シグナル
        Note over MW: 現在の局面・定跡ウィンドウを更新<br/>（駒音は PieceSoundPlayer）
        SGC-->>BSC: true
        BSC->>BIC: onMoveApplied(from, to, true)
        Note over BIC: 赤(移動元)/黄(移動先)ハイライト
        BSC->>MC: onHumanMove(from, to, 棋譜の文字列)
        MC->>GRU: appendKifuLine()（考慮時間つきで棋譜欄に追記）

        alt エンジン手番（HvEモード）
            MC->>USI: requestHumanReply()（position + go）
            USI-->>MC: matchMoveReady シグナル（bestmove）
            MC->>SGC: applyMove()（合法手か確かめてから指す）
            MC->>GRU: エンジンの手を棋譜欄に追記
        end
    else 違法手の場合
        SGC-->>BSC: false（盤は変わらない）
        BSC->>BIC: onMoveApplied(from, to, false)
        Note over BIC: 駒の選択とドラッグを解除
    end
```

**主要クラスの役割:**

| クラス | 責務 |
|--------|------|
| `ShogiView` | Qt Graphics Viewベースの盤面描画とマウスイベントの発行 |
| `BoardInteractionController` | 2クリック方式の駒選択・移動要求、ハイライト管理 |
| `BoardSetupController` | 着手要求を受け、モード（局面編集・棋譜の変化の入力・対局）ごとに振り分ける |
| `ShogiGameController` | 指し手の検証と盤面更新の実行 |
| `EngineMoveValidator` | 合法手判定ロジック（王手放置、二歩、打ち歩詰め、行き所のない駒等を検査） |
| `ShogiBoard` | 9x9盤面 + 駒台のデータモデル |
| `GameRecordUpdateService` | 棋譜欄（`KifuRecordListModel`）への追記とライブセッションの更新 |

**補足:**
- `MainWindow` は着手要求を `MainWindowServiceRegistry::handleMoveRequested()` に渡すだけで、判定と後処理は `BoardSetupController::onMoveRequested()` が行う
- 成り・不成は `ShogiGameController::validateAndMove()` が決める。両方とも合法なら成りダイアログ（`setPromotionChooser()` で渡す `DialogLaunchWiring::choosePromotion()`）で選ばせ、成りだけが合法（行き所のない駒）なら成る（6.6 節）
- `MatchCoordinator::onHumanMove()` は `MatchTurnHandler` を通して対局モードの Strategy（HvE なら `HumanVsEngineStrategy`）に渡す。HvE では考慮時間を確定して棋譜に追記したあと、`Usi::requestHumanReply()` でエンジンに position と go を送り、`matchMoveReady` シグナルで応手を受け取る
- HvHモードでは Strategy が考慮時間と棋譜を記録し、手番が交互に切り替わるだけで、エンジンとの通信はない。通信対局では `BoardSetupController` が `csaMoveRequested` シグナルで手をサーバーへ送る
- 対局していないときに盤で指した手は、`BranchTreeEditController::recordBoardMove()` が棋譜の変化として記録する

---

### 15.3 棋譜読み込みフロー

棋譜ファイル（KIF / KI2 / CSA / JKF / USEN / USI形式）を読み込み、内部データモデルを構築してUIに反映するまでのフロー。

```mermaid
sequenceDiagram
    participant User as ユーザー
    participant KFC as KifuFileController
    participant KLC as KifuLoadCoordinator
    participant KLP as KifuLoadParser
    participant KAS as KifuApplyService
    participant KBT as KifuBranchTree
    participant KRM as KifuRecordListModel
    participant SV as ShogiView

    User->>KFC: ファイル選択（メニュー「棋譜を開く」）
    KFC->>KLC: loadFileAsync(filePath)
    KLC->>KLP: parseFile(filePath)（別スレッド）
    Note over KLP: 形式を判定して変換器の parseWithVariations() を呼ぶ<br/>初期局面SFEN・対局情報・本譜と変化（KifParseResult）を返す
    KLP-->>KLC: KifuLoadResult

    KLC->>KAS: populateGameInfo() / applyPlayersFromGameInfo()
    KLC->>KAS: applyParsedResult(..., record, ...)

    KAS-->>KRM: displayGameRecord フック
    KRM->>KRM: 表示データを設定

    KAS->>KBT: buildFromKifParseResult()
    Note over KBT: 本譜ラインをルートに設定<br/>変化手順を子ノードとして追加<br/>（棋譜の正本）

    KAS-->>SV: syncBoardAndHighlightsAtRow(0) フック
    SV->>SV: 開始局面を盤面に描画
```

**主要クラスの役割:**

| クラス | 責務 |
|--------|------|
| `KifuLoadCoordinator` | 棋譜読み込みの統括。パースの起動（別スレッド）と結果の適用 |
| `KifuLoadParser` | 形式判定とフォーマット変換器の呼び出し。結果を `KifuLoadResult` にまとめる |
| `KifuApplyService` | 解析結果を棋譜欄・分岐ツリー・盤面に当てはめる |
| `KifToSfenConverter` | KIF形式の解析。手合割→SFEN変換、指し手→USI変換、変化抽出 |
| `KifuBranchTree` | 分岐棋譜のツリー構造。本譜と変化を木構造で管理する棋譜の正本 |
| `KifuRecordListModel` | 棋譜欄表示用のQtモデル |

**補足:**
- 同期版の `loadKifuFromFile()` などと、テキストからの読み込み（`loadTextAsync()` / `loadKifuFromString()`、クリップボード貼り付けで使用、形式を自動判定）も同じ `applyLoadResult()` を通る
- KIF以外の形式も同様のパターンで処理される: `Ki2ToSfenConverter`（KI2形式）、`CsaToSfenConverter`（CSA形式）等
- 分岐ツリー構築後、`KifuApplyService` が分岐のある手数を棋譜モデルの `setBranchPlyMarks()` に渡す
- 読み込みは対局の局面列・指し手列（`sfenRecord` / `gameMoves`）を書かない。対局開始時に表示中の手順から作り直す（8.7）

---

### 15.4 検討モードフロー

検討モードは、現在の局面をエンジンに解析させ、候補手と評価値をリアルタイム表示する機能。局面移動に追従して再解析する動作も含む。

```mermaid
sequenceDiagram
    participant User as ユーザー
    participant EAT as EngineAnalysisTab
    participant CW as ConsiderationWiring
    participant CFC as ConsiderationFlowController
    participant MC as MatchCoordinator
    participant USI as Usi
    participant TIP as ThinkingInfoPresenter
    participant SETM as ShogiEngineThinkingModel
    participant CMUIC as ConsiderationModeUIController

    User->>EAT: 検討ボタンクリック
    EAT-->>CW: 検討開始リクエスト
    CW->>CW: displayConsiderationDialog()
    CW->>CFC: runDirect(deps, params, positionStr)

    CFC->>MC: startAnalysis(AnalysisOptions)
    MC->>USI: startAndInitializeEngineAsync(enginePath)
    Note over USI: エンジンプロセス起動<br/>usi → usiok<br/>isready → readyok
    MC->>USI: sendAnalysisCommands(positionStr, byoyomiMs, multiPV)
    Note over USI: position ... を送信<br/>go infinite を送信

    loop エンジン思考中
        USI-->>TIP: info行受信
        TIP->>TIP: processInfoLine()
        Note over TIP: 評価値・読み筋・深さ等を解析<br/>USI指し手を漢字棋譜に変換
        TIP-->>USI: thinkingInfoUpdated シグナル
        USI-->>SETM: 思考モデルに行追加
        CMUIC->>CMUIC: updateArrows()
        Note over CMUIC: 候補手を盤面に矢印表示
    end

    alt 時間制限あり
        USI->>USI: タイマー発火 → stop送信
        USI-->>MC: bestMoveReceived シグナル
        MC-->>CMUIC: considerationWaitingStarted シグナル
        Note over CMUIC: 待機状態へ（次の局面選択待ち）
    end

    alt ユーザーが局面を移動
        User->>CW: 棋譜欄で別の手を選択
        CW->>MC: updateConsiderationPosition(newPositionStr)
        MC->>USI: stop → bestmove待ち
        MC->>USI: sendAnalysisCommands(newPositionStr)
        Note over MC: 新しい局面で検討を再開
    end

    User->>EAT: 検討中止ボタンクリック
    EAT-->>CW: stopRequested シグナル
    MC->>USI: stop
    MC->>MC: stopAnalysisEngine()
    MC-->>CMUIC: considerationModeEnded シグナル
    Note over CMUIC: 矢印クリア<br/>UI状態を復元
```

**主要クラスの役割:**

| クラス | 責務 |
|--------|------|
| `EngineAnalysisTab` | 検討タブUI。エンジン選択・時間設定・MultiPV設定・検討開始/中止ボタン |
| `ConsiderationWiring` | 検討モード関連のシグナル/スロット配線。UIコントローラの管理 |
| `ConsiderationFlowController` | 検討パラメータの組み立てと`MatchCoordinator`への委譲 |
| `MatchCoordinator` | 検討エンジンの起動・停止・局面変更時の再起動を管理 |
| `Usi` | エンジンへのposition/goコマンド送信、info行の受信 |
| `ThinkingInfoPresenter` | info行を解析し、評価値・読み筋（漢字変換含む）をシグナルで通知 |
| `ShogiEngineThinkingModel` | 思考情報の表示モデル（MultiPV対応） |
| `ConsiderationModeUIController` | 検討モードのUI状態管理。矢印描画・タイマー表示・ボタン制御 |

**補足:**
- 検討モードのMultiPVは動的に変更可能。`ConsiderationModeUIController::onMultiPVChanged()` → `MatchCoordinator::updateConsiderationMultiPV()` → `Usi::updateConsiderationMultiPV()` の経路で伝播する
- 局面変更時は `updateConsiderationPosition()` が呼ばれ、現在の検討をstopしてから新局面で再開する。二重再開を防ぐ `m_considerationRestartInProgress` フラグがある
- 矢印表示は `ConsiderationModeUIController::updateArrows()` が `ShogiEngineThinkingModel` からUSI形式の候補手を読み取り、`ShogiView` に矢印オーバーレイを描画する

---

### 15.5 CSA通信対局フロー

CSAプロトコルを使用したネットワーク対局の、接続からログイン、対局開始、指し手交換、対局終了までの一連のフロー。

```mermaid
sequenceDiagram
    participant User as ユーザー
    participant CGD as CsaGameDialog
    participant CGC as CsaGameCoordinator
    participant CC as CsaClient
    participant Srv as CSAサーバー
    participant SGC as ShogiGameController
    participant SV as ShogiView
    participant USI as Usi

    User->>CGD: 接続情報を入力しOK
    CGD->>CGC: startGame(StartOptions)
    CGC->>CC: connectToServer(host, port)
    CC->>Srv: TCP接続
    Srv-->>CC: 接続確立
    CC-->>CGC: connectionStateChanged(Connected)

    CGC->>CC: login(username, password)
    CC->>Srv: LOGIN username password
    Srv-->>CC: LOGIN:username OK
    CC-->>CGC: loginSucceeded シグナル

    Note over Srv: マッチング処理
    Srv-->>CC: BEGIN Game_Summary
    CC->>CC: processGameSummary()
    Note over CC: 対局ID・対局者名<br/>手番・時間設定<br/>初期局面を解析
    Srv-->>CC: END Game_Summary
    CC-->>CGC: gameSummaryReceived シグナル

    CGC->>CGC: setupInitialPosition()
    CGC->>SGC: 初期局面を設定
    CGC->>SV: 盤面描画

    CGC->>CC: agree(gameId)
    CC->>Srv: AGREE gameId
    Srv-->>CC: START:gameId
    CC-->>CGC: gameStarted シグナル
    CGC-->>User: gameStarted(blackName, whiteName)

    alt 人間が操作する場合
        loop 対局中
            alt 自分の手番
                User->>SV: 盤面クリック（指し手）
                SV-->>CGC: onHumanMove(from, to, promote)
                CGC->>CC: sendMove("+7776FU")
                CC->>Srv: +7776FU
                Srv-->>CC: +7776FU,T3
                CC-->>CGC: moveConfirmed シグナル
                CGC-->>SV: 盤面更新・ハイライト
            else 相手の手番
                Srv-->>CC: -3334FU,T5
                CC-->>CGC: moveReceived(move, consumedTimeMs)
                CGC->>CGC: applyMoveToBoard(csaMove)
                CGC->>SGC: 盤面に指し手を適用
                CGC-->>SV: 盤面更新・ハイライト
            end
        end
    else エンジンが操作する場合
        CGC->>USI: initializeEngine()
        loop 対局中
            alt 自分の手番
                CGC->>USI: startEngineThinking()
                USI-->>CGC: bestMoveReceived シグナル
                CGC->>CC: sendMove(usiToCsa(bestmove))
            else 相手の手番
                Srv-->>CC: 相手の指し手
                CC-->>CGC: moveReceived シグナル
                CGC->>SGC: 盤面に適用
            end
        end
    end

    Srv-->>CC: #RESIGN or #TIME_UP etc.
    CC->>CC: processResultLine()
    CC-->>CGC: gameEnded(result, cause, consumedTimeMs)
    CGC-->>User: gameEnded シグナル
    CGC->>CGC: cleanup()
    CC->>Srv: LOGOUT
```

**主要クラスの役割:**

| クラス | 責務 |
|--------|------|
| `CsaGameDialog` | CSA通信対局の接続設定UI（ホスト・ポート・ユーザー名・パスワード・エンジン選択） |
| `CsaGameCoordinator` | CSA通信対局の進行管理。CsaClientとGUIコンポーネントの橋渡し |
| `CsaClient` | CSAプロトコルのTCP/IP通信実装。メッセージの送受信と状態管理 |
| `ShogiGameController` | 盤面データの更新 |
| `Usi` | エンジン対局時のUSIエンジン通信（オプション） |

**補足:**
- `CsaClient`は状態マシンパターンで実装されており、`ConnectionState`が `Disconnected → Connecting → Connected → LoggedIn → WaitingForGame → GameReady → InGame → GameOver` と遷移する
- CSA形式の指し手（例: `+7776FU`）とUSI形式の指し手（例: `7g7f`）の変換は`CsaGameCoordinator`内の `csaToUsi()` / `usiToCsa()` が担当する
- エンジン対局時は `CsaGameCoordinator` が `Usi` インスタンスを所有し、`bestMoveReceived` シグナルで応手を受け取ってCSAサーバーに送信する
- 終局原因（投了・時間切れ・千日手・入玉宣言等）は `CsaClient::GameEndCause` 列挙型で分類され、対応する結果表示が行われる

<!-- chapter-15-end -->

---

<!-- chapter-16-start -->
## 第16章 国際化（i18n）と翻訳

ShogiBoardQは日本語・英語・簡体字中国語・繁体字中国語の4言語をサポートしている（原文は日本語）。本章ではQtの国際化フレームワークを活用した翻訳の仕組みと、実行時の言語切替メカニズムについて解説する。

### 16.1 翻訳ワークフローの全体像

ソースコード中の `tr()` マクロで囲まれた文字列が翻訳対象となり、以下のパイプラインで処理される。

```
┌─────────────────────────────────────────────────────────────────┐
│                     翻訳ワークフロー                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ① ソースコード (.cpp / .h / .ui)                                │
│      tr("言語設定") / tr("棋譜解析完了")                           │
│            │                                                    │
│            ▼                                                    │
│  ② lupdate（文字列抽出）                                         │
│      cmake --build build --target update_translations          │
│            │                                                    │
│            ▼                                                    │
│  ③ .ts ファイル編集（XML形式）                                    │
│      <source> に対する <translation> を記述                       │
│            │                                                    │
│            ▼                                                    │
│  ④ lrelease / cmake ビルド（バイナリ変換）                        │
│      qt_add_translation() が .ts → .qm を生成                   │
│            │                                                    │
│            ▼                                                    │
│  ⑤ .qm ファイル（コンパクトなバイナリ翻訳）                       │
│      実行時に QTranslator::load() で読み込み                      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

各ステップの概要を以下にまとめる。

| ステップ | ツール/ファイル | 役割 |
|----------|----------------|------|
| ① `tr()` | ソースコード中のマクロ | 翻訳対象の文字列をマーク |
| ② `lupdate` | `update_translations` ターゲット | ソースから `.ts` ファイルへ文字列を抽出・同期（明示的に実行する） |
| ③ `.ts` 編集 | テキストエディタ or Qt Linguist | 翻訳テキストを入力 |
| ④ `lrelease` | CMakeビルド時に自動実行 | `.ts` → `.qm` バイナリ変換 |
| ⑤ `.qm` ロード | `QTranslator` + `QApplication` | 実行時に翻訳を適用 |

### 16.2 翻訳ファイルの構造

翻訳ファイルは `resources/translations/` に格納されるXML形式の `.ts` ファイルである。

| ファイル | 言語 |
|---------|------|
| `ShogiBoardQ_ja_JP.ts` | 日本語（原文と同じ訳を入れる） |
| `ShogiBoardQ_en.ts` | 英語 |
| `ShogiBoardQ_zh_CN.ts` | 簡体字中国語 |
| `ShogiBoardQ_zh_TW.ts` | 繁体字中国語（台湾の用語） |

4言語とも未翻訳（`type="unfinished"`）を0件に保つ。CI の `translation-check` ジョブは、未翻訳の数が `resources/translations/baseline-unfinished.txt` から増えると失敗する。`tst_translation_files` は、4言語のカタログ（原文の集合）とプレースホルダー（`%1` など）が揃っているかを確かめる。

#### .ts ファイルのXML構造

```xml
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ja_JP">
  <context>
    <name>AnalysisFlowController</name>       <!-- クラス名 -->
    <message>
      <location filename="../../src/analysis/analysisflowcontroller.cpp" line="49"/>
      <source>内部エラー: sfenRecord が未準備です。</source>  <!-- 原文 -->
      <translation type="unfinished"></translation>           <!-- 翻訳 -->
    </message>
    ...
  </context>
  <context>
    <name>BranchTreeManager</name>
    ...
  </context>
</TS>
```

主要なXML要素を以下に示す。

| 要素 | 説明 |
|------|------|
| `<TS>` | ルート要素。`version` と `language` 属性を持つ |
| `<context>` | クラス単位のグルーピング。`<name>` でクラス名を示す |
| `<message>` | 翻訳単位。1つの `tr()` 呼び出しに対応 |
| `<location>` | ソースファイルのパスと行番号（`lupdate` が自動設定） |
| `<source>` | 翻訳元の文字列（`tr()` に渡された文字列） |
| `<translation>` | 翻訳先の文字列。未翻訳の場合は `type="unfinished"` |

`<translation>` 要素の `type` 属性の意味は以下の通り。

| type属性 | 意味 |
|----------|------|
| `type="unfinished"` | 未翻訳。翻訳テキストを追加する必要がある |
| `type="obsolete"` | ソースコードから該当する `tr()` が削除された |
| （属性なし） | 翻訳済み。正常状態 |

### 16.3 CMakeによるビルド統合

`CMakeLists.txt` で翻訳ファイルのビルドパイプラインが定義されている。

```cmake
# ==== Translations (.ts) ====
set(TS_FILES
    resources/translations/ShogiBoardQ_ja_JP.ts
    resources/translations/ShogiBoardQ_en.ts
    resources/translations/ShogiBoardQ_zh_CN.ts
    resources/translations/ShogiBoardQ_zh_TW.ts
)

# .qm 生成（lrelease のみ。ソースのスキャンはしない）
qt_add_translation(QM_FILES ${TS_FILES})
add_custom_target(translations ALL DEPENDS ${QM_FILES})
add_dependencies(ShogiBoardQ translations)

# lupdate（ソースから .ts を更新）は明示的に実行する
add_custom_target(update_translations
    COMMAND Qt6::lupdate ${CMAKE_SOURCE_DIR}/src -ts ${TS_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# .qm ファイルを実行可能ファイルと同じディレクトリにコピー
foreach(QM_FILE ${QM_FILES})
    add_custom_command(TARGET ShogiBoardQ POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${QM_FILE}
            $<TARGET_FILE_DIR:ShogiBoardQ>)
endforeach()

# Qt 標準ダイアログの翻訳（qtbase_ja / zh_CN / zh_TW）をリソースに同梱する
qt_add_resources(ShogiBoardQ qt_dialog_translations PREFIX "/translations/qt" ...)
```

ポイント:

- 通常のビルドは `lrelease` だけを行う。ソースの `tr()` を `.ts` に反映するには `update_translations` ターゲットを実行する
- `lupdate` のスキャン対象を `${CMAKE_SOURCE_DIR}/src` に限定することで、`build/` 配下の autogen ファイルを拾わない
- 生成された `.qm` ファイルはビルド後に実行ファイルと同じディレクトリへコピーされる
- Qt 標準ダイアログ（ファイル選択など）の翻訳は、配布スクリプトが落としても使えるよう `:/translations/qt/` に埋め込む

### 16.4 起動時の翻訳ロード

`main.cpp` は `QApplication` を作った直後、ウィジェットを作る前に `ApplicationTranslations`（`src/services/applicationtranslations.h/.cpp`）を作り、終了処理まで持っておく。`ApplicationTranslations` は `AppSettings::language`（`General/language`）を読み、言語を1つに決めて翻訳を読み込む。

```
┌──────────────────────────────────────────────────────────┐
│               翻訳ロードのフロー                            │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  AppSettings::language() で設定値を取得                     │
│         │  "system" ならシステムの UI 言語の先頭を使う         │
│         ▼                                                │
│  KifuPresentation::resolveLanguage()                     │
│         ├── 日本語 → "ja_JP"                               │
│         ├── 中国語 → 繁体字なら "zh_TW"、それ以外は "zh_CN"   │
│         └── それ以外 → "en"                                │
│         │                                                │
│         ├→ Qt 標準ダイアログ: qtbase_<言語>.qm               │
│         │   （:/translations/qt/ → Qt の翻訳ディレクトリ）     │
│         ├→ アプリ: ShogiBoardQ_<言語>.qm                    │
│         │   （実行ファイルと同じディレクトリ）                  │
│         └→ 棋譜表記の設定: KifuPresentation::configure()     │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

`main.cpp` の該当部分（抜粋）:

```cpp
// 終了時の設定初期化メッセージも翻訳できるよう、このスコープで保持する。
const ApplicationTranslations translations;

configureApplicationAppearance(a, translations.language());  // フォントも言語に合わせる
```

`ApplicationTranslations` のコンストラクタ（抜粋）:

```cpp
m_language = KifuPresentation::resolveLanguage(AppSettings::language(), systemLanguage);

// Qt 標準ダイアログの翻訳（日本語は "ja"）
const QString qtLanguage = m_language == QStringLiteral("ja_JP") ? QStringLiteral("ja") : m_language;
if (m_qtTranslator.load(":/translations/qt/qtbase_" + qtLanguage + ".qm")
    || m_qtTranslator.load("qtbase_" + qtLanguage, QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
    QCoreApplication::installTranslator(&m_qtTranslator);

// アプリの翻訳
if (m_appTranslator.load(directory.filePath("ShogiBoardQ_" + m_language + ".qm")))
    QCoreApplication::installTranslator(&m_appTranslator);

// 棋譜の表記（漢数字か英字か、など）は言語と表記の設定から決める
KifuPresentation::configure(m_language, AppSettings::moveNotation(), AppSettings::notationOrigin());
```

言語は日本語・英語・簡体字・繁体字のいずれかに必ず決まる。日本語のときも `ShogiBoardQ_ja_JP.qm` を読み込む（訳は原文と同じ）。`QTranslator` は `ApplicationTranslations` のメンバなので、破棄するとアプリケーションへの登録も外れる。

### 16.5 LanguageController: 実行時言語切替

LanguageControllerは、メニューからの言語と棋譜表記の選択をハンドリングし、設定の保存と再起動の案内を行うコントローラである。

**ファイル**: `src/ui/controllers/languagecontroller.h`, `src/ui/controllers/languagecontroller.cpp`

#### クラス構成

```
LanguageController (QObject)
├── メンバ
│   ├── m_actionGroup: QActionGroup*      言語の排他選択グループ
│   ├── m_systemAction: QAction*          「システム言語」メニュー項目
│   ├── m_japaneseAction: QAction*        「日本語」メニュー項目
│   ├── m_englishAction: QAction*         「英語」メニュー項目
│   ├── m_simplifiedAction: QAction*      「简体中文」メニュー項目
│   ├── m_traditionalAction: QAction*     「繁體中文」メニュー項目
│   ├── m_notationGroup: QActionGroup*    棋譜表記（言語に合わせる・日本語表記・英語表記）の排他選択グループ
│   └── m_parentWidget: QWidget*          ダイアログ表示用の親ウィジェット
├── public メソッド
│   ├── setActions()                      言語のメニューアクションの登録
│   ├── setNotationActions()              棋譜表記のメニューアクションの登録
│   ├── setParentWidget()                 親ウィジェットの設定
│   └── updateMenuState()                 現在の設定でチェック状態を更新
├── public slots
│   ├── onSystemLanguageTriggered()       「システム言語」選択時
│   ├── onJapaneseTriggered()             「日本語」選択時
│   └── onEnglishTriggered()              「英語」選択時
├── private slots
│   ├── onChineseSimplifiedTriggered()    「简体中文」選択時
│   ├── onChineseTraditionalTriggered()   「繁體中文」選択時
│   ├── onNotationTriggered(QAction*)     棋譜表記の選択時
│   ├── onOriginTriggered(bool)           「英語表記で移動元を常に表示」の切り替え時
│   └── showNotationHelp()                棋譜表記の読み方を表示
└── private メソッド
    ├── changeLanguage(const QString&)    設定保存＋再起動の案内
    └── restartNotice()                   再起動の案内ダイアログ
```

#### 言語切替のシーケンス

```mermaid
sequenceDiagram
    participant User as ユーザー
    participant Menu as 言語メニュー
    participant LC as LanguageController
    participant SS as AppSettings
    participant MB as QMessageBox

    User->>Menu: 「日本語」を選択
    Menu->>LC: onJapaneseTriggered()
    LC->>LC: changeLanguage("ja_JP")
    LC->>SS: language() で現在値を確認
    SS-->>LC: "system"（変更前の値）
    LC->>SS: language.set("ja_JP")
    LC->>MB: information("再起動してください")（restartNotice）
    MB-->>User: ダイアログ表示
    LC->>LC: updateMenuState()
    LC->>Menu: 「日本語」にチェックマーク
```

重要な設計上のポイント:

- **再起動が必要**: 言語と棋譜表記の変更は即座には反映されない。翻訳と棋譜表記は起動時に `ApplicationTranslations` が決めるため、変更を反映するにはアプリケーションの再起動が必要
- **排他選択**: `QActionGroup` により、5つの言語オプション（システム/日本語/英語/简体中文/繁體中文）は常に1つだけが選択される
- **同一言語の再選択を無視**: `changeLanguage()` は現在の設定と同じ値が指定された場合、何もしない

#### MainWindowとの統合

LanguageControllerは `MainWindowFoundationRegistry::ensureLanguageController()` で生成される（起動の最終段で呼ばれる）。

```cpp
void MainWindowFoundationRegistry::ensureLanguageController()
{
    if (m_mw.m_languageController) return;

    m_mw.m_languageController = std::make_unique<LanguageController>();
    m_mw.m_languageController->setParentWidget(&m_mw);
    m_mw.m_languageController->setActions(
        m_mw.ui->actionLanguageSystem,
        m_mw.ui->actionLanguageJapanese,
        m_mw.ui->actionLanguageEnglish,
        m_mw.ui->actionLanguageChineseSimplified,
        m_mw.ui->actionLanguageChineseTraditional);
    m_mw.m_languageController->setNotationActions(
        m_mw.ui->actionNotationAuto, m_mw.ui->actionNotationJapanese,
        m_mw.ui->actionNotationWestern, m_mw.ui->actionNotationOrigin, m_mw.ui->actionNotationHelp);
}
```

`setActions()`・`setNotationActions()` の内部で各アクションの `triggered` シグナルと対応するスロットが接続されるため、呼び出し側での追加的な `connect()` は不要である。

### 16.6 AppSettingsによる言語設定の永続化

言語設定は `AppSettings::language`（`src/services/appsettings.h`）としてINIファイルに保存される。ほかの設定と同じく、宣言の表に1行で書いた `Setting<QString>` である（§12.5）。

```cpp
// appsettings.h
/// 言語コード（デフォルト: "system"）
inline const Setting<QString> language{"General/language", "system"};

// 読み書き
const QString current = AppSettings::language();  // LanguageController::changeLanguage()
AppSettings::language.set(lang);
```

起動時は `ApplicationTranslations`（`src/services/applicationtranslations.cpp`）がこの値を読み、`"system"` ならシステムのUI言語から解決して翻訳ファイルを選ぶ。

| 設定キー | 取りうる値 | デフォルト | 説明 |
|---------|-----------|-----------|------|
| `General/language` | `"system"`, `"ja_JP"`, `"en"`, `"zh_CN"`, `"zh_TW"` | `"system"` | 表示言語の設定 |

### 16.7 翻訳追加の手順

新しい翻訳可能文字列を追加してから翻訳が反映されるまでの具体的な手順を示す。

#### ステップ1: ソースコードで `tr()` を使用

```cpp
// 新しいダイアログメッセージを追加する例
QMessageBox::information(this,
    tr("エクスポート完了"),
    tr("棋譜のエクスポートが完了しました。"));
```

#### ステップ2: `lupdate` で翻訳ファイルを更新

```bash
cmake --build build --target update_translations
```

このコマンドにより、新しい `tr()` 文字列が4言語の `.ts` ファイルに `type="unfinished"` として追加される。既存の翻訳は保持される。

#### ステップ3: `.ts` ファイルを編集して翻訳を入力

日本語が原文の場合、英語・簡体字・繁体字（`ShogiBoardQ_en.ts`・`ShogiBoardQ_zh_CN.ts`・`ShogiBoardQ_zh_TW.ts`）に翻訳を追加し、`ShogiBoardQ_ja_JP.ts` には原文と同じ文字列を訳として入れる。英語の例:

```xml
<message>
    <source>エクスポート完了</source>
    <translation>Export Complete</translation>   <!-- type="unfinished" を削除 -->
</message>
<message>
    <source>棋譜のエクスポートが完了しました。</source>
    <translation>Kifu export has been completed.</translation>
</message>
```

翻訳を入力したら `type="unfinished"` 属性を削除する（または Qt Linguist で翻訳済みにマークする）。4言語とも未翻訳が0件になったことを確かめる。

```bash
grep -c 'type="unfinished"' resources/translations/*.ts
```

`%n` の複数形（`<numerusform>`）は使わない。`tst_translation_files` が訳を空と判定する。件数は `%2` などの通常の引数で渡す。

#### ステップ4: ビルドして .qm を生成

```bash
cmake --build build
```

CMakeの `qt_add_translation()` が `lrelease` を実行し、`.ts` → `.qm` の変換が行われる。

#### ステップ5: 動作確認

アプリケーションを起動し、言語メニューから対象言語を選択（→再起動）して翻訳が反映されていることを確認する。

### 16.8 `tr()` マクロの使い方

#### 基本的な使い方

```cpp
// QObject を継承するクラス内で使用
setWindowTitle(tr("対局設定"));

// 引数を含む文字列
statusBar()->showMessage(tr("手数: %1").arg(moveCount));
```

#### 注意事項

| 項目 | 説明 |
|------|------|
| QObject派生クラス内で使用 | `tr()` は `QObject::tr()` のマクロであり、QObject派生クラスのメンバ関数内で使用する |
| リテラル文字列のみ | `tr(variable)` はNG。`lupdate` は静的にソースを解析するため、リテラル文字列のみ抽出できる |
| `%1`, `%2` で引数 | `QString::arg()` と組み合わせて動的な値を埋め込む |
| `.ui` ファイル | Qt Designer で設定した文字列は自動的に `tr()` でラップされる |

<!-- chapter-16-end -->

---

<!-- chapter-17-start -->
## 第17章 新機能の追加ガイド

本章では、ShogiBoardQに新機能を追加する際の標準的な手順を、実践チュートリアル形式で解説する。仮の新機能「棋譜にタグを付ける機能」（棋譜ファイルに「急戦」「居飛車」等のタグを付与し、検索・フィルタリングを可能にする）を題材として、ファイル配置からビルド確認までの全工程を一通り示す。

### 17.1 ファイル配置先の選定

新しいクラスをどのディレクトリに配置するかは、そのクラスの責務で決まる。判断基準を以下にまとめる。

#### ディレクトリ選定フローチャート

```
新クラスの主な責務は？
│
├─ 将棋のルール・盤面ロジック ──→ src/core/
│
├─ 対局進行・ゲーム管理 ──→ src/game/
│
├─ 棋譜データ・フォーマット変換 ──→ src/kifu/
│    └─ フォーマット変換器 ──→ src/kifu/formats/
│
├─ エンジン通信・USIプロトコル ──→ src/engine/
│
├─ 解析（検討・棋譜解析・詰将棋） ──→ src/analysis/
│
├─ ネットワーク通信（CSA） ──→ src/network/
│
├─ ナビゲーション（棋譜移動・分岐） ──→ src/navigation/
│
├─ 盤面操作（編集・画像出力） ──→ src/board/
│
├─ UI関連
│    ├─ データ表示の同期 ──→ src/ui/presenters/
│    ├─ ユーザー操作のハンドリング ──→ src/ui/controllers/
│    ├─ シグナル/スロット接続 ──→ src/ui/wiring/
│    └─ 複数コンポーネントの調整 ──→ src/ui/coordinators/
│
├─ ウィジェット（再利用可能なUI部品） ──→ src/widgets/
│
├─ ダイアログ（モーダル/モードレス） ──→ src/dialogs/
│
├─ Qtモデル（QAbstractListModel等） ──→ src/models/
│
├─ 設定・永続化・横断サービス ──→ src/services/
│
├─ 自動化 API、CLI と共有する GUI 非依存のサービス ──→ src/automation/
│
├─ CLI（shogiboardq-cli）のコマンド ──→ src/cli/
│
└─ 共通ユーティリティ ──→ src/common/
```

#### 今回の例: 棋譜タグ機能

棋譜タグ機能は以下の3つのクラスで構成すると仮定する。

| クラス | 責務 | 配置先 |
|-------|------|--------|
| `KifuTagModel` | タグデータの管理（追加・削除・検索） | `src/models/` |
| `KifuTagWiring` | MainWindowとタグUIの接続 | `src/ui/wiring/` |
| `KifuTagDialog` | タグ編集ダイアログのUI | `src/dialogs/` |

> **ポイント**: ヘッダファイル（`.h`）は対応するソースファイル（`.cpp`）と同じディレクトリに配置する。ソースファイルは `CMakeLists.txt` に列挙しているので、配置先に対応する `SRC_*`（例: `src/models/` なら `SRC_MODELS`）に `.cpp` と `.h` を書き足す（[1.4節](#14-cmakeビルド構成の概要)）。

### 17.2 Deps構造体を持つ新クラスの作成

Wiringクラスは第4章で解説した「Deps構造体パターン」に従う。ここでは `KifuTagWiring` の実装例を示す。

#### ヘッダファイル

```cpp
// src/ui/wiring/kifutagwiring.h

#ifndef KIFUTAGWIRING_H
#define KIFUTAGWIRING_H

/// @file kifutagwiring.h
/// @brief 棋譜タグ機能の配線クラスの定義

#include <QObject>
#include <functional>

class QWidget;
class KifuTagModel;
class KifuTagDialog;

/**
 * @brief 棋譜タグ機能のUI配線を担当するクラス
 *
 * 責務:
 * - タグ編集ダイアログの表示制御
 * - タグモデルとUIの接続
 *
 * Deps構造体で必要なオブジェクトを受け取り、updateDeps()で再設定可能。
 */
class KifuTagWiring : public QObject
{
    Q_OBJECT

public:
    /// 依存オブジェクト
    struct Deps {
        QWidget* parentWidget = nullptr;       ///< 親ウィジェット（非所有）
        KifuTagModel* tagModel = nullptr;      ///< タグモデル（非所有）
        std::function<void()> ensureTagModel;   ///< 遅延初期化コールバック
    };

    explicit KifuTagWiring(const Deps& deps, QObject* parent = nullptr);
    ~KifuTagWiring() override = default;

    /// 依存オブジェクトを再設定する
    void updateDeps(const Deps& deps);

public slots:
    /// タグ編集ダイアログを表示する（actionEditTags に接続）
    void showTagEditDialog();

    /// タグが変更されたときの処理（KifuTagModel::tagsChanged に接続）
    void onTagsChanged();

signals:
    /// タグ変更の保存を要求する（→ KifuFileController::overwriteKifuFile）
    void saveRequested();

private:
    QWidget* m_parentWidget = nullptr;
    KifuTagModel* m_tagModel = nullptr;
    std::function<void()> m_ensureTagModel;
};

#endif // KIFUTAGWIRING_H
```

#### ソースファイル

```cpp
// src/ui/wiring/kifutagwiring.cpp

/// @file kifutagwiring.cpp
/// @brief 棋譜タグ機能の配線クラスの実装

#include "kifutagwiring.h"

#include "kifutagmodel.h"
#include "kifutagdialog.h"

// ============================================================
// 初期化
// ============================================================

KifuTagWiring::KifuTagWiring(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_parentWidget(deps.parentWidget)
    , m_tagModel(deps.tagModel)
    , m_ensureTagModel(deps.ensureTagModel)
{
}

void KifuTagWiring::updateDeps(const Deps& deps)
{
    m_parentWidget = deps.parentWidget;
    m_tagModel = deps.tagModel;
    if (deps.ensureTagModel) {
        m_ensureTagModel = deps.ensureTagModel;
    }
}

// ============================================================
// スロット
// ============================================================

void KifuTagWiring::showTagEditDialog()
{
    // タグモデルが未初期化の場合、コールバックで遅延初期化を実行
    if (!m_tagModel && m_ensureTagModel) {
        m_ensureTagModel();
    }
    if (!m_tagModel) return;

    KifuTagDialog dialog(m_tagModel, m_parentWidget);
    if (dialog.exec() == QDialog::Accepted) {
        emit saveRequested();
    }
}

void KifuTagWiring::onTagsChanged()
{
    // タグ変更時の処理（例: ステータスバー更新、棋譜モデルへの反映）
}
```

**テンプレートの要点**:

| 要素 | ルール |
|------|--------|
| **Deps のフィールド** | 全て `nullptr` または適切なデフォルト値で初期化 |
| **コンストラクタ** | 初期化リストで `Deps` の値をメンバーにコピー |
| **updateDeps()** | 全フィールドを上書き。`std::function` は有効な場合のみ更新 |
| **ファイルヘッダー** | `@file` と `@brief` の2行（commenting-style-guide.md §2） |
| **クラスコメント** | `@brief` + 責務の箇条書き（commenting-style-guide.md §3） |

### 17.3 レジストリにensure*()メソッドを追加

部品の生成と依存の設定は、MainWindow ではなくレジストリで行う。MainWindow には `ensure*()` を置かない（`tst_structural_kpi` の `mainWindowEnsureLimit` が 0 個であることを確かめる）。どのレジストリに足すかは [レジストリ境界仕様](registry-boundary.md) に従う。棋譜タグは棋譜系なので `KifuSubRegistry` に足す。

#### 1. mainwindow.h にメンバーを宣言

```cpp
// src/app/mainwindow.h に追加（レジストリは MainWindow の friend なので private に置く）

private:
    // --- 棋譜タグ ---
    KifuTagWiring* m_kifuTagWiring = nullptr;
    KifuTagModel* m_kifuTagModel = nullptr;
```

> **ポイント**: 前方宣言 `class KifuTagWiring;` をヘッダ冒頭の前方宣言セクションに追加する。`#include` ではなく前方宣言を使うことで、ヘッダの依存関係を最小化する。

#### 2. レジストリに ensure*() を宣言・実装

依存の更新が要る部品は、生成（`create*`）と依存の更新（`refresh*Deps`）に分ける（[レジストリ境界仕様](registry-boundary.md) の「ensure* の2段構造パターン」）。

```cpp
// src/app/kifusubregistry.h に追加
public:
    void ensureKifuTagWiring();
    void refreshKifuTagWiringDeps();
private:
    void createKifuTagWiring();
```

```cpp
// src/app/kifusubregistry.cpp に追加

void KifuSubRegistry::ensureKifuTagWiring()
{
    if (!m_mw.m_kifuTagWiring) {
        createKifuTagWiring();
    }
    refreshKifuTagWiringDeps();   // 生成済みでも最新の部品を渡し直す
}

void KifuSubRegistry::createKifuTagWiring()
{
    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_kifuTagWiring = new KifuTagWiring(KifuTagWiring::Deps{}, &m_mw);

    // 配線は作ったときに1回だけ行う（§17.4）
    connect(m_mw.ui->actionEditTags, &QAction::triggered,
            m_mw.m_kifuTagWiring, &KifuTagWiring::showTagEditDialog);
}

void KifuSubRegistry::refreshKifuTagWiringDeps()
{
    if (!m_mw.m_kifuTagWiring) return;

    KifuTagWiring::Deps deps;
    deps.parentWidget = &m_mw;
    deps.tagModel = m_mw.m_kifuTagModel;
    deps.ensureTagModel = [this]() {
        if (!m_mw.m_kifuTagModel) {
            // Lifetime: owned by MainWindow (QObject parent=&m_mw)
            m_mw.m_kifuTagModel = new KifuTagModel(&m_mw);
        }
        refreshKifuTagWiringDeps();   // 作ったモデルを渡し直す
    };
    m_mw.m_kifuTagWiring->updateDeps(deps);
}
```

`ensure*()` の3つの処理:

| 処理 | 内容 | 対応するコード |
|------|------|-------------|
| ① 生成（初回だけ） | MainWindow を親にして作る | `createKifuTagWiring()` |
| ② 配線（初回だけ） | 作った直後にシグナルをつなぐ | `createKifuTagWiring()` の `connect()` |
| ③ 依存の更新（毎回） | `Deps` を作り直して渡す | `refreshKifuTagWiringDeps()` |

レジストリごとの `ensure*()` の数には上限がある（`tst_structural_kpi` の `serviceRegistryEnsureLimit`・`subRegistryEnsureLimits`）。上限に達しているときは、既存の `ensure*()` をまとめられないか先に検討し、上限を上げる場合はその理由をレビューで説明する（[レジストリ境界仕様](registry-boundary.md)）。`refresh*Deps()` は数えない。

#### 3. 呼び出し箇所の追加

`ensure*()` は必要になった時点で呼ぶ。メニューアクションのように起動直後から使える必要があるものは、起動の最終段（`MainWindowServiceRegistry::finalizeAndConfigureUiForLifecycle()`）で呼ぶ。

```cpp
// src/app/mainwindowlifecyclewiring.cpp
void MainWindowServiceRegistry::finalizeAndConfigureUiForLifecycle()
{
    // ...
    m_kifu->ensureKifuTagWiring();
}
```

### 17.4 Wiringクラスでシグナル/スロット接続

シグナル/スロット接続は **`connect()` のメンバー関数ポインタ構文** を使用する。ラムダ式は禁止である（CLAUDE.md の Code Style 参照）。

#### 接続コードの例

```cpp
// KifuSubRegistry::createKifuTagWiring() 内（作ったときに1回だけ）

// メニューアクション → KifuTagWiring のスロット
connect(m_mw.ui->actionEditTags, &QAction::triggered,
        m_mw.m_kifuTagWiring, &KifuTagWiring::showTagEditDialog);

// KifuTagWiring のシグナル → 棋譜ファイル操作のスロット
ensureKifuFileController();
connect(m_mw.m_kifuTagWiring, &KifuTagWiring::saveRequested,
        m_mw.m_kifuFileController, &KifuFileController::overwriteKifuFile);
```

#### 接続時の注意事項

| ルール | 理由 | 例 |
|--------|------|-----|
| ラムダ禁止 | 接続の追跡を困難にするため | `connect(a, &A::sig, [this](){...})` は使わない |
| `Qt::UniqueConnection` | 重複接続を防ぐ（`wire()` で一括接続する場合） | `connect(..., Qt::UniqueConnection)` |
| 接続先のコメント | コードを追わずに接続関係を把握できるようにする | `/// タグ変更の保存を要求（→ MainWindow）` |

#### Wiringクラス内の内部接続

Wiringクラスが内部にコントローラを持つ場合、`ensure*()` 内でコントローラのシグナルを接続する。

```cpp
void KifuTagWiring::ensureTagController()
{
    if (m_tagController) return;

    m_tagController = new KifuTagController(this);

    // コントローラからのシグナルをWiringのシグナルに中継
    connect(m_tagController, &KifuTagController::tagAdded,
            this, &KifuTagWiring::onTagsChanged);
}
```

### 17.5 設定の宣言の表に設定項目を追加

ダイアログの状態（ウィンドウサイズ、フォントサイズ等）を永続化する場合は、ドメイン別ヘッダーの宣言の表に `Setting<T>` を1行ずつ足す（§12.5、[settings-service.md](settings-service.md)）。getter と setter の組やキー定数は書かない。

#### 1. ドメイン別ヘッダーに宣言を追加

棋譜タグは対局・棋譜の設定なので `gamesettings.h` に置く。

```cpp
// src/services/gamesettings.h — namespace GameSettings 内に追加

/// 棋譜タグダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> kifuTagDialogFontSize{"FontSize/kifuTag", 10};

/// 棋譜タグダイアログのウィンドウサイズ（デフォルト: 400x300）
inline const Setting<QSize> kifuTagDialogSize{"SizeRelated/kifuTagDialogSize", QSize(400, 300)};
```

宣言は `inline const` なので `.cpp` への実装は要らない。

#### 2. ダイアログ側で読み込み・保存

```cpp
// src/dialogs/kifutagdialog.cpp

KifuTagDialog::KifuTagDialog(KifuTagModel* model, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_fontHelper(GameSettings::kifuTagDialogFontSize, 8, 24, 1)  // 初期値を読み、変更のたびに保存する
{
    // 設定の読み込み
    DialogUtils::restoreDialogSize(this, GameSettings::kifuTagDialogSize);
    applyFontSize();  // m_fontHelper.fontSize() を画面に反映する
}

void KifuTagDialog::closeEvent(QCloseEvent* event)
{
    // 設定の保存（フォントサイズは FontSizeHelper が保存済み）
    DialogUtils::saveDialogSize(this, GameSettings::kifuTagDialogSize);
    QDialog::closeEvent(event);
}
```

A-/A+ ボタンのスロットでは `m_fontHelper.increase()` / `decrease()` が `true` を返したときだけ `applyFontSize()` を呼ぶ（`KifuPasteDialog` と同じ形）。

#### 設定項目を追加するときの規約

| 項目 | 規約 |
|------|------|
| **置き場所** | 機能に合うドメイン別ヘッダー（`appsettings.h`・`gamesettings.h`・`analysissettings.h`・`networksettings.h`・`josekisettings.h`・`tsumeshogisettings.h`・`enginedialogsettings.h`・`docksettings.h`） |
| **宣言** | `inline const Setting<T> name{"Group/key", 既定値};` を1行。ドキュメントコメントに既定値を書く |
| **INIキー命名** | `グループ名/キー名` 形式（例: `FontSize/kifuTag`）。キーを変えると保存済みの値は読まれなくなる |
| **読み書き** | `NS::name()` / `NS::name.set(v)` / `NS::name.defaultValue()`。既定値が翻訳文や実行時の場所で決まる項目は `NS::name.valueOr(...)` |
| **関数にする項目** | 値の範囲を絞る、複数のキーを構造体にまとめる、配列で保存する、古い保存値を引き継ぐ、といった処理が要るものだけ。`SettingsCommon::openSettings()` を使い、キーは `settingskeys.h` に置く |
| **QSettings の直接構築** | `beginReadArray` / `beginGroup` でグループを開いたまま読み書きする配列データだけ（`QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat)`） |
| **永続化対象の判断** | ウィンドウサイズ、フォントサイズ、列幅、最後の選択状態など |

### 17.6 tr()による翻訳対応

ユーザーに表示する文字列は全て `tr()` でラップし、翻訳可能にする。

#### tr() の使い方

```cpp
// 基本形: QObject 派生クラス内
setWindowTitle(tr("タグ編集"));

// パラメータ付き
statusBar()->showMessage(tr("%1 件のタグを追加しました").arg(count));

// コンテキスト付き（同じ文字列でも異なるクラスで別の翻訳が必要な場合）
// tr() は自動的にクラス名をコンテキストとして使用する

// QObject 非派生クラスの場合
QCoreApplication::translate("KifuTagDialog", "タグ編集");
```

#### 翻訳で避けるべきパターン

```cpp
// 悪い例: 文字列の連結
label->setText(tr("合計") + ": " + QString::number(count) + tr("件"));

// 良い例: %1 プレースホルダを使う
label->setText(tr("合計: %1 件").arg(count));

// 悪い例: 翻訳不要な文字列を tr() で囲む
qDebug() << tr("Debug message");   // デバッグログは翻訳不要

// 良い例: tr() を付けない
qDebug() << "Debug message";
```

#### lupdate の実行

`tr()` でラップした文字列を追加・変更したら、翻訳ファイルを更新する。

```bash
lupdate src -ts resources/translations/ShogiBoardQ_ja_JP.ts resources/translations/ShogiBoardQ_en.ts
```

#### .ts ファイルの編集

`lupdate` 実行後、`.ts` ファイル内の `type="unfinished"` エントリに翻訳を追加する。

```xml
<!-- resources/translations/ShogiBoardQ_en.ts に追加されるエントリ例 -->
<context>
    <name>KifuTagDialog</name>
    <message>
        <source>タグ編集</source>
        <translation type="unfinished"></translation>  <!-- ← 翻訳を追加 -->
    </message>
</context>
```

翻訳を追加した後:

```xml
<message>
    <source>タグ編集</source>
    <translation>Edit Tags</translation>  <!-- type属性を削除 -->
</message>
```

> **参考**: 翻訳ワークフローの詳細は [第16章](#第16章-国際化i18nと翻訳) を参照。

### 17.7 コメントの記述

[commenting-style-guide.md](commenting-style-guide.md) に従ってコメントを記述する。ここでは新機能追加時に必要なコメントの種類と具体例をまとめる。

#### ファイルヘッダー（§2）

全ての `.h` / `.cpp` ファイルに `@file` と `@brief` を付ける。

```cpp
// .h ファイル
/// @file kifutagwiring.h
/// @brief 棋譜タグ機能の配線クラスの定義

// .cpp ファイル
/// @file kifutagwiring.cpp
/// @brief 棋譜タグ機能の配線クラスの実装
```

#### クラスコメント（§3）

クラス宣言の直前に `@brief` + 責務を記述する。Deps構造体を持つクラスでは、Depsパターンの使用を明記する。

```cpp
/**
 * @brief 棋譜タグ機能のUI配線を担当するクラス
 *
 * 責務:
 * - タグ編集ダイアログの表示制御
 * - タグモデルとUIの接続
 *
 * Deps構造体で必要なオブジェクトを受け取り、updateDeps()で再設定可能。
 */
class KifuTagWiring : public QObject
```

#### シグナル・スロットコメント（§5）

接続元または接続先を明示する。

```cpp
signals:
    /// タグ変更の保存を要求する（→ KifuFileController::overwriteKifuFile）
    void saveRequested();

public slots:
    /// タグ編集ダイアログを表示する（actionEditTags に接続）
    void showTagEditDialog();
```

#### メンバー変数（§6）

`///<` で行末コメントを付ける。ポインタは所有権を明記する。

```cpp
private:
    QWidget* m_parentWidget = nullptr;     ///< 親ウィジェット（非所有）
    KifuTagModel* m_tagModel = nullptr;    ///< タグモデル（非所有）
    std::function<void()> m_ensureTagModel; ///< タグモデル遅延初期化コールバック
```

#### .cpp 内のインラインコメント（§9）

**Why（なぜ）** を書く。コードの繰り返しは書かない。

```cpp
// 良い例: なぜそうするかを説明
// タグモデルが未初期化の場合、コールバックで遅延初期化を実行
if (!m_tagModel && m_ensureTagModel) {
    m_ensureTagModel();
}

// 悪い例: コードの繰り返し
// タグモデルがnullかチェックする  ← 見れば分かる
if (!m_tagModel) return;
```

#### セクション区切り（§8）

```cpp
// .h 内
public:
    // --- 初期化 ---
    void updateDeps(const Deps& deps);

    // --- 操作 ---
    void showTagEditDialog();

// .cpp 内
// ============================================================
// 初期化
// ============================================================

KifuTagWiring::KifuTagWiring(const Deps& deps, QObject* parent)
```

### 17.8 チェックリスト

新機能の追加が完了したら、以下の項目を確認する。

#### コーディング規約チェック

| # | 確認項目 | 参照 |
|---|---------|------|
| 1 | `connect()` でラムダ式を使っていないか | CLAUDE.md Code Style |
| 2 | `std::as_const()` でQt コンテナの detach を防いでいるか | CLAUDE.md Code Style |
| 3 | コンパイラ警告が出ていないか（`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`） | CLAUDE.md Code Style |
| 4 | `tr()` でユーザー向け文字列をラップしているか | 第16章、§17.6 |
| 5 | commenting-style-guide.md に従ったコメントを書いたか | §17.7 |

#### 設計パターンチェック

| # | 確認項目 | 参照 |
|---|---------|------|
| 6 | `Deps` 構造体で依存を注入しているか | 第4章 §4.1 |
| 7 | `ensure*()` メソッドにガードを入れているか | 第4章 §4.2 |
| 8 | 遅延初期化の依存ギャップに対処しているか（`std::function` コールバック） | 第4章 §4.2 |
| 9 | MainWindowへのロジック追加を最小限にしているか | CLAUDE.md Design Principles |
| 10 | ポインタの所有権をコメントで明記しているか | commenting-style-guide.md §6 |

#### 設定の永続化チェック

| # | 確認項目 | 参照 |
|---|---------|------|
| 11 | ダイアログのウィンドウサイズを保存しているか（`DialogUtils::restoreDialogSize` / `saveDialogSize`） | CLAUDE.md Settings Update Guidelines |
| 12 | フォントサイズ調整機能がある場合、サイズを保存しているか（`FontSizeHelper` に宣言を渡す） | CLAUDE.md Settings Update Guidelines |
| 13 | テーブル/リストの列幅を保存しているか（該当する場合） | CLAUDE.md Settings Update Guidelines |
| 14 | コンボボックス等の選択状態を保存しているか（該当する場合） | CLAUDE.md Settings Update Guidelines |

どの項目も、getter と setter の組ではなく `Setting<T>` の宣言1行で足す。関数にするのは処理が要る項目だけ（§17.5）。

#### 翻訳チェック

| # | 確認項目 | 参照 |
|---|---------|------|
| 15 | `update_translations` ターゲット（`lupdate`）を実行して `.ts` ファイルを更新したか | CLAUDE.md Translation Guidelines |
| 16 | 4言語の `.ts` ファイルの `type="unfinished"` エントリに翻訳を追加し、未翻訳が0件か | 第16章 §16.2、§16.7 |
| 17 | デバッグログやINIキー等の翻訳不要な文字列に `tr()` を付けていないか | §17.6 |

#### ビルド・動作確認チェック

| # | 確認項目 | 手順 |
|---|---------|------|
| 18 | ビルドが通るか | `cmake --build build` |
| 19 | clang-tidy の警告が出ていないか | `cmake -B build -DENABLE_CLANG_TIDY=ON && cmake --build build` |
| 20 | 追加した機能が正常に動作するか | `./build/ShogiBoardQ` で手動確認 |

#### 全手順のまとめ

```
新機能の追加手順

① ファイル配置先を決定（§17.1 のフローチャート参照）
     │
② ヘッダ / ソースファイルを作成（§17.2）
   ├─ Deps 構造体を定義
   ├─ コンストラクタ / updateDeps() を実装
   └─ スロットメソッドを実装
     │
③ レジストリに ensure*() メソッドを追加（§17.3）
   ├─ mainwindow.h にメンバーを宣言
   ├─ 担当のレジストリに ensure*() / create*() / refresh*Deps() を実装
   └─ CMakeLists.txt の SRC_* に新しいファイルを列挙（§1.4）
     │
④ シグナル / スロット接続を追加（§17.4）
   └─ connect() はメンバー関数ポインタ構文のみ
     │
⑤ 設定の宣言の表に設定項目を追加（§17.5）
   ├─ ドメイン別ヘッダーに Setting<T> を1行ずつ宣言
   └─ ダイアログで読み込み / 保存（FontSizeHelper・DialogUtils に宣言を渡す）
     │
⑥ tr() で翻訳対応（§17.6）
   ├─ ユーザー向け文字列を tr() でラップ
   ├─ lupdate を実行
   └─ .ts ファイルに翻訳を追加
     │
⑦ コメントを記述（§17.7）
   └─ commenting-style-guide.md に準拠
     │
⑧ チェックリストで最終確認（§17.8）
   └─ ビルド → clang-tidy → 動作確認
```

<!-- chapter-17-end -->

---

<!-- chapter-18-start -->
## 第18章 用語集・索引

本章は、ShogiBoardQの開発において頻出する将棋用語・技術略語を整理し、全クラスとファイルの逆引き索引を提供する。

---

### 18.1 将棋用語 日英対応表

| 日本語 | 英語 | SFEN/USI表記 | 補足 |
|--------|------|-------------|------|
| 先手（せんて） | Black / First player | `b` | SFEN手番表記。先に指す側 |
| 後手（ごて） | White / Second player | `w` | SFEN手番表記 |
| 玉将（ぎょくしょう） | King | `K` / `k` | 先手=`K`、後手=`k` |
| 飛車（ひしゃ） | Rook | `R` / `r` | 成ると竜王（`+R`） |
| 角行（かくぎょう） | Bishop | `B` / `b` | 成ると竜馬（`+B`） |
| 金将（きんしょう） | Gold | `G` / `g` | 成りなし |
| 銀将（ぎんしょう） | Silver | `S` / `s` | 成ると成銀（`+S`） |
| 桂馬（けいま） | Knight | `N` / `n` | 成ると成桂（`+N`） |
| 香車（きょうしゃ） | Lance | `L` / `l` | 成ると成香（`+L`） |
| 歩兵（ふひょう） | Pawn | `P` / `p` | 成ると「と金」（`+P`） |
| 成る（なる） | Promote | `+`（USI末尾） | 例: `7c7b+` |
| 不成（ならず） | Non-promote | （`+`なし） | 例: `7c7b` |
| 持ち駒（もちごま） | Hand pieces / Pieces in hand | SFEN第3フィールド | 例: `S2Pb2p` |
| 王手（おうて） | Check | — | 玉が取られる状態 |
| 詰み（つみ） | Checkmate | `mate` | USIの`score mate N` |
| 千日手（せんにちて） | Repetition | — | 同一局面4回で引き分け |
| 入玉（にゅうぎょく） | Entering King | — | 玉が敵陣に入ること |
| 持将棋（じしょうぎ） | Impasse / Jishogi | — | 両玉入玉で点数判定 |
| 投了（とうりょう） | Resignation | — | 負けを宣言 |
| 中断（ちゅうだん） | Adjournment / Break | — | 対局の一時停止 |
| 筋（すじ） | File | 1〜9（右→左） | 縦の列。9が左端 |
| 段（だん） | Rank | 1〜9 / a〜i | 横の行。1（a）が先手側 |
| 棋譜（きふ） | Kifu / Game record | — | 指し手の記録 |
| 定跡（じょうせき） | Joseki / Opening book | — | 定番の序盤手順 |
| 検討（けんとう） | Consideration / Analysis | — | エンジンによる局面分析 |
| 読み筋（よみすじ） | Principal Variation (PV) | `pv`（USI info） | エンジンの最善手順 |
| 評価値（ひょうかち） | Evaluation score | `score cp N` | センチポーン単位 |
| 秒読み（びょうよみ） | Byoyomi | `byoyomi` | 持ち時間後の秒数制限 |
| 手数（てすう） | Move number / Ply | SFEN第4フィールド | 何手目かの番号 |
| 駒台（こまだい） | Piece stand | — | 持ち駒の置き場 |
| 打つ（うつ） | Drop | `P*3d`（USI） | 持ち駒を盤上に置く |

---

### 18.2 略語一覧

| 略語 | 正式名称 | 意味 | 参照章 |
|------|---------|------|--------|
| **SFEN** | Shogi Forsyth-Edwards Notation | 局面を1行テキストで表現する記法。FEN（チェス）の将棋版 | 第2章 §2.2 |
| **USI** | Universal Shogi Interface | 将棋エンジンとGUI間の通信プロトコル。UCI（チェス）の将棋版 | 第2章 §2.3, 第7章 |
| **CSA** | Computer Shogi Association | コンピュータ将棋協会。ネットワーク対局プロトコルおよび棋譜形式の名称 | 第2章 §2.5, 第12章 |
| **PV** | Principal Variation | エンジンが示す最善手順（読み筋） | 第7章 §7.4, 第9章 |
| **MultiPV** | Multiple Principal Variations | 複数の候補手を同時に表示する機能 | 第9章 §9.2 |
| **KIF** | 棋譜ファイル（Kifu File） | 日本語表記の棋譜形式。柿木将棋が起源 | 第2章 §2.5 |
| **KI2** | 棋譜ファイル2（Kifu File 2） | KIFの改良版。座標を省略し日本語のみで表記 | 第2章 §2.5 |
| **JKF** | JSON Kifu Format | JSON形式の棋譜フォーマット | 第2章 §2.5 |
| **USEN** | URL-Safe Encoded Notation | URL安全な棋譜エンコード形式 | 第2章 §2.5 |
| **MVP** | Model-View-Presenter | UI設計パターン。Model→Viewの同期をPresenterが担当 | 第4章 §4.4 |
| **Deps** | Dependencies（依存構造体） | 依存注入用のPOD構造体パターン | 第4章 §4.1 |
| **MOC** | Meta-Object Compiler | Qtのメタオブジェクトコンパイラ。シグナル/スロットを処理 | 第4章 |
| **INI** | Initialization File | Windows形式の設定ファイル。`Setting<T>` の宣言の表で読み書きする | 第12章 §12.5 |

---

### 18.3 クラス名逆引き一覧

全主要クラス・名前空間をアルファベット順に掲載する。「種別」列の凡例: C=クラス, NS=名前空間。

| 名前 | 種別 | ディレクトリ | 説明 | 解説章 |
|------|------|-------------|------|--------|
| AboutCoordinator | NS | ui/coordinators | バージョン情報ダイアログの表示 | 第10章 |
| AbstractListModel | C | models | ポインタ所有リストモデルのテンプレート基底 | 第11章 |
| AnalysisCoordinator | C | analysis | 局面解析の低レベル制御（Idle/SinglePosition/RangePositions） | 第9章 |
| AnalysisFlowController | C | analysis | 棋譜解析フローの制御 | 第9章 |
| AnalysisResultsPresenter | C | analysis | 解析結果の表示プレゼンター | 第9章 |
| AnalysisTabWiring | C | ui/wiring | 解析タブのシグナル/スロット配線 | 第10章 |
| AppToolTipFilter | C | widgets | アプリ全体のツールチップフィルタ | 第11章 |
| BoardImageExporter | C | board | 盤面画像のクリップボードコピー・ファイル保存 | 第13章 |
| BoardInteractionController | C | board | 盤面クリック・ドラッグ操作とハイライト制御 | 第13章 |
| BoardSetupController | C | ui/controllers | 盤面初期配置のセットアップ | 第10章 |
| BoardSyncPresenter | C | ui/presenters | 盤面状態の同期プレゼンター | 第10章 |
| ChangeEngineSettingsDialog | C | dialogs | エンジン設定変更ダイアログ | 第11章 |
| CollapsibleGroupBox | C | widgets | 折りたたみ可能なグループボックス | 第11章 |
| CommentCoordinator | C | app | コメント編集の統合調整 | 第14章 |
| ConsecutiveGamesController | C | game | 連続対局の進行管理 | 第6章 |
| ConsiderationFlowController | C | analysis | 検討モードのフロー制御 | 第9章 |
| ConsiderationModeUIController | C | analysis | 検討モードUI状態の制御 | 第9章 |
| ConsiderationWiring | C | ui/wiring | 検討モードのシグナル/スロット配線 | 第10章 |
| CsaClient | C | network | CSAプロトコル通信クライアント | 第12章 |
| CsaGameCoordinator | C | network | CSAネットワーク対局の統合調整 | 第12章 |
| CsaGameDialog | C | dialogs | CSA対局接続ダイアログ | 第11章 |
| CsaGameWiring | C | ui/wiring | CSA対局のシグナル/スロット配線 | 第10章 |
| CsaToSfenConverter | C | kifu/formats | CSA形式→SFEN変換器 | 第8章 |
| CsaWaitingDialog | C | dialogs | CSA接続待機ダイアログ | 第11章 |
| DialogCoordinator | C | ui/coordinators | ダイアログ生成・管理の統合調整 | 第10章 |
| DockCreationService | C | app | ドックウィジェット生成サービス | 第14章 |
| DockLayoutManager | C | ui/coordinators | ドックレイアウト状態の管理 | 第10章 |
| ElideLabel | C | widgets | テキスト省略表示ラベル | 第11章 |
| EngineAnalysisTab | C | widgets | エンジン解析結果タブウィジェット | 第11章 |
| EngineInfoWidget | C | widgets | エンジン情報表示ウィジェット | 第11章 |
| EngineMoveValidator | C | core | 合法手判定・合法手の一覧 | 第2章, 第5章 |
| EngineOptionDescriptions | C | engine | USIエンジンオプション説明データベース | 第7章 |
| EngineProcessManager | C | engine | エンジンプロセスのライフサイクル管理 | 第7章 |
| EngineRegistrationDialog | C | dialogs | エンジン登録・設定ダイアログ | 第11章 |
| EngineSettingsConstants | NS | engine | エンジン設定の定数定義 | 第7章 |
| EngineSettingsCoordinator | NS | engine | エンジン設定の統合調整 | 第7章 |
| ErrorBus | C | common | アプリ全体のエラー通知イベントバス（シングルトン） | 第4章, 第12章 |
| EvalGraphPresenter | NS | ui/presenters | 評価値グラフの表示フック | 第10章 |
| EvaluationChartWidget | C | widgets | 評価値グラフチャートウィジェット | 第11章 |
| EvaluationGraphController | C | ui/controllers | 評価値グラフの表示制御 | 第10章 |
| FlowLayout | C | widgets | フロー（折り返し）レイアウトマネージャ | 第11章 |
| GameInfoPaneController | C | widgets | 対局情報ペインの制御 | 第11章 |
| GameRecordModel | C | kifu | 棋譜データモデル（コメントのSingle Source of Truth） | 第8章 |
| GameRecordPresenter | C | ui/presenters | 棋譜表示プレゼンター | 第10章 |
| GameStartCoordinator | C | game | 対局開始フローの統合調整 | 第6章 |
| GameStateController | C | game | 対局状態・終局処理の管理 | 第6章 |
| GlobalToolTip | C | widgets | グローバルツールチップ表示ウィジェット | 第11章 |
| JishogiCalculator | C | common | 持将棋の点数計算・判定 | 第12章 |
| JishogiScoreDialogController | C | ui/controllers | 持将棋点数ダイアログの制御 | 第10章 |
| JkfToSfenConverter | C | kifu/formats | JKF形式→SFEN変換器 | 第8章 |
| JosekiMergeDialog | C | dialogs | 定跡結合ダイアログ | 第11章 |
| JosekiMoveDialog | C | dialogs | 定跡手入力ダイアログ | 第11章 |
| JosekiWindow | C | dialogs | 定跡管理ウィンドウ | 第11章 |
| JosekiWindowWiring | C | ui/wiring | 定跡ウィンドウのシグナル/スロット配線 | 第10章 |
| Ki2ToSfenConverter | C | kifu/formats | KI2形式→SFEN変換器 | 第8章 |
| KifMoveText | NS | core | KIF 形式の指し手の文字列（ヘッダーオンリー） | 第5章 |
| KifReader | NS | kifu | KIF形式ファイルの読み込み | 第8章 |
| KifToSfenConverter | C | kifu/formats | KIF形式→SFEN変換器 | 第8章 |
| KifuAnalysisDialog | C | dialogs | 棋譜解析設定ダイアログ | 第11章 |
| KifuAnalysisListModel | C | analysis | 棋譜解析結果リストモデル（8列テーブル） | 第9章 |
| KifuAnalysisResultsDisplay | C | widgets | 棋譜解析結果表示ウィジェット | 第9章, 第11章 |
| KifuBranchDisplay | C | widgets | 分岐候補手表示ウィジェット | 第11章 |
| KifuBranchListModel | C | models | 分岐候補リストモデル | 第11章 |
| KifuBranchNode | C | kifu | 分岐ツリーのノード構造体 | 第8章 |
| KifuBranchTree | C | kifu | 分岐ツリーデータモデル | 第8章 |
| KifuBranchTreeBuilder | C | kifu | 分岐ツリー構築ビルダー | 第8章 |
| KifuClipboardService | NS | kifu | クリップボード棋譜操作（形式自動検出） | 第8章 |
| KifuContentBuilder | C | kifu | 棋譜コンテンツ文字列の構築 | 第8章 |
| KifuDisplay | C | widgets | 棋譜指し手表示ウィジェット | 第11章 |
| KifuDisplayCoordinator | C | ui/coordinators | 棋譜表示の統合調整 | 第10章 |
| KifuExportController | C | kifu | 棋譜エクスポートの制御 | 第8章 |
| KifuIoService | NS | kifu | 棋譜ファイルI/Oサービス | 第8章 |
| KifuLoadCoordinator | C | kifu | 棋譜ファイル読み込みの統合調整（形式自動検出） | 第8章 |
| KifuNavigationController | C | navigation | 棋譜ナビゲーション制御 | 第13章 |
| KifuNavigationState | C | kifu | 棋譜ナビゲーション状態の管理 | 第8章, 第13章 |
| KifuPasteDialog | C | dialogs | 棋譜ペーストダイアログ | 第11章 |
| KifuRecordListModel | C | models | 棋譜レコードリストモデル | 第11章 |
| KifuSaveCoordinator | NS | kifu | 棋譜ファイル保存の統合調整 | 第8章 |
| KifuSubRegistry | C | app | 棋譜系（読み込み・保存・分岐ナビ・定跡）の ensure* | 第8章, 第14章 |
| LanguageController | C | ui/controllers | アプリ言語切替の制御 | 第10章, 第16章 |
| LiveGameSession | C | kifu | 対局中の棋譜一時記録セッション | 第8章 |
| LongLongSpinBox | C | widgets | 64ビット整数スピンボックス | 第11章 |
| MainWindow | C | app | メインウィンドウ（ファサード/ハブ） | 第3章, 第10章, 第14章 |
| MainWindowFoundationRegistry | C | app | 複数の領域が使う部品の ensure* | 第14章 |
| MainWindowServiceRegistry | C | app | ensure*（部品の生成と依存更新）・起動手順・主要操作の集約 | 第4章, 第14章 |
| MatchCoordinator | C | game | 対局進行の司令塔（マスターコントローラ） | 第6章 |
| MatchCoordinatorWiring | C | ui/wiring | 司令塔の生成（一度だけ）と配線、GameStartCoordinator の生成 | 第6章, 第14章 |
| MenuButtonWidget | C | widgets | メニューボタンウィジェット | 第11章 |
| MenuWindow | C | dialogs | 対局開始メニューウィンドウ | 第11章 |
| MenuWindowWiring | C | ui/wiring | メニューウィンドウのシグナル/スロット配線 | 第10章 |
| NumericRightAlignCommaDelegate | C | widgets | 数値右寄せカンマ区切りデリゲート | 第11章 |
| NyugyokuDeclarationHandler | C | ui/controllers | 入玉宣言の処理ハンドラ | 第10章 |
| PlayerInfoController | C | ui/controllers | プレイヤー情報表示の制御 | 第10章 |
| PlayerInfoWiring | C | ui/wiring | プレイヤー情報のシグナル/スロット配線 | 第10章 |
| PlayerNameService | C | services | プレイヤー名解決サービス | 第12章 |
| PositionEditController | C | board | 局面編集モードの制御 | 第13章 |
| PositionEditCoordinator | C | ui/coordinators | 局面編集の統合調整 | 第10章 |
| PreStartCleanupHandler | C | game | 対局開始前クリーンアップ処理 | 第6章 |
| PromoteDialog | C | dialogs | 成/不成確認ダイアログ | 第11章 |
| PromotionFlow | C | game | 成/不成選択フロー | 第6章 |
| PvBoardDialog | C | dialogs | 読み筋盤面表示ダイアログ | 第11章 |
| PvClickController | C | ui/controllers | 読み筋クリックの制御 | 第10章 |
| RecordLine | C | kifu | 分岐ツリーの1本の手順の値（局面列・指し手・表記・表示中の手数） | 第8章 |
| RecordNavigationHandler | C | navigation | 棋譜ペイン行変更ハンドラ | 第13章 |
| RecordPane | C | widgets | 棋譜ペインウィジェット | 第11章 |
| RecordPaneWiring | C | ui/wiring | 棋譜ペインのシグナル/スロット配線 | 第10章 |
| ReplayController | C | ui/controllers | 棋譜再生・自動再生の制御 | 第10章 |
| Setting | C | services | 設定ファイルの1項目（キーと既定値）の宣言。読み書きの入口になるテンプレート（ヘッダーオンリー） | 第12章 |
| SettingsCommon | NS | services | 設定ファイルのパスと、アプリ全体で共有する `QSettings` | 第12章 |
| SfenPositionTracer | C | board | SFEN局面状態のトレーサー | 第13章 |
| SfenUtils | NS | core | SFEN文字列のユーティリティ（ヘッダーオンリー） | 第2章, 第5章 |
| ShogiBoard | C | core | 将棋盤・駒台の状態管理 | 第5章 |
| ShogiClock | C | core | 対局時計・時間管理 | 第5章 |
| ShogiEngineInfoParser | C | engine | USIエンジンinfo行のパーサー | 第7章 |
| ShogiEngineThinkingModel | C | engine | エンジン思考結果リストモデル（6列） | 第7章 |
| ShogiGameController | C | game | 対局進行・盤面更新・合法手検証 | 第6章 |
| ShogiInfoRecord | C | kifu | USIエンジン思考情報レコード | 第7章, 第8章 |
| ShogiNotation | NS | core | 棋譜の表記（筋・段・駒の名前・CSA の駒種）の対応表（ヘッダーオンリー） | 第2章, 第5章 |
| ShogiView | C | views | 将棋盤の描画・レンダリングビュー | 第11章 |
| StartGameDialog | C | dialogs | 対局開始設定ダイアログ | 第11章 |
| ThinkingInfoPresenter | C | engine | 思考情報のGUI表示プレゼンター | 第7章 |
| TimeControlController | C | ui/controllers | 持ち時間・時計表示の制御 | 第10章 |
| TimeControlUtil | NS | services | 持ち時間設定のユーティリティ | 第12章 |
| TimeDisplayPresenter | C | ui/presenters | 時間表示更新プレゼンター | 第10章 |
| TsumePositionUtil | C | common | 詰将棋局面のユーティリティ | 第9章, 第12章 |
| TsumeSearchFlowController | C | analysis | 詰将棋探索フローの制御 | 第9章 |
| TsumeShogiSearchDialog | C | dialogs | 詰将棋探索ダイアログ | 第11章 |
| TurnManager | C | game | 手番管理・形式変換 | 第6章 |
| TurnSyncBridge | C | game | GameController/TurnManager/UI間の手番同期 | 第6章 |
| UiActionsWiring | C | ui/wiring | メニュー・ツールバーアクションのシグナル/スロット配線 | 第10章 |
| UsenToSfenConverter | C | kifu/formats | USEN形式→SFEN変換器 | 第8章 |
| Usi | C | engine | USIプロトコル通信ファサード | 第7章 |
| UsiCommLogModel | C | engine | USI通信ログ・エンジン情報モデル | 第7章 |
| UsiCommandController | C | ui/controllers | USIエンジンコマンド送信の制御 | 第10章 |
| UsiProtocolHandler | C | engine | USIプロトコルの送受信管理 | 第7章 |
| UsiToSfenConverter | C | kifu/formats | USI形式→SFEN変換器 | 第8章 |
| VersionDialog | C | dialogs | バージョン情報ダイアログ | 第11章 |

---

### 18.4 ファイル→章 対応表（逆引き）

主要ソースファイルがどの章で解説されているかの逆引き一覧。ヘッダー（`.h`）と実装（`.cpp`）は同一ディレクトリに配置されているため、ファイル名部分のみ記載する。

#### core/（第2章, 第5章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| shogiboard | 第2章, 第5章 | 盤面・駒台の状態管理 |
| shogimove | 第2章, 第5章 | 指し手データ構造 |
| enginemovevalidator | 第2章, 第5章 | 合法手判定 |
| fmv* | 第2章, 第5章 | 合法手判定の内部（局面・利き・指し手の生成） |
| shogiclock | 第5章 | 対局時計 |
| playmode | 第3章, 第5章 | 動作モード列挙体 |
| sfenutils | 第2章, 第5章 | SFENユーティリティ |
| shoginotation | 第2章, 第5章 | 棋譜の表記の対応表 |
| kifmovetext | 第5章 | KIF 形式の指し手の文字列 |
| legalmovestatus | 第5章 | 合法手ステータス |

#### game/（第6章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| matchcoordinator | 第4章, 第6章 | 対局進行の司令塔 |
| shogigamecontroller | 第6章 | 対局フロー制御 |
| gamestartcoordinator | 第6章 | 対局開始フロー |
| gamestatecontroller | 第6章 | 対局状態・終局処理 |
| turnmanager | 第6章 | 手番管理 |
| turnsyncbridge | 第6章 | 手番同期ブリッジ |
| promotionflow | 第6章 | 成/不成フロー |
| prestartcleanuphandler | 第6章 | 対局前クリーンアップ |
| consecutivegamescontroller | 第6章 | 連続対局制御 |

#### engine/（第7章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| usi | 第7章 | エンジンファサード |
| usiprotocolhandler | 第7章 | USIプロトコル処理 |
| engineprocessmanager | 第7章 | エンジンプロセス管理 |
| thinkinginfopresenter | 第7章 | 思考情報表示 |
| shogiengineinfoparser | 第7章 | info行パーサー |
| shogienginethinkingmodel | 第7章 | 思考結果モデル |
| usicommlogmodel | 第7章 | 通信ログモデル |
| enginesettingscoordinator | 第7章 | エンジン設定調整 |
| engineoptiondescriptions | 第7章 | オプション説明 |
| engineoptions | 第7章 | オプション定義 |
| enginesettingsconstants | 第1章, 第7章 | 設定定数 |

#### kifu/（第8章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| gamerecordmodel | 第8章 | 棋譜データモデル |
| kifubranchtree | 第8章 | 分岐ツリー |
| kifubranchnode | 第8章 | 分岐ノード |
| kifubranchtreebuilder | 第8章 | ツリー構築 |
| kifuloadcoordinator | 第8章 | 棋譜読み込み |
| kifusavecoordinator | 第8章 | 棋譜保存 |
| kifuioservice | 第8章 | ファイルI/O |
| kifuexportcontroller | 第8章 | エクスポート制御 |
| kifunavigationstate | 第8章, 第13章 | ナビゲーション状態 |
| recordline | 第8章 | 手順の値（RecordLine） |
| livegamesession | 第8章 | 対局中セッション |
| kifuclipboardservice | 第8章 | クリップボード操作 |
| kifucontentbuilder | 第8章 | コンテンツ構築 |
| shogiinforecord | 第7章, 第8章 | 思考情報レコード |
| kifreader | 第8章 | KIFファイル読み込み |
| kifdisplayitem | 第8章 | 表示アイテム定義 |
| kifparsetypes | 第8章 | パース型定義 |
| kifutypes | 第8章 | 共通型定義 |

#### kifu/formats/（第8章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| kiftosfenconverter | 第8章 | KIF→SFEN変換 |
| ki2tosfenconverter | 第8章 | KI2→SFEN変換 |
| csatosfenconverter | 第8章 | CSA→SFEN変換 |
| jkftosfenconverter | 第8章 | JKF→SFEN変換 |
| usentosfenconverter | 第8章 | USEN→SFEN変換 |
| usitosfenconverter | 第8章 | USI→SFEN変換 |

#### analysis/（第9章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| analysiscoordinator | 第9章 | 解析の低レベル制御 |
| analysisflowcontroller | 第9章 | 棋譜解析フロー |
| considerationflowcontroller | 第9章 | 検討モードフロー |
| considerationmodeuicontroller | 第9章 | 検討モードUI |
| tsumesearchflowcontroller | 第9章 | 詰将棋探索フロー |
| analysisresultspresenter | 第9章 | 解析結果表示 |
| kifuanalysislistmodel | 第9章 | 解析結果リストモデル |

#### network/（第12章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| csaclient | 第12章 | CSAプロトコルクライアント |
| csagamecoordinator | 第12章 | CSA対局調整 |

#### navigation/（第13章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| kifunavigationcontroller | 第13章 | ナビゲーション制御 |
| recordnavigationhandler | 第13章 | 棋譜ペイン行変更 |

#### board/（第13章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| boardinteractioncontroller | 第13章 | 盤面操作制御 |
| positioneditcontroller | 第13章 | 局面編集制御 |
| boardimageexporter | 第13章 | 盤面画像出力 |
| sfenpositiontracer | 第13章 | SFEN局面トレース |

#### ui/presenters/（第10章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| boardsyncpresenter | 第4章, 第10章 | 盤面同期 |
| evalgraphpresenter | 第10章 | 評価値グラフフック |
| gamerecordpresenter (recordpresenter) | 第4章, 第10章 | 棋譜表示 |
| navigationpresenter | 第10章 | ナビゲーション状態 |
| timedisplaypresenter | 第10章 | 時間表示更新 |

#### ui/controllers/（第10章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| boardsetupcontroller | 第10章 | 初期配置セットアップ |
| evaluationgraphcontroller | 第10章 | 評価値グラフ制御 |
| jishogiscoredialogcontroller | 第10章 | 持将棋点数ダイアログ |
| languagecontroller | 第10章, 第16章 | 言語切替 |
| nyugyokudeclarationhandler | 第10章 | 入玉宣言 |
| playerinfocontroller | 第10章 | プレイヤー情報 |
| pvclickcontroller | 第10章 | 読み筋クリック |
| replaycontroller | 第10章 | 棋譜再生 |
| timecontrolcontroller | 第10章 | 持ち時間制御 |
| usicommandcontroller | 第10章 | USIコマンド送信 |

#### ui/wiring/（第10章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| uiactionswiring | 第4章, 第10章 | メニュー・ツールバー配線 |
| considerationwiring | 第4章, 第10章 | 検討モード配線 |
| csagamewiring | 第10章 | CSA対局配線 |
| playerinfowiring | 第10章 | プレイヤー情報配線 |
| analysistabwiring | 第10章 | 解析タブ配線 |
| recordpanewiring | 第10章 | 棋譜ペイン配線 |
| menuwindowwiring | 第10章 | メニューウィンドウ配線 |
| matchcoordinatorwiring | 第6章, 第14章 | 司令塔の生成と配線 |
| josekiwindowwiring | 第10章 | 定跡ウィンドウ配線 |

#### ui/coordinators/（第10章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| dialogcoordinator | 第4章, 第10章 | ダイアログ管理 |
| gamelayoutbuilder | 第10章 | レイアウト構築 |
| docklayoutmanager | 第10章 | ドックレイアウト |
| kifudisplaycoordinator | 第10章 | 棋譜表示調整 |
| positioneditcoordinator | 第10章 | 局面編集調整 |
| aboutcoordinator | 第10章 | バージョン情報 |

#### views/（第11章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| shogiview | 第11章 | 将棋盤レンダリング |

#### widgets/（第11章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| recordpane | 第11章 | 棋譜ペイン |
| engineanalysistab | 第11章 | エンジン解析タブ |
| evaluationchartwidget | 第11章 | 評価値チャート |
| kifudisplay | 第11章 | 棋譜表示 |
| kifubranchdisplay | 第11章 | 分岐候補表示 |
| gameinfopanecontroller | 第11章 | 対局情報ペイン |
| engineinfowidget | 第11章 | エンジン情報 |
| kifuanalysisresultsdisplay | 第9章, 第11章 | 解析結果表示 |
| menubuttonwidget | 第11章 | メニューボタン |
| collapsiblegroupbox | 第11章 | 折りたたみボックス |
| elidelabel | 第11章 | テキスト省略ラベル |
| flowlayout | 第11章 | フローレイアウト |
| globaltooltip | 第11章 | ツールチップ |
| apptooltipfilter | 第11章 | ツールチップフィルタ |
| branchrowdelegate | 第11章 | 分岐行デリゲート |
| longlongspinbox | 第11章 | 64bit スピンボックス |
| numericrightaligncommadelegate | 第11章 | 数値デリゲート |

#### dialogs/（第11章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| startgamedialog | 第11章 | 対局開始 |
| kifuanalysisdialog | 第11章 | 棋譜解析 |
| csagamedialog | 第11章 | CSA対局 |
| engineregistrationdialog | 第11章 | エンジン登録 |
| changeenginesettingsdialog | 第11章 | エンジン設定変更 |
| promotedialog | 第11章 | 成/不成確認 |
| versiondialog | 第11章 | バージョン情報 |
| tsumeshogisearchdialog | 第11章 | 詰将棋探索 |
| pvboarddialog | 第11章 | 読み筋盤面 |
| kifupastedialog | 第11章 | 棋譜ペースト |
| csawaitingdialog | 第11章 | CSA待機 |
| menuwindow | 第11章 | メニューウィンドウ |
| josekiwindow | 第11章 | 定跡ウィンドウ |
| josekimergedialog | 第11章 | 定跡結合 |
| josekimovedialog | 第11章 | 定跡手入力 |

#### models/（第11章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| abstractlistmodel | 第11章 | リストモデル基底テンプレート |
| kifurecordlistmodel | 第11章 | 棋譜レコードモデル |
| kifubranchlistmodel | 第11章 | 分岐候補モデル |

#### services/（第12章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| setting | 第12章 | 設定1項目の宣言（`Setting<T>`） |
| settingscommon | 第12章 | 設定ファイルのパスと共有 `QSettings` |
| appsettings ほか `*settings` | 第12章 | ドメイン別の設定の宣言の表 |
| settingsservice | 第12章 | ドメイン別設定ヘッダーをまとめて取り込むだけのヘッダー |
| playernameservice | 第12章 | プレイヤー名解決 |
| timecontrolutil | 第12章 | 持ち時間ユーティリティ |

#### common/（第4章, 第12章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| errorbus | 第4章, 第12章 | エラー通知バス |
| jishogicalculator | 第12章 | 持将棋点数計算 |
| tsumepositionutil | 第9章, 第12章 | 詰将棋局面ユーティリティ |

#### app/（第1章, 第3章, 第14章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| main | 第1章 | アプリケーションエントリーポイント |
| mainwindow | 第3章, 第10章, 第14章 | メインウィンドウ（ハブ） |
| mainwindowserviceregistry | 第4章, 第14章 | ensure*・起動手順・主要操作（実装は gamesubregistry*.cpp などに分割） |
| mainwindowfoundationregistry | 第14章 | 複数の領域が使う部品の ensure* |
| kifusubregistry | 第8章, 第14章 | 棋譜系の ensure* |
| mainwindowlifecyclepipeline | 第14章 | 起動・終了の手順 |
| commentcoordinator | 第14章 | コメント調整 |
| dockcreationservice | 第14章 | ドック生成サービス |

#### resources/（第1章, 第16章）

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| shogiboardq.qrc | 第1章 | Qtリソース定義 |
| translations/ShogiBoardQ_ja_JP.ts | 第1章, 第16章 | 日本語翻訳 |
| translations/ShogiBoardQ_en.ts | 第1章, 第16章 | 英語翻訳 |

#### その他

| ファイル | 解説章 | 内容 |
|---------|--------|------|
| CMakeLists.txt | 第1章 | CMakeビルド設定 |
| CLAUDE.md | 第1章, 第4章 | 開発ガイドライン |

---

### 18.5 章別 主要トピック索引

各章の主要トピックへのクイックリファレンス。

| トピック | 章 | 節 |
|---------|----|----|
| SFEN記法 | 第2章 | §2.2 |
| USI指し手記法 | 第2章 | §2.3 |
| 棋譜フォーマット比較 | 第2章 | §2.5 |
| 5層アーキテクチャ | 第3章 | §3.1 |
| PlayMode列挙体（13モード） | 第3章 | §3.3 |
| Deps構造体パターン | 第4章 | §4.1 |
| ensure*()遅延初期化 | 第4章 | §4.2 |
| Hooks/std::functionコールバック | 第4章 | §4.3 |
| Wiringパターン | 第4章 | §4.3 |
| MVP分割（Presenter/Controller/Coordinator） | 第4章 | §4.4 |
| ErrorBusシングルトン | 第4章 | §4.5 |
| connect()でのラムダ禁止 | 第4章 | §4.6 |
| m_boardData（81要素QVector） | 第5章 | §5.1 |
| MatchCoordinator（対局司令塔） | 第6章 | §6.1 |
| Player列挙体（P1/P2） | 第6章 | §6.1 |
| TerminalType（10種の終局型） | 第8章 | §8.2 |
| エンジン3層構造（Usi/ProtocolHandler/ProcessManager） | 第7章 | §7.1 |
| SearchPhase（Idle/Main/Ponder） | 第7章 | §7.2 |
| 分岐ツリー（KifuBranchTree/Node） | 第8章 | §8.1 |
| 検討モード（Consideration） | 第9章 | §9.2 |
| 棋譜解析（KifuAnalysis） | 第9章 | §9.3 |
| 詰将棋探索（TsumeSearch） | 第9章 | §9.4 |
| ShogiViewの描画パイプライン | 第11章 | §11.1 |
| CSAプロトコル状態遷移 | 第12章 | §12.1 |
| 設定の宣言の表（`Setting<T>`）とINI永続化 | 第12章 | §12.5 |
| MainWindowのensure*()群（約40メソッド） | 第14章 | §14.2 |
| リファクタリング経緯 | 第14章 | §14.4 |
| ユースケースシーケンス図（5パターン） | 第15章 | §15.1–§15.5 |
| Qt Linguistワークフロー | 第16章 | §16.1 |
| 新機能追加チェックリスト | 第17章 | §17.8 |

<!-- chapter-18-end -->
