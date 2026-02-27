# Task 12: SettingsService ドメイン分割の分析と計画

## フェーズ

Phase 3（中期）- P1-1 対応・分析フェーズ

## 背景

`settingsservice.h` が 564 行、216 宣言、`settingsservice.cpp` が 1608 行。設定追加時に API が単調増加し、責務境界が曖昧。

## 実施内容

1. `src/services/settingsservice.h` と `.cpp` を読み、全 getter/setter を列挙する
2. 各関数を以下のドメインに分類する：
   - **WindowSettings**: ウィンドウサイズ、位置、ジオメトリ関連
   - **FontSettings**: フォントサイズ、フォントファミリー関連
   - **JosekiSettings**: 定跡関連設定
   - **AnalysisSettings**: 解析・エンジン関連設定
   - **GameSettings**: 対局・棋譜関連設定
   - **NetworkSettings**: CSA ネットワーク関連設定
   - **GeneralSettings**: その他全般
3. 分割計画を `docs/dev/settings-service-split-plan.md` に記録する：
   - ドメインごとの関数一覧と件数
   - 構造体化の方針（例: `struct WindowSettings { QSize mainWindowSize; ... };`）
   - 移行の段階（互換 adapter の要否）
   - 分割順序の推奨

## 完了条件

- 全 getter/setter がドメイン分類されている
- `docs/dev/settings-service-split-plan.md` が作成されている
- 構造体化の方針が明確である

## 注意事項

- このタスクは**分析のみ**。コード変更は行わない
- 既存キーとの互換性維持が重要（QSettings のキー名は変えない）
