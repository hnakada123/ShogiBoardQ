# 定期メンテナンスチェックリスト

## 月次

- `cmake --build build` を実行してビルド健全性を確認
- `ctest --test-dir build --output-on-failure` を実行して回帰確認
- `bash scripts/update-test-summary.sh --build-dir build --check` を実行
- 翻訳未完了件数を確認し、改善があれば baseline を更新
- CI 失敗傾向（flake / timeout）を確認し、対策Issueを作成

## 四半期

- 依存ツール（Qt/CMake/Actions）の更新可否を評価
- カバレッジ推移を確認し、閾値の見直しを判断
- 主要ドキュメント（KPI/手動テスト/運用手順）を棚卸し
- クロスプラットフォーム配布（Linux/macOS/Windows）の実機確認

## リリース前

- リリースタグで `release.yml` を実行
- 互換性テスト（設定・棋譜）を確認
- 生成物の署名/公証結果を確認
- SHA256 と SBOM が添付されていることを確認
- リリースノートの互換性注意事項を確認

## 担当運用

- 月次・四半期の担当者を決め、実施日を Issue で記録する
- 未実施項目は次回へ持ち越さず、必ずフォローIssueを起票する
