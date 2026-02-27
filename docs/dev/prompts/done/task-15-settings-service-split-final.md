# Task 15: SettingsService 分割 - 残りドメイン構造体化と最終整理

## フェーズ

Phase 3（中期）- P1-1 対応・実装フェーズ3

## 背景

Task 14 に続き、残りのドメインを全て構造体化し、SettingsService の分割を完了する。

## 実施内容

1. `docs/dev/settings-service-split-plan.md` を読み、未完了ドメインを確認する
2. 残りのドメイン構造体を全て作成する
3. `SettingsService` に残る関数が共通/基盤的なもののみであることを確認する
4. 呼び出し元のうち、容易に新 API に移行できるものは移行する
5. `CMakeLists.txt` を更新する
6. ビルド確認: `cmake --build build`
7. KPI 確認:
   - `SettingsService` 公開宣言数が 120 以下か
   - `settingsservice.cpp` が 900 行以下か

## 完了条件

- 全ドメインの構造体化が完了している
- `SettingsService` 公開宣言数 216 → 120 以下
- `settingsservice.cpp` 1608 行 → 900 行以下
- ビルドが通る

## 前提

- Task 14 が完了していること
