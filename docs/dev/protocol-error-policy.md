# プロトコルエラー方針（USI / CSA 統一ガイドライン）

USI（エンジン連携）とCSA（通信対局）のエラー処理・ログ出力を統一するための方針。
`debug-logging-guidelines.md` を上位規約として、プロトコル固有の方針をここに定義する。

---

## 1. ログカテゴリ

| カテゴリ | 対象 | 定義場所 |
|---------|------|---------|
| `lcEngine` (`shogi.engine`) | USI エンジン通信 | `logcategories.cpp` |
| `lcNetwork` (`shogi.network`) | CSA 通信対局 | `logcategories.cpp` |

両カテゴリは `logcategories.h` で `Q_DECLARE_LOGGING_CATEGORY` され、
全ソースから `#include "logcategories.h"` でインクルードできる。

---

## 2. ログレベル使い分け

| レベル | マクロ | 用途 | リリースビルド | 具体例 |
|--------|--------|------|---------------|--------|
| **Debug** | `qCDebug(lcEngine)` / `qCDebug(lcNetwork)` | プロトコルメッセージの送受信トレース、内部状態ダンプ | 無効 | `"info行受信:"`, `"Sent:"`, `"Recv:"`, `"processGameMessage:"` |
| **Info** | `qCInfo(lcEngine)` / `qCInfo(lcNetwork)` | 接続・切断・ログイン成功などの運用イベント | 有効 | `"Connected to server"`, `"Login successful"`, `"エンジン起動完了"` |
| **Warning** | `qCWarning(lcEngine)` / `qCWarning(lcNetwork)` | 回復可能なエラー、予期しないメッセージ | 有効 | `"Invalid time value"`, `"Socket error:"`, `"手リストが空"` |
| **Critical** | `qCCritical(lcEngine)` / `qCCritical(lcNetwork)` | 回復不能なエラー（プロセスクラッシュ、起動失敗） | 有効 | `"エンジンプロセスがクラッシュ"`, `"エンジン起動に失敗"` |

### 判断基準

- **qCDebug**: 毎手・毎行発生する通信ログ、内部状態の確認用
- **qCInfo**: セッション中に1回程度の重要イベント（接続、ログイン、エンジン起動）
- **qCWarning**: 処理は続行できるが想定外の状況（不正な入力値、フォールバック発生）
- **qCCritical**: 当該機能が使用不能になるエラー（プロセスクラッシュ、起動失敗）

---

## 3. エラー通知方式

### シグナルによる伝播（推奨パターン）

エラーはシグナルチェーンで上位に伝播する。各レイヤで `errorOccurred(const QString& message)` シグナルを統一的に使用する。

```
USI:  UsiProtocolHandler::errorOccurred → Usi::errorOccurred → 呼び出し元
      EngineProcessManager::processError → Usi::onProcessError → Usi::errorOccurred

CSA:  CsaClient::errorOccurred → CsaGameCoordinator::onClientError → CsaGameCoordinator::errorOccurred
```

### ErrorBus

`ErrorBus::instance().postError(message)` はアプリケーション全体のエラー通知用。
プロトコル層では個別シグナルが上位に接続されているため、ErrorBus は使用しない。

---

## 4. エラー種別と対応

### USI プロトコル

| エラー種別 | ログレベル | シグナル | 例 |
|-----------|-----------|---------|-----|
| エンジン起動失敗 | `qCCritical` | `processError` | ファイル未存在、起動タイムアウト |
| エンジンクラッシュ | `qCCritical` | `processError` | プロセス異常終了 |
| usiok/readyok タイムアウト | `qCWarning` | `errorOccurred` | 初期化シーケンスのタイムアウト |
| bestmove フォーマット不正 | `qCWarning` | `errorOccurred` | パース失敗 |
| 空の手リスト | `qCWarning` | ー（フォールバック実行） | `go infinite` にフォールバック |
| コマンド送信失敗 | `qCWarning` | ー | プロセス未準備 |

### CSA プロトコル

| エラー種別 | ログレベル | シグナル | 例 |
|-----------|-----------|---------|-----|
| 接続タイムアウト | `qCWarning` | `errorOccurred` | サーバー応答なし |
| ソケットエラー | `qCWarning` | `errorOccurred` | 接続断、リモート切断 |
| メッセージ送信失敗 | `qCWarning` | `errorOccurred` | ソケット書き込みエラー |
| ログイン失敗 | `qCWarning` | `loginFailed` | 認証エラー |
| 不正な指し手座標 | `qCWarning` | `errorOccurred` | 座標範囲外 |
| 局面パース失敗 | `qCWarning` | ー（平手にフォールバック） | CSA局面行の解析失敗 |
| 不正な時間値 | `qCWarning` | ー | toInt() 失敗 |

---

## 5. プロトコルメッセージのログ書式

### USI 送受信

```cpp
// EngineProcessManager が統一的にログ出力
qCDebug(lcEngine).nospace() << logPrefix() << " 送信: " << command;
qCDebug(lcEngine).nospace() << logPrefix() << " 受信: " << line;
```

### CSA 送受信

```cpp
// CsaClient が統一的にログ出力
qCDebug(lcNetwork).noquote() << "Sent:" << message;
qCDebug(lcNetwork).noquote() << "Recv:" << line;
```

### 書式の違い

- **USI**: `.nospace()` + `logPrefix()` で `[E1/P1] エンジン名 送信: command` 形式
- **CSA**: `.noquote()` でプロトコルメッセージをそのまま出力

この違いはプロトコルの性質に由来する（USIは複数エンジン管理のためプレフィックス付加が必要）。

---

## 6. Warning メッセージの書式ガイドライン

```cpp
// 良い例: コンテキスト + 問題 + 値
qCWarning(lcEngine) << "sendGoSearchmoves: 手リストが空、go infiniteを送信";
qCWarning(lcNetwork) << "Invalid CSA move coordinates: toFile=" << toFile << "toRank=" << toRank << "move=" << move;

// 避ける例: コンテキスト不足
qCWarning(lcEngine) << "empty";
qCWarning(lcNetwork) << "error";
```

### 必須要素

1. **発生箇所**: メソッド名またはコンテキスト（例: `"sendGoSearchmoves:"`, `"boardToCSA:"`）
2. **問題の内容**: 何が想定外だったか（例: `"手リストが空"`, `"Invalid coordinates"`）
3. **関連する値**: デバッグに必要なデータ（例: 受信した指し手文字列、座標値）
