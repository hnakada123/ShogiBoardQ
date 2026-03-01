# Task 20260301-mt-03: 定跡ファイルI/Oのバックグラウンド化（P1）

## 背景

定跡ウィンドウ（`JosekiWindow`）で定跡ファイル（`.db`）の読み書きがメインスレッドで同期実行されている。大規模定跡ファイルでは読み込み/保存中にUIが凍結する。

### 対象ファイル

| ファイル | 行 | メソッド | 処理内容 |
|---------|-----|---------|---------|
| `src/dialogs/josekiwindow.cpp` | 95, 110 | UI操作からの直接I/O | ロード/保存トリガー |
| `src/dialogs/josekiwindow.cpp` | 168, 190 | 保存操作 | ファイル書き込み |
| `src/dialogs/josekirepository.cpp` | 148, 174 | 読み込みループ | 行ごとのパース |
| `src/dialogs/josekirepository.cpp` | 336, 353, 376 | 保存ループ | 行ごとの書き出し |

## 作業内容

### Step 1: 現状把握

`src/dialogs/josekirepository.h/.cpp` と `src/dialogs/josekiwindow.h/.cpp` を読み、以下を確認する:

- `JosekiRepository` のデータ構造（メンバ変数の型）
- `loadFromFile()` / `saveToFile()` の引数と戻り値
- `JosekiWindow` でのロード/保存呼び出しフロー
- ロード/保存結果がUIにどう反映されるか

### Step 2: JosekiLoadResult / JosekiSaveResult 構造体の定義

`src/dialogs/josekiloadresult.h` を新規作成する:

```cpp
#pragma once
#include <QString>
#include <QList>

struct JosekiLoadResult {
    bool success = false;
    QString errorMessage;
    // JosekiRepository のデータ型に合わせた値型リスト
    // 例: QList<JosekiEntry> entries;
    int entryCount = 0;
};

struct JosekiSaveResult {
    bool success = false;
    QString errorMessage;
    int savedCount = 0;
};
```

**注意**: `JosekiRepository` の内部データ構造を確認してから型を決定する。

### Step 3: JosekiRepository のパース/シリアライズを分離

`josekirepository.cpp` を修正:

1. **`parseFromFile(filePath)` → `JosekiLoadResult`**: ファイル読込+パースのみ。UIアクセスなし
2. **`serializeToFile(filePath, entries)` → `JosekiSaveResult`**: データ書き出しのみ

既存の `loadFromFile()` / `saveToFile()` はこれらを呼び出す形にリファクタする。

### Step 4: JosekiWindow に非同期ロード/保存を実装

```cpp
#include <QtConcurrent>
#include <QFutureWatcher>
```

**ロード**:
1. `loadJosekiAsync(filePath)` メソッドを追加
2. `QtConcurrent::run` で `JosekiRepository::parseFromFile()` を実行
3. `QFutureWatcher<JosekiLoadResult>::finished` で結果をUIに適用
4. ロード中はボタンを無効化し、プログレス表示

**保存**:
1. `saveJosekiAsync(filePath)` メソッドを追加
2. `QtConcurrent::run` で `JosekiRepository::serializeToFile()` を実行
3. 完了通知でステータスバーにメッセージ表示
4. 保存中はボタンを無効化

### Step 5: ロード中/保存中のUI状態管理

- ロード/保存中は「読み込み中...」「保存中...」をステータスバーに表示
- 定跡操作ボタン（追加/削除/編集）を無効化
- `QApplication::setOverrideCursor(Qt::WaitCursor)` を使用
- 完了時にカーソルとボタン状態を復元

### Step 6: エラーハンドリング

- ファイルが存在しない/読み取り権限がない場合のエラーダイアログ
- 保存先が書き込み不可の場合のエラーダイアログ
- エラー文字列はワーカースレッドで生成し、UIスレッドで表示

### Step 7: ビルド・テスト確認

```bash
cmake --build build
cd build && ctest --output-on-failure
```

**手動テスト項目**:
1. 定跡ファイルの読み込みが完了すること
2. 読み込み中にウィンドウ操作が可能なこと
3. 保存が完了しファイルが正しく書き出されること
4. 存在しないファイルを指定した場合のエラー表示

## 完了条件

- [ ] 定跡ファイル読み込みが `QtConcurrent::run` でバックグラウンド実行される
- [ ] 定跡ファイル保存が `QtConcurrent::run` でバックグラウンド実行される
- [ ] ロード/保存中のUI状態管理がある
- [ ] エラーハンドリングが適切
- [ ] 全テスト pass
