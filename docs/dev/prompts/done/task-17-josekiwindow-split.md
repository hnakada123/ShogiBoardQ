# Task 17: josekiwindow.cpp の分割

## フェーズ

Phase 4（中長期）- P0-4 対応

## 背景

`src/dialogs/josekiwindow.cpp` は 2406 行あり、View ロジックとデータ操作が混在している。

## 実施内容

1. `src/dialogs/josekiwindow.h` と `josekiwindow.cpp` を読み、責務を分析する
2. 以下の方針で分割する：
   - **Coordinator/Presenter**: 定跡データの操作ロジック（検索、フィルタ、ソート等）を抽出
   - **View**: UI イベント処理、表示更新のみ残す
   - データモデルが未分離の場合はモデルクラスも検討

3. 新クラスを作成し、ロジックを移動する
   - `src/dialogs/` または `src/ui/coordinators/` に配置
4. `CMakeLists.txt` を更新する
5. ビルド確認: `cmake --build build`

## 完了条件

- `josekiwindow.cpp` が 1000 行以下
- ビジネスロジックが Coordinator/Presenter に分離されている
- ビルドが通る

## 注意事項

- 定跡ウィンドウは独立したダイアログなので、MainWindow への影響は最小限のはず
- `.ui` ファイルがある場合は、UI 変更は最小限に留める
