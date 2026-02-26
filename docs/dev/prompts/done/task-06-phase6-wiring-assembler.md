# Phase 6: MainWindowWiringAssembler 導入 — 配線組み立ての分離

## 背景

`docs/dev/mainwindow-responsibility-delegation-plan.md` のフェーズ6。
`MainWindow` に残る大規模な配線組み立てロジックを `MainWindowWiringAssembler` に集約する。
高リスクのため、慎重に進めること。

## 対象メソッド

`src/app/mainwindow.cpp` / `src/app/mainwindow.h` から以下を移譲:

- `buildRuntimeRefs()` — 実行時参照の束ね（約79行）
- `buildMatchWiringDeps()` — MatchWiring 依存構築（約80行）
- `wireMatchWiringSignals()` — MatchWiring signal/slot 接続
- `initializeDialogLaunchWiring()` — DialogLaunchWiring 初期化（約56行）
- `createAndWireKifuLoadCoordinator()` — KifuLoadCoordinator 生成・配線（約58行）

## 実装手順

1. **事前調査（重要）**
   - 上記メソッドの実装を詳細に読み、以下を整理:
     - 参照する MainWindow メンバ変数の完全リスト
     - `MainWindowCompositionRoot` との役割分担
     - `MainWindowRuntimeRefs` / `MainWindowDepsFactory` との関係
     - `MainWindowMatchWiringDepsService` との関係
   - 既存の Wiring クラス群（`MatchCoordinatorWiring`, `DialogLaunchWiring` 等）との関係を確認

2. **新規クラス作成**
   - `src/app/mainwindowwiringassembler.h` / `src/app/mainwindowwiringassembler.cpp` を作成
   - `MainWindowCompositionRoot` とは「組み立て専用」として明確に分ける:
     - `CompositionRoot` = ensure* による遅延生成
     - `WiringAssembler` = 参照束ね・signal/slot 接続
   - MainWindow のメンバへのアクセスは Refs struct 経由

3. **ロジックの移植**
   - `buildRuntimeRefs` → `WiringAssembler::buildRuntimeRefs`
   - `buildMatchWiringDeps` → `WiringAssembler::buildMatchWiringDeps`
   - `wireMatchWiringSignals` → `WiringAssembler::wireMatchWiringSignals`
   - 等
   - MainWindow 側は1〜2行の委譲に変更

4. **CMakeLists.txt 更新**

5. **ビルド確認**
   ```bash
   cmake -B build -S . -G Ninja && ninja -C build
   ```

## 完了条件

- 配線組み立ての本体ロジックが `MainWindowWiringAssembler` に移動
- `MainWindow` の `ensure*` は `CompositionRoot` / `WiringAssembler` 呼び出し中心
- ビルドが通る

## 注意

- **高リスク**: 配線の欠落は実行時にしか判明しない
- 接続漏れを防ぐため、移植前後で signal/slot 接続数が一致することを確認
- `KifuLoadCoordinator` 再生成時の古い接続残りに注意（計画書 8.3 の重点回帰観点）
- `MainWindowCompositionRoot` と競合させない設計にすること
- 手動テスト必須: 全主要機能が動作すること（配線漏れは全機能に影響）
