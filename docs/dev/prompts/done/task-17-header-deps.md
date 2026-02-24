# Task 17: ヘッダ依存の削減

`mainwindow.h` を中心に、前方宣言で代替可能な `#include` を `.cpp` 側に移動してください。

## 背景

`mainwindow.h` は30個の `#include` と56個の前方宣言がある。ポインタや参照でしか使われていないヘッダを `.cpp` に移動すると、再ビルド範囲が減少する。

## 作業内容

### Step 1: mainwindow.h の #include 監査

`mainwindow.h` の各 `#include` について、以下を確認:

- **ヘッダに残すべき**: 基底クラス、値メンバ、インライン関数で型の完全定義が必要
- **前方宣言に置換可能**: ポインタ/参照メンバ、メソッドの引数/戻り値がポインタ/参照

### Step 2: mainwindow.h の修正

前方宣言で足りるものを `#include` → `class XxxClass;` に置換し、元の `#include` を `mainwindow.cpp` に移動。

例:
```cpp
// mainwindow.h
// Before
#include "src/game/matchcoordinator.h"
// After
class MatchCoordinator;

// mainwindow.cpp
#include "src/game/matchcoordinator.h"  // ← ここに移動
```

### Step 3: 他の大規模ヘッダの監査

以下のヘッダも同様に監査:
- `src/game/matchcoordinator.h`
- `src/network/csagamecoordinator.h`
- `src/dialogs/` 配下の大きなダイアログヘッダ

### Step 4: ビルド確認

- 全ファイルがビルドできること
- 循環依存が発生していないこと

## 制約

- CLAUDE.md のコードスタイルに準拠
- `#include` の順序は既存のスタイルに合わせる
- シグナル/スロットの引数に使われる型は完全定義が必要（MOC の制約）
- 全既存テストが通ること
