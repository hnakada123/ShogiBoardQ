# Deps/Hooks/Refs パターン設計ガイド

本プロジェクトでは、依存注入に **3 種の構造体パターン** を使い分けている。
いずれもクラスの外部依存を明示的に宣言し、テスト容易性と遅延初期化への対応を両立させる。

## 1. 概要

| パターン | 方向 | 用途 | 主な使用層 |
|---|---|---|---|
| **Deps** | 外→内 | 外部依存オブジェクト・コールバックの注入 | 全層（`app/`, `ui/`, `services/`, `game/`） |
| **Hooks** | 内→外 | 下位クラスから上位層への通知コールバック | `game/`, `engine/`, `kifu/`, `network/` |
| **Refs** | 内→外 | 親の内部状態への読み書きアクセス | `game/`, `kifu/`, `ui/` |

依存の流れ:

```
上位層 (MainWindow / Wiring)
  │
  ├── Deps ──→ サービス / コントローラ / コーディネータ
  │              │
  │              ├── Refs ──→ 子ハンドラ（親の状態を読み書き）
  │              └── Hooks ─→ 子ハンドラ（親やさらに上位層へコールバック）
  │
  └── Hooks ─→ MatchCoordinator → 上位層への通知
```

## 2. Deps パターン

### 2.1 いつ使うか

- クラスが外部オブジェクトへの非所有参照を必要とする場合
- 遅延初期化（`ensure*()` パターン）で依存が段階的に揃う場合
- 上位層の操作を呼び出す必要がある場合（`std::function` コールバック）

### 2.2 基本構造

```cpp
class MyService : public QObject {
    Q_OBJECT
public:
    struct Deps {
        // 非所有ポインタ（外部オブジェクトへの参照）
        ShogiGameController* gc = nullptr;
        ShogiView* view = nullptr;

        // 遅延初期化コールバック（兄弟オブジェクトの生成トリガー）
        std::function<void()> ensureSomething;

        // アクションコールバック（上位層の操作を呼び出す）
        std::function<void(int)> updateStatus;
    };

    void updateDeps(const Deps& deps);

private:
    Deps m_deps;
};
```

### 2.3 ルール

- **全メンバは nullptr / デフォルト初期化**: 部分的にしか依存が揃わない状態を許容する
- **`updateDeps()` は複数回呼ばれうる**: 起動パイプラインの複数段階で依存が追加される
- **非所有**: Deps 内のポインタは所有権を持たない（ライフタイムは呼び出し側が管理）
- **null チェックは使用時に行う**: 遅延初期化により一時的に null になりうるメンバがある

### 2.4 バリエーション

#### シングルポインタ（安定した依存）

```cpp
ShogiGameController* gc = nullptr;   // 起動後は変わらない
```

#### ダブルポインタ（遅延生成される依存）

```cpp
GameStartCoordinator** gameStart = nullptr;  // ensure*() で生成後にポインタが書き込まれる
```

呼び出し側（通常は MainWindow）の `T*` メンバへのポインタ。`ensure*()` で生成された後、`*gameStart` が非 null になる。

#### 状態ポインタ（上位層の状態を直接参照）

```cpp
PlayMode* playMode = nullptr;       // MainWindow の状態変数を直接参照
int* currentMoveIndex = nullptr;    // 読み書き両用
```

#### 遅延初期化コールバック

```cpp
std::function<void()> ensureDialogCoordinator;  // 兄弟クラスの生成を要求
```

抽出元クラスで `ensure*()` を呼んでいた箇所の代替。循環依存を避けつつ遅延初期化を可能にする。

### 2.5 注入方法

| 方式 | 使用場面 |
|---|---|
| `updateDeps()` のみ | 大多数のクラス（段階的に依存が揃う） |
| コンストラクタ + `updateDeps()` | `ConsiderationWiring` など、初期化時に一部の依存が確定しているクラス |
| コンストラクタのみ | `MatchCoordinator`（依存が変わるたびに再生成される） |

### 2.6 実例: UiStatePolicyManager（シンプル）

```cpp
// src/ui/coordinators/uistatepolicymanager.h
struct Deps {
    Ui::MainWindow* ui = nullptr;
    RecordPane* recordPane = nullptr;
    EngineAnalysisTab* analysisTab = nullptr;
    BoardInteractionController* boardController = nullptr;
};
void updateDeps(const Deps& deps);
```

全メンバがシングルポインタで、コールバックなし。最もシンプルな形。

### 2.7 実例: BoardLoadService（コールバック付き）

```cpp
// src/app/boardloadservice.h
struct Deps {
    ShogiGameController* gc = nullptr;
    ShogiView* view = nullptr;
    BoardSyncPresenter* boardSync = nullptr;
    QString* currentSfenStr = nullptr;             // 状態ポインタ
    std::function<void()> setCurrentTurn;           // アクションコールバック
    std::function<void()> ensureBoardSyncPresenter; // 遅延初期化コールバック
    std::function<void()> beginBranchNavGuard;
};
```

## 3. Hooks パターン

### 3.1 いつ使うか

- 下位層のクラスが上位層に **同期的な通知・操作要求** を送る場合
- シグナル/スロットが使えない場合（非 QObject クラス、または戻り値が必要な呼び出し）
- 子ハンドラを親クラスの具体型に依存させたくない場合

### 3.2 基本構造

```cpp
class GameEndHandler : public QObject {
    Q_OBJECT
public:
    struct Hooks {
        std::function<void()> disarmHumanTimerIfNeeded;
        std::function<Usi*()> primaryEngine;
        std::function<void(const GameEndInfo&, bool, bool)> setGameOver;
        // MC::Hooks からのパススルー
        std::function<void(const QString&, const QString&)> appendKifuLine;
        std::function<void(const QString&, const QString&)> showGameOverDialog;
        std::function<void()> autoSaveKifuIfEnabled;
    };

    void setHooks(const Hooks& hooks);

private:
    Hooks m_hooks;
};
```

### 3.3 ルール

- **`std::function` のみ**: シグナル/スロットではなくコールバック関数で構成
- **設定は1回**: `setHooks()` は親の `ensure*()` 内で1回だけ呼ばれる
- **親が `this` を lambda にキャプチャ**: 子ハンドラは親の具体型を知らない
- **パススルー可**: 親自身が受け取った Hooks の一部をそのまま子に渡す

### 3.4 ネストサブ構造体による責務分類

大規模な Hooks は責務ごとにサブ構造体に分類する。

```cpp
// src/game/matchcoordinator.h
struct Hooks {
    struct UI {
        std::function<void(Player cur)> updateTurnDisplay;
        std::function<void(const QString& p1, const QString& p2)> setPlayersNames;
        std::function<void()> renderBoardFromGc;
        std::function<void(const QString&, const QString&)> showGameOverDialog;
        std::function<void(const QPoint&, const QPoint&)> showMoveHighlights;
    };
    struct Time {
        std::function<qint64(Player)> remainingMsFor;
        std::function<qint64(Player)> incrementMsFor;
        std::function<qint64()> byoyomiMs;
    };
    struct Engine { /* ... */ };
    struct Game   { /* ... */ };

    UI ui;
    Time time;
    Engine engine;
    Game game;
};
```

### 3.5 Hooks のパススルー

子ハンドラの Hooks は、親の Hooks を部分的に再公開することが多い。

```
MatchCoordinator::Hooks::game.appendKifuLine
         ↓ パススルー
GameEndHandler::Hooks::appendKifuLine
```

これにより子ハンドラは自己完結し、`MatchCoordinator::Hooks` の全体構造を知る必要がない。

### 3.6 注入方法

| 方式 | 使用場面 |
|---|---|
| `setHooks()` | 大多数（親の `ensure*()` で post-construction 設定） |
| コンストラクタ経由 | `MatchCoordinator`（`Deps` 構造体の中に `Hooks` を含めて注入） |

### 3.7 Hooks ファクトリ

`MatchCoordinator::Hooks` は複雑なため、専用のファクトリクラス `MatchCoordinatorHooksFactory::buildHooks()` で一括生成される。

## 4. Refs パターン

### 4.1 いつ使うか

- 子ハンドラが親の **プライベート状態を直接読み書き** する必要がある場合
- Hooks（コールバック）では冗長になる単純な状態アクセス
- 主に `game/` 層の子ハンドラ（`MatchCoordinator` の分割で生まれたクラス）で使用

### 4.2 基本構造

```cpp
class GameStartOrchestrator {
public:
    struct Refs {
        ShogiGameController* gc = nullptr;
        PlayMode* playMode = nullptr;
        int* maxMoves = nullptr;
        Player* currentTurn = nullptr;
        bool* autoSaveKifu = nullptr;
        QString* humanName1 = nullptr;
        // ...
    };

    void setRefs(const Refs& refs);

private:
    Refs m_refs;
};
```

### 4.3 ルール

- **親の状態を直接指す**: ポインタ先は親のプライベートメンバ
- **所有権は親**: Refs 内のポインタは借用のみ
- **読み書き両用**: Deps のポインタと異なり、値の書き換えも行う
- **設定は1回**: `setRefs()` は親の `ensure*()` 内で1回だけ呼ばれる

### 4.4 プロバイダ関数（動的に変化するポインタ）

実行時に差し替わるポインタ（エンジン、ストラテジー等）は、生ポインタの代わりにプロバイダ関数を使う。

```cpp
// src/game/gameendhandler.h
struct Refs {
    ShogiGameController* gc = nullptr;            // 安定（変わらない）
    ShogiClock* clock = nullptr;                   // 安定
    std::function<Usi*()> usi1Provider;             // 動的（エンジン再生成で変わる）
    std::function<Usi*()> usi2Provider;
    std::function<GameModeStrategy*()> strategyProvider; // 動的
    PlayMode* playMode = nullptr;
    GameOverState* gameOver = nullptr;
    QStringList* sfenHistory = nullptr;
};
```

### 4.5 Deps との違い

| | Deps | Refs |
|---|---|---|
| **方向** | 外部（呼び出し側）→ クラス | 親 → 子ハンドラ |
| **アクセス** | 主に読み取り・メソッド呼び出し | 読み書き両用 |
| **ポインタ先** | 外部オブジェクト | 親のプライベートメンバ |
| **更新頻度** | `updateDeps()` で複数回可 | `setRefs()` で1回 |
| **コールバック** | `std::function` を含む | 通常は含まない（プロバイダ関数を除く） |

## 5. 判断フローチャート

新しいクラスに依存注入パターンを適用する際の判断基準:

```
クラスが外部オブジェクトを参照する必要がある？
  │
  ├── Yes → 親クラスから分割した子ハンドラか？
  │           │
  │           ├── Yes → 親の内部状態を直接読み書きする？
  │           │           │
  │           │           ├── Yes → Refs を使う
  │           │           └── No  → 操作は必要？
  │           │                       │
  │           │                       ├── Yes → Hooks を使う
  │           │                       └── No  → Deps を使う
  │           │
  │           └── No → Deps を使う
  │
  └── No → パターン不要
```

### 組み合わせの典型例

| クラスの種類 | Deps | Refs | Hooks | 例 |
|---|---|---|---|---|
| Wiring / Service | ○ | - | - | `UiStatePolicyManager`, `BoardLoadService` |
| 子ハンドラ（状態操作あり） | - | ○ | ○ | `GameEndHandler`, `GameStartOrchestrator` |
| 子ハンドラ（通知のみ） | - | - | ○ | `UsiMatchHandler` |
| 中間コーディネータ | ○ | - | ○ | `MatchCoordinator`（Deps に Hooks を含む） |

## 6. アンチパターン

### 6.1 Deps に Refs 相当のものを入れない

```cpp
// Bad: Deps に親の内部状態ポインタを入れている（Refs を使うべき）
struct Deps {
    GameOverState* gameOver = nullptr;  // 親の内部状態
};

// Good: 親の内部状態は Refs に分離
struct Refs {
    GameOverState* gameOver = nullptr;
};
```

ただし、Wiring 層の Deps では MainWindow の状態ポインタ（`PlayMode*`, `int*` 等）を含むことがある。これは Wiring 層が MainWindow の「延長」として機能するためで、親子関係の Refs とは異なる。

### 6.2 Hooks でシグナルを代替しすぎない

```cpp
// Bad: 多数のリスナーに通知する場合は Hooks ではなくシグナルを使う
struct Hooks {
    std::function<void()> notifyAllListeners;  // 1対多の通知
};

// Good: QObject のシグナルを使う
signals:
    void gameEnded(const GameEndInfo& info);
```

Hooks は **1対1のコールバック** に適している。複数の受信者がある場合はシグナルを使う。

### 6.3 updateDeps() 後の null 忘れ

```cpp
// Bad: updateDeps() 後に旧ポインタが残る可能性を考慮していない
void doSomething() {
    m_deps.gc->someMethod();  // gc が null かもしれない
}

// Good: null チェックまたは事前条件の確認
void doSomething() {
    if (!m_deps.gc) return;
    m_deps.gc->someMethod();
}
```
