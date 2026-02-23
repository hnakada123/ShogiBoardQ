# Task 10: unique_ptr 移行

明示的に `delete` を使用しているメンバ変数を `std::unique_ptr` に移行してください。

## 作業内容

### Step 1: 対象箇所の特定

コードベース全体で `delete m_` を検索し、Qt の親子関係で管理されていない手動 delete を特定する。

既知の対象:
- `src/dialogs/pvboarddialog.cpp`: `delete m_fromHighlight`, `delete m_toHighlight`
- `src/dialogs/engineregistrationdialog.cpp`: `delete m_process`
- `src/game/gamestartcoordinator.cpp`: `delete m_match` 等

### Step 2: unique_ptr への置換

各対象について:

1. **ヘッダ**: `Type* m_member = nullptr` → `std::unique_ptr<Type> m_member`
2. **生成箇所**: `m_member = new Type(...)` → `m_member = std::make_unique<Type>(...)`
3. **delete 箇所**: `delete m_member` → 行を削除（unique_ptr が自動解放）
4. **ポインタ使用箇所**: `m_member->method()` はそのまま使える。生ポインタが必要な箇所では `m_member.get()` を使用
5. **null チェック**: `if (m_member)` はそのまま使える

### Step 3: Qt 親子関係の確認

**変更してはいけないケース**:
- `new QWidget(parent)` のように親オブジェクトに渡しているもの → Qt が管理するため unique_ptr 不要
- `QObject::deleteLater()` で削除しているもの → 非同期削除のため unique_ptr と相性が悪い

これらは現状維持とする。

### Step 4: デストラクタの簡素化

unique_ptr 化により不要になった `delete` 文をデストラクタから削除。デストラクタが空になった場合は `= default` に変更。

## 制約

- `#include <memory>` を追加
- CLAUDE.md のコードスタイルに準拠
- Qt の親子関係で管理されているオブジェクトは対象外
- 全既存テストが通ること
