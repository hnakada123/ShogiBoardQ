# 千日手検出機能

本ドキュメントでは、対局中に千日手（同一局面の4回出現による引き分け）をリアルタイムで検出し、終局処理を行う機能について説明する。

## 千日手ルール（日本将棋連盟 対局規則 第8条）

| 種類 | 条件 | 結果 |
|------|------|------|
| 通常の千日手 | 盤面・持駒・手番がすべて同一の局面が4回出現 | 引き分け（無勝負） |
| 連続王手の千日手 | 繰り返し手順中、片方の指し手がすべて王手 | 王手を続けた側の反則負け |

## アーキテクチャ

```
着手（人間 or エンジン）
    │
    ▼
MatchCoordinator::completeMove()   ← 人間の手は completeHumanMove()、エンジンの手は applyEngineMove() から
    │  時計の確定・対局の手順（m_record）と棋譜への追記
    ▼
checkAndHandleSennichite()         ← MatchCoordinator
    │
    ├─ SennichiteDetector::check(m_record.sfens())
    │       │
    │       ├─ positionKey() で手数を除いた局面キーを抽出
    │       ├─ 同一キーの出現回数をカウント
    │       ├─ 4回未満 → Result::None（何もしない）
    │       └─ 4回以上 → checkContinuousCheck()
    │               │
    │               ├─ 3回目〜4回目の間の各局面で王手判定
    │               ├─ 全て先手の王手 → ContinuousCheckByBlack
    │               ├─ 全て後手の王手 → ContinuousCheckByWhite
    │               └─ それ以外 → Draw
    │
    ├─ Result::Draw → endGame({Cause::Sennichite, …}, nullopt)
    ├─ Result::ContinuousCheckByBlack/ByWhite → endGame({Cause::OuteSennichite, Turn::Black / Turn::White}, 同じ側)
    └─ Result::None → 最大手数の判定へ進み、対局を続行
```

## ファイル構成

| ファイル | 役割 |
|----------|------|
| `src/game/sennichitedetector.h` | 千日手検出ユーティリティクラスの定義 |
| `src/game/sennichitedetector.cpp` | 検出ロジックの実装 |
| `src/game/matchtypes.h` | 終局原因 `MatchCause`（`Sennichite`/`OuteSennichite`） |
| `src/game/matchcoordinator.h/.cpp` | 一手の確定ごとの検出呼び出し・終局処理・終局行の追記・ダイアログの文言 |

## SennichiteDetector クラス

状態を持たない static メソッドのみのユーティリティクラス。

### 公開 API

```cpp
/// 判定結果
enum class Result {
    None,                 // 千日手ではない
    Draw,                 // 千日手（引き分け）
    ContinuousCheckByBlack,  // 先手の連続王手 → 先手の反則負け
    ContinuousCheckByWhite   // 後手の連続王手 → 後手の反則負け
};

/// sfenRecord の最新局面が4回目の出現かチェック
static Result check(const QStringList& sfenRecord);

/// SFEN文字列から手数を除いた局面キーを返す
/// "lnsgkgsnl/... b - 1" → "lnsgkgsnl/... b -"
static QString positionKey(const QString& sfen);
```

### 検出アルゴリズム

1. `sfenRecord.last()` から `positionKey()` で局面キー（手数除く）を取得
2. SFEN 履歴全体を走査し、同一キーの出現インデックスを収集
3. 4回未満 → `Result::None`
4. 4回以上 → `checkContinuousCheck()` で連続王手を判定

### 連続王手判定

3回目と4回目の出現の間（`thirdIdx+1` 〜 `fourthIdx`）の局面を調べる:

- 各局面について `ShogiBoard::setSfen()` で盤面を復元
- `EngineMoveValidator::isInCheck(board, turn)` で手番側の玉が王手されているか判定
- 「手番側の玉が王手されている」= 直前の相手の手が王手
- 先手の手がすべて王手かどうかは後手番の局面だけ、後手の手は先手番の局面だけで数える
  （王手をかけた側の手番の局面まで分母に入れると、毎手王手でも成立しない）

```
SFEN 手番フィールド:
  "b"（先手番）→ 直前に後手が指した → 先手玉が王手 = 後手の王手
  "w"（後手番）→ 直前に先手が指した → 後手玉が王手 = 先手の王手
```

一方の指し手がすべて王手だった場合、その側の反則負けとなる。テストは `tests/tst_sennichite_detector.cpp`。

## MatchCoordinator での処理

### 終局原因

`MatchCoordinator::Cause` は `MatchCause`（`src/game/matchtypes.h`）の別名。

```cpp
enum class MatchCause : int {
    Resignation    = 0,  ///< 投了
    Timeout        = 1,  ///< 時間切れ
    BreakOff       = 2,  ///< 中断
    Jishogi        = 3,  ///< 持将棋／最大手数到達
    NyugyokuWin    = 4,  ///< 入玉宣言勝ち
    IllegalMove    = 5,  ///< 反則負け（入玉宣言失敗など）
    Sennichite     = 6,  ///< 千日手（引き分け）
    OuteSennichite = 7   ///< 連続王手の千日手（反則負け）
};
```

### 関連メソッド

| メソッド | 役割 |
|----------|------|
| `checkAndHandleSennichite()` | 千日手チェックのエントリポイント。対局の手順（`MatchCoordinator::record()`）の局面列を判定する。対局者の組（人間・エンジン）によらず同じ手順を使う。終局した場合 `true` を返す |
| `endGame(info, engineLoser, notice)` | 終局処理（時計を止め、エンジンへ `gameover` を送り、終局行を追記して待機に戻す）。通常千日手は `engineLoser` なしで呼び、エンジンへ `gameover draw` を送る。連続王手の千日手は王手を続けた側を `engineLoser` にして、`gameover win`/`lose` を送る |
| `appendTerminalRecord(info)` | 終局原因から終局行（`RecordEntry::ending()`）を作って棋譜に追記する |

### 検出呼び出しポイント

人間の手（`MatchStrategyHost::completeHumanMove()`）もエンジンの手（`MatchStrategyHost::applyEngineMove()`）も、`MatchCoordinator::completeMove()` で一手を確定する。千日手の判定はここで、その手を対局の手順と棋譜に追記した後、最大手数の判定の前に1回だけ行う（`if (checkAndHandleSennichite()) return false;`）。
千日手が成立すると終局行（`▲千日手` など）を追記して以降の棋譜追記を止めるため、先に判定すると
千日手を成立させた手そのものが棋譜から抜ける。

`completeMove()` は対局が続くかを返す。`HumanVsHumanStrategy::onHumanMove()`・`HumanVsEngineStrategy::onHumanMove()`・`EngineVsEngineStrategy::onEngineMoveReady()` は `false` なら次の手番の処理（考慮時間の計測・エンジンへの思考の依頼）をしない。

### 終局時の表示

| 原因 | ダイアログメッセージ | 棋譜追記 |
|------|-------------------|----------|
| 千日手 | 「千日手が成立しました。」 | `▲千日手` / `△千日手`（終局した局面の手番側） |
| 連続王手 | 「先手の連続王手の千日手。後手の勝ちです。」 | 手番側の `反則勝ち` / `反則負け`（下記） |

終局行の印と消費時間は、終局した局面の手番側のもの（`RecordEntry::endingSide()`。棋譜を読み込んだときに付く印と同じ）。
棋譜形式（KIF・CSA・JKF）には連続王手の千日手の終局語が無いため、王手を続けた側の反則として
`TerminalType::IllegalWin` / `TerminalType::IllegalLoss` の終局行で記録する。王手の手で成立したときは手番側（勝者）の `反則勝ち`、
逃げた手で成立したときは手番側（王手を続けた側）の `反則負け` になり、CSA では `%+ILLEGAL_ACTION` /
`%-ILLEGAL_ACTION` / `%ILLEGAL_MOVE` として書き出され（`TerminalCodes::csaSpecial()`）、どの形式で保存しても勝敗が残る。
CSA通信対局（`CsaGameProgress::endRecord()`）も同じ規則で終局行を作る。

## 翻訳

| 日本語（source） | 英語 |
|-----------------|------|
| 千日手が成立しました。 | Sennichite (draw by repetition). |
| %1の連続王手の千日手。%2の勝ちです。 | Perpetual check by %1. %2 wins. |
