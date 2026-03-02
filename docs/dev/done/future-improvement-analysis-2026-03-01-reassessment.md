# 今後の改善点 詳細分析（再評価版, 2026-03-01）

## 1. 目的

本ドキュメントは、ShogiBoardQ の現行コードベースに対して、次の改善投資先を優先度付きで整理するための再評価レポートである。  
既存の品質基盤（テスト/KPI/分割済み設計）を前提に、**「次に詰まるポイント」**を先回りして明確化する。

- 対象: 設計健全性、品質ゲート、保守性、CI運用、回帰耐性
- 非対象: 新機能仕様、UI全面刷新、配布戦略

---

## 2. 評価方法

2026-03-01 時点のワークツリーで、以下を実測した。

- `ctest --test-dir build --output-on-failure -j4`
- `ctest --test-dir build -N`
- `find/rg/wc` による規模・しきい値集計
- `src/app/mainwindow*`、`tests/tst_structural_kpi.cpp`、`tests/tst_layer_dependencies.cpp` の構造確認

---

## 3. 現状サマリ（実測）

### 3.1 品質ゲート・テスト

| 指標 | 実測値 |
|---|---:|
| ctest 登録数 | 56 |
| ctest 実行結果 | 56/56 pass |
| レイヤ依存テスト | pass |
| 構造KPIテスト | pass |

### 3.2 規模・複雑度の概観

| 指標 | 実測値 |
|---|---:|
| `src` 配下 `.cpp/.h` ファイル数 | 589 |
| `src` 配下総行数 | 105,696 |
| 600行超ファイル数 | 0 |
| 550行超ファイル数 | 12 |
| 500行超ファイル数 | 31 |
| 最大行数ファイル | `src/kifu/formats/kiflexer.cpp` (596行) |

補足: 「600行超 0」は大きな成果だが、550行近辺に多くのファイルが密集している。

### 3.3 MainWindow 周辺（依存集中の観測）

| 指標 | 実測値 |
|---|---:|
| `MainWindow` `friend class` 数 | 7 |
| `MainWindow` `public slots` 数 | 13 |
| `MainWindow` raw pointer 風メンバ（`* m_... = nullptr`） | 42 |
| `MainWindow` `std::unique_ptr` メンバ | 31 |
| `MainWindow` `QPointer` メンバ | 6 |
| `mainwindow.cpp` `#include` 数 | 49 |

補足: 責務移譲は進んでいるが、依存のハブである状態は続いている。

---

## 4. 強み（維持すべき点）

1. 量的品質管理が実運用されている  
   `tst_structural_kpi` で規模/設計指標を継続監視できている。

2. 層依存の崩壊を検知できる  
   `tst_layer_dependencies` で `core`/`engine`/`kifu/formats` などの逆流を検出できる。

3. リファクタリング文化が根付いている  
   `docs/dev/done` に連続した分析・実施記録が残っており、改善が単発で終わっていない。

4. テストの回しやすさ  
   56 テストが短時間で完走でき、開発サイクルに乗りやすい。

---

## 5. 改善課題（優先度順）

## 5.1 P0: 構造KPIの検知漏れ修正（最優先）

### 問題

`lambda connect` 件数チェックが「1行内に `connect(` と `[` が同居」するケースのみ検出しており、複数行記述を取りこぼす。  
実際には lambda connect が存在するが、KPIは pass してしまう。

- 検知ロジック: `tests/tst_structural_kpi.cpp` (`lambdaConnectCount`)
- 実在箇所例:
  - `src/app/kifusubregistry.cpp`（`statusMessage` 接続）
  - `src/widgets/recordpane.cpp`（`rowsInserted` / `currentRowChanged`）

### 影響

- 「lambda connect 0件」という品質指標の信頼性が下がる
- 方針違反が蓄積してもCIで止められない

### 改善案

1. 複数行対応の検出へ修正（最低限）
   - `connect(` 以降を `);` までバッファし、その範囲に `[` があるか判定
2. 可能なら AST ベース検出へ移行（中期）
   - `clang-query`/`clang-tidy` ルール化の検討
3. 方針見直し
   - 禁止ではなく「例外許容 + 理由コメント必須」へ変更する選択肢も明記

### 完了条件

- 複数行 lambda connect を `tst_structural_kpi` が検出できる
- 現在残る lambda connect の扱い（禁止 or 例外登録）が文書化される

---

## 5.2 P1: 「600行未満達成後」の次KPI設計

### 問題

`600行超 0` を達成済みのため、既存KPIだけでは改善余地を表現しにくい。  
現在は 550行近辺に複雑度が滞留している。

### 影響

- 見かけ上は健全でも、読み解きコストと変更リスクが高止まりする
- ファイル長以外の複雑度（分岐密度、依存密度）が見えない

### 改善案

1. 新しい閾値を導入
   - `>550` 件数
   - `>500` 件数
2. 「ファイル長」以外の指標を導入
   - 1関数あたり行数上限（例: 80〜120）
   - `#include` 数上限（例: 35）
   - 依存先モジュール数
3. KPIを「監視」と「ゲート」に分離
   - 最初は警告ログ化し、安定後に fail 条件へ昇格

### 完了条件

- `tst_structural_kpi` に次世代指標が追加される
- しきい値の根拠が `docs/dev` に記録される

---

## 5.3 P1: MainWindow 依存ハブの縮退

### 問題

MainWindow の責務委譲は進んだが、依存保持量が依然大きい。

- raw pointer 風メンバ: 42
- `#include`: 49
- `friend class`: 7
- `public slots`: 13（上限値と同水準）

### 影響

- 変更時の影響範囲予測が難しい
- Registry/SubRegistry 側の改修が MainWindow ヘッダに波及しやすい

### 改善案

1. `MainWindowRuntimeRefs` の用途分割
   - `GameRuntimeRefs` / `KifuRuntimeRefs` / `UiRuntimeRefs`
2. MainWindow からの直接代入を縮小
   - 状態更新は `Service` API 経由へ寄せる
3. 非所有参照の意図明確化
   - `QPointer` にすべき対象の見直し
   - raw pointer 維持理由をコメントに固定

### 完了条件

- `mainwindow.cpp` の include 数削減（目安: 49 -> 40以下）
- raw pointer 風メンバ削減（目安: 42 -> 35以下）
- friend 数の横ばい回避（新規追加ゼロ）

---

## 5.4 P1: Registry API の余白確保

### 問題

`ensure*` 系メソッド数が上限ギリギリ運用になりやすく、今後の機能追加時に設計を押し広げる余地が少ない。

### 影響

- 短期実装で `ensure*` を増やしやすく、APIが再肥大化する
- 境界が崩れた際の差分レビューが難化

### 改善案

1. `ensureX()` と `refreshXDeps()` の分離を標準化
2. SubRegistry 側へ責務を追加する際の規約を明記
3. 「追加禁止」ではなく「追加時に削減セット提出」をルール化

### 完了条件

- `MainWindowServiceRegistry` の `ensure*` 新規追加ゼロを維持
- 主要サブレジストリで `refresh*` 命名が統一される

---

## 5.5 P2: テスト戦略の深掘り（仕様境界）

### 問題

テスト数とカバレッジは高いが、次の境界で将来リスクが残る。

- パーサ系（KIF/KI2/CSA/JKF/USI/USEN）の異常入力多様性
- ネットワーク/プロセスの非同期エッジケース
- UI配線の契約逸脱（接続漏れ/重複）

### 改善案

1. コンバータにプロパティテスト/ファジングを導入
2. USI/CSA の異常系シナリオを追加
3. Wiring契約テストを段階拡張

### 完了条件

- 代表フォーマット変換にランダム入力系テストが追加される
- 非同期エラー経路の退行検知が増える

---

## 5.6 P2: CIの「実行しているが保証していない」領域の補強

### 問題

現状でも品質は高いが、次の観点は常時ゲート化の余地がある。

- Sanitizer（ASan/UBSan）
- 依存削減チェック（IWYUの自動検証）
- 翻訳更新（`lupdate` 差分）整合チェック

### 改善案

1. 週次ジョブで Sanitizer を回す（最初は fail させない）
2. include 関連の監視ジョブを導入
3. `tr()` 変更時の翻訳更新漏れをCIで検出

### 完了条件

- CIに週次品質ジョブが追加される
- 主要品質ジョブの失敗理由がドキュメント化される

---

## 6. 推奨ロードマップ

| 期間 | 優先タスク | 成果物 |
|---|---|---|
| 0-2週間 | KPI検知漏れ修正（P0） | `tst_structural_kpi` 改修 + docs更新 |
| 2-6週間 | 次世代KPI導入、MainWindow依存削減着手（P1） | 新KPI + 依存削減PR群 |
| 1-3ヶ月 | Registry運用改善、テスト深掘り（P1/P2） | 追加テスト・境界規約 |
| 3ヶ月以降 | CI高度化（Sanitizer/IWYU/翻訳） | 週次品質ジョブ安定運用 |

---

## 7. KPI提案（改訂案）

## 7.1 継続するKPI

- `files_over_600 == 0`
- `old_style_signal_slot == 0`
- `MainWindow ensure* == 0`
- レイヤ依存違反ゼロ

## 7.2 新規追加を推奨するKPI

1. `files_over_550 <= 10`（現状 12）
2. `files_over_500 <= 25`（現状 31）
3. `mainwindow_include_count <= 45`（現状 49）
4. `mainwindow_raw_ptr_members <= 38`（現状 42）
5. `lambda_connect_count` は複数行対応ロジックで測定

---

## 8. 実行時の注意点

1. 指標だけを先に厳格化しない  
   まずは測定精度を上げ、次にしきい値を下げる。

2. 「禁止ルール」と「例外運用」を分ける  
   特に lambda connect は、全面禁止より根拠付き例外の方が現実的な場合がある。

3. リファクタ時は所有権ルールを同時更新  
   `docs/dev/ownership-guidelines.md` と実装コメントを常に同期する。

---

## 9. 結論

ShogiBoardQ は、テスト・構造KPI・設計分割の3点がそろった強いコードベースであり、  
**現時点の主課題は「大規模な欠陥」ではなく「品質ゲート精度と依存密度の最適化」**である。

次の改善投資は、以下の順が最も費用対効果が高い。

1. KPI検知漏れの解消（P0）
2. 600行達成後の次世代KPI設計（P1）
3. MainWindow依存ハブの段階縮退（P1）

この順序で進めることで、既存の高い品質基盤を維持したまま、将来の機能追加余力を確保できる。
