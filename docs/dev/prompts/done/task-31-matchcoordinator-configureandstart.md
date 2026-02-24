# Task 31: MatchCoordinator configureAndStart() の分解

## Workstream B-1: MatchCoordinator 責務分割（最優先）

## 目的

`MatchCoordinator::configureAndStart()` を分解し、役割ごとに責務を分離する。

## 背景

- `src/game/matchcoordinator.cpp` は 2,598 行と肥大化
- `configureAndStart()` が長大で、履歴探索・開始設定・モード分岐が同居している
- 可読性が低く、変更時の影響範囲が大きい

## 対象ファイル

- `src/game/matchcoordinator.h`
- `src/game/matchcoordinator.cpp`
- 必要に応じて `src/game/` 配下に新規ヘルパ/サービスを追加

## 実装内容

`configureAndStart()` を以下の最低 4 つの役割に分離する:

1. **既存履歴同期と探索**
   - 既存の棋譜履歴との同期処理
   - 履歴探索ロジック

2. **開始 SFEN 正規化と position 文字列構築**
   - SFEN 文字列の正規化
   - USI position コマンド用の文字列構築

3. **フック呼び出し / UI 初期化**
   - 対局開始前のフック処理
   - UI 状態の初期化

4. **`PlayMode` 別起動**
   - HvH（人 vs 人）
   - HvE（人 vs エンジン）
   - EvE（エンジン vs エンジン）
   - 各モード固有の起動処理

まず `configureAndStart()` の現在の実装を読み、上記の責務境界を特定してから分解すること。

## 制約

- 既存挙動を変更しない
- 対局開始フロー（平手/駒落ち/現在局面/分岐/連続対局）を維持
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- USI/CSA 連携挙動を維持

## 受け入れ条件

- `configureAndStart()` の読みやすさが向上し、分岐ごとの責務が明確
- EvE/HvE/HvH の起動シナリオで回帰がない
- 分離した各処理の責務が 1 つに寄っている

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 出力

- 変更ファイル一覧
- `configureAndStart()` の分解構造の説明
- 回帰リスク
- 残課題
