# GameRecordModel 分割分析

## 概要

| 項目 | 値 |
|------|-----|
| ファイル | `src/kifu/gamerecordmodel.cpp` / `.h` |
| 行数 | 1,728行 (cpp) + 390行 (h) = 2,118行 |
| 責務数 | 6カテゴリ（後述） |
| 依存元 | 15ファイル以上 |

## 1. 全メソッド分類

### カテゴリA: Pure Data Model（データ保持・アクセス）〜190行

初期化・バインド・コメント/しおり操作・dirty管理。これがクラスの本来の責務。

| メソッド | 行範囲 | 説明 |
|----------|--------|------|
| `GameRecordModel()` | 270-273 | コンストラクタ |
| `bind()` | 279-285 | 外部データストアをバインド |
| `setBranchTree()` | 287-291 | BranchTree参照の設定 |
| `setNavigationState()` | 293-297 | NavigationState参照の設定 |
| `initializeFromDisplayItems()` | 299-318 | 棋譜読み込み時の初期化 |
| `clear()` | 320-327 | 全データクリア |
| `setComment()` | 333-363 | コメント設定（同期更新含む） |
| `setCommentUpdateCallback()` | 365-368 | コメント更新コールバック登録 |
| `comment()` | 370-376 | コメント取得 |
| `comments()` | ヘッダ | コメント配列全体取得（inline） |
| `commentCount()` | ヘッダ | コメント配列サイズ（inline） |
| `ensureCommentCapacity()` | 378-383 | コメント配列拡張 |
| `setBookmark()` | 389-427 | しおり設定（同期更新含む） |
| `bookmark()` | 429-435 | しおり取得 |
| `setBookmarkUpdateCallback()` | 437-440 | しおり更新コールバック登録 |
| `ensureBookmarkCapacity()` | 442-447 | しおり配列拡張 |
| `isDirty()` | ヘッダ | 変更フラグ取得（inline） |
| `clearDirty()` | ヘッダ | 変更フラグクリア（inline） |
| `activeRow()` | 449-456 | アクティブ行インデックス |
| `branchTree()` | ヘッダ | BranchTree参照取得（inline） |
| `syncToExternalStores()` | 462-487 | 外部ストアへの同期（private） |
| signal: `commentChanged()` | ヘッダ | コメント変更シグナル |

### カテゴリB: データ収集・ヘッダ生成（エクスポート共通）〜90行

全エクスポータから使用される共通機能。

| メソッド | 行範囲 | 説明 |
|----------|--------|------|
| `collectMainlineForExport()` | 608-655 | 本譜の表示データを収集 |
| `collectGameInfo()` (static) | 657-745 | 出力コンテキストからヘッダ情報を収集 |
| `resolvePlayerNames()` (static) | 747-779 | 対局者名を解決 |

### カテゴリC: KIF形式出力 〜140行

| メソッド | 行範囲 | 説明 |
|----------|--------|------|
| `toKifLines()` | 493-606 | KIF形式の行リスト生成 |
| `outputVariationFromBranchLine()` | 785-825 | KIF形式の分岐出力 |

### カテゴリD: KI2形式出力 〜130行

| メソッド | 行範囲 | 説明 |
|----------|--------|------|
| `toKi2Lines()` | 831-972 | KI2形式の行リスト生成 |
| `outputKi2VariationFromBranchLine()` | 974-1061 | KI2形式の分岐出力 |

### カテゴリE: USEN形式出力 〜400行

最大のカテゴリ。独自エンコーディングロジックが大量にある。

| メソッド | 行範囲 | 説明 |
|----------|--------|------|
| `toUsenLines()` | 1375-1481 | USEN形式の行リスト生成 |
| `sfenToUsenPosition()` (static) | 1178-1211 | SFEN→USEN局面変換 |
| `encodeUsiMoveToUsen()` (static) | 1135-1176 | USI指し手→USENエンコード |
| `intToBase36()` (static) | 1111-1133 | base36変換 |
| `getUsenTerminalCode()` (static) | 1213-1226 | USEN終局コード |
| `inferUsiFromSfenDiff()` (static) | 1228-1373 | SFEN差分からUSI指し手推測 |

### カテゴリF: USI形式出力 〜70行

| メソッド | 行範囲 | 説明 |
|----------|--------|------|
| `toUsiLines()` | 1659-1728 | USI position コマンド生成 |
| `getUsiTerminalCode()` (static) | 1487-1525 | USI終局コード |

### カテゴリG: BOD形式ヘルパ 〜130行

KIF/KI2の「その他」局面表示で使用。

| メソッド | 行範囲 | 説明 |
|----------|--------|------|
| `sfenToBodLines()` (static) | 1527-1657 | SFEN→BOD形式変換 |

### カテゴリH: CSA/JKF形式出力（既に委譲済み）〜10行

| メソッド | 行範囲 | 説明 |
|----------|--------|------|
| `toCsaLines()` | 1067-1070 | CsaExporter へ委譲 |
| `toJkfLines()` | 1076-1079 | JkfExporter へ委譲 |

### フリー関数（ファイルスコープ static）〜260行

KIF/KI2出力で共用されるヘルパ群。

| 関数 | 行範囲 | 使用箇所 |
|------|--------|----------|
| `fwColonLine()` | 37-40 | KIF/KI2ヘッダ |
| `removeTurnMarker()` | 43-50 | KIF/KI2/USEN |
| `formatKifTime()` | 54-77 | KIF出力 |
| `isTerminalMove()` | 80-101 | KIF/KI2/USEN/USI |
| `buildEndingLine()` | 105-205 | KIF/KI2結果行 |
| `hasNextSibling()` | 210-216 | KIF分岐マーク |
| `formatKifMoveLine()` | 219-225 | KIF指し手行 |
| `appendKifBookmarks()` | 228-239 | KIF/KI2しおり |
| `appendKifComments()` | 241-264 | KIF/KI2コメント |
| `usiPieceToUsenDropCode()` | 1094-1106 | USENエンコード |

## 2. 依存関係分析

### GameRecordModelの利用者一覧

| ファイル | 使用カテゴリ | アクセスパターン |
|----------|-------------|-----------------|
| CommentCoordinator | A (setComment, bookmark, setBookmark) | ポインタ保持 |
| GameRecordModelBuilder | A (bind, setBranchTree, setNavigationState, callbacks) | ファクトリ生成 |
| GameRecordLoadService | A (initializeFromDisplayItems) | コールバック経由 |
| MainWindowResetService | A (clear) | ポインタ保持 |
| BranchNavigationWiring | A (setBranchTree, setNavigationState) | Deps経由 |
| KifuExportController | C,D,E,F,H + A (clearDirty, branchTree) | Deps経由 |
| KifuClipboardService | C,D (toKifLines, toKi2Lines) | ポインタ経由 |
| CsaExporter | B (collectGameInfo, resolvePlayerNames) | const参照 |
| JkfExporter | B (collectGameInfo, collectMainlineForExport, branchTree) | const参照 |
| MainWindowState | 型のみ | ポインタ保持 |
| MainWindowRuntimeRefs | 型のみ | ポインタ保持 |
| MainWindowKifuRegistry | A (生成管理) | ensure管理 |

### 重要な依存方向

```
KifuExportController → GameRecordModel → KifuBranchTree
                                       → KifuNavigationState
                                       → KifDisplayItem (liveDisp)

CsaExporter → GameRecordModel (const ref + static methods)
JkfExporter → GameRecordModel (const ref + static methods)

CommentCoordinator → GameRecordModel (data operations only)
```

### 循環依存: なし

GameRecordModel は他のクラスに依存せず（KifuBranchTree, KifuNavigationState は注入される参照）、一方向の依存関係が保たれている。

## 3. 分割計画

### 設計方針

既存の `CsaExporter` / `JkfExporter` パターンに倣い、各フォーマットのエクスポートロジックを独立した Exporter クラスに抽出する。

- `static QStringList exportLines(const GameRecordModel& model, const ExportContext& ctx, ...)` のシグネチャパターン
- GameRecordModel の公開APIのみ使用（`collectMainlineForExport()`, `collectGameInfo()`, `resolvePlayerNames()`, `branchTree()`）
- フォーマット固有のヘルパ関数は各Exporterファイルのファイルスコープに配置

### 分割後のクラス構成

#### 3.1 GameRecordModel（残存）〜280行

**責務**: データ保持・コメント/しおり管理・エクスポート用ファサード

残すメソッド:
- カテゴリA: 全メソッド（データ保持・アクセス）
- カテゴリB: `collectMainlineForExport()`, `collectGameInfo()` (static), `resolvePlayerNames()` (static)
- ファサード: `toKifLines()`, `toKi2Lines()`, `toUsenLines()`, `toUsiLines()` → 各Exporterへ委譲（1行ずつ）

ヘッダ変更:
- `ExportContext` 構造体はそのまま残す（既存APIの一部）
- private セクションからエクスポート用ヘルパ宣言を削除

#### 3.2 KifExporter（新規）〜400行

**ファイル**: `src/kifu/formats/kifexporter.h` / `.cpp`

```cpp
class KifExporter
{
public:
    static QStringList exportLines(const GameRecordModel& model,
                                   const GameRecordModel::ExportContext& ctx);
};
```

移動するロジック:
- `toKifLines()` 本体 → `exportLines()`
- `outputVariationFromBranchLine()` → ファイルスコープ static
- `sfenToBodLines()` → ファイルスコープ static（KI2からも使えるよう公開ヘルパも検討）
- フリー関数: `fwColonLine`, `removeTurnMarker`, `formatKifTime`, `isTerminalMove`, `buildEndingLine`, `hasNextSibling`, `formatKifMoveLine`, `appendKifBookmarks`, `appendKifComments`
- KifFormat 名前空間定数

#### 3.3 Ki2Exporter（新規）〜200行

**ファイル**: `src/kifu/formats/ki2exporter.h` / `.cpp`

```cpp
class Ki2Exporter
{
public:
    static QStringList exportLines(const GameRecordModel& model,
                                   const GameRecordModel::ExportContext& ctx);
};
```

移動するロジック:
- `toKi2Lines()` 本体 → `exportLines()`
- `outputKi2VariationFromBranchLine()` → ファイルスコープ static
- 必要なフリー関数の複製（`removeTurnMarker`, `isTerminalMove`, `buildEndingLine`, `fwColonLine`, `appendKifBookmarks`, `appendKifComments`）
- `sfenToBodLines()` は KifExporter から呼び出し、または kifexportutils.h に共通化

#### 3.4 UsenExporter（新規）〜400行

**ファイル**: `src/kifu/formats/usenexporter.h` / `.cpp`

```cpp
class UsenExporter
{
public:
    static QStringList exportLines(const GameRecordModel& model,
                                   const GameRecordModel::ExportContext& ctx,
                                   const QStringList& usiMoves);
};
```

移動するロジック:
- `toUsenLines()` 本体 → `exportLines()`
- `sfenToUsenPosition()`, `encodeUsiMoveToUsen()`, `intToBase36()`, `getUsenTerminalCode()`, `inferUsiFromSfenDiff()` → ファイルスコープ static
- `usiPieceToUsenDropCode()`, `kUsenBase36Chars` → ファイルスコープ
- 必要なフリー関数の複製（`removeTurnMarker`, `isTerminalMove`）

#### 3.5 UsiExporter（新規）〜70行

**ファイル**: `src/kifu/formats/usiexporter.h` / `.cpp`

```cpp
class UsiExporter
{
public:
    static QStringList exportLines(const GameRecordModel& model,
                                   const GameRecordModel::ExportContext& ctx,
                                   const QStringList& usiMoves);
};
```

移動するロジック:
- `toUsiLines()` 本体 → `exportLines()`
- `getUsiTerminalCode()` → ファイルスコープ static
- 必要なフリー関数の複製（`removeTurnMarker`, `isTerminalMove`）

### 共有ヘルパの扱い

`removeTurnMarker()` と `isTerminalMove()` は既に CsaExporter.cpp で複製されている。
同じパターンに従い、各 Exporter ファイルにファイルスコープ static として複製する。
コード量は少なく（~20行ずつ）、共通ヘッダを作るオーバーヘッドより複製が適切。

`sfenToBodLines()` は KIF と KI2 の両方で使用されるため、以下の2案がある:
- **案A（推奨）**: KifExporter に public static メソッドとして配置し、Ki2Exporter から `KifExporter::sfenToBodLines()` で呼ぶ
- **案B**: `kifexportutils.h/.cpp` を新設して共通化

案Aの方がファイル数増加を抑えられる。

## 4. 段階的実装計画

各段階でビルド可能であること。API互換性を維持するため、GameRecordModel のファサードメソッドは残す。

### Phase 1: UsenExporter 抽出（最大効果、~400行削減）

1. `src/kifu/formats/usenexporter.h/.cpp` を作成
2. `toUsenLines()` のロジックを `UsenExporter::exportLines()` に移動
3. 関連する static ヘルパ5つをファイルスコープに移動
4. `GameRecordModel::toUsenLines()` を1行委譲に変更:
   ```cpp
   QStringList GameRecordModel::toUsenLines(const ExportContext& ctx, const QStringList& usiMoves) const
   {
       return UsenExporter::exportLines(*this, ctx, usiMoves);
   }
   ```
5. GameRecordModel ヘッダから private ヘルパ宣言を削除
6. CMakeLists.txt に新ファイルを追加
7. ビルド確認

### Phase 2: UsiExporter 抽出（小さい、~70行削減）

1. `src/kifu/formats/usiexporter.h/.cpp` を作成
2. `toUsiLines()` のロジックを `UsiExporter::exportLines()` に移動
3. `getUsiTerminalCode()` をファイルスコープに移動
4. `GameRecordModel::toUsiLines()` を1行委譲に変更
5. CMakeLists.txt 更新
6. ビルド確認

### Phase 3: KifExporter 抽出（~400行削減、BODヘルパ含む）

1. `src/kifu/formats/kifexporter.h/.cpp` を作成
2. `toKifLines()` のロジックを `KifExporter::exportLines()` に移動
3. `outputVariationFromBranchLine()` をファイルスコープに移動
4. `sfenToBodLines()` を `KifExporter` の public static メソッドとして配置
5. KIF関連のフリー関数（9つ）をファイルスコープに移動
6. `GameRecordModel::toKifLines()` を1行委譲に変更
7. CMakeLists.txt 更新
8. ビルド確認

### Phase 4: Ki2Exporter 抽出（~200行削減）

1. `src/kifu/formats/ki2exporter.h/.cpp` を作成
2. `toKi2Lines()` のロジックを `Ki2Exporter::exportLines()` に移動
3. `outputKi2VariationFromBranchLine()` をファイルスコープに移動
4. `sfenToBodLines()` は `KifExporter::sfenToBodLines()` を使用
5. 共用フリー関数（6つ）を複製配置
6. `GameRecordModel::toKi2Lines()` を1行委譲に変更
7. CMakeLists.txt 更新
8. ビルド確認

### Phase 5: クリーンアップ

1. GameRecordModel ヘッダから不要になった private 宣言を削除
2. 不要になった `#include` を削除（`ki2tosfenconverter.h` など）
3. テスト更新（export メソッドのテストは引き続きファサード経由で動作するため変更不要のはず）
4. 全テスト実行・ビルド確認

## 5. 分割後の見通し

| クラス | 行数(cpp) | 責務 |
|--------|-----------|------|
| GameRecordModel | ~280 | データ保持・コメント/しおり・エクスポートファサード |
| KifExporter | ~400 | KIF形式出力（BODヘルパ含む） |
| Ki2Exporter | ~200 | KI2形式出力 |
| CsaExporter（既存） | ~760 | CSA形式出力 |
| JkfExporter（既存） | ~740 | JKF形式出力 |
| UsenExporter | ~400 | USEN形式出力 |
| UsiExporter | ~70 | USI形式出力 |

**削減量**: GameRecordModel は 1,728行 → ~280行（約84%削減）

## 6. リスク・注意点

1. **API互換性**: ファサードメソッドを残すため、既存の呼び出し元は変更不要
2. **テスト**: `tst_gamerecordmodel.cpp` のexport テストはファサード経由のため影響なし
3. **ExportContext**: GameRecordModel に残す（全Exporterから参照される型）
4. **collectMainlineForExport**: public のまま残す（CsaExporter, JkfExporter が直接呼ぶ）
5. **フリー関数の複製**: CsaExporter で確立されたパターンに従う。将来的に共通化を検討可能
6. **Ki2ToSfenConverter 依存**: `toKi2Lines()` は `Ki2ToSfenConverter::convertPrettyMoveToKi2()` と `Ki2ToSfenConverter::initBoardFromSfen()` を使用。Ki2Exporter に移動しても依存先は同じ `formats/` ディレクトリ内なので問題なし
