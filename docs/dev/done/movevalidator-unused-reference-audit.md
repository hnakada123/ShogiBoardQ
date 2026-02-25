# MoveValidator 参照削除監査（移行完了版）

更新日: 2026-02-17

## 結論

MoveValidator から FastMoveValidator への移行完了に伴い、MoveValidator 本体と MoveValidator 専用テストは削除済み。

## 削除済みコード

- `src/core/movevalidator.h`
- `src/core/movevalidator.cpp`
- `src/core/movevalidatorselector.h`
- `tests/tst_movevalidator.cpp`
- `tests/tst_movevalidator_equivalence.cpp`
- `tests/tst_movevalidator_perf.cpp`

## 現在の本番参照（FastMoveValidator）

- `src/game/shogigamecontroller.cpp`
- `src/game/sennichitedetector.cpp`
- `src/ui/controllers/jishogiscoredialogcontroller.cpp`
- `src/ui/controllers/nyugyokudeclarationhandler.cpp`

## 継続テスト

- `./build/tests/tst_fastmovevalidator -platform offscreen`
- `./build/tests/tst_integration -platform offscreen`
- `./build/tests/tst_coredatastructures -platform offscreen`

## 影響範囲

- 本番機能: MoveValidator 依存は除去済み
- テスト: 比較テストは廃止し、FastMoveValidator 単独検証へ移行
- ドキュメント: 旧 MoveValidator 前提の説明は順次 FastMoveValidator 前提へ更新
