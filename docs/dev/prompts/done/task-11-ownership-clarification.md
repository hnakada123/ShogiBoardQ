# Task 11: MainWindow メンバの所有権明文化とポインタ整理

## フェーズ

Phase 2（中期）- P0-2 対応

## 背景

`MainWindow` に `nullptr` 初期化の生ポインタメンバが 71 件あり、所有権を誤読しやすい。

## 実施内容

1. `mainwindow.h` の全ポインタメンバを列挙し、以下に分類する：
   - **Qt 親子所有**: `new` で生成し parent を `this` に設定しているもの → 既にOK
   - **unique_ptr 化可能**: MainWindow が唯一の所有者で、Qt 親子関係を使っていないもの → `std::unique_ptr` に変更
   - **非所有（観測ポインタ）**: 別クラスが所有し、MainWindow は参照のみ → コメントまたは命名で明示
   - **Deps/RuntimeRefs 経由**: 既に Deps パターンで管理されているもの → 変更不要

2. 変更を段階的に実施する：
   - `unique_ptr` 化可能なメンバから着手
   - `ensure*` パターンで遅延初期化されるものは `unique_ptr` + `ensure` 内で `make_unique` に
   - Qt 親子所有に乗せられるものは parent 設定を徹底

3. 各変更後にビルド確認

## 完了条件

- `MainWindow` の生ポインタ（`= nullptr`）が 71 → 50 以下
- 所有権が型または命名で判別可能
- ビルドが通る

## 注意事項

- Qt の QObject 親子モデルと unique_ptr は**併用しない**（二重解放になる）
- QObject 派生クラスで parent が設定されているものは生ポインタのままでよい
- `ensure*` で遅延初期化されるオブジェクトは、unique_ptr 化する場合 `.get()` が必要な箇所を丁寧に確認する
