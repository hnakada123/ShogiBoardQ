# Task 20260301-mt-10: 詰将棋局面生成の並列化（P2）

## 背景

詰将棋局面生成では、駒のトリミング候補列挙 + 各候補の「詰みか否か」チェックを繰り返す。一回のループは短いが、候補数が多いとメインスレッドを占有し続ける。エンジンとのやり取りは非同期だが、局面生成・SFEN文字列操作・合法性チェックが同期実行されている。

### 対象ファイル

| ファイル | 行 | メソッド | 処理内容 |
|---------|-----|---------|---------|
| `src/analysis/tsumeshogipositiongenerator.cpp` | 13, 17, 40, 62 | 多重ループ | 駒トリミング候補生成 |
| `src/analysis/tsumeshogipositiongenerator.cpp` | 275 | 合法性チェック | 各候補局面の検証 |

## 作業内容

### Step 1: 現状把握

以下のファイルを読み、詰将棋局面生成の全体フローを理解する:

- `src/analysis/tsumeshogipositiongenerator.h/.cpp`
- 呼び出し元（`TsumeSearchFlowController` 等）

確認ポイント:
- 入力: 元の局面（SFEN文字列 or ShogiBoard 参照）
- 出力: トリミングされた詰将棋局面のリスト
- 処理の流れ: 候補生成 → 合法性チェック → エンジンによる詰みチェック → 結果収集
- `ShogiBoard` への依存（参照 or コピー）
- `MoveValidator` の使用箇所

### Step 2: CPU集約部分の特定

プロファイリングポイントを追加し、ボトルネックを特定する:

1. **候補生成ループ**: 駒の組み合わせを列挙する部分
2. **合法性チェック**: `MoveValidator` による各候補局面の検証
3. **SFEN文字列操作**: 局面文字列の生成

ボトルネックがエンジン待機（I/O）にある場合は、並列化の効果は限定的。
CPU計算（候補生成 + 合法性チェック）にある場合は並列化が有効。

### Step 3: 局面生成のスレッドプール化

`QThreadPool` + `QtConcurrent::run` で候補局面の生成を並列化する:

```cpp
#include <QtConcurrent>

struct TrimCandidate {
    QString sfen;
    // 必要な属性
};

QList<TrimCandidate> TsumeshogiPositionGenerator::generateCandidatesParallel(
    const QString& baseSfen,
    const QList<PieceRemoval>& removalPatterns)
{
    // 各除去パターンを並列に評価
    QFuture<QList<TrimCandidate>> future = QtConcurrent::mappedReduced(
        removalPatterns,
        // map: 各パターンの候補生成（ワーカースレッド）
        [baseSfen](const PieceRemoval& removal) -> QList<TrimCandidate> {
            // ShogiBoard のコピーを作成して操作
            ShogiBoard board;
            board.fromSfen(baseSfen);
            // 駒を除去して合法性チェック
            // ...
            return candidates;
        },
        // reduce: 結果の集約（メインスレッド）
        [](QList<TrimCandidate>& result, const QList<TrimCandidate>& partial) {
            result.append(partial);
        }
    );

    return future.result();
}
```

**注意**: `ShogiBoard` のコピーを各ワーカーで独立に使用する（共有しない）。`MoveValidator` も同様にワーカーごとに独立インスタンスまたは static メソッドを使用する。

### Step 4: エンジンによる詰みチェックの並列化（オプション）

複数のエンジンインスタンスを並行して実行できる場合:

1. エンジンプール（2〜4エンジン）を作成
2. 各候補局面を空きエンジンに割り当て
3. 結果を集約

**注意**: これは大幅な複雑化を伴うため、Step 3 のCPU並列化だけで十分な効果が得られるか先に確認する。

### Step 5: キャンセルサポート

- `CancelFlag` (`std::shared_ptr<std::atomic_bool>`) を候補生成に渡す
- ワーカースレッド内でフラグを定期的にチェック
- キャンセル時は部分結果を返す

### Step 6: 進捗通知

- `QtConcurrent::mappedReduced` の `QFutureWatcher::progressValueChanged` で進捗通知
- メインスレッドのプログレスダイアログを更新

### Step 7: ShogiBoard のスレッド安全性確認

`ShogiBoard` を複数スレッドから**独立インスタンスとして**使用する場合は安全。
同一インスタンスを共有する場合は `QMutex` が必要だが、コピーを使う設計にする。

確認ポイント:
- `ShogiBoard::fromSfen()` が static データにアクセスしないか
- `MoveValidator` が static データにアクセスしないか
- `ShogiUtils` のユーティリティ関数がスレッドセーフか

### Step 8: ビルド・テスト確認

```bash
cmake --build build
cd build && ctest --output-on-failure
```

**手動テスト項目**:
1. 詰将棋局面生成が完了すること
2. 生成結果が正しいこと（シングルスレッド版と同じ結果）
3. 生成中にUI操作が可能なこと
4. キャンセルが機能すること
5. マルチコア環境で生成速度が向上すること

## 注意事項

- `QtConcurrent::mappedReduced` のラムダ使用は `connect()` とは異なりCLAUDE.md ルールの対象外（`connect()` ではないため）
- `ShogiBoard` のコピーコストが高い場合は、SFEN文字列からの再構築に切り替え
- スレッド数は `QThreadPool::globalInstance()->maxThreadCount()` に任せる（CPU コア数に自動調整）

## 完了条件

- [ ] 候補生成ループがスレッドプールで並列実行される
- [ ] `ShogiBoard` は各ワーカーで独立インスタンスを使用
- [ ] キャンセルサポートがある
- [ ] 生成結果がシングルスレッド版と一致する
- [ ] 全テスト pass
