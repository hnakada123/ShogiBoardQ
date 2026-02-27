# Task 14: SettingsService 分割 - 第2〜第3ドメイン構造体化

## フェーズ

Phase 3（中期）- P1-1 対応・実装フェーズ2

## 背景

Task 13 に続き、残りのドメインを構造体化する。

## 実施内容

1. `docs/dev/settings-service-split-plan.md` を読み、次のドメインを確認する
2. 各ドメインの構造体/クラスを作成する
3. `SettingsService` の対応する関数を新クラスへの委譲に変更する
4. `CMakeLists.txt` に新ファイルを追加する
5. ビルド確認: `cmake --build build`

## 完了条件

- 対象ドメインの構造体が作成されている
- ビルドが通る

## 前提

- Task 13 が完了していること
