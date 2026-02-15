# コメントスタイルガイド

ShogiBoardQ プロジェクトにおけるソースコード・コメントの統一規約。
新規にコードを読む人が素早く構造と意図を把握できることを最優先とする。

---

## 目次

1. [基本方針](#1-基本方針)
2. [ファイルヘッダー](#2-ファイルヘッダー)
3. [クラスコメント](#3-クラスコメント)
4. [メソッド・関数コメント](#4-メソッド関数コメント)
5. [シグナル・スロット](#5-シグナルスロット)
6. [メンバー変数](#6-メンバー変数)
7. [列挙型・構造体](#7-列挙型構造体)
8. [セクション区切り](#8-セクション区切り)
9. [インラインコメント（実装部）](#9-インラインコメント実装部)
10. [書かないコメント](#10-書かないコメント)
11. [既存コードへの適用方針](#11-既存コードへの適用方針)

---

## 1. 基本方針

| 項目 | 規約 |
|------|------|
| **言語** | 日本語（プロジェクト標準） |
| **記法** | Doxygen 形式（`/** */`, `///`, `///<`） |
| **ドキュメント対象** | ヘッダーファイル（`.h`）に記述。`.cpp` には実装上の補足のみ |
| **原則** | **Why（なぜ）を書く。What（何）はコードが語る** |

### コメント記法の使い分け

```
/** ... */   → クラス、メソッド、構造体の Doxygen ドキュメント（複数行）
///          → 1行で収まる Doxygen ドキュメント
///<         → enum値・メンバー変数の行末コメント
// ---       → セクション区切り内の小見出し
```

> **禁止**: `/* ... */` を通常コメントに使わない（Doxygen ブロックとの混同を防ぐ）。

---

## 2. ファイルヘッダー

各 `.h` / `.cpp` ファイルの先頭（インクルードガードの直後、`#include` の前）に配置する。

### ヘッダーファイル（`.h`）

```cpp
#ifndef CSACLIENT_H
#define CSACLIENT_H

/// @file csaclient.h
/// @brief CSAプロトコルクライアントクラスの定義

#include <QObject>
```

### ソースファイル（`.cpp`）

```cpp
/// @file csaclient.cpp
/// @brief CSAプロトコルクライアントクラスの実装

#include "csaclient.h"
```

**ポイント**:
- `@file` と `@brief` の2行のみ。簡潔に保つ
- 著作権表記やライセンス情報は不要（このプロジェクトでは使用しない）
- `@brief` にはそのファイルの主要クラス名や役割を1行で記述する

---

## 3. クラスコメント

クラス宣言の直前に Doxygen ブロックを記述する。
新規参加者が「このクラスは何をするのか」を即座に把握できることが目的。

### 基本形（小〜中規模クラス）

```cpp
/**
 * @brief 盤面の描画と入力操作を担当するGraphics Viewクラス
 *
 * Qt Graphics View Frameworkを使用して将棋盤を描画し、
 * マウス操作による駒の選択・移動をハンドリングする。
 */
class ShogiView : public QGraphicsView
```

### 拡張形（設計意図の説明が必要な場合）

クラスの責務が複雑な場合や、アーキテクチャ上の位置付けを明示したい場合に使用する。

```cpp
/**
 * @brief USIプロトコル通信を管理するファサードクラス
 *
 * 内部実装を以下のクラスに委譲する:
 * - EngineProcessManager: QProcessの管理
 * - UsiProtocolHandler: USIプロトコルの送受信
 * - ThinkingInfoPresenter: GUIへの表示更新
 *
 * @note 既存コードとの後方互換性を維持するためのインターフェース層。
 */
class Usi : public QObject
```

### Deps 構造体を持つクラス

このプロジェクト独自の依存注入パターンを使うクラスでは、Deps の意味を補足する。

```cpp
/**
 * @brief 検討モードのシグナル/スロット接続を管理するクラス
 *
 * MainWindowから検討関連の接続ロジックを分離。
 * Deps構造体で必要なオブジェクトを受け取り、updateDeps()で再設定可能。
 */
class ConsiderationWiring : public QObject
```

---

## 4. メソッド・関数コメント

### 基本ルール

- **公開メソッド（public）**: ヘッダーファイルに Doxygen コメントを記述する
- **保護・非公開メソッド（protected/private）**: 意図が名前から明らかでない場合のみ記述する
- **`.cpp` 側**: ヘッダーのコメントと重複させない。実装上の注意点がある場合のみ記述する

### 1行で済む場合

```cpp
/// 現在の局面をSFEN文字列で返す
QString toSfen() const;

/// 持ち駒を駒台に追加する
void incrementPieceOnStand(QChar piece);
```

### パラメータや戻り値の説明が必要な場合

```cpp
/**
 * @brief SFEN文字列で盤面全体を更新する
 * @param sfenStr 盤面＋駒台＋手番を含むSFEN文字列
 *
 * 文法チェックを行い、不正な場合は盤面を変更しない。
 */
void setSfen(const QString& sfenStr);
```

### 戻り値が重要な場合

```cpp
/**
 * @brief 指定マスの駒が移動可能か判定する
 * @param file 筋（1〜9）
 * @param rank 段（1〜9）
 * @return 移動可能なら true
 */
bool canMovePiece(int file, int rank) const;
```

### 記述不要なケース

名前から意図が自明なメソッドにはコメントを付けない。

```cpp
// コメント不要な例
int ranks() const { return m_ranks; }
int files() const { return m_files; }
bool isRunning() const;
void stop();
```

---

## 5. シグナル・スロット

Qt のシグナル/スロットは、クラス間の接続関係を示す重要な公開インターフェース。
**誰が emit し、誰が受け取るか** をコメントで明示する。

### シグナル

```cpp
signals:
    /// エンジンにUSIコマンドの送信を要求する（Coordinator → Usi）
    void requestSendUsiCommand(const QString& line);

    /// 解析の進捗を通知する（scoreCp未設定時はINT_MIN、mate=0は詰みなし）
    void analysisProgress(int ply, int depth, int scoreCp, int mate,
                          const QString& pv);
```

**ポイント**: パラメータの特殊な値（`INT_MIN`, `0`, `-1` 等）の意味を補足する。

### スロット

```cpp
public slots:
    /// エンジンの "info ..." 出力を受け取る（Usi::infoReceived に接続）
    void onEngineInfoLine(const QString& line);

    /// エンジンの "bestmove ..." を受け取り次の局面へ進む
    void onEngineBestmoveReceived(const QString& line);
```

**ポイント**: 接続元のシグナル名を `（... に接続）` で示すと、コードを追わずに接続関係が分かる。

---

## 6. メンバー変数

### 行末コメント（`///<`）

メンバー変数は Doxygen の `///<` で行末にコメントを記述する。

```cpp
private:
    int m_currentPly = 0;           ///< 現在の手数（0始まり）
    Mode m_mode = Idle;             ///< 現在の解析モード
    QPointer<Usi> m_engine;         ///< USIエンジンへの参照（非所有）
    QTimer m_delayTimer;            ///< go送信のディレイ用タイマー
```

### 補足が長い場合

```cpp
    /// 各 ply に対応した "position ..." コマンド列
    /// KifuLoadCoordinator が構築した局面再現用コマンドを参照する
    QStringList* m_sfenHistory = nullptr;
```

### 所有権の明示

Qt でポインタを保持する場合、**所有（親子関係）か参照か**を明記する。
これはメモリリークやダングリングポインタの防止に直結する。

```cpp
    QPointer<EngineAnalysisTab> m_analysisTab;  ///< 解析タブへの参照（非所有）
    QTimer* m_timer = nullptr;                  ///< 思考タイマー（thisが親、所有）
```

---

## 7. 列挙型・構造体

### 列挙型

値ごとに `///<` で意味を記述する。

```cpp
/**
 * @brief 接続状態
 */
enum class ConnectionState {
    Disconnected,       ///< 未接続
    Connecting,         ///< 接続中
    Connected,          ///< 接続済み（ログイン前）
    LoggedIn,           ///< ログイン済み（対局待ち）
    InGame,             ///< 対局中
    GameOver            ///< 対局終了
};
```

### 構造体

フィールドごとに `///<` で意味を記述する。設計オプションの場合はデフォルト値の意図も補足する。

```cpp
/// 解析オプション
struct Options {
    int  startPly   = 0;     ///< 解析開始手数（0以上）
    int  endPly     = -1;    ///< 解析終了手数（-1は末尾まで）
    int  movetimeMs = 1000;  ///< 1局面あたりの思考時間（ms）
    int  multiPV    = 1;     ///< MultiPVの本数
    bool centerTree = true;  ///< 解析進捗時にツリーをセンタリングするか
};
```

---

## 8. セクション区切り

クラス内の論理的なグループを区切る。

### ヘッダーファイル内

```cpp
public:
    // --- 初期化・バインド ---

    /// 既存のデータストアをバインドする（参照を保持、所有しない）
    void bind(QList<KifDisplayItem>* liveDisp);

    // --- 操作 API ---

    /// 指定範囲を順次解析する
    void startAnalyzeRange();

    /// 現在の解析を中断する
    void stop();
```

### ソースファイル内

```cpp
// ============================================================
// 初期化
// ============================================================

AnalysisCoordinator::AnalysisCoordinator(const Deps& d, QObject* parent)
    : QObject(parent)
{
    ...
}

// ============================================================
// 操作 API
// ============================================================

void AnalysisCoordinator::startAnalyzeRange()
{
    ...
}
```

**使い分け**:
- `.h` 内: `// --- セクション名 ---` （軽量、視覚的に十分）
- `.cpp` 内: `// ====...====` で囲む（長いファイルでのスクロール時に視認しやすい）

---

## 9. インラインコメント（実装部）

`.cpp` ファイル内の実装コメントは「**なぜそうするのか**」を書く。

### 良い例

```cpp
// 1手前の局面に戻してから再解析する（エンジンが現局面のキャッシュを
// 使い回すのを防ぐため）
m_currentPly--;
sendPositionCommand();
```

```cpp
// Qt の QProcess::finished は非同期で届くため、
// ここではフラグだけ立てて実際の後処理は slot で行う
m_stopping = true;
```

```cpp
// SFEN の駒台部分が "-" の場合は持ち駒なし
if (handStr == "-") {
    return;
}
```

### 悪い例（コードの繰り返し）

```cpp
// plyをデクリメントする  ← コードを読めば分かる
m_currentPly--;

// タイマーを停止する  ← コードを読めば分かる
m_timer->stop();
```

### TODO / FIXME

将来の改善点や既知の問題には標準マーカーを使用する。

```cpp
// TODO: MultiPV > 1 の場合に各PVラインを個別にハイライトする
// FIXME: エンジンが応答しない場合のタイムアウト処理が未実装
```

---

## 10. 書かないコメント

以下のコメントは**記述しない**。コードの可読性を下げるノイズとなるため。

| 書かないもの | 理由 |
|-------------|------|
| `// コンストラクタ` | 見れば分かる |
| `// デストラクタ` | 見れば分かる |
| `// getter` / `// setter` | 命名規約から自明 |
| `// 変更履歴` / 日付コメント | Git で管理する |
| コメントアウトしたコード | 削除して Git に任せる |
| 条件分岐の `} // end if` | ブロックが長い場合はメソッド分割で対処する |
| `★` マーカー付き変更メモ | コミットメッセージに書く |

---

## 11. 既存コードへの適用方針

全ファイルを一括変換するのではなく、以下のルールで段階的に統一する。

1. **新規ファイル**: 本ガイドに完全に準拠する
2. **修正するファイル**: 変更したクラス・メソッドのコメントを本ガイドに合わせる
3. **既存の `//` コメント**: 内容が正確であればそのまま残す。Doxygen 形式への変換は当該コード修正時に行う
4. **ファイルヘッダー**: 新規作成・大幅修正時に追加する

### 適用済みタグ

スタイルガイドを適用したファイルには、リリース前に一括検出・除去できるよう `@todo remove` タグを挿入する。

```cpp
/// @todo remove コメントスタイルガイド適用済み
```

**挿入箇所**:
- **`.h` ファイル**: クラス・構造体・メソッドの Doxygen コメント内（個別フィールドの `///<` には不要）
- **`.cpp` ファイル**: `@file`/`@brief` ヘッダーに1行追加、さらに**適用済みの各関数定義の直前**にも1行追加

```cpp
// .h の例
/**
 * @brief 対局開始フローを司るコーディネータ
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class GameStartCoordinator : public QObject

// .cpp の例（ファイルヘッダー）
/// @file gamestartcoordinator.cpp
/// @brief 対局開始コーディネータクラスの実装
/// @todo remove コメントスタイルガイド適用済み

// .cpp の例（関数定義の直前）
/// @todo remove コメントスタイルガイド適用済み
void GameStartCoordinator::start(const StartParams& params)
{
    ...
}
```

**注意**: `.cpp` の関数タグは適用済みの関数にのみ付与する。未適用の関数にはタグを付けない。これにより `grep` で適用範囲を正確に把握できる。

**リリース時の除去**:
```bash
grep -rn "@todo remove コメントスタイルガイド適用済み" src/
```

### コメント内容の精査

スタイルガイドを適用する際は、書式の変換（`//` → `///<`、`@file`/`@brief` 追加等）だけでなく、**コメントの内容**も以下の観点で精査・修正すること。書式だけ整えて内容を見直さないのは不十分である。

| チェック観点 | 確認内容 | 対処 |
|---|---|---|
| **§4 自明な What の排除** | メソッド名から意図が読み取れるのにコメントが名前を繰り返していないか（例: `/// 盤面を反転する` on `flipBoard()`） | コメントを削除する |
| **§5 シグナルの接続先** | `connect()` を `grep` して実際の接続先を確認し、コメントに反映しているか | 接続先を `（→ Receiver::slot）` で明記する |
| **§5 未接続シグナルの検出** | `emit` されているが `connect()` が存在しないシグナル、または宣言のみで `emit` もされていないシグナルがないか | 不要なら削除する。将来用なら理由を `// TODO:` で残す |
| **§10 禁止コメントの残存** | `// コンストラクタ`、`★` マーカー等が残っていないか | 削除する |
| **§9 Why の確認** | `.cpp` 内のインラインコメントが「なぜ」を説明しているか、コードの繰り返しになっていないか | What コメントを削除、または Why に書き換える |

---

## クイックリファレンス

```cpp
/// @file myclass.h
/// @brief 〇〇を担当するクラスの定義

#ifndef MYCLASS_H
#define MYCLASS_H

#include <QObject>

class OtherClass;

/**
 * @brief 〇〇を担当するクラス
 *
 * 詳細な説明（必要な場合のみ）。
 * 責務や設計意図を簡潔に記述する。
 */
class MyClass : public QObject
{
    Q_OBJECT

public:
    /// 解析モード
    enum class Mode {
        Idle,       ///< 待機中
        Running,    ///< 実行中
        Paused      ///< 一時停止
    };
    Q_ENUM(Mode)

    /// 依存オブジェクト
    struct Deps {
        QStringList* sfenRecord = nullptr;  ///< 局面コマンド列（非所有）
    };

    explicit MyClass(const Deps& d, QObject* parent = nullptr);
    ~MyClass() override;

    // --- 初期化 ---

    /// 依存オブジェクトを再設定する
    void setDeps(const Deps& d);

    // --- 操作 API ---

    /**
     * @brief 指定範囲を解析する
     * @param startPly 開始手数
     * @param endPly   終了手数（-1で末尾まで）
     */
    void analyze(int startPly, int endPly = -1);

    /// 解析を中断する
    void stop();

signals:
    /// 解析の進捗を通知する（Controller → View）
    void progressChanged(int ply, int totalPly);

    /// 解析完了を通知する
    void finished();

public slots:
    /// エンジン出力を受け取る（Usi::infoReceived に接続）
    void onEngineOutput(const QString& line);

private:
    Mode m_mode = Mode::Idle;       ///< 現在のモード
    int  m_currentPly = 0;          ///< 処理中の手数
    QPointer<OtherClass> m_other;   ///< 外部オブジェクトへの参照（非所有）
};

#endif // MYCLASS_H
```
