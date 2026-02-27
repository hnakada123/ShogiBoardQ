# Task 04: MainWindow friend class 削減の実装

## 目的
Task 03 の分析結果に基づき、`MainWindow` の `friend class` を 11 → 3以下に削減する。

## 前提
- Task 03 の分析ドキュメントが完成していること
- Task 01 の構造KPIテストが導入済みであること

## 作業内容

### Phase A: Registry 系 friend の統合
現在 `MainWindowServiceRegistry` 配下に 5つの sub-registry がある:
- `MainWindowAnalysisRegistry`
- `MainWindowBoardRegistry`
- `MainWindowGameRegistry`
- `MainWindowKifuRegistry`
- `MainWindowUiRegistry`

これら 5つの sub-registry は `MainWindowServiceRegistry` 経由でのみ MainWindow にアクセスしている。
`MainWindowServiceRegistry` が MainWindow へのアクセスを仲介する構造に変更し、sub-registry から `friend class` を削除する。

具体的手順:
1. 各 sub-registry が直接参照している MainWindow メンバを特定する
2. 必要なメンバへのアクセスを `MainWindowServiceRegistry` のメソッド経由に変更する
3. 各 sub-registry の `friend class MainWindowXxxRegistry` 宣言を `mainwindow.h` から削除する

### Phase B: Bootstrapper/Factory/Assembler の friend 削減
Task 03 の分析に基づき、以下の friend を削除する:
- `MainWindowDockBootstrapper` → 必要なアクセスを public/protected API に置換
- `MainWindowRuntimeRefsFactory` → buildRuntimeRefs() 経由に限定
- `MainWindowWiringAssembler` → ServiceRegistry 経由に限定

### Phase C: mainwindow.h のヘッダーファイルクリーンアップ
- 削除した friend class の前方宣言を整理する
- 不要になった include を削除する

### テスト実行
```bash
cmake --build build
cd build && ctest --output-on-failure
```

## 完了条件
- [ ] `friend class` が 3以下に削減されている
- [ ] 全テストが PASS する（既存31件 + 新規KPIテスト）
- [ ] 構造KPIテストの friend 上限値を更新する
- [ ] コンパイル警告が増えていないこと

## 注意事項
- 1 Phase ごとにビルド＋テストを実行し、回帰を即検知する
- sub-registry が MainWindow に直接アクセスしている箇所が多い場合は、段階的に進める
- `w->m_xxx` のアクセスを `w->xxx()` getter に変更する場合、getter が clazy-const-signal-or-slot 警告を出さないよう `public:` セクション（`public slots:` ではない）に配置する
