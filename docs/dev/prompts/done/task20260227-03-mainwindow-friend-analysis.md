# Task 03: MainWindow friend class 削減の分析

## 目的
`MainWindow` の 11個の `friend class` を分析し、3以下に削減する計画を策定する。

## 背景
現在の friend class 一覧（11件）:
1. `MainWindowUiBootstrapper`
2. `MainWindowRuntimeRefsFactory`
3. `MainWindowWiringAssembler`
4. `MainWindowServiceRegistry`
5. `MainWindowAnalysisRegistry`
6. `MainWindowBoardRegistry`
7. `MainWindowGameRegistry`
8. `MainWindowKifuRegistry`
9. `MainWindowUiRegistry`
10. `MainWindowDockBootstrapper`
11. `MainWindowLifecyclePipeline`

## 作業内容

### 1. 各 friend class のアクセス分析
各 friend class が MainWindow のどの private/protected メンバに実際にアクセスしているかを洗い出す。

以下のファイルを調査する:
- `src/app/mainwindowuibootstrapper.cpp`
- `src/app/mainwindowruntimerefsfactory.cpp`
- `src/app/mainwindowwiringassembler.cpp`
- `src/app/mainwindowserviceregistry.cpp`
- `src/app/mainwindowanalysisregistry.cpp`
- `src/app/mainwindowboardregistry.cpp`
- `src/app/mainwindowgameregistry.cpp`
- `src/app/mainwindowkifuregistry.cpp`
- `src/app/mainwindowuiregistry.cpp`
- `src/app/mainwindowdockbootstrapper.cpp`
- `src/app/mainwindowlifecyclepipeline.cpp`

各ファイルで `w->` または `w.` でアクセスしている MainWindow のメンバを列挙する。

### 2. アクセスパターンの分類
各メンバアクセスを以下に分類する:
- **A: public化可能** - 外部からアクセスされても安全なメンバ
- **B: getter/setter追加で対応可能** - アクセサメソッドを追加すれば friend 不要
- **C: 構造変更が必要** - メンバ自体を別クラスに移動すべき
- **D: friend が必須** - 初期化フェーズなど、friend でないと実現困難

### 3. 削減計画の策定
分析結果を元に、以下を含む計画を `docs/dev/done/` に作成する:
- 残すべき friend class（3以下）とその理由
- 削除する各 friend class の代替手段
- 実装順序と依存関係

## 出力
`docs/dev/done/mainwindow-friend-reduction-analysis.md` に分析結果と計画を記載する。

## 完了条件
- [ ] 全 friend class のアクセスパターンが列挙されている
- [ ] 各 friend class の削除可否と代替手段が明記されている
- [ ] 3以下に削減する具体的な実装計画がある

## 注意事項
- この段階ではコード変更は行わない（分析のみ）
- Registry 系（5つ）は類似パターンなのでまとめて分析できる可能性が高い
- 5つの `*Registry` は `MainWindowServiceRegistry` 経由の階層構造になっているため、ServiceRegistry 1つに集約できる可能性がある
