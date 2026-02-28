# Task 17: プロトコルエラー方針統一（テーマF: 棋譜/通信仕様境界整理）

## 目的

USI/CSAプロトコルのエラーコード・ログ出力方針を統一し、デバッグ・障害分析を容易にする。

## 背景

- USI（`usiprotocolhandler.cpp`）とCSA（`csaclient.cpp`, `csagamecoordinator.cpp`）でエラー処理方針がバラバラ
- ログレベル・出力形式が統一されていない
- テーマF（棋譜/通信仕様境界整理）の補助作業

## 対象ファイル

- `src/engine/usiprotocolhandler.cpp`（902行）
- `src/engine/usi.cpp`（1084行）
- `src/network/csaclient.cpp`（693行）
- `src/network/csagamecoordinator.cpp`（798行）

## 事前調査

### Step 1: 現在のエラー処理パターン

```bash
# USI側のエラー処理
rg "qWarning|qCritical|qDebug|error|Error" src/engine/usiprotocolhandler.cpp -n | head -20
rg "qWarning|qCritical|qDebug|error|Error" src/engine/usi.cpp -n | head -20

# CSA側のエラー処理
rg "qWarning|qCritical|qDebug|error|Error" src/network/csaclient.cpp -n | head -20
rg "qWarning|qCritical|qDebug|error|Error" src/network/csagamecoordinator.cpp -n | head -20

# ロギングカテゴリ
rg "Q_LOGGING_CATEGORY|qCDebug|qCWarning|qCCritical" src/engine/ src/network/ -n
```

### Step 2: エラー種別の洗い出し

1. **プロトコル違反**: 予期しないメッセージ
2. **通信エラー**: 接続断、タイムアウト
3. **内部エラー**: 不正な状態遷移
4. **ユーザー起因**: 不正な入力・設定

## 実装手順

### Step 3: 統一エラー方針の策定

以下の方針を策定し `docs/dev/protocol-error-policy.md` に記録:

1. **ログカテゴリ**: `lcUsi`（USI用）、`lcCsa`（CSA用）をQ_LOGGING_CATEGORYで統一
2. **ログレベル**:
   - `qCDebug`: プロトコルメッセージの送受信（デバッグ用）
   - `qCWarning`: 予期しないメッセージ、回復可能なエラー
   - `qCCritical`: 通信断、回復不能なエラー
3. **エラーコード**: 列挙型で統一（USI/CSA共通の基底 + プロトコル固有）
4. **ユーザー通知**: ErrorBus経由で統一

### Step 4: USI側のエラー処理統一

1. `qDebug()` → `qCDebug(lcUsi)` に統一
2. エラーメッセージのフォーマットを統一
3. エラー時のシグナル発行を標準化

### Step 5: CSA側のエラー処理統一

1. `qDebug()` → `qCDebug(lcCsa)` に統一
2. エラーメッセージのフォーマットをUSIと揃える
3. エラー時のシグナル発行を標準化

### Step 6: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 完了条件

- [ ] エラー方針文書が `docs/dev/protocol-error-policy.md` に記録されている
- [ ] USI/CSAのログ出力がカテゴリ化されている
- [ ] エラーメッセージのフォーマットが統一されている
- [ ] 全テスト通過

## KPI変化目標

- プロトコルエラー処理の統一率: 100%

## 注意事項

- `debug-logging-guidelines.md` のルールに従う（`qDebug()` はデバッグビルド専用）
- 既存のエラー通知フロー（ErrorBus等）を壊さない
- ログカテゴリが既に定義されている場合は再利用する
