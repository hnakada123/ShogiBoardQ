# Task 08: JosekiWindow 分割の分析

## 目的
`JosekiWindow`（2406行 + 450行ヘッダー）の責務を分析し、View/Presenter/Repository に分割する計画を策定する。

## 背景
`josekiwindow.cpp` は現在 2406行で、プロジェクト最大のファイル。
UI構築・イベント処理・ファイルI/O・変換ロジックが同居している。

## 作業内容

### 1. 責務の洗い出し
`josekiwindow.cpp` の全メソッドを以下のカテゴリに分類する:

#### A: View（表示のみ）
- UI要素の初期化・レイアウト
- テーブルへのデータ表示
- ユーザー入力イベントのハンドリング（Presenter への委譲のみ）

#### B: Presenter（状態遷移・操作フロー）
- 定跡データの検索・フィルタリング
- 操作フロー（追加・削除・マージ・移動）
- 盤面連携ロジック
- バリデーション

#### C: Repository（データI/O）
- ファイルの読み込み・保存
- データフォーマットの変換
- データベース（もしあれば）とのやり取り

#### D: Model（データ構造）
- `QTableWidget` で直接管理しているデータ構造
- `QAbstractTableModel` に移行すべきデータ

### 2. QTableWidget → QAbstractTableModel 移行可能性の分析
- 現在のテーブル操作パターンを洗い出す
- `setItem()`, `item()`, `rowCount()` 等の使用箇所をカウントする
- Model/View パターンへの移行コストを見積もる

### 3. 分割計画の策定
以下のクラス構成を提案する:
- `JosekiWindow` (View): `.ui` + 薄い QWidget
- `JosekiPresenter`: 状態遷移・操作フロー
- `JosekiRepository`: ファイルI/O・データ変換
- `JosekiTableModel`: QAbstractTableModel ベースのデータモデル

各クラスの責務・メソッド一覧・依存関係を明記する。

### 4. 実装順序の提案
段階的に分割できる順序を提案する（各段階でビルド可能であること）。

## 出力
`docs/dev/done/josekiwindow-split-analysis.md` に分析結果と計画を記載する。

## 完了条件
- [ ] 全メソッドがカテゴリ分類されている
- [ ] QTableWidget → QAbstractTableModel の移行計画がある
- [ ] 分割後の各クラスの責務が明確に定義されている
- [ ] 段階的な実装計画がある

## 注意事項
- この段階ではコード変更は行わない（分析のみ）
- `josekiwindow.h` のシグナル・スロット宣言も確認する
- 既存の `tst_josekiwindow.cpp` テストが分割後も動作する計画にする
