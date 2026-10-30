# 設定ファイルと設定項目の宣言

ShogiBoardQ のアプリケーション設定の永続化に関するガイド。

設定項目は `Setting<T>`（`src/services/setting.h`）で**キーと既定値を1行で宣言**し、読み書きはその宣言を通す。
getter と setter の組やキー定数は手で書かない。値の範囲を絞る項目・構造体を複数のキーに保存する項目・配列で保存する項目も、
`BoundedSetting<T>`・`StructSetting<T>`・`ListSetting<T>`・`StructListSetting<T>`（同じヘッダー）で宣言する。
これらで表せない処理（以前の版の値の引き継ぎ、ほかの項目への代替など）が要る項目だけを関数として書く。

---

## 目次

1. [設定ファイルの保存先](#1-設定ファイルの保存先)
2. [構成](#2-構成)
3. [設定の読み書き](#3-設定の読み書き)
4. [新しい設定項目の追加手順](#4-新しい設定項目の追加手順)
5. [関数として書く項目](#5-関数として書く項目)
6. [QSettings を直接開く場合（配列・グループのデータ）](#6-qsettings-を直接開く場合配列グループのデータ)
7. [エンジンの登録情報（EngineProfileStore）](#7-エンジンの登録情報engineprofilestore)
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
| `src/services/setting.h` | `Setting<T>`。設定ファイルの1項目（キーと既定値）を表し、読み書きの入口になる。範囲付きの `BoundedSetting<T>`、構造体の `StructSetting<T>`（項目は `SettingField<T>`）、配列の `ListSetting<T>`・`StructListSetting<T>` も置く（[3章](#範囲付き構造体配列の宣言)） |
| `src/services/settingscommon.h/.cpp` | 設定ファイルのパス（`settingsFilePath()`）、アプリ全体で共有する `QSettings`（`openSettings()`）、全設定の消去（`resetAllSettings()`） |
| ドメイン別ヘッダー（下表） | 設定項目の宣言の表。処理が要る項目の関数宣言もここに置く |
| `src/services/settingskeys.h` | 関数として書いた項目と、機能ごとのエンジン選択（`EngineListSettings`）が使うキー定数。宣言の表の項目はキーを宣言に直接書くので、新しい項目のためにここへ定数を足さない |
| `src/services/engineprofilestore.h/.cpp` | 登録エンジンの設定（一覧・USIオプション・先読みの設定）の保存形式を知る唯一の場所（[7章](#7-エンジンの登録情報engineprofilestore)） |
| `src/services/enginelistsettings.h/.cpp` | 登録エンジン一覧の検索（名前・パス）と、機能ごとに選んだエンジンの保存（[7章](#7-エンジンの登録情報engineprofilestore)） |
| `src/services/engineoption.h` | USIオプション1つの値 `EngineOption`（名前・型・既定値・範囲・現在値・選択肢） |

### ドメイン別ヘッダー

宣言だけの項目は `inline const` なのでヘッダーだけで完結する。`.cpp` は関数として書いた項目がある namespace にだけある。

| namespace | ファイル | 主な内容 |
|---|---|---|
| `AppSettings` | `appsettings.h` | 言語・指し手の表記、GUI共通フォント、UI状態、駒音、メニューウィンドウ、メインウィンドウのサイズと盤のマスの大きさ、補助ダイアログの文字サイズ・大きさ（id ごと） |
| `AppearanceSettings` | `src/style/appearancesettings.h/.cpp` | 対局画面の外観（駒のセット・盤の配色・木目と駒の影と大きさ）と外観ダイアログ（大きさ・開いていたタブ・駒の絞り込み）。値の型（`BoardColors`・`BoardVisuals`・`PieceStyleCatalog`）と同じ style の層に置く。読み書きして変更を知らせるのは `BoardAppearance` |
| `GameSettings` | `gamesettings.h` | 棋譜ファイルのディレクトリ、棋譜欄、コメント・対局情報タブ、対局開始ダイアログ（`GameSettings::StartGame`）、棋譜貼り付け、局面集、持将棋の点数ダイアログ |
| `AnalysisSettings` | `analysissettings.h/.cpp` | 分岐ツリー、評価値グラフ、解析タブ（列幅・USIログ・思考タブ）、棋譜解析、検討、詰み探索、読み筋盤面 |
| `NetworkSettings` | `networksettings.h` | CSA通信対局ダイアログ（対局者・接続先・詳細設定・接続先の履歴 `csaServerHistory`）、待機ダイアログ、通信ログ |
| `JosekiSettings` | `josekisettings.h` | 定跡ウィンドウ、定跡手編集・定跡マージダイアログ |
| `TsumeshogiSettings` | `tsumeshogisettings.h/.cpp` | 詰将棋局面生成ダイアログ、詰将棋の問題一覧・対局画面 |
| `EngineDialogSettings` | `enginedialogsettings.h` | エンジン設定・エンジン登録ダイアログ |
| `DockSettings` | `docksettings.h/.cpp` | ドック1つの状態 `DockState`（フローティング・表示状態・ジオメトリ。全12個のドックを `dockState(group, defaultVisible)` / `setDockState(group, state)` で読み書き）、ドックの固定、名前付きレイアウト |

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
| `isSaved()` | 値が保存されているか（以前の版のキーが残っているかを確かめるときなどに使う） |
| `remove()` | 保存した値を消す |
| `key()` | INI のキー |
| `defaultValue()` | 宣言した既定値。「既定値に戻す」ボタンや、範囲外の保存値を戻すときに使う |

読み書きはすべて `SettingsCommon::openSettings()` の共有 `QSettings` を通る。

### 範囲付き・構造体・配列の宣言

`Setting<T>` の1行で書けない項目も、`setting.h` の次の型で1つの宣言にする。どれも読むのは `NS::name()`、書くのは `NS::name.set(value)`。

| 型 | 保存の形 | 使っている宣言 |
|---|---|---|
| `BoundedSetting<T>(キー, 既定値, 最小, 最大)` | 1つのキー。読みも書きも `qBound` で範囲に収める（`minimum()`・`maximum()`） | `AppSettings::pieceSoundVolume`（0〜100）、`AppearanceSettings::appearancePieceFilter`（0〜`PieceStyleCatalog::Family::Count` − 1。0 は「すべて」）、`AnalysisSettings::usiLogCommandTarget`（0〜2）、`AppSettings::menuWindowButtonSize`（48〜120）、`AnalysisSettings::branchTreeZoomPercent`（50〜200）。画面は範囲の定数を持たず、`minimum()`・`maximum()` を読む |
| `StructSetting<T>({項目...}, 整える関数)` | 構造体のメンバごとに1つのキー。項目は `SettingField<T>::of(キー, &T::メンバ)`。保存していない項目は `T{}` の既定値のまま。整える関数（省略可）は読んだ値と書く値の両方に掛ける。項目ごとの保存の有無は `StructSetting<T>::isSaved(キー)` | `AppSettings::pieceSoundTone`（`PieceSoundTone::clamped` で範囲に収める）、`TsumeshogiSettings::playPreferences`・`collectionPreferences`、`AnalysisSettings::evalChart`・`consideration`（検討の条件 `ConsiderationSettings`。エンジンの名前は保存しない） |
| `ListSetting<T>(配列のキー, 要素のキー)` | QSettings の配列（`<配列のキー>/size` と `<配列のキー>/<番号>/<要素のキー>`）。配列のキーは実行時に作ってもよい | `JosekiSettings::josekiWindowColumnWidths`、`AnalysisSettings::engineInfoColumnWidths(ThinkingTable)`・`thinkingViewColumnWidths(ThinkingTable)`（表の番号でキーを作った宣言を返す関数）、`kifuAnalysisColumnWidths()`・`GameSettings::gameInfoColumnWidths()`（以前の版の形を移してから宣言を返す）。列幅は widgets の `ColumnWidthMemory` が読み書きする（0 は自動、正の値は利用者が決めた幅） |
| `StructListSetting<T>(配列のキー, {項目...})` | 構造体の配列。要素ごとに `SettingField<T>` の項目を書く | `NetworkSettings::csaServerHistory`（`CsaServerHistoryEntry` の `host`・`port`・`useTls`・`id`。新しい順で最大 `kCsaServerHistoryLimit` = 10 件。件数を絞るのは `CsaGameDialog`） |

```cpp
// src/services/appsettings.h
inline const BoundedSetting<int> pieceSoundVolume{"Sound/pieceSoundVolume", 30, 0, 100};
inline const StructSetting<PieceSoundTone> pieceSoundTone{
    {SettingField<PieceSoundTone>::of("Sound/pieceSoundPitch", &PieceSoundTone::pitchSemitones),
     SettingField<PieceSoundTone>::of("Sound/pieceSoundEqLow", &PieceSoundTone::lowDb),
     /* ... */},
    &PieceSoundTone::clamped};

// 使う側
AppSettings::pieceSoundVolume.set(percent);              // 範囲外なら 0〜100 に収めて保存
const PieceSoundTone tone = AppSettings::pieceSoundTone();
```

`ListSetting`・`StructListSetting` は共有の `QSettings` で配列を開き、同じ呼び出しの中で閉じる（[6章](#6-qsettings-を直接開く場合配列グループのデータ)）。

### 既定値が実行時に決まる項目

宣言の既定値を空にしておき、読む側で `valueOr()` に渡す。

```cpp
// src/widgets/playersetupwidget.cpp（m_saved は GameSettings::StartGame::player1 か player2）
m_humanName->setText(m_saved.humanName.valueOr(tr("You")));
// src/dialogs/startgamedialog.cpp（SettingsFormBinding に既定の文字の関数を渡す。ドキュメントフォルダ）
m_form.add(ui->lineEditKifuSaveDir, Saved::kifuSaveDir, &defaultKifuSaveDir);
```

### 同じ形の項目をまとめる

`Setting<T>` をメンバに持つ構造体を作り、インスタンスごとにキーを宣言できる。キーは実行時に組み立ててよい（`Setting`・`SettingField` はキーの写しを持つ）ので、
グループ名や番号だけ違う組は、キーを組み立てて宣言を返す関数を1つ書き、組ごとにその関数で宣言する。
対局開始ダイアログの対局者ごとの項目は `GameSettings::StartGame::Player` にまとめ、`playerSettings('1', true)` で `player1`（先手／下手）、`playerSettings('2', false)` で `player2`（後手／上手）を宣言している。
定跡を使う条件（`StructSetting<JosekiPlayOptions>`）も `JosekiSettings::playOptionsSetting("JosekiPlay1")`・`("JosekiPlay2")` で宣言する。

```cpp
// src/dialogs/startgamedialog.cpp: saveTime(const Saved::Player& saved, ...) は player1・player2 を同じ関数で書く
saved.byoyomiSec.set(static_cast<int>(time.byoyomiMs / kSecondMs));
```

対局者の欄（`PlayerSetupWidget`）は、作るときにその対局者の `Player` を受け取り、人間かエンジンか（`isHuman`。旧版との互換のため `isEngine` も書く）・名前（`humanName`）・エンジン（`engineName`）を読み書きする。
エンジンは名前で保存する（`EngineSelector` の保存先に `engineName` を渡す）。名前が無い以前の版の値だけ、`engineNumber`・`enginePath` を読む。保存したエンジンが削除されていれば人間に戻す。
時間（`basicTimeHour`・`basicTimeMinutes`・`byoyomiSec`・`addEachMoveSec`）は `Player::time()`・`setTime()`・`defaultTime()`（`gamesettings.cpp`）が `TimeControlSide` との間で変換して読み書きする。

開始局面は手合割の名前（`Handicaps` の名前）で `startingPositionName` に保存する。空なら現在の局面。
以前の版は、以前の一覧の並びの番号（0=現在の局面、1=平手 … 14=十枚落ち）を `startingPositionNumber` に保存していた。
`GameSettings::StartGame::startingPosition()` はこのキーが残っていれば先にそれを読み、`setStartingPosition()` は名前で保存してこのキーを消す（`gamesettings.cpp`）。ダイアログはこの2つを使う。

`GameSettings::StartGame` の項目はダイアログ（`startgamedialog.cpp`・`playersetupwidget.cpp`）のほか、`NyugyokuJudgement::configuredRule()`（`jishogiRule`）も読む。

### 入力欄と設定の対応の表（SettingsFormBinding）

ダイアログの入力欄ごとに読み込み・保存・既定値に戻すを3回書かず、`SettingsFormBinding`（`src/widgets/settingsformbinding.h`）に欄と宣言を1度だけ `add()` する。
`load()` は保存した値を入れ、`save()` は欄の値を保存し、`reset()` は宣言の既定値を入れる（保存はしない）。

| 欄 | 宣言の型 |
|---|---|
| `QSpinBox` | `Setting<int>` |
| `QAbstractButton`（チェックボックス・ラジオボタン）・`QGroupBox`（チェックできるグループ） | `Setting<bool>` |
| `QComboBox`（選んだ位置） | `Setting<int>` |
| `QLineEdit` | `Setting<QString>`（既定の文字の関数を渡せる。未保存のときと既定値に戻すときに使う） |

```cpp
// src/dialogs/startgamedialog.cpp
m_form.add(ui->spinBoxMaxMoves, Saved::maxMoves);
m_form.add(ui->checkBoxAutoSaveKifu, Saved::autoSaveKifu);
m_form.load();
```

対局開始ダイアログ（対局者・時間・開始局面以外の欄）と詰将棋生成ダイアログ（生成の条件の欄と表示の欄）が使う。詰将棋生成ダイアログの「既定値に戻す」は、生成の条件の表の `reset()` で宣言の既定値に戻す。

### 盤のマスの大きさ

盤のマスの大きさの範囲（20〜150、既定 50）は core の `BoardSquareSize` だけで決め、`AppSettings::squareSize`・`GameSettings::sfenCollectionSquareSize` は `BoundedSetting`、詰将棋を解く画面の `TsumeshogiSettings::PlayPreferences::squareSize` は整える関数（`normalizedOf`）でその範囲に収める。CLI の `render-board` の `square_size` も同じ範囲を使う。`ShogiView` は設定を読まず、本体の盤の大きさは `MainWindowAppearanceController` が読み書きする（拡大・縮小のたびに保存する）。

---

## 4. 新しい設定項目の追加手順

CLAUDE.md の「Settings Update Guidelines」と同じ手順。

1. ドメイン別ヘッダーに `inline const Setting<T> name{"Group/key", 既定値};` を1行足す
2. 読むときは `NS::name()`、書くときは `NS::name.set(value)`、既定値は `NS::name.defaultValue()`。既定値が翻訳文や実行時の場所で決まる項目は、宣言の既定値を空にして `NS::name.valueOr(tr("..."))` で読む
3. ダイアログのコンストラクタで読み込み、デストラクタ・`done()`・`closeEvent` で保存する。ウィンドウサイズは `DialogUtils::restoreDialogSize` / `saveDialogSize`（または `DialogFontScale::Options::size`）、A-/A+ は `DialogFontScale::install(this, 宣言, options)` に宣言をそのまま渡す
4. 値の範囲を絞る項目は `BoundedSetting<T>`、構造体を項目ごとのキーに保存する項目は `StructSetting<T>`、配列で保存する項目は `ListSetting<T>`・`StructListSetting<T>` で宣言する（[3章](#範囲付き構造体配列の宣言)）。これらで表せない処理（以前の版の値の引き継ぎ・ほかの項目への代替・独自の保存形式など）が要る項目だけを関数として `.cpp` に書く（[5章](#5-関数として書く項目)）

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
{
    setupUi();

    // 文字の大きさ（初期値の読み込みと変更のたびの保存）。A-/A+ を渡さなければ決定ボタンの列に作る
    DialogFontScale::Options fontOptions;
    fontOptions.decrease = ui->buttonFontDecrease;
    fontOptions.increase = ui->buttonFontIncrease;
    DialogFontScale::install(this, GameSettings::myDialogFontSize, fontOptions);

    DialogUtils::restoreDialogSize(this, GameSettings::myDialogSize);
}

MyDialog::~MyDialog()
{
    DialogUtils::saveDialogSize(this, GameSettings::myDialogSize);
}
```

実際のコードでは `KifuPasteDialog`（`src/dialogs/kifupastedialog.cpp`）が同じ形になっている。

- `DialogFontScale::install(QDialog*, const Setting<int>&, const Options&)`（`src/widgets/dialogfontscale.h`）は、宣言から初期値を読んで `Options` の `minSize`〜`maxSize`（既定 8〜24）に収め、A-/A+ で変わるたびに同じ宣言へ保存する。`Options::size` を渡すと、ダイアログの大きさも開くときに戻し、閉じるときに保存する。文字の大きさの宣言の既定値が 0 なら、未保存のときはダイアログの元の大きさから始める。各部品は元の大きさからの差で拡大・縮小するので、見出しの相対の大きさは作るときに1回決めればよい。大きさに合わせた追加の調整は、返す部品の `fontSize()`（`FontSizeHelper*`）の `fontSizeChanged` につなぐ。ダイアログに `FontSizeHelper` のメンバや `applyFontSize()` は書かない
- ドックのパネルは `FontSizeHelper(const Setting<int>&, int min = 8, int max = 24, int step = 1)`（`src/widgets/fontsizehelper.h`）を持ち、A-/A+ のボタンを `FontSizeControl`（`src/widgets/fontsizecontrol.h`）で結ぶ。A-/A+ の文字・見た目・説明と、範囲の端で押せなくする処理は `FontSizeControl` だけが決める。文字の反映は `fontSizeChanged` をパネルの表示の作り直しにつなぐ（定跡ウィンドウのようにウィンドウ全体に付けるときは `applyTo(window)`、つまり `DialogUtils::applyFontSize`）
- `DialogUtils::restoreDialogSize(QWidget*, const Setting<QSize>&)` は、幅・高さとも 100 を超える保存値だけを適用する。`DialogUtils::saveDialogSize(const QWidget*, const Setting<QSize>&)` は現在のサイズを保存する（`src/widgets/dialogutils.h`）

### INI キーの命名

INI ファイル内のキーは `グループ名/キー名` の形式で、宣言の第1引数がそのままキーになる。
キーを変えると、それまで保存していた値は読まれなくなる。

| 用途 | 既存の例 |
|---|---|
| ウィンドウサイズ | `SizeRelated/kifuPasteDialogSize`、`JosekiWindow/mergeDialogSize`、`EngineSettings/dialogSize` |
| フォントサイズ | `FontSize/kifuPasteDialog`、`JosekiWindow/fontSize`、`DialogAppearance/promotion/fontSize`（補助ダイアログ） |
| 表示状態 | `RecordPane/timeColumnVisible`、`EvalChartDock/visible` |
| 最後の選択 | `UI/lastSelectedTabIndex`、`MenuWindow/currentTab` |
| 最後に使ったフォルダ | `General/lastKifuDirectory`、`SfenCollection/lastDirectory` |

---

## 5. 関数として書く項目

宣言の表（`Setting<T>` と [3章](#範囲付き構造体配列の宣言) の型）に書けないのは、保存・読み出しのときにそれ以上の処理が要る項目だけ。現在の関数は次のとおり。

| 処理 | 関数 |
|---|---|
| 古い保存値の引き継ぎ・不正値の補正 | `AppearanceSettings::appearanceSection`（外観ダイアログのタブの id を文字列で返す。旧版のタブの番号は id に読み替える）、`AppearanceSettings::pieceStyle`（`PieceStyleCatalog` にない値・廃止したセットは `standard` に戻す）、`EngineListSettings::savedSelectionName`（詰将棋問題一覧の判定エンジンの以前の版の `"@hayanagi"` を空の名前として読む）、`GameSettings::StartGame::startingPosition` / `setStartingPosition`（以前の版の開始局面の番号を読み、保存するときに消す）、`AnalysisSettings::kifuAnalysisColumnWidths`（1つのキーに `QVariantList` で保存していた値を配列に移す）、`GameSettings::gameInfoColumnWidths`（項目名の列幅 `GameInfo/keyColumnWidth` を配列に移す） |
| 保存の形と値の形の換算 | `GameSettings::StartGame::Player::time` / `setTime` / `defaultTime`（持ち時間を時間・分・秒の項目に保存し、`TimeControlSide` のミリ秒で読み書きする） |
| 未保存の項目をほかの項目で代える | `AppearanceSettings::boardVisuals` / `setBoardVisuals`（中身は `StructSetting<BoardVisuals>`。駒台の木目 `standWoodGrain` が未保存なら、それを分ける前の版の将棋盤の木目 `woodGrain` を使う） |
| 独自の保存形式 | `AppearanceSettings::boardColors` / `setBoardColors`（色を `#RRGGBB`・`#AARRGGBB` の文字列で保存し、不正な色は標準色に戻す）、`DockSettings::dockState` / `setDockState`（グループ名を引数で受ける。キーは `<グループ>/floating`・`/visible`・`/geometry`。グループは `DockRegistry` の表の `settingsGroup`。表示の既定値も引数で受ける） |
| キーを実行時に作る | `AnalysisSettings::engineInfoColumnWidths` / `thinkingViewColumnWidths`（表 `ThinkingTable` の番号でキーを作った `ListSetting<int>` の宣言を返す） |
| グループの中の名前の一覧 | `DockSettings::savedDockLayoutNames` / `saveDockLayout` / `loadDockLayout` / `deleteDockLayout` |

関数は namespace のドメイン別ヘッダーに宣言し、`.cpp` で `SettingsCommon::openSettings()` を使って実装する。キーは `settingskeys.h` の定数に置く（`AppearanceSettings` のキーは `appearancesettings.cpp` の中）。

補助ダイアログ（成り・しおり・駒音・メッセージボックスなど）の文字の大きさと大きさも、関数ではなく `appsettings.h` の宣言の表に1行ずつ書く（`AppSettings::promotionDialogFontSize`・`promotionDialogSize`・`messageBoxFontSize`・`bookmarkDialogFontSize` など）。キーは `DialogAppearance/<id>/fontSize`（既定値 0）・`DialogAppearance/<id>/size`（既定値 `QSize()`）で、以前の版と同じ。既定値 0 は「未保存ならダイアログの元の大きさから始める」の意味。

```cpp
// src/style/appearancesettings.cpp（宣言の型を使い、代替の処理だけを関数に書く）
BoardVisuals boardVisuals()
{
    BoardVisuals visuals = visualsSetting()();   // StructSetting<BoardVisuals>
    // 駒台の木目を分ける前の版は、将棋盤の木目の設定だけを保存していた
    if (!StructSetting<BoardVisuals>::isSaved(kStandWoodGrain)) visuals.standWoodGrain = visuals.woodGrain;
    return visuals;
}
```

範囲や形式を変える宣言・関数を足したら、範囲外の値・旧形式の値を含めて `tst_settings_roundtrip` に往復のテストを書く（`appSettings_pieceSoundVolume`・`appearanceSettings_boardVisuals`・`appearanceSettings_appearanceSection` など）。

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

現在この形で開いているのは次のファイルだけ。

| ファイル | 関数 | データ |
|---|---|---|
| `src/services/engineprofilestore.cpp` | `EngineProfileStore::loadEngines()`・`saveEngines()`・`loadOptions()`・`saveOptions()`・`removeEngine()`・`loadPonder()`・`savePonder()` | 登録一覧の `[Engines]` 配列と、エンジンごとのグループ（`EngineOptions/<名前>`）のオプション配列・GUI 先読み設定（[7章](#7-エンジンの登録情報enginesettingsconstants)） |

以前の版のエンジン名の最上位グループからの移行（`EngineProfileStore::migrateLegacyOptionGroups()`、起動時）は共有の `QSettings` を使う。

`ListSetting<T>`・`StructListSetting<T>`（列幅の `AnalysisSettings::engineInfoColumnWidths`・`JosekiSettings::josekiWindowColumnWidths`、CSA通信対局の接続先の履歴 `NetworkSettings::csaServerHistory` など）とドックレイアウト名の一覧（`DockSettings::savedDockLayoutNames`）は、共有の `QSettings` で開いた配列・グループを同じ呼び出しの中ですぐ閉じている。

単独の値を読み書きするために `QSettings` を直接開かない。宣言の表に1行足す。

---

## 7. エンジンの登録情報（EngineProfileStore）

エンジンの登録情報（名前、パス、作者、オプション）は配列で保存するため、宣言の表ではなく [6章](#6-qsettings-を直接開く場合配列グループのデータ) の方法で読み書きする。
この保存形式を知るのは `EngineProfileStore`（`src/services/engineprofilestore.h`）だけで、ほかの部品は設定ファイルを開かずにその関数を使う。
INI のキー名も `engineprofilestore.cpp` の中だけに置く（テストが設定ファイルを直接準備するときは `tests/engine_profile_format_helper.h` の語を使う）。オプションの型を表す語（`spin`・`check` など）は `EngineOption::kTypeSpin` などで、USI の受信行の解釈・設定ダイアログと共有する。

```
[Engines]
1\name=YaneuraOu
1\path=/path/to/yaneuraou
1\author=...
size=1

[EngineOptions]       ← エンジンごとの設定（グループ名は engineOptionsGroup(名前)）
YaneuraOu\1\name=Threads
...
```

エンジンごとの設定は `EngineOptions/<名前>` のグループに置く。
名前の `%`・`/`・`\` は `SettingsCommon::escapeKey()` でエスケープするので（ドックのレイアウト名と同じ）、`General` や `/` を含む名前でもほかの設定と衝突しない。
グループの中身は次のとおり（INI では `[EngineOptions]` の下に `YaneuraOu\1\name=...` の形で並ぶ）。

```
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

| 関数 | 内容 |
|---|---|
| `loadEngines()` / `saveEngines()` | 登録一覧の読み込みと置き換え（保存は一覧全体を書き直す） |
| `loadOptions()` / `saveOptions()` | オプション配列の読み込みと置き換え。保存は `valueList` を含む全行を書き、古い行は消し、`gui/...` は残す。`valueList` は JSON の配列で書き、読むときは以前の版の空白区切りも受け付ける |
| `removeEngine()` | エンジンごとのグループ（オプションと GUI 設定）を消す |
| `loadPonder()` / `savePonder()` | 先読みの GUI 設定（`PonderPreferences`: `enabled`・`sendUnreportedOption`・`defaultEnabled`）。下の「先読みのGUI設定」 |
| `launchOptions(名前)` | 起動時に送る設定 `LaunchOptions`（`USI_Ponder` を除いたオプション。button 型は値が `on` のものだけ。と、先読みの可否・未報告の `USI_Ponder` を送るか） |
| `migrateLegacyOptionGroups()` | 以前の版の最上位グループからの移行（下記） |

予約オプションの名前は `kPonderOptionName`、判定は `isPonderOption()`。

以前の版はエンジン名をそのまま最上位のグループ名にしていた。GUI と `shogiboardq-cli` は起動時に
`EngineProfileStore::migrateLegacyOptionGroups()` を呼び、登録済みエンジンの配列（`size` と番号付きのキー）と
`gui/...` の設定だけを `EngineOptions` の下へ移す（同じ名前のグループにあるほかの設定には触れない）。

登録一覧を読むときは `EngineListSettings::loadEngines()`（`EngineProfileStore::loadEngines()` に任せる）を使う。名前とパスの変換は `pathForName()` / `nameForPath()`、一覧の中の位置は `indexOfName()` / `indexOfPath()`。

```cpp
#include "enginelistsettings.h"

const QList<EngineListSettings::EngineEntry> engines = EngineListSettings::loadEngines();
const int index = EngineListSettings::indexOfName(engines, engineName);
const QString path = index >= 0 ? engines.at(index).path : QString();
```

### 利用箇所

| 用途 | ファイル |
|---|---|
| 登録一覧の読み込み（`loadEngines()`） | `widgets/engineselector.cpp`（下の「機能ごとに選んだエンジン」）、`considerationuicontroller.cpp`、`engineregistrationhandler.cpp`、`automation/enginecatalog.cpp` |
| 登録一覧とオプションの書き込み・削除 | `engineregistrationhandler.cpp`（エンジン登録ダイアログ。`EngineProfileStore::saveEngines()`・`saveOptions()`・`removeEngine()`） |
| オプションの読み書き | `enginesettingsoptionhandler.cpp`（エンジン設定ダイアログ。`readEngineOptions()` が `loadOptions()`、`writeEngineOptions()` が一覧全体を `saveOptions()` で書く） |
| 起動時のオプション送信 | `usienginesession.cpp` の `startAsync()` が `EngineProfileStore::launchOptions()` を `UsiProtocolHandler::setLaunchOptions()` に渡す。`UsiProtocolHandler` は設定ファイルを開かない（`USI_Ponder` は除き、GUI の先読み設定から初期化時に一度だけ送る） |

### 機能ごとに選んだエンジン

機能ごとに選んだエンジンは、`EngineListSettings::saveSelection()` で名前（id name）を保存し、`restoreSelection()` で一覧の位置に戻す。
保存場所は機能ごとの関数が `SelectionKeys`（名前のキーと、以前の版の番号・パスのキー）で返す。

| 機能 | 関数 | 名前のキー | 以前の版のキー（読むだけ） |
|---|---|---|---|
| 検討 | `considerationSelection()` | `Consideration/engineName` | `Consideration/engineIndex` |
| 棋譜解析 | `kifuAnalysisSelection()` | `KifuAnalysis/engineName` | `KifuAnalysis/engineIndex` |
| 詰み探索 | `tsumeSearchSelection()` | `TsumeSearch/engineName` | `TsumeSearch/enginePath`（パス） |
| 詰将棋生成 | `tsumeshogiGeneratorSelection()` | `TsumeshogiGenerator/engineName` | `TsumeshogiGenerator/engineIndex` |
| CSA通信対局 | `csaGameSelection()` | `CsaGameSettings/engineName` | `CsaGameSettings/engineNumber` |
| 詰将棋問題一覧の判定 | `tsumeCollectionSelection()` | `TsumeCollection/engineName`（内蔵の Hayanagi なら空の文字列） | `TsumeCollection/engine`（パス） |
| 対局開始の対局者 | `GameSettings::StartGame::player1`・`player2` の宣言（`PlayerSetupWidget` が `SelectionKeys` にする） | `GameSettings/engineName1`・`engineName2` | `GameSettings/engineNumber1`・`engineNumber2`（番号）、`enginePath1`・`enginePath2`（パス） |

キーは `settingskeys.h` の定数（`kCsaEngineName`・`kCsaEngineNumber`・`kTsumeCollectionEngineName` など）に置く。
CSA通信対局のキーは以前の版と同じで、`NetworkSettings` には宣言しない。
以前の版のキーは、名前で見つからないときだけ読み、保存時に消す。
`savedSelectionName(keys)` は保存した名前を返す（保存していなければ `std::nullopt`。空の文字列は登録エンジン以外の項目を選んだ印）。

これらの機能の画面は、コンボ・「設定」ボタン・未登録の案内を `EngineSelector`（`src/widgets/engineselector.h`）に結び、`Options::keys` に上の関数の値を渡す。
`EngineSelector` は作るときに保存した選択を戻し（保存が無ければ先頭の登録エンジン）、`saveSelection()` で保存する。
検討タブも、設定の正本 `ConsiderationSettings` を保存するときにエンジンは `EngineSelector::saveSelection()` で保存する（時間と候補手の数は `AnalysisSettings::consideration`）。
対局開始ダイアログ（`StartGameDialog`）も、対局者の欄（`PlayerSetupWidget`）が `EngineSelector` を使う。コンボの先頭の内蔵の項目を「人間」にし、保存先は `GameSettings::StartGame::player1`・`player2` の `engineName`・`engineNumber`・`enginePath` である。

詰将棋問題一覧の判定エンジンは、内蔵の Hayanagi の項目（`Options::builtinLabel`）と、名前に KomoringHeights を含む登録エンジンだけを出す。
以前の版は `TsumeCollection/engine` にパスを保存し、内蔵の Hayanagi は `"@hayanagi"` と書いていた。
保存場所 `EngineListSettings::tsumeCollectionSelection()` は、以前の版のパスのキーに書いた内蔵の印（`SelectionKeys::legacyBuiltinPath` = `"@hayanagi"`）を持ち、
名前が保存されていなければ `savedSelectionName()` がそれを空の名前（内蔵）として返す。パスは以前の版のパスのキーとして `restoreSelection()` が読む。
どちらも読むだけで、選び直して保存したときに名前を書き、以前の版のキーを消す。

### 先読みのGUI設定

先読みのGUI設定は `EngineProfileStore::loadPonder()` / `savePonder()` が読み書きし、エンジン名グループの下に次のキーを保存する。エンジン削除時は同じグループとともに削除される。

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
| `boundedSettingClampsStoredValue` | `BoundedSetting` が範囲外の保存値を読むときも書くときも範囲に収めること |
| `structSettingUsesFieldKeys` | `StructSetting` が項目ごとのキーに書き、保存していない項目は構造体の既定値のままにすること |
| `listSettingUsesArrayFormat` | `ListSetting` が配列の形式で書き、実行時に作ったキーでも読み書きできること |
| `structListSettingKeepsCsaHistoryFormat` | `StructListSetting` の `csaServerHistory` が以前の版の配列の形式を読めること |
| `appSettings_*`・`analysisSettings_*`・`gameSettings_*` など | ドメインごとの往復。範囲や形式を変える項目は範囲外の値・旧形式の値も確かめる |

ほかのテストで設定ファイルを使うときは、設定に触れる前に `SHOGIBOARDQ_CONFIG_HOME` を一時ディレクトリに向ける（`tst_startgamedialog`・`tst_nyugyoku_judgement`・`tst_engineprofilestore`・`tst_engineselector` など）。
`tst_engineselector` は、機能ごとに選んだエンジンの保存・復元（名前・以前の版の番号・詰将棋問題一覧の `"@hayanagi"` とパス）を確かめる。

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
