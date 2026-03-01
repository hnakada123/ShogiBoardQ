# Task 20260301-mt-01: スレッド基盤整備（P0）

## 背景

ShogiBoardQ は現在すべての処理がメインスレッド（GUIスレッド）で実行されている。
マルチスレッド化に向け、CMake設定、スレッド安全ガイドライン、共通値型の定義を先行して整備する。

### 現状

- `QThread`, `QRunnable`, `QtConcurrent`, `std::thread` は一切未使用
- 同期プリミティブ（`QMutex`, `std::mutex` 等）も未使用
- `QEventLoop` + `processEvents` + `msleep` によるブロッキング待機が6箇所以上存在

## 作業内容

### Step 1: CMakeLists.txt に Concurrent モジュールを追加

`CMakeLists.txt` の `find_package(Qt6 ...)` に `Concurrent` を追加する。

```cmake
find_package(Qt6 REQUIRED COMPONENTS
    Widgets Charts Network LinguistTools Concurrent)
```

`target_link_libraries` にも `Qt6::Concurrent` を追加する。

### Step 2: スレッド安全ガイドライン文書を作成

`docs/dev/threading-guidelines.md` を作成し、以下の内容を記載する:

1. **スレッドモデル**: メインスレッド（GUI）+ ワーカースレッド（エンジンI/O、ファイルI/O）
2. **UIアクセスルール**: `QWidget`, `QAbstractItemModel`, `QGraphicsItem` の操作はメインスレッド限定
3. **QProcess ルール**: 生成したスレッドでのみ操作（スレッド移動禁止）
4. **スレッド間通信**: `Qt::QueuedConnection` のシグナル/スロット、または `QtConcurrent` + `QFutureWatcher`
5. **データ受け渡し**: ワーカーには値オブジェクト（コピー）のみ渡す。共有ミュータブル状態は禁止
6. **stale結果の破棄**: `requestId` / `generation` 番号で古い結果を無視
7. **キャンセル**: `std::atomic_bool` または `QAtomicInt` フラグを使用
8. **connect() ルール**: スレッド間接続でもラムダ禁止（CLAUDE.md 準拠）
9. **既存パターンとの整合**: `ensure*()` 遅延初期化、Deps/Hooks/Refs パターンを維持

### Step 3: スレッド間通信用の共通値型ヘッダを作成

`src/common/threadtypes.h` を作成し、以下の型を定義する:

```cpp
#pragma once

#include <QString>
#include <QList>
#include <atomic>

// スレッド間受け渡し用の共通型

// 非同期ジョブの世代番号（stale結果破棄用）
using JobGeneration = quint64;

// 非同期ジョブのキャンセルフラグ
// ワーカースレッドで定期的にチェックし、trueならジョブを中断する
using CancelFlag = std::shared_ptr<std::atomic_bool>;

inline CancelFlag makeCancelFlag()
{
    return std::make_shared<std::atomic_bool>(false);
}
```

### Step 4: CLAUDE.md にスレッドガイドライン参照を追加

`CLAUDE.md` の `## Design Principles` セクションに以下を追記する:

```markdown
- **スレッド安全性**: `docs/dev/threading-guidelines.md` に従う。UIアクセスはメインスレッド限定、ワーカーには値オブジェクトのみ渡す
```

### Step 5: ビルド確認

```bash
cmake -B build -S .
cmake --build build
cd build && ctest --output-on-failure
```

- Qt6::Concurrent がリンクできること
- 既存テストが全て pass すること

## 完了条件

- [ ] `Qt6::Concurrent` がCMakeに追加されリンクできる
- [ ] `docs/dev/threading-guidelines.md` が作成されている
- [ ] `src/common/threadtypes.h` が作成されている
- [ ] CLAUDE.md にスレッドガイドライン参照が追加されている
- [ ] 全テスト pass
