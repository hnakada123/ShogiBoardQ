# Task 16: shogiview.cpp の分割

## フェーズ

Phase 4（中長期）- P0-4 対応

## 背景

`src/views/shogiview.cpp` は 2836 行あり、描画・入力・レイアウトの責務が混在している。

## 実施内容

1. `src/views/shogiview.h` と `shogiview.cpp` を読み、メソッドを以下の責務に分類する：
   - **描画**: `paint*`, `draw*`, `update*` 系メソッド
   - **入力処理**: `mouse*`, `key*`, `drag*` 系イベントハンドラ
   - **レイアウト**: サイズ計算、座標変換、配置ロジック
   - **状態管理**: メンバ変数の初期化・更新

2. 分割方針を決定する：
   - **案A**: 部分クラスパターン（`shogiview_draw.cpp`, `shogiview_input.cpp`, `shogiview_layout.cpp` に分割、ヘッダーは共通）
   - **案B**: ヘルパークラス抽出（`ShogiViewRenderer`, `ShogiViewInputHandler` 等を別クラスに）
   - Qt の QGraphicsView 制約を考慮し、最適な方法を選択

3. 分割を実施する
4. `CMakeLists.txt` を更新する
5. ビルド確認: `cmake --build build`

## 完了条件

- `shogiview.cpp` 単体が 1000 行以下、または論理的に分割されている
- ビルドが通る
- 既存の描画・操作動作が変わらない

## 注意事項

- Qt の `QGraphicsView` / `QGraphicsScene` の仮想関数オーバーライドは ShogiView 本体に残す必要がある
- protected/private メソッドのアクセス制御に注意
