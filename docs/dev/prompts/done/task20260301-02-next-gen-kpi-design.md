# Task 20260301-02: 次世代KPI設計・導入（P1）

## 背景

「600行超ファイル 0件」を達成済みだが、550行近辺に複雑度が滞留している（12ファイルが550行超）。
既存KPIだけでは改善余地を表現しにくいため、次世代の品質指標を設計・導入する。

### 現状の数値（2026-03-01 実測）

| 指標 | 実測値 |
|---|---:|
| 550行超ファイル数 | 12 |
| 500行超ファイル数 | 31 |
| mainwindow.cpp include数 | 49 |
| mainwindow.h raw pointer 風メンバ | 42 |

### 550行超ファイル一覧（上位12件）

| 行数 | ファイル |
|---:|---|
| 596 | src/kifu/formats/kiflexer.cpp |
| 593 | src/kifu/formats/csalexer.cpp |
| 592 | src/ui/coordinators/kifudisplaycoordinator.cpp |
| 591 | src/kifu/formats/usentosfenconverter.cpp |
| 590 | src/dialogs/josekiwindow.cpp |
| 584 | src/kifu/kifuapplyservice.cpp |
| 579 | src/views/shogiview.cpp |
| 579 | src/engine/usiprotocolhandler.cpp |
| 576 | src/ui/coordinators/dialogcoordinator.cpp |
| 576 | src/dialogs/josekiwindowui.cpp |
| 561 | src/kifu/formats/kifexporter.cpp |
| 554 | src/network/csagamecoordinator.cpp |

## 作業内容

### Step 1: 新KPIテストの追加

`tests/tst_structural_kpi.cpp` に以下のテストメソッドを追加する。

#### 1-a: ファイル行数の段階的閾値

```cpp
// i) files_over_550 上限チェック
void filesOver550()
{
    const int kMaxFilesOver550 = 12;  // 実測値: 12
    // countFilesOverLines(550) を実装して使用
}

// j) files_over_500 上限チェック
void filesOver500()
{
    const int kMaxFilesOver500 = 31;  // 実測値: 31
}
```

**実装**: 既存の `fileLimits()` テスト内のファイル走査ロジックを共通関数 `countFilesOverLines(int threshold)` に抽出し、再利用する。

#### 1-b: mainwindow.cpp の include 数上限

```cpp
// k) mainwindow include 数の上限チェック
void mainWindowIncludeCount()
{
    const int kMaxIncludes = 49;  // 実測値: 49
    // #include 行をカウント（コメント行を除く）
}
```

**実装**: `countMatchingLines()` を使い、`mainwindow.cpp` の `#include` 行数をカウントする。

#### 1-c: 1関数あたり行数上限（監視用）

```cpp
// l) 長関数の監視（ゲートではなくログ出力）
void longFunctionMonitor()
{
    const int kFunctionLengthWarning = 100;  // 警告閾値
    // { と } のネスト深度で関数ブロックを推定し、
    // 100行超の関数を qDebug() でレポート
    // QVERIFY は使わない（監視のみ）
}
```

**注意**: 正確な関数境界検出は困難なため、最初は「メソッド定義開始行（`ClassName::methodName` パターン）から次のメソッド定義開始行まで」の簡易推定でよい。ゲートではなく監視（qDebug出力のみ）とする。

### Step 2: private slots セクションへの登録

新テストメソッドを `private slots:` セクションに追加する。

### Step 3: KPI出力の統合

既存の KPI ログ出力形式に合わせる:
```
KPI: files_over_550 = 12 (limit: 12)
KPI: files_over_500 = 31 (limit: 31)
KPI: mainwindow_include_count = 49 (limit: 49)
```

これにより `scripts/extract-kpi-json.sh` と CI の Step Summary に自動反映される。

### Step 4: しきい値根拠の文書化

`docs/dev/kpi-thresholds.md` を新規作成し、以下を記録する:

```markdown
# KPI しきい値根拠

## 既存KPI
| KPI | 閾値 | 設定日 | 根拠 |
|---|---|---|---|
| files_over_600 | 0 | - | 全ファイル600行以下達成 |
| lambda_connect_count | N | 2026-03-01 | Task 01 で例外登録済み |
| ... | ... | ... | ... |

## 新規KPI（2026-03-01 導入）
| KPI | 閾値 | 設定日 | 根拠 |
|---|---|---|---|
| files_over_550 | 12 | 2026-03-01 | 実測値。段階的削減目標 |
| files_over_500 | 31 | 2026-03-01 | 実測値。段階的削減目標 |
| mainwindow_include_count | 49 | 2026-03-01 | 実測値。依存削減の追跡用 |
```

### Step 5: ビルド・テスト確認

```bash
cmake --build build
cd build && ctest --output-on-failure -R tst_structural_kpi
```

- 全KPIテストが pass すること
- 新KPIの値がログに出力されること

## 完了条件

- [x] `tst_structural_kpi` に `filesOver550`, `filesOver500`, `mainWindowIncludeCount` テストが追加される
- [x] 長関数監視がログ出力される
- [x] しきい値の根拠が `docs/dev/kpi-thresholds.md` に記録される
- [x] CI の Step Summary に新KPI値が表示される
- [x] 全テスト pass
