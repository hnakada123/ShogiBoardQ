# MoveValidator 未使用参照監査（FastMoveValidator 全面移行後）

更新日: 2026-02-17

## 1. 削除前の参照一覧

`rg -n "\\bMoveValidator\\b|movevalidator\\.h|movevalidatorselector\\.h" src tests docs CMakeLists.txt` で調査。

### 1.1 本番コード（`src/`）
- `src/core/movevalidator.h`
- `src/core/movevalidator.cpp`
- `src/core/movevalidatorselector.h`（調査時点）

補足:
- アプリ本体の実利用箇所（`ShogiGameController`, `SennichiteDetector`, `JishogiScoreDialogController`, `NyugyokuDeclarationHandler`）は `FastMoveValidator` へ移行済み。

### 1.2 テストコード（`tests/`）
- `tests/tst_movevalidator.cpp`
- `tests/tst_movevalidator_equivalence.cpp`
- `tests/tst_movevalidator_perf.cpp`
- `tests/tst_integration.cpp`

補足:
- 同値比較・回帰検知・性能比較のため、`MoveValidator` を意図的に参照している。

### 1.3 ドキュメント（`docs/`）
- `docs/dev/movevalidator-usage.md`
- `docs/dev/movevalidator-algorithm.md`
- `docs/dev/developer-guide.md`
- ほか移行計画/テストサマリ文書

## 2. 安全に削除可能と判断したコード

## 削除済み
- `src/core/movevalidatorselector.h`

理由:
- `rg -n "movevalidatorselector\\.h|ActiveMoveValidator|kUseFastMoveValidator" src tests CMakeLists.txt` の結果、参照 0 件（自己ファイル定義のみ）。
- 依存先がなく、削除してもビルド/実行経路に影響しない。

## 3. まだ削除しないコード

- `src/core/movevalidator.h`
- `src/core/movevalidator.cpp`

理由:
- `tests/` が比較対象として依存しており、現時点で削除すると同値性検証系テストが失われる。
- 段階的移行での安全性（退避/ロールバック可能性）を維持するため、当面は残置が妥当。

## 4. 影響範囲

- 直接影響:
  - `movevalidatorselector.h` を include していた箇所は存在しないため、コンパイル依存への実影響はなし。
- 間接影響:
  - 「機能フラグで即時切替」用の未使用抽象は削除された。
  - ただし現在の本番経路は `FastMoveValidator` 直利用のため、動作面の変更はない。

## 5. 検証結果

- Build: `cmake --build build -j4` 成功
- Test:
  - `./build/tests/tst_movevalidator -platform offscreen` : PASS (9/9)
  - `./build/tests/tst_fastmovevalidator -platform offscreen` : PASS (5/5)
  - `./build/tests/tst_movevalidator_equivalence -platform offscreen` : PASS (23/23)
  - `./build/tests/tst_integration -platform offscreen` : PASS (7/7)
