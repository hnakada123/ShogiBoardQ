# Task 05: MainWindow ensure* メソッド削減の分析

## 目的
`MainWindow` の 38個の `ensure*` メソッドを分析し、12以下に削減する計画を策定する。

## 背景
現在 `mainwindow.h` に 38個の `ensure*` メソッドが宣言されている。
各メソッドの実装は `mainwindow.cpp` にあり、大半は `m_registry->ensureXxx()` への1行転送になっている。

## 作業内容

### 1. ensure* メソッドの呼び出し元分析
38個の `ensure*` それぞれについて、以下を調査する:
- **誰が呼んでいるか**: MainWindow 自身 / friend class / connect() 経由
- **呼び出し頻度**: 1箇所のみ / 複数箇所
- **転送先**: `m_registry->ensureXxx()` への単純転送か、追加ロジックがあるか

### 2. 削減パターンの分類
各 ensure* を以下に分類する:

#### パターン A: MainWindow から完全削除可能
- 呼び出し元が friend class のみ → friend class が直接 `m_registry->ensureXxx()` を呼べる
- 呼び出し元が MainWindow 内の1箇所のみ → インライン化

#### パターン B: MainWindow に残すが Registry に委譲済み
- MainWindow の public slot から呼ばれている → スロットとして残す必要あり
- connect() のターゲットになっている → メソッドとして残す必要あり

#### パターン C: 起動フェーズに移動可能
- `MainWindowLifecyclePipeline::runStartup()` で一括初期化に変更できる
- 遅延初期化が本当に必要か再評価する

### 3. 削減計画の策定
分析結果を元に:
- 残すべき ensure* メソッド（12以下）の一覧と理由
- 削除する各メソッドの移行先
- 実装順序

## 出力
`docs/dev/done/mainwindow-ensure-reduction-analysis.md` に分析結果を記載する。

## 完了条件
- [ ] 38個の ensure* 全ての呼び出し元が列挙されている
- [ ] 各 ensure* の削減可否と移行先が明記されている
- [ ] 12以下に削減する具体的な計画がある

## 注意事項
- この段階ではコード変更は行わない（分析のみ）
- `mainwindow.cpp` の ensure* 実装を確認し、転送のみか追加ロジックがあるかを区別する
- Task 04（friend 削減）との依存関係に注意する。friend が削除された場合、ensure* へのアクセス経路が変わる可能性がある
