# ダイアログボイラープレート共通ユーティリティ設計

## 1. 現状の重複パターン

### 1.1 サイズ保存/復元（13ダイアログ）

| ダイアログ | デフォルトサイズ | minimumSize | Settings メソッド |
|---|---|---|---|
| StartGameDialog | 1000×580 | なし | `GameSettings::startGameDialogSize()` |
| ChangeEngineSettingsDialog | 800×800 | なし | `EngineDialogSettings::engineSettingsDialogSize()` |
| EngineRegistrationDialog | 647×421 | なし | `EngineDialogSettings::engineRegistrationDialogSize()` |
| KifuPasteDialog | 600×500 | なし | `GameSettings::kifuPasteDialogSize()` |
| KifuAnalysisDialog | 500×340 | なし | `AnalysisSettings::kifuAnalysisDialogSize()` |
| PvBoardDialog | 620×720 | 400×500 | `AnalysisSettings::pvBoardDialogSize()` |
| SfenCollectionDialog | 620×780 | なし | `GameSettings::sfenCollectionDialogSize()` |
| JosekiWindow | 800×500 | なし | `JosekiSettings::josekiWindowSize()` |
| JosekiMoveDialog | 500×600 | なし | `JosekiSettings::josekiMoveDialogSize()` |
| JosekiMergeDialog | 600×500 | なし | `JosekiSettings::josekiMergeDialogSize()` |
| MenuWindow | 500×400 | なし | `AppSettings::menuWindowSize()` |
| TsumeshogiGeneratorDialog | 600×550 | なし | `TsumeshogiSettings::tsumeshogiGeneratorDialogSize()` |
| CsaGameDialog | (Settingsのデフォルト) | なし | `NetworkSettings::csaGameDialogSize()` |

**共通パターン:**
```cpp
// コンストラクタ
QSize savedSize = XxxSettings::xxxDialogSize();
if (savedSize.isValid() && savedSize.width() > 100 && savedSize.height() > 100) {
    resize(savedSize);
}

// デストラクタ or closeEvent
XxxSettings::setXxxDialogSize(size());
```

**微差異:**
- PvBoardDialog のみ `setMinimumSize()` を併用
- JosekiWindow は `loadSettings()`/`saveSettings()` に他の設定と一緒に束ねている
- デフォルトサイズは Settings の getter 内で指定（`s.value("key", QSize(w,h)).toSize()`）

### 1.2 フォント増減（12ダイアログ + 4ウィジェット）

#### ダイアログ

| ダイアログ | デフォルト | Min | Max | Step | Settings メソッド | applyFontSize の特徴 |
|---|---|---|---|---|---|---|
| StartGameDialog | 9 | 7 | 16 | 1 | `GameSettings::startGameDialogFontSize()` | ComboBox view にも適用 |
| KifuPasteDialog | 10 | 7 | 20 | 1 | `GameSettings::kifuPasteDialogFontSize()` | テキストエディタは等幅フォント維持 |
| ConsiderationDialog | 10 | 8 | 24 | 1 | `AnalysisSettings::considerationDialogFontSize()` | ComboBox view にも適用 |
| CsaGameDialog | 10 | 8 | 24 | 1 | `NetworkSettings::csaGameDialogFontSize()` | 基本パターン |
| CsaWaitingDialog | 10 | 8 | 24 | 1 | `NetworkSettings::csaWaitingDialogFontSize()` | 太字ステータスラベル維持 |
| KifuAnalysisDialog | 10 | 8 | 24 | **2** | `AnalysisSettings::kifuAnalysisFontSize()` | 基本パターン |
| ChangeEngineSettingsDialog | 12 | 8 | 20 | 1 | `EngineDialogSettings::engineSettingsFontSize()` | 基本パターン |
| EngineRegistrationDialog | 12 | 8 | 20 | 1 | `EngineDialogSettings::engineRegistrationFontSize()` | 基本パターン |
| JosekiWindow | 10 | 6 | 24 | 1 | `JosekiSettings::josekiWindowFontSize()` | テーブルヘッダー/StyleSheet調整 |
| JosekiMoveDialog | 10 | 6 | 24 | 1 | `JosekiSettings::josekiMoveDialogFontSize()` | 基本パターン |
| JosekiMergeDialog | 10 | 6 | 24 | 1 | `JosekiSettings::josekiMergeDialogFontSize()` | 基本パターン |
| TsumeshogiGeneratorDialog | 10 | 8 | 24 | 1 | `TsumeshogiSettings::tsumeshogiGeneratorFontSize()` | 基本パターン |

#### ウィジェット（参考・フォントボタン連動あり）

| ウィジェット | Settings メソッド |
|---|---|
| RecordPane | `GameSettings::kifuPaneFontSize()` |
| EngineAnalysisTab | `AnalysisSettings::thinkingFontSize()` |
| UsiLogPanel | `AnalysisSettings::usiLogFontSize()` |
| EvaluationChartWidget | `AnalysisSettings::evalChartLabelFontSize()` |

**共通ロジック（状態管理部分）:**
```cpp
void onFontIncrease() {
    if (m_fontSize < MAX) { m_fontSize += step; applyFontSize(); save(m_fontSize); }
}
void onFontDecrease() {
    if (m_fontSize > MIN) { m_fontSize -= step; applyFontSize(); save(m_fontSize); }
}
```

**ダイアログ固有ロジック（applyFontSize）:**
- **基本パターン**: `setFont()` + `findChildren<QWidget*>()` に全適用
- **ComboBox view 追加適用**: `findChildren<QComboBox*>()` → `view()->setFont()`
- **等幅フォント維持**: 特定ウィジェットは別フォントオブジェクトで適用
- **テーブルヘッダー調整**: ヘッダー高さ・列幅・StyleSheet を再計算
- **太字維持**: 特定ラベルの bold 属性を保持

## 2. 共通ユーティリティ設計

### 2.1 配置先

`src/common/dialogutils.h` / `src/common/dialogutils.cpp`

既存の `src/common/` にはプロジェクト横断のユーティリティ（`buttonstyles.h`, `errorbus.h`, `logcategories.h` 等）が配置されており、ダイアログ共通処理の配置先として適切。

### 2.2 サイズ保存/復元 — フリー関数

状態を持たないため、namespace 内のフリー関数として実装する。

```cpp
// dialogutils.h
#pragma once

#include <QSize>
#include <QWidget>
#include <functional>

namespace DialogUtils {

/// 保存済みサイズを復元する。savedSize が無効または小さすぎる場合は何もしない。
void restoreDialogSize(QWidget* dialog,
                       const QSize& savedSize);

/// 現在のウィジェットサイズを保存する。
void saveDialogSize(const QWidget* dialog,
                    std::function<void(const QSize&)> setter);

} // namespace DialogUtils
```

```cpp
// dialogutils.cpp
#include "dialogutils.h"

namespace DialogUtils {

void restoreDialogSize(QWidget* dialog, const QSize& savedSize)
{
    if (savedSize.isValid() && savedSize.width() > 100 && savedSize.height() > 100) {
        dialog->resize(savedSize);
    }
}

void saveDialogSize(const QWidget* dialog,
                    std::function<void(const QSize&)> setter)
{
    setter(dialog->size());
}

} // namespace DialogUtils
```

**設計判断:**
- デフォルトサイズは各 Settings の getter 側で既に管理されているため、ここでは扱わない
- `minimumSize` は `restoreDialogSize()` 呼び出し前に各ダイアログが `setMinimumSize()` すればよい（PvBoardDialog のみ）
- `saveDialogSize` は `setter` をコールバックで受け取り、Settings 実装から独立させる

### 2.3 フォント増減 — ヘルパークラス

状態管理（現在値・最小・最大・ステップ）が必要なため、ヘルパークラスとして実装する。
非 QObject（メンバ変数として保持。connect に使わない）。

```cpp
// dialogutils.h（上記に追記）

/// ダイアログのフォントサイズ増減を管理するヘルパー。
/// 状態管理（clamp + save）のみ担当。applyFontSize() はダイアログ側で実装する。
class FontSizeHelper {
public:
    struct Config {
        int initialSize;         // Settings から読んだ初期値
        int minSize = 8;
        int maxSize = 24;
        int step = 1;
        std::function<void(int)> saveFn;  // Settings への保存コールバック
    };

    explicit FontSizeHelper(const Config& config);

    /// フォントサイズを step 分増加する。変更があった場合 true を返す。
    bool increase();

    /// フォントサイズを step 分減少する。変更があった場合 true を返す。
    bool decrease();

    /// 現在のフォントサイズを返す。
    int fontSize() const;

private:
    int m_fontSize;
    int m_minSize;
    int m_maxSize;
    int m_step;
    std::function<void(int)> m_saveFn;
};
```

```cpp
// dialogutils.cpp（追記）

FontSizeHelper::FontSizeHelper(const Config& config)
    : m_fontSize(qBound(config.minSize, config.initialSize, config.maxSize))
    , m_minSize(config.minSize)
    , m_maxSize(config.maxSize)
    , m_step(config.step)
    , m_saveFn(config.saveFn)
{
}

bool FontSizeHelper::increase()
{
    if (m_fontSize + m_step > m_maxSize) return false;
    m_fontSize += m_step;
    if (m_saveFn) m_saveFn(m_fontSize);
    return true;
}

bool FontSizeHelper::decrease()
{
    if (m_fontSize - m_step < m_minSize) return false;
    m_fontSize -= m_step;
    if (m_saveFn) m_saveFn(m_fontSize);
    return true;
}

int FontSizeHelper::fontSize() const
{
    return m_fontSize;
}
```

**設計判断:**
- `increase()`/`decrease()` は bool を返し、値が変わった場合のみ true → 呼び出し側は `if (m_fontHelper.increase()) applyFontSize();` とする
- `applyFontSize()` はダイアログ固有ロジック（ComboBox view、等幅フォント維持、テーブルヘッダー調整等）があるため、各ダイアログが引き続き実装する
- `saveFn` コールバックにより Settings 実装から独立。`std::function` を使うが、`connect()` ではなくメンバ変数のコールバックなので CLAUDE.md のルールに抵触しない
- コンストラクタで `qBound()` をかけることで、Settings ファイル汚損時の安全策を一元化

## 3. 各ダイアログへの適用方法

### 3.1 サイズ保存/復元の適用例

**Before (CsaGameDialog):**
```cpp
CsaGameDialog::CsaGameDialog(QWidget *parent)
    : QDialog(parent), ...
{
    // ... setup ...
    QSize savedSize = NetworkSettings::csaGameDialogSize();
    if (savedSize.isValid() && savedSize.width() > 100 && savedSize.height() > 100) {
        resize(savedSize);
    }
}

CsaGameDialog::~CsaGameDialog()
{
    NetworkSettings::setCsaGameDialogSize(size());
}
```

**After:**
```cpp
#include "dialogutils.h"

CsaGameDialog::CsaGameDialog(QWidget *parent)
    : QDialog(parent), ...
{
    // ... setup ...
    DialogUtils::restoreDialogSize(this, NetworkSettings::csaGameDialogSize());
}

CsaGameDialog::~CsaGameDialog()
{
    DialogUtils::saveDialogSize(this, NetworkSettings::setCsaGameDialogSize);
}
```

**PvBoardDialog（minimumSize あり）:**
```cpp
PvBoardDialog::PvBoardDialog(...)
{
    setMinimumSize(400, 500);  // ← 従来通りダイアログ側で設定
    DialogUtils::restoreDialogSize(this, AnalysisSettings::pvBoardDialogSize());
}
```

**JosekiWindow（loadSettings 内）:**
```cpp
void JosekiWindow::loadSettings()
{
    m_fontSize = JosekiSettings::josekiWindowFontSize();
    applyFontSize();
    DialogUtils::restoreDialogSize(this, JosekiSettings::josekiWindowSize());
    // ... 他の設定 ...
}
```

### 3.2 フォント増減の適用例

**Before (ConsiderationDialog):**
```cpp
class ConsiderationDialog : public QDialog {
    int m_fontSize;
    // ...
    void onFontIncrease();
    void onFontDecrease();
    void updateFontSize(int delta);
    void applyFontSize();
};

ConsiderationDialog::ConsiderationDialog(QWidget *parent)
    : QDialog(parent)
    , m_fontSize(AnalysisSettings::considerationDialogFontSize())
{
    applyFontSize();
    // ...
}

void ConsiderationDialog::onFontIncrease() { updateFontSize(1); }
void ConsiderationDialog::onFontDecrease() { updateFontSize(-1); }

void ConsiderationDialog::updateFontSize(int delta)
{
    m_fontSize += delta;
    if (m_fontSize < 8) m_fontSize = 8;
    if (m_fontSize > 24) m_fontSize = 24;
    applyFontSize();
    AnalysisSettings::setConsiderationDialogFontSize(m_fontSize);
}
```

**After:**
```cpp
#include "dialogutils.h"

class ConsiderationDialog : public QDialog {
    FontSizeHelper m_fontHelper;
    // ...
    void onFontIncrease();
    void onFontDecrease();
    void applyFontSize();  // ← ダイアログ固有ロジックは残す
};

ConsiderationDialog::ConsiderationDialog(QWidget *parent)
    : QDialog(parent)
    , m_fontHelper({
          AnalysisSettings::considerationDialogFontSize(),
          8, 24, 1,
          AnalysisSettings::setConsiderationDialogFontSize
      })
{
    applyFontSize();
    // ...
}

void ConsiderationDialog::onFontIncrease()
{
    if (m_fontHelper.increase()) applyFontSize();
}

void ConsiderationDialog::onFontDecrease()
{
    if (m_fontHelper.decrease()) applyFontSize();
}

void ConsiderationDialog::applyFontSize()
{
    // ダイアログ固有のフォント適用ロジック（変更なし）
    int size = m_fontHelper.fontSize();
    QFont f = font();
    f.setPointSize(size);
    setFont(f);
    // ...
}
```

**削除されるメソッド:**
- `updateFontSize(int delta)` — FontSizeHelper に吸収
- `m_fontSize` メンバ変数 — `m_fontHelper.fontSize()` に置換

**KifuAnalysisDialog（step=2）:**
```cpp
, m_fontHelper({
      AnalysisSettings::kifuAnalysisFontSize(),
      8, 24, 2,   // ← step=2
      AnalysisSettings::setKifuAnalysisFontSize
  })
```

**MenuWindow（フォントとボタンサイズが別管理）:**

MenuWindow はフォントサイズとボタンサイズの2軸を持ち、`updateAllButtonSizes()` で両方を同時適用する。
FontSizeHelper はフォントサイズ側にのみ適用し、ボタンサイズは従来通りダイアログ側で管理する。

```cpp
class MenuWindow : public QWidget {
    FontSizeHelper m_fontHelper;
    int m_buttonSize;
    int m_iconSize;
    // ...
};

void MenuWindow::onFontSizeIncrease()
{
    if (m_fontHelper.increase()) updateAllButtonSizes();
}
```

## 4. 移行手順

### Phase 1: ユーティリティファイル作成
1. `src/common/dialogutils.h` と `src/common/dialogutils.cpp` を作成
2. CMakeLists.txt に追加
3. ビルド確認

### Phase 2: サイズ保存/復元の移行（13ダイアログ）
各ダイアログを1つずつ変更:
1. `#include "dialogutils.h"` を追加
2. コンストラクタの `if (savedSize.isValid() ...)` を `DialogUtils::restoreDialogSize()` に置換
3. デストラクタ/closeEvent の `Settings::setXxx(size())` を `DialogUtils::saveDialogSize()` に置換
4. ビルド確認

**適用順（依存関係なし、任意の順序で可能）:**
1. CsaGameDialog
2. StartGameDialog
3. ChangeEngineSettingsDialog
4. EngineRegistrationDialog
5. KifuPasteDialog
6. KifuAnalysisDialog
7. PvBoardDialog
8. SfenCollectionDialog
9. JosekiWindow（loadSettings/saveSettings 内）
10. JosekiMoveDialog
11. JosekiMergeDialog
12. MenuWindow
13. TsumeshogiGeneratorDialog

### Phase 3: フォント増減の移行（12ダイアログ）
各ダイアログを1つずつ変更:
1. `m_fontSize` メンバ変数を `FontSizeHelper m_fontHelper` に置換
2. コンストラクタ初期化子リストで `Config` を設定
3. `onFontIncrease()`/`onFontDecrease()` を簡素化
4. `updateFontSize(int delta)` を削除
5. `applyFontSize()` 内の `m_fontSize` 参照を `m_fontHelper.fontSize()` に置換
6. ビルド確認

**適用順（単純なものから）:**
1. CsaGameDialog（基本パターン）
2. ConsiderationDialog（基本パターン + ComboBox）
3. TsumeshogiGeneratorDialog（基本パターン）
4. JosekiMoveDialog（基本パターン）
5. JosekiMergeDialog（基本パターン）
6. ChangeEngineSettingsDialog（基本パターン）
7. EngineRegistrationDialog（基本パターン）
8. CsaWaitingDialog（太字維持あり）
9. KifuAnalysisDialog（step=2）
10. StartGameDialog（Named constants パターン）
11. KifuPasteDialog（等幅フォント維持）
12. JosekiWindow（テーブルヘッダー調整）

### Phase 4: ウィジェットへの適用検討（任意）
ダイアログと同様のパターンを持つウィジェット（RecordPane, EngineAnalysisTab, UsiLogPanel 等）にも FontSizeHelper を適用可能。ただしこれらはダイアログほどボイラープレートが大きくないため、優先度は低い。

## 5. 削減効果の見積もり

### サイズ保存/復元
- 各ダイアログ: 4行 → 1行（コンストラクタ）、1行 → 1行（デストラクタ）
- 合計: 約 **39行削減**（13ダイアログ × 3行）

### フォント増減
- 各ダイアログ: `onFontIncrease` + `onFontDecrease` + `updateFontSize` ≒ 15〜20行 → 6行
- 合計: 約 **120〜170行削減**（12ダイアログ × 10〜14行）

### 新規コード
- `dialogutils.h`: 約30行
- `dialogutils.cpp`: 約35行

**純削減: 約 100〜170行**
