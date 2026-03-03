# Task 20260303-19: 翻訳ファイルロードテスト追加

## 概要
翻訳ファイル（.qm）のロードと runtime 言語切替の基盤テストを追加する。現状は設定永続化テスト（appSettings_language）のみで、翻訳ファイルの実際のロード検証がない。

## 優先度
Low

## 背景
- Qt の翻訳は `.qm` ファイルを `QTranslator::load()` でロードし、`QCoreApplication::installTranslator()` で適用する
- ShogiBoardQ は `resources/translations/` に `.ts` → `.qm` をビルドして利用
- 翻訳ファイルの存在確認とロード可否は CI のビルド環境で検証可能
- LanguageController のテストは Task 17 で行うため、ここでは翻訳ファイル自体の品質テストに焦点

## 対象ファイル

### 参照（変更不要だが理解が必要）
- `resources/translations/ShogiBoardQ_ja_JP.ts` - 日本語翻訳ソース
- `resources/translations/ShogiBoardQ_en.ts` - 英語翻訳ソース
- `resources/translations/baseline-unfinished.txt` - 未翻訳ベースライン

### 新規作成
1. `tests/tst_translation_files.cpp` - テストファイル

### 修正対象
2. `tests/CMakeLists.txt` - テスト登録追加

## 実装手順

### Step 1: テストファイル新規作成

`tests/tst_translation_files.cpp` を新規作成する。

テストケース方針:

**翻訳ソースファイル存在テスト:**
1. `tsFile_japanese_exists` - ShogiBoardQ_ja_JP.ts が存在
2. `tsFile_english_exists` - ShogiBoardQ_en.ts が存在

**翻訳ソースファイル品質テスト:**
3. `tsFile_japanese_wellFormedXml` - .ts ファイルが整形式XML
4. `tsFile_english_wellFormedXml` - .ts ファイルが整形式XML
5. `tsFile_japanese_hasTranslations` - 翻訳エントリが1件以上存在
6. `tsFile_english_hasTranslations` - 翻訳エントリが1件以上存在

**未翻訳ベースラインテスト:**
7. `baseline_exists` - baseline-unfinished.txt が存在
8. `baseline_unfinishedCount_matchesOrBetter` - 各 .ts ファイルの未翻訳数がベースライン以下

**翻訳整合性テスト:**
9. `tsFile_japanese_noEmptyTranslation` - 翻訳済み（type!="unfinished"）エントリで空の translation がない
10. `tsFile_contexts_consistent` - 日本語・英語で context 名が一致

テンプレート:
```cpp
/// @file tst_translation_files.cpp
/// @brief 翻訳ファイル品質テスト

#include <QtTest>
#include <QFile>
#include <QXmlStreamReader>
#include <QDir>

class TestTranslationFiles : public QObject
{
    Q_OBJECT

private:
    QString translationsDir() const
    {
        return QStringLiteral(SOURCE_DIR "/resources/translations");
    }

    /// .ts ファイル内の未翻訳数をカウント
    int countUnfinished(const QString& filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return -1;
        int count = 0;
        QXmlStreamReader xml(&file);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("translation")) {
                if (xml.attributes().value(QLatin1String("type")) == QLatin1String("unfinished"))
                    ++count;
            }
        }
        return count;
    }

    /// .ts ファイル内の翻訳エントリ総数をカウント
    int countTranslationEntries(const QString& filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return -1;
        int count = 0;
        QXmlStreamReader xml(&file);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("translation"))
                ++count;
        }
        return count;
    }

private slots:
    void tsFile_japanese_exists();
    void tsFile_english_exists();
    void tsFile_japanese_wellFormedXml();
    void tsFile_english_wellFormedXml();
    void tsFile_japanese_hasTranslations();
    void tsFile_english_hasTranslations();
    void baseline_exists();
    void baseline_unfinishedCount_matchesOrBetter();
    void tsFile_japanese_noEmptyFinishedTranslation();
};
```

実装指針:
- `SOURCE_DIR` マクロで翻訳ファイルディレクトリにアクセス（CMake の `target_compile_definitions` で定義）
- XML パースは `QXmlStreamReader` を使用
- ベースラインテストは `baseline-unfinished.txt` の各行（`ファイル名:数値`）をパースして比較
- 翻訳整合性テストでは `type="unfinished"` でない `<translation>` 要素が空でないことを検証
- `.qm` ファイルのロードテストは、ビルドディレクトリに .qm が存在しない場合があるため QSKIP ガード付き

### Step 2: CMakeLists.txt にテスト登録

`tests/CMakeLists.txt` に以下を追加:

```cmake
# ============================================================
# Unit: 翻訳ファイル品質テスト
# ============================================================
add_shogi_test(tst_translation_files
    tst_translation_files.cpp
)
target_compile_definitions(tst_translation_files PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
```

依存ソースは不要（XML パースは Qt 標準ライブラリ）。

### Step 3: ビルド・テスト実行
```bash
cmake -B build -S .
cmake --build build -j4
ctest --test-dir build --output-on-failure -R tst_translation_files
```

### Step 4: 全テスト実行
```bash
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- 翻訳ファイル存在テスト 2件以上
- XML 整形式テスト 2件以上
- 未翻訳ベースラインテスト 1件以上
- ビルド成功（warning なし）
- 既存テスト全件成功
