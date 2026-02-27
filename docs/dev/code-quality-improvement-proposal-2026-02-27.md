# 現在ソースコードの改善提案（2026-02-27）

## 1. 目的

本書は、現在の `ShogiBoardQ` コードベースに対して、保守性・安全性・拡張性をさらに高めるための改善ポイントと実施方法を整理した提案書である。  
機能追加ではなく、構造品質の継続的改善を目的とする。

---

## 2. 現状サマリ（2026-02-27 時点）

### 2.1 定量指標

| 項目 | 現状値 |
|---|---:|
| `src/` の `.cpp/.h` ファイル数 | 494 |
| `src/` の総行数（`.cpp/.h`） | 103,794 |
| 600行超ファイル数 | 34 |
| 1000行超ファイル数 | 8 |
| `connect(...)` 呼び出し数 | 617 |
| lambda を使う `connect(...)` 箇所 | 6 |
| `delete` 出現数（`src/`） | 17 |
| テスト数（`ctest -N`） | 37 |
| `MainWindow` の `friend class` 件数 | 2 |
| 旧式 `SIGNAL()`/`SLOT()` マクロ使用箇所 | 5 |
| ダイアログサイズ保存/復元ボイラープレート重複 | 12 |
| フォント増減スロット重複（`onFontIncrease` 等） | 12 |
| 構造テスト (`tst_layer_dependencies`, `tst_structural_kpi`) | Pass |
| 全テスト (`ctest --output-on-failure`) | 37/37 Pass |

### 2.2 代表的な巨大ファイル（行数上位）

1. `src/network/csagamecoordinator.cpp` (1569)
2. `src/ui/coordinators/kifudisplaycoordinator.cpp` (1412)
3. `src/widgets/engineanalysistab.cpp` (1378)
4. `src/kifu/kifuloadcoordinator.cpp` (1263)
5. `src/game/gamestartcoordinator.cpp` (1181)
6. `src/analysis/analysisflowcontroller.cpp` (1133)
7. `src/engine/usi.cpp` (1084)
8. `src/widgets/evaluationchartwidget.cpp` (1018)

### 2.3 品質ゲートの評価

- 強み:
  - 警告設定が強い（`-Wall -Wextra -Wpedantic -Wshadow -Wconversion` など）
  - 構造テストが存在し、レイヤ依存違反と巨大化の監視を自動化済み
  - CI に Build/Test/Static Analysis/Translation Check があり、基礎品質は高い
- 課題:
  - 巨大ファイル群と司令塔クラス群の責務集中が残存
  - 所有権モデルが混在し、手動 `delete` が散在
  - 一部の複雑領域（棋譜変換、CSA通信、ゲーム開始周辺）の変更コストが高い

---

## 3. 総合所感

現状は「高機能かつ高テスト網羅」で、既に実戦投入レベルの品質にある。  
一方で、今後の機能追加とバグ修正速度を維持するには、以下の3点を先に解消するのが最も費用対効果が高い。

1. 巨大ファイル分割（変更影響範囲の縮小）
2. 司令塔責務の分割（依存点集中の緩和）
3. 所有権モデルの統一（破棄経路の安全化）

---

## 4. 優先度別改善提案

## P0: 直近で着手すべき改善

### P0-1. 巨大ファイル分割の継続

### 対象

- `src/network/csagamecoordinator.cpp`
- `src/ui/coordinators/kifudisplaycoordinator.cpp`
- `src/widgets/engineanalysistab.cpp`
- `src/kifu/kifuloadcoordinator.cpp`
- `src/game/gamestartcoordinator.cpp`
- `src/analysis/analysisflowcontroller.cpp`
- `src/engine/usi.cpp`

### 問題

- 1ファイルに複数責務が同居し、レビュー単位が大きすぎる
- テスト失敗時の切り分け時間が増大しやすい
- 小改修でも副作用調査が広範囲になる

### 改善方法

1. 1ファイル1責務の境界を先に定義する
2. 「状態管理」「I/O」「変換」「UI反映」「配線」を別クラスへ分離する
3. 抽出後のクラス間契約は `Deps` 構造体または明示 setter で固定化する
4. 分割ごとに既存テスト + 対象ユニットテストを追加して回帰を抑える

### 完了条件（段階）

- 第1段階: 1000行超ファイルを `8 -> 4` 以下
- 第2段階: 600行超ファイルを `34 -> 20` 以下
- 第3段階: 例外リスト（`tst_structural_kpi`）の上限を実測に合わせて段階的に引き下げる

---

### P0-2. `GameStartCoordinator` / `MatchCoordinator` の責務再分割

### 対象

- `src/game/gamestartcoordinator.cpp`
- `src/game/matchcoordinator.cpp`
- `src/game/matchcoordinator.h`

### 問題

- 対局開始・時計・終局・USI連携・UI通知が集中しやすい
- Hook 構造体の責務が広く、依存注入が肥大化しやすい

### 改善方法

1. ユースケース単位で司令塔内部を分離する
   - `StartMatchUseCase`
   - `StopMatchUseCase`
   - `HandleEngineEventUseCase`
   - `ApplyTimeControlUseCase`
2. `Hooks` を用途別に分割する
   - UI Hook
   - Time Hook
   - Engine Command Hook
3. `MatchCoordinator` はオーケストレーション専任に限定する
4. 既存の `AnalysisSessionHandler` / `GameEndHandler` と同じ分割パターンを拡張適用する

### 完了条件

- `matchcoordinator.cpp` を 600行台へ圧縮
- `matchcoordinator.h` を 500行以下へ圧縮
- Hook 群の型責務を分離し、未使用コールバックを整理

---

### P0-3. 所有権モデル統一と手動 `delete` 削減

### 対象（例）

- `src/engine/engineprocessmanager.cpp`
- `src/game/gamestartcoordinator.cpp`
- `src/models/kifurecordlistmodel.cpp`
- `src/engine/shogienginethinkingmodel.cpp`
- `src/board/boardinteractioncontroller.cpp`
- `src/dialogs/menuwindow.cpp`
- `src/game/shogigamecontroller.cpp`

### 問題

- `QObject` 親子管理と手動解放の混在で、ライフサイクル追跡が難しい
- エラーパスや早期 return で解放漏れリスクが増える

### 改善方法

1. ルールを明文化する
   - `QObject` 派生: parent ownership を基本
   - 非 `QObject`: `std::unique_ptr` を基本
2. 非所有参照は「所有しない」ことを型または命名で明示する
3. `delete` が必要な箇所は、代替困難な理由をコメントで残す
4. 破棄順序依存がある場合は lifecycle クラスに閉じ込める

### 完了条件

- `src/` の `delete` 件数を `17 -> 8` 以下
- 新規コードで裸 `new/delete` を禁止（Factory/CompositionRoot 経由に限定）

---

## P1: 中期で進める改善

### P1-1. 棋譜コンバータの共通パイプライン化

### 対象

- `src/kifu/formats/kiftosfenconverter.cpp`
- `src/kifu/formats/ki2tosfenconverter.cpp`
- `src/kifu/formats/usitosfenconverter.cpp`
- `src/kifu/formats/usentosfenconverter.cpp`
- `src/kifu/formats/csatosfenconverter.cpp`
- `src/kifu/formats/jkftosfenconverter.cpp`
- `src/kifu/formats/parsecommon.cpp`

### 問題

- 入力形式ごとに重複した分解/検証ロジックが生まれやすい
- 異常系修正の横展開漏れが起きやすい

### 改善方法

1. `Lexer -> Parser -> MoveApplier -> ResultBuilder` の共通パイプラインを定義
2. 共通の数値変換・駒変換・時刻解釈を `parsecommon` に集約
3. フォーマット差分は戦略クラス（policy）で吸収
4. 変換器テストに異常系テーブルを追加

### 完了条件

- 変換器間の重複ロジックを 30%以上削減
- 新フォーマット追加時の実装点を「policy + parser差分」に限定

---

### P1-2. エラーハンドリングの一貫化

### 問題

- 失敗表現が `bool / 空文字 / assert` に分散している
- システム境界（ファイル入力、USI出力）の検証が強弱混在

### 改善方法

1. パース系は `std::optional` を基本にして失敗を型で表現
2. リリースで必要な検証は `Q_ASSERT` 依存を避ける
3. `ErrorBus` 利用方針を統一し、ログカテゴリ単位で通知先を固定
4. 入力境界でバリデーションを実施して内部不変条件を保つ

### 完了条件

- 主要パーサの戻り値契約を統一
- 入力異常時のログ/通知動作をテストで検証

---

### P1-3. `connect` 方針の統一（lambda 接続の最小化）

### 現状

- `connect(...)` 合計: 617
- lambda 接続: 6

### 改善方法

1. 可能な箇所はメンバ関数ポインタ接続へ寄せる
2. lambda が必要な場合は理由をコメントで明示
3. 接続登録を Wiring 層へ集約し、散在を防ぐ

### 完了条件

- lambda 接続の用途を「変換が必要な接着ロジック」に限定
- 接続ポリシーを `docs/dev` に明文化

---

### P1-4. ダイアログのボイラープレート重複解消

### 問題

- ウィンドウサイズ保存/復元パターンが9+ダイアログにほぼ同一のコードで存在する
- フォントサイズ増減パターン（`onFontIncrease` / `onFontDecrease` / `updateFontSize`）が6ダイアログに重複
- 各ダイアログで同じ条件式 `savedSize.isValid() && savedSize.width() > 100 && savedSize.height() > 100` を繰り返している

### 対象（サイズ保存/復元の重複）

- `src/dialogs/josekiwindowui.cpp`
- `src/dialogs/csagamedialog.cpp`
- `src/dialogs/josekimovedialog.cpp`
- `src/dialogs/pvboarddialog.cpp`
- `src/dialogs/kifuanalysisdialog.cpp`
- `src/dialogs/josekimergedialog.cpp`
- `src/dialogs/startgamedialog.cpp`
- `src/dialogs/kifupastedialog.cpp`
- `src/dialogs/sfencollectiondialog.cpp`
- 他（`engineregistrationdialog`, `changeenginesettingsdialog`, `tsumeshogigeneratordialog`）

### 対象（フォント増減の重複）

- `src/dialogs/considerationdialog.cpp`
- `src/dialogs/csagamedialog.cpp`
- `src/dialogs/csawaitingdialog.cpp`（本体+ログの2セット）
- `src/dialogs/kifuanalysisdialog.cpp`
- `src/dialogs/tsumeshogigeneratordialog.cpp`

### 改善方法

1. ウィンドウサイズ保存/復元のユーティリティ関数を作成する
   ```cpp
   // dialogutils.h
   namespace DialogUtils {
       void restoreDialogSize(QWidget* dialog, const QSize& saved, const QSize& fallback);
       void saveDialogSize(QWidget* dialog, std::function<void(const QSize&)> setter);
   }
   ```
2. フォントサイズ管理を共通ミックスインまたはヘルパーとして抽出する
   ```cpp
   class FontSizeHelper {
   public:
       FontSizeHelper(int initial, int min, int max, std::function<void(int)> saveFn);
       void increase();
       void decrease();
       int fontSize() const;
   private:
       int m_fontSize;
       int m_min, m_max;
       std::function<void(int)> m_saveFn;
   };
   ```
3. 各ダイアログのコンストラクタ/クローズ処理をユーティリティ呼び出しに置換
4. デフォルトサイズのマジックナンバーを定数化

### 完了条件

- サイズ保存/復元の同一パターンを1箇所の実装に集約
- フォント増減の重複実装を共通ヘルパーに集約
- ダイアログごとのボイラープレートが各2〜3行に収まる

---

### P1-5. テストカバレッジの空白領域拡充

### 問題

- 37テスト中、UI/ダイアログ/ネットワーク/設定永続化のテストがほぼ存在しない
- ダイアログテスト: `tst_josekiwindow` の1件のみ（17ダイアログ中）
- CSA通信: `tst_csaconverter`（フォーマット変換）のみ、プロトコル状態遷移は未テスト
- Settings永続化: 保存→復元のラウンドトリップテストなし
- Undo/Redo操作: テストなし

### 改善方法

1. CSAプロトコルのメッセージ解析と状態遷移テストを追加
   - 対象: `CsaClient` のパース関数、接続/ログイン/対局状態の遷移
2. ダイアログの設定永続化テストを追加
   - 対象: 代表的なダイアログ（StartGameDialog, EngineRegistrationDialog 等）
   - サイズ保存→復元、フォントサイズ保存→復元の往復確認
3. SettingsService のラウンドトリップテストを追加
   - 対象: 各 namespace（GameSettings, AnalysisSettings, NetworkSettings 等）
   - テンポラリ INI ファイルへの書き込み→読み取り一致確認

### 完了条件

- CSAプロトコル状態遷移テスト追加
- 設定保存/復元のラウンドトリップテスト追加
- テスト数の純増（37 → 42 以上を目安）

---

## P2: 継続改善

### P2-1. 構造テストの拡張

### 改善方法

1. `tst_structural_kpi` に以下の監視を追加
   - 1000行超ファイル数の上限
   - `delete` 件数の上限
   - lambda `connect` 件数の上限
2. 既存の例外リストを「増やさない」方針で運用
3. KPIを超えたPRは fail とする

### 完了条件

- 構造劣化を CI で自動検知し、レビュー依存を減らす

---

### P2-2. CI 品質ゲートの段階強化

### 現状

- 翻訳未完了数: 1193
- CI 閾値: 1201（余裕 8）

### 改善方法

1. 閾値を固定値ではなく「前回値以下」ルールへ段階移行
2. `clang-tidy` 警告ゼロを維持しつつ対象チェックを段階拡張
3. 構造KPIテストを必須ジョブ化

### 完了条件

- 品質後退を未然に止めるゲートが実用レベルで回ること

---

### P2-3. 旧式 `SIGNAL()`/`SLOT()` マクロの排除

### 現状

- 3ファイル、5箇所に旧形式の文字列ベース接続が残存
  - `src/ui/coordinators/positioneditcoordinator.cpp` — `SLOT(finishPositionEditing())`
  - `src/board/positioneditcontroller.cpp` — `SIGNAL(clicked())` / `SLOT()`
  - `src/game/turnsyncbridge.cpp` — `SIGNAL(changed(...))` / `SLOT(onTurnManagerChanged(...))`

### 問題

- 旧形式ではシグナル/スロットの不一致がコンパイル時に検出されない
- 実行時にしかエラーが発覚せず、デバッグコストが高い
- CLAUDE.md のコードスタイルポリシー（メンバ関数ポインタ構文）と不整合

### 改善方法

1. 各箇所をメンバ関数ポインタ構文へ移行する
2. `positioneditcontroller.cpp` の動的 disconnect/connect は、`QMetaObject::Connection` を保持して管理する方式に変更

### 完了条件

- `src/` 内の `SIGNAL()` / `SLOT()` マクロ使用が 0 件
- 構造KPIに旧式接続件数の監視を追加

---

### P2-4. ダイアログデフォルトサイズ定数の集約

### 現状

- 各ダイアログにマジックナンバーとしてデフォルトサイズが埋め込まれている
  - `resize(600, 500)` — josekimergedialog, kifupastedialog
  - `resize(620, 720)` — pvboarddialog
  - `resize(620, 780)` — sfencollectiondialog
  - `resize(600, 550)` — tsumeshogigeneratordialog
  - `resize(950, 600)` — josekiwindow
  - `setMinimumSize(400, 300)` — changeenginesettingsdialog
  - `setMinimumSize(500, 400)` — kifupastedialog, josekimergedialog

### 問題

- サイズの意図（小/中/大ダイアログ）が読み取りにくい
- 変更時に影響箇所の特定が困難

### 改善方法

1. 各ダイアログのデフォルトサイズをクラス内 `constexpr` 定数として定義
   ```cpp
   // 各ダイアログの .cpp ファイル先頭
   namespace {
       constexpr QSize kDefaultSize{600, 500};
       constexpr QSize kMinimumSize{500, 400};
   }
   ```
2. P1-4 のユーティリティ関数と組み合わせ、`resize()` 直呼び出しを置換

### 完了条件

- ダイアログサイズのマジックナンバーが名前付き定数に置換される
- サイズ値の意図がコード上で明確になる

---

## 5. 実行計画（8週間）

### Week 1-2

1. `gamestartcoordinator` と `matchcoordinator` の責務分割設計
2. `delete` 低減の先行PR（安全な箇所から）
3. 構造KPI拡張のテスト追加

### Week 3-4

1. 巨大ファイル上位3件の分割実装
2. 棋譜コンバータ共通化の土台（`parsecommon` 強化）
3. Hook 分割と依存注入の整理

### Week 5-6

1. 変換器異常系テスト拡充
2. CI 閾値調整（翻訳・構造KPI）
3. 例外リスト見直しと上限の再設定
4. ダイアログボイラープレート共通化（P1-4）
5. 旧式 SIGNAL/SLOT 排除（P2-3）
6. テストカバレッジ空白領域の拡充（P1-5）

---

## 6. 最初に着手するPR案（推奨順）

1. `MatchCoordinator` 分割PR  
   目的: 司令塔責務の縮小、影響範囲の明確化

2. 所有権統一PR  
   目的: `delete` 削減、破棄経路の安全化

3. 構造KPI強化PR  
   目的: 改善の後戻り防止

4. コンバータ共通化PR（第一段）  
   目的: 重複ロジック削減と異常系統一

---

## 7. 追跡KPI

| KPI | 初期値 | 実績(2026-02-28) | 目標 | 達成 |
|---|---:|---:|---:|:---:|
| 1000行超ファイル数 | 8 | 3 | 4以下 | **達成** |
| 600行超ファイル数 | 34 | 32 | 20以下 | 未達 |
| `delete` 件数 | 17 | 5 | 8以下 | **達成** |
| lambda `connect` 件数 | 6 | 0 | 3以下 | **達成** |
| 旧式 `SIGNAL()`/`SLOT()` 件数 | 5 | 0 | 0 | **達成** |
| ダイアログボイラープレート重複箇所 | 12 | 12 | 共通化により各2〜3行 | 未達 |
| 翻訳未完了数 | 1193 | 1216 | 継続減少（回帰禁止） | 未達(+23) |
| テスト数 | 37 | 41 | 42以上 | 未達(残1) |
| 全テスト成功率 | 100% | - | 維持 | - |

---

## 8. 実施上の注意

1. 1PR 1責務を徹底し、機能変更とリファクタを混在させない
2. 既存テスト + 対象追加テストを同時に入れる
3. 分割前後で public API 契約を明確化し、依存先の意図しない変更を防ぐ
4. 例外リストを増やす場合は、期限付きで「削減計画」を同時に書く

---

## 9. 補足

本提案は 2026-02-27 時点のローカル計測値に基づく。  
将来の再計測では、まず `tests/tst_structural_kpi.cpp` の上限値と実測差分を確認し、実装計画を更新すること。
