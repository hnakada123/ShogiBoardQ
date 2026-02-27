# 改善提案の実行タスク分解（Issue/PR単位, 2026-02-27）

## 1. 目的

`docs/dev/code-quality-improvement-proposal-2026-02-27.md` を、実際に着手できる Issue/PR 単位へ分解する。  
本書は「そのまま Issue 作成・PR 作成に使えること」を目的とする。

---

## 2. 現在の基準値（開始時点）

| 指標 | 値 |
|---|---:|
| `src` `.cpp/.h` 総行数 | 103,794 |
| 600行超ファイル数 | 34 |
| 1000行超ファイル数 | 8 |
| `delete` 件数（`src`） | 17 |
| `connect(...)` 総数 | 617 |
| lambda `connect(...)` 件数 | 6 |
| 旧式 `SIGNAL()`/`SLOT()` 件数 | 5 |
| ダイアログサイズ保存/復元ボイラープレート重複 | 12 |
| フォント増減スロット重複 | 12 |
| テスト数 | 37 |
| テスト成功率 | 100% |

---

## 3. 実行順（着手順）

### フェーズA: ガードレール先行（後戻り防止）

1. `ISSUE-001` 構造KPI拡張（1000行超/`delete`/lambda `connect`）
2. `ISSUE-002` CI への構造KPI必須化
3. `ISSUE-003` 所有権ルール文書化 + PRチェック項目追加

### フェーズB: 高リスク領域の分割（司令塔）

4. `ISSUE-010` `MatchCoordinator` 分割設計
5. `ISSUE-011` `MatchCoordinator` 実装分割（UseCase抽出）
6. `ISSUE-012` `GameStartCoordinator` 分割と再配線
7. `ISSUE-013` 対局開始/終了フロー回帰テスト強化

### フェーズC: 巨大ファイル上位の削減

8. `ISSUE-020` `csagamecoordinator.cpp` 分割
9. `ISSUE-021` `kifudisplaycoordinator.cpp` 分割
10. `ISSUE-022` `engineanalysistab.cpp` 分割
11. `ISSUE-023` `kifuloadcoordinator.cpp` 分割

### フェーズD: 変換器共通化と異常系強化

12. `ISSUE-030` Converter共通パイプライン設計
13. `ISSUE-031` `parsecommon` への共通抽出（第1段）
14. `ISSUE-032` KIF/KI2 移行
15. `ISSUE-033` CSA/JKF/USI/USEN 移行
16. `ISSUE-034` 変換器異常系テスト拡張

### フェーズE: 所有権統一の仕上げ

17. `ISSUE-040` `delete` 削減バッチ1（低リスク）
18. `ISSUE-041` `delete` 削減バッチ2（中リスク）
19. `ISSUE-042` `delete` 削減バッチ3（高リスク）

### フェーズF: ダイアログ品質・テスト拡充

20. `ISSUE-060` ダイアログボイラープレート共通ユーティリティ設計
21. `ISSUE-061` ダイアログサイズ保存/復元の共通化実装
22. `ISSUE-062` フォントサイズ増減パターンの共通化実装
23. `ISSUE-063` ダイアログデフォルトサイズ定数化
24. `ISSUE-064` 旧式 SIGNAL/SLOT マクロ排除
25. `ISSUE-065` テストカバレッジ拡充（CSAプロトコル・設定永続化）

### フェーズG: 収束

26. `ISSUE-050` KPI再計測と閾値引き下げ
27. `ISSUE-051` 例外リスト整理（`tst_structural_kpi`）

---

## 4. Issue 一覧（Issue作成用）

## ISSUE-001: 構造KPIテスト拡張

- 目的:
  - 構造劣化をCIで即検知する
- 対象:
  - `tests/tst_structural_kpi.cpp`
- 実施内容:
  - 1000行超ファイル数の上限チェック追加
  - `delete` 件数の上限チェック追加
  - lambda `connect` 件数の上限チェック追加
- DoD:
  - テストがローカル/CIで安定実行
  - 上限値は「現状値 + 最小マージン」で設定
- 予定PR:
  - `PR-001-a` KPI追加
  - `PR-001-b` テストメッセージ整備
- 依存:
  - なし

## ISSUE-002: CIで構造KPIを必須化

- 目的:
  - ルールを運用に固定する
- 対象:
  - `.github/workflows/ci.yml`
- 実施内容:
  - 構造テストを必須ジョブ化
  - fail時の要約出力改善
- DoD:
  - PRで構造違反時にCIが確実にFail
- 予定PR:
  - `PR-002-a` CIジョブ整理
- 依存:
  - `ISSUE-001`

## ISSUE-003: 所有権ルール明文化

- 目的:
  - `new/delete` 混在の拡大を止める
- 対象:
  - `docs/dev/` 配下に ownership ガイド追加
  - `AGENTS.md` / 開発ガイドにチェック項目追記（必要なら）
- 実施内容:
  - QObject/非QObjectの所有権ルール明文化
  - PRレビュー観点（所有者・解放責務・破棄順）追加
- DoD:
  - 新規PRで所有権判断が再現可能
- 予定PR:
  - `PR-003-a` ドキュメント追加
- 依存:
  - なし

## ISSUE-010: MatchCoordinator分割設計

- 目的:
  - 司令塔の責務をユースケース単位に分解する設計を固定
- 対象:
  - `src/game/matchcoordinator.h`
  - `src/game/matchcoordinator.cpp`
  - `docs/dev/`
- 実施内容:
  - UseCase候補と責務境界を文書化
  - Hook群を UI/Time/EngineCommand に分割設計
- DoD:
  - 設計文書 + 影響ファイル一覧 + 移行手順が確定
- 予定PR:
  - `PR-010-a` 設計文書のみ
- 依存:
  - `ISSUE-001`, `ISSUE-002`

## ISSUE-011: MatchCoordinator実装分割（UseCase抽出）

- 目的:
  - `matchcoordinator.cpp` の責務集中を実装上で削減
- 対象:
  - `src/game/matchcoordinator.*`
  - 新規 `src/game/*usecase*`（必要分）
- 実施内容:
  - 開始・停止・イベント処理・時間設定を UseCase クラスへ抽出
  - `MatchCoordinator` はオーケストレータ専任に縮小
- DoD:
  - 振る舞い互換（全テストPass）
  - `matchcoordinator.cpp` 行数の純減
- 予定PR:
  - `PR-011-a` Hook分割
  - `PR-011-b` Start/Stop UseCase 抽出
  - `PR-011-c` EngineEvent/TimeControl UseCase 抽出
- 依存:
  - `ISSUE-010`

## ISSUE-012: GameStartCoordinator分割と再配線

- 目的:
  - 開始フローと配線責務を分離し保守容易性を上げる
- 対象:
  - `src/game/gamestartcoordinator.*`
  - `src/ui/wiring/matchcoordinatorwiring.*`
- 実施内容:
  - ダイアログ値解釈、開始準備、配線処理を分離
  - `createAndWireMatch()` の肥大化を抑制
- DoD:
  - 対局開始フロー互換
  - 行数削減 + テスト通過
- 予定PR:
  - `PR-012-a` 開始準備の抽出
  - `PR-012-b` wiring 分離
- 依存:
  - `ISSUE-011`

## ISSUE-013: 対局フロー回帰テスト強化

- 目的:
  - 分割後の回帰を先回りで検知する
- 対象:
  - `tests/tst_game_start_flow.cpp`
  - `tests/tst_preset_gamestart_cleanup.cpp`
  - `tests/tst_integration.cpp`
- 実施内容:
  - 開始/再開/中断/終局/再開始のケース拡張
- DoD:
  - 分割に対する防波堤テストが追加される
- 予定PR:
  - `PR-013-a` テストケース追加
- 依存:
  - `ISSUE-011`, `ISSUE-012`

## ISSUE-020: csagamecoordinator 分割

- 目的:
  - 最大級ファイルを分割し変更リスクを下げる
- 対象:
  - `src/network/csagamecoordinator.cpp`
- 実施内容:
  - 通信状態管理 / UI通知 / 対局ロジックを分離
- DoD:
  - 既存通信テスト・関連テスト通過
  - 行数上限の段階改善
- 予定PR:
  - `PR-020-a` state抽出
  - `PR-020-b` command/response処理抽出
- 依存:
  - `ISSUE-001`, `ISSUE-002`

## ISSUE-021: kifudisplaycoordinator 分割

- 目的:
  - UI表示制御の責務集中を緩和
- 対象:
  - `src/ui/coordinators/kifudisplaycoordinator.cpp`
- 実施内容:
  - 表示計算 / 選択状態同期 / 反映処理を分割
- DoD:
  - UI表示整合テストを維持
- 予定PR:
  - `PR-021-a` presenter抽出
  - `PR-021-b` state同期抽出
- 依存:
  - `ISSUE-013`

## ISSUE-022: engineanalysistab 分割

- 目的:
  - Widgetの肥大化抑制
- 対象:
  - `src/widgets/engineanalysistab.cpp`
- 実施内容:
  - タブ構築 / 更新 / イベント処理を分離
- DoD:
  - 解析タブ挙動が互換
- 予定PR:
  - `PR-022-a` UI構築分離
  - `PR-022-b` 更新ロジック分離
- 依存:
  - `ISSUE-021`

## ISSUE-023: kifuloadcoordinator 分割

- 目的:
  - 棋譜ロードのI/O責務と適用責務を分ける
- 対象:
  - `src/kifu/kifuloadcoordinator.cpp`
- 実施内容:
  - ファイル読込・変換・反映の3段へ分割
- DoD:
  - 棋譜ロード系テスト全通過
- 予定PR:
  - `PR-023-a` I/O層抽出
  - `PR-023-b` 適用層抽出
- 依存:
  - `ISSUE-021`

## ISSUE-030: Converter共通パイプライン設計

- 目的:
  - 形式間重複の削減指針を固定
- 対象:
  - `docs/dev/`
  - `src/kifu/formats/*`
- 実施内容:
  - `Lexer -> Parser -> MoveApplier -> ResultBuilder` 契約定義
  - 移行順序を決定
- DoD:
  - 設計文書合意済み
- 予定PR:
  - `PR-030-a` 設計文書のみ
- 依存:
  - `ISSUE-001`, `ISSUE-002`

## ISSUE-031: parsecommon 抽出（第1段）

- 目的:
  - 重複する基礎処理を共通化
- 対象:
  - `src/kifu/formats/parsecommon.*`
  - 各 converter
- 実施内容:
  - 数値変換、座標変換、駒表記変換、時間解釈の共通関数化
- DoD:
  - 変換器テストが全通過
  - 重複行が削減
- 予定PR:
  - `PR-031-a` utility抽出
- 依存:
  - `ISSUE-030`

## ISSUE-032: KIF/KI2 移行

- 目的:
  - 主要フォーマットを新パイプラインへ移す
- 対象:
  - `kiftosfenconverter.cpp`
  - `ki2tosfenconverter.cpp`
- 実施内容:
  - 共通部利用へ置換
  - 振る舞い差分の吸収
- DoD:
  - `tst_kifconverter` / `tst_ki2converter` 全通過
- 予定PR:
  - `PR-032-a` KIF移行
  - `PR-032-b` KI2移行
- 依存:
  - `ISSUE-031`

## ISSUE-033: CSA/JKF/USI/USEN 移行

- 目的:
  - 残りフォーマットを段階移行し重複削減を完了
- 対象:
  - `csatosfenconverter.cpp`
  - `jkftosfenconverter.cpp`
  - `usitosfenconverter.cpp`
  - `usentosfenconverter.cpp`
- 実施内容:
  - 各フォーマットを順次移行
  - 互換性差分を回帰テストで保証
- DoD:
  - 全 converter テスト通過
- 予定PR:
  - `PR-033-a` CSA/JKF
  - `PR-033-b` USI/USEN
- 依存:
  - `ISSUE-032`

## ISSUE-034: Converter異常系テスト拡張

- 目的:
  - 例外入力への堅牢性を強化
- 対象:
  - `tests/tst_*converter.cpp`
  - `tests/tst_parsecommon.cpp`
- 実施内容:
  - 異常ケーステーブル追加
  - 境界値ケース追加
- DoD:
  - 異常系が明示的に担保される
- 予定PR:
  - `PR-034-a` 異常系テスト追加
- 依存:
  - `ISSUE-033`

## ISSUE-040: delete削減バッチ1（低リスク）

- 目的:
  - 安全な箇所から手動解放を除去
- 対象:
  - `src/engine/engineprocessmanager.cpp`
  - `src/board/boardinteractioncontroller.cpp`
- 実施内容:
  - parent ownership/スマートポインタへの置換
- DoD:
  - 振る舞い互換 + テスト通過
- 予定PR:
  - `PR-040-a` 低リスク2ファイル
- 依存:
  - `ISSUE-003`

## ISSUE-041: delete削減バッチ2（中リスク）

- 目的:
  - モデル/ダイアログ領域の所有権を統一
- 対象:
  - `src/models/kifurecordlistmodel.cpp`
  - `src/dialogs/menuwindow.cpp`
  - `src/engine/shogienginethinkingmodel.cpp`
- 実施内容:
  - 所有者の明確化
  - 解放責務の単一化
- DoD:
  - 二重解放/リーク回避が確認できる
- 予定PR:
  - `PR-041-a` model側
  - `PR-041-b` dialog/engine側
- 依存:
  - `ISSUE-040`

## ISSUE-042: delete削減バッチ3（高リスク）

- 目的:
  - 対局ライフサイクル中心部の手動解放を縮小
- 対象:
  - `src/game/gamestartcoordinator.cpp`
  - `src/game/shogigamecontroller.cpp`
- 実施内容:
  - ライフサイクル統合クラスへ解放責務を集約
- DoD:
  - 対局開始/終了/再開始の回帰無し
- 予定PR:
  - `PR-042-a` game start側
  - `PR-042-b` game controller側
- 依存:
  - `ISSUE-012`, `ISSUE-013`, `ISSUE-041`

## ISSUE-060: ダイアログボイラープレート共通ユーティリティ設計

- 目的:
  - ダイアログ間の重複パターンを共通化するための設計を固定
- 対象:
  - `src/dialogs/` 配下
  - 新規 `src/common/dialogutils.h/.cpp`（または同等）
- 実施内容:
  - サイズ保存/復元（12箇所の重複）の共通関数設計
  - フォント増減（6ダイアログ）の共通ヘルパー設計
  - 各ダイアログの適用方式（ユーティリティ関数 vs ミックスイン）を決定
- DoD:
  - 設計文書 + 共通インターフェース案が確定
- 予定PR:
  - `PR-060-a` 設計文書のみ
- 依存:
  - なし

## ISSUE-061: ダイアログサイズ保存/復元の共通化実装

- 目的:
  - 12箇所の `savedSize.isValid() && savedSize.width() > 100 ...` 重複を解消
- 対象:
  - `src/dialogs/josekiwindowui.cpp`
  - `src/dialogs/csagamedialog.cpp`
  - `src/dialogs/josekimovedialog.cpp`
  - `src/dialogs/pvboarddialog.cpp`
  - `src/dialogs/kifuanalysisdialog.cpp`
  - `src/dialogs/josekimergedialog.cpp`
  - `src/dialogs/startgamedialog.cpp`
  - `src/dialogs/kifupastedialog.cpp`
  - `src/dialogs/sfencollectiondialog.cpp`
  - `src/dialogs/engineregistrationdialog.cpp`
  - `src/dialogs/changeenginesettingsdialog.cpp`
  - `src/dialogs/tsumeshogigeneratordialog.cpp`
- 実施内容:
  - 共通ユーティリティ関数を実装
  - 各ダイアログのコンストラクタ・closeEvent を共通関数呼び出しに置換
- DoD:
  - 12箇所の重複が共通関数呼び出しに統一
  - 振る舞い互換（全テストPass）
- 予定PR:
  - `PR-061-a` 共通関数追加 + 対象ダイアログ6件移行
  - `PR-061-b` 残りダイアログ移行
- 依存:
  - `ISSUE-060`

## ISSUE-062: フォントサイズ増減パターンの共通化実装

- 目的:
  - 6ダイアログの `onFontIncrease/onFontDecrease/updateFontSize` 重複を解消
- 対象:
  - `src/dialogs/considerationdialog.cpp`
  - `src/dialogs/csagamedialog.cpp`
  - `src/dialogs/csawaitingdialog.cpp`
  - `src/dialogs/kifuanalysisdialog.cpp`
  - `src/dialogs/tsumeshogigeneratordialog.cpp`
- 実施内容:
  - `FontSizeHelper` クラスまたは共通関数を実装
  - 各ダイアログのフォント管理を共通ヘルパー経由に置換
  - フォントサイズ範囲（min=8, max=24）の定数化
- DoD:
  - フォント管理ロジックが1箇所に集約
  - 各ダイアログのフォント関連メソッドがヘルパー委譲に簡素化
- 予定PR:
  - `PR-062-a` FontSizeHelper 追加 + 移行
- 依存:
  - `ISSUE-060`

## ISSUE-063: ダイアログデフォルトサイズ定数化

- 目的:
  - ダイアログサイズのマジックナンバーを名前付き定数に置換
- 対象:
  - `src/dialogs/` 配下の `resize()` / `setMinimumSize()` 呼び出し
- 実施内容:
  - 各ダイアログの .cpp ファイルに `constexpr QSize kDefaultSize` / `constexpr QSize kMinimumSize` を定義
  - `resize(600, 500)` → `resize(kDefaultSize)` への置換
- DoD:
  - ダイアログサイズの数値リテラルが名前付き定数に置換
- 予定PR:
  - `PR-063-a` 定数化
- 依存:
  - `ISSUE-061`

## ISSUE-064: 旧式 SIGNAL/SLOT マクロ排除

- 目的:
  - 残存する旧形式接続をメンバ関数ポインタ構文に統一
- 対象:
  - `src/ui/coordinators/positioneditcoordinator.cpp` (1箇所)
  - `src/board/positioneditcontroller.cpp` (2箇所)
  - `src/game/turnsyncbridge.cpp` (1箇所)
- 実施内容:
  - `SIGNAL()` / `SLOT()` マクロを `&Class::method` 構文に移行
  - 動的 disconnect/connect は `QMetaObject::Connection` 管理に変更
- DoD:
  - `src/` 内の `SIGNAL()` / `SLOT()` マクロ使用が 0 件
  - 構造KPIに旧式接続の監視を追加
- 予定PR:
  - `PR-064-a` 3ファイル移行
- 依存:
  - `ISSUE-001`

## ISSUE-065: テストカバレッジ拡充（CSAプロトコル・設定永続化）

- 目的:
  - テスト空白領域を埋め、回帰検知力を強化
- 対象:
  - 新規 `tests/tst_csaprotocol.cpp`
  - 新規 `tests/tst_settings_roundtrip.cpp`
- 実施内容:
  - CSAプロトコルのメッセージパース・状態遷移テスト追加
  - SettingsService の保存→復元ラウンドトリップテスト追加
    - GameSettings, AnalysisSettings, NetworkSettings 等の主要 namespace を対象
    - テンポラリ INI ファイルへの書き込み→読み取り一致確認
- DoD:
  - CSAプロトコルの基本メッセージが自動テストでカバーされる
  - 設定の保存→復元が型ごとに検証される
  - テスト数 37 → 39 以上
- 予定PR:
  - `PR-065-a` CSAプロトコルテスト
  - `PR-065-b` 設定ラウンドトリップテスト
- 依存:
  - `ISSUE-001`, `ISSUE-002`

## ISSUE-050: KPI再計測と閾値引き下げ

- 目的:
  - 改善結果を品質ゲートへ反映
- 対象:
  - `tests/tst_structural_kpi.cpp`
  - `docs/dev/code-quality-improvement-proposal-2026-02-27.md`
- 実施内容:
  - 実測値を再取得し上限値を引き下げ
  - 旧式 SIGNAL/SLOT 件数、ダイアログ重複指標も追加
  - 目標未達箇所は次期Issueへ繰り越し
- DoD:
  - 閾値が現実に即して更新される
- 予定PR:
  - `PR-050-a` KPI更新
- 依存:
  - `ISSUE-020` 〜 `ISSUE-042`, `ISSUE-060` 〜 `ISSUE-065`

## ISSUE-051: 例外リスト整理（knownLargeFiles）

- 目的:
  - 例外常態化を防ぐ
- 対象:
  - `tests/tst_structural_kpi.cpp`
- 実施内容:
  - 縮小済みファイルの上限を実測へ更新
  - 削除できる例外を削除
- DoD:
  - 例外リストが実態と一致
- 予定PR:
  - `PR-051-a` 例外更新
- 依存:
  - `ISSUE-050`

---

## 5. PR作成ルール（本計画向け）

1. 1PR 1責務
2. 機能変更とリファクタを混在させない
3. 変更PRには以下を必須記載
   - 変更前後の行数差分（対象ファイル）
   - 影響テスト一覧
   - 互換性の確認結果
4. マージ条件
   - Build 成功
   - `ctest --output-on-failure` 成功
   - 構造KPIテスト成功

---

## 6. 最初の 5 PR（推奨）

1. `PR-001-a` 構造KPI追加（1000行超/`delete`/lambda `connect`）
2. `PR-002-a` CIで構造KPI必須化
3. `PR-010-a` MatchCoordinator 分割設計書
4. `PR-011-a` MatchCoordinator Hook分割
5. `PR-011-b` MatchCoordinator Start/Stop UseCase 抽出

---

## 7. 週次の完了判定

### Week 1 完了条件

- `ISSUE-001`〜`ISSUE-003` 完了
- `ISSUE-010` 着手済み

### Week 2 完了条件

- `ISSUE-011` の主要PR（`PR-011-a`, `PR-011-b`）完了

### Week 3-4 完了条件

- `ISSUE-012`, `ISSUE-013`, `ISSUE-020`, `ISSUE-021` 完了

### Week 5 完了条件

- `ISSUE-022`, `ISSUE-023`, `ISSUE-030`, `ISSUE-031`, `ISSUE-032` 完了

### Week 6 完了条件

- `ISSUE-033`, `ISSUE-034`, `ISSUE-040`〜`ISSUE-042` 完了

### Week 7-8 完了条件

- `ISSUE-060`〜`ISSUE-065` 完了
- `ISSUE-050`, `ISSUE-051` 完了

---

## 8. トラッキングテンプレート（運用用）

| ID | 状態 | 担当 | PR | 指標影響 | 備考 |
|---|---|---|---|---|---|
| ISSUE-001 | TODO | - | - | KPI |  |
| ISSUE-002 | TODO | - | - | CI |  |
| ISSUE-003 | TODO | - | - | Ownership |  |
| ISSUE-010 | TODO | - | - | Coordinator |  |
| ISSUE-011 | TODO | - | - | Coordinator |  |
| ISSUE-012 | TODO | - | - | Coordinator |  |
| ISSUE-013 | TODO | - | - | Test |  |
| ISSUE-020 | TODO | - | - | 1000行超削減 |  |
| ISSUE-021 | TODO | - | - | 1000行超削減 |  |
| ISSUE-022 | TODO | - | - | 1000行超削減 |  |
| ISSUE-023 | TODO | - | - | 1000行超削減 |  |
| ISSUE-030 | TODO | - | - | Converter |  |
| ISSUE-031 | TODO | - | - | Converter |  |
| ISSUE-032 | TODO | - | - | Converter |  |
| ISSUE-033 | TODO | - | - | Converter |  |
| ISSUE-034 | TODO | - | - | Test |  |
| ISSUE-040 | TODO | - | - | delete削減 |  |
| ISSUE-041 | TODO | - | - | delete削減 |  |
| ISSUE-042 | TODO | - | - | delete削減 |  |
| ISSUE-060 | TODO | - | - | Dialog重複 |  |
| ISSUE-061 | TODO | - | - | Dialog重複 |  |
| ISSUE-062 | TODO | - | - | Dialog重複 |  |
| ISSUE-063 | TODO | - | - | Dialog定数化 |  |
| ISSUE-064 | TODO | - | - | SIGNAL/SLOT |  |
| ISSUE-065 | TODO | - | - | Test |  |
| ISSUE-050 | TODO | - | - | KPI更新 |  |
| ISSUE-051 | TODO | - | - | 例外整理 |  |
