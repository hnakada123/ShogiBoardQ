# MainWindow責務分割の次作業メモ（1・2）

## 目的
`MainWindow`の肥大化を段階的に解消するため、次に実施する2作業を具体化する。

## 1. DialogCoordinator側の重複ロジック統合

### 背景
`MainWindow`から切り出した検討局面解決ロジックと、`DialogCoordinator`内の検討開始ロジックに重複がある。

### 対象
- `src/ui/coordinators/dialogcoordinator.cpp`
- `src/ui/coordinators/dialogcoordinator.h`
- `src/kifu/considerationpositionresolver.h`
- `src/kifu/considerationpositionresolver.cpp`

### 実施タスク
1. `ConsiderationPositionResolver`の入力構造を見直し、`DialogCoordinator`が必要とする情報を渡せるようにする。
2. `DialogCoordinator::buildPositionStringForIndex()`を削除し、`ConsiderationPositionResolver`呼び出しへ置換する。
3. `startConsiderationFromContext()`内の`previousFileTo`/`previousRankTo`/`lastUsiMove`解決処理を、共通化可能な範囲で`ConsiderationPositionResolver`へ移す。
4. `DialogCoordinator`に残る局所ロジックは、UI依存処理とドメイン処理が混ざらないよう整理する。
5. 置換後に不要となる`#include`、ヘルパーメソッド、コメントを削除する。

### 完了条件
- `DialogCoordinator`内の検討局面文字列生成ロジックが単一実装に統一されている。
- 既存機能（検討開始、分岐局面、棋譜読み込み局面）で挙動差分がない。
- ビルドが通る。

### 確認観点
- 通常対局中の検討開始
- 棋譜読込後の任意手数から検討開始
- 分岐ライン選択中の検討開始

## 2. MainWindowの`ensure*`群の用途別分離

### 背景
`MainWindow`には`ensure*`系初期化/配線メソッドが集中しており、責務境界が曖昧になりやすい。

### 対象
- `src/app/mainwindow.cpp`
- `src/app/mainwindow.h`
- 必要に応じた新規クラス（`src/app/`または`src/ui/wiring/`）

### 実施タスク
1. `ensure*`メソッドを「初期化」「依存注入」「シグナル配線」の3分類で棚卸しする。
2. 依存関係が密なまとまり単位で、移譲先クラスを定義する。
3. 第1段として配線系を分離する。
4. `MainWindow`は「呼び出し順制御」と「UIルート管理」に責務を限定する。
5. 段階移行ごとにビルドと動作確認を行い、1コミット1テーマで進める。

### 推奨分割単位（第1段）
- `MatchCoordinator`関連配線
- `DialogCoordinator`関連配線
- `RecordNavigationHandler`関連配線
- `ConsiderationWiring`関連配線

### 完了条件
- `MainWindow`の`ensure*`実装行数が段階的に減少している。
- 新規クラスの責務が名前と一致している。
- 既存シグナル配線（`Qt::UniqueConnection`）が維持されている。
- ビルドが通る。

### 注意点
- 既存の初期化順序は動作前提になっているため、順序変更は最小化する。
- 所有権の変更は同時に広げず、まずは移譲だけを優先する。
- 大規模一括変更を避け、動作確認可能な小分けで進める。
