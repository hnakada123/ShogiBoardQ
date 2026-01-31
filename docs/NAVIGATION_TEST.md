# 棋譜ナビゲーション自動テスト

このドキュメントは、ShogiBoardQの棋譜ナビゲーション機能の自動テスト方法を説明します。

## テスト用KIFファイル

テストには `test_branch.kif` を使用します（リポジトリルートに配置）。

```
# ---- 分岐テスト用棋譜 ----
手合割：平手
先手：テスト先手
後手：テスト後手

   1 ７六歩(77)
   2 ３四歩(33)
   3 ２六歩(27)
   4 ８四歩(83)
   5 ２五歩(26)
   6 ８五歩(84)
   7 ７八金(69)

変化：3手
   3 ６六歩(67)
   4 ８四歩(83)
   5 ６五歩(66)

変化：5手
   5 ７七角(88)
   6 ３二金(41)
```

### ライン構造

- **Line 0 (本譜)**: 開始局面 → ７六歩 → ３四歩 → ２六歩 → ８四歩 → ２五歩 → ８五歩 → ７八金
- **Line 1 (６六歩分岐)**: 開始局面 → ７六歩 → ３四歩 → ６六歩 → ８四歩 → ６五歩
- **Line 2 (７七角分岐)**: 開始局面 → ７六歩 → ３四歩 → ６六歩 → ８四歩 → ７七角 → ３二金

## コマンドラインテストオプション

### 基本オプション

| オプション | 説明 | 実行タイミング |
|------------|------|----------------|
| `--test-mode` | テストモード有効化（詳細ログ出力） | 起動時 |
| `--load-kif <file>` | KIFファイルを読み込み | 500ms |
| `--navigate-to <ply>` | 指定手数まで移動 | 1000ms |
| `--click-branch <index>` | 分岐候補をクリック（0始まり） | 1500ms |
| `--dump-state-after <ms>` | 指定ms後に状態をダンプして終了 | 指定時間 |

### ナビゲーションオプション（第1フェーズ）

| オプション | 説明 | 実行タイミング |
|------------|------|----------------|
| `--click-next <count>` | 1手進むを指定回数クリック | 2000ms〜 |
| `--click-prev <count>` | 1手戻るを指定回数クリック | next後 +500ms〜 |

### ナビゲーションオプション（第2フェーズ）

| オプション | 説明 | 実行タイミング |
|------------|------|----------------|
| `--click-branch2 <index>` | 2回目の分岐候補クリック | prev後 +500ms |
| `--click-next2 <count>` | branch2後の1手進む | branch2後 +500ms |
| `--click-prev2 <count>` | next2後の1手戻る | next2後 +500ms |
| `--click-next3 <count>` | prev2後の1手進む | prev2後 +500ms |

### 棋譜欄直接クリック

| オプション | 説明 | 実行タイミング |
|------------|------|----------------|
| `--click-kifu-row <row>` | 棋譜欄の行を直接クリック | 全操作完了後 +500ms |

### 分岐ツリー直接クリック

| オプション | 説明 | 実行タイミング |
|------------|------|----------------|
| `--click-tree-node <row,ply>` | 分岐ツリーのノードを直接クリック | kifu-row後 +500ms |

## テストシナリオ

### 1. 基本ナビゲーションテスト（本譜）

棋譜欄の各行をクリックして、分岐ツリーが正しくハイライトされることを確認。

```bash
# Row 0〜7 を順にテスト
for row in 0 1 2 3 4 5 6 7; do
    ./build/ShogiBoardQ --test-mode \
        --load-kif test_branch.kif \
        --click-kifu-row $row \
        --dump-state-after 3000
done
```

**期待結果**: 全ての行で `highlighting branch tree at line= 0` となること

### 2. 分岐選択テスト

3手目で分岐を選択し、正しく分岐ラインに移動することを確認。

```bash
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --navigate-to 3 \
    --click-branch 1 \
    --dump-state-after 4000
```

**期待結果**:
- `currentPly: 3`
- `currentLineIndex: 1`
- `currentNode displayText: "▲６六歩(67)"`

### 3. 分岐内ナビゲーションテスト

分岐選択後に1手進む/戻るで分岐ライン上を移動することを確認。

```bash
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --navigate-to 3 \
    --click-branch 1 \
    --click-prev 1 \
    --click-next 1 \
    --dump-state-after 5000
```

**期待結果**:
- `currentPly: 3`
- `currentLineIndex: 1`
- `currentNode displayText: "▲６六歩(67)"`

### 4. サブブランチ（７七角）テスト

６六歩分岐から７七角サブブランチに移動することを確認。

```bash
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --navigate-to 3 \
    --click-branch 1 \
    --click-next 2 \
    --click-branch2 1 \
    --dump-state-after 5000
```

**期待結果**:
- `currentPly: 5`
- `currentLineIndex: 2`
- `currentNode displayText: "▲７七角(88)"`

### 5. サブブランチ内ナビゲーションテスト

７七角サブブランチで戻る→進むを実行し、サブブランチに留まることを確認。

```bash
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --navigate-to 3 \
    --click-branch 1 \
    --click-next 2 \
    --click-branch2 1 \
    --click-prev2 1 \
    --click-next3 1 \
    --dump-state-after 7000
```

**期待結果**:
- `currentPly: 5`
- `currentLineIndex: 2`
- `currentNode displayText: "▲７七角(88)"`

### 6. 分岐選択後の棋譜欄クリックテスト

分岐を選択した後に棋譜欄をクリックし、分岐ツリーが正しくハイライトされることを確認。

```bash
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --navigate-to 3 \
    --click-branch 1 \
    --click-kifu-row 4 \
    --dump-state-after 6000
```

**期待結果**:
- `highlighting branch tree at line= 1 ply= 4`
- `currentPly: 4`
- `currentLineIndex: 1`

### 7. 分岐ツリー直接クリック後のナビゲーションテスト

分岐ツリーの７七角ノードを直接クリックし、その後1手戻るボタンで分岐ライン上の前のノードに正しく移動することを確認。

```bash
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --click-tree-node 2,5 \
    --click-prev 1 \
    --dump-state-after 5000
```

**期待結果**:
- `currentPly: 4`
- `currentLineIndex: 2`（本譜ではなく分岐ライン2に留まる）
- `currentNode displayText: "△８四歩(83)"`（Line 2の4手目）

**補足**: このテストは、分岐ツリーのノードを直接クリックした後に1手戻るボタンを押した際、
誤って本譜ラインにジャンプしてしまうバグ（修正済み）の再発を防ぐためのものです。

### 8. 分岐ツリーから分岐候補クリックでライン切り替えテスト

分岐ツリーで分岐ラインのノードをクリックした後、分岐候補欄から本譜の手を選択すると、
棋譜欄と分岐ツリーのハイライトが正しく本譜に切り替わることを確認。

```bash
./build/ShogiBoardQ --test-mode \
    --load-kif test2_branch.kif \
    --click-tree-node 3,3 \
    --click-branch 0 \
    --dump-state-after 6000
```

**期待結果**:
- `currentPly: 3`
- `currentLineIndex: 0`（分岐ライン3から本譜ライン0に切り替わる）
- `currentNode displayText: "▲２六歩(27)"`
- `kifu[ 3 ]:    3 ▲２六歩(27)+`（棋譜欄が本譜の内容に更新される）

**補足**: このテストは、分岐ツリーで分岐ラインを選択後に分岐候補から別ラインの手を選択した際、
棋譜欄の内容と分岐ツリーのハイライトが正しく更新されないバグ（修正済み）の再発を防ぐためのものです。

### 9. 1手進むボタンで移動後の分岐候補欄有効化テスト

棋譜読み込み後に1手進むボタンで3手目まで移動し、分岐候補欄が有効（クリック可能）であることを確認。

```bash
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --click-next 3 \
    --dump-state-after 4000
```

**期待結果**:
- `branchView enabled: true`
- `kifuBranchModel rowCount: 2`
- `branch[ 0 ]: "▲２六歩(27)"`
- `branch[ 1 ]: "▲６六歩(67)"`

**補足**: このテストは、棋譜欄を直接クリックした場合は分岐候補欄が有効になるが、
1手進むボタンで移動した場合は分岐候補欄が無効（薄い文字でクリック不可）になるバグ（修正済み）の再発を防ぐためのものです。
棋譜読み込み時に分岐候補ビューが `setEnabled(false)` で無効化され、
ナビゲーションボタン経由の移動では有効化処理が呼ばれていなかったことが原因でした。

## 検証項目

テスト実行後、debug.logで以下の項目を確認できます：

1. **将棋盤の局面** (`currentSfen`)
2. **棋譜欄の状態** (`kifuRecordModel rowCount`, `currentHighlightRow`)
3. **分岐候補欄** (`kifuBranchModel rowCount`, 各候補の内容, `branchView enabled`)
4. **ナビゲーション状態** (`currentPly`, `currentLineIndex`, `isOnMainLine`, `currentNode displayText`)
5. **分岐ツリー** (`branchTree lineCount`, 各ラインのノード数)

## 一括テストスクリプト

```bash
#!/bin/bash
cd /home/nakada/GitHub/ShogiBoardQ

echo "=== 棋譜ナビゲーション自動テスト ==="

# テスト1: 本譜の全行クリック
echo ""
echo "--- テスト1: 本譜の全行クリック ---"
for row in 0 1 2 3 4 5 6 7; do
    ./build/ShogiBoardQ --test-mode \
        --load-kif test_branch.kif \
        --click-kifu-row $row \
        --dump-state-after 3000 2>&1 > /dev/null
    grep "highlighting branch tree" debug.log | tail -1
done

# テスト2: 分岐選択
echo ""
echo "--- テスト2: 分岐選択 ---"
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --navigate-to 3 \
    --click-branch 1 \
    --dump-state-after 4000 2>&1 > /dev/null
grep -E "currentPly:|currentLineIndex:|currentNode displayText:" debug.log | tail -3

# テスト3: 分岐内ナビゲーション
echo ""
echo "--- テスト3: 分岐内ナビゲーション ---"
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --navigate-to 3 \
    --click-branch 1 \
    --click-prev 1 \
    --click-next 1 \
    --dump-state-after 5000 2>&1 > /dev/null
grep -E "currentPly:|currentLineIndex:|currentNode displayText:" debug.log | tail -3

# テスト4: サブブランチ（７七角）
echo ""
echo "--- テスト4: サブブランチ（７七角） ---"
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --navigate-to 3 \
    --click-branch 1 \
    --click-next 2 \
    --click-branch2 1 \
    --dump-state-after 5000 2>&1 > /dev/null
grep -E "currentPly:|currentLineIndex:|currentNode displayText:" debug.log | tail -3

# テスト5: サブブランチ内ナビゲーション
echo ""
echo "--- テスト5: サブブランチ内ナビゲーション ---"
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --navigate-to 3 \
    --click-branch 1 \
    --click-next 2 \
    --click-branch2 1 \
    --click-prev2 1 \
    --click-next3 1 \
    --dump-state-after 7000 2>&1 > /dev/null
grep -E "currentPly:|currentLineIndex:|currentNode displayText:" debug.log | tail -3

# テスト6: 分岐選択後の棋譜欄クリック
echo ""
echo "--- テスト6: 分岐選択後の棋譜欄クリック ---"
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --navigate-to 3 \
    --click-branch 1 \
    --click-kifu-row 4 \
    --dump-state-after 6000 2>&1 > /dev/null
grep -E "highlighting branch tree|currentPly:|currentLineIndex:" debug.log | tail -3

# テスト7: 分岐ツリー直接クリック後のナビゲーション
echo ""
echo "--- テスト7: 分岐ツリー直接クリック後のナビゲーション ---"
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --click-tree-node 2,5 \
    --click-prev 1 \
    --dump-state-after 5000 2>&1 > /dev/null
grep -E "currentPly:|currentLineIndex:|currentNode displayText:" debug.log | tail -3

# テスト8: 分岐ツリーから分岐候補クリックでライン切り替え（test2_branch.kif使用）
echo ""
echo "--- テスト8: 分岐ツリーから分岐候補クリックでライン切り替え ---"
./build/ShogiBoardQ --test-mode \
    --load-kif test2_branch.kif \
    --click-tree-node 3,3 \
    --click-branch 0 \
    --dump-state-after 6000 2>&1 > /dev/null
grep -E "currentPly:|currentLineIndex:|currentNode displayText:" debug.log | tail -3

# テスト9: 1手進むボタンで移動後の分岐候補欄有効化
echo ""
echo "--- テスト9: 1手進むボタンで移動後の分岐候補欄有効化 ---"
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --click-next 3 \
    --dump-state-after 4000 2>&1 > /dev/null
grep -E "branchView enabled:|kifuBranchModel rowCount:|branch\[" debug.log | tail -4

echo ""
echo "=== テスト完了 ==="
```

## 関連ソースファイル

- `src/app/main.cpp` - テストオプションの定義と実行タイミング
- `src/app/mainwindow.cpp` - テストメソッド実装（`clickKifuRow`, `clickBranchCandidate`, `dumpTestState`等）
- `src/navigation/kifunavigationcontroller.cpp` - 分岐ナビゲーションロジック
- `src/kifu/kifunavigationstate.cpp` - ナビゲーション状態管理
- `src/ui/coordinators/kifudisplaycoordinator.cpp` - 棋譜表示・分岐ツリーハイライト連携
