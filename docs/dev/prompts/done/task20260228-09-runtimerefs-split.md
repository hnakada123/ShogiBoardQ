# Task 09: MainWindowRuntimeRefs 領域別分割

## 概要

`MainWindowRuntimeRefs`（141フィールド）を `GameRefs / KifuRefs / UiRefs` 等の領域別構造体に分割し、依存密度を低減する。

## 前提条件

- Task 01（ServiceRegistry再分割）完了推奨（サブレジストリとの整合を取る）

## 現状

- `src/app/mainwindowruntimerefs.h`: 143行、約45フィールド
- カテゴリ別内訳:
  - UI参照: 3フィールド（parentWidget, mainWindow, statusBar）
  - サービス参照: 5フィールド（match, gameController, shogiView, kifuLoadCoordinator, csaGameCoordinator）
  - モデル参照: 8フィールド（kifuRecordModel, thinking1/2, consideration, commLog1/2, gameRecordModel, considerationModel）
  - 状態参照: 8フィールド（playMode, currentMoveIndex, currentSfenStr, startSfenStr, skipBoardSyncForBranchNav, commentsByRow, resumeSfenStr, onMainRowGuard）
  - 棋譜参照: 8フィールド（sfenRecord, gameMoves, gameUsiMoves, moveRecords, positionStrList, activePly, currentSelectedPly, saveFileName）
  - 分岐ナビゲーション参照: 3フィールド
  - コントローラ/Wiring参照: 12フィールド
  - 対局者名参照: 4フィールド（humanName1/2, engineName1/2）
  - その他: 5フィールド

- `MainWindowDepsFactory` が `buildRuntimeRefs()` で全フィールドを束ねている

## 実施内容

### Step 1: 利用箇所の調査

`MainWindowRuntimeRefs` がどこで使われているか全箇所を調査:

```
grep -rn "MainWindowRuntimeRefs\|RuntimeRefs\|runtimeRefs\|buildRuntimeRefs" src/
```

各 Wiring/Controller の Deps が RuntimeRefs のどのフィールドを参照しているかマッピングする。

### Step 2: 領域別構造体の設計

RuntimeRefs を以下のサブ構造体に分割:

```cpp
struct GameRefs {
    MatchCoordinator* match = nullptr;
    ShogiGameController* gameController = nullptr;
    // ... Game系フィールド
};

struct KifuRefs {
    QStringList* sfenRecord = nullptr;
    QVector<ShogiMove>* gameMoves = nullptr;
    // ... Kifu系フィールド
};

struct UiRefs {
    QWidget* parentWidget = nullptr;
    MainWindow* mainWindow = nullptr;
    // ... UI系フィールド
};

struct MainWindowRuntimeRefs {
    GameRefs game;
    KifuRefs kifu;
    UiRefs ui;
    // 共通フィールド
};
```

### Step 3: DepsFactory の更新

1. `buildRuntimeRefs()` をサブ構造体に対応するよう更新
2. 各 Wiring/Controller の Deps がサブ構造体を受け取るよう変更
3. 不要なフィールドへの依存を断つ

### Step 4: 段階的移行

全一括変更はリスクが高いため、以下の順で段階的に移行:

1. サブ構造体を定義（互換性維持のため元フィールドも残す）
2. 新規参照を段階的にサブ構造体経由に変更
3. 旧フィールドへの参照がなくなったら削除

## 完了条件

- `MainWindowRuntimeRefs` が領域別に整理されていること
- `buildRuntimeRefs()` のフィールド数が 30% 以上削減されていること
- ビルド成功
- 全テスト通過

## 注意事項

- `MainWindowRuntimeRefs` は値オブジェクト（non-QObject）のため、所有権は変わらない
- フィールドの移動は、参照元が正しくアクセスできることを確認しながら行う
- 一度に全フィールドを移動せず、カテゴリ単位で段階的に進める
- 前方宣言の追加が必要になる場合がある
