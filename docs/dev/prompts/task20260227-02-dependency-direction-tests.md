# Task 02: 依存方向検証テスト追加

## 目的
レイヤ間の依存方向ルールを自動検証し、将来の境界違反を CI で自動検出する。

## 背景
現時点でレイヤ境界違反は検出されていないが、防止の仕組みがない。

## 依存ルール定義

以下の `#include` を禁止する:

| 禁止対象ディレクトリ | 禁止される include パターン |
|---|---|
| `src/core/` | `app/`, `ui/`, `dialogs/`, `widgets/`, `views/` |
| `src/kifu/formats/` | `app/`, `dialogs/`, `widgets/`, `views/` |
| `src/engine/` | `app/`, `dialogs/`, `widgets/`, `views/` |
| `src/game/` | `dialogs/`, `widgets/`, `views/` |
| `src/services/` | `app/`, `dialogs/`, `widgets/`, `views/` |

## 作業内容

### 1. テストファイル作成
`tests/tst_layer_dependencies.cpp` を新規作成する。

### 2. テストケース
- 各レイヤのソースファイルをスキャンし、`#include` 行を解析する
- 禁止パターンに一致する include があれば FAIL にする
- エラーメッセージに「どのファイルのどの include が違反か」を明記する

### 3. 実装方針
```cpp
// テストデータ構造
struct LayerRule {
    QString sourceDir;          // e.g., "src/core"
    QStringList forbiddenDirs;  // e.g., {"app/", "ui/", "dialogs/"}
};
```

- `QDirIterator` で対象ディレクトリの `.cpp` / `.h` を列挙する
- 各ファイルの `#include` 行を正規表現で抽出する
- 禁止パターンとマッチングする
- 違反を全て収集してから一括報告する（1件見つけたら即 FAIL ではなく全件報告）

### 4. CMakeLists.txt 更新
`tests/CMakeLists.txt` に追加する。

### 5. テスト実行確認
```bash
cmake --build build
cd build && ctest --output-on-failure
```

## 完了条件
- [ ] `tst_layer_dependencies.cpp` が追加され、全テストが PASS する
- [ ] 既存の全テストが引き続き PASS する
- [ ] 依存違反を意図的に追加した場合に FAIL することを手動確認

## 注意事項
- `#include "xxx.h"` 形式のみ検査（`<QtXxx>` はスキップ）
- コメント行（`//`）内の include は除外する
- ソースディレクトリのパスは `CMAKE_SOURCE_DIR` 経由で取得する
