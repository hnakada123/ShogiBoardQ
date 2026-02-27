# Task 03: MainWindowServiceRegistry の分割方針策定

## フェーズ

Phase 1（短期）- P0-3 対応・分析フェーズ

## 背景

`mainwindowserviceregistry.cpp` は 1353 行、95 メソッド定義、42 件の `std::bind` が集中しており、配線変更・初期化順変更時のレビュー難度が高い。

## 実施内容

1. `src/app/mainwindowserviceregistry.h` と `.cpp` を読み、全 `ensure*` メソッドを列挙する
2. 各メソッドを以下のカテゴリに分類する：
   - **UI系**: UI ウィジェット・プレゼンター・ビュー関連
   - **Game系**: 対局・ゲーム進行・ターン管理関連
   - **Kifu系**: 棋譜・ナビゲーション・ファイルI/O関連
   - **Analysis系**: 解析・検討モード・エンジン関連
   - **その他/共通**: 複数カテゴリにまたがるもの
3. 分類結果と依存関係を `docs/dev/service-registry-split-plan.md` に記録する
   - カテゴリごとのメソッド一覧
   - カテゴリ間の依存（あるensureが別カテゴリのensureを呼ぶケース）
   - 分割順序の推奨（依存が少ないカテゴリから着手）
4. 分割先クラス名の案を提示する（例: `MainWindowUiRegistry`, `MainWindowGameRegistry` 等）

## 完了条件

- 全 ensure* メソッドがカテゴリ分類されている
- カテゴリ間依存が明示されている
- 分割順序の推奨が記載されている
- `docs/dev/service-registry-split-plan.md` が作成されている

## 注意事項

- このタスクは**分析のみ**。コード変更は行わない
- 分割後も `MainWindow` 側の呼び出しコードは最小限の変更で済むよう考慮する
