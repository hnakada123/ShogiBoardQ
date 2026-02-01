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
| `--click-tree-node2 <row,ply>` | 2回目の分岐ツリーノードクリック | tree-node後 +1500ms |
| `--click-next-after-kifu <count>` | click-kifu-row後の1手進む | kifu-row後 +500ms |

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

### 10. 分岐ツリーの罫線接続確認テスト

分岐ツリーにおいて、各分岐ラインの branchPoint（分岐元ノード）が正しく設定されていることを確認。
特に「分岐の分岐」（Line 2 のような二段階分岐）の罫線が正しい親ノードから引かれることを確認する。

```bash
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --dump-state-after 2000
```

**期待結果**:
- `line[ 0 ] ... branchPly: 0 branchPoint: "null"`（本譜は分岐なし）
- `line[ 1 ] ... branchPly: 3 branchPoint: "△３四歩(33)"`（2手目から3手目で分岐）
- `line[ 2 ] ... branchPly: 5 branchPoint: "△８四歩(83)"`（4手目から5手目で分岐）

**補足**: このテストは、Line 2（７七角分岐）の罫線がLine 1の4手目「△８四歩(83)」から正しく引かれることを確認します。
修正前は、最初に見つかった分岐点（3手目「▲６六歩」の親である2手目「△３四歩」）を使用していたため、
Line 2 の罫線が誤って3手目の位置から引かれていました。
修正により、最後に見つかった分岐点（そのライン固有の分岐点）を使用するようになりました。

### 11. 分岐ライン上での「本譜へ戻る」表示テスト

分岐ライン上にいるとき、分岐候補欄に「本譜へ戻る」ボタンが正しく表示されることを確認。

```bash
./build/ShogiBoardQ --test-mode \
    --load-kif test2_branch.kif \
    --click-tree-node 1,5 \
    --dump-state-after 4000
```

**期待結果**:
- `kifuBranchModel hasBackToMainRow: true`
- `branch[ 2 ]: "本譜へ戻る"`（分岐候補欄に「本譜へ戻る」が表示される）
- `currentLineIndex: 1`（分岐ライン上にいる）

**補足**: このテストは、分岐ライン上にいるにもかかわらず「本譜へ戻る」ボタンが表示されないバグ（修正済み）の再発を防ぐためのものです。
元の実装では `KifuBranchNode::isMainLine()` が「親の最初の子か」だけで判定していたため、
分岐ライン上でも親の最初の子であれば「本譜」と判定されていました。
修正により、`currentLineIndex()` が 0 かどうかで本譜判定を行うようになりました。

### 12. 分岐ライン選択後に共有ノードクリックでライン維持テスト

分岐ツリーで分岐ラインのノードをクリックした後、開始局面や分岐前の共有ノードをクリックしても、
棋譜欄が現在の分岐ラインの内容を維持することを確認。

```bash
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --click-tree-node 1,4 \
    --click-tree-node2 0,0 \
    --dump-state-after 5000
```

**期待結果**:
- `currentPly: 0`
- `currentLineIndex: 1`（分岐ライン1に留まる、本譜に切り替わらない）
- `kifu[ 3 ]:    3 ▲６六歩(67)+`（棋譜欄が分岐ラインの内容を維持）

**補足**: このテストは、分岐ライン上にいるときに、分岐ツリーの開始局面や分岐前の共有ノードをクリックすると
棋譜欄が本譜に切り替わってしまうバグの再発を防ぐためのものです。
開始局面や分岐前の指し手は複数のラインで共有されているため、クリック時に現在のラインコンテキストを維持すべきです。

### 13. 棋譜欄クリック後の1手進むボタンテスト

棋譜欄で分岐ライン上の指し手をクリックした後、1手進むボタンで正しく次の手に移動することを確認。

```bash
./build/ShogiBoardQ --test-mode \
    --load-kif test2_branch.kif \
    --click-tree-node 1,4 \
    --click-kifu-row 4 \
    --click-next-after-kifu 1 \
    --dump-state-after 12000
```

**期待結果**:
- `currentPly: 5`（4手目から5手目に進む）
- `currentLineIndex: 1`（分岐ライン1に留まる）
- `currentNode displayText: "▲４六歩(47)"`（Line 1の5手目）

**補足**: このテストは、棋譜欄で分岐ライン上の指し手をクリックした後に1手進むボタンを押しても
局面が進まないバグの再発を防ぐためのものです。
原因は2つありました：
1. `onLegacyPositionChanged` で `rememberLineSelection` が呼ばれず、次のナビゲーションで正しい子が選択されなかった
2. `m_skipBoardSyncForBranchNav` フラグが true のままで、`onRecordPaneMainRowChanged` がブロックされていた

## 検証項目

テスト実行後、debug.logで以下の項目を確認できます：

1. **将棋盤の局面** (`currentSfen`)
2. **棋譜欄の状態** (`kifuRecordModel rowCount`, `currentHighlightRow`)
3. **分岐候補欄** (`kifuBranchModel rowCount`, 各候補の内容, `branchView enabled`)
4. **ナビゲーション状態** (`currentPly`, `currentLineIndex`, `isOnMainLine`, `currentNode displayText`)
5. **分岐ツリー** (`branchTree lineCount`, 各ラインのノード数, `branchPly`, `branchPoint`)

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

# テスト10: 分岐ツリーの罫線接続確認
echo ""
echo "--- テスト10: 分岐ツリーの罫線接続確認 ---"
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --dump-state-after 2000 2>&1 > /dev/null
grep -E "line\[.*branchPly.*branchPoint" debug.log | tail -3

# テスト11: 分岐ライン上での「本譜へ戻る」表示
echo ""
echo "--- テスト11: 分岐ライン上での「本譜へ戻る」表示 ---"
./build/ShogiBoardQ --test-mode \
    --load-kif test2_branch.kif \
    --click-tree-node 1,5 \
    --dump-state-after 4000 2>&1 > /dev/null
grep -E "hasBackToMainRow|currentLineIndex:|branch\[" debug.log | tail -5

# テスト12: 分岐ライン選択後に共有ノードクリックでライン維持
echo ""
echo "--- テスト12: 分岐ライン選択後に共有ノードクリックでライン維持 ---"
./build/ShogiBoardQ --test-mode \
    --load-kif test_branch.kif \
    --click-tree-node 1,4 \
    --click-tree-node2 0,0 \
    --dump-state-after 5000 2>&1 > /dev/null
grep -E "currentPly:|currentLineIndex:|kifu\[" debug.log | tail -5

# テスト13: 棋譜欄クリック後の1手進むボタン
echo ""
echo "--- テスト13: 棋譜欄クリック後の1手進むボタン ---"
./build/ShogiBoardQ --test-mode \
    --load-kif test2_branch.kif \
    --click-tree-node 1,4 \
    --click-kifu-row 4 \
    --click-next-after-kifu 1 \
    --dump-state-after 12000 2>&1 > /dev/null
grep -E "currentPly:|currentLineIndex:|currentNode displayText:" debug.log | tail -3

echo ""
echo "=== テスト完了 ==="
```

## 関連ソースファイル

- `src/app/main.cpp` - テストオプションの定義と実行タイミング
- `src/app/mainwindow.cpp` - テストメソッド実装（`clickKifuRow`, `clickBranchCandidate`, `dumpTestState`等）
- `src/navigation/kifunavigationcontroller.cpp` - 分岐ナビゲーションロジック
- `src/kifu/kifunavigationstate.cpp` - ナビゲーション状態管理
- `src/ui/coordinators/kifudisplaycoordinator.cpp` - 棋譜表示・分岐ツリーハイライト連携
