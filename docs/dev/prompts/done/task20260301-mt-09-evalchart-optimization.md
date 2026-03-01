# Task 20260301-mt-09: 評価値グラフ描画最適化（P2）

## 背景

Qt Charts はアンチエイリアシング有効時にCPUレンダリングが重い。300手以上の対局で解析結果を逐次追加すると、各 `appendScore()` 呼び出しごとに再描画が発生し、フレームレートが低下する。マルチスレッド化ではなく、メインスレッド内でのバッチ更新と描画スロットリングで対応する。

### 対象ファイル

| ファイル | メソッド | 処理内容 |
|---------|---------|---------|
| `src/widgets/evaluationchartwidget.cpp` | `appendScore*()` / `update()` | Qt Charts のCPU描画 |

## 作業内容

### Step 1: 現状把握

`src/widgets/evaluationchartwidget.h/.cpp` を読み、以下を確認する:

- `appendScore()` 系メソッドの種類と呼び出し頻度
- `QLineSeries` / `QChart` / `QChartView` の使い方
- 現在の描画更新タイミング（毎回 vs バッチ）
- アンチエイリアシングの設定

### Step 2: バッチ更新の実装

ペンディングバッファとフラッシュタイマーを導入する:

```cpp
// evaluationchartwidget.h に追加
struct PendingScore {
    int ply;
    double score;
    // 必要に応じて他のフィールド
};

QList<PendingScore> m_pendingScores;
QTimer* m_flushTimer = nullptr;
```

```cpp
// evaluationchartwidget.cpp

void EvaluationChartWidget::initFlushTimer()
{
    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(true);
    connect(m_flushTimer, &QTimer::timeout,
            this, &EvaluationChartWidget::flushPendingScores);
}

void EvaluationChartWidget::appendScoreBuffered(int ply, double score)
{
    m_pendingScores.append({ply, score});

    if (!m_flushTimer->isActive()) {
        m_flushTimer->start(100);  // 100ms間隔でフラッシュ
    }
}

void EvaluationChartWidget::flushPendingScores()
{
    if (m_pendingScores.isEmpty())
        return;

    // 描画抑制
    m_chartView->setUpdatesEnabled(false);

    for (const auto& item : std::as_const(m_pendingScores)) {
        // 既存の appendScore ロジックを呼ぶ（1点追加）
        m_series->append(item.ply, item.score);
    }

    // 一括再描画
    m_chartView->setUpdatesEnabled(true);
    m_pendingScores.clear();
}
```

### Step 3: 既存の appendScore メソッドの移行

既存の `appendScore*()` メソッドの呼び出し元を確認し、バッファ版に切り替える:

1. 一括解析からの呼び出し → `appendScoreBuffered()` に変更
2. リアルタイム解析（検討モード）からの呼び出し → 既存の即時版を維持（1手ずつの表示が重要なため）
3. 棋譜ロード時の全データプロット → `appendScoreBuffered()` + 即時 `flushPendingScores()` の呼び出し

### Step 4: 全データクリア＋再プロットの最適化

棋譜を開いた時やナビゲーション時に全データを再プロットするケースがある場合:

```cpp
void EvaluationChartWidget::replaceAllScores(const QList<QPointF>& points)
{
    m_chartView->setUpdatesEnabled(false);
    m_series->clear();
    m_series->replace(points);  // QLineSeries::replace() で一括置換
    m_chartView->setUpdatesEnabled(true);
}
```

`QLineSeries::replace()` は個別の `append()` より大幅に高速。

### Step 5: アンチエイリアシングの条件付き無効化（オプション）

300手以上のデータがある場合にアンチエイリアシングを自動で無効化する:

```cpp
void EvaluationChartWidget::updateAntialiasing()
{
    const bool heavy = (m_series->count() > 300);
    m_chartView->setRenderHint(QPainter::Antialiasing, !heavy);
}
```

この最適化はユーザー体験に影響するため、オプションまたは設定で切り替え可能にする。

### Step 6: デストラクタでのフラッシュタイマー停止

```cpp
EvaluationChartWidget::~EvaluationChartWidget()
{
    if (m_flushTimer && m_flushTimer->isActive()) {
        m_flushTimer->stop();
    }
}
```

### Step 7: ビルド・テスト確認

```bash
cmake --build build
cd build && ctest --output-on-failure
```

**手動テスト項目**:
1. 解析結果がグラフに正しく表示されること
2. 一括解析中のグラフ更新がスムーズなこと（100ms間隔のバッチ更新）
3. 検討モードでリアルタイムのグラフ更新が機能すること
4. 棋譜切替時にグラフが正しくクリア＋再プロットされること
5. 長い棋譜（300手以上）でのパフォーマンスが改善されていること

## 完了条件

- [ ] バッチ更新（100msフラッシュ）が実装されている
- [ ] `setUpdatesEnabled(false/true)` で描画が抑制/一括更新される
- [ ] `QLineSeries::replace()` による一括置換が使用されている
- [ ] 既存の表示機能が損なわれていない
- [ ] 全テスト pass
