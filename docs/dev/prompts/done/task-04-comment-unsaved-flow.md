# Task 04: コメント未保存確認と行復元フローの移譲

元ドキュメント: `docs/dev/mainwindow-responsibility-delegation-methods.md` セクション 4.2

## 対象メソッド

- `MainWindow::onRecordRowChangedByPresenter`

## 現状問題

- 未保存コメント確認、キャンセル時の行復元、コメント表示更新が混在。
- `QSignalBlocker` と `QTableView` 操作が `MainWindow` に残っている。

## 移譲先

- 既存 `CommentCoordinator` へ集約する。

## 実装手順

1. `CommentCoordinator` に新規メソッドを追加する。
   `bool handleRecordRowChangeRequest(int row, EngineAnalysisTab* analysisTab, RecordPane* recordPane)`
2. 内部で以下を実施する。
   - `hasUnsavedComment()` 判定
   - `confirmDiscardUnsavedComment()` 判定
   - キャンセル時の `QSignalBlocker` + 行復元
3. `MainWindow::onRecordRowChangedByPresenter` は以下だけにする。
   - `ensureCommentCoordinator()`
   - `if (!m_commentCoordinator->handleRecordRowChangeRequest(...)) return;`
   - `broadcastComment(...)`
4. 可能なら `comment.trimmed().isEmpty() ? tr("コメントなし") : comment` も `CommentCoordinator` に寄せる。

## 受け入れ条件

- `MainWindow::onRecordRowChangedByPresenter` が 10 行以内になる。
- `QTableView` と `QSignalBlocker` 依存が `MainWindow` から消える。

## 回帰確認

- 未保存コメントありで別行選択時に確認ダイアログが出る。
- 「キャンセル」で行選択が元に戻る。
- コメント表示が棋譜ペインとコメントパネルで同期する。
