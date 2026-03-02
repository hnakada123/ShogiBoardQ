# Task 01: 座標系定数の統一（P0 / §2.1）

## 概要

駒台ファイル座標が2系統存在し、値が異なる潜在バグ。共有定数ヘッダを作成して統一する。

## 現状

| 場所 | 先手駒台 | 後手駒台 |
|---|---|---|
| `src/board/boardinteractioncontroller.h` | `kBlackStandFile = 10` | `kWhiteStandFile = 11` |
| `src/core/fmvconverter.cpp` | `kBlackHandFile = 9` | `kWhiteHandFile = 10` |

さらに以下のファイルで `10` / `11` がマジックナンバーとしてハードコード:
- `piecemoverules.cpp`
- `shogiboard.cpp`
- `shogiboard_edit.cpp`
- `csamoveprogresshandler.cpp`
- `csamoveconverter.cpp`
- `shogigamecontroller.cpp`

## 手順

### Step 1: 座標体系の調査

1. `src/board/boardinteractioncontroller.h` の `kBlackStandFile = 10`, `kWhiteStandFile = 11` の使用箇所を全て洗い出す
2. `src/core/fmvconverter.cpp` の `kBlackHandFile = 9`, `kWhiteHandFile = 10` の使用箇所を全て洗い出す
3. 両体系の値が異なる理由（FMV層のオフセット等）を解明し、意図的な設計かバグかを判定する
4. マジックナンバー `10`, `11` を使っている全ファイルをリストアップし、どちらの体系に属するか分類する

### Step 2: 共有定数ヘッダの作成

1. `src/common/boardconstants.h` を作成する（`#ifndef` ガード使用）
2. 盤面サイズ定数を定義:
   ```cpp
   namespace BoardConstants {
   constexpr int kBoardSize = 9;
   constexpr int kNumBoardSquares = 81;
   constexpr int kBlackStandFile = 10;
   constexpr int kWhiteStandFile = 11;
   }
   ```
3. FMV層が異なる値を使う必然性がある場合は、FMV層専用の変換関数またはオフセット定数を併せて定義し、コメントで理由を明記する
4. `CMakeLists.txt` の `SRC_COMMON` にファイルを追加する

### Step 3: 全ファイルの定数置換

1. `boardinteractioncontroller.h` のローカル定数を削除し、`boardconstants.h` をインクルード
2. `fmvconverter.cpp` のローカル定数を削除し、共有定数を使用（変換が必要な場合はオフセット適用）
3. マジックナンバー `10`, `11` を使用している全ファイルで名前付き定数に置換
4. `src/engine/usi.h` と `src/engine/usiprotocolhandler.h` の `SENTE_HAND_FILE = 10`, `GOTE_HAND_FILE = 11`, `BOARD_SIZE = 9`, `NUM_BOARD_SQUARES = 81` も共有定数に置換

### Step 4: ビルド・テスト

1. `cmake --build build` でビルドが通ることを確認
2. テストを実行して全パスを確認
3. 座標値が変わるファイルについては、既存の動作に影響がないことをダブルチェック

## 注意事項

- FMV層（`fmvconverter.cpp`）の値 `9/10` が意図的に異なる場合、無理に統一せず変換ロジックを入れること
- 既存テスト56件が全パスすることを必ず確認
