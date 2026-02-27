# 棋譜変換器 共通処理分析と集約計画

## 1. 現状のファイルサイズ

| ファイル | 行数 | 備考 |
|---|---|---|
| `kiftosfenconverter.cpp` | 1,278 | KIF形式（手番号＋移動元座標あり） |
| `ki2tosfenconverter.cpp` | 1,549 | KI2形式（移動元なし→盤面推論必要） |
| `csatosfenconverter.cpp` | 1,004 | CSA形式（コンピュータ向け、+7776FU形式） |
| `jkftosfenconverter.cpp` | 949 | JKF形式（JSON構造化データ） |
| `parsecommon.cpp` | 304 | 既存共通ユーティリティ |
| `notationutils.cpp` | 100 | 既存座標変換ユーティリティ |
| `kifreader.cpp` | 161 | 文字コード自動判別読み込み |
| **合計** | **5,345** | |

### テストファイル

| テストファイル | 行数 |
|---|---|
| `tst_kifconverter.cpp` | 185 |
| `tst_ki2converter.cpp` | 57 |
| `tst_csaconverter.cpp` | 285 |
| `tst_jkfconverter.cpp` | 53 |
| **合計** | **580** |

KI2とJKFのテストカバレッジが極めて薄い（各50行台）。リファクタリング前にテスト拡充が望ましい。

---

## 2. 各変換器の処理フロー分析

### 2.1 KIF変換器（1,278行）

```
入力: KIFテキストファイル（手番号・移動元座標あり）

1. ファイル読込     KifReader::readAllLinesAuto()  ← 共通
2. 初期局面検出     detectInitialSfenFromFile()
                    ├─ buildInitialSfenFromBod()     ← BOD盤面図解析（~200行）
                    └─ 手合割ラベル → SFEN           ← NotationUtils共通
3. 行分類           isCommentLine(), isBookmarkLine(),
                    isSkippableLine(), isBoardHeaderOrFrame(),
                    containsAnyTerminal(), startsWithMoveNumber()
4. 指し手解析       convertMoveLine()
                    ├─ findDestination()              数字+漢数字 → 座標, "同"対応
                    ├─ pieceKanjiToUsiUpper()          漢字駒名 → USI文字
                    ├─ isPromotionMoveText()           成り判定
                    └─ NotationUtils::formatSfen*()    USI文字列生成  ← 共通
5. 表示アイテム構築  extractMovesWithTimes()
                    ├─ 時間解析 (kifTimeRe正規表現)
                    ├─ コメント/しおり処理
                    └─ 終局語判定 (isTerminalWord)
6. 変化解析         parseWithVariations()
                    ├─ extractMainLine()
                    ├─ variationHeaderCaptureRe()
                    └─ parseVariationBlock()
7. 対局情報抽出     extractGameInfo()                 "key：value" 形式
```

**特徴**: 移動元座標が明示されるため、盤面状態の追跡は不要。変化（分岐）の解析機能あり。

### 2.2 KI2変換器（1,549行）

```
入力: KI2テキストファイル（移動元座標なし）

1. ファイル読込     KifReader::readAllLinesAuto()  ← 共通
2. 初期局面検出     detectInitialSfenFromFile()
                    └─ buildInitialSfenFromBod()     ← KIF実装に委譲
3. 盤面初期化       initBoardFromSfen()              SFEN → board[9][9] + hands
4. 行分類           isCommentLine(), isBookmarkLine(),
                    isSkippableLine(), isBoardHeaderOrFrame(),
                    isKi2MoveLine() (▲/△ prefix)
5. 指し手抽出       extractMovesFromLine()           1行から複数手を分離
6. 指し手→USI変換  convertKi2MoveToUsi()
                    ├─ findDestination()              座標取得
                    ├─ getPieceTypeAndPromoted()       駒種・成り取得
                    ├─ extractMoveModifier()           右/左/上/引/寄/直 抽出
                    ├─ inferSourceSquare()             移動元推論
                    │   ├─ collectCandidates()         候補駒収集
                    │   └─ filterByDirection()         修飾語で絞込
                    └─ NotationUtils::formatSfen*()    ← 共通
7. 盤面更新         applyMoveToBoard()               盤面状態を手に反映
8. 表示アイテム構築  extractMovesWithTimes()          ※KI2には時間情報なし
9. KI2出力          convertPrettyMoveToKi2()          KIF→KI2変換用
                    └─ generateModifier()             修飾語自動生成
10. 対局情報抽出    extractGameInfo()                 KIFとほぼ同一
```

**特徴**: 盤面追跡による移動元推論が核心機能。KI2固有の修飾語（右/左/上/引/寄/直）の解析・生成あり。変化の解析はなし。

### 2.3 CSA変換器（1,004行）

```
入力: CSAテキストファイル（コンピュータ向け）

1. ファイル読込     readAllLinesDetectEncoding()     独自エンコーディング検出
2. 初期局面解析     parseStartPos()
                    ├─ PI (平手)
                    ├─ P1..P9 (盤面行)              applyPRowLine()
                    ├─ P+/P- (持駒/配置)            applyPPlusMinusLine()
                    ├─ 00AL (残り全部)              processAlRemainder()
                    └─ Board → SFEN                  toSfenBoard() + handsToSfen()
3. 行分類           isMoveLine(), isResultLine(),
                    isMetaLine(), isCommentLine()
4. 指し手解析       parseMoveLine()
                    ├─ parseCsaMoveToken()            +7776FU → 座標+駒種
                    ├─ 盤面更新（Board struct）
                    ├─ USI文字列生成                  NotationUtils  ← 共通
                    └─ Pretty表記生成                 side mark + 座標 + 漢字
5. 時間解析         parseTimeTokenMs()               T秒トークン
6. 終局コード       csaResultToLabel()               %TORYO → "投了"
7. メインパーサ     parse()                          CsaParseState構造体
                    ├─ カンマ区切りトークン処理
                    └─ コメント正規化 (normalizeCsaCommentLine)
8. 対局情報抽出     extractGameInfo()                 $KEY:VALUE, N+/N-
```

**特徴**: 独自のBoard/Cell/Piece型定義。指し手解析と盤面更新が一体化。CSA固有のPI/P行形式の複雑な初期局面解析。

### 2.4 JKF変換器（949行）

```
入力: JSONファイル

1. ファイル読込     loadJsonFile()                   JSON解析
2. 初期局面検出     buildInitialSfen()
                    ├─ presetToSfenImpl()             英語プリセット → SFEN
                    └─ buildSfenFromInitialData()     カスタム局面 → SFEN
3. 指し手解析       convertMoveToUsi()               from/to JSON → USI
4. Pretty表記生成   convertMoveToPretty()            JSON → 日本語表記
                    ├─ pieceKindToKanjiImpl()          駒名漢字
                    ├─ relativeToModifierImpl()        修飾語生成
                    └─ zenkakuDigit()/kanjiRank()      数字変換
5. 時間解析         formatTimeText()                 now/total JSON → mm:ss/HH:MM:SS
6. 終局語           specialToJapaneseImpl()          TORYO → "投了"
7. 変化解析         parseMovesArray()                forks配列処理
8. 対局情報         extractGameInfo()                header JSON → list
```

**特徴**: テキスト解析が不要（JSON構造化データ）。最もシンプルな変換器。独自の数字変換関数がNotationUtilsと重複。

---

## 3. 重複コードの定量的洗い出し

### 3.1 既に共有化済みの処理

| 処理 | 共有先 | 利用変換器 |
|---|---|---|
| flexDigit変換 | `KifuParseCommon` | KIF, KI2 |
| 終局語リスト（16語） | `KifuParseCommon::terminalWords()` | KIF, KI2 |
| 終局語判定 | `KifuParseCommon::isTerminalWord*()` | KIF, KI2 |
| 漢字駒名→Piece | `KifuParseCommon::mapKanjiPiece()` | KIF, KI2 |
| 盤面枠行判定 | `KifuParseCommon::isBoardHeaderOrFrame()` | KIF, KI2 |
| スキップ行判定 | `KifuParseCommon::isKifSkippableHeaderLine()` | KIF, KI2 |
| 終局語含有判定 | `KifuParseCommon::containsAnyTerminal()` | KIF, KI2 |
| USI移動フォーマット | `NotationUtils::formatSfenMove/Drop()` | KIF, KI2, CSA, JKF |
| 手合→初期SFEN | `NotationUtils::mapHandicapToSfen()` | KIF, KI2 |
| 全角/漢数字変換 | `NotationUtils::intToZenkaku/KanjiDigit()` | KIF, KI2, CSA |
| BOD盤面解析 | `KifToSfenConverter::buildInitialSfenFromBod()` | KIF, KI2(委譲) |

### 3.2 重複しているが未共有の処理

#### A. KIF↔KI2 間の重複（高優先度）

| 処理 | KIF行数 | KI2行数 | 類似度 | 統合可能性 |
|---|---|---|---|---|
| `isCommentLine()` | 5 | 5 | 完全一致 | ★ 即座に共有可能 |
| `isBookmarkLine()` | 1 | 1 | 完全一致 | ★ 即座に共有可能 |
| `findDestination()` | 40 | 35 | ~80% | ◎ パラメータ化で統合可 |
| `isPromotionMoveText()` | 35 | 20 | ~80% | ◎ パラメータ化で統合可 |
| `detectInitialSfenFromFile()` | 40 | 35 | ~90% | ◎ 共通ヘルパに抽出可 |
| `extractOpeningComment()` | 40 | 35 | ~85% | ◎ 共通ヘルパに抽出可 |
| `extractGameInfo()` | 80 | 50 | ~90% | ◎ 共通ヘルパに抽出可 |
| `extractGameInfoMap()` | 5 | 5 | 完全一致 | ★ 即座に共有可能 |
| **小計** | **~246** | **~186** | | 統合で**~180行**削減見込 |

#### B. CSA+JKF 間の重複（中優先度）

| 処理 | CSA行数 | JKF行数 | 類似度 | 統合可能性 |
|---|---|---|---|---|
| 終局語→日本語マッピング | 16 | 18 | ~90% | ◎ 共通テーブル化 |
| 駒種→漢字変換 | 16 | 14 | ~90% | ◎ CSAコード基準で統合可 |
| CSAコード→駒種 | 14 | 14 | ~80% | ○ 戻り値型が異なる |
| basePieceOf/isPromoted | 12 | 10 | ~80% | ○ CSA固有Piece型 |
| **小計** | **~58** | **~56** | | 統合で**~50行**削減見込 |

#### C. 時間フォーマット重複（中優先度）

| 処理 | KIF行数 | CSA行数 | JKF行数 | 統合可能性 |
|---|---|---|---|---|
| mm:ss フォーマット | - | 7 | 6 | ★ 同一 |
| HH:MM:SS フォーマット | - | 8 | 7 | ★ 同一 |
| mm:ss/HH:MM:SS 結合 | 15 | 3 | 3 | ◎ |
| **小計** | **~15** | **~18** | **~16** | 統合で**~30行**削減見込 |

#### D. JKF→NotationUtils重複（低優先度）

| 処理 | JKF行数 | 既存共通 | 統合可能性 |
|---|---|---|---|
| `zenkakuDigit()` | 8 | `NotationUtils::intToZenkakuDigit()` | ★ 置換のみ |
| `kanjiRank()` | 8 | `NotationUtils::intToKanjiDigit()` | ★ 置換のみ |
| **小計** | **~16** | | 統合で**~16行**削減見込 |

#### E. その他の小規模重複

| 処理 | 行数 | 変換器 | 統合可能性 |
|---|---|---|---|
| 開始局面アイテム作成 | ~10×2 | CSA, JKF（inline） | ○ `createOpeningDisplayItem`使用 |
| `extractGameInfoMap()` | ~5×3 | KIF, KI2, JKF | ★ 共通化可 |
| **小計** | **~25** | | 統合で**~20行**削減見込 |

### 3.3 重複削減サマリ

| カテゴリ | 推定重複行数 | 削減見込 |
|---|---|---|
| A. KIF↔KI2共有 | ~430 | ~180行 |
| B. CSA+JKF共有 | ~114 | ~50行 |
| C. 時間フォーマット | ~49 | ~30行 |
| D. JKF→NotationUtils | ~16 | ~16行 |
| E. その他 | ~25 | ~20行 |
| **合計** | **~634** | **~296行削減** |

現行4,780行中、約6%の削減（4,780→4,484行目安）。
共通ユーティリティ（parsecommon + notationutils）は404行→約550行に増加。

---

## 4. パイプライン設計

### 4.1 現実的なパイプライン構造

各変換器の処理は以下の5段階に分解できるが、フォーマット固有部分が大きいため「完全なパイプライン共通化」は非現実的である。

```
┌─────────────────────────────────────────────────────────────────┐
│ Stage 1: File Reading (フォーマット固有)                         │
│                                                                 │
│   KIF/KI2: KifReader::readAllLinesAuto()        [共通]          │
│   CSA:     readAllLinesDetectEncoding()          [独自]          │
│   JKF:     loadJsonFile() → QJsonObject          [独自]          │
└────────────────────────┬────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────────┐
│ Stage 2: Initial Position Setup (部分共通)                       │
│                                                                 │
│   KIF/KI2: handicap label → SFEN                [共通]          │
│            BOD → SFEN                            [共通]          │
│   CSA:     PI/P行 → Board → SFEN                [独自]          │
│   JKF:     preset/data → SFEN                    [独自]          │
│                                                                 │
│   共通出力: 初期SFEN文字列                                        │
└────────────────────────┬────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────────┐
│ Stage 3: Move Parsing (フォーマット固有)                         │
│                                                                 │
│   KIF: text line → dest + piece + from + promote → USI          │
│   KI2: text → dest + piece + modifier → board推論 → USI         │
│   CSA: token → from + to + piece → USI + board更新               │
│   JKF: JSON → from + to + piece + promote → USI                 │
│                                                                 │
│   共通出力: USI指し手文字列                                       │
│   共通ユーティリティ:                                             │
│     - formatSfenMove/Drop()          [既存共通]                  │
│     - findDestination()              [KIF/KI2共通化対象]         │
│     - isPromotionMoveText()          [KIF/KI2共通化対象]         │
│     - mapKanjiPiece()                [既存共通]                  │
└────────────────────────┬────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────────┐
│ Stage 4: Display Item Construction (部分共通)                    │
│                                                                 │
│   共通処理:                                                      │
│     - 手番マーク (▲/△) from ply number                          │
│     - 座標表記 (全角+漢数字 or "同")        [既存共通]            │
│     - 駒漢字名                              [部分共通]            │
│     - 時間フォーマット (mm:ss/HH:MM:SS)     [共通化対象]         │
│     - 開始局面アイテム生成                   [既存共通]            │
│     - コメント付与                           [既存共通]            │
│                                                                 │
│   フォーマット固有:                                               │
│     - KIF: 時間正規表現パース                                    │
│     - KI2: 修飾語付き表記生成                                    │
│     - CSA: Tトークンパース、CSAコメント正規化                     │
│     - JKF: JSONコメント配列処理                                  │
│                                                                 │
│   共通出力: QList<KifDisplayItem>                                │
└────────────────────────┬────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────────┐
│ Stage 5: Result/Terminal Handling (部分共通)                     │
│                                                                 │
│   共通: 終局語の日本語テキスト（投了、中断、千日手、...）          │
│                                                                 │
│   フォーマット固有トリガ:                                         │
│     - KIF/KI2: 終局語テキスト直接マッチ      [既存共通]           │
│     - CSA:     %TORYO等のCSAコード           [独自→共通化対象]   │
│     - JKF:     special JSONフィールド         [独自→共通化対象]   │
│                                                                 │
│   共通出力: 終局タイプ + 日本語ラベル                             │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 共通インターフェース（データ型）

既存のデータ型が既にパイプラインの出力型として機能している：

```cpp
// パイプライン出力型（既存・変更不要）
struct KifDisplayItem;      // Stage 4 の出力単位
struct KifLine;             // Stage 3-5 の手順データ
struct KifParseResult;      // 全体の出力（本譜+変化）
struct KifGameInfoItem;     // 対局情報
```

### 4.3 共通化の方針

**方針**: 新しい抽象レイヤー（基底クラス/テンプレート等）は導入しない。既存の `parsecommon` / `notationutils` にフリー関数を追加する形で共通処理を集約する。

**理由**:
1. 4つの変換器は全てstaticメンバ関数のみで構成されており、インスタンス状態を持たない
2. フォーマット間の差異が大きく、共通基底クラスは不自然
3. フリー関数の共有が最もシンプルで低リスク

---

## 5. 段階的実装計画

### Phase 1: KIF↔KI2 共通処理の parsecommon 集約（~180行削減）

**目的**: KIFとKI2で重複している7つの処理をparsecommonに移動。

**対象関数**:

| 関数 | 移動先 | 備考 |
|---|---|---|
| `isCommentLine()` (KIF形式) | `KifuParseCommon::isKifCommentLine()` | '*' or '＊' |
| `isBookmarkLine()` | `KifuParseCommon::isBookmarkLine()` | '&' |
| `findDestination()` | `KifuParseCommon::findDestination()` | ▲/△除去をオプション化 |
| `isPromotionMoveText()` | `KifuParseCommon::isPromotionMoveText()` | 除去マーカーをパラメータ化 |
| `extractGameInfo()` | 共通ヘルパ関数 | KIF/KI2で呼出し |
| `extractGameInfoMap()` | `KifuParseCommon::toGameInfoMap()` | 3変換器で共通 |
| `detectInitialSfenFromFile()` | 共通ヘルパ分離 | BOD試行→手合割の共通フロー |
| `extractOpeningComment()` | 共通ヘルパ分離 | 行判定コールバックで差異吸収 |

**作業手順**:
1. `parsecommon.h/.cpp` に新関数を追加
2. KIF変換器の各関数を共通関数の呼び出しに置換
3. KI2変換器の各関数を共通関数の呼び出しに置換
4. テスト実行で回帰がないことを確認
5. 各変換器のprivate宣言から移動した関数を削除

**リスク**: 低。関数シグネチャの微妙な差異（KI2の`findDestination`は▲/△除去が必要）に注意。

### Phase 2: 時間フォーマットの共通化（~30行削減）

**目的**: mm:ss / HH:MM:SS / mm:ss/HH:MM:SS フォーマットを NotationUtils に集約。

**対象**:

| 関数 | 現在の場所 | 移動先 |
|---|---|---|
| `mmssFromMs()` / `formatMS()` | CSA (static), JKF (static) | `NotationUtils::formatTimeMS()` |
| `hhmmssFromMs()` / `formatHMS()` | CSA (static), JKF (static) | `NotationUtils::formatTimeHMS()` |
| `composeTimeText()` | CSA (static) | `NotationUtils::formatTimeText()` |

**作業手順**:
1. `notationutils.h/.cpp` に時間フォーマット関数を追加
2. CSA変換器のローカル関数を共通関数の呼び出しに置換
3. JKF変換器のローカル関数を共通関数の呼び出しに置換
4. KIF変換器でも使える箇所があれば適用

### Phase 3: 終局語マッピングの共通化（~50行削減）

**目的**: CSAコード/JKFスペシャル→日本語の変換を共通テーブル化。

**対象**:

| 関数 | 現在の場所 | 統合先 |
|---|---|---|
| `csaResultToLabel()` | CSA (anonymous ns) | `KifuParseCommon::csaSpecialToJapanese()` |
| `specialToJapaneseImpl()` | JKF (anonymous ns) | 同上（JKFのキーは%なしCSAコードと同一） |

**設計**: CSAの `%TORYO` とJKFの `TORYO` は `%` プレフィックスの有無のみが異なる。共通関数は `%` を除去した上で同一テーブルを参照する。

**作業手順**:
1. `parsecommon.h/.cpp` に `csaSpecialToJapanese()` を追加
2. CSA変換器の `csaResultToLabel()` を置換
3. JKF変換器の `specialToJapaneseImpl()` を置換

### Phase 4: JKF の NotationUtils 重複解消（~16行削減）

**目的**: JKF固有の数字変換関数を既存NotationUtils関数に置換。

**対象**:

| JKF関数 | 置換先 |
|---|---|
| `zenkakuDigit(int)` | `NotationUtils::intToZenkakuDigit(int)` |
| `kanjiRank(int)` | `NotationUtils::intToKanjiDigit(int)` |

**作業手順**:
1. JKF変換器の `convertMoveToPretty()` 内の呼び出しを置換
2. anonymous namespace から不要になった関数を削除

### Phase 5: CSA/JKF の開始局面アイテム共通化（~10行削減）

**目的**: CSA/JKFのインライン開始局面アイテム生成を `KifuParseCommon::createOpeningDisplayItem()` に統一。

**作業手順**:
1. CSA変換器のインライン生成箇所を `createOpeningDisplayItem()` 呼び出しに置換
2. JKF変換器の同様の箇所を置換

### Phase 6: CSA駒変換の共通化検討（要検討）

**現状**: CSA変換器は独自のPiece enum（`NO_P, FU, KY, ...`）を持つ。JKFも `pieceKindFromCsaImpl()` でCSA駒コードを使う。

**課題**: CSAの`Piece`型とcore層の`Piece`型が異なる。統合するとCSA変換器の大幅な書き換えが必要。

**判断**: **Phase 6は保留**。CSA独自のPiece型はCSA形式のP行解析と一体化しており、共通化のコスト対効果が低い。JKFの `pieceKindFromCsaImpl()` / `pieceKindToKanjiImpl()` のみ小規模共通化の余地あり。

---

## 6. 実施順序とリスク評価

| Phase | 難易度 | リスク | 削減行数 | 優先度 |
|---|---|---|---|---|
| Phase 1: KIF↔KI2共有 | 中 | 低 | ~180 | 高 |
| Phase 2: 時間フォーマット | 低 | 低 | ~30 | 中 |
| Phase 3: 終局語マッピング | 低 | 低 | ~50 | 中 |
| Phase 4: JKF NotationUtils | 低 | 低 | ~16 | 中 |
| Phase 5: 開始局面アイテム | 低 | 低 | ~10 | 低 |
| Phase 6: CSA駒変換 | 高 | 中 | ~30 | 保留 |

**推奨実施順序**: Phase 1 → Phase 3 → Phase 2 → Phase 4 → Phase 5

Phase 1が最大の効果を持つため最優先。Phase 3はPhase 2より先に行うことで、CSA/JKFの終局語処理が統一され、Phase 2の時間フォーマット統合時の見通しが良くなる。

---

## 7. 注意事項

### 7.1 完全共通化が不可能な領域

1. **CSAの初期局面解析（PI/P行）**: CSA固有の複雑な形式。共通化不可。
2. **KI2の移動元推論エンジン**: KI2固有の核心機能。他形式では不要。
3. **JKFのJSON解析**: テキスト処理とは根本的に異なる。共通化不可。
4. **CSAの独自Board/Piece型**: 形式固有の最適化。変更コスト高。

### 7.2 テスト拡充の必要性

リファクタリング前に以下のテスト拡充を推奨：

| テスト | 現状行数 | 推奨追加 |
|---|---|---|
| `tst_ki2converter.cpp` | 57 | 修飾語テスト、BOD付きテスト、終局語テスト |
| `tst_jkfconverter.cpp` | 53 | 変化テスト、カスタム初期局面テスト、時間テスト |

### 7.3 parsecommon の肥大化への対策

Phase 1-5 の実施後、parsecommon は~550行に成長する。必要に応じて以下の分割を検討：

- `parsecommon.h/.cpp` → KIF/KI2テキスト解析用ユーティリティ（現行）
- `kifucommon.h/.cpp` → 全フォーマット共通（終局語、時間、GameInfoMap等）

ただし、現段階での分割は時期尚早。550行程度であれば1ファイルで管理可能。
