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
| [第5章](#第5章-core層純粋なゲームロジック) | core層：純粋なゲームロジック | ShogiBoard、ShogiMove、MoveValidator |
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
| [第17章](#第17章-テストとデバッグ) | テストとデバッグ | テスト実行、TestAutomationHelper、整合性検証 |
| [第18章](#第18章-新機能の追加ガイド) | 新機能の追加ガイド | 実践チュートリアル |
| [第19章](#第19章-用語集索引) | 用語集・索引 | 将棋用語日英対応、クラス名逆引き |

---

## 改訂履歴

| 日付 | 内容 |
|------|------|
| 2026-02-08 | 初版作成（第0章・第1章） |
| 2026-02-08 | 第2章「将棋のドメイン知識」作成 |

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
| **国際化** | 日本語・英語の2言語対応（実行時切替可能） |

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

# ソースファイルの自動収集（ディレクトリ別）
file(GLOB_RECURSE SRC_APP    CONFIGURE_DEPENDS src/app/*.cpp)
file(GLOB_RECURSE SRC_CORE   CONFIGURE_DEPENDS src/core/*.cpp)
# ... 全16ディレクトリ
```

**`GLOB_RECURSE`** を使用しているため、新しいソースファイルを追加する際に `CMakeLists.txt` を編集する必要はない。ファイルを適切なディレクトリに配置すれば自動的にビルド対象となる（CMake再構成時に検出される）。

**コンパイラ警告**: 以下の警告オプションが有効化されている:

```
-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
-Wnon-virtual-dtor -Woverloaded-virtual -Wformat=2 -Wimplicit-fallthrough
-Wunused -Wunused-function -Wunused-variable -Wunused-parameter
```

### 1.5 ディレクトリ構造

#### プロジェクトルート

```
ShogiBoardQ/
├── CMakeLists.txt          # トップレベルビルド構成
├── CLAUDE.md               # 開発ガイドライン
├── README.md               # プロジェクト概要
├── .clang-tidy             # 静的解析設定
├── src/                    # ソースコード（後述）
├── tests/                  # テストコード
├── resources/              # リソースファイル
│   ├── shogiboardq.qrc     # Qt リソース定義ファイル
│   ├── icons/              # アプリケーションアイコン
│   ├── images/
│   │   ├── actions/        # メニュー/ツールバーアイコン
│   │   └── pieces/         # 駒画像
│   ├── platform/           # プラットフォーム固有ファイル
│   └── translations/       # 翻訳ファイル（.ts）
├── docs/                   # ドキュメント
├── scripts/                # ユーティリティスクリプト
└── build/                  # ビルド出力（git管理外）
```

#### ソースコード（`src/`）

ヘッダファイル（`.h`）は対応するソースファイル（`.cpp`）と同じディレクトリに配置される。

```
src/
├── app/                            # アプリケーションエントリーポイント
│   ├── main.cpp                    #   エントリーポイント
│   ├── mainwindow.cpp/.h/.ui       #   メインウィンドウ（ハブ/ファサード）
│   ├── commentcoordinator.cpp/.h   #   コメント機能の調整
│   ├── dockcreationservice.cpp/.h  #   ドックウィジェット生成
│   └── testautomationhelper.cpp/.h #   テスト自動化ヘルパー
│
├── core/                           # 純粋なゲームロジック（Qt GUI非依存）
│   ├── shogiboard.cpp/.h           #   盤面状態・駒管理
│   ├── shogimove.cpp/.h            #   指し手の表現
│   ├── movevalidator.cpp/.h        #   合法手判定
│   ├── shogiclock.cpp/.h           #   対局時計
│   ├── shogiutils.cpp/.h           #   ユーティリティ関数
│   ├── sfenutils.h                 #   SFEN文字列操作
│   ├── playmode.h                  #   プレイモード定義
│   └── legalmovestatus.h           #   合法手判定の状態
│
├── game/                           # 対局管理
│   ├── matchcoordinator.cpp/.h     #   対局全体の司令塔
│   ├── shogigamecontroller.cpp/.h  #   ゲームフロー制御
│   ├── gamestartcoordinator.cpp/.h #   対局開始フロー
│   ├── gamestatecontroller.cpp/.h  #   ゲーム状態制御
│   ├── turnmanager.cpp/.h          #   手番管理
│   ├── turnsyncbridge.cpp/.h       #   手番同期ブリッジ
│   ├── promotionflow.cpp/.h        #   成/不成の選択フロー
│   ├── prestartcleanuphandler.cpp/.h # 対局前クリーンアップ
│   └── consecutivegamescontroller.cpp/.h # 連続対局制御
│
├── kifu/                           # 棋譜管理
│   ├── gamerecordmodel.cpp/.h      #   棋譜データモデル
│   ├── kifubranchnode.cpp/.h       #   分岐ノード
│   ├── kifubranchtree.cpp/.h       #   分岐ツリー
│   ├── kifubranchtreebuilder.cpp/.h#   分岐ツリー構築
│   ├── livegamesession.cpp/.h      #   リアルタイム記録セッション
│   ├── kifuloadcoordinator.cpp/.h  #   棋譜読み込み調整
│   ├── kifusavecoordinator.cpp/.h  #   棋譜保存調整
│   ├── kifuioservice.cpp/.h        #   棋譜ファイルI/O
│   ├── kifuexportcontroller.cpp/.h #   棋譜エクスポート制御
│   ├── kifuclipboardservice.cpp/.h #   クリップボード操作
│   ├── kifucontentbuilder.cpp/.h   #   棋譜コンテンツ構築
│   ├── kifunavigationstate.cpp/.h  #   ナビゲーション状態
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
│       ├── usentosfenconverter.cpp/.h  #   USEN → SFEN
│       └── usitosfenconverter.cpp/.h   #   USI → SFEN
│
├── analysis/                       # 解析機能
│   ├── analysiscoordinator.cpp/.h  #   解析オーケストレーション
│   ├── analysisflowcontroller.cpp/.h # 解析フロー制御
│   ├── considerationflowcontroller.cpp/.h # 検討モード制御
│   ├── considerationmodeuicontroller.cpp/.h # 検討UI制御
│   ├── tsumesearchflowcontroller.cpp/.h # 詰将棋探索制御
│   ├── analysisresultspresenter.cpp/.h # 解析結果表示
│   └── kifuanalysislistmodel.cpp/.h # 棋譜解析リストモデル
│
├── engine/                         # USIエンジン連携
│   ├── usi.cpp/.h                  #   エンジンファサード
│   ├── usiprotocolhandler.cpp/.h   #   USIプロトコル処理
│   ├── engineprocessmanager.cpp/.h #   エンジンプロセス管理
│   ├── enginesettingscoordinator.cpp/.h # エンジン設定調整
│   ├── shogiengineinfoparser.cpp/.h # エンジン情報パース
│   ├── shogienginethinkingmodel.cpp/.h # 思考情報Qtモデル
│   ├── thinkinginfopresenter.cpp/.h # 思考情報表示
│   ├── usicommlogmodel.cpp/.h      #   USI通信ログモデル
│   ├── engineoptiondescriptions.cpp/.h # エンジンオプション説明
│   ├── engineoptions.h             #   エンジンオプション定義
│   └── enginesettingsconstants.h   #   エンジン設定定数
│
├── network/                        # CSA通信対局
│   ├── csaclient.cpp/.h            #   CSAプロトコルクライアント
│   └── csagamecoordinator.cpp/.h   #   CSA対局調整
│
├── navigation/                     # ナビゲーション
│   ├── kifunavigationcontroller.cpp/.h # 棋譜ナビゲーション制御
│   └── recordnavigationhandler.cpp/.h # レコードペイン行変更ハンドラ
│
├── board/                          # 盤面操作
│   ├── boardinteractioncontroller.cpp/.h # 盤面インタラクション制御
│   ├── positioneditcontroller.cpp/.h # 局面編集制御
│   ├── boardimageexporter.cpp/.h   #   盤面画像エクスポート
│   └── sfenpositiontracer.cpp/.h   #   SFEN局面追跡
│
├── ui/                             # UI層
│   ├── presenters/                 #   MVPプレゼンター層
│   │   ├── boardsyncpresenter.cpp/.h   # 盤面同期
│   │   ├── evalgraphpresenter.cpp/.h   # 評価グラフ
│   │   ├── navigationpresenter.cpp/.h  # ナビゲーション表示
│   │   ├── recordpresenter.cpp/.h      # 棋譜表示
│   │   └── timedisplaypresenter.cpp/.h # 時間表示
│   ├── controllers/                #   UIイベントハンドラ
│   │   ├── boardsetupcontroller.cpp/.h     # 盤面セットアップ
│   │   ├── evaluationgraphcontroller.cpp/.h # 評価グラフ制御
│   │   ├── languagecontroller.cpp/.h       # 言語切替制御
│   │   ├── playerinfocontroller.cpp/.h     # 対局者情報制御
│   │   ├── pvclickcontroller.cpp/.h        # PVクリック制御
│   │   ├── replaycontroller.cpp/.h         # リプレイ制御
│   │   ├── timecontrolcontroller.cpp/.h    # 時間制御
│   │   ├── usicommandcontroller.cpp/.h     # USIコマンド制御
│   │   ├── nyugyokudeclarationhandler.cpp/.h # 入玉宣言処理
│   │   └── jishogiscoredialogcontroller.cpp/.h # 持将棋スコア
│   ├── wiring/                     #   シグナル/スロット接続
│   │   ├── uiactionswiring.cpp/.h      # UIアクション接続
│   │   ├── analysistabwiring.cpp/.h    # 解析タブ接続
│   │   ├── considerationwiring.cpp/.h  # 検討モード接続
│   │   ├── csagamewiring.cpp/.h        # CSA対局接続
│   │   ├── playerinfowiring.cpp/.h     # 対局者情報接続
│   │   ├── recordpanewiring.cpp/.h     # 棋譜ペイン接続
│   │   ├── menuwindowwiring.cpp/.h     # メニューウィンドウ接続
│   │   └── josekiwindowwiring.cpp/.h   # 定跡ウィンドウ接続
│   └── coordinators/               #   UI調整
│       ├── dialogcoordinator.cpp/.h     # ダイアログ調整
│       ├── gamelayoutbuilder.cpp/.h     # ゲームレイアウト構築
│       ├── docklayoutmanager.cpp/.h     # ドックレイアウト管理
│       ├── kifudisplaycoordinator.cpp/.h # 棋譜表示調整
│       ├── positioneditcoordinator.cpp/.h # 局面編集調整
│       └── aboutcoordinator.cpp/.h      # バージョン情報調整
│
├── views/                          # Qt Graphics View
│   └── shogiview.cpp/.h            #   将棋盤描画ビュー
│
├── widgets/                        # カスタムQtウィジェット
│   ├── recordpane.cpp/.h           #   棋譜表示ペイン
│   ├── engineanalysistab.cpp/.h    #   エンジン解析タブ
│   ├── evaluationchartwidget.cpp/.h #  評価値グラフウィジェット
│   ├── kifudisplay.cpp/.h          #   棋譜表示ウィジェット
│   ├── kifubranchdisplay.cpp/.h    #   分岐表示ウィジェット
│   ├── branchtreewidget.cpp/.h     #   分岐ツリーウィジェット
│   ├── gameinfopanecontroller.cpp/.h # 対局情報ペイン制御
│   ├── engineinfowidget.cpp/.h     #   エンジン情報ウィジェット
│   ├── kifuanalysisresultsdisplay.cpp/.h # 棋譜解析結果表示
│   ├── menubuttonwidget.cpp/.h     #   メニューボタン
│   ├── collapsiblegroupbox.cpp/.h  #   折りたたみグループボックス
│   ├── elidelabel.cpp/.h           #   省略ラベル
│   ├── flowlayout.cpp/.h           #   フローレイアウト
│   ├── globaltooltip.cpp/.h        #   グローバルツールチップ
│   ├── apptooltipfilter.cpp/.h     #   ツールチップフィルタ
│   ├── branchrowdelegate.cpp/.h    #   分岐行デリゲート
│   ├── longlongspinbox.cpp/.h      #   long long対応スピンボックス
│   └── numeric_right_align_comma_delegate.h # 数値右寄せデリゲート
│
├── dialogs/                        # ダイアログ実装
│   ├── startgamedialog.cpp/.h/.ui  #   対局開始ダイアログ
│   ├── considerationdialog.cpp/.h/.ui # 検討設定ダイアログ
│   ├── kifuanalysisdialog.cpp/.h/.ui # 棋譜解析ダイアログ
│   ├── csagamedialog.cpp/.h/.ui    #   CSA対局接続ダイアログ
│   ├── engineregistrationdialog.cpp/.h/.ui # エンジン登録ダイアログ
│   ├── changeenginesettingsdialog.cpp/.h/.ui # エンジン設定変更
│   ├── promotedialog.cpp/.h/.ui    #   成/不成選択ダイアログ
│   ├── versiondialog.cpp/.h/.ui    #   バージョン情報ダイアログ
│   ├── tsumeshogisearchdialog.cpp/.h # 詰将棋探索ダイアログ
│   ├── pvboarddialog.cpp/.h        #   読み筋盤面ダイアログ
│   ├── kifupastedialog.cpp/.h      #   棋譜貼り付けダイアログ
│   ├── csawaitingdialog.cpp/.h     #   CSA待機ダイアログ
│   ├── menuwindow.cpp/.h           #   メニューウィンドウ
│   ├── josekiwindow.cpp/.h         #   定跡ウィンドウ
│   ├── josekimergedialog.cpp/.h    #   定跡結合ダイアログ
│   └── josekimovedialog.cpp/.h     #   定跡移動ダイアログ
│
├── models/                         # Qtモデル
│   ├── abstractlistmodel.h         #   抽象リストモデル基底
│   ├── kifurecordlistmodel.cpp/.h  #   棋譜レコードリストモデル
│   └── kifubranchlistmodel.cpp/.h  #   分岐リストモデル
│
├── services/                       # クロスカッティングサービス
│   ├── settingsservice.cpp/.h      #   INI設定の読み書き
│   ├── timekeepingservice.cpp/.h   #   時間管理サービス
│   ├── playernameservice.cpp/.h    #   対局者名解決サービス
│   └── timecontrolutil.cpp/.h      #   時間制御ユーティリティ
│
└── common/                         # 共有ユーティリティ
    ├── errorbus.cpp/.h             #   エラー通知バス
    ├── jishogicalculator.cpp/.h    #   持将棋点数計算
    └── tsumepositionutil.cpp/.h    #   詰将棋局面ユーティリティ
```

### 1.6 コード規模

| ディレクトリ | ファイル数 | 行数 | 概要 |
|-------------|-----------|------|------|
| `app/` | 9 | 7,271 | メインウィンドウ、エントリーポイント |
| `core/` | 13 | 3,950 | 純粋ゲームロジック |
| `game/` | 18 | 7,887 | 対局管理 |
| `kifu/` | 31 | 10,282 | 棋譜管理（formatsを除く） |
| `kifu/formats/` | 12 | 7,651 | フォーマット変換器 |
| `engine/` | 20 | 5,709 | USIエンジン連携 |
| `analysis/` | 14 | 3,564 | 解析機能 |
| `network/` | 4 | 3,033 | CSA通信対局 |
| `navigation/` | 4 | 953 | ナビゲーション |
| `board/` | 8 | 1,536 | 盤面操作 |
| `ui/presenters/` | 10 | 1,380 | MVPプレゼンター |
| `ui/controllers/` | 20 | 3,466 | UIコントローラ |
| `ui/wiring/` | 16 | 2,921 | シグナル/スロット接続 |
| `ui/coordinators/` | 12 | 3,407 | UI調整 |
| `views/` | 2 | 4,158 | 将棋盤描画 |
| `widgets/` | 35 | 9,543 | カスタムウィジェット |
| `dialogs/` | 32 | 11,427 | ダイアログ |
| `models/` | 5 | 838 | Qtモデル |
| `services/` | 8 | 2,151 | 横断サービス |
| `common/` | 6 | 478 | 共有ユーティリティ |
| **合計** | **279** | **約91,600** | |

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
├── CMakeLists.txt                      # テスト用ビルド構成
├── fakes/                              # テスト用モック/フェイク実装
├── tst_kifudisplaycoordinator.cpp      # 棋譜表示調整テスト
├── tst_livegamesession.cpp             # ライブゲームセッションテスト
└── tst_prestartcleanuphandler.cpp      # 対局前クリーンアップテスト
```

テストの詳細は[第17章](#第17章-テストとデバッグ)で解説する。

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

日本語の棋譜表記では筋を全角数字、段を漢数字で表す。`shogiutils.cpp` に変換関数がある。

```cpp
// src/core/shogiutils.cpp

// 段番号（1〜9）→ 漢字（"一"〜"九"）
QString transRankTo(const int rankTo)
{
    static const QStringList rankStrings = { "", "一", "二", "三", "四", "五", "六", "七", "八", "九" };
    // ...
    return rankStrings.at(rankTo);
}

// 筋番号（1〜9）→ 全角数字（"１"〜"９"）
QString transFileTo(const int fileTo)
{
    static const QStringList fileStrings = { "", "１", "２", "３", "４", "５", "６", "７", "８", "９" };
    // ...
    return fileStrings.at(fileTo);
}
```

例: 7筋六段 → `"７六"` (7筋=`"７"`, 6段=`"六"`)

#### 内部インデックスとの対応

コード内部では、`ShogiMove` の座標値は **0-indexed**（0〜8）で管理される。

```cpp
// src/core/shogimove.h
struct ShogiMove {
    QPoint fromSquare;   // 移動元（盤上:0-8、駒台:9=先手,10=後手）
    QPoint toSquare;     // 移動先（0-8）
    QChar movingPiece;   // 動かした駒（SFEN表記）
    QChar capturedPiece; // 取った駒（なければ ' '）
    bool isPromotion;    // 成りフラグ
};
```

| 表記系 | 筋の範囲 | 段の範囲 | 用途 |
|--------|---------|---------|------|
| 外部（SFEN/USI/棋譜） | 1〜9 | 1〜9（USIでは a〜i） | ファイルI/O、エンジン通信 |
| 内部（`ShogiMove`） | 0〜8 | 0〜8 | コード内の座標計算 |
| 駒台 | 9=先手, 10=後手 | 駒種別に割当 | 持ち駒管理 |

**変換式**: 内部値 + 1 = 外部値

```cpp
// src/core/shogiutils.cpp — moveToUsi() より
const int toFile = toX + 1;          // 0-indexed → 1-indexed
const QChar toRank = QChar('a' + toY); // 0 → 'a', 8 → 'i'
```

`ShogiBoard` の盤面データは1次元配列 `QVector<QChar> m_boardData`（81要素）で保持される。

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

> **注意**: SFEN文字列の盤面パートは9一（左上）から1一（右上）へ左から右に読む。つまり筋が**9→1**の降順で並ぶ。`setPiecePlacementFromSfen()` ではこれに合わせて `file = fileCount; file > 0; --file` とループしている。

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

SFEN標準では成駒を `+` 接頭辞で表すが、本プロジェクトではコード内部で1文字に変換して扱う。

| 成駒名 | 日本語 | SFEN標準 | 内部文字（先手） | 内部文字（後手） |
|--------|--------|---------|----------------|----------------|
| と金 | Promoted Pawn | `+P` / `+p` | `Q` | `q` |
| 成香 | Promoted Lance | `+L` / `+l` | `M` | `m` |
| 成桂 | Promoted Knight | `+N` / `+n` | `O` | `o` |
| 成銀 | Promoted Silver | `+S` / `+s` | `T` | `t` |
| 馬 | Horse (Promoted Bishop) | `+B` / `+b` | `C` | `c` |
| 龍 | Dragon (Promoted Rook) | `+R` / `+r` | `U` | `u` |

> **金将と玉将には成駒がない。**

この変換は `ShogiBoard::validateAndConvertSfenBoardStr()` で行われる。

```cpp
// src/core/shogiboard.cpp

const QStringList promotions = {"+P", "+L", "+N", "+S", "+B", "+R",
                                "+p", "+l", "+n", "+s", "+b", "+r"};
const QString replacements = "QMOTCUqmotcu";

for (qsizetype i = 0; i < promotions.size(); ++i) {
    initialSfenStr.replace(promotions[i], replacements[i]);
}
```

逆変換（内部文字 → SFEN文字列）は `convertPieceToSfen()` が担う。

```cpp
// src/core/shogiboard.cpp
QString ShogiBoard::convertPieceToSfen(const QChar piece) const
{
    static const QMap<QChar, QString> sfenMap = {
        {'Q', "+P"}, {'M', "+L"}, {'O', "+N"}, {'T', "+S"},
        {'C', "+B"}, {'U', "+R"}, {'q', "+p"}, {'m', "+l"},
        {'o', "+n"}, {'t', "+s"}, {'c', "+b"}, {'u', "+r"}
    };
    return sfenMap.contains(piece) ? sfenMap.value(piece) : QString(piece);
}
```

#### 駒台（持ち駒）

駒台は `QMap<QChar, int>` で管理される。キーは駒文字、値は枚数。

```cpp
// src/core/shogiboard.cpp — initStand()
static const QList<QChar> pieces = {'p', 'l', 'n', 's', 'g', 'b', 'r', 'k',
                                    'P', 'L', 'N', 'S', 'G', 'B', 'R', 'K'};
```

駒を取ったとき、成駒は元の駒に変換して相手の駒台に加える。

```cpp
// src/core/shogiboard.cpp — convertPieceChar()
// 成駒は相手の生駒に変換し、それ以外は大文字/小文字を反転する
static const QMap<QChar, QChar> conversionMap = {
    {'Q', 'p'}, {'M', 'l'}, {'O', 'n'}, {'T', 's'}, {'C', 'b'}, {'U', 'r'},
    {'q', 'P'}, {'m', 'L'}, {'o', 'N'}, {'t', 'S'}, {'c', 'B'}, {'u', 'R'}
};
```

#### 全駒リストと PieceType 列挙

`MoveValidator` では駒種を列挙型で定義している。

```cpp
// src/core/movevalidator.h
enum PieceType {
    PAWN,             // 歩   (P/p)
    LANCE,            // 香   (L/l)
    KNIGHT,           // 桂   (N/n)
    SILVER,           // 銀   (S/s)
    GOLD,             // 金   (G/g)
    BISHOP,           // 角   (B/b)
    ROOK,             // 飛   (R/r)
    KING,             // 玉   (K/k)
    PROMOTED_PAWN,    // と金 (Q/q)
    PROMOTED_LANCE,   // 成香 (M/m)
    PROMOTED_KNIGHT,  // 成桂 (O/o)
    PROMOTED_SILVER,  // 成銀 (T/t)
    HORSE,            // 馬   (C/c)
    DRAGON,           // 龍   (U/u)
    PIECE_TYPE_SIZE   // 駒種の総数 = 14
};
```

全28文字（先手14種 + 後手14種）がビットボード管理の対象となる。

```cpp
// src/core/movevalidator.cpp — コンストラクタ
m_allPieces = {'P', 'L', 'N', 'S', 'G', 'B', 'R', 'K', 'Q', 'M', 'O', 'T', 'C', 'U',
               'p', 'l', 'n', 's', 'g', 'b', 'r', 'k', 'q', 'm', 'o', 't', 'c', 'u'};
```

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

盤面は `/` で9段に区切られ、各段は9一（左上、筋9）から1一（右上、筋1）へ**右から左**に読む。

- アルファベット文字: 駒（大文字=先手、小文字=後手）
- 数字（1〜9）: 連続する空マスの数
- `+`: 直後の駒文字が成駒であることを示す（`+P` = と金）

```
lnsgkgsnl    ← 1段目: 後手の 香桂銀金玉金銀桂香
/1r5b1/      ← 2段目: 空1 飛 空5 角 空1
ppppppppp    ← 3段目: 後手の歩×9
/9/9/9/      ← 4〜6段目: 全て空
PPPPPPPPP    ← 7段目: 先手の歩×9
/1B5R1/      ← 8段目: 空1 角 空5 飛 空1
LNSGKGSNL    ← 9段目: 先手の 香桂銀金玉金銀桂香
```

SFEN文字列の検証は `ShogiBoard::validateSfenString()` で行われる。

```cpp
// src/core/shogiboard.cpp
void ShogiBoard::validateSfenString(const QString& sfenStr,
                                     QString& sfenBoardStr, QString& sfenStandStr)
{
    QStringList sfenComponents = sfenStr.split(" ");
    if (sfenComponents.size() != 4) {
        // エラー: SFEN文字列は4要素で構成される必要がある
    }
    sfenBoardStr = sfenComponents.at(0);         // 盤面
    m_currentPlayer = sfenComponents.at(1);       // 手番 "b" or "w"
    sfenStandStr = sfenComponents.at(2);          // 持ち駒
    m_currentMoveNumber = sfenComponents.at(3).toInt(); // 手数
}
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

出力順序は `R B G S N L P`（先手 → 後手）の順。

```cpp
// src/core/shogiboard.cpp — convertStandToSfen()
QList<QChar> keys = {'R', 'B', 'G', 'S', 'N', 'L', 'P'};
for (const auto& key : keys) {
    // 先手
    int value = m_pieceStand.value(key, 0);
    if (value > 0) {
        handPiece += (value > 1 ? QString::number(value) : "") + convertPieceToSfen(key);
    }
    // 後手（小文字）
    value = m_pieceStand.value(key.toLower(), 0);
    if (value > 0) {
        handPiece += (value > 1 ? QString::number(value) : "") + convertPieceToSfen(key.toLower());
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

```cpp
// src/core/shogiutils.cpp — moveToUsi()

QString moveToUsi(const ShogiMove& move)
{
    const int toFile = toX + 1;
    const QChar toRank = QChar('a' + toY);

    // 駒打ち: fromXが9（先手駒台）または10（後手駒台）
    if (fromX == 9 || fromX == 10) {
        QChar piece = move.movingPiece.toUpper();
        return QStringLiteral("%1*%2%3").arg(piece).arg(toFile).arg(toRank);
    }

    // 通常移動
    const int fromFile = fromX + 1;
    const QChar fromRank = QChar('a' + fromY);
    QString usi = QStringLiteral("%1%2%3%4")
                      .arg(fromFile).arg(fromRank).arg(toFile).arg(toRank);

    if (move.isPromotion) {
        usi += QLatin1Char('+');
    }
    return usi;
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

**KI2**: KIFの簡略版。移動元座標が省略されるため、パーサは現在の盤面を追跡し合法手から移動元を推定する必要がある（`inferSourceSquare()` ロジック）。複数の駒が同じマスに移動できる場合は「右」「左」「上」「引」「寄」「直」等の修飾語で区別する。

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
| 内部(0-8) → 日本語筋 | `transFileTo(file+1)` | → `"１"`〜`"９"` |
| 内部(0-8) → 日本語段 | `transRankTo(rank+1)` | → `"一"`〜`"九"` |
| 全角数字 → 数値 | `parseFullwidthFile(ch)` | `"１"` → `1` |
| 漢数字 → 数値 | `parseKanjiRank(ch)` | `"一"` → `1` |
| file,rank → 配列index | `(rank-1)*9 + (file-1)` | 1-indexed入力 |

<!-- chapter-2-end -->

---

<!-- chapter-3-start -->
## 第3章 アーキテクチャ全体図

*（後続セッションで作成予定）*

<!-- chapter-3-end -->

---

<!-- chapter-4-start -->
## 第4章 設計パターンとコーディング規約

*（後続セッションで作成予定）*

<!-- chapter-4-end -->

---

<!-- chapter-5-start -->
## 第5章 core層：純粋なゲームロジック

*（後続セッションで作成予定）*

<!-- chapter-5-end -->

---

<!-- chapter-6-start -->
## 第6章 game層：対局管理

*（後続セッションで作成予定）*

<!-- chapter-6-end -->

---

<!-- chapter-7-start -->
## 第7章 engine層：USIエンジン連携

*（後続セッションで作成予定）*

<!-- chapter-7-end -->

---

<!-- chapter-8-start -->
## 第8章 kifu層：棋譜管理

*（後続セッションで作成予定）*

<!-- chapter-8-end -->

---

<!-- chapter-9-start -->
## 第9章 analysis層：解析機能

*（後続セッションで作成予定）*

<!-- chapter-9-end -->

---

<!-- chapter-10-start -->
## 第10章 UI層：プレゼンテーション

*（後続セッションで作成予定）*

<!-- chapter-10-end -->

---

<!-- chapter-11-start -->
## 第11章 views/widgets/dialogs層：Qt UI部品

*（後続セッションで作成予定）*

<!-- chapter-11-end -->

---

<!-- chapter-12-start -->
## 第12章 network層とservices層

*（後続セッションで作成予定）*

<!-- chapter-12-end -->

---

<!-- chapter-13-start -->
## 第13章 navigation層とboard層

*（後続セッションで作成予定）*

<!-- chapter-13-end -->

---

<!-- chapter-14-start -->
## 第14章 MainWindowの役割と構造

*（後続セッションで作成予定）*

<!-- chapter-14-end -->

---

<!-- chapter-15-start -->
## 第15章 機能フロー詳解

*（後続セッションで作成予定）*

<!-- chapter-15-end -->

---

<!-- chapter-16-start -->
## 第16章 国際化（i18n）と翻訳

*（後続セッションで作成予定）*

<!-- chapter-16-end -->

---

<!-- chapter-17-start -->
## 第17章 テストとデバッグ

*（後続セッションで作成予定）*

<!-- chapter-17-end -->

---

<!-- chapter-18-start -->
## 第18章 新機能の追加ガイド

*（後続セッションで作成予定）*

<!-- chapter-18-end -->

---

<!-- chapter-19-start -->
## 第19章 用語集・索引

*（後続セッションで作成予定）*

<!-- chapter-19-end -->
