# 開発者向け資料

## 置き方

| 場所 | 置くもの |
|---|---|
| このディレクトリの直下 | 今のコードに合わせて更新し続ける文書（設計・規約・手順） |
| [reports/](reports/) | ある時点の監査・調査・検証の報告。行番号やクラス名は当時のもので、後から書き直さない |
| [prompts/](prompts/) | 別の PC や後の会話で使う依頼文のうち、これからも使うもの、まだ終わっていないもの |

- 終わった計画書と依頼文は、置き場に移さずに消す。git の履歴に残る。
- 直下の文書の場所は変えない。配布済みの ZIP の README、公開ガイド、ソースのコメント、スクリプトが `docs/dev/<名前>.md` を指している。

## 全体の設計

- [developer-guide.md](developer-guide.md): ソースコードの解説マニュアル
- [composition-root.md](composition-root.md): 合成ルート（`MainWindowComposer`）と部品の組み立て
- [deps-hooks-refs-pattern.md](deps-hooks-refs-pattern.md): 依存注入の3種類の構造体の使い分け
- [ui-state-policy.md](ui-state-policy.md): 動作状態ごとの UI 要素の有効・無効
- [settings-service.md](settings-service.md): 設定項目の宣言と保存
- [move-and-record-data-structures.md](move-and-record-data-structures.md): 指し手と棋譜のデータ構造
- [button-color-scheme.md](button-color-scheme.md): ボタンの役割と配色

## コーディング規約

- [ownership-guidelines.md](ownership-guidelines.md): 所有権
- [threading-guidelines.md](threading-guidelines.md): スレッド安全性
- [debug-logging-guidelines.md](debug-logging-guidelines.md): デバッグ文とログ
- [protocol-error-policy.md](protocol-error-policy.md): USI・CSA のエラー処理とログ
- [commenting-style-guide.md](commenting-style-guide.md): コメントの書き方
- [refactoring-inspection-checklist.md](refactoring-inspection-checklist.md): リファクタリングの確認項目

## 対局

- [game-start-flow.md](game-start-flow.md): 対局ダイアログから対局開始まで
- [game-play-to-resignation-flow.md](game-play-to-resignation-flow.md): 対局開始から投了まで
- [game-time-control-spec.md](game-time-control-spec.md): 時間設定と時計表示の仕様
- [matchcoordinator-split-design.md](matchcoordinator-split-design.md): `MatchCoordinator` の設計
- [sennichite-detection.md](sennichite-detection.md): 千日手の検出
- [p2p-game-spec.md](p2p-game-spec.md): P2P対局と、双方が署名した対局記録の実装仕様（設計案。ブロックチェーン・ウォレットは使わない）
- [p2p-interop-spec.html](p2p-interop-spec.html): P2P対局と相互に対局できるソフトウェアを別に作るための仕様書（通信の層・招待文字列・フレーム・交渉・対局・再接続・証跡の検証と、試験用の値）
- [p2p-development-strategy.md](p2p-development-strategy.md): 別リポジトリでのP2P実験版開発、配布・権限の分離と段階的な検証方針
- [p2p-wallet-user-burden.md](p2p-wallet-user-burden.md): P2P対局にウォレットを導入した場合の利用者の負担（署名、暗号資産の手数料など）。ブロックチェーンとウォレットをやめた判断の記録

## 棋譜・解析

- [kifu-io-features.md](kifu-io-features.md): 棋譜の読み込み・出力・クリップボード
- [kif-branch-plus-marker.md](kif-branch-plus-marker.md): KIF の分岐の `+` の仕様
- [kifu-analysis-judgment.md](kifu-analysis-judgment.md): 棋譜解析の形勢判断の基準

## 詰将棋

- [tsume-play.md](tsume-play.md): 詰将棋対局
- [tsumeshogi-generator.md](tsumeshogi-generator.md): 局面の自動生成
- [tsumeshogi-collection-generation.md](tsumeshogi-collection-generation.md): 同梱の問題集6000題の作り方

## MCP サーバー

- [mcp-server.md](mcp-server.md): MCP サーバーと自動化 API の設計

## ビルド・リリース

- [linux-build-and-release.md](linux-build-and-release.md): Linux（`scripts/docs/build_guide/` から生成するので直接編集しない）
- [windows-build-and-release.md](windows-build-and-release.md): Windows
- [macos-build-and-release.md](macos-build-and-release.md): macOS
- [arch-linux-aur-packaging.md](arch-linux-aur-packaging.md): AUR への登録
- [versioning.md](versioning.md): 版番号とリリースの運用
- [qt-licensing.md](qt-licensing.md): Qt の文書とリリースの添付

## テスト・デバッグ

- [test-summary.md](test-summary.md): テストの一覧（`scripts/update-test-summary.sh` で生成）
- [manual-test-scenarios.md](manual-test-scenarios.md): 手動テストのシナリオ
- [kpi-thresholds.md](kpi-thresholds.md): 構造 KPI のしきい値の根拠
- [gui-runtime-debugging.md](gui-runtime-debugging.md): GUI のランタイムデバッグ
- [live-game-verification.md](live-game-verification.md): 対局中の挙動の実機検証
- [ci-troubleshooting.md](ci-troubleshooting.md): CI の失敗への対処

## 運用

- [support-policy.md](support-policy.md): サポートポリシー
- [incident-response.md](incident-response.md): 障害対応
- [maintenance-checklist.md](maintenance-checklist.md): 定期メンテナンス
- [translation-quality-policy.md](translation-quality-policy.md): 翻訳の品質

## 報告（reports/）

コード監査（各回は前回の修正の後の状態が対象）:

- [2026-10-16](reports/code-audit-2026-10-16.md)・[2026-10-23](reports/code-audit-2026-10-23.md)・[2026-10-29](reports/code-audit-2026-10-29.md)・[2026-11-01](reports/code-audit-2026-11-01.md)・[2026-11-07](reports/code-audit-2026-11-07.md)

セキュリティ監査:

- [2026-10-23](reports/security-audit-2026-10-23.md)・[2026-11-07](reports/security-audit-2026-11-07.md)

性能:

- [board-input-performance-2026-10-11.md](reports/board-input-performance-2026-10-11.md): 盤の操作と思考情報の表示の高速化
- [gui-rendering-audit-2026-10-12.md](reports/gui-rendering-audit-2026-10-12.md): GUI の描画処理の監査

不具合の調査・プロトコルの準拠:

- [engine-timeout-investigation-2026-09-08.md](reports/engine-timeout-investigation-2026-09-08.md)・[engine-timeout-fix-2026-09-08.md](reports/engine-timeout-fix-2026-09-08.md): エンジンの時間切れの調査と修正
- [tsumeshogi-yodzume-investigation.md](reports/tsumeshogi-yodzume-investigation.md): 詰将棋生成の余詰の調査（証跡は `reports/tsumeshogi-yodzume-evidence/`）
- [csa-protocol-audit-2026-10-13.md](reports/csa-protocol-audit-2026-10-13.md): CSA プロトコル 1.2.1 への準拠

MCP での機能検証:

- [mcp-coverage-implementation-2026-09-28.md](reports/mcp-coverage-implementation-2026-09-28.md): 未対応の経路の補完
- [csa-game-mcp-audit-2026-09-27.md](reports/csa-game-mcp-audit-2026-09-27.md): CSA 通信対局
- [consideration-mcp-audit-2026-09-27.md](reports/consideration-mcp-audit-2026-09-27.md): 検討モード
- [dock-mcp-audit-2026-09-27.md](reports/dock-mcp-audit-2026-09-27.md): ドック
- [game-info-mcp-audit-2026-09-27.md](reports/game-info-mcp-audit-2026-09-27.md): 対局情報
- [kifu-management-mcp-audit-2026-09-27.md](reports/kifu-management-mcp-audit-2026-09-27.md): 棋譜管理

## 依頼文（prompts/）

繰り返し使うもの:

- [guide-update-prompts.md](prompts/guide-update-prompts.md): ガイドの更新・新規作成の依頼例
- [multi-os-screenshot-prompts.md](prompts/multi-os-screenshot-prompts.md): Windows・macOS の画面の撮り直し
- [piece-font-request-prompts.md](prompts/piece-font-request-prompts.md): 新しい駒書体の作成
- [windows-kifu-compatibility-test.md](prompts/windows-kifu-compatibility-test.md): Windows での棋譜の相互読み込みの検証

まだ終わっていないもの:

- [task20261013-tcp-keepalive-windows-macos.md](prompts/task20261013-tcp-keepalive-windows-macos.md): TCP キープアライブの Windows・macOS 実機での確認
