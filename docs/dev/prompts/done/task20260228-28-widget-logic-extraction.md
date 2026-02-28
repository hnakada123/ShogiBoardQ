# Task 28: Widget層ロジック抽出（Phase 4: 品質仕上げ）

## 目的

大型Widget/Dialogからビジネスロジックを Presenter/Controller に抽出し、UI/ロジック分離を推進する。

## 背景

- Widget層にUI構築・イベント処理・ビジネスロジックが混在
- `recordpane.cpp`（677行）、`pvboarddialog.cpp`（884行）が主要候補
- 既存のMVPパターン（Presenter/Controller層）に合わせた抽出が可能
- 包括的改善分析 §11.2, §12 P2

## 対象ファイル（優先順）

1. `src/widgets/recordpane.cpp` (677行) — 棋譜表示 + 選択ロジック + モデル管理
2. `src/dialogs/pvboarddialog.cpp` (884行) — PV盤面表示 + エンジン連携 + UI構築

## 事前調査

### Step 1: recordpane.cpp の責務分析

```bash
cat src/widgets/recordpane.h
cat src/widgets/recordpane.cpp
```

メソッドを以下に分類:
1. **UI構築**: ウィジェット生成、レイアウト
2. **データバインディング**: モデルとの接続、表示更新
3. **ビジネスロジック**: 選択状態管理、ナビゲーション判定
4. **イベント処理**: クリック、キーボード、コンテキストメニュー

### Step 2: pvboarddialog.cpp の責務分析

```bash
cat src/dialogs/pvboarddialog.h
cat src/dialogs/pvboarddialog.cpp
```

メソッドを以下に分類:
1. **UI構築**: ダイアログレイアウト、盤面ウィジェット
2. **エンジン連携**: PV解析結果の受信・表示
3. **盤面操作**: PV手順の適用・ナビゲーション

### Step 3: 既存のPresenter/Controllerパターン確認

```bash
# 既存の Presenter の例
cat src/ui/presenters/recordpresenter.h
cat src/ui/presenters/recordpresenter.cpp
```

## 実装手順

### Step 4: recordpane のロジック抽出

**抽出先**: `src/ui/presenters/recordpresenter.cpp` の拡張、または新規 Controller

```bash
# RecordPresenter の現在の責務
rg -n "^void |^bool |^int " src/ui/presenters/recordpresenter.cpp
```

recordpane から以下を抽出:
- 選択状態の判定ロジック
- モデル更新のトリガー判定
- ナビゲーション関連の計算

### Step 5: pvboarddialog のロジック抽出

**抽出先**: 新規 `src/ui/controllers/pvboardcontroller.h/.cpp`

pvboarddialog から以下を抽出:
- PV手順の解析・盤面適用ロジック
- エンジン出力の変換ロジック
- 盤面ナビゲーション（前進/後退）のロジック

### Step 6: 新クラスの実装

Deps パターンに従い、依存を構造体で注入:

```cpp
class PvBoardController : public QObject {
    Q_OBJECT
public:
    struct Deps {
        ShogiGameController* gc = nullptr;
        // ...
    };
    void updateDeps(const Deps& deps);
    // ...
};
```

### Step 7: Widget/Dialog の簡素化

抽出後のWidget/Dialogは:
- UI構築とレイアウトのみ担当
- イベントを Controller/Presenter に委譲
- 表示更新を Presenter から受け取る

### Step 8: CMakeLists.txt 更新

```bash
scripts/update-sources.sh
```

### Step 9: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen build/tests/tst_structural_kpi
```

### Step 10: 構造KPI更新

分割後の行数を `knownLargeFiles()` に反映。

## 完了条件

- [ ] `recordpane.cpp` が 600行以下に削減
- [ ] `pvboarddialog.cpp` が 700行以下に削減
- [ ] 抽出されたロジックが Presenter/Controller に配置されている
- [ ] 全テスト通過
- [ ] 構造KPI更新済み

## KPI変化目標

- `recordpane.cpp`: 677 → 500以下
- `pvboarddialog.cpp`: 884 → 700以下
- 600行超ファイル数: -1〜2

## 注意事項

- Widget の外部インターフェース（シグナル/スロット）は可能な限り維持する
- 他のWiring クラスからの接続が壊れないよう注意
- `connect()` でラムダを使用しない
- SettingsService のダイアログサイズ保存機能がある場合は維持する
- pvboarddialog は複雑度が高いため、段階的に抽出する（全部一度にやらない）
