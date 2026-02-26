# Task 65: Facade最終化 — MainWindowに残す責務の明確化（C16）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C16 に対応。
推奨実装順序の最終段階。

## 前提

Task 50〜64 が全て完了していること。本タスクは移譲作業の総仕上げ。

## 背景

C01〜C15 の移譲が完了した後、MainWindow に残るべき責務を明確化し、不要な残存コードを除去する最終フェーズ。

## 目的

`MainWindow` を「UIルート」「外部公開API」「トップレベルイベント受け口」のみの Facade に仕上げる。

## 対象

| 項目 | 内容 |
|---|---|
| MainWindow コンストラクタ | pipeline 起動のみ |
| ~MainWindow デストラクタ | pipeline shutdown のみ |
| `evalChart()` | 必要最小限の公開API |
| 残存スロット | controller/coordinator への1行転送に限定 |

## 実施内容

### Phase 1: 残存コードの棚卸し
1. MainWindow に残っている全メソッドをリストアップ。
2. 以下に分類:
   - **残すべき**: コンストラクタ/デストラクタ、公開API、イベントハンドラ（closeEvent等）
   - **移動すべき**: まだ残っているロジック（見落とし）
   - **削除すべき**: 使われていないメソッド、不要な転送

### Phase 2: 残存ロジックの移動
1. Phase 1 で「移動すべき」と判定したものを適切なクラスに移動。
2. 不要な転送スロットを削除し、直接接続に変更。

### Phase 3: private データの整理
1. `MainWindow` 直接所有のメンバ変数を最小化。
2. `MainWindowStateStore`（C06）と `ServiceRegistry`（C13）への退避が完了しているか確認。
3. 不要な `#include` を削除。

### Phase 4: RuntimeRefs の分割（任意）
1. `MainWindowRuntimeRefs` が巨大化している場合、用途別DTOへ分割を検討。
2. 循環参照の有無を確認し、必要なら解消。

### Phase 5: 最終確認
1. `MainWindow` のメソッド数、行数を計測。
2. 完了定義を満たしているか確認:
   - 業務ロジック・UI詳細ロジック・依存生成ロジックが残っていない
   - 主責務が「画面起動時の最小配線」「Facade APIの公開」のみ
   - 既存機能が回帰していない
   - `#include`, private メンバ, `ensure*` が大幅減少している

## 制約

- 既存挙動を変更しない
- 公開APIの互換性を維持
- 他のクラスが MainWindow のメソッドを参照している場合は、まず参照元を確認してから削除

## 受け入れ条件

- `MainWindow` に業務ロジック・UI詳細ロジック・依存生成ロジックが残っていない
- `MainWindow` の主責務が「画面起動時の最小配線」「Facade APIの公開」のみ
- 受け口スロットは controller/coordinator への1行転送に限定
- private データが最小化されている
- ビルドが通る
- 全回帰テストがパスする

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト（全項目）

1. 新規対局（平手/駒落ち/途中局面）
2. 棋譜ロード後の行選択・分岐遷移・コメント編集
3. PVクリック、検討開始/停止、USIコマンド送信
4. CSA対局中のナビゲーション制約
5. 投了/中断/時間切れ/連続対局遷移
6. ドック保存/復元/ロック、ツールバー表示保存
7. 起動直後と終了直前の設定保存

## 出力

- MainWindow の最終行数（cpp/h）
- MainWindow の最終メソッド数
- MainWindow の最終 #include 数
- 完了定義の達成状況
- 残課題（もしあれば）
