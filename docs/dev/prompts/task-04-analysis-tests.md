# Task 4: AnalysisFlowController テスト追加

`src/analysis/analysisflowcontroller.h/.cpp` の解析フロー制御に対するユニットテストを作成してください。

## 対象ファイル

- テスト対象: `src/analysis/analysisflowcontroller.h/.cpp`
- 新規作成: `tests/tst_analysisflow.cpp`
- テスト登録: `tests/CMakeLists.txt` に追加

## 前提

まず `analysisflowcontroller.h/.cpp` を読み、クラスのインターフェース（公開メソッド、シグナル、依存関係）を把握した上でテストを設計すること。依存オブジェクトが多い場合はスタブまたはモックで代替する。

## テストすべき項目

### 1. 状態遷移
- 初期状態（Idle）の確認
- 解析開始 → Running 状態への遷移
- 解析停止 → Idle 状態への復帰
- 解析中に再度開始を要求した場合の挙動

### 2. シグナル発火
- 解析開始時のシグナル発火確認（`QSignalSpy` 使用）
- 解析停止時のシグナル発火確認
- 進捗更新時のシグナル発火確認

### 3. エラー時の状態回復
- 依存オブジェクトがない状態で解析開始を試みた場合
- 解析中にエラーが発生した場合の状態遷移

## 設計方針

- AnalysisFlowController の依存関係を最小限のスタブで満たす
- エンジンプロセスの起動は不要（プロトコル解析はしない）
- `QSignalSpy` でシグナル検証
- テスト用スタブは `tests/test_stubs.cpp` パターンに従う

## 制約

- `connect()` にラムダを使わない
- Qt Test フレームワーク使用
- 実装後、ビルドして全テストが通ることを確認
