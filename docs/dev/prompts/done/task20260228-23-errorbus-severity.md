# Task 23: ErrorBus severity追加（Phase 2: テスト基盤拡充）

## 目的

`ErrorBus` にエラーの深刻度（severity）区分を追加し、エラー種別に応じた表示・対応を可能にする。

## 背景

- 現在の `ErrorBus` は単一の `errorOccurred(QString)` シグナルのみ
- 情報メッセージ、警告、致命的エラーが全て同一扱い
- エラーの深刻度に応じた UI 表示の分岐ができない
- 包括的改善分析 §6.2, §6.3 P2

## 対象ファイル

- `src/common/errorbus.h`
- `src/common/errorbus.cpp`
- `ErrorBus::postError()` の呼び出し元（7ファイル程度）
- `ErrorBus::errorOccurred()` の接続先

## 事前調査

### Step 1: 現在の ErrorBus の実装確認

```bash
cat src/common/errorbus.h
cat src/common/errorbus.cpp
```

### Step 2: postError() の呼び出し箇所の特定

```bash
rg "postError\(" src --type cpp --type-add 'header:*.h' --type header -n
```

### Step 3: errorOccurred() シグナルの接続先

```bash
rg "errorOccurred" src --type cpp -n
```

### Step 4: 各呼び出しの深刻度の分類

各 `postError()` 呼び出しを以下に分類:
- **Info**: ユーザーへの情報通知（操作結果等）
- **Warning**: 続行可能だが注意が必要な状態
- **Error**: 操作失敗（ファイルI/O失敗等）
- **Critical**: アプリケーション継続に影響する致命的エラー

## 実装手順

### Step 5: ErrorLevel enum の追加

```cpp
// src/common/errorbus.h
class ErrorBus final : public QObject {
    Q_OBJECT
public:
    enum class ErrorLevel {
        Info,
        Warning,
        Error,
        Critical
    };
    Q_ENUM(ErrorLevel)

    static ErrorBus& instance();

    // 新API（推奨）
    void postMessage(ErrorLevel level, const QString& message);

    // 後方互換（既存呼び出しを壊さない）
    void postError(const QString& message);

signals:
    // 新シグナル
    void messagePosted(ErrorBus::ErrorLevel level, const QString& message);

    // 後方互換
    void errorOccurred(const QString& message);
};
```

### Step 6: 実装の更新

```cpp
// src/common/errorbus.cpp
void ErrorBus::postMessage(ErrorLevel level, const QString& message)
{
    emit messagePosted(level, message);
    // 後方互換: Error/Critical は旧シグナルも発火
    if (level == ErrorLevel::Error || level == ErrorLevel::Critical) {
        emit errorOccurred(message);
    }
}

void ErrorBus::postError(const QString& message)
{
    postMessage(ErrorLevel::Error, message);
}
```

### Step 7: 呼び出し元の段階的移行

各 `postError()` 呼び出しを、調査結果に基づき適切な `postMessage()` に置換:

```cpp
// Before
ErrorBus::instance().postError(tr("ファイルの保存に失敗しました"));

// After
ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Error,
                                  tr("ファイルの保存に失敗しました"));
```

全呼び出しを一度に置換する必要はない。新APIを追加後、順次移行。

### Step 8: 接続先の更新

`errorOccurred` に接続している箇所を確認し、必要に応じて `messagePosted` への移行を検討:

```bash
rg "errorOccurred" src --type cpp -n
```

### Step 9: テストの追加

既存の `tst_errorbus.cpp` がある場合はそこに追加。なければ新規作成:

```cpp
void TestErrorBus::testPostMessage_data()
{
    QTest::addColumn<ErrorBus::ErrorLevel>("level");
    QTest::addColumn<QString>("message");
    QTest::addColumn<bool>("emitsErrorOccurred");

    QTest::newRow("info") << ErrorBus::ErrorLevel::Info << "info msg" << false;
    QTest::newRow("warning") << ErrorBus::ErrorLevel::Warning << "warn msg" << false;
    QTest::newRow("error") << ErrorBus::ErrorLevel::Error << "error msg" << true;
    QTest::newRow("critical") << ErrorBus::ErrorLevel::Critical << "critical msg" << true;
}
```

### Step 10: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 完了条件

- [ ] `ErrorBus::ErrorLevel` enum が定義されている
- [ ] `postMessage(ErrorLevel, QString)` メソッドが追加されている
- [ ] `messagePosted(ErrorLevel, QString)` シグナルが追加されている
- [ ] 後方互換性が維持されている（`postError()` / `errorOccurred()` が動作）
- [ ] テストが追加されている
- [ ] 全テスト通過

## KPI変化目標

- ErrorBus severity区分: なし → 4段階（Info/Warning/Error/Critical）
- テスト数: +1（ErrorBus テスト追加の場合）

## 注意事項

- `Q_ENUM(ErrorLevel)` を使用してMOC連携を行う
- シグナルの引数型は完全修飾する（`ErrorBus::ErrorLevel`、clazy-fully-qualified-moc-types 準拠）
- 既存の `postError()` / `errorOccurred()` は後方互換のため残す
- UI側の表示変更（severity に応じたアイコン/色分け等）は将来タスクとする
