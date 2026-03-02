# Task 12: USI/CSA 異常系シナリオ拡張（ISSUE-020 / P2）

## 概要

USI/CSA プロトコルハンドラの異常系テストを拡充し、非同期境界の回帰耐性を高める。

## 現状

| テストファイル | テストメソッド数 | 行数 |
|---|---|---|
| `tests/tst_usiprotocolhandler.cpp` | 39 | 706 |
| `tests/tst_csaprotocol.cpp` | 39 | 729 |

現在のテストは主に正常系パース（infoLine, bestmove, gameSummary 等）と基本的な異常入力をカバー。

## 手順

### Step 1: 不足シナリオの洗い出し

以下の異常系シナリオをカバーする:

**USI プロトコル:**
1. **タイムアウト**: `readyok` / `bestmove` が一定時間内に返らない
2. **部分応答**: `info` 行が途中で切れる（incomplete line）
3. **不正な応答順序**: `bestmove` の前に `readyok` が来ない
4. **大量出力**: `info` 行が非常に多い（バッファ溢れ的シナリオ）
5. **エンジンクラッシュ**: プロセスが突然終了

**CSA プロトコル:**
1. **接続切断**: ゲーム中にサーバーとの接続が切れる
2. **再接続**: 切断後の再接続フロー
3. **不正な手**: サーバーから不正なCSA形式の手が送られる
4. **時間切れ**: サーバーからの TIME_UP 通知
5. **部分受信**: パケット分割による不完全な行

### Step 2: テストケースの実装

1. 各シナリオのテストメソッドを追加する
2. テストスタブ/モックを使い、異常応答をシミュレートする
3. ログ/通知の期待値を `QCOMPARE` / `QVERIFY` で検証する

**テストパターン例:**
```cpp
void tst_UsiProtocolHandler::timeout_bestmove_emitsError()
{
    // Setup: bestmove を返さないエンジン応答
    // Act: 一定時間待機
    // Assert: タイムアウトシグナルが発火
}
```

### Step 3: ビルド・テスト

1. `cmake --build build` でビルド確認
2. テスト実行:
   ```bash
   ctest --test-dir build -R "tst_usiprotocolhandler|tst_csaprotocol" --output-on-failure
   ```

## 完了条件

- 異常系ケースの自動検知範囲が拡大する
- 新規テストが全パスする

## 注意事項

- タイムアウト系テストは実行時間に影響するため、短いタイムアウト値を使用する
- 非同期テストには `QSignalSpy` + `waitForSignal` を活用する
- ISSUE-005 完了後に着手する
