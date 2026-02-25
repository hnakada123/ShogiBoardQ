# ShogiBoardQ コードレビュー：改善提案

## 概要

約19万行・333ファイルの規模で、アーキテクチャの層分離、コーディング規律、リファクタリングの継続的な実施の点で高い水準にある。以下は、さらなる品質向上のための改善提案をまとめたものである。

---

## 1. MainWindow の肥大化（優先度：中）

### 現状

| 指標 | 値 |
|------|-----|
| mainwindow.cpp | 4,558行 |
| mainwindow.h | 820行 |
| メンバ変数 | 約111個 |
| ensure*() メソッド | 37個 |
| connect() 呼び出し | 71箇所 |
| 前方宣言 | 56個 |
| 50行超のメソッド | 14個 |

リファクタリングで約22%削減されたが、依然として巨大である。

### 具体的な改善案

#### a. 長大メソッドの分解

特に以下のメソッドは分解の候補：

| メソッド | 行数 | 提案 |
|---------|------|------|
| `resetToInitialState()` | 164行 | フェーズ別に3〜4メソッドに分解（UI状態リセット、モデルリセット、エンジンリセット等） |
| `ensureMatchCoordinatorWiring()` | 117行 | シグナル接続をカテゴリ別ヘルパーに分離 |
| `initializeBranchNavigationClasses()` | 101行 | ブランチナビゲーション専用クラスへ抽出 |
| `setupEngineAnalysisTab()` | 83行 | AnalysisTab の初期化を専用クラスへ |

#### b. サブシステムクラスへの集約

111個のメンバ変数は、関連するものをサブシステムとしてグループ化できる：

```
// 例：解析関連をまとめる
struct AnalysisSubsystem {
    AnalysisPresenter* presenter;
    AnalysisWiring* wiring;
    AnalysisTabWiring* tabWiring;
    AnalysisResultsPresenter* resultsPresenter;
    // ... 関連する ensure*() もここに移動
};
```

候補となるグループ：
- **解析系**（`m_analysisPresenter`, `m_analysisWiring`, `m_analysisTab` 等）
- **棋譜系**（`m_kifuRecordModel`, `m_kifuBranchModel`, `m_kifuLoadCoordinator` 等）
- **対局系**（`m_match`, `m_gameStart`, `m_gameStartCoordinator` 等）
- **Dock系**（12個のドックウィジェット変数）

---

## 2. 型安全性の強化（優先度：高）

### 現状の問題点

#### a. 駒の表現が QChar

```cpp
// 現状：実行時まで不正な駒文字を検出できない
QChar movingPiece = QLatin1Char(' ');
QChar capturedPiece = QLatin1Char(' ');
QVector<QChar> m_boardData;  // 盤面データも QChar
```

#### 改善案

```cpp
enum class Piece : char {
    None = ' ',
    Pawn = 'P', Lance = 'L', Knight = 'N', Silver = 'S',
    Gold = 'G', Bishop = 'B', Rook = 'R', King = 'K',
    PromotedPawn = 'T', PromotedLance = 'U', PromotedKnight = 'M',
    PromotedSilver = 'A', Horse = 'H', Dragon = 'D',
    // 小文字は後手（変換メソッドで対応）
};
```

コンパイル時に不正な駒を防止でき、switch 文の網羅性チェックも効く。

#### b. 手番がマジック文字列

```cpp
// 現状：typo しても検出できない
QString m_currentPlayer;  // "b" or "w"
if (playerTurnStr == "b" || playerTurnStr == "w") { ... }
```

#### 改善案

```cpp
enum class Turn { Black, White };
```

#### c. 特殊手のマジック文字列

```cpp
// 現状
if (m_bestMove.compare(QStringLiteral("resign"), Qt::CaseInsensitive) == 0) { ... }
```

#### 改善案

```cpp
enum class SpecialMove { None, Resign, Win, Draw };
```

---

## 3. エラーハンドリングの改善（優先度：中）

### 現状の問題点

- `try`/`catch` が一切使われていない
- `std::optional` / `std::variant` が未使用
- パース結果は空文字列や `bool` 返却で表現
- `Q_ASSERT` はリリースビルドで無効化される

### 具体的な改善案

#### a. std::optional の導入

```cpp
// 現状：出力パラメータで結果を返す
bool validateSfenString(const QString& sfen, QString& board, QString& stand);

// 改善案：意図が明確になる
struct SfenComponents { QString board; QString stand; QString turn; int moveNumber; };
std::optional<SfenComponents> parseSfen(const QString& sfen);
```

#### b. 文字列→整数変換の安全化

```cpp
// 現状：バリデーションなしの危険なパターン（usitosfenconverter.cpp）
int fromFile = usi.at(0).toLatin1() - '0';  // 数字でなければ不正値

// 改善案
std::optional<int> parseDigit(QChar ch) {
    if (ch >= '1' && ch <= '9') return ch.toLatin1() - '0';
    return std::nullopt;
}
```

#### c. Q_ASSERT の代替

リリースビルドでも有効なチェックが必要な箇所では：

```cpp
// 現状：リリースで無効
Q_ASSERT(m_view);

// 改善案：リリースでも有効な null チェック
if (Q_UNLIKELY(!m_view)) {
    ErrorBus::instance().postError(tr("Internal error: view is null"));
    return;
}
```

---

## 4. テストカバレッジの拡充（優先度：高）

### 現状

- テストコード約4,300行 vs 本体約192,000行
- 191テスト関数 / 20テストファイル
- コアロジックは比較的よくカバーされている

### カバレッジ状況

#### テスト済み

| モジュール | テスト数 | 評価 |
|-----------|---------|------|
| ShogiBoard（SFEN I/O、駒操作） | 15 | 充実 |
| FastMoveValidator（合法手生成） | 7 | 良好 |
| ShogiClock（時間制御） | 9 | 良好 |
| 棋譜フォーマット変換（KIF/KI2/JKF/USEN） | 21 | 良好 |
| KifuBranchTree / Navigation | 26 | 充実 |
| GameRecordModel | 11 | 良好 |
| JosekiWindow | 30 | 非常に充実 |
| 対局開始フロー / クリーンアップ | 30 | 充実 |

#### 未テスト（改善すべき領域）

| モジュール | テスト数 | リスク |
|-----------|---------|------|
| **USI プロトコル（エンジン通信）** | 0 | 高：エンジン連携の中核 |
| **CSA ネットワーク対局** | 0 | 高：通信対局の中核 |
| **解析モード（AnalysisFlowController 等）** | 0 | 高：解析機能全般 |
| **CSA/USI フォーマット変換** | 各2 | 中：最低限のみ |
| **SettingsService** | 0（間接のみ） | 中：設定永続化 |
| **ShogiView（描画）** | 0 | 低：UI 描画 |
| **各種ダイアログ** | 0（JosekiWindow除く） | 低：UI |

### 推奨するテスト追加順

1. **USI プロトコルハンドラ**：モックエンジンプロセスを使い、プロトコル解析（`info`行、`bestmove`行）をテスト
2. **CSA クライアント**：モックサーバーレスポンスでプロトコル処理をテスト
3. **AnalysisFlowController**：解析モードの状態遷移をテスト
4. **CSA/USI フォーマット変換**：エッジケース（不正入力、空文字列）のテスト拡充
5. **SettingsService**：設定値の保存・復元の単体テスト

---

## 5. シグナル/スロットの複雑性（優先度：低）

### 現状

| 指標 | 値 |
|------|-----|
| connect() 総数 | 642箇所 / 66ファイル |
| emit 総数 | 343箇所 |
| MainWindow の connect() | 71箇所 |
| Wiring 層合計 | 3,383行 |

### 具体的な改善案

#### a. UiActionsWiring の分割

40行に44個の `connect()` が集中しており、非常に密度が高い。機能ドメイン別に分割することで可読性が向上する：

- `FileActionsWiring`（ファイル操作）
- `EditActionsWiring`（編集操作）
- `GameActionsWiring`（対局操作）
- `AnalysisActionsWiring`（解析操作）

#### b. シグナル接続の棚卸し

以下のような未使用の可能性があるシグナルが存在する：

- `AnalysisCoordinator::analysisStarted` — emit されるが接続先なし（コメントで言及あり）

定期的に、emit されているが connect されていないシグナル（およびその逆）を監査すると、デッドコードの蓄積を防げる。

#### c. シグナルチェーンの深さ制限

一部で4〜5段のシグナルチェーンが確認された：

```
AnalysisFlowController::analysisProgress
  → MainWindow::onKifuAnalysisProgress
    → AnalysisResultsPresenter（リスト更新）
      → 行選択トリガー
        → AnalysisCoordinator::updateAnalysisPosition
```

深すぎるチェーンはデバッグを困難にする。3段以内を目安に、直接的な接続を検討するとよい。

---

## 6. リソース管理（優先度：中）

### 現状の問題点

Qt の親子関係による自動管理が大半だが、明示的な `delete` が散在している：

```cpp
// pvboarddialog.cpp
delete m_fromHighlight;
delete m_toHighlight;

// engineregistrationdialog.cpp
delete m_process;

// gamestartcoordinator.cpp
delete m_match;
```

### 改善案

Qt 親子関係に属さないオブジェクトには `std::unique_ptr` を使用する：

```cpp
// 現状
QProcess* m_process = nullptr;
// デストラクタで delete m_process;

// 改善案
std::unique_ptr<QProcess> m_process;
// デストラクタ不要（自動解放）
```

コード変更時のパス漏れによるメモリリークを防止できる。

---

## 7. その他の改善候補

### a. Q_LOGGING_CATEGORY の共有化

現状、各 `.cpp` ファイルが個別にカテゴリを宣言している：

```cpp
// 各ファイルに散在
Q_LOGGING_CATEGORY(lcCore, "shogiboardq.core")
Q_LOGGING_CATEGORY(lcApp, "shogiboardq.app")
```

モジュール共通のヘッダーで `Q_DECLARE_LOGGING_CATEGORY` し、1箇所の `.cpp` で `Q_LOGGING_CATEGORY` を定義すると一貫性が増す。

### b. 定局面検出ロジックの静的判定

```cpp
// 現状：実行時の文字列比較
static bool isStandardStartposSfen(const QString& sfen) {
    const QString canon = QStringLiteral("lnsgkgsnl/...");
    return (!sfen.isEmpty() && sfen.trimmed() == canon);
}
```

`constexpr` やコンパイル時定数として定義することで、意図がより明確になる。

### c. God Object の兆候

MainWindow 以外にも大規模クラスが存在する：

| クラス | メンバ変数数 | 備考 |
|--------|------------|------|
| JosekiWindow | 約126 | 定跡ウィンドウ |
| CsaGameCoordinator | 約104 | CSA プロトコル変換＋状態管理 |
| JosekiMoveDialog | 約99 | 定跡手ダイアログ |
| ShogiView | 約74 | 描画エンジン |
| GameRecordModel | 約62 | 棋譜データモデル |

CsaGameCoordinator はプロトコル変換層を分離すると責務が明確になる。JosekiWindow / JosekiMoveDialog は UI が複雑なためある程度やむを得ないが、ロジック部分の抽出は検討に値する。

---

## 改善の優先順位まとめ

| 優先度 | 項目 | 期待効果 |
|--------|------|---------|
| **高** | 型安全性の強化（駒・手番の enum 化） | コンパイル時のバグ検出、コードの自己文書化 |
| **高** | テストカバレッジ拡充（USI/CSA/解析） | リグレッション防止、リファクタリングの安全性向上 |
| **中** | エラーハンドリング改善（std::optional 導入） | 不正入力時の安全性向上 |
| **中** | MainWindow の更なる分解 | 保守性の向上 |
| **中** | リソース管理（unique_ptr 化） | メモリリーク防止 |
| **低** | シグナル/スロット整理 | デバッグ容易性の向上 |
| **低** | ロギングカテゴリの共有化 | コード一貫性の向上 |

---

## 8. 追補：未記載の改善提案

以下は、上記セクション（1〜7）に含まれていない追加の改善候補である。

### 8.1 CI の品質ゲート整備（優先度：高）

現状、ローカルでは高品質にビルドできるが、品質基準を自動的に担保する CI 条件が明示されていない。以下をパイプライン化すると回帰を防ぎやすい。

- `cmake -B build -S . -DBUILD_TESTING=ON` + `ctest --output-on-failure`
- `-DENABLE_CLANG_TIDY=ON` / `-DENABLE_CPPCHECK=ON` の定期実行
- Debug ビルドでの `ASan/UBSan` 実行（少なくとも nightly）
- warning を CI fail 条件にする（新規 warning の流入防止）

### 8.2 CMake のソース列挙方式見直し（優先度：中）

現状は `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` で広範囲にソース収集している。規模拡大時は以下の課題が出やすい。

- 意図しないファイルの自動混入
- 差分レビュー時に「どのファイルがビルド対象に追加されたか」が見えにくい

改善として、主要ターゲットは `target_sources()` で明示列挙し、更新コストはスクリプト補助で吸収する方式が望ましい。

### 8.3 ヘッダ依存の削減（優先度：中）

`mainwindow.h` などで重いヘッダを直接取り込んでおり、再ビルド範囲が広がりやすい。前方宣言だけで足りる依存を `.cpp` 側に寄せると、ビルド時間と結合度を抑えられる。

加えて、巨大クラスには `PImpl` の導入を検討すると、公開ヘッダの安定性が上がり、将来の分割も進めやすい。

### 8.4 SettingsService のスキーマ管理（優先度：中）

設定キー文字列が広範囲に散在すると、キー名変更や互換維持時に破壊的変更が起きやすい。次の整備を推奨する。

- キー定義を `constexpr` として一元化
- 設定フォーマットのバージョン番号を保持
- 起動時に旧バージョン設定からの移行関数を実行

これにより、設定追加・改名・廃止の安全性が上がる。

### 8.5 Strategy 実装の `friend` 依存解消（優先度：中）

`MatchCoordinator` は strategy クラスに `friend` を付与している。内部状態への強結合を生みやすいため、`StrategyContext`（必要最小限 API のみ公開）へ置き換えると保守性が向上する。

例：

- `requestGo(Player)` のような高レベル操作を公開
- 残り時間や手番取得は読み取り専用インターフェース経由
- 終局処理は coordinator 側の専用メソッドに限定

### 8.6 棋譜変換ロジックの共通化（優先度：低）

KIF/KI2/CSA/USI/USEN 変換で、文字列分解・数字変換・駒表現変換の類似実装が重複しやすい。共通ユーティリティ（例：`MoveCodec` / `NotationParser`）を設けることで、バグ修正の水平展開漏れを防げる。

### 8.7 翻訳品質の運用基準化（優先度：低）

多言語対応を行っているため、未翻訳・未確定訳の増加を運用で制御したい。`lrelease` 結果を CI で収集し、未翻訳件数の閾値監視（増加時に通知）を導入すると、i18n 品質を維持しやすい。
