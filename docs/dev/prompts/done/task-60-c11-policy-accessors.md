# Task 60: ポリシー判定/時間取得アクセサの移譲（C11）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C11 に対応。
推奨実装順序の第5段階（表示・I/O・判定系）。

## 背景

対局中判定・手番判定・時間取得などのアクセサが MainWindow に残っており、MatchCoordinatorWiring 等から `std::bind` で MainWindow メンバ関数が参照されている。

## 目的

判定・取得系を `MatchRuntimeQueryService`（新規）に集約し、MainWindow から状態参照を排除する。

## 対象メソッド

| メソッド | 責務 |
|---|---|
| `isHumanTurnNow` | 人間手番判定 |
| `isGameActivelyInProgress` | 対局中判定 |
| `isHvH` | 人間同士対局判定 |
| `isHumanSide` | 人間側判定 |
| `getRemainingMsFor` | 残り時間取得 |
| `getIncrementMsFor` | 加算時間取得 |
| `getByoyomiMs` | 秒読み時間取得 |
| `sfenRecord` | SFEN棋譜取得（mutable） |
| `sfenRecord() const` | SFEN棋譜取得（const） |

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- 既存拡張: `src/app/playmodepolicyservice.h` / `.cpp`
- 新規: `src/app/matchruntimequeryservice.h` / `.cpp`（または既存サービスに統合）
- `CMakeLists.txt`

## 実装手順

### Phase 1: MatchRuntimeQueryService 作成
1. `MatchRuntimeQueryService` を新規作成。
2. `ShogiGameController*`, `ShogiClock*`, PlayMode情報 等を Deps で受け取る。
3. `isHumanTurnNow`, `isGameActivelyInProgress`, `isHvH`, `isHumanSide` を移動。

### Phase 2: 時間取得の移動
1. `getRemainingMsFor`, `getIncrementMsFor`, `getByoyomiMs` を移動。
2. `ShogiClock` への直接参照を QueryService に集約。

### Phase 3: SFEN アクセサの移動
1. `sfenRecord` を QueryService または GameRecordModel 側へ移動。

### Phase 4: 呼び出し元の更新
1. `MatchCoordinatorWiring` へは `MatchRuntimeQueryService` を注入。
2. MainWindow メンバ関数 bind を QueryService 経由の呼び出しに置換。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う（lambda connect 禁止）
- 手番判定・対局中判定のタイミングを変更しない

## 受け入れ条件

- 判定・取得系が QueryService に集約されている
- MainWindow から対象メソッドが削除されている
- MatchCoordinatorWiring の bind が更新されている
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- 手番判定（人間/エンジン）
- 対局中判定
- 持ち時間/加算/秒読み取得
- HvH/HvE/EvE判定

## 出力

- 変更ファイル一覧
- 移動したメソッドの一覧
- MainWindow の行数変化
- 回帰リスク
- 残課題
