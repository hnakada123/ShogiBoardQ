# Converter 共通パイプライン設計書

## 1. 概要

本文書は、6つの棋譜コンバータ（KIF, KI2, CSA, JKF, USI, USEN → SFEN）間の重複ロジックを削減するための共通パイプライン設計を記述する。

### 対象コンバータ

| コンバータ | ファイル | 行数(cpp) | Lexer/Parser |
|---|---|---|---|
| KIF | `kiftosfenconverter.cpp` | 583 | `KifLexer` |
| KI2 | `ki2tosfenconverter.cpp` | 582 | `Ki2Lexer` |
| CSA | `csatosfenconverter.cpp` | 331 | `CsaLexer` |
| JKF | `jkftosfenconverter.cpp` | 451 | `JkfMoveParser` |
| USI | `usitosfenconverter.cpp` | 676 | (内蔵) |
| USEN | `usentosfenconverter.cpp` | 911 | (内蔵) |

### 既存の共通基盤

- `parsecommon.h/.cpp` — 数値変換、終局語判定、コメント処理、時間フォーマット
- `notationutils.h/.cpp` — 座標変換、手合→SFEN変換、USI指し手文字列生成
- `kifdisplayitem.h` — 表示用データ構造体
- `kifparsetypes.h` — パース結果データ構造体（KifLine, KifVariation, KifParseResult）
- `sfenpositiontracer.h/.cpp` — 盤面追跡・SFEN生成

---

## 2. 現在の重複箇所マップ

### 2.1 完全重複（コピー＆ペースト）

| 重複コード | 出現箇所 | 推定削減行数 |
|---|---|---|
| `usiToPrettyMove()` | USI, USEN | ~80行 |
| `pieceToKanji()` | USI, USEN | ~20行 |
| `tokenToKanji()` | USI, USEN | ~20行 |
| `rankLetterToNum()` | USI, USEN | ~5行 |
| `extractPieceTokenFromUsi()` (無名名前空間) | USI, USEN | ~15行 |
| `buildKifLine()` | USI, USEN | ~55行 |
| `kZenkakuDigits`, `kKanjiRanks` 定数 | USI, USEN | ~4行 |
| 終局コードテーブル (`kTerminalCodes[]`) | USI, USEN (異なる内容だが同じ構造) | ~15行 |

**小計: ~215行の完全重複**

### 2.2 パターン重複（同一構造・微差）

| 重複パターン | 出現箇所 | 内容 |
|---|---|---|
| 手番文字列生成 | 全6コンバータ | `(ply % 2 != 0) ? "▲" : "△"` |
| 開始局面エントリ生成 | CSA, JKF, USI, USEN | `KifDisplayItem{ply=0, time="00:00/00:00:00"}` |
| 終局語→日本語変換 | USI(`terminalCodeToJapanese`), USEN(同), CSA(`csaResultToLabel`), JKF(`specialToJapanese`) | テーブル引きの実装パターンは同じ |
| コメントバッファ flush | KIF, KI2, CSA, JKF | pending → last item に付与するパターン |
| `extractGameInfo` ヘッダ解析 | KIF, KI2 | ほぼ同一の正規表現とヘッダパース（~50行） |
| 盤面初期化＋SFEN解析 | KI2(`initBoardFromSfen`), `SfenPositionTracer` | KI2が独自のSFEN盤面パーサを持つ |
| `extractGameInfoMap` ラッパー | KIF, KI2, JKF | `toGameInfoMap(extractGameInfo(...))` の1行ラッパー |
| `mapHandicapToSfen` ラッパー | KIF, KI2 | `NotationUtils::mapHandicapToSfen()` への委譲 |
| `buildInitialSfenFromBod` | KI2→KIF | KI2がKIFに完全委譲 |
| `detectInitialSfenFromFile` | KI2→KIF | KI2がKIFに完全委譲 |

### 2.3 共通化の難易度別分類

```
容易（単純な関数抽出）:
├── usiToPrettyMove / pieceToKanji / tokenToKanji → 共通ユーティリティへ
├── teban文字列生成 → ヘルパ関数化
├── 開始局面エントリ生成 → createOpeningDisplayItem() の統一使用
└── extractGameInfoMap ラッパー → 既存 toGameInfoMap() への統合

中程度（インターフェース設計が必要）:
├── buildKifLine / extractMovesWithTimes の共通ループ → CommonMoveApplier
├── 終局コード変換の統一テーブル → 形式別テーブル + 共通ルックアップ
└── コメントバッファ管理 → CommentAccumulator クラス

困難（形式固有ロジックとの分離が必要）:
├── parseWithVariations の共通化 → 変化の表現方法が形式で大きく異なる
└── extractGameInfo の共通化 → ヘッダ形式が全く異なる
```

---

## 3. 共通パイプラインのアーキテクチャ

### 3.1 処理段階モデル

現在の各コンバータは以下の4段階を内包している:

```
┌──────────────┐   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐
│  FileReader  │──▶│ FormatParser │──▶│ MoveApplier  │──▶│ResultBuilder │
│              │   │              │   │              │   │              │
│ ファイルI/O  │   │ 形式固有の   │   │ USI変換      │   │ KifParseResult│
│ エンコーディ │   │ 字句解析     │   │ 盤面追跡     │   │ 組み立て     │
│ ング検出     │   │ 構文解析     │   │ Pretty生成   │   │ sfenList構築 │
│              │   │ 指し手抽出   │   │ コメント管理 │   │ gameMoves構築│
└──────────────┘   └──────────────┘   └──────────────┘   └──────────────┘
  形式固有           形式固有            共通可能           共通可能
```

### 3.2 共通化対象の判別

```
         ┌─────────────────────────────────────────────┐
         │         形式固有（変更なし）                 │
         │                                             │
         │  KifLexer: KIF字句解析・手番号検出          │
         │  Ki2Lexer: KI2曖昧解決・候補絞り込み       │
         │  CsaLexer: CSA盤面管理・座標変換           │
         │  JkfMoveParser: JSON指し手変換              │
         │                                             │
         │  各Converter: ファイルI/O・状態管理ループ   │
         └─────────────────────────────────────────────┘

         ┌─────────────────────────────────────────────┐
         │         共通化対象                           │
         │                                             │
         │  ① UsiPrettyMoveBuilder (新規)              │
         │     - usiToPrettyMove()                     │
         │     - pieceToKanji() / tokenToKanji()       │
         │     - extractPieceTokenFromUsi()            │
         │     - kZenkakuDigits / kKanjiRanks 定数     │
         │                                             │
         │  ② KifLineBuilder (新規)                    │
         │     - buildKifLine() の共通実装             │
         │     - 終局項目追加                          │
         │     - SfenPositionTracer連携                │
         │                                             │
         │  ③ TerminalCodeResolver (新規)              │
         │     - 全形式の終局コード→日本語統一マップ   │
         │                                             │
         │  ④ parsecommon 拡張                         │
         │     - tebanMark(ply) ヘルパ                 │
         │     - createOpeningDisplayItem() の統一使用 │
         │     - CommentAccumulator クラス             │
         └─────────────────────────────────────────────┘
```

### 3.3 クラス図

```
namespace KifuParseCommon {              (既存、拡張)
  ├── tebanMark(int ply) → QString      (新規: "▲"/"△" 生成)
  ├── createOpeningDisplayItem()         (既存)
  ├── CommentAccumulator                 (新規)
  │   ├── appendComment(text)
  │   ├── flushToLastItem(items)
  │   └── openingComment() / clear()
  └── ...既存関数群...
}

class UsiPrettyMoveBuilder {             (新規)
  static:
  ├── usiToPrettyMove(usi, ply, prevToFile&, prevToRank&, pieceToken)
  │   → QString  (日本語指し手表記)
  ├── pieceToKanji(QChar usiPiece) → QString
  ├── tokenToKanji(QString token) → QString
  └── extractPieceToken(usi, SfenPositionTracer&) → QString
}

class KifLineBuilder {                   (新規)
  static:
  └── buildFromUsiMoves(
        usiMoves, baseSfen, startPly,
        terminalCode, terminalMapper,
        outLine&, warn*
      )
      → USI指し手列から KifLine (disp + usiMoves) を構築
         終局コードは terminalMapper で形式別変換
}

class TerminalCodeResolver {             (新規)
  static:
  ├── fromUsi(code) → QString           (USI形式: resign, break, ...)
  ├── fromUsen(code) → QString          (USEN形式: r, i, t, ...)
  ├── fromCsa(code) → QString           (CSA形式: %TORYO, %CHUDAN, ...)
  ├── fromJkf(code) → QString           (JKF形式: TORYO, CHUDAN, ...)
  └── fromKif(text) → QString           (KIF形式: 投了, 中断, ... → 正規化)
}
```

---

## 4. 各形式の差分一覧

### 4.1 入力形式と字句解析の差分

| 項目 | KIF | KI2 | CSA | JKF | USI | USEN |
|---|---|---|---|---|---|---|
| 入力形式 | テキスト行 | テキスト行 | テキスト行 | JSON | テキスト1行 | エンコード文字列 |
| エンコーディング | Shift-JIS/UTF-8自動検出 | 同左 | CSAヘッダ/自動検出 | UTF-8(JSON) | 自動検出 | UTF-8 |
| 手番号 | あり(明示) | なし(推定) | なし(+/-で表現) | なし(配列index) | なし(順序) | なし(順序) |
| 移動元 | `(筋段)`で明示 | 省略(盤面から推定) | 4桁座標で明示 | `from`オブジェクト | USI座標 | base36エンコード |
| 時間情報 | あり(消費時間) | なし | あり(T行) | あり(time obj) | なし | なし |
| コメント | `*`/`＊`行 | 同左 | `'`行 | comments配列 | なし | なし |
| 変化(分岐) | `変化：N手`ブロック | なし | なし | forks配列 | なし | `~N.`区切り |
| 終局表現 | 日本語テキスト | 結果行 | `%`コード | special文字列 | 英語コード | 1文字コード |

### 4.2 盤面追跡の必要性

| コンバータ | USI変換に盤面追跡が必要か | Pretty生成に盤面追跡が必要か |
|---|---|---|
| KIF | 不要（移動元が明示） | 不要（原文がPretty） |
| KI2 | **必要**（移動元を推定） | 不要（原文がPretty） |
| CSA | 不要（移動元が明示） | 不要（CsaLexerが生成） |
| JKF | 不要（移動元が明示） | 不要（JkfMoveParserが生成） |
| USI | 不要（入力がUSI） | **必要**（駒名をSFENから取得） |
| USEN | 不要（デコードでUSI生成） | **必要**（駒名をSFENから取得） |

### 4.3 各コンバータの公開API対応表

| API | KIF | KI2 | CSA | JKF | USI | USEN |
|---|---|---|---|---|---|---|
| `detectInitialSfenFromFile` | ○ | ○(KIF委譲) | × | ○ | ○ | ○ |
| `convertFile` | ○ | ○ | × | ○ | ○ | ○ |
| `extractMovesWithTimes` | ○ | ○ | × | ○ | ○ | ○ |
| `parseWithVariations` | ○ | ○ | ×(parse()) | ○ | ○ | ○ |
| `extractGameInfo` | ○ | ○ | ○ | ○ | ○(空) | ○(空) |
| `extractGameInfoMap` | ○ | ○ | × | ○ | ○(空) | ○(空) |

CSAのみ `parse()` という統一APIを持ち、`convertFile`/`extractMovesWithTimes` を個別には提供していない。

---

## 5. 共通化の具体的な設計

### 5.1 UsiPrettyMoveBuilder

USI・USENで完全に重複している USI→日本語表記変換を共通クラスに抽出する。

**ファイル**: `src/kifu/formats/usiprettymovebuilder.h/.cpp`

```cpp
namespace UsiPrettyMoveBuilder {

// USI指し手 → 日本語表記 (KIF風)
// pieceToken: SfenPositionTracer::tokenAtFileRank() の戻り値 ("P", "+B" 等)
QString usiToPrettyMove(const QString& usi, int plyNumber,
                        int& prevToFile, int& prevToRank,
                        const QString& pieceToken);

// 駒文字 → 漢字 (P→歩, L→香, ...)
QString pieceCharToKanji(QChar usiPiece);

// 盤面トークン → 漢字 (成駒対応: +P→と, +B→馬, ...)
QString tokenToKanji(const QString& token);

// SfenPositionTracerから移動元の駒トークンを取得
QString extractPieceToken(const QString& usi, SfenPositionTracer& tracer);

} // namespace UsiPrettyMoveBuilder
```

**適用先**: `UsiToSfenConverter`, `UsenToSfenConverter` の各 `usiToPrettyMove()` を削除して委譲。

### 5.2 KifLineBuilder

USI指し手列から KifLine の表示データ（disp）を構築する共通処理。
USI・USENの `buildKifLine()` および `extractMovesWithTimes()` / `parseWithVariations()` 内の重複ループを統合する。

**ファイル**: `src/kifu/formats/kiflinebuilder.h/.cpp`

```cpp
namespace KifLineBuilder {

// USI指し手列から KifLine::disp を構築
// terminalLabel: 終局理由の日本語ラベル（空文字列なら終局なし）
void buildDispFromUsiMoves(const QStringList& usiMoves,
                           const QString& baseSfen,
                           int startPly,
                           const QString& terminalLabel,
                           KifLine& outLine);

// 開始局面エントリ＋本譜指し手＋終局理由をまとめて構築する便利関数
void buildFullDisp(const QStringList& usiMoves,
                   const QString& baseSfen,
                   const QString& terminalLabel,
                   const QString& openingComment,
                   KifLine& outLine);

} // namespace KifLineBuilder
```

**適用先**:
- `UsiToSfenConverter::buildKifLine()` → 削除して委譲
- `UsenToSfenConverter::buildKifLine()` → 削除して委譲
- `UsiToSfenConverter::extractMovesWithTimes()` → `buildFullDisp()` 呼び出しに簡略化
- `UsenToSfenConverter::extractMovesWithTimes()` → 同上
- `UsiToSfenConverter::parseWithVariations()` 本譜構築部 → 同上
- `UsenToSfenConverter::parseWithVariations()` 本譜構築部 → 同上

### 5.3 TerminalCodeResolver

各形式で異なる終局コードを日本語に変換するための統一インターフェース。

**ファイル**: `src/kifu/formats/terminalcoderesolver.h/.cpp`

```cpp
namespace TerminalCodeResolver {

// USI形式の終局コード → 日本語 (resign→投了, break→中断, ...)
QString fromUsi(const QString& code);

// USEN形式の終局コード → 日本語 (r→投了, i→反則, ...)
QString fromUsen(const QString& code);

// CSA/JKF共通の終局コード → 日本語
// KifuParseCommon::csaSpecialToJapanese() を統合
QString fromCsa(const QString& code);

} // namespace TerminalCodeResolver
```

**適用先**:
- `UsiToSfenConverter::terminalCodeToJapanese()` → 削除して委譲
- `UsenToSfenConverter::terminalCodeToJapanese()` → 削除して委譲
- `KifuParseCommon::csaSpecialToJapanese()` → `TerminalCodeResolver::fromCsa()` に移行
- `JkfMoveParser::specialToJapanese()` → `TerminalCodeResolver::fromCsa()` に統合可能（同じテーブル）

### 5.4 parsecommon 拡張

```cpp
namespace KifuParseCommon {

// 手番マーク生成（全コンバータで重複）
inline QString tebanMark(int ply) {
    return (ply % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");
}

// createOpeningDisplayItem() は既存。
// CSA, JKF, USI, USEN で手動作成しているコードをこの関数に統一。

} // namespace KifuParseCommon
```

### 5.5 extractGameInfo 共通化（KIF/KI2）

KIFとKI2の `extractGameInfo()` はほぼ同一。ヘッダパース部分を共通関数に抽出する。

```cpp
namespace KifuParseCommon {

// KIF/KI2共通: ヘッダ行から対局情報を抽出
// stopPredicate: 指し手行を検出したら true を返す（形式により判定方法が異なる）
QList<KifGameInfoItem> extractHeaderGameInfo(
    const QStringList& lines,
    std::function<bool(const QString&)> stopPredicate);

} // namespace KifuParseCommon
```

---

## 6. 移行手順と順序

### Phase 1: USI/USEN 共通ユーティリティ抽出（最優先）

**理由**: USIとUSENは最も重複が大きく、完全なコピー＆ペーストが多い。共通化の効果が最大。

1. `UsiPrettyMoveBuilder` 名前空間を新規作成
   - USI/USENの `usiToPrettyMove`, `pieceToKanji`, `tokenToKanji`, `extractPieceTokenFromUsi` を移動
   - 両コンバータを委譲に書き換え
2. `KifLineBuilder` 名前空間を新規作成
   - USI/USENの `buildKifLine()` を統合
   - `extractMovesWithTimes()`, `parseWithVariations()` 内の重複ループを `KifLineBuilder` に委譲
3. `TerminalCodeResolver` 名前空間を新規作成
   - USI/USENの `terminalCodeToJapanese()` を移行
   - `kTerminalCodes[]` テーブルを統合

**推定削減行数**: ~300行（USI: ~150行, USEN: ~150行）

### Phase 2: parsecommon 拡張と全コンバータ微修正

1. `tebanMark()` ヘルパ追加
   - 全6コンバータの手番文字列生成を `KifuParseCommon::tebanMark()` に置換
2. `createOpeningDisplayItem()` の統一使用
   - CSA, JKF, USI, USEN で手動作成している開始局面エントリを共通関数呼び出しに変更
3. `TerminalCodeResolver::fromCsa()` に `csaSpecialToJapanese()` を移行
   - CSA, JKFの終局コード変換を統合

**推定削減行数**: ~60行（各コンバータで数行ずつ）

### Phase 3: KIF/KI2 extractGameInfo 統合

1. `KifuParseCommon::extractHeaderGameInfo()` を新規作成
2. KIF/KI2の `extractGameInfo()` を共通関数＋形式別 stopPredicate に書き換え

**推定削減行数**: ~50行

### Phase 4: KI2 盤面パーサの SfenPositionTracer 統合（オプション）

KI2が独自に持つ `parseSfenBoardField()`, `parseSfenHandsField()`, `applyMoveOnBoard()` 等を `SfenPositionTracer` に統合する可能性を検討。

**注意**: KI2の盤面追跡は `convertKi2MoveToUsi()` の曖昧解決に密結合しているため、安易な統合は避ける。`SfenPositionTracer` のAPIを拡張するか、KI2固有のまま残すかは追加調査が必要。

---

## 7. テスト方針

### 7.1 既存テストの活用

各コンバータには既存の棋譜ファイルによる結合テストが存在する。共通化の前後で以下を確認:

- `parseWithVariations()` の出力（mainline.usiMoves, mainline.disp, variations）が一致すること
- `extractMovesWithTimes()` の出力が一致すること
- `convertFile()` の出力が一致すること
- `detectInitialSfenFromFile()` の出力が一致すること

### 7.2 新規ユニットテスト

共通化で新規作成するクラス/名前空間に対して個別テストを追加:

| テスト対象 | テスト内容 |
|---|---|
| `UsiPrettyMoveBuilder::usiToPrettyMove` | 通常移動、駒打ち、成り、「同」、各駒種 |
| `UsiPrettyMoveBuilder::tokenToKanji` | 全駒種（成駒含む）の変換 |
| `KifLineBuilder::buildDispFromUsiMoves` | 平手・駒落ち初期局面、終局あり/なし |
| `TerminalCodeResolver::fromUsi` | 全USI終局コード（resign, break, ...） |
| `TerminalCodeResolver::fromUsen` | 全USEN終局コード（r, i, t, p, j） |
| `TerminalCodeResolver::fromCsa` | 全CSA終局コード（%TORYO, %CHUDAN, ...） |
| `KifuParseCommon::tebanMark` | 奇数手→▲、偶数手→△ |

### 7.3 回帰テストの実行手順

```bash
# 1. 全テストの実行
cmake --build build && ctest --test-dir build --output-on-failure

# 2. 棋譜読み込みの手動テスト
#    各形式のサンプルファイルを読み込み、表示が変わらないことを確認
#    - KIF: resources/testdata/*.kif
#    - KI2: resources/testdata/*.ki2
#    - CSA: resources/testdata/*.csa
#    - JKF: resources/testdata/*.jkf
#    - USI: resources/testdata/*.usi
#    - USEN: resources/testdata/*.usen
```

---

## 8. 設計上の注意事項

### 8.1 形式固有ロジックは各Lexerに残す

字句解析（トークン化）と形式固有の変換は各Lexer/Parserの責務であり、共通化の対象外とする。
特に以下は形式の多様性が大きく、無理な共通化は複雑さを増すだけ:

- KIF: 手番号検出、BODパース、変化ブロック解析
- KI2: 曖昧指し手解決（候補絞り込み）
- CSA: 盤面初期化（PI/P1-P9行）、カンマ区切りトークン
- JKF: JSON構造のトラバース、forks処理
- USEN: base36デコード

### 8.2 静的メソッドの名前空間化

新規の共通コードは namespace（`UsiPrettyMoveBuilder`, `KifLineBuilder`, `TerminalCodeResolver`）として実装する。既存のパターン（`KifuParseCommon`, `CsaLexer`, `JkfMoveParser` が名前空間）に合わせる。

### 8.3 後方互換性

各コンバータの公開API（`convertFile`, `extractMovesWithTimes`, `parseWithVariations` 等）は変更しない。内部実装のみを共通ユーティリティへの委譲に置き換える。

### 8.4 段階的移行

一度にすべてを共通化するのではなく、Phase 1（USI/USEN）で効果を確認してから Phase 2以降に進む。各Phaseは独立してマージ可能。

---

## 9. 削減効果の見積もり

| Phase | 対象 | 推定削減行数 | 新規追加行数 | 純削減 |
|---|---|---|---|---|
| Phase 1 | USI/USEN共通化 | ~300行 | ~120行 | ~180行 |
| Phase 2 | parsecommon拡張 | ~60行 | ~15行 | ~45行 |
| Phase 3 | KIF/KI2 GameInfo統合 | ~50行 | ~30行 | ~20行 |
| Phase 4 | KI2盤面パーサ統合 | ~100行 | ~50行 | ~50行 |
| **合計** | | **~510行** | **~215行** | **~295行** |

全コンバータ合計約3,534行のうち、約8%の純削減。
重複コードの排除による保守性向上が主な効果。
