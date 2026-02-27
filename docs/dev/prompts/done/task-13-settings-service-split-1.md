# Task 13: SettingsService 分割 - 第1ドメイン構造体化

## フェーズ

Phase 3（中期）- P1-1 対応・実装フェーズ1

## 背景

Task 12 の分析に基づき、最も独立性が高いドメインから構造体化を開始する。

## 実施内容

1. `docs/dev/settings-service-split-plan.md` を読み、第1ドメインを確認する
2. ドメイン用の構造体/クラスを作成する（例: `WindowSettings`）
   - `src/services/` に配置
   - getter/setter を構造体のメソッドとして実装
   - QSettings のキー名は既存と完全互換を維持
3. `SettingsService` の対応する関数を新クラスへの委譲に変更する
   - 既存の呼び出しコードは**変更しない**（互換 adapter として SettingsService の関数は残す）
   - 新コードは新クラスを直接使用可能にする
4. `CMakeLists.txt` に新ファイルを追加する
5. ビルド確認: `cmake --build build`

## 完了条件

- 第1ドメインの構造体が作成されている
- 既存コードとの互換が維持されている
- ビルドが通る

## 前提

- Task 12 の分析が完了していること

## 注意事項

- QSettings のキー名は変更しない（ユーザーの既存設定が読めなくなる）
- 段階的移行: まず委譲パターンで導入し、後で呼び出し元を順次新 API に移行
