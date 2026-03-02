# Task 13: TsumeResult 重複定義解消 + USI 共有定数（P3 / §2.6）

## 概要

`Usi` と `UsiProtocolHandler` の両方に同一の `TsumeResult` 構造体が定義されている。また `SENTE_HAND_FILE`, `GOTE_HAND_FILE`, `BOARD_SIZE`, `NUM_BOARD_SQUARES` の定数も重複している。

## 現状

### TsumeResult の重複

`src/engine/usi.h` (lines 57-65):
```cpp
struct TsumeResult {
    enum Kind { Solved, NoMate, NotImplemented, Unknown } kind = Unknown;
    QStringList pvMoves;
};
```

`src/engine/usiprotocolhandler.h` (lines 48-56): 同一の定義。

### 定数の重複

両ファイルで以下が重複:
- `SENTE_HAND_FILE = 10`
- `GOTE_HAND_FILE = 11`
- `BOARD_SIZE = 9`
- `NUM_BOARD_SQUARES = 81`

## 手順

### Step 1: 依存関係の調査

1. `Usi::TsumeResult` と `UsiProtocolHandler::TsumeResult` の使用箇所を全てリストアップする
2. どちらの名前空間の `TsumeResult` がより広く使われているか確認する
3. `Usi` と `UsiProtocolHandler` の include 関係を確認する

### Step 2: TsumeResult の統一

**方針A: `UsiProtocolHandler::TsumeResult` を正とし、`Usi` は type alias**
```cpp
// usi.h
using TsumeResult = UsiProtocolHandler::TsumeResult;
```

**方針B: 共通ヘッダに移動（Task 01 の `boardconstants.h` と同時に行う場合）**
- `src/engine/usitypes.h` 等の共通ヘッダを作成

**方針C: `Usi::TsumeResult` を正とし、`UsiProtocolHandler` で using**
- `Usi` がパブリック API なのでこちらが自然

### Step 3: 定数の統一

1. Task 01 で `boardconstants.h` を作成済みの場合は、そこから参照する
2. Task 01 が未実施の場合は、`src/engine/` 内で一方を削除し type alias / include で参照する
3. `SENTE_HAND_FILE` / `GOTE_HAND_FILE` が `boardconstants.h` の `kBlackStandFile` / `kWhiteStandFile` と同じ値であることを確認する

### Step 4: ビルド・テスト

1. `cmake --build build` でビルドが通ることを確認
2. テストを実行して全パスを確認

## 注意事項

- `Usi::TsumeResult` と `UsiProtocolHandler::TsumeResult` は構造的に同一だが、名前空間が異なるため型としては別物。既存コードでどちらが使われているか確認が重要
- Task 01（座標定数統一）との連携を意識すること
