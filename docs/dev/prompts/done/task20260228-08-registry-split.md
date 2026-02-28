# Task 08: MainWindowServiceRegistry 再分割（テーマB: MainWindow依存密度低減）

## 目的

`MainWindowServiceRegistry` の `ensure*` をドメイン単位の子レジストリに再分割し、ensure*件数を50→35に削減する。

## 背景

- Task 07で作成した `docs/dev/ensure-graph-analysis.md` の設計に基づいて実施
- `MainWindowServiceRegistry` に集中する50件のensure*を分散し、認知負荷を下げる
- テーマB（MainWindow周辺の依存密度低減）の主作業

## 前提

- Task 07（ensure*グラフ分析）が完了していること
- `docs/dev/ensure-graph-analysis.md` が存在すること

## 事前調査

### Step 1: Task 07の設計確認

```bash
cat docs/dev/ensure-graph-analysis.md
```

### Step 2: 現状のRegistryファイル構成確認

```bash
wc -l src/app/mainwindow*registry*.cpp src/app/mainwindow*registry*.h
cat src/app/mainwindowserviceregistry.h
```

## 実装手順

### Step 3: 子レジストリクラスの作成

Task 07の分析結果に基づき、ドメイン単位で子レジストリを作成または既存を再編:

既存の分割ファイル:
- `mainwindowanalysisregistry.cpp`
- `mainwindowboardregistry.cpp`
- `mainwindowgameregistry.cpp`
- `mainwindowkifuregistry.cpp`
- `mainwindowuiregistry.cpp`

これらの粒度をさらに見直し、ensure*メソッドをより適切なグループへ移動する。

### Step 4: ensure*メソッドの移動

1. 移動先の子レジストリにメソッド実装を移動
2. ヘッダの宣言を更新
3. 呼び出し元を新しいアクセスパスに更新

### Step 5: 不要なensure*の統合・削除

- 類似のensure*を統合（例: 同じ依存セットを持つもの）
- デッドコードとなったensure*を削除

### Step 6: CMakeLists.txt更新

```bash
scripts/update-sources.sh
```

### Step 7: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen build/tests/tst_structural_kpi
QT_QPA_PLATFORM=offscreen build/tests/tst_layer_dependencies
```

### Step 8: 構造KPI更新

`tests/tst_structural_kpi.cpp` の `ensure*` 上限を実測値に合わせる。

## 完了条件

- [ ] `MainWindowServiceRegistry` の `ensure*` 件数が50から段階的に減少（目標: 35）
- [ ] 全テスト通過
- [ ] 起動/終了/新規対局開始フローに回帰がない
- [ ] 構造KPI例外値が最新化されている

## KPI変化目標

- `ensure*` 件数: 50 → 35
- 子レジストリの責務が明確になっている

## 注意事項

- `updateDeps()` チェーンの整合性を維持する
- lazy-init（遅延初期化）パターンを壊さない
- 循環依存を導入しない
- 段階的に移行し、1PRあたりの変更ファイル数は15以下に抑える
