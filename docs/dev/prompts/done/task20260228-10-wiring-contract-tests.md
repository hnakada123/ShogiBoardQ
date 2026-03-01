# Task 10: wiring契約テスト水平展開

## 概要

既存の `tst_wiring_contracts.cpp`（MatchCoordinatorWiring のみ）を他の主要 Wiring クラスに水平展開し、配線変更による回帰を検知する。

## 前提条件

- Task 04（異常系テスト追加）完了推奨（テストパターンを踏襲する）

## 現状

- `tests/tst_wiring_contracts.cpp` — MatchCoordinatorWiring の結線契約テストのみ
- 他の Wiring クラスにはテストがない:
  - `src/ui/wiring/uiactionswiring.h/.cpp`
  - `src/ui/wiring/csagamewiring.h/.cpp`
  - `src/ui/wiring/analysistabwiring.h/.cpp`
  - `src/ui/wiring/playerinfowiring.h/.cpp`
  - `src/ui/wiring/considerationwiring.h/.cpp`
  - `src/ui/wiring/dialoglaunchwiring.h/.cpp`
  - `src/ui/wiring/branchnavigationwiring.h/.cpp`
  - `src/ui/wiring/dialogcoordinatorwiring.h/.cpp`
  - `src/ui/wiring/matchcoordinatorwiring.h/.cpp`（テスト済み）

## 実施内容

### Step 1: 優先度判定

各 Wiring クラスのシグナル/スロット接続数と複雑度を調査し、テスト優先度を決定:

```bash
grep -c "connect(" src/ui/wiring/*.cpp | sort -t: -k2 -rn
```

接続数が多い Wiring から優先的にテストを追加する。

### Step 2: テスト追加（上位4クラス）

以下の各 Wiring に対して契約テストを作成:

1. **CsaGameWiring** — CSA通信対局の配線（通信関連で重要度高）
2. **PlayerInfoWiring** — 対局者情報の配線（Task 58 で多数追加されたメソッド）
3. **AnalysisTabWiring** — 解析タブの配線
4. **ConsiderationWiring** — 検討モードの配線

各テストは以下を検証:
- Deps 構造体の全フィールドが使用されていること
- `connect()` 呼び出しで接続されるシグナル/スロットが存在すること
- 配線後に主要シグナルを emit して期待通りのスロットが呼ばれること

### Step 3: テストファイル作成

- `tests/tst_wiring_csagame.cpp`
- `tests/tst_wiring_playerinfo.cpp`
- `tests/tst_wiring_analysistab.cpp`
- `tests/tst_wiring_consideration.cpp`

### Step 4: CMake登録

`tests/CMakeLists.txt` に追加して全件パスを確認。

## 完了条件

- 4つの Wiring クラスに契約テストが追加されている
- app/wiring系テスト +8 ケース以上
- 全テスト通過
- ビルド成功

## 注意事項

- 既存の `tst_wiring_contracts.cpp` のパターン（スタブクラス、ソース解析型）を踏襲する
- テストがヘッドレス環境（`QT_QPA_PLATFORM=offscreen`）で実行可能であること
- Wiring クラスが `MainWindow` に強く依存する場合は、ソース解析型テスト（connect 文の存在確認）でもよい
