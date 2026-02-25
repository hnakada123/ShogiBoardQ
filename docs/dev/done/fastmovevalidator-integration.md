# FastMoveValidator 組み込み・移行ガイド

## 状態

FastMoveValidator への移行は完了。
現在の本番コードは `FastMoveValidator` を直接利用している。

## 現在の利用箇所

- `src/game/shogigamecontroller.cpp`
- `src/game/sennichitedetector.cpp`
- `src/ui/controllers/jishogiscoredialogcontroller.cpp`
- `src/ui/controllers/nyugyokudeclarationhandler.cpp`

## 削除済み項目

- `src/core/movevalidator.h`
- `src/core/movevalidator.cpp`
- `tests/tst_movevalidator.cpp`
- `tests/tst_movevalidator_equivalence.cpp`
- `tests/tst_movevalidator_perf.cpp`

## 現在の検証方針

- 正常系・禁手系・成り関連は `tests/tst_fastmovevalidator.cpp` で検証
- 連携挙動は `tests/tst_integration.cpp` で検証
- コアデータ構造との整合は `tests/tst_coredatastructures.cpp` で検証

## 補足

旧 MoveValidator 前提の説明は履歴として残っている文書があるため、現行仕様参照時は本ドキュメントと `docs/dev/developer-guide.md` の FastMoveValidator 記述を優先すること。
