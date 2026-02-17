# デバッグ文コーディング規約

ShogiBoardQ プロジェクトにおけるデバッグ出力・ログの統一規約。
問題調査の効率化と、リリースビルドでのパフォーマンス確保を両立することを目的とする。

---

## 目次

1. [基本方針](#1-基本方針)
2. [出力フォーマット](#2-出力フォーマット)
3. [QLoggingCategory の使用](#3-qloggingcategory-の使用)
4. [リリースビルドでの無効化](#4-リリースビルドでの無効化)
5. [デバッグ文を追加すべき箇所](#5-デバッグ文を追加すべき箇所)
6. [書かないデバッグ文](#6-書かないデバッグ文)
7. [既存コードへの適用方針](#7-既存コードへの適用方針)

---

## 1. 基本方針

ログレベルを適切に使い分けることで、デバッグ時に必要な情報を得つつ、リリースビルドでは不要な出力を排除する。

| レベル | Qt マクロ | カテゴリ版 | 用途 | リリースビルド |
|--------|-----------|-----------|------|---------------|
| **Debug** | `qDebug()` | `qCDebug()` | 開発時トレース（変数値、処理フロー確認） | **無効**（`QT_NO_DEBUG_OUTPUT`） |
| **Info** | `qInfo()` | `qCInfo()` | 重要な運用情報（接続、設定変更、起動/終了） | 有効 |
| **Warning** | `qWarning()` | `qCWarning()` | リカバリ可能な問題（ファイル未検出、想定外の値） | 有効 |
| **Critical** | `qCritical()` | `qCCritical()` | 処理続行困難なエラー（必須リソース欠損等） | 有効 |

### 判断基準

- **開発者だけが必要とする情報** → `qDebug()` / `qCDebug()`
- **運用中にログとして残すべき情報** → `qInfo()` 以上

### `qCDebug` にすべき具体パターン

以下のパターンは**必ず `qCDebug()`** を使用する。リリースビルドでは不要な開発者向け情報である。

| パターン | 例 |
|---------|-----|
| 関数の入口/出口トレース | `"validateAndMove enter argMove="`, `"exit recSize="` |
| ポインタアドレスの出力 | `"sfenRecord*=" << static_cast<const void*>(ptr)` |
| 毎手呼ばれるSFEN/盤面出力 | `"setSfen:"`, `"last sfen ="` |
| 処理フロー分岐の確認 | `"HvE: forwarding to ..."`, `"not a live play mode"` |
| 変数値・内部状態のダンプ | `"effective modeNow="`, `"recSize(before)="` |
| UI操作イベントの詳細 | `"onMoveRequested from= to="` |
| タイマー遅延の診断 | `"updateClock elapsed=66ms"` |

### `qCInfo` にすべき具体パターン

以下のパターンのみ `qCInfo()` を使用する。アプリケーション起動時や運用中に**1回きりまたは低頻度**で出力される重要情報。

| パターン | 例 |
|---------|-----|
| アプリ起動時の設定情報 | `"Language setting:"`, `"Application dir:"` |
| エンジン/サーバーの起動・終了 | `"エンジン起動完了:"`, `"CSA接続"` |
| 解析モードの開始・終了 | `"解析モード開始: MultiPV ="` |
| ファイル読み込み成功（ファイル名） | `"棋譜読み込み完了:"` |

---

## 2. 出力フォーマット

`main.cpp` の `messageHandler` が自動的に統一フォーマットで出力する。呼び出し側でフォーマットを意識する必要はない。

### 出力例

```
2024-01-15 10:30:45.123 [DEBUG] [shogi.clock] shogiclock.cpp:42 (setPlayerTimes) 先手残り時間: 300000ms
2024-01-15 10:30:45.456 [INFO] csaclient.cpp:128 (connectToServer) CSAサーバーに接続しました
2024-01-15 10:30:46.789 [WARN] kifuioservice.cpp:55 (loadFromFile) ファイルが見つかりません: /path/to/file.kif
```

### フォーマット構成

```
<タイムスタンプ> [<レベル>] [<カテゴリ>] <ファイル名>:<行番号> (<関数名>) <メッセージ>
```

| 要素 | 説明 |
|------|------|
| タイムスタンプ | `yyyy-MM-dd hh:mm:ss.zzz` 形式 |
| レベル | `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL` |
| カテゴリ | `QLoggingCategory` 使用時のみ表示（`default` は省略） |
| ファイル名:行番号 | デバッグビルドでのみ利用可能（`QMessageLogContext`） |
| 関数名 | デバッグビルドでのみ利用可能 |

> **注意**: ソース位置情報（ファイル名、行番号、関数名）はデバッグビルドでのみ `QMessageLogContext` に含まれる。リリースビルドではこれらは空になる。

### メッセージの書き方

```cpp
// 良い例: 状態と値が分かる
qDebug() << "先手残り時間:" << remainingMs << "ms";
qCDebug(lcEngine) << "USI送信:" << command;

// 悪い例: 情報不足
qDebug() << "ここに来た";
qDebug() << "OK";
```

---

## 3. QLoggingCategory の使用

モジュールごとに `QLoggingCategory` を定義し、カテゴリ別にログの有効/無効を制御可能にする。

### 命名規則

```
shogi.<module>
```

| カテゴリ名 | 対象モジュール |
|-----------|--------------|
| `shogi.engine` | USIエンジン連携 (`src/engine/`) |
| `shogi.network` | CSA通信対局 (`src/network/`) |
| `shogi.kifu` | 棋譜関連 (`src/kifu/`) |
| `shogi.analysis` | 解析機能 (`src/analysis/`) |
| `shogi.game` | ゲーム進行 (`src/game/`) |
| `shogi.board` | 盤面操作 (`src/board/`) |
| `shogi.navigation` | ナビゲーション (`src/navigation/`) |
| `shogi.core` | 盤面・駒・合法手判定 (`src/core/` ※clock除く) |
| `shogi.clock` | 対局時計 (`src/core/shogiclock.*`) |
| `shogi.view` | 盤面描画 (`src/views/`) |
| `shogi.ui` | UI層全般 (`src/ui/`) |

### 定義方法

**ヘッダーファイル（`.h`）で宣言:**

```cpp
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcEngine)
```

**ソースファイル（`.cpp`）で定義:**

```cpp
Q_LOGGING_CATEGORY(lcEngine, "shogi.engine")
```

### 使用方法

```cpp
qCDebug(lcEngine) << "USI送信:" << command;
qCInfo(lcEngine) << "エンジン起動完了:" << engineName;
qCWarning(lcEngine) << "エンジン応答タイムアウト";
```

### カテゴリの定義場所

- モジュール内の**主要なソースファイル**に `Q_LOGGING_CATEGORY` を1つ定義する
- 同モジュール内の他ファイルからは `Q_DECLARE_LOGGING_CATEGORY` で参照する
- 1モジュール1カテゴリを基本とする（サブカテゴリが必要な場合は `shogi.engine.protocol` のように拡張可）

### 実行時の制御

`QT_LOGGING_RULES` 環境変数でカテゴリごとに有効/無効を切り替えられる:

```bash
# エンジン関連のデバッグログのみ有効化
QT_LOGGING_RULES="shogi.engine.debug=true" ./ShogiBoardQ

# 全カテゴリのデバッグログを有効化
QT_LOGGING_RULES="shogi.*.debug=true" ./ShogiBoardQ

# 特定カテゴリを無効化
QT_LOGGING_RULES="shogi.clock.debug=false" ./ShogiBoardQ
```

---

## 4. リリースビルドでの無効化

### CMake 設定

`CMakeLists.txt` に以下を設定済み:

```cmake
# リリースビルドではqDebug()出力を無効化
target_compile_definitions(ShogiBoardQ PRIVATE
    $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
)
```

### 動作

| ビルドタイプ | `qDebug()` / `qCDebug()` | `qInfo()` 以上 |
|-------------|--------------------------|----------------|
| Debug | **出力される** | 出力される |
| Release / RelWithDebInfo | **出力されない**（コンパイル時に除去） | 出力される |

### 注意点

- `QT_NO_DEBUG_OUTPUT` は `qDebug()` と `qCDebug()` の両方を無効化する
- `qInfo()`, `qWarning()`, `qCritical()` はリリースビルドでも出力される
- リリースビルドで残すべき情報は `qInfo()` 以上を使うこと

---

## 5. デバッグ文を追加すべき箇所

以下の箇所にはデバッグ出力を積極的に配置する。

### 外部通信

USIコマンド送受信、CSAプロトコルの通信内容。問題発生時に通信ログが最重要の手がかりとなる。

```cpp
qCDebug(lcEngine) << "USI送信:" << command;
qCDebug(lcEngine) << "USI受信:" << line;
qCDebug(lcNetwork) << "CSA送信:" << message;
```

### 状態遷移

ゲーム状態、解析モード、接続状態などの変更。状態遷移のタイミングと順序の確認に必要。

```cpp
// 頻繁に発生する内部状態変更 → qCDebug
qCDebug(lcGame) << "状態遷移:" << oldState << "->" << newState;

// ユーザー操作によるモード開始/終了（低頻度・運用情報） → qCInfo
qCInfo(lcAnalysis) << "解析モード開始: MultiPV =" << multiPv;
```

> **注意**: 迷った場合は `qCDebug` を選択する。`qCInfo` はリリースビルドで出力されるため、
> 高頻度に呼ばれる可能性がある箇所では必ず `qCDebug` を使用すること。

### エラーハンドリング

ファイルI/O失敗、パース失敗、想定外の入力。`qWarning()` 以上を使用する。

```cpp
qCWarning(lcKifu) << "棋譜ファイル読み込み失敗:" << filePath << error;
qCWarning(lcEngine) << "不正なUSI応答:" << line;
```

### パフォーマンス計測

棋譜読み込み等の重い処理の所要時間。ボトルネックの特定に有用。

```cpp
QElapsedTimer timer;
timer.start();
// ... 重い処理 ...
qCDebug(lcKifu) << "棋譜読み込み完了:" << timer.elapsed() << "ms";
```

### 初期化・終了処理

アプリケーション起動時の設定読み込み、エンジン接続、リソース解放。起動失敗の原因特定に必要。

```cpp
qCInfo(lcEngine) << "エンジン起動:" << enginePath;
qCInfo(lcEngine) << "エンジン終了:" << engineName;
```

---

## 6. 書かないデバッグ文

以下のデバッグ出力は**記述しない**。ログの可読性低下やパフォーマンス劣化の原因となる。

| 書かないもの | 理由 |
|-------------|------|
| getter/setter の単純呼び出し | 呼び出し頻度が高く、ログが肥大化する |
| 毎フレーム/毎tick 呼ばれる高頻度メソッド | ログファイルが膨大になり、他の情報が埋もれる |
| 個人情報・セキュリティ情報 | ログファイルへの漏洩リスク |
| 意味のないメッセージ（`"ここに来た"`, `"OK"`） | デバッグ情報として無価値 |
| 大量データのダンプ（盤面全体のSFEN毎手出力等） | 必要時のみ一時的に追加し、コミットしない |

---

## 7. 既存コードへの適用方針

全ファイルを一括変換するのではなく、以下のルールで段階的に移行する。

### 移行ルール

1. **新規ファイル**: 本ガイドに完全に準拠する。`QLoggingCategory` を定義し、`qCDebug()` 等を使用する
2. **修正するファイル**: 変更した関数内のデバッグ文を本ガイドに合わせる
3. **既存の `qDebug()` + プレフィックス**: 動作に問題がなければそのまま残す。カテゴリへの移行は当該コード修正時に行う
4. **アドホックなプレフィックス**: `[CSA]`, `[KLC]` 等の手動プレフィックスは、`QLoggingCategory` 移行時に除去する（カテゴリ名が同等の役割を果たすため）

### 移行例

```cpp
// 移行前（アドホックなプレフィックス）
qDebug() << "[CSA] 接続開始:" << host << port;

// 移行後（QLoggingCategory使用）
qCDebug(lcNetwork) << "接続開始:" << host << port;
```

### 既存のQLoggingCategory

以下のファイルでは既に `QLoggingCategory` が使用されている:

- `src/core/shogiboard.cpp` — `shogi.core`（`shogiboard.cpp`, `fastmovevalidator.cpp`, `shogiutils.cpp` で使用）
- `src/core/shogiclock.cpp` — `shogi.clock`
- `src/views/shogiview.cpp` — `shogi.view`

これらは本ガイドの命名規則に準拠しており、変更不要である。
