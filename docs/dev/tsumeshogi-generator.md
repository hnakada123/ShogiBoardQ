# 詰将棋局面自動生成機能

## 概要

ランダムに詰将棋の候補局面を生成し、USIエンジンで詰み探索を行い、指定手数の詰みが存在する局面を自動で発見する機能。発見した局面は不要駒トリミング処理により、最小限の駒で構成された状態に最適化される。

## アーキテクチャ

### クラス構成

| クラス | ファイル | 責務 |
|---|---|---|
| `TsumeshogiGeneratorDialog` | `src/dialogs/tsumeshogigeneratordialog.h/.cpp` | UI（パラメータ設定・進捗表示・結果テーブル） |
| `TsumeshogiGenerator` | `src/analysis/tsumeshogigenerator.h/.cpp` | オーケストレータ（生成→検証→トリミングの全体制御） |
| `TsumeshogiPositionGenerator` | `src/analysis/tsumeshogipositiongenerator.h/.cpp` | ランダム局面生成（SFEN文字列出力） |

```
TsumeshogiGeneratorDialog
  │
  ├─ TsumeshogiGenerator          ← オーケストレータ
  │    ├─ TsumeshogiPositionGenerator  ← ランダム局面生成
  │    └─ Usi                          ← エンジン通信（詰探索）
  │
  signals: positionFound / progressUpdated / finished / errorOccurred
```

### シグナルフロー

```
Dialog ──start()──▶ Generator ──generateAndSendNext()──▶ Usi
                                                           │
    onPositionFound() ◀── positionFound ◀── processResult() ◀── onCheckmateSolved()
    onProgressUpdated() ◀── progressUpdated
    onGeneratorFinished() ◀── finished
```

## 処理フロー

### 全体フロー

```
[開始]
  │
  ▼
ランダム局面生成 (TsumeshogiPositionGenerator::generate)
  │
  ▼
エンジンに送信 (go mate <timeout>)
  │
  ▼
エンジン応答
  ├─ checkmate <N手PV>
  │   ├─ N == 目標手数 → トリミングフェーズへ
  │   └─ N != 目標手数 → 次の局面へ
  ├─ checkmate nomate → 次の局面へ
  ├─ checkmate timeout → 次の局面へ（`checkmateUnknown` として通知される）
  ├─ エンジン無応答（安全タイマー） → stop を送信して応答を待つ
  │   ├─ 応答あり → 結果として扱わず破棄し、次の局面へ（遅延応答の誤採択防止）
  │   └─ 3秒応答なし → エンジン固着とみなし生成終了（errorOccurred + finished）
  └─ エンジンエラー（プロセス異常終了など） → 生成終了（errorOccurred + finished）
```

### 状態機械 (Phase)

`TsumeshogiGenerator` は3つの状態（Phase）で動作する。

| Phase | 説明 | 遷移先 |
|---|---|---|
| `Idle` | 停止中。`start()` で Searching へ | → Searching |

`start()` のエンジン初期化中（usiok/readyok 待ち）はイベントループが回るため、この間の `stop()` は
`m_stopRequestedDuringStart` に記録し、初期化の待機を中断させて `start()` 復帰後に後始末する。
| `Searching` | ランダム生成→エンジン検証ループ中 | → Trimming（詰み発見時）/ → Idle（停止・上限到達） |
| `Trimming` | 不要駒除去検証中 | → Searching（トリミング完了後）/ → Idle（停止時） |

```
     start()         N手詰発見
Idle ──────▶ Searching ──────▶ Trimming
  ▲            │    ▲              │
  │  stop()    │    │  完了         │  stop()
  └────────────┘    └──────────────┘
  └────────────────────────────────────┘
```

## ランダム局面生成 (TsumeshogiPositionGenerator)

### 生成パラメータ

| パラメータ | デフォルト | 説明 |
|---|---|---|
| `maxAttackPieces` | 4 | 攻め駒上限（盤上+持駒合計） |
| `maxDefendPieces` | 1 | 守り駒上限（玉以外） |
| `attackRange` | 3 | 玉中心の攻め駒配置範囲（マス） |
| `addRemainingToDefenderHand` | true | 未使用駒を受方持駒に入れる |

### 生成アルゴリズム

1. **後手の玉をランダム配置** — 9x9盤面の任意のマス
2. **攻め駒を配置** — 1〜maxAttackPieces枚
   - 70%の確率で盤上（玉周辺attackRangeマス以内）、30%の確率で持駒
   - 30%の確率で成駒として配置
   - 段制約に違反する場合は自動で成駒に変換（金は不可なので再試行）
   - 同じ筋に同じ側の不成歩がある場合は二歩になるため別のマスを再試行
3. **守り駒を配置** — 0〜maxDefendPieces枚（玉以外の後手駒、玉周辺2マス以内、二歩回避は攻め駒と同様）
4. **王手検証** — 初期局面で後手玉に王手がかかっていないことを `FastMoveValidator` で検証
5. **リトライ** — 王手がかかっている場合は最大100回再生成

### 詰将棋の慣例

- 攻方（先手）の玉は配置しない
- 未使用駒は全て受方（後手）持駒に追加（40駒保存則）
- 持駒に成駒は置けない

### 盤面内部表現

駒の格納に `QChar` の Unicode Private Use Area (U+E000〜) を利用:
- 生駒: そのままのSFEN文字（`P`, `p`, `k` 等）
- 成駒: `0xE000 + 元文字のUnicodeコードポイント`（例: `+P` → `U+E050`）

## エンジン検証 (TsumeshogiGenerator)

### USIプロトコル

エンジンとの通信は USI の `go mate` コマンドを使用する。

```
→ position sfen <SFEN文字列>
→ go mate <timeout_ms>
← checkmate <PV>        … 詰みあり（PVは指し手列）
← checkmate nomate      … 詰みなし
← checkmate timeout     … 時間切れ
← checkmate notimplemented … エンジン非対応
```

### フィルタリング

エンジンが返すPV（Principal Variation）の手数が「ちょうどN手」のみを採択する。N手以下の短手数やN手超の長手数は棄却される。

この判定はエンジンが**最短手順**を返すことを前提にしている。最短性が保証されない設定では、実際にはより短手数で詰む局面が「N手詰」として採択されうる。
KomoringHeights の場合は `PostSearchLevel` を `MinLength`（最短性を保証）にする必要がある（`None` / `UpperBound` は不可）。ダイアログにもこの旨の注意書きを表示している。

発見局面は SFEN 単位で重複排除する（トリミングにより異なる候補が同じ最小局面に収束することがあるため）。重複した局面は発見数に数えない。

### タイマー

| タイマー | 用途 | 間隔 |
|---|---|---|
| `m_safetyTimer` | エンジン無応答ガード | `timeoutMs + 5000ms`（シングルショット） |
| `m_safetyTimer`（stop 応答待ち） | 安全タイマー発火後に送った stop への応答待ち | 3000ms（シングルショット） |
| `m_progressTimer` | UI進捗更新 | 500ms（リピート） |

## 不要駒トリミング

### 目的

ランダム生成された局面には「飾り駒」（取り除いても詰み手順に影響しない不要な駒）が含まれることがある。トリミング処理により、詰み手数を変えずに除去可能な駒を反復的に特定・除去し、最小限の駒で構成された局面を出力する。

### アルゴリズム

```
N手詰の局面発見（SFEN, PV）
  │
  ▼
startTrimmingPhase(sfen, pv)
  │
  ▼
除去候補リストを構築（enumerateRemovablePieces）
  │
  ▼
候補[0]を除去したSFENを生成（removePieceFromSfen）
  │
  ▼
エンジンに送信（go mate）
  │
  ▼
エンジン応答:
  ├─ ちょうどN手詰 → 除去確定
  │   → 除去後のSFENをベースに候補リストを再構築
  │   → 先頭から再開（新たに除去可能になった駒があるかもしれない）
  │
  └─ それ以外（不詰・手数変化・タイムアウト） → 除去却下
      → 次の候補へ（インデックスを進める）
          │
          ├─ 次の候補あり → エンジンに送信
          └─ 全候補試行済み → トリミング完了
              → positionFound シグナルを emit
              → Searching フェーズに復帰
```

### 除去候補の列挙ルール

| 対象 | 除去可否 | 理由 |
|---|---|---|
| 盤上の攻方駒（玉以外） | 可 | 攻め駒が不要な可能性 |
| 盤上の守方駒（玉以外） | 可 | 守り駒が不要な可能性 |
| 攻方（先手）持駒 | 可 | 持駒が不要な可能性 |
| 守方（後手）持駒 | **不可** | 守方の防御資源を減らすと詰みやすくなり不正 |
| 玉（攻方・守方とも） | **不可** | 玉は除去不可 |

### 40駒保存則

将棋の駒は全部で40枚であり、詰将棋でも盤上+持駒の合計が40枚を保つ慣例がある。除去した駒は消滅させず、守方（後手）持駒に移動する:

- **盤上の駒を除去**: 生駒に戻して後手持駒に追加（成駒は持駒にできないため）
- **攻方持駒を除去**: そのまま後手持駒に移動

### 候補リスト再構築

駒を1つ除去するたびに候補リストを再構築し、先頭から再試行する。これは、ある駒の除去によって別の駒も不要になるケースに対応するためである。

例: 攻方の銀を除去 → 銀で合駒する変化が消滅 → 攻方の桂（銀合の変化でのみ必要だった）も不要になる

### 除去候補の事前検証

除去後の局面で開始時に守方玉へ王手がかかる場合（例: 飛車と玉の間にあった守り駒を除去）、詰将棋として不正なのでエンジンに送らず候補から外す。判定には `TsumeshogiPositionGenerator::isDefenderKingInCheck()` を使う。

### 中断時の扱い

トリミング中に `stop()`・エンジンエラー・エンジン固着で終了する場合、その時点のベース局面（`m_trimBaseSfen`）は検証済みのN手詰なので、`positionFound` として出力してから終了する。

## SFEN解析・再構築

トリミング処理のために、SFEN文字列を構造化データ (`ParsedSfen`) に変換し、駒の追加・除去を行った後、再びSFEN文字列に復元する。

### ParsedSfen 構造体

```cpp
struct ParsedSfen {
    QString cells[9][9];     // [rank][file] = "" or "P"/"+R"/"p" etc.
    int senteHand[7] = {};   // P,L,N,S,G,B,R の先手持駒数
    int goteHand[7] = {};    // P,L,N,S,G,B,R の後手持駒数
};
```

### 駒種インデックス

| インデックス | 0 | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|---|
| 駒種 | P(歩) | L(香) | N(桂) | S(銀) | G(金) | B(角) | R(飛) |

### SFEN解析パターン

`SfenPositionTracer::setFromSfen()` のパターンを踏襲:
1. スペースで分割 → 盤面部/手番部/持駒部/手数部
2. 盤面部を `/` で9段に分割 → 各段を文字単位でパース
   - 数字 = 空きマス数
   - `+` = 次の文字が成駒
   - 英字 = 駒（大文字=先手、小文字=後手）
3. 持駒部を文字単位でパース
   - 数字 = 枚数の蓄積（複数桁対応）
   - 英字 = 駒種（大文字=先手、小文字=後手）
   - `-` = 持駒なし

### SFEN再構築

持駒の出力順は SFEN 慣例に従い、価値の高い順: R, B, G, S, N, L, P

## ダイアログ (TsumeshogiGeneratorDialog)

### UI構成

```
┌─────────────────────────────────────────┐
│ エンジン設定                              │
│  エンジン: [▼ コンボボックス]  [設定...]    │
│                                           │
│ 生成設定                                  │
│  目標手数:     [  3 ] 手詰                │
│  攻め駒上限:   [  4 ] 枚                  │
│  守り駒上限:   [  1 ] 枚                  │
│  配置範囲:     [  3 ] マス（玉中心）       │
│  探索時間/局面: [  5 ] 秒                  │
│  生成上限:     [ 10 ]                     │
│                                           │
│  [開始]  [停止]                            │
│                                           │
│ 探索済み: 150 局面 / 発見: 3 局面          │
│ 経過時間: 00:01:23                        │
│ 状態: トリミング中（候補 3/9）             │
│                                           │
│ 結果一覧                                  │
│ ┌───┬──────────────────┬────┐             │
│ │ # │ SFEN              │手数│             │
│ ├───┼──────────────────┼────┤             │
│ │ 1 │ 9/9/9/...         │  3 │             │
│ │ 2 │ 9/9/9/...         │  3 │             │
│ └───┴──────────────────┴────┘             │
│                                           │
│ [A-] [A+] [既定値に戻す]  [ ]手順も出力 [ファイル保存] [選択コピー] [全コピー] [閉じる] │
└─────────────────────────────────────────┘
```

### 設定の永続化

SettingsService を通じて以下の設定が保存・復元される:
- ダイアログサイズ
- フォントサイズ
- 選択エンジンインデックス
- 各生成パラメータ（目標手数、攻め駒上限、守り駒上限、配置範囲、探索時間、生成上限）
- 手順出力オプション（ファイル保存・コピー時に `moves ...` を付加するか）

### 状態表示と手順出力

- 状態ラベルは `searchPhaseStarted`（探索中）と `trimmingProgress`（トリミング中 候補 i/n）で更新し、`finished` で「待機中」に戻す。トリミング中は「探索済み」が進まないため、何をしているかを示す目的。
- 「手順も出力」を有効にすると、ファイル保存・コピーの各行が `<SFEN> moves <USI手順>` になる。無効時は SFEN のみで、SFEN集ダイアログ等の1行1SFEN形式と互換。
- ジェネレータ（`TsumeshogiGenerator`）はダイアログの子として1つだけ生成し、開始のたびに再利用する。`start()` が状態を初期化し、`Usi` は開始ごとに生成・終了時に `deleteLater` で破棄する。

## 設定パラメータ一覧

### TsumeshogiGenerator::Settings

| フィールド | 型 | デフォルト | 説明 |
|---|---|---|---|
| `enginePath` | QString | — | エンジン実行ファイルパス |
| `engineName` | QString | — | エンジン名 |
| `targetMoves` | int | 3 | 目標手数（奇数: 1,3,5,...） |
| `timeoutMs` | int | 5000 | 1局面あたりのエンジン探索時間(ms) |
| `maxPositionsToFind` | int | 10 | 見つける局面数の上限（0=無制限） |
| `posGenSettings` | Settings | — | 局面生成パラメータ（下記） |

### TsumeshogiPositionGenerator::Settings

| フィールド | 型 | デフォルト | 説明 |
|---|---|---|---|
| `maxAttackPieces` | int | 4 | 攻め駒上限（盤上+持駒合計） |
| `maxDefendPieces` | int | 1 | 守り駒上限（玉以外） |
| `attackRange` | int | 3 | 玉中心の攻め駒配置範囲 |
| `addRemainingToDefenderHand` | bool | true | 未使用駒を受方持駒に入れる |

## エンジン応答スロットの分岐

全エンジン応答スロットは `m_phase` を参照して処理を分岐する:

| スロット | Searching時 | Trimming時 |
|---|---|---|
| `onCheckmateSolved` | N手→トリミング開始 / 他→次局面 | N手→除去確定→再構築 / 他→次候補 |
| `onCheckmateNoMate` | 次の局面へ | 除去却下→次の候補へ |
| `onCheckmateUnknown`（timeout 含む） | 次の局面へ | 除去却下→次の候補へ |
| `onSafetyTimeout`（1回目） | stop 送信→応答待ち | stop 送信→応答待ち |
| `onSafetyTimeout`（応答待ち中） | エラー終了（エンジン固着） | エラー終了（ベース局面は出力） |
| `onCheckmateNotImplemented` | エラー終了 | エラー終了 |
| `onEngineError` | エラー終了 | エラー終了（ベース局面は出力） |

「次の局面へ / 除去却下→次の候補へ」は `advanceAfterFailure()` に共通化している。応答待ち中（`m_awaitingStopResponse`）に届いた応答は `consumeStaleResponse()` で結果として扱わず破棄し、同じく `advanceAfterFailure()` に進む。

## ファイル一覧

| ファイル | 行数目安 | 概要 |
|---|---|---|
| `src/analysis/tsumeshogigenerator.h` | ~130 | オーケストレータ定義 |
| `src/analysis/tsumeshogigenerator.cpp` | ~510 | オーケストレータ実装（検索+トリミング） |
| `src/analysis/tsumeshogipositiongenerator.h` | ~70 | 局面生成クラス定義 |
| `src/analysis/tsumeshogipositiongenerator.cpp` | ~360 | 局面生成クラス実装 |
| `src/dialogs/tsumeshogigeneratordialog.h` | ~110 | ダイアログ定義 |
| `src/dialogs/tsumeshogigeneratordialog.cpp` | ~630 | ダイアログ実装 |
