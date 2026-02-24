# Task 22: ビルド警告ゼロ化

## 目的

クリーンビルド時の `nodiscard` 警告を解消する。

## 背景

`tests/tst_josekiwindow.cpp` で `QTest::qWaitForWindowExposed()` の戻り値が4箇所で未チェックであり、ビルド時に `nodiscard` 警告が出ている。

## 対象ファイル

- `tests/tst_josekiwindow.cpp`（175, 198, 209, 322行目）

## 作業

1. `QTest::qWaitForWindowExposed(...)` の戻り値を `QVERIFY()` で検証する。
2. `tests/` 配下に同様の呼び出しが他にあれば横展開で修正する。

修正例:
```cpp
// Before
QTest::qWaitForWindowExposed(&window);

// After
QVERIFY(QTest::qWaitForWindowExposed(&window));
```

## 受け入れ条件

- [ ] `cmake --build build --clean-first -j4` で `nodiscard` 警告が消えている。
- [ ] `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4` が通る。
