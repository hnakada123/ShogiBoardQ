# Task 14: インクルードガード統一（P3 / §2.5）

## 概要

291個の `.h` ファイルで `#ifndef` ガード（280個）と `#pragma once`（11個）が混在。一部は二重ガード。`#ifndef` に統一する。

## 対象ファイル

### `#pragma once` のみ（`#ifndef` に変更）

1. `src/common/logcategories.h`
2. `src/common/threadtypes.h`
3. `src/common/dialogutils.h`
4. `src/common/fontsizehelper.h`
5. `src/kifu/kifreader.h`
6. `src/dialogs/engineregistrationworker.h`
7. `src/dialogs/josekiioresult.h`

### `#pragma once` + `#ifndef` 二重ガード（`#pragma once` を削除）

1. `src/widgets/evaluationchartwidget.h`
2. `src/widgets/apptooltipfilter.h`
3. `src/widgets/globaltooltip.h`
4. `src/widgets/evaluationchartconfigurator.h`

## 手順

### Step 1: `#pragma once` のみのファイルを `#ifndef` ガードに変更

1. 各ファイルの `#pragma once` を削除
2. プロジェクトの既存命名規約に従って `#ifndef` / `#define` / `#endif` ガードを追加
3. 命名規約の確認: 既存ファイルのガードマクロ名パターンを調べる（例: `LOGCATEGORIES_H`, `THREADTYPES_H` 等）

### Step 2: 二重ガードから `#pragma once` を削除

1. 4つの二重ガードファイルから `#pragma once` 行を削除する
2. `#ifndef` ガードはそのまま残す

### Step 3: ビルド確認

1. `cmake --build build` でビルドが通ることを確認
2. `grep -r "#pragma once" src/` でゼロ件になることを確認

## 注意事項

- `#ifndef` ガードのマクロ名は既存のパターンに合わせること
- インクルードガードの変更は動作に影響しない（コンパイラの最適化が若干異なるのみ）
