# Task 23: 所有権の明確化・unique_ptr 移行

## 目的

所有権が曖昧なメンバ変数を整理し、メモリリーク・二重解放のリスクを下げる。

## 背景

- `m_gameSessionFacade` は `new` で確保されているがコメントが「非所有」で `delete` が存在しない（メモリリーク）。
- 明示的な `delete` が散在し、コード変更時のパス漏れリスクがある。

## 対象ファイル

- `src/app/mainwindow.h` / `src/app/mainwindow.cpp`
- 関連する `ensureXxx()` 実装
- 明示的 `delete` が存在する以下のファイル:
  - `src/dialogs/pvboarddialog.cpp`（`m_fromHighlight`, `m_toHighlight`）
  - `src/dialogs/engineregistrationdialog.cpp`（`m_process`）
  - `src/game/gamestartcoordinator.cpp`（`m_match`）

## 作業

### Step 1: MainWindow の所有権監査

1. `mainwindow.h` のメンバ変数のうち「非所有」コメントが付いたものを洗い出す。
2. 各メンバの実際の生成箇所（`new`）と解放箇所（`delete` または Qt 親子）を確認する。
3. コメントと実態の不一致を修正する。

### Step 2: m_gameSessionFacade の修正

`GameSessionFacade* m_gameSessionFacade` を `std::unique_ptr<GameSessionFacade>` に変更する。

### Step 3: 明示的 delete の unique_ptr 化

Qt 親子関係に属さないオブジェクトで `delete` しているものを `std::unique_ptr` に移行する。

注意:
- Qt 親子関係で管理されるオブジェクトは対象外（`deleteLater()` や親の `delete` で解放される）。
- 挙動変更は避ける（寿命管理の明確化のみ）。
- 既存シグナル/スロット接続順序を壊さない。

## 受け入れ条件

- [ ] `new` で確保したオブジェクトに対応する解放が全て存在する（リークなし）。
- [ ] コメント上の所有権表現と実コードが一致している。
- [ ] ビルド成功、既存テストが通る。
