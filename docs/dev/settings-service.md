# 設定ファイルと設定項目の宣言

ShogiBoardQ のアプリケーション設定の永続化に関するガイド。

設定項目は `Setting<T>`（`src/services/setting.h`）で**キーと既定値を1行で宣言**し、読み書きはその宣言を通す。
getter と setter の組やキー定数は手で書かない。値の範囲を絞る、複数のキーを構造体にまとめる、配列で保存する、
といった処理が要る項目だけを関数として書く。

---

## 目次

1. [設定ファイルの保存先](#1-設定ファイルの保存先)
2. [構成](#2-構成)
3. [設定の読み書き](#3-設定の読み書き)
4. [新しい設定項目の追加手順](#4-新しい設定項目の追加手順)
5. [関数として書く項目](#5-関数として書く項目)
6. [QSettings を直接開く場合（配列・グループのデータ）](#6-qsettings-を直接開く場合配列グループのデータ)
7. [エンジンの登録情報（EngineSettingsConstants）](#7-エンジンの登録情報enginesettingsconstants)
8. [テスト](#8-テスト)
9. [旧バージョンからの移行](#9-旧バージョンからの移行)

---

## 1. 設定ファイルの保存先

設定は `ShogiBoardQ.ini`（INI形式）に保存される。
`QStandardPaths::AppConfigLocation` を使用し、プラットフォームごとの標準的な場所に配置される。

| プラットフォーム | パス |
|---|---|
| **macOS** | `~/Library/Preferences/ShogiBoardQ/ShogiBoardQ.ini` |
| **Linux** | `~/.config/ShogiBoardQ/ShogiBoardQ.ini` |
| **Windows** | `C:/Users/<user>/AppData/Local/ShogiBoardQ/ShogiBoardQ.ini` |

パス解決は `SettingsCommon::settingsFilePath()`（`src/services/settingscommon.h`）が行う。
ディレクトリが存在しない場合は初回アクセス時に自動作成される。
環境変数 `SHOGIBOARDQ_CONFIG_HOME` を指定すると、その下の `ShogiBoardQ` フォルダを使う（`AppPaths::configDirectory()`。テストや撮影で普段の設定を書き換えないために使う）。

`settingsFilePath()` は最初に呼ばれたときのパスを覚え、`SettingsCommon::openSettings()` はそのパスで開いた1つの `QSettings` を返し続ける。
保存先を切り替えるときは、設定に触れる前に環境変数を設定する。

> **前提条件**: `main.cpp` で `QApplication::setApplicationName("ShogiBoardQ")` が設定されていること。
> これにより `QStandardPaths` がアプリ名ベースのディレクトリを返す。CLI（`src/cli/main.cpp`）も同じ名前を設定している。

> **注意**: `QDir::setCurrent(QApplication::applicationDirPath())` + 相対パスでの `QSettings` 構築は**使用しないこと**。
> macOS の `.app` バンドル内に書き込もうとしてコード署名が壊れる原因となる。

---

## 2. 構成

### ファイル構成

| ファイル | 内容 |
|---|---|
| `src/services/setting.h` | `Setting<T>`。設定ファイルの1項目（キーと既定値）を表し、読み書きの入口になる |
| `src/services/settingscommon.h/.cpp` | 設定ファイルのパス（`settingsFilePath()`）、アプリ全体で共有する `QSettings`（`openSettings()`）、全設定の消去（`resetAllSettings()`） |
| ドメイン別ヘッダー（下表） | 設定項目の宣言の表。処理が要る項目の関数宣言もここに置く |
| `src/services/settingskeys.h` | 関数として書いた項目と、機能ごとのエンジン選択（`EngineListSettings`）が使うキー定数。宣言の表の項目はキーを宣言に直接書くので、新しい項目のためにここへ定数を足さない |
| `src/services/settingsservice.h` | ドメイン別ヘッダーをまとめて include するだけのヘッダー（関数はない。`settingsservice.cpp` も空）。新しいコードはドメイン別ヘッダーを直接 include する |
| `src/services/enginelistsettings.h/.cpp` | エンジン登録一覧の読み込みと、機能ごとに選んだエンジンの保存（[7章](#7-エンジンの登録情報enginesettingsconstants)） |
| `src/services/enginepondersettings.h/.cpp` | エンジンごとの GUI 先読み設定（[7章](#7-エンジンの登録情報enginesettingsconstants)） |

### ドメイン別ヘッダー

宣言だけの項目は `inline const` なのでヘッダーだけで完結する。`.cpp` は関数として書いた項目がある namespace にだけある。

| namespace | ファイル | 主な内容 |
|---|---|---|
| `AppSettings` | `appsettings.h/.cpp` | 言語・指し手の表記、GUI共通フォント、UI状態、駒画像・駒音、盤の配色、メニューウィンドウ、メインウィンドウのサイズと盤のマスの大きさ、補助ダイアログの文字サイズ・大きさ（id ごと） |
| `GameSettings` | `gamesettings.h` | 棋譜ファイルのディレクトリ、棋譜欄、コメント・対局情報タブ、対局開始ダイアログ（`GameSettings::StartGame`）、棋譜貼り付け、局面集、持将棋の点数ダイアログ |
| `AnalysisSettings` | `analysissettings.h/.cpp` | 分岐ツリー、評価値グラフ、解析タブ（列幅・USIログ・思考タブ）、棋譜解析、検討、詰み探索、読み筋盤面 |
| `NetworkSettings` | `networksettings.h` | CSA通信対局ダイアログ（対局者・接続先・詳細設定）、待機ダイアログ、通信ログ |
| `JosekiSettings` | `josekisettings.h/.cpp` | 定跡ウィンドウ、定跡手編集・定跡マージダイアログ |
| `TsumeshogiSettings` | `tsumeshogisettings.h/.cpp` | 詰将棋局面生成ダイアログ、詰将棋の問題一覧・対局画面 |
| `EngineDialogSettings` | `enginedialogsettings.h` | エンジン設定・エンジン登録ダイアログ |
| `DockSettings` | `docksettings.h/.cpp` | ドックのフローティング・ジオメトリ・表示状態、ドックの固定、名前付きレイアウト |

### GUI共通フォント（`AppSettings::uiFontFamily`）

書体は `AppSettings::uiFontFamily`（`General/uiFontFamily`。空文字列は標準）に、フォント設定ダイアログのサイズは `AppSettings::fontSettingsDialogSize` に保存する。

「表示 → GUI全体のフォント…」で書体を選び、OKで画面全体に反映する。
各画面の文字サイズ・太字設定は維持する。キャンセル時は書体を変更せず、
「標準に戻す」で標準書体と通信ログの等幅書体に戻せる。
起動時は `ApplicationFonts::initialize(AppSettings::uiFontFamily(), language)` で復元する。
標準書体は解決済みのUI言語に応じて選び、英語はOS標準、日本語は日本語用、
中国語は簡体字・繁体字用のインストール済み書体を優先する。候補が未導入ならOS標準を使う。
保存された書体が未導入の場合も、その言語の標準書体に戻る。フォントは同梱しない。
日本語表記の棋譜・読み筋は `ApplicationFonts::japaneseFont()` で日本語の字形を使う。
利用者の明示的な書体指定はこれらの欄でも優先する。文字サイズや装飾は変更しない。
日本語棋譜専用の入力欄等には `useJapaneseFont()` を適用し、親から文字サイズが
再設定された際にも書体を維持する。Qt 6.8以降は文字種別の補完先もUI言語に合わせ、
Qt 6.7はフォントの候補リストと棋譜欄の明示指定で補完する。

---

## 3. 設定の読み書き

### 宣言

```cpp
// src/services/gamesettings.h
namespace GameSettings {
/// 棋譜コメントタブのフォントサイズ（デフォルト: 10）
inline const Setting<int> commentFontSize{"FontSize/comment", 10};
/// 棋譜貼り付けダイアログのウィンドウサイズ（デフォルト: 600x500）
inline const Setting<QSize> kifuPasteDialogSize{"SizeRelated/kifuPasteDialogSize", QSize(600, 500)};
}
```

キーは `"グループ名/キー名"` の文字列を直接書く。
`T` は `QVariant` に入る型（`int`・`bool`・`QString`・`QSize`・`QStringList`・`QByteArray` など）。

### 読み書き

```cpp
#include "gamesettings.h"

const int size = GameSettings::commentFontSize();                 // 読む（未保存なら既定値 10）
GameSettings::commentFontSize.set(12);                            // 書く
const int fallback = GameSettings::commentFontSize.defaultValue(); // 宣言した既定値
GameSettings::commentFontSize.remove();                           // 保存値を消す（以後は既定値）
```

| メンバ | 内容 |
|---|---|
| `value()` / `operator()()` | 保存した値。未保存なら宣言の既定値 |
| `valueOr(fallback)` | 保存した値。未保存なら `fallback`。既定値が翻訳文や実行時の場所で決まる項目に使う |
| `set(v)` | 値を保存する |
| `remove()` | 保存した値を消す |
| `key()` | INI のキー |
| `defaultValue()` | 宣言した既定値。「既定値に戻す」ボタンや、範囲外の保存値を戻すときに使う |

読み書きはすべて `SettingsCommon::openSettings()` の共有 `QSettings` を通る。

### 既定値が実行時に決まる項目

宣言の既定値を空にしておき、読む側で `valueOr()` に渡す。

```cpp
// src/dialogs/startgamedialog_settings.cpp
namespace Saved = GameSettings::StartGame;
ui->lineEditHumanName1->setText(Saved::player1.humanName.valueOr(tr("You")));
ui->lineEditKifuSaveDir->setText(Saved::kifuSaveDir.valueOr(defaultKifuSaveDir()));  // ドキュメントフォルダ
```

### 同じ形の項目をまとめる

`Setting<T>` をメンバに持つ構造体を作り、インスタンスごとにキーを宣言できる。
対局開始ダイアログの対局者ごとの項目は `GameSettings::StartGame::Player` にまとめ、`player1`（先手／下手）と `player2`（後手／上手）を宣言している。

```cpp
Saved::player1.byoyomiSec.set(ui->byoyomiSec1->value());
const QString name1 = GameSettings::StartGame::player1.engineName();  // GameStartOrchestrator（ダイアログなしの開始）
```

`GameSettings::StartGame` の項目はダイアログ（`startgamedialog_settings.cpp`）のほか、`NyugyokuJudgement::configuredRule()`（`jishogiRule`）と `GameStartOrchestrator`（`player1/2.engineName`）も読む。

### 範囲を読む側で確かめる項目

`AppSettings::squareSize` は宣言だけで、範囲外の保存値は読む側（`ShogiView`）で既定値に戻している。

```cpp
int sq = AppSettings::squareSize();
if (sq < 20 || sq > 150) sq = AppSettings::squareSize.defaultValue();
```

---

## 4. 新しい設定項目の追加手順

CLAUDE.md の「Settings Update Guidelines」と同じ手順。

1. ドメイン別ヘッダーに `inline const Setting<T> name{"Group/key", 既定値};` を1行足す
2. 読むときは `NS::name()`、書くときは `NS::name.set(value)`、既定値は `NS::name.defaultValue()`。既定値が翻訳文や実行時の場所で決まる項目は、宣言の既定値を空にして `NS::name.valueOr(tr("..."))` で読む
3. ダイアログのコンストラクタで読み込み、デストラクタ・`done()`・`closeEvent` で保存する。ウィンドウサイズは `DialogUtils::restoreDialogSize` / `saveDialogSize`、A-/A+ は `FontSizeHelper` に宣言をそのまま渡す
4. 値の範囲を絞る、複数のキーを構造体にまとめる、配列で保存する、といった処理が要る項目だけを関数として `.cpp` に書く（[5章](#5-関数として書く項目)）

### 実装例

```cpp
// gamesettings.h
/// 〇〇ダイアログのウィンドウサイズ（デフォルト: 800x600）
inline const Setting<QSize> myDialogSize{"SizeRelated/myDialogSize", QSize(800, 600)};
/// 〇〇ダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> myDialogFontSize{"FontSize/myDialog", 10};

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

実際のコードでは `KifuPasteDialog`（`src/dialogs/kifupastedialog.cpp`）が同じ形になっている。

- `FontSizeHelper(const Setting<int>&, int min = 8, int max = 24, int step = 1)`（`src/common/fontsizehelper.h`）は、宣言から初期値を読んで範囲内に収め、`increase()` / `decrease()` で変わるたびに同じ宣言へ保存する。文字の反映（`applyFontSize()`）はダイアログ側で書く
- `DialogUtils::restoreDialogSize(QWidget*, const Setting<QSize>&)` は、幅・高さとも 100 を超える保存値だけを適用する。`DialogUtils::saveDialogSize(const QWidget*, const Setting<QSize>&)` は現在のサイズを保存する（`src/common/dialogutils.h`）

### INI キーの命名

INI ファイル内のキーは `グループ名/キー名` の形式で、宣言の第1引数がそのままキーになる。
キーを変えると、それまで保存していた値は読まれなくなる。

| 用途 | 既存の例 |
|---|---|
| ウィンドウサイズ | `SizeRelated/kifuPasteDialogSize`、`JosekiWindow/size`、`EngineSettings/dialogSize` |
| フォントサイズ | `FontSize/kifuPasteDialog`、`JosekiWindow/fontSize` |
| 表示状態 | `RecordPane/timeColumnVisible`、`EvalChartDock/visible` |
| 最後の選択 | `UI/lastSelectedTabIndex`、`MenuWindow/currentTab` |
| 最後に使ったフォルダ | `General/lastKifuDirectory`、`SfenCollection/lastDirectory` |

---

## 5. 関数として書く項目

宣言の表に書けないのは、保存・読み出しのときに処理が要る項目だけ。現在の関数は次のとおり。

| 処理 | 関数 |
|---|---|
| 値の範囲を絞る | `AppSettings::pieceSoundVolume`（0〜100）、`AnalysisSettings::usiLogCommandTarget`（0〜2）、`AppSettings::appearancePieceFilter`（0〜7）、`AppSettings::dialogFontSize`（8〜24） |
| 複数のキーを構造体にまとめる | `AppSettings::boardColors`、`AppSettings::boardVisuals`、`AppSettings::pieceSoundTone`、`TsumeshogiSettings::collectionPreferences` / `playPreferences` |
| 古い保存値の引き継ぎ・不正値の補正 | `AppSettings::appearanceSection`（旧版の番号を名前に読み替える）、`AppSettings::pieceStyle`（一覧にない値・廃止したセットは `standard` に戻す） |
| リスト・配列 | `AnalysisSettings::engineInfoColumnWidths` / `thinkingViewColumnWidths` / `kifuAnalysisColumnWidths`、`JosekiSettings::josekiWindowColumnWidths`、`DockSettings::savedDockLayoutNames` / `saveDockLayout` / `loadDockLayout` / `deleteDockLayout` |
| id ごとのキー | `AppSettings::dialogFontSize` / `auxiliaryDialogSize`（`DialogAppearance/<id>/fontSize`・`DialogAppearance/<id>/size`。個別の項目を持たない補助ダイアログ用） |

関数は namespace のドメイン別ヘッダーに宣言し、`.cpp` で `SettingsCommon::openSettings()` を使って実装する。キーは `settingskeys.h` の定数に置く（id から組み立てる `DialogAppearance/<id>/...` だけは関数の中に書いている）。

```cpp
// src/services/appsettings.cpp
int pieceSoundVolume()
{
    QSettings& s = SettingsCommon::openSettings();
    return qBound(0, s.value(SettingsKeys::kPieceSoundVolume, 30).toInt(), 100);
}

void setPieceSoundVolume(int percent)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kPieceSoundVolume, qBound(0, percent, 100));
}
```

範囲や形式を変える関数を足したら、範囲外の値・旧形式の値を含めて `tst_settings_roundtrip` に往復のテストを書く（`appSettings_pieceSoundVolume`・`appSettings_appearanceSection` など）。

---

## 6. QSettings を直接開く場合（配列・グループのデータ）

`openSettings()` が返すのはアプリ全体で1つの `QSettings` で、`beginGroup()`・`beginReadArray()` はその現在のグループを変える。
グループを開いている間に宣言の表の項目を読み書きすると、キーがグループの下にずれる。
そのため、配列やグループを開いたまま読み書きを続けるデータだけは、関数の中で設定ファイルを直接開く。

```cpp
QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
const int size = settings.beginReadArray(EnginesGroupName);
// ...
settings.endArray();
```

現在この形で開いているのは次の箇所だけ（12か所）。

| ファイル | 関数 | データ |
|---|---|---|
| `src/services/enginelistsettings.cpp` | `EngineListSettings::loadEngines()` | `[Engines]` 配列（登録エンジンの名前・パス・作者）の読み込み |
| `src/dialogs/engineregistrationhandler.cpp` | `saveEnginesToSettingsFile()`・`saveEngineOptionsToSettings()`・`removeEnginesGroup()`・`removeEngineNameGroup()` | 登録一覧と、エンジン名グループのオプション配列の書き込み・削除 |
| `src/dialogs/enginesettingsoptionhandler.cpp` | `readEngineOptions()`・`saveOptionsToSettings()` | エンジン設定ダイアログでのオプション配列の読み書き |
| `src/engine/usiprotocolhandler.cpp` | `loadEngineOptions()` | エンジン起動時にオプション配列を読み、`setoption` を組み立てる |
| `src/services/enginepondersettings.cpp` | `EnginePonderSettings::load()`・`save()` | エンジン名グループの GUI 先読み設定と `USI_Ponder` の保存値の同期 |
| `src/dialogs/csagamedialog_settings.cpp` | `loadServerHistory()`・`saveServerHistory()` | CSA通信対局の接続履歴 `CsaServerHistory` 配列（最大10件） |

列幅（`AnalysisSettings::engineInfoColumnWidths` など）とドックレイアウト名の一覧（`DockSettings::savedDockLayoutNames`）は、共有の `QSettings` で開いた配列・グループを同じ関数の中ですぐ閉じている。

単独の値を読み書きするために `QSettings` を直接開かない。宣言の表に1行足す。

---

## 7. エンジンの登録情報（EngineSettingsConstants）

エンジンの登録情報（名前、パス、作者、オプション）は配列で保存するため、宣言の表ではなく [6章](#6-qsettings-を直接開く場合配列グループのデータ) の方法で読み書きする。
INI キー名は `src/engine/enginesettingsconstants.h` に定数として定義されている。

```
[Engines]
1\name=YaneuraOu
1\path=/path/to/yaneuraou
1\author=...
size=1

[YaneuraOu]           ← エンジン名がグループ名（USIオプションの配列）
1\name=Threads
1\type=spin
1\default=...
1\min=...
1\max=...
1\value=4
1\valueList=...
size=...
gui\ponderEnabled=true
gui\sendUnreportedPonderOption=true
```

登録一覧を読むときは `EngineListSettings::loadEngines()` を使う。名前とパスの変換は `pathForName()` / `nameForPath()`。

```cpp
#include "enginelistsettings.h"

const QList<EngineListSettings::EngineEntry> engines = EngineListSettings::loadEngines();
const int index = EngineListSettings::indexOfName(engines, engineName);
const QString path = index >= 0 ? engines.at(index).path : QString();
```

### 利用箇所

| 用途 | ファイル |
|---|---|
| 登録一覧の読み込み（`loadEngines()`） | `startgamedialog.cpp`、`kifuanalysisdialog.cpp`、`csagamedialog.cpp`、`tsumeshogisearchdialog.cpp`、`tsumeshogigeneratordialog.cpp`、`tsumecollectiondialog.cpp`、`considerationtabmanager.cpp`、`considerationflowcontroller.cpp`、`engineregistrationhandler.cpp`、`automation/enginecatalog.cpp` |
| 登録一覧とオプションの書き込み・削除 | `engineregistrationhandler.cpp`（エンジン登録ダイアログ） |
| オプションの読み書き | `enginesettingsoptionhandler.cpp`（エンジン設定ダイアログ） |
| 起動時のオプション送信 | `usiprotocolhandler.cpp`（`loadEngineOptions()`。`USI_Ponder` は除き、GUI の先読み設定から初期化時に一度だけ送る） |

### 機能ごとに選んだエンジン

検討・棋譜解析・詰み探索・詰将棋生成で選んだエンジンは、`EngineListSettings::saveSelection()` で名前（id name）を保存し、`restoreSelection()` で一覧の位置に戻す。
保存場所は `considerationSelection()`・`kifuAnalysisSelection()`・`tsumeSearchSelection()`・`tsumeshogiGeneratorSelection()` が返す（`Consideration/engineName` など）。
以前の版が保存した番号（`*/engineIndex`）やパス（`TsumeSearch/enginePath`）は、名前で見つからないときだけ読み、保存時に消す。

### 先読みのGUI設定

先読みのGUI設定は `EnginePonderSettings` が管理し、エンジン名グループの下に次のキーを保存する。エンジン削除時は同じグループとともに削除される。

| キー | 用途・既定値 |
|---|---|
| `gui/ponderEnabled` | GUIが先読みを許可するか。未保存なら既存の `USI_Ponder` の値を引き継ぎ、それもなければ `false` |
| `gui/sendUnreportedPonderOption` | 未報告の予約オプション `USI_Ponder` にも設定を通知するか。既定値 `true` |

GUI設定を保存すると、既存の `USI_Ponder` オプションの保存値も同期する。未報告エンジンに架空のオプション行は追加しない。「既定値に戻す」はエンジンが登録時に報告した既定値（未報告なら無効）と、通知有効を復元する。
初期化時は `isready` より前に `USI_Ponder` を一度だけ通知する。通知を省略する設定でも、オプションが報告されている場合は送信する。GUI側の先読み許可は通知可否に依存せず、実際の先読み開始には `bestmove` の予測応手が必要となる。

---

## 8. テスト

`tests/tst_settings_roundtrip.cpp` が設定の保存→復元を確かめる。`initTestCase()` で `QStandardPaths::setTestModeEnabled(true)` を呼び、テスト用の設定ファイルを消してから始める。

| テスト | 内容 |
|---|---|
| `declaredDefaultsBeforeSaving` | 未保存の項目が宣言した既定値を返すこと（保存する前に実行する） |
| `settingWritesDeclaredKeyAndRemoveRestoresDefault` | `set()` が宣言したキーへ書き、`remove()` で既定値へ戻ること |
| `appSettings_*`・`analysisSettings_*`・`gameSettings_*` など | ドメインごとの往復。関数として書いた項目は範囲外の値・旧形式の値も確かめる |

ほかのテストで設定ファイルを使うときは、設定に触れる前に `SHOGIBOARDQ_CONFIG_HOME` を一時ディレクトリに向ける（`tst_startgamedialog`・`tst_nyugyoku_judgement`・`tst_enginepondersettings` など）。

---

## 9. 旧バージョンからの移行

以前のバージョンでは設定ファイルが実行ファイルと同じディレクトリに配置されていた。
新バージョンでは `QStandardPaths` ベースの場所に変更されたため、旧ファイルは自動的には読み込まれない。

### 手動移行

旧ファイルを新しい場所にコピーする。

```bash
# Linux
cp /path/to/ShogiBoardQ/ShogiBoardQ.ini ~/.config/ShogiBoardQ/ShogiBoardQ.ini

# macOS
cp /path/to/ShogiBoardQ.app/../ShogiBoardQ.ini ~/Library/Preferences/ShogiBoardQ/ShogiBoardQ.ini

# Windows
copy C:\path\to\ShogiBoardQ\ShogiBoardQ.ini %LOCALAPPDATA%\ShogiBoardQ\ShogiBoardQ.ini
```
