# ボタン配色仕様

## 概要

アプリケーション全体のボタンに一貫した配色を適用するため、ボタンの役割ごとにカラーカテゴリを定義している。すべてのスタイル定義は `src/common/buttonstyles.h` の `ButtonStyles` 名前空間に集約されている。

## 設計方針

- **機能別カラー分類**: ボタンの役割に応じて色を割り当てる
- **一箇所管理**: ローカルにスタイルを定義せず、`ButtonStyles::xxx()` を使用する
- **QPushButton / QToolButton 両対応**: 各スタイルに両方のセレクタを記述
- **全状態カバー**: normal / hover / pressed / disabled / checked を定義

## カラーカテゴリ一覧

### fontButton — フォントサイズ（ライトブルー）

| 状態 | 背景色 | 文字色 | ボーダー |
|------|--------|--------|----------|
| Normal | `#e3f2fd` | default | `#90caf9` |
| Hover | `#bbdefb` | — | — |
| Pressed | `#90caf9` | — | — |
| Disabled | `#f5f5f5` | `#9e9e9e` | `#e0e0e0` |

**用途**: A+/A- ボタン、しおり編集ボタン

### navigationButton — ナビゲーション（緑）

| 状態 | 背景色 | 文字色 | ボーダー |
|------|--------|--------|----------|
| Normal | `#4F9272` | white | `#3d7259` |
| Hover | `#5ba583` | — | — |
| Pressed | `#3d7259` | — | — |
| Disabled | `#b0c4b8` | `#e0e0e0` | `#9eb3a5` |

**用途**: 棋譜ドックの ▲| ▲▲ ▲ ▼ ▼▼ ▼| ボタン（コンパクト、`min-width: 28px; max-width: 36px`）

### wideNavigationButton — 横長ナビゲーション（緑）

色は `navigationButton` と同一。`min-width` / `max-width` 制約なし、`padding: 4px 8px`。

**用途**: PvBoardDialog、SfenCollectionDialog のナビゲーションボタン（テキスト付き）

### primaryAction — プライマリアクション（青）

| 状態 | 背景色 | 文字色 | ボーダー |
|------|--------|--------|----------|
| Normal | `#1976d2` | white | `#1565c0` |
| Hover | `#1e88e5` | — | — |
| Pressed | `#1565c0` | — | — |
| Disabled | `#b0bec5` | `#eceff1` | `#90a4ae` |

**用途**: 検討開始、エンジン設定、対局情報更新、取り込み、選択、コメント更新など

### secondaryNeutral — セカンダリ/ニュートラル（グレー）

| 状態 | 背景色 | 文字色 | ボーダー |
|------|--------|--------|----------|
| Normal | `#e0e0e0` | default | `#bdbdbd` |
| Hover | `#d0d0d0` | — | — |
| Pressed | `#bdbdbd` | — | — |
| Disabled | `#f5f5f5` | `#9e9e9e` | `#e0e0e0` |

**用途**: キャンセル、閉じる、盤面回転、将棋盤拡大/縮小、クリア、通信ログ

### dangerStop — 停止/中止（ライトオレンジ）

| 状態 | 背景色 | 文字色 | ボーダー |
|------|--------|--------|----------|
| Normal | `#fff3e0` | default | `#ffcc80` |
| Hover | `#ffe0b2` | — | — |
| Pressed | `#ffcc80` | — | — |
| Checked | `#ffcc80` | — | — |
| Disabled | `#f5f5f5` | `#9e9e9e` | `#e0e0e0` |

**用途**: 棋譜解析中止、対局キャンセル、定跡停止

### editOperation — 編集操作（ライトグリーン）

| 状態 | 背景色 | 文字色 | ボーダー |
|------|--------|--------|----------|
| Normal | `#e8f5e9` | default | `#a5d6a7` |
| Hover | `#c8e6c9` | — | — |
| Pressed | `#a5d6a7` | — | — |
| Disabled | `#f5f5f5` | `#9e9e9e` | `#e0e0e0` |

**用途**: 切り取り、コピー、貼り付け、定跡手追加、マージ、クリップボードから貼り付け

### undoRedo — 元に戻す/やり直し（ライトオレンジ）

色は `dangerStop` と同系。`padding` / `checked` 状態なし。

**用途**: ↩ 元に戻す、↪ やり直し

### toggleButton — トグル（ブルーグレー / 濃い青）

| 状態 | 背景色 | 文字色 | ボーダー |
|------|--------|--------|----------|
| Normal | `#8a9bb5` | white | `#6b7d99` |
| Checked | `#4A6FA5` | white | `#3d5a80` |
| Hover | `#5a82b8` | — | — |
| Pressed | `#3d5a80` | — | — |
| Disabled | `#c0c8d4` | `#e0e0e0` | `#a8b4c4` |

**用途**: 消費時間列、しおり列、コメント列の表示/非表示トグル

### fileOperation — ファイル操作（ライトブルー）

色は `fontButton` と同一。`padding: 4px 8px` で幅広テキスト対応。

**用途**: 定跡ウィンドウのファイル新規/開く/保存/別名保存/履歴、局面集のファイルを開く/履歴

### customizeSettings — カスタマイズ（オレンジ）

| 状態 | 背景色 | 文字色 | ボーダー |
|------|--------|--------|----------|
| Normal | `#FF9800` | white | なし |
| Hover | `#F57C00` | — | — |
| Pressed | `#E65100` | — | — |
| Checked | `#E65100` | white | — |
| Disabled | `#ffe0b2` | `#bdbdbd` | — |

**用途**: メニューウィンドウのカスタマイズ（⚙）ボタン

### menuMainButton — メニューメインボタン（青）

| 状態 | 背景色 | 文字色 | ボーダー |
|------|--------|--------|----------|
| Normal | `#42A5F5` | white | なし |
| Hover | `#1E88E5` | — | — |
| Pressed | `#1565C0` | — | — |
| Disabled | `#B0BEC5` | `#ECEFF1` | — |

**用途**: MenuButtonWidget のメインボタン

### menuAddButton — 追加（緑丸）

| 状態 | 背景色 | 文字色 |
|------|--------|--------|
| Normal | `#4CAF50` | white |
| Hover | `#43A047` | — |
| Disabled | `#c8e6c9` | `#a5d6a7` |

**用途**: メニューカスタマイズの「+」ボタン

### menuRemoveButton — 削除（赤丸）

| 状態 | 背景色 | 文字色 |
|------|--------|--------|
| Normal | `#f44336` | white |
| Hover | `#E53935` | — |
| Disabled | `#ffcdd2` | `#ef9a9a` |

**用途**: メニューカスタマイズの「×」ボタン

### テーブル内ボタン

| 関数名 | 用途 | 背景色 | 文字色 |
|--------|------|--------|--------|
| `tablePlayButton` | 定跡テーブルの「着手」 | `#1976d2` | white |
| `tableEditButton` | 定跡テーブルの「編集」 | `#e8f5e9` | `#2e7d32` |
| `tableDeleteButton` | 定跡テーブルの「削除」 | `#f44336` | white |
| `tableRegisterButton` | マージダイアログの「登録」 | `#1976d2` | white |

## QDialogButtonBox グローバルスタイル

`main.cpp` で `QApplication::setStyleSheet()` により全ダイアログの OK/キャンセルボタンに統一スタイルを適用:

- **通常ボタン**: `secondaryNeutral` 相当（グレー）
- **デフォルトボタン** (`:default`): `primaryAction` 相当（青）

## 特殊なボタン

### ShogiView 編集終了ボタン

盤面上に配置される特殊ボタン。高い視認性が必要なため `ButtonStyles` を使用せず、`shogiview.cpp` 内で直接赤色 (`#e53935`) のスタイルを定義している。

### フラットラベルボタン

`m_btnCsaSendToServer`（engineanalysistab、csawaitingdialog）はクリック不可のラベルとして使用されているため、スタイルを適用しない。

## 使用方法

```cpp
#include "buttonstyles.h"

// ボタンにスタイルを適用
myButton->setStyleSheet(ButtonStyles::primaryAction());

// ローカルにスタイル文字列を定義しないこと（非推奨）
// const QString style = QStringLiteral("QPushButton { background-color: #1976d2; ... }");
```

## ボタン配色の視覚的サマリー

```
ライトブルー (#e3f2fd)  → フォントA+/A-、ファイル操作
緑           (#4F9272)  → ナビゲーション ▲▼
青           (#1976d2)  → プライマリアクション（開始/適用/選択）
グレー       (#e0e0e0)  → セカンダリ（キャンセル/閉じる/回転）
ライトオレンジ (#fff3e0) → 停止/中止、Undo/Redo
ライトグリーン (#e8f5e9) → 編集操作（コピー/貼付/追加）
ブルーグレー (#8a9bb5)  → トグル（非選択時）
濃い青       (#4A6FA5)  → トグル（選択時）
オレンジ     (#FF9800)  → カスタマイズ
赤           (#e53935)  → 編集終了（ShogiView専用）
```
