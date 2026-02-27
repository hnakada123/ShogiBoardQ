# Task 23: 生メモリ管理の監査と改善

## フェーズ

Phase 5（継続運用）- P1-2 対応

## 背景

`new` 使用 624 箇所、`delete` 明示 29 箇所。所有権を誤読しやすく、リファクタ時の破壊リスクが上がる。

## 実施内容

1. `delete` が明示されている 29 箇所を全て確認する：
   ```
   rg "\bdelete\b" src/ --type cpp
   ```

2. 各 `delete` について以下を判定する：
   - **Qt 親子モデルで不要**: parent が設定されているので `delete` は不要 → 削除
   - **unique_ptr 化可能**: 所有権が明確で smart pointer に置き換え可能 → 変更
   - **必要**: コンテナの要素削除等、明示 delete が必要 → 残す
   - **qDeleteAll 置き換え可能**: コンテナの全要素削除 → `qDeleteAll` に統一

3. 改善を実施する
4. ビルド確認: `cmake --build build`

## 完了条件

- 不要な `delete` が除去されている
- smart pointer 化できるものが改善されている
- ビルドが通る
- メモリリークが発生しない（既存動作と同等）

## 注意事項

- Qt の QObject 親子モデルでは parent が設定されていれば自動 delete される
- `unique_ptr` と Qt 親子モデルは併用しない
- `new QWidget(this)` のような Qt イディオムは変更しない（parent による管理が正しい）
- 疑わしい場合は変更しない
