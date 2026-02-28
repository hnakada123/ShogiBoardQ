# Task 26: csagamecoordinator.cpp 分割（Phase 3: 大型ファイル削減）

## 目的

`src/network/csagamecoordinator.cpp`（798行）を責務ごとに分割し、600行以下にする。

## 背景

- CSA通信対局の全ライフサイクルが1ファイルに集中
- 接続管理、メッセージ解析、対局進行、切断処理が混在
- future-improvement テーマB 優先候補 #1
- 包括的改善分析 §2.4, §11.2

## 対象ファイル

- `src/network/csagamecoordinator.cpp` (798行)
- `src/network/csagamecoordinator.h`

## 事前調査

### Step 1: メソッド一覧と責務分類

```bash
# ヘッダのメソッド宣言
cat src/network/csagamecoordinator.h

# 実装ファイルのメソッド行数
rg -n "^void |^bool |^int |^QString |^QList|^std::|^CsaGame" src/network/csagamecoordinator.cpp

# 使用箇所
rg "CsaGameCoordinator" src --type cpp --type-add 'header:*.h' --type header -l
```

### Step 2: シグナル/スロット接続の確認

```bash
rg "csaGameCoordinator\|CsaGameCoordinator" src --type cpp -C 2
```

### Step 3: 責務の3分類

1. **接続/切断管理**: CSAサーバーへの接続、認証、切断処理
2. **メッセージ処理**: CSAプロトコルメッセージの受信・解析・応答
3. **対局進行**: 対局状態管理、手の送受信、時間管理

## 実装手順

### Step 4: 抽出先クラスの設計

責務分析の結果に基づき、以下のようなクラスを新規作成（名前は調査結果で調整）:

- `src/network/csasessionmanager.h/.cpp` — 接続/認証/切断ライフサイクル
- `src/network/csamessagehandler.h/.cpp` — CSAメッセージの受信・解析・ディスパッチ

または:

- `src/network/csagameprogresshandler.h/.cpp` — 対局進行（手の送受信、時間管理）

### Step 5: 依存注入パターンの選択

既存パターン（Deps/Hooks/Refs）に従い、新クラスの依存を設計:

```cpp
struct Deps {
    CsaClient* client = nullptr;
    ShogiGameController* gc = nullptr;
    // ...
};
```

### Step 6: メソッド移動と配線更新

1. 対象メソッドを新クラスに移動
2. 元ファイルには委譲呼び出しを配置
3. シグナル/スロット接続を更新
4. `CsaGameWiring` の接続先を必要に応じて更新

### Step 7: CMakeLists.txt 更新

```bash
scripts/update-sources.sh
```

### Step 8: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen build/tests/tst_structural_kpi
QT_QPA_PLATFORM=offscreen build/tests/tst_layer_dependencies
```

### Step 9: 構造KPI更新

`tests/tst_structural_kpi.cpp` の `knownLargeFiles()` を更新。
`csagamecoordinator.cpp` の行数減少を反映。

## 完了条件

- [ ] `csagamecoordinator.cpp` が798行から600行以下に削減
- [ ] 新規クラスが1-2件作成されている
- [ ] シグナル/スロット接続が正しく維持されている
- [ ] 全テスト通過
- [ ] 構造KPIが更新されている
- [ ] `tst_layer_dependencies` が通過

## KPI変化目標

- `csagamecoordinator.cpp`: 798 → 600以下
- 600行超ファイル数: -1

## 注意事項

- `connect()` でラムダを使用しない
- CSA通信テスト（`tst_csaprotocol`）が通過することを確認
- `CsaGameWiring` の接続先変更が必要な場合は慎重に更新
- ネットワーク関連のエラーハンドリングを維持する
- 既存の Deps/Hooks パターンに従う
