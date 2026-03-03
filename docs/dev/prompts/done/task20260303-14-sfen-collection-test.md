# Task 20260303-14: 局面集ビューアテスト追加

## 概要
SfenCollectionDialog の SFEN パース・ナビゲーションロジックのテストを追加する。現状は設定永続化テストと棋譜ロードの callback テスト1件のみ。

## 優先度
Medium

## 背景
- `SfenCollectionDialog` は QDialog サブクラスで UI 結合が強い（526行）
- 内部の `parseSfenLines()` は private だが、SFEN テキストをパースしてリストに変換する重要なロジック
- テストアプローチ: ダイアログを実際にインスタンス化してファイルロード→ナビゲーション動作を検証するウィジェットテスト

## 対象ファイル

### 参照（変更不要だが理解が必要）
- `src/dialogs/sfencollectiondialog.h` - ダイアログ定義（117行）
- `src/dialogs/sfencollectiondialog.cpp` - ダイアログ実装（526行）

### 新規作成
1. `tests/tst_sfen_collection.cpp` - テストファイル
2. `tests/fixtures/test_collection.sfen` - テスト用SFENファイル

### 修正対象
3. `tests/CMakeLists.txt` - テスト登録追加

## 実装手順

### Step 1: テスト用フィクスチャ作成

`tests/fixtures/test_collection.sfen` を新規作成:

```
lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1
lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2
lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3
```

3局面の最小限テストデータ。各行が1つのSFEN局面。

### Step 2: テストファイル新規作成

`tests/tst_sfen_collection.cpp` を新規作成する。

テストケース方針:

**SFEN パーステスト（ダイアログのテキスト読み込み）:**
1. `loadText_validSfen_parsesCorrectCount` - 3行のSFENテキストで3局面がパースされる
2. `loadText_emptyLines_skipped` - 空行が含まれていてもスキップされる
3. `loadText_commentLines_skipped` - `#` で始まるコメント行がスキップされる（仕様確認）
4. `loadText_singleLine_parsesOne` - 1行のみでも正常にパース

**ナビゲーションテスト（ダイアログ操作）:**
5. `navigation_goFirst_selectsFirst` - 先頭へ移動
6. `navigation_goLast_selectsLast` - 末尾へ移動
7. `navigation_goForward_advances` - 1つ先へ
8. `navigation_goBack_retreats` - 1つ前へ

**シグナルテスト:**
9. `positionSelected_emitsCorrectSfen` - 局面選択時に positionSelected シグナルが正しいSFENで発火

**エッジケーステスト:**
10. `loadText_empty_noPositions` - 空テキストで局面数0

テンプレート:
```cpp
/// @file tst_sfen_collection.cpp
/// @brief 局面集ビューアテスト

#include <QtTest>
#include <QSignalSpy>

#include "sfencollectiondialog.h"

class TestSfenCollection : public QObject
{
    Q_OBJECT

private slots:
    void loadText_validSfen_parsesCorrectCount();
    void loadText_emptyLines_skipped();
    void loadText_singleLine_parsesOne();
    void loadText_empty_noPositions();
    void positionSelected_emitsCorrectSfen();
};
```

実装指針:
- SfenCollectionDialog のインスタンス化には QApplication + offscreen 環境が必要
- ダイアログの `parseSfenLines` が private の場合、テキストの直接ロードを `loadFromText` 等の public メソッド経由で行う。もしなければ QFile 経由でフィクスチャファイルを読み込む
- `positionSelected` シグナルは `QSignalSpy` で検証
- ダイアログの内部状態（現在のインデックス、局面数）は UI 要素のテキストから間接的に検証するか、friend class を使用
- ダイアログの public API を先に確認し、テスト可能な範囲を調整すること。もし `loadFromFile` が唯一の読み込みパスなら、フィクスチャファイルを使用

### Step 3: CMakeLists.txt にテスト登録

`tests/CMakeLists.txt` に以下を追加:

```cmake
# ============================================================
# Unit: SfenCollectionDialog テスト
# ============================================================
add_shogi_test(tst_sfen_collection
    tst_sfen_collection.cpp
    ${TEST_STUBS}
    ${SRC}/dialogs/sfencollectiondialog.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
    ${SETTINGS_SOURCES}
)
```

SfenCollectionDialog は ShogiBoard, ShogiView 等に依存する。ShogiView はグラフィックスシーンを使用するため追加ソースが必要になる可能性がある。コンパイルエラーに応じて依存を追加すること。

依存が重すぎてコンパイルが困難な場合は、ソース解析型テスト（ファイル内容のパターンマッチ）にフォールバックすることを検討。

### Step 4: ビルド・テスト実行
```bash
cmake -B build -S .
cmake --build build -j4
ctest --test-dir build --output-on-failure -R tst_sfen_collection
```

### Step 5: 全テスト実行
```bash
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- SFEN パーステスト 3件以上
- ナビゲーションまたはシグナルテスト 1件以上
- エッジケーステスト 1件以上
- ビルド成功（warning なし）
- 既存テスト全件成功

## 補足
- SfenCollectionDialog の依存が重い場合、パースロジックを独立ユーティリティに抽出するリファクタリングを先行させてもよい（別タスク）
