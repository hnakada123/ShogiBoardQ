# 包括的改善分析レポート（2026-02-28）

## 1. エグゼクティブサマリ

ShogiBoardQ は、大規模リファクタリングを経て品質基盤が整った段階にある。
構造KPIテスト・静的解析・CIパイプラインが有効に機能し、コードスタイルの一貫性も高い。

本レポートは、既存の2つの分析ドキュメントを補完し、
これまで体系的に評価されていなかった改善領域を網羅的に分析する。

- `future-improvement-analysis-2026-02-28-current.md` — コード構造中心の分析（app層複雑度、大型ファイル、所有権、配線テスト、CI可観測性）
- `code-quality-execution-tasks-2026-02-27.md` — 実行タスク分解（27 Issue）

### 改善テーマ一覧

| # | テーマ | 新規/既存 | 評価 |
|---|--------|-----------|------|
| 1 | テストカバレッジ拡充 | 新規 | 全体 ~16%、UI/App層 0% |
| 2 | アーキテクチャパターンの一貫性 | 新規 | 非常に良好（Deps/Hooks/Refs統一済み） |
| 3 | C++17機能活用の拡大 | 新規 | 採用率 ~30%、拡大余地大 |
| 4 | エラーハンドリングの体系化 | 新規 | 多層だが統一性に課題 |
| 5 | スレッド安全性の確認 | 新規 | 安全（シングルスレッド設計） |
| 6 | 前方宣言の最適化 | 新規 | 主要クラスに集中、妥当 |
| 7 | CI/CD強化 | 既存+拡張 | コードカバレッジ未導入 |
| 8 | ドキュメント整備 | 新規 | 開発ガイド充実、APIドキュメント不足 |
| 9 | UI/ロジック分離 | 既存+拡張 | MVP進行中、Widget層に残存課題 |

---

## 2. 現状スナップショット

### 2.1 コード規模

| 指標 | 値 |
|---|---:|
| `src/` 内 `.cpp/.h` ファイル数 | 532 |
| `src/` 総行数 | 104,455 |
| `tests/` テストファイル数 | 57 |
| `tests/` 総行数 | ~20,014 |
| テスト/実装比率 | 19.2% |

### 2.2 モジュール別行数

| モジュール | ファイル数 | 行数 | テスト有無 |
|---|---:|---:|---|
| `kifu` | 81 | 18,192 | 部分的（変換器は充実） |
| `ui` | 94 | 15,668 | 極めて薄い（1/60+） |
| `dialogs` | 41 | 12,467 | 極めて薄い（1/25） |
| `game` | 46 | 9,895 | 部分的（6/25+） |
| `widgets` | 49 | 9,736 | なし（0/29） |
| `app` | 71 | 8,821 | なし（0/50+） |
| `engine` | 24 | 5,870 | 部分的（2/12） |
| `core` | 25 | 4,686 | 充実（9/10, 90%） |
| `analysis` | 20 | 4,671 | 部分的（3/7） |
| `views` | 10 | 4,032 | なし（0/8） |
| `services` | 33 | 3,218 | 部分的（3/13） |
| `network` | 8 | 2,858 | 部分的（2/4） |
| `board` | 8 | 1,492 | 部分的（2/4） |
| `navigation` | 4 | 1,134 | 部分的（1/2） |
| `common` | 13 | 888 | 部分的（1/6） |
| `models` | 5 | 827 | なし（0/5） |

### 2.3 品質ゲート

| 指標 | 値 |
|---|---:|
| テスト数（ctest） | 46 |
| テスト結果 | 46/46 pass |
| `delete` 件数（`src/*.cpp`） | 1（FlowLayout、正当） |
| lambda `connect` 件数 | 0 |
| 旧式 `SIGNAL/SLOT` 件数 | 0 |
| `struct Deps` 件数 | 45 |
| `struct Hooks` 件数 | 8 |
| `struct Refs` 件数 | 9 |
| `updateDeps()` 宣言数（ヘッダー） | 25 |

### 2.4 600行超ファイル（上位15件）

| ファイル | 行数 | モジュール |
|---|---:|---|
| `src/core/fmvlegalcore.cpp` | 930 | core |
| `src/dialogs/pvboarddialog.cpp` | 884 | dialogs |
| `src/dialogs/engineregistrationdialog.cpp` | 841 | dialogs |
| `src/dialogs/josekimovedialog.cpp` | 838 | dialogs |
| `src/core/shogiboard.cpp` | 820 | core |
| `src/views/shogiview_draw.cpp` | 808 | views |
| `src/dialogs/changeenginesettingsdialog.cpp` | 804 | dialogs |
| `src/views/shogiview.cpp` | 802 | views |
| `src/network/csagamecoordinator.cpp` | 798 | network |
| `src/game/matchcoordinator.cpp` | 789 | game |
| `src/engine/usi.cpp` | 781 | engine |
| `src/kifu/formats/csaexporter.cpp` | 758 | kifu |
| `src/kifu/formats/jkfexporter.cpp` | 735 | kifu |
| `src/game/gamestartcoordinator.cpp` | 727 | game |
| `src/ui/coordinators/kifudisplaycoordinator.cpp` | 720 | ui |

---

## 3. テストカバレッジ分析

### 3.1 テストフレームワーク

- **フレームワーク**: Qt Test（QtTest モジュール）
- **テストランナー**: ctest（CMake統合）
- **CI環境**: `QT_QPA_PLATFORM=offscreen`（ヘッドレス実行）
- **シグナルテスト**: `QSignalSpy` によるシグナル検証
- **テストスタブ**: 手書き（11ファイル、~2,800行）。モックフレームワーク未使用

### 3.2 モジュール別テストカバレッジ

#### テスト充実度: 高（カバレッジ 70%以上）

| モジュール | テスト対象 | カバレッジ | 主要テスト |
|---|---|---|---|
| `core` | 9/10 クラス | 90% | tst_shogiboard, tst_shogiclock, tst_fmv* (8件) |

`core` は最も充実したテスト基盤を持つ。`fmvattacks.cpp` のみ直接テストがないが、`enginemovevalidator` テスト経由で間接的にカバーされている。

#### テスト充実度: 中（カバレッジ 25-70%）

| モジュール | テスト対象 | カバレッジ | 主要ギャップ |
|---|---|---|---|
| `kifu` | 8/30+ | 27% | Lexer群、IOサービス、ロードコーディネータ |
| `network` | 2/4 | 50% | CsaGameCoordinator |
| `board` | 2/4 | 50% | BoardInteractionController, BoardImageExporter |
| `navigation` | 1/2 | 50% | RecordNavigationHandler |
| `analysis` | 3/7 | 43% | ConsiderationFlowController, TsumeGenerator |
| `game` | 6/25+ | 24% | MatchCoordinator, Strategy群, MatchTimekeeper |
| `services` | 3/13 | 23% | TimekeepingService, UiNotificationService |
| `engine` | 2/12 | 17% | Usi本体, EngineProcessManager, InfoParser |
| `common` | 1/6 | 17% | ErrorBus, FontSizeHelper, DialogUtils |

#### テスト充実度: 低（カバレッジ 5%未満）

| モジュール | テスト対象 | カバレッジ | 備考 |
|---|---|---|---|
| `dialogs` | 1/25 | 4% | JosekiWindowのみ。24ダイアログ未テスト |
| `ui` | 1/60+ | 2% | UiStatePolicyManagerのみ。Presenter/Controller/Wiring全未テスト |
| `app` | 0/50+ | 0% | MainWindow, 全Registry, 全Service未テスト |
| `views` | 0/8 | 0% | 描画コード全未テスト |
| `widgets` | 0/29 | 0% | カスタムWidget全未テスト |
| `models` | 0/5 | 0% | Qt Model全未テスト |

### 3.3 テストの構造的特徴

**強み:**
- `core` 層のデータ構造テストは網羅的（ShogiBoard, ShogiMove, ShogiClock, 合法手生成）
- 全6形式の棋譜変換器にラウンドトリップテスト完備
- EngineMoveValidator は8テストファイル・約1,200行で重点テスト
- 構造KPIテスト（`tst_structural_kpi`）によるコード品質の自動監視
- 統合テスト（`tst_integration`）による複合フローの検証

**課題:**
- **モックフレームワーク不在**: 全スタブが手書き（メンテナンスコスト高）
- **UI層テスト皆無**: Presenter/Controller/Wiring の動作検証なし
- **配線契約テスト不足**: シグナル転送チェーンの回帰検知が弱い
- **エラーケーステスト不足**: 主にハッピーパスのみ
- **パフォーマンステスト不在**: `bench_movevalidator.cpp` 以外なし

### 3.4 優先テスト拡充候補

| 優先度 | 対象 | 理由 | 推定工数 |
|---|---|---|---|
| P1 | MatchCoordinator | 最重要オーケストレータ、789行、テストなし | 大 |
| P1 | Wiring契約テスト | シグナル転送回帰の検知。変更頻度高 | 中 |
| P1 | エラーケーステスト | 変換器・ファイルI/Oの異常系 | 中 |
| P2 | Strategy群テスト | 3戦型の振る舞い検証 | 中 |
| P2 | EngineProcessManager | プロセスライフサイクルの検証 | 中 |
| P3 | Widget/Dialog | GUI依存が強く自動テスト困難 | 大 |

---

## 4. アーキテクチャパターンの一貫性

### 4.1 Deps/Hooks/Refs パターン

本プロジェクトでは、依存注入に3種の構造体パターンを使い分けている。

| パターン | 使用数 | 用途 | 方向 |
|---|---:|---|---|
| `Deps` | 45 | 入力依存（ポインタ、コールバック） | 外→内 |
| `Hooks` | 8 | 出力コールバック（上位層への通知） | 内→外 |
| `Refs` | 9 | 状態参照（読み取り専用ポインタ） | 内→外（読み取り） |

**評価: 非常に良好**

- 全45クラスの `Deps` が `updateDeps()` メソッドを伴い、パターンが統一されている
- `Hooks` は `game/` 層の8クラスに集中し、MatchCoordinator のネストサブ構造体（UI/Time/Engine/Game）が最も複雑
- `Refs` は状態読み取り専用として明確に分離されている
- Wiring層（13ファイル）は全て `Deps` パターンを使用

**課題なし**: パターンの適用は全体的に一貫している。

### 4.2 Wiring層の一貫性

`src/ui/wiring/` に19クラスが存在し、シグナル/スロット接続の責務を担う。

**評価: 良好**

- 全クラスが `QObject` 継承 + 親所有権パターン
- 公開メソッドは `wire()` または `ensure()` に統一
- ファサードパターン: `UiActionsWiring` が4つのサブWiring（File/Game/Edit/View）を集約

**軽微な不整合:**
- `AnalysisTabWiring` の `buildUiAndWire()` がUI構築と配線を混在（他は分離）
- `RecordNavigationWiring` が `ensure()` + `WiringTargets` パターンで他と異なる（意図的設計）

### 4.3 命名規約の一貫性

| 接尾辞 | 件数 | 意味 | 一貫性 |
|---|---:|---|---|
| Controller | 24 | 入力イベントをドメイン操作に変換 | 一貫 |
| Coordinator | 16 | 複数サブシステムのオーケストレーション | 一貫 |
| Service | 20 | ステートレスまたは最小状態のユーティリティ | 一貫 |
| Manager | 8 | リソース/ライフサイクル管理 | ほぼ一貫 |
| Handler | 9 | 特定イベントの処理 | 一貫 |
| Presenter | 10 | データの表示形式変換（MVP） | 一貫 |
| Wiring | 19 | シグナル/スロット接続 | 一貫 |

**軽微な命名不整合（2件）:**
1. `UiStatePolicyManager` — 実態は状態テーブル駆動のCoordinator。`UiStatePolicyCoordinator` がより適切
2. `DockLayoutManager` — ドックウィジェットのレイアウト制御。Manager名は許容範囲

### 4.4 シグナル/スロットパターン

**評価: 完全準拠**

- lambda `connect` 使用: **0件**（CLAUDE.md ルール完全遵守）
- 旧式 `SIGNAL()`/`SLOT()` マクロ: **0件**（排除完了）
- `QOverload<>` による型安全なオーバーロード解決: 14箇所で正しく使用

### 4.5 所有権パターン

**評価: 非常に良好**

- `delete` 使用: **1件**のみ（`FlowLayout` の `QLayoutItem` 削除、正当かつコメント付き）
- QObject 親所有権: 100% 準拠
- 非QObject: `std::unique_ptr` で統一
- 裸の `new` は全て `QWidget` の親付き生成

---

## 5. C++17機能活用の分析

### 5.1 現在の採用状況

| 機能 | 使用数 | 採用率 | 代表ファイル |
|---|---:|---|---|
| `std::optional` | 14 | 低〜中 | `shogiboard.h`, `parsecommon.h`, `usiprotocolhandler.h` |
| 構造化束縛 `auto [a,b]` | 4 | 低 | `matchcoordinator.cpp`, `enginelifecyclemanager.cpp` |
| `[[nodiscard]]` | 1 | 極低 | `usi.cpp` |
| `[[maybe_unused]]` | 1 | 極低 | `main.cpp` |
| `QStringView` | 20 | 低〜中 | 各所（Qt6のstring_view相当） |
| `std::make_unique` | 多数 | 高 | 全モジュール |
| `std::array` | 少数 | 低 | `parsecommon.cpp` |
| `std::variant` | 0 | 未使用 | — |
| `if constexpr` | 0 | 未使用 | — |
| `std::string_view` | 0 | 未使用 | Qt の `QStringView` で代替 |

**採用率: 約30%**（C++17ポテンシャルに対して）

### 5.2 拡大推奨領域

#### 5.2.1 `[[nodiscard]]` の追加（推奨度: 高、コスト: 低）

現在1件のみ。以下の関数カテゴリに追加を推奨:

- **ファイルI/O関数**: `writeKifuFile()`, `saveKifuToFile()` 等（戻り値 `bool` の無視防止）
- **バリデーション関数**: `MoveValidator` の全検証関数
- **パース関数**: `parseSfen()`, `parseMoveLabel()` 等
- **変換関数**: 全形式コンバータの `parse()` / `convert()` 系

推定対象: 20〜30関数

```cpp
// Before
bool writeKifuFile(const QString& filePath, ...);

// After
[[nodiscard]] bool writeKifuFile(const QString& filePath, ...);
```

#### 5.2.2 `std::optional` の拡大（推奨度: 中、コスト: 中）

現在の `bool` + out-parameter パターンを `std::optional` に置換可能な箇所:

**候補1: ShogiUtils**
```cpp
// Before (shogiutils.h)
bool parseMoveLabel(const QString& moveLabel, int* outFile, int* outRank);

// After
std::optional<std::pair<int, int>> parseMoveLabel(const QString& moveLabel);
```

**候補2: 変換器群**（6ファイル、計~15箇所）
```cpp
// Before (kiftosfenconverter.h)
static bool parseWithVariations(const QString& kifPath,
                                KifParseResult& out,
                                QString* errorMessage = nullptr);

// After
static std::optional<KifParseResult> parseWithVariations(
    const QString& kifPath, QString* errorMessage = nullptr);
```

**候補3: KifuIOService**
```cpp
// Before
bool writeKifuFile(..., QString* errorText, ...);

// After — Result型パターン
struct WriteResult { bool success; QString errorText; };
[[nodiscard]] WriteResult writeKifuFile(...);
```

#### 5.2.3 `std::variant` の導入（推奨度: 低、コスト: 中）

現在は enum + 条件付きフィールドで状態を表現:

```cpp
// 現行 (usiprotocolhandler.h)
struct TsumeResult {
    enum Kind { Solved, NoMate, NotImplemented, Unknown } kind = Unknown;
    QStringList pvMoves;  // kind == Solved の場合のみ有効
};
```

`std::variant` で型安全にできるが、既存コードとの互換性コストが高い。
短期的には `[[nodiscard]]` と `std::optional` の拡大を優先すべき。

#### 5.2.4 `QStringView` の拡大（推奨度: 中、コスト: 低）

文字列を変更しないパース関数の引数に `QStringView` を適用可能:

- `parsecommon.cpp` の文字解析関数群
- `shogiboard.cpp` の SFEN パース
- `usiprotocolhandler.cpp` のコマンドパース

Qt6 では `QStringView` が推奨されており、コピーコスト削減とAPI明確化に寄与。

---

## 6. エラーハンドリングパターン

### 6.1 現行パターンの分類

本プロジェクトでは4つのエラー報告パターンが混在している。

| パターン | 使用箇所 | 用途 |
|---|---|---|
| `bool` 戻り値 + `QString*` out-param | ファイルI/O、変換器 | 呼び出し元へのエラー報告 |
| `ErrorBus` シグナル | 16箇所（7ファイル発信） | グローバルなエラー通知 |
| Qt シグナル (`errorOccurred`) | エンジン、プロセス管理 | 非同期エラー通知 |
| `std::optional` 戻り値 | パース関数 | 入力検証のエラー表現 |

**C++例外は不使用**（`try/catch` 3件、全て外部ライブラリ起因の防御的キャッチ）。

### 6.2 パターン別評価

#### ErrorBus（`src/common/errorbus.h/.cpp`）

```cpp
class ErrorBus final : public QObject {
    Q_OBJECT
public:
    static ErrorBus& instance();
    void postError(const QString& message) { emit errorOccurred(message); }
signals:
    void errorOccurred(const QString& message);
};
```

**評価:**
- シンプルで効果的なグローバル通知メカニズム
- **課題**: エラーの深刻度（severity）区分がない。全エラーが同一扱い
- **課題**: `emit errorOccurred(message)` の接続先が保証されない（サイレント失敗の可能性）
- **改善案**: `ErrorLevel` enum（Info/Warning/Error/Critical）の追加

#### bool + out-param パターン（`src/kifu/kifuioservice.cpp`）

**評価:**
- ファイルI/Oのエラーハンドリングは包括的（パス検証→ディレクトリ作成→オープン→書込→クローズの5段階チェック）
- **課題**: `errorText` が `nullptr` の場合、エラー原因が報告されない
- **課題**: `[[nodiscard]]` がないため、戻り値の無視が検知されない
- **改善案**: `[[nodiscard]]` 追加 + Result型への段階移行

#### サイレント失敗の潜在箇所

以下のパターンでエラーが黙殺される可能性がある:

1. **シグナル未接続時**: `emit errorOccurred(message)` のスロットが未接続
2. **nullable out-param**: `QString* errorMessage = nullptr` でエラー詳細が失われる
3. **戻り値未チェック**: `[[nodiscard]]` がないため、コンパイラが警告しない

### 6.3 改善提案

| 施策 | 効果 | コスト | 優先度 |
|---|---|---|---|
| `[[nodiscard]]` 一括追加 | 戻り値無視の防止 | 低（機械的） | P1 |
| ErrorBus に severity 追加 | エラー分類の明確化 | 低 | P2 |
| Result型の段階導入 | エラー情報の構造化 | 中 | P3 |
| エラーパス回帰テスト | 異常系の検証強化 | 中 | P2 |

---

## 7. スレッド安全性

### 7.1 アーキテクチャ概要

ShogiBoardQ は**シングルスレッド設計**を採用している。

| 要素 | 使用状況 |
|---|---|
| `QThread` / `moveToThread()` | 未使用（診断ログのみ） |
| `QMutex` / `QReadWriteLock` | 未使用 |
| `QSemaphore` | 未使用 |
| `Qt::QueuedConnection` | 未使用 |
| アトミック操作 | 未使用 |

### 7.2 プロセス分離によるエンジン通信

USI エンジンは別プロセスとして起動され、テキストベースの IPC で通信する。

```
┌──────────────┐     ┌──────────────┐
│ ShogiBoardQ  │     │ USI Engine   │
│ (GUI thread) │────→│ (Separate    │
│              │←────│  Process)    │
│  QProcess    │     │              │
└──────────────┘     └──────────────┘
     stdin/stdout (line-based text)
```

- `QProcess` がバッファリングとスレッド安全性を内部的に保証
- 共有メモリなし。IPC はラインベースのテキスト（USIプロトコル）
- `readyReadStandardOutput` シグナルはGUIスレッドで配信

### 7.3 潜在的リスク

#### ポーリングパターン（`usiprotocolhandler.cpp`）

```cpp
QThread::msleep(1);  // ビジーウェイトループ内
```

**評価**: GUIスレッドをブロックするが、エンジン初期化時の意図的な同期待ち。
短時間（数十ms〜数秒）で完了するため、実用上の問題は低い。

#### QEventLoop による同期待ち

```cpp
bool waitForResponseFlag(bool& flag, void(UsiProtocolHandler::*signal)(), int timeoutMs);
```

**評価**: ネストされた `QEventLoop` によるブロッキング待ち。
タイムアウト付きで安全だが、デッドロックの理論的リスクは残る。

### 7.4 総合評価

**安全**: シングルスレッド + プロセス分離により、データ競合のリスクは実質ゼロ。
同期プリミティブが不要な設計は、複雑性の回避として妥当。

将来的にマルチスレッド化（バックグラウンド解析、並列棋譜パース等）を行う場合は、
`QThread` + `QueuedConnection` の導入が必要になる。

---

## 8. 前方宣言の分析

### 8.1 前方宣言頻度（上位15クラス）

| クラス | 前方宣言回数 | 役割 |
|---|---:|---|
| `ShogiGameController` | 27 | ゲーム状態管理の中核 |
| `ShogiView` | 26 | 盤面描画の中核 |
| `QWidget` | 24 | Qt基底クラス |
| `KifuRecordListModel` | 23 | 棋譜データモデル |
| `KifuBranchTree` | 21 | 分岐木データ構造 |
| `KifuNavigationState` | 16 | ナビゲーション状態 |
| `UsiCommLogModel` | 14 | エンジン通信ログ |
| `ShogiEngineThinkingModel` | 14 | エンジン思考情報 |
| `MatchCoordinator` | 14 | 対局オーケストレータ |
| `QLabel` | 13 | Qt UIコンポーネント |
| `TimeControlController` | 12 | 時間制御 |
| `KifuLoadCoordinator` | 12 | 棋譜ロード |
| `ShogiClock` | 11 | 対局時計 |
| `QPushButton` | 11 | Qt UIコンポーネント |
| `Usi` | 10 | エンジンインターフェース |

### 8.2 評価

**妥当性: 高**

- 前方宣言の頻度はクラスのアーキテクチャ上の重要度と相関している
- `ShogiGameController`（27回）と `ShogiView`（26回）が最多だが、これらは全モジュールから参照される中核コンポーネントであり、正当
- 循環依存は検出されていない
- 前方宣言により include サイクルが適切に回避されている

### 8.3 改善の余地

**共通前方宣言ヘッダーの検討:**

頻出クラス（10回以上）を集約した `src/common/fwd.h` の導入:

```cpp
// src/common/fwd.h
#pragma once
class ShogiGameController;
class ShogiView;
class KifuRecordListModel;
class KifuBranchTree;
class KifuNavigationState;
// ...
```

**メリット**: 個別ファイルでの前方宣言の重複を削減、変更時の一括更新が容易
**デメリット**: 不要な前方宣言を取り込むリスク、include依存の不透明化

**判断**: 現状の個別前方宣言は十分機能しており、共通ヘッダー導入の優先度は低い。
クラス名変更が頻繁に発生する場合にのみ検討すべき。

---

## 9. CI/CD強化

### 9.1 現行CI構成

| ジョブ | プラットフォーム | トリガー | 内容 |
|---|---|---|---|
| `build-and-test` | ubuntu-latest | push/PR | Release + テスト + KPIサマリ |
| `build-debug` | ubuntu-latest | push/PR | Debug + テスト |
| `static-analysis` | ubuntu-latest | push/PR | clang-tidy |
| `sanitize` | ubuntu-latest | 手動/ラベル | ASan + UBSan |
| `translation-check` | ubuntu-latest | push/PR | 未翻訳数閾値チェック |

### 9.2 未導入の改善項目

#### 9.2.1 コードカバレッジ（推奨度: 高）

現状、テストカバレッジの数値的な可視化がない。

**導入案:**
```yaml
- name: Configure with Coverage
  run: cmake -B build -S . -DBUILD_TESTING=ON
       -DCMAKE_BUILD_TYPE=Debug
       -DCMAKE_CXX_FLAGS="--coverage -fprofile-arcs -ftest-coverage"

- name: Generate Coverage Report
  run: |
    cd build
    ctest --output-on-failure
    gcovr --root ../src --xml coverage.xml --html coverage.html

- name: Upload Coverage
  uses: codecov/codecov-action@v4
  with:
    files: build/coverage.xml
```

**期待効果**: テストギャップの数値的把握、PR ごとのカバレッジ変化追跡

#### 9.2.2 クロスプラットフォームビルド（推奨度: 中）

現状は Ubuntu のみ。Windows/macOS ビルドの追加:

```yaml
strategy:
  matrix:
    os: [ubuntu-latest, windows-latest, macos-latest]
```

**障壁**: Qt6 のプラットフォーム別セットアップコスト、CI 時間の増加
**判断**: macOS は `docs/dev/macos-build-and-release.md` が存在するため対応可能性あり

#### 9.2.3 KPI 前回比較（推奨度: 中）

PR ごとに前回の KPI 値との差分を表示:

```yaml
- name: Compare KPI
  run: |
    # Download previous KPI artifact
    gh api repos/$REPO/actions/artifacts --jq '...' > prev-kpi.json
    python3 scripts/compare-kpi.py prev-kpi.json kpi-results.json >> $GITHUB_STEP_SUMMARY
```

**期待効果**: 品質劣化の即時検知、改善トレンドの可視化

#### 9.2.4 Sanitizer の定期実行化（推奨度: 中）

現状は手動トリガーまたはラベル付き PR のみ。Nightly ジョブ化:

```yaml
on:
  schedule:
    - cron: '0 3 * * 1'  # 毎週月曜 03:00 UTC
```

#### 9.2.5 ベンチマーク CI（推奨度: 低）

`bench_movevalidator.cpp` が存在するが、CI 統合されていない。
パフォーマンス回帰の自動検知には `github-action-benchmark` 等の導入が必要。

### 9.3 CI改善ロードマップ

| フェーズ | 施策 | 工数 |
|---|---|---|
| 短期 | コードカバレッジ導入 | 小（CI yaml + gcovr） |
| 短期 | KPI前回比較 | 小（スクリプト追加） |
| 中期 | Sanitizer定期実行 | 小（cron追加） |
| 中期 | macOSビルド追加 | 中（Qt6セットアップ） |
| 長期 | Windowsビルド追加 | 中〜大（MSVC対応） |
| 長期 | ベンチマークCI | 中（ベースライン設定） |

---

## 10. ドキュメント整備

### 10.1 既存ドキュメント一覧

`docs/dev/` に29ファイルが存在し、開発ガイドは充実している。

| カテゴリ | ファイル数 | 主要ファイル |
|---|---:|---|
| ビルド・リリース | 3 | `linux-build-and-release.md`, `macos-*`, `windows-*` |
| 設計・アーキテクチャ | 6 | `game-start-flow.md`, `ui-state-policy.md`, `matchcoordinator-split-design.md` |
| コードスタイル | 4 | `ownership-guidelines.md`, `commenting-style-guide.md`, `debug-logging-guidelines.md` |
| 機能仕様 | 7 | `kifu-io-features.md`, `tsumeshogi-generator.md`, `sennichite-detection.md` |
| テスト | 2 | `test-summary.md`, `manual-test-scenarios.md` |
| 改善計画 | 3 | `future-improvement-analysis-*.md`, `code-quality-execution-tasks-*.md` |
| その他 | 4 | `developer-guide.md`, `refactoring-inspection-checklist.md` |

### 10.2 ドキュメントギャップ

#### 不足領域1: APIドキュメント（優先度: 中）

ヘッダーファイルのコメントは最小限。公開APIの使用方法・前提条件・副作用が暗黙的。

**改善案:**
- 主要公開クラス（`ShogiBoard`, `ShogiGameController`, `MatchCoordinator`）にDoxygenコメントを段階追加
- ただし CLAUDE.md の「Don't add docstrings, comments, or type annotations to code you didn't change」に注意

#### 不足領域2: アーキテクチャ概観図（優先度: 中）

モジュール間の依存関係を図示するドキュメントがない。

**改善案:**
- `docs/dev/architecture-overview.md` に依存関係図（Mermaid形式）を作成
- レイヤー構成（core → game → app → ui）の明示

#### 不足領域3: 新規開発者向けオンボーディング（優先度: 低）

`CLAUDE.md` と `developer-guide.md` は存在するが、
「初めてコードに触れる開発者が最初に読むべき順序」のガイドがない。

#### 不足領域4: Deps/Hooks/Refs パターンの設計意図ドキュメント（優先度: 低）

45クラスで使用されるパターンだが、設計意図・使い分けルールの公式ドキュメントがない。
現状はコードの一貫性から暗黙的に理解可能だが、明文化すると新規参画時のコストが下がる。

---

## 11. UI/ロジック分離

### 11.1 現状評価

MVP（Model-View-Presenter）パターンの導入が進んでおり、分離状況は概ね良好。

| 層 | ファイル数 | 責務 | 分離度 |
|---|---:|---|---|
| Presenters | 10 | データ→表示形式変換 | 良好 |
| Controllers | 24 | UI入力→ドメイン操作 | 良好 |
| Wiring | 19 | シグナル/スロット接続 | 良好 |
| Coordinators | 16 | ワークフローオーケストレーション | 良好 |
| Widgets | 49 | UIコンポーネント | 課題あり |
| Dialogs | 41 | ダイアログUI | 課題あり |

### 11.2 残存課題

#### Widget層のロジック混在

大型Widget（600行超）にはUI構築・イベント処理・ビジネスロジックが混在している。

**主要な問題ファイル:**

| ファイル | 行数 | 課題 |
|---|---:|---|
| `src/widgets/recordpane.cpp` | 677 | 棋譜表示 + 選択ロジック + モデル管理 |
| `src/widgets/engineanalysistab.cpp` | (Widget群) | 解析タブの構築 + 更新 + イベント処理 |
| `src/dialogs/pvboarddialog.cpp` | 884 | PV盤面表示 + エンジン連携 + UI構築 |
| `src/dialogs/engineregistrationdialog.cpp` | 841 | エンジン登録 + プロセス検証 + 設定管理 |

**改善方針:**
- Widget の内部ロジックを Presenter/Controller に抽出
- ダイアログの検証ロジックをサービスクラスに移動
- 既存の `code-quality-execution-tasks` の ISSUE-021, ISSUE-022 と整合

#### View層の描画コード

`src/views/shogiview_draw.cpp`（808行）と `src/views/shogiview.cpp`（802行）は
描画ロジックが集中しているが、これは Qt の `QGraphicsView` フレームワークの性質上、
ある程度の集中は不可避。分割は `shogiview_labels.cpp`（704行）のように
ファイル分割で対応済み。

---

## 12. 優先度マトリクス

### 12.1 全改善テーマの効果/コスト/リスク評価

| # | テーマ | 効果 | コスト | リスク | 優先度 |
|---|--------|------|--------|--------|--------|
| 1 | `[[nodiscard]]` 一括追加 | 中 | 低 | 低 | **P1** |
| 2 | コードカバレッジCI導入 | 高 | 低 | 低 | **P1** |
| 3 | テストカバレッジ拡充（core以外） | 高 | 高 | 中 | **P1** |
| 4 | KPI前回比較CI導入 | 中 | 低 | 低 | **P1** |
| 5 | `std::optional` 拡大 | 中 | 中 | 低 | **P2** |
| 6 | ErrorBus severity追加 | 中 | 低 | 低 | **P2** |
| 7 | エラーケーステスト拡充 | 高 | 中 | 低 | **P2** |
| 8 | Sanitizer定期実行化 | 中 | 低 | 低 | **P2** |
| 9 | アーキテクチャ概観図作成 | 中 | 低 | なし | **P2** |
| 10 | Widget層ロジック抽出 | 高 | 高 | 中 | **P2** |
| 11 | `QStringView` 拡大 | 低 | 中 | 低 | **P3** |
| 12 | クロスプラットフォームCI | 中 | 中〜大 | 低 | **P3** |
| 13 | Deps/Hooks設計意図ドキュメント | 低 | 低 | なし | **P3** |
| 14 | 共通前方宣言ヘッダー | 低 | 低 | 低 | **P4** |
| 15 | `std::variant` 導入 | 低 | 中 | 中 | **P4** |

### 12.2 クイックウィン（低コスト・即効果）

以下は1-2日で実施可能な改善:

1. **`[[nodiscard]]` 追加**: 機械的に20〜30関数に追加。コンパイラ警告で戻り値無視を防止
2. **コードカバレッジCI**: `ci.yml` に gcovr + Codecov の追加（約30行の変更）
3. **KPI前回比較**: 既存の artifact ダウンロード + diff スクリプト追加
4. **Sanitizer cron化**: `schedule` トリガーの追加（3行の変更）

---

## 13. 既存分析との統合ロードマップ

### 13.1 既存ドキュメントとの関係

```
code-quality-execution-tasks-2026-02-27.md (27 Issue)
  ├── ISSUE-001〜003: ガードレール ← 本レポート §9 CI/CD強化 と統合
  ├── ISSUE-010〜013: 司令塔分割 ← future-improvement §4.1 テーマA と同一
  ├── ISSUE-020〜023: 大型ファイル ← future-improvement §4.2 テーマB と同一
  ├── ISSUE-030〜034: 変換器共通化 ← 本レポート §3 テストカバレッジの kifu ギャップ
  ├── ISSUE-040〜042: delete削減 ← future-improvement §4.3 テーマC（完了済み）
  ├── ISSUE-060〜065: ダイアログ品質 ← 本レポート §11 Widget層課題
  └── ISSUE-050〜051: KPI更新 ← 本レポート §9 CI/CD強化

future-improvement-analysis-2026-02-28-current.md (6テーマ)
  ├── テーマA: app司令塔複雑度 ← 本レポート §4 で一貫性は高いと確認
  ├── テーマB: 大型ファイル ← 本レポート §2.4 で最新データ更新
  ├── テーマC: 所有権明示化 ← 本レポート §4.5 で良好と確認（delete 1件）
  ├── テーマD: 配線契約テスト ← 本レポート §3 テストカバレッジ分析と統合
  ├── テーマE: CI可観測性 ← 本レポート §9 でコードカバレッジ等を追加提案
  └── テーマF: ドメイン境界 ← 本レポート §5 C++17活用で補完

本レポート（新規テーマ）
  ├── §3: テストカバレッジ詳細分析（モジュール別マッピング）
  ├── §4: アーキテクチャパターン一貫性（Deps/Hooks/Refs/命名規約）
  ├── §5: C++17機能活用の拡大提案
  ├── §6: エラーハンドリングの体系化
  ├── §7: スレッド安全性の確認（安全）
  └── §8: 前方宣言の最適化分析
```

### 13.2 統合ロードマップ（16週間）

#### Phase 1: ガードレール強化（Week 1-2）

| 施策 | 出典 | 工数 |
|---|---|---|
| `[[nodiscard]]` 一括追加 | 本レポート §5.2.1 | 小 |
| コードカバレッジCI導入 | 本レポート §9.2.1 | 小 |
| KPI前回比較CI | 本レポート §9.2.3 | 小 |
| Sanitizer cron化 | 本レポート §9.2.4 | 小 |
| 構造KPI拡張 | ISSUE-001, ISSUE-002 | 小 |

**完了条件:**
- `[[nodiscard]]` が主要関数に付与
- CI にカバレッジレポートが表示
- KPI の PR 差分が Step Summary に出力

#### Phase 2: テスト基盤拡充（Week 3-6）

| 施策 | 出典 | 工数 |
|---|---|---|
| 配線契約テスト追加（2件） | テーマD, 本レポート §3.4 | 中 |
| エラーケーステスト追加 | ISSUE-034, 本レポート §6 | 中 |
| MatchCoordinator テスト設計 | 本レポート §3.4 P1 | 中 |
| ErrorBus severity追加 | 本レポート §6.3 | 小 |

**完了条件:**
- テスト数 46 → 50 以上
- 異常系テストケース追加
- ErrorBus にレベル区分導入

#### Phase 3: 司令塔・大型ファイル削減（Week 7-10）

| 施策 | 出典 | 工数 |
|---|---|---|
| app サブレジストリ導入 | テーマA | 中 |
| `csagamecoordinator` 分割 | テーマB, ISSUE-020 | 中 |
| `matchcoordinator` 責務抽出 | テーマB, ISSUE-011 | 中 |
| `std::optional` 拡大（変換器群） | 本レポート §5.2.2 | 中 |

**完了条件:**
- `sr_ensure_methods` 35 → 33 以下
- 600行超ファイル 32 → 28 以下

#### Phase 4: 品質仕上げ（Week 11-16）

| 施策 | 出典 | 工数 |
|---|---|---|
| Widget層ロジック抽出 | 本レポート §11, ISSUE-021/022 | 大 |
| 変換器共通化 | ISSUE-030〜033 | 大 |
| ダイアログ重複共通化 | ISSUE-060〜063 | 中 |
| アーキテクチャ概観図作成 | 本レポート §10.2 | 小 |
| クロスプラットフォームCI検討 | 本レポート §9.2.2 | 中 |

**完了条件:**
- 600行超ファイル 28 → 22 以下
- 800行超ファイル 8 → 5 以下
- テスト数 50 → 56 以上
- ダイアログ重複パターン共通化完了

---

## 14. 総括

### 強み（維持すべき点）

1. **アーキテクチャパターンの一貫性**: Deps/Hooks/Refs が45/8/9クラスで統一的に適用
2. **コードスタイルの遵守**: lambda connect 0件、旧式 SIGNAL/SLOT 0件、delete 1件
3. **命名規約の整合性**: Controller/Coordinator/Service/Handler/Presenter/Wiring が意味通りに使い分け
4. **構造KPIによる品質ゲート**: CI で自動監視、閾値管理
5. **スレッド安全性**: シングルスレッド + プロセス分離の堅牢な設計

### 改善すべき点（優先順）

1. **テストカバレッジ**: 全体 ~16%。UI/App/Widget層が 0%。カバレッジ可視化も未導入
2. **C++17活用**: `[[nodiscard]]` が1件のみ。型安全性向上の機会を逃している
3. **エラーハンドリング**: 4パターンが混在。severity 区分なし。サイレント失敗の可能性
4. **CI/CD**: コードカバレッジ未導入。クロスプラットフォーム未対応
5. **大型ファイル**: 600行超が32件残存（既存分析で指摘済み、継続対応中）

### 最優先アクション（今週着手可能）

1. `[[nodiscard]]` を主要関数に一括追加（半日）
2. CI にコードカバレッジレポートを追加（半日）
3. KPI 前回比較を CI Step Summary に出力（半日）
4. Sanitizer ジョブを weekly cron 化（30分）
