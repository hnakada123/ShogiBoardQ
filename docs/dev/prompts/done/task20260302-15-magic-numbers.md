# Task 15: マジックナンバー定数化（P3 / §2.7）

## 概要

UI やタイマー関連のマジックナンバーを名前付き定数に置換し、可読性を向上させる。

## 対象箇所

| ファイル | 値 | 用途 |
|---|---|---|
| `kifuloadcoordinator.cpp:483` | `QColor(255, 220, 160)` | 分岐ハイライト色 |
| `evaluationchartwidget.cpp:54` | `QColor(255, 255, 255, 80)` | グリッド線色 |
| `evaluationchartwidget.cpp:62` | `QColor(0, 100, 0)` | チャート背景色 |
| `consecutivegamescontroller.cpp:114` | `100` (ms) | 次局開始待ち |
| `engineanalysispresenter.cpp:168` | `500` (ms) | 遅延表示 |
| `csaclient.cpp:49,52` | `60000`, `1000` | 時間単位変換 |

## 手順

### Step 1: 全体検索

1. 上記以外にもマジックナンバーがないか追加検索する
2. 各マジックナンバーの意味を確認する

### Step 2: 定数化

各ファイルでファイルスコープまたは無名名前空間に `constexpr` 定数を定義する:

```cpp
// kifuloadcoordinator.cpp
namespace {
constexpr QColor kBranchHighlightColor(255, 220, 160);
}

// evaluationchartwidget.cpp
namespace {
constexpr QColor kGridLineColor(255, 255, 255, 80);
constexpr QColor kChartBackgroundColor(0, 100, 0);
}

// consecutivegamescontroller.cpp
namespace {
constexpr int kNextGameDelayMs = 100;
}

// engineanalysispresenter.cpp
namespace {
constexpr int kDelayedDisplayMs = 500;
}

// csaclient.cpp
namespace {
constexpr int kMsPerSecond = 1000;
constexpr int kMsPerMinute = 60000;
}
```

### Step 3: ビルド・テスト

1. `cmake --build build` でビルドが通ることを確認
2. テストを実行して全パスを確認

## 注意事項

- `QColor` の `constexpr` はQt6で対応。コンパイルエラーが出る場合は `const` にする
- 定数名はファイルローカルで使うため `k` プレフィックスを使用
- 時間変換の `60000` / `1000` は明らかな場合は置換しなくてもよい（判断に迷う場合はそのまま）
