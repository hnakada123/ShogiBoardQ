# 設定ファイルと SettingsService

ShogiBoardQ のアプリケーション設定の永続化に関するガイド。

---

## 目次

1. [設定ファイルの保存先](#1-設定ファイルの保存先)
2. [SettingsService の概要](#2-settingsservice-の概要)
3. [設定の読み書き方法](#3-設定の読み書き方法)
4. [新しい設定項目の追加手順](#4-新しい設定項目の追加手順)
5. [エンジン設定（EngineSettingsConstants）](#5-エンジン設定enginesettingsconstants)
6. [旧バージョンからの移行](#6-旧バージョンからの移行)

---

## 1. 設定ファイルの保存先

設定は `ShogiBoardQ.ini`（INI形式）に保存される。
`QStandardPaths::AppConfigLocation` を使用し、プラットフォームごとの標準的な場所に配置される。

| プラットフォーム | パス |
|---|---|
| **macOS** | `~/Library/Preferences/ShogiBoardQ/ShogiBoardQ.ini` |
| **Linux** | `~/.config/ShogiBoardQ/ShogiBoardQ.ini` |
| **Windows** | `C:/Users/<user>/AppData/Local/ShogiBoardQ/ShogiBoardQ.ini` |

パス解決は `SettingsService::settingsFilePath()` が行う。
ディレクトリが存在しない場合は初回アクセス時に自動作成される。

> **前提条件**: `main.cpp` で `QApplication::setApplicationName("ShogiBoardQ")` が設定されていること。
> これにより `QStandardPaths` がアプリ名ベースのディレクトリを返す。

---

## 2. SettingsService の概要

### ファイル構成

| ファイル | 内容 |
|---|---|
| `src/services/settingsservice.h` | 公開 API（namespace `SettingsService` の関数宣言） |
| `src/services/settingsservice.cpp` | 実装（INI の読み書き） |

### 設計方針

- **namespace ベース**: クラスではなく `namespace SettingsService` に free function を配置
- **getter/setter ペア**: 各設定項目に対して取得関数と保存関数を1組提供
- **デフォルト値内蔵**: getter 内で `QSettings::value()` の第2引数としてデフォルト値を指定
- **パス解決の一元化**: `settingsFilePath()` で INI ファイルのフルパスを返し、各関数はこれを使用

### 管理している設定カテゴリ

| カテゴリ | 設定例 |
|---|---|
| ウィンドウサイズ | メインウィンドウ、各ダイアログのサイズ |
| フォントサイズ | 棋譜欄、思考タブ、USIログ、コメント欄など |
| 列幅 | エンジン情報テーブル、思考タブ読み筋テーブル |
| ドック状態 | フローティング、ジオメトリ、表示/非表示 |
| ユーザー設定 | 言語、ツールバー表示、お気に入りアクション |
| 評価値グラフ | Y軸上限、X軸上限、間隔、ラベルフォントサイズ |
| 解析設定 | 棋譜解析の思考時間、エンジン選択、解析範囲 |
| 検討設定 | エンジン選択、時間設定、候補手数（MultiPV） |
| ドックレイアウト | カスタムレイアウトの保存・読み込み・削除 |

---

## 3. 設定の読み書き方法

### SettingsService を使う場合（推奨）

```cpp
#include "settingsservice.h"

// 読み込み
int fontSize = SettingsService::kifuPaneFontSize();  // デフォルト: 10

// 保存
SettingsService::setKifuPaneFontSize(12);
```

### 直接 QSettings を使う場合（エンジン設定など）

`SettingsService` に関数がない設定項目を読み書きする場合は、`settingsFilePath()` を使用する。

```cpp
#include "settingsservice.h"
#include <QSettings>

QSettings settings(SettingsService::settingsFilePath(), QSettings::IniFormat);
int size = settings.beginReadArray("Engines");
// ...
settings.endArray();
```

> **注意**: `QDir::setCurrent(QApplication::applicationDirPath())` + 相対パスでの `QSettings` 構築は**使用しないこと**。
> macOS の `.app` バンドル内に書き込もうとしてコード署名が壊れる原因となる。

---

## 4. 新しい設定項目の追加手順

### 手順 1: settingsservice.h に宣言を追加

```cpp
/// ○○ダイアログのフォントサイズを取得（デフォルト: 10）
int myDialogFontSize();
/// ○○ダイアログのフォントサイズを保存
void setMyDialogFontSize(int size);
```

### 手順 2: settingsservice.cpp に実装を追加

既存の実装パターンに従う。

```cpp
int myDialogFontSize()
{
    QSettings s(settingsFilePath(), QSettings::IniFormat);
    return s.value("MyDialog/fontSize", 10).toInt();
}

void setMyDialogFontSize(int size)
{
    QSettings s(settingsFilePath(), QSettings::IniFormat);
    s.setValue("MyDialog/fontSize", size);
}
```

### 手順 3: ダイアログ側で読み込み・保存を実装

```cpp
// コンストラクタで読み込み
MyDialog::MyDialog(QWidget* parent) : QDialog(parent)
{
    // ...
    int fontSize = SettingsService::myDialogFontSize();
    applyFontSize(fontSize);
}

// デストラクタまたは closeEvent で保存
void MyDialog::closeEvent(QCloseEvent* event)
{
    SettingsService::setMyDialogFontSize(currentFontSize());
    SettingsService::setMyDialogSize(size());
    QDialog::closeEvent(event);
}
```

### INI キーの命名規約

INI ファイル内のキーは `グループ名/キー名` の形式で管理される。

| 用途 | キー例 |
|---|---|
| ウィンドウサイズ | `MyDialog/size` |
| フォントサイズ | `MyDialog/fontSize` |
| 列幅 | `MyDialog/columnWidths` |
| 表示状態 | `MyDialog/visible` |
| 最後の選択 | `MyDialog/lastIndex` |

---

## 5. エンジン設定（EngineSettingsConstants）

エンジンの登録情報（名前、パス、オプション）は `SettingsService` ではなく、各ダイアログが直接 `QSettings` を使って読み書きする。
INI キー名は `src/engine/enginesettingsconstants.h` に定数として定義されている。

```cpp
#include "enginesettingsconstants.h"
#include "settingsservice.h"
#include <QSettings>

using namespace EngineSettingsConstants;

QSettings settings(SettingsService::settingsFilePath(), QSettings::IniFormat);
int count = settings.beginReadArray(EnginesGroupName);  // "Engines"
for (int i = 0; i < count; ++i) {
    settings.setArrayIndex(i);
    QString name = settings.value(EngineNameKey).toString();   // "name"
    QString path = settings.value(EnginePathKey).toString();   // "path"
}
settings.endArray();
```

### 利用箇所

| ファイル | 用途 |
|---|---|
| `engineregistrationdialog.cpp` | エンジンの追加・削除・オプション編集 |
| `changeenginesettingsdialog.cpp` | エンジンオプションの変更 |
| `startgamedialog.cpp` | 対局開始時のエンジン選択 |
| `csagamedialog.cpp` | CSA通信対局のエンジン選択 |
| `kifuanalysisdialog.cpp` | 棋譜解析のエンジン選択 |
| `considerationdialog.cpp` | 検討モードのエンジン選択 |
| `considerationflowcontroller.cpp` | 検討開始時のエンジンパス解決 |
| `usiprotocolhandler.cpp` | USIオプションの読み込み |
| `engineanalysistab.cpp` | 解析タブのエンジン選択 |
| `matchcoordinator.cpp` | 対局中のエンジン設定参照 |
| `csagamecoordinator.cpp` | CSA対局中のエンジン設定参照 |

---

## 6. 旧バージョンからの移行

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
