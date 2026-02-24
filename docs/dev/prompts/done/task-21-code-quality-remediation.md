# Task 21: コード品質改善 概要

## 目的

ShogiBoardQ の保守性と設計一貫性を改善する。
機能追加ではなく、既存挙動を維持したまま品質を上げることが目的。

## コードベース全体の観察

コードベース全体のレビューで、改善の余地がある以下の4点が確認された。

### 観察 1: MainWindow がまだ大きい

リファクタリングで 5,368行 → 4,569行へ削減されており、方向性は正しい。ただし依然として「神クラス」（1つのクラスがあまりにも多くの責務を抱えている状態）の傾向がある。

- メンバ変数が約111個
- `ensure*()` メソッドが37個
- `connect()` 呼び出しが71箇所
- 50行超のメソッドが14個（`resetToInitialState` 164行、`ensureMatchCoordinatorWiring` 117行等）

### 観察 2: スマートポインタの活用が限定的

`std::unique_ptr` の使用が約10箇所にとどまり、大半が Qt の親子管理に委ねられている。Qt の慣習としては妥当だが、Qt 管理外のオブジェクトには改善の余地がある。

特に `m_gameSessionFacade` は `new` で確保されているのにコメントが「非所有」で、かつ `delete` が存在しない（メモリリークの可能性）。

### 観察 3: 一部のパーサーの複雑さ

KIF/KI2 パーサーなどで、正規表現と条件分岐が深くネストしている箇所がある。将棋の棋譜フォーマット自体の複雑さを考えると仕方ない面もあるが、extract-method で分割する余地はある。

### 観察 4: エラーハンドリングの一貫性

`ErrorBus` が用意されているが全体的に使用されているわけではなく、一部の関数では入力検証が省略されている。`std::optional` の活用が限定的で、パース結果を空文字列や `bool` で返す箇所がある。

## 現状の具体的な問題点

- `MainWindow` の肥大化が継続している。
  - `src/app/mainwindow.cpp`: 約 4,569 行
  - `src/app/mainwindow.h`: 約 825 行
- コア司令塔も大きい。
  - `src/game/matchcoordinator.cpp`: 約 2,598 行
  - `src/game/matchcoordinator.h`: 約 611 行
- 設定サービスが巨大化している。
  - `src/services/settingsservice.cpp`: 約 1,608 行
- クリーンビルド時、テストコードで警告あり（`nodiscard` 戻り値未使用）。
  - `tests/tst_josekiwindow.cpp`: 175, 198, 209, 322行目
- `ctest` は GUI 環境依存で abort しやすく、`QT_QPA_PLATFORM=offscreen` では通る。
- CLAUDE.md で禁止されている lambda connect が7箇所残存。
  - `src/ui/coordinators/docklayoutmanager.cpp`: 3箇所
  - `src/engine/usi.cpp`: 2箇所
  - `src/widgets/collapsiblegroupbox.cpp`: 1箇所
  - `src/widgets/menubuttonwidget.cpp`: 1箇所

## 改修タスク一覧

上記の問題を以下のタスクに分割し、1タスク＝1セッション（`/clear` 区切り）で完了可能な粒度にしている。

| タスク | 内容 | 優先度 | 規模 |
|--------|------|--------|------|
| Task 22 | ビルド警告ゼロ化 | P0（必須） | 小 |
| Task 23 | 所有権の明確化・unique_ptr 移行 | P1（必須） | 中 |
| Task 24 | MainWindow 責務分割 | P2（必須） | 大 |
| Task 25 | lambda connect の解消 | P3（推奨） | 小 |
| Task 26 | パーサーの複雑さ低減 | P4（推奨） | 中 |
| Task 27 | エラーハンドリングの一貫性向上 | P5（推奨） | 中 |

## 共通テスト/検証手順

全タスクで以下を実施する:

```bash
cmake --build build --clean-first -j4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

任意（推奨）:
```bash
cmake -B build -S . -DENABLE_CLANG_TIDY=ON
cmake -B build -S . -DENABLE_CPPCHECK=ON
```

## 共通実装ルール

- 既存の機能仕様は変えない。
- リファクタは小さめのコミット単位で進める。
- 命名規約（ファイル: snake_case、クラス: CamelCase）を守る。
- `MainWindow` にロジックを戻さない。
- CLAUDE.md のコードスタイルに準拠。
