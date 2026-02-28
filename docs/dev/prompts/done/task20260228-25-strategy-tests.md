# Task 25: Strategy群テスト追加（Phase 2: テスト基盤拡充）

## 目的

3つの対局戦型Strategy（人間 vs エンジン、エンジン vs エンジン、人間 vs 人間）のユニットテストを追加する。

## 背景

- `game/` モジュール内の Strategy パターンは対局進行の核心部分
- 現在テストがなく、戦型ごとの振る舞い差異が未検証
- Strategy の切替ミスが対局の根本的な不具合につながる
- 包括的改善分析 §3.4 P2

## 対象テスト

新規作成:
- `tests/tst_gamestrategy.cpp`

## 事前調査

### Step 1: Strategy クラスの一覧と構造

```bash
# Strategy 関連ファイルの特定
rg -l "Strategy" src/game/ --type cpp --type-add 'header:*.h' --type header

# 基底クラス/インターフェース
rg "class.*Strategy" src/game/*.h
```

### Step 2: 各 Strategy の公開インターフェース

```bash
# 各 Strategy のメソッド一覧
for f in src/game/*strategy*.h; do
    echo "=== $(basename $f) ==="
    cat "$f"
done
```

### Step 3: Strategy の生成・使用箇所

```bash
# Strategy がどこで生成されているか
rg "make_unique.*Strategy\|new.*Strategy" src --type cpp
```

### Step 4: Strategy の振る舞い差異の整理

各 Strategy で異なる振る舞い:
- 手の入力方法（UI入力 vs エンジン出力）
- 時計の管理方法
- 手番切替のトリガー

## 実装手順

### Step 5: テストケースの設計

```cpp
class TestGameStrategy : public QObject
{
    Q_OBJECT

private slots:
    // 人間 vs エンジン Strategy
    void humanVsEngine_humanMoveAccepted();
    void humanVsEngine_engineMoveRequested();
    void humanVsEngine_turnAlternates();

    // エンジン vs エンジン Strategy
    void engineVsEngine_bothEnginesMove();
    void engineVsEngine_autoAdvance();

    // 人間 vs 人間 Strategy
    void humanVsHuman_bothSidesAcceptInput();
    void humanVsHuman_noEngineInvolved();

    // 共通振る舞い
    void allStrategies_respectTimeControl();
    void allStrategies_detectGameEnd();
};
```

### Step 6: テスト用スタブの準備

```bash
# 既存スタブの確認
ls tests/stubs/
cat tests/stubs/stub_usi.h 2>/dev/null
```

必要に応じてエンジンスタブ、ゲームコントローラスタブを作成/拡張。

### Step 7: テストの実装

各 Strategy を単独でインスタンス化し、入力に対する出力（シグナル、状態変化）を検証。
`QSignalSpy` でシグナル発火を確認。

### Step 8: CMakeLists.txt に追加

```bash
scripts/update-sources.sh
```

### Step 9: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R gamestrategy --output-on-failure -V
```

## 完了条件

- [ ] `tests/tst_gamestrategy.cpp` が作成されている
- [ ] 3つの Strategy それぞれに最低2件のテストケース
- [ ] 共通振る舞いのテストが含まれている
- [ ] 全テスト通過
- [ ] テストケース数: 8件以上

## KPI変化目標

- テスト数: +1（新規テストファイル）
- Strategy群カバレッジ: 0% → 部分的

## 注意事項

- Strategy は MatchCoordinator と密結合の可能性があるため、スタブの範囲を慎重に設計する
- 非同期処理（エンジンの思考時間）はタイマーをモック化するか、同期的なテスト設計にする
- 各 Strategy の生成に必要な前提条件（設定値等）を調査してからテストを設計する
