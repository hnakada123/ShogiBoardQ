# Task 20260301-05: テスト戦略の深掘り（P2）

## 背景

テスト数（56件）とカバレッジは高いが、以下の境界で将来リスクが残る:

1. パーサ系（KIF/KI2/CSA/JKF/USI/USEN）の異常入力多様性
2. ネットワーク/プロセスの非同期エッジケース
3. UI配線の契約逸脱（接続漏れ/重複）

### 現在のパーサテスト一覧

| フォーマット | テストファイル |
|---|---|
| KIF | tst_kifconverter.cpp |
| KI2 | tst_ki2converter.cpp |
| CSA | tst_csaconverter.cpp |
| JKF | tst_jkfconverter.cpp |
| USI | tst_usiconverter.cpp |
| USEN | tst_usenconverter.cpp |
| CSA Protocol | tst_csaprotocol.cpp |
| USI Protocol | tst_usiprotocolhandler.cpp |

## 作業内容

### Phase 1: パーサ異常入力テストの追加

#### Step 1-1: 対象フォーマットの選定

代表フォーマットとして **KIF** と **CSA** を最初の対象とする（最も利用頻度が高い）。

#### Step 1-2: 異常入力テストケースの設計

以下のカテゴリで異常入力テストを追加する:

**KIF フォーマット（tst_kifconverter.cpp に追加）**:
```cpp
// 1. 空入力
void convertEmptyInput();

// 2. ヘッダのみ（手数なし）
void convertHeaderOnly();

// 3. 不正な駒名
void convertInvalidPieceName();

// 4. 不正な座標（0九、10一 等）
void convertInvalidCoordinate();

// 5. 文字コード混在（UTF-8 + Shift_JIS）
void convertMixedEncoding();

// 6. 極端に長い行（バッファオーバーフロー対策）
void convertExtremelyLongLine();

// 7. 不正な分岐記述
void convertMalformedBranch();

// 8. 途中で切れた入力（EOF）
void convertTruncatedInput();
```

**CSA フォーマット（tst_csaconverter.cpp に追加）**:
```cpp
// 1. 不正なバージョン行
void convertInvalidVersion();

// 2. 不正な手番指定
void convertInvalidTurnSpecifier();

// 3. 不正な座標フォーマット
void convertInvalidMoveFormat();

// 4. 空の棋譜
void convertEmptyGame();

// 5. 特殊結果文字列（%KACHI, %TORYO 以外）
void convertUnknownSpecialMove();

// 6. 途中で切れた入力
void convertTruncatedCsaInput();
```

#### Step 1-3: テストの実装

各テストケースでは:
1. 異常入力を文字列リテラルで定義
2. コンバータに渡す
3. **クラッシュしないこと**を最低限検証（QVERIFY でエラーなし or 適切なエラーメッセージ）
4. 可能であれば期待するエラー種別を検証

### Phase 2: USI/CSA プロトコル異常系テストの追加

#### Step 2-1: USI プロトコル異常系

`tst_usiprotocolhandler.cpp` に以下を追加:

```cpp
// 1. 不正な bestmove フォーマット
void handleInvalidBestmove();

// 2. info 行の不正な score
void handleInvalidInfoScore();

// 3. option 行の不正な型
void handleInvalidOptionType();

// 4. 予期しないコマンドシーケンス（isready 前の go 等）
void handleUnexpectedCommandSequence();
```

#### Step 2-2: CSA プロトコル異常系

`tst_csaprotocol.cpp` に以下を追加:

```cpp
// 1. 不正なログインレスポンス
void handleInvalidLoginResponse();

// 2. 接続タイムアウトシミュレーション
void handleConnectionTimeout();

// 3. 対局中の不正なサーバーメッセージ
void handleInvalidServerMessage();
```

### Phase 3: Wiring 契約テストの拡張

#### Step 3-1: 接続漏れ検出テストの設計

`tests/tst_wiring_contract.cpp` を新規作成（または既存テストに追加）:

```cpp
// MainWindow の public slots が全て connect() されているか検証
// 手法: src/ 配下の .cpp で connect() 行を検索し、
//       各 public slot 名が少なくとも1つの connect() に含まれるか確認
void allPublicSlotsConnected();

// 重複接続の検出
// 手法: 同一の sender::signal → receiver::slot ペアが複数回接続されていないか
void noDuplicateConnections();
```

**注意**: これは静的解析ベースのテスト（ソースコード文字列検索）であり、実行時テストではない。

### Step 4: ビルド・テスト確認

```bash
cmake --build build
cd build && ctest --output-on-failure
```

新規テストが全て pass すること。

## 完了条件

- [x] KIF/CSA コンバータに異常入力テストが追加される（各6件以上）
- [x] USI/CSA プロトコルに異常系テストが追加される（各3件以上）
- [x] Wiring 契約テストが追加される（接続漏れ検出）
- [x] 全テスト pass
- [x] tests/CMakeLists.txt に新規テストが登録される
