# 現在コードベースの改善計画（2026-02-27）

## 目的
- 現在の「高機能・高テスト網羅」状態を維持したまま、保守コストの高い箇所を段階的に削減する。
- 設計の到達点を「機能拡張しやすい最小責務構造」に置く。

## 対象範囲
- `src/` の設計・責務分割・依存方向
- `tests/` の構造テストと回帰テスト運用
- `CMakeLists.txt` / CI の品質ゲート

## 現状サマリ（確認結果）
- テストは `31` 件あり、`ctest --test-dir build --output-on-failure` で全件成功。
- 警告ポリシーは強い（`-Wall -Wextra -Wpedantic -Wshadow -Wconversion` ほか）。
- CI で build/test/clang-tidy/翻訳チェックが実行される。
- 一方で、巨大クラス・巨大ファイル・依存点集中は残っている。

## 改善箇所と改善方法

### 1. `MainWindow` 周辺の責務集中をさらに解体する
対象:
- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- `src/app/mainwindowserviceregistry.*`
- `src/app/mainwindow*registry.*`

現状課題:
- `mainwindow.h` は 500 行超、`friend class` が多く、`ensure*` が多数存在する。
- 「Facade化」は進んでいるが、依然として依存の中心点になっている。

改善方法:
1. `MainWindow` を「UIルート + ルーティング」に限定する。
2. `ensure*` の生成責務を `MainWindowServiceRegistry` 系へ強制移管する。
3. `MainWindowRuntimeRefs` を用途別に分割する。
4. `friend` を段階的に削減し、必要なアクセスは `public/protected` API へ置換する。
5. `MainWindow` 内の「状態保持」を縮小し、セッション状態は専用 state holder に移す。

完了条件:
- `mainwindow.h` の行数を 350 行以下。
- `friend class` を 11 -> 3 以下。
- `ensure*` を 38 -> 12 以下。
- `MainWindow` が自前で `new` を行わない状態にする。

---

### 2. 巨大ファイルの分割（`josekiwindow.cpp` と棋譜変換系）
対象:
- `src/dialogs/josekiwindow.cpp`（約 2400 行）
- `src/kifu/formats/kiftosfenconverter.cpp`（約 1200 行）
- `src/kifu/formats/ki2tosfenconverter.cpp`（約 1500 行）
- `src/kifu/formats/csatosfenconverter.cpp`
- `src/kifu/formats/jkftosfenconverter.cpp`

現状課題:
- UI構築・イベント処理・ファイルI/O・変換ロジックが同居。
- 変更影響範囲が広く、レビューと障害切り分けに時間がかかる。

改善方法（JosekiWindow）:
1. `JosekiWindowView`（表示のみ）を `.ui` + 薄い QWidget クラスにする。
2. `JosekiPresenter` に状態遷移・操作フローを集約。
3. `JosekiRepository` にファイルI/Oとフォーマット変換を集約。
4. `QTableWidget` 直操作を `QAbstractTableModel` ベースへ移行する。

改善方法（Converter）:
1. 各フォーマットを共通パイプライン化する（`Lexer -> Parser -> MoveApplier -> Formatter`）。
2. 終局語・コメント・時間解析などの共通処理を `parsecommon` に集約。
3. パース例外ケースをテーブル駆動テスト化し、回帰容易性を上げる。

完了条件:
- 1ファイル 600 行超を原則禁止。
- 変換器は 1クラス1責務（字句解析/構文解析/適用）に分割。
- 各変換器で共通処理重複を 50%以上削減。

---

### 3. 所有権モデルの統一（raw pointer 依存の低減）
対象:
- `src/app/` `src/ui/` `src/game/` `src/kifu/` の `new` を伴う箇所

現状課題:
- `QObject parent` 管理と `unique_ptr` 管理が混在し、ライフサイクル追跡が難しい箇所がある。
- 「非所有ポインタ」の注釈はあるが、構造的保証が弱い。

改善方法:
1. 原則を明文化する。
   - `QObject` 派生は parent ownership を基本とする。
   - 非QObjectは `std::unique_ptr` を基本とする。
2. 非所有参照は `T*` より `T&` / `observer_ptr`（導入可能なら）を優先。
3. 生成箇所を Factory/CompositionRoot に限定し、他レイヤでの直接 `new` を禁止。
4. 破棄順依存がある箇所は `lifecycle` クラスに隔離する。

完了条件:
- 生成責務が `*factory*` / `*composition*` / `*wiring*` へ集中。
- 「どこが所有するか不明」のコメントを削除可能な状態にする。

---

### 4. 依存方向の固定（レイヤ境界の静的チェック）
対象:
- `src/ui` `src/app` `src/game` `src/kifu` `src/core` `src/services`

現状課題:
- 実装上は分割されているが、将来の逆流（境界違反）を防ぐ仕組みが弱い。

改善方法:
1. 依存ルールを定義する（例）:
   - `core` は `ui/app` を参照しない
   - `kifu/formats` は `dialogs/widgets` を参照しない
2. `tests/architecture/` に include 依存検証テストを追加（`rg` で禁止 include を検出）。
3. ルール違反は CI で fail にする。

完了条件:
- 依存方向違反を CI で自動検出できる。
- レビューで「構造違反」を人手確認しなくてもよい状態に近づける。

---

### 5. テスト戦略の強化（機能テスト + 構造テスト）
対象:
- `tests/CMakeLists.txt`
- `tests/tst_*`

現状課題:
- 機能テストは強いが、構造的劣化（巨大化・循環依存）を止めるテストが不足。

改善方法:
1. 「構造KPIテスト」を追加:
   - ファイル行数上限
   - 1クラスあたり公開メソッド上限
   - `friend class` 上限
2. Converter 系に対して property-based 的な異常系生成テストを追加。
3. UIロジックは Presenter 単体テストに移し、Widgets テスト依存を下げる。

完了条件:
- 機能だけでなく、設計劣化もテストで検知できる。

---

### 6. CI の厳格化（警告と翻訳の扱い）
対象:
- `.github/workflows/ci.yml`
- `CMakeLists.txt`

現状課題:
- 翻訳未完了は warning 扱いで、品質ゲートとしては弱い。

改善方法:
1. 翻訳未完了数の閾値を段階的に 0 へ近づける（最終は fail）。
2. `clang-tidy` のチェックセットを段階拡張する。
3. 主要ブランチは `Release + BUILD_TESTING=ON` に加えて `Debug` も定期実行。

完了条件:
- 「警告の常態化」を防ぎ、失敗条件が明確な CI になる。

## 優先度付き実行計画

### フェーズ1（短期: 1〜2週間）
1. `MainWindow` の `ensure*` と `friend` 削減の第一段。
2. `JosekiWindow` の View/Presenter 分離の土台作成。
3. 構造KPIテストを追加（行数・friend・依存違反）。

### フェーズ2（中期: 2〜4週間）
1. Converter パイプライン分割（KIF -> KI2 -> CSA -> JKF の順）。
2. 所有権統一ルールの適用と `new` 発生箇所の集約。
3. CI の翻訳/静的解析ゲート強化。

### フェーズ3（継続）
1. 巨大ファイル上限の運用定着。
2. 新機能追加時の設計逸脱を構造テストで自動阻止。

## KPI（追跡指標）
- `src/app/mainwindow.h` 行数
- `MainWindow` の `friend class` 件数
- `MainWindow` の `ensure*` 件数
- 600行超ファイル数
- 変換器間の重複コード量
- CI 警告件数（clang-tidy / 翻訳未完）

## 実施時の注意
- 一度に大規模移動しない（1PR 1責務）。
- 機能仕様の変更とリファクタを同一PRで混ぜない。
- 分割後は必ず `ctest --output-on-failure` を実行し、回帰を即検知する。

## 最初の3PR案
1. `MainWindow` 指標改善PR（`friend/ensure` 削減 + 依存注入整理）
2. `JosekiWindow` 分割PR（View/Presenter/Repository）
3. `KIF Converter` パイプライン導入PR（共通処理の抽出 + テスト追加）

