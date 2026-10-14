# 対局ダイアログから対局開始までのフロー

本ドキュメントでは、GUIの「対局」メニューから「対局開始」をクリックし、実際に対局が始まるまでに実行される関数とその内容を時系列で説明する。

先手・後手はすべて `Turn`（`Turn::Black`／`Turn::White`、`src/core/shogitypes.h`）で表す。部品の生成と配線は `MainWindowServiceRegistry`（`src/app/mainwindowserviceregistry.*`、`src/app/gamesubregistry*.cpp`、`src/app/mainwindow*registry.cpp`）が行う。

## 全体フロー図

```
ユーザー操作: メニュー「対局」→「対局開始」クリック
    │
    ▼
GameActionsWiring::wire()
    actionStartGame::triggered → GameSessionOrchestrator::initializeGame()
    │
    ▼
GameSessionOrchestrator::initializeGame()
    │  StartGameDialogの表示（モーダル）と設定値（StartGameDialogData）の取り出し
    │  表示中の手順（分岐ツリー）から対局の局面列・指し手列を作り直す
    │  現在局面SFENの解決、コンテキスト(Ctx)の構築
    │
    ▼
GameStartCoordinator::initializeGame(Ctx)
    │  1) 編集済み開始局面の判定
    │  2) ダイアログ設定値（Ctx::dialogData）の取得と開始局面の玉の検証
    │  3) 開始SFENの決定（現在局面 or プリセット）
    │  3.5) 開始SFENの正規化と局面列の整理
    │  4) PlayModeの決定
    │  5) StartOptionsの構築と盤の向きの調整
    │  6) 時間設定（TimeSettings）の構築
    │  7) prepare() → 時計の初期化
    │  8) playerNamesResolved / consecutiveGamesConfigured シグナル発行
    │  9) start(StartParams) → 対局開始
    │  10) 時計開始 + エンジン初手
    │
    ▼
GameStartCoordinator::start(StartParams)
    │  時間の決まりの正規化・時計への適用依頼
    │
    ▼
MatchCoordinator::configureAndStart(StartOptions)
    │  0) 中断した対局の破棄、指し手列を局面列の長さにそろえる（alignGameMovesWithSfenHistory）
    │  以降は GameStartOrchestrator::configureAndStart() が行う
    │  1) 前回対局履歴の保存
    │  2) 開始局面を過去の対局から探す
    │  3) 終局状態のクリアと対局設定の保存
    │  4) Hooks経由でUI初期化
    │  5) 開始手番の決定（SFEN解析）
    │  6) position文字列の初期化
    │  7) モード別の開始処理（MatchTurnHandler::createAndStartModeStrategy）
    │
    ▼
HumanVsHumanStrategy / HumanVsEngineStrategy / EngineVsEngineStrategy::start()
    │
    ▼
対局開始（エンジンの手番なら初手の思考を要求）
```

---

## 1. メニューアクションの接続

**ファイル:** `src/ui/wiring/gameactionswiring.cpp`

```cpp
QObject::connect(ui->actionStartGame, &QAction::triggered,
                 gso, &GameSessionOrchestrator::initializeGame, Qt::UniqueConnection);
```

ユーザーがメニュー「対局」→「対局開始」をクリックすると、`actionStartGame` の `triggered` シグナルが発火し、`GameSessionOrchestrator::initializeGame()` スロットが呼ばれる。`GameActionsWiring` は `UiActionsWiring` が作り、`GameSessionOrchestrator` は `MainWindowServiceRegistry::ensureGameSessionOrchestrator()`（`src/app/gamesubregistry_session.cpp`）が作る。

---

## 2. GameSessionOrchestrator::initializeGame()

**ファイル:** `src/app/gamesessionorchestrator.cpp`, `src/app/mainwindowgamestartservice.cpp`

対局開始フローのエントリポイント。対局開始ダイアログを表示し、対局の局面列・指し手列を棋譜の正本（分岐ツリー）から作り直してから、GameStartCoordinatorに渡すコンテキスト（Ctx）を準備する。

### 処理内容

1. **GameStartCoordinatorの取得**
   ```cpp
   auto* gameStart = get(m_deps.gameStart);  // 取得関数が、まだ無ければ作ってから返す
   if (!gameStart) return;
   ```
   `Deps::gameStart` はレジストリが渡す取得関数で、`MainWindowServiceRegistry::ensureGameStartCoordinator()`（`src/app/gamesubregistry.cpp`）を呼んでから返す。`GameStartCoordinator` は1つだけで、`MatchCoordinatorWiring::ensureMenuGameStartCoordinator()` が作り、`MatchCoordinatorWiring` が持つ（Qtの親）。

2. **対局開始ダイアログの表示**
   ```cpp
   StartGameDialog dlg;
   if (hasEditedStart) {
       dlg.forceCurrentPositionSelection();  // 編集した局面なら「現在の局面」を選ぶ
   }
   if (dlg.exec() != QDialog::Accepted) {
       return;
   }
   const StartGameDialogData dialogData = extractDialogData(dlg);

   if (dialogData.startingPositionNumber != 0 && m_deps.confirmDiscardUnsavedKifu
       && !m_deps.confirmDiscardUnsavedKifu()) return;
   ```
   `hasEditedStart` は、開始SFEN（`startSfenStr`）か局面列の先頭が平手でも `"startpos"` でもないときに true になる。ダイアログの値は `extractDialogData()` が `StartGameDialogData`（`src/game/startgamedatabridge.h`）に写す。game 層はダイアログを参照せず、この値だけを受け取る。プリセット（平手・駒落ち）で始めるときは、未保存の棋譜を捨ててよいか確かめる。

3. **表示中の手順と手数の取得**
   ```cpp
   const RecordLine line = m_deps.navState ? m_deps.navState->currentRecordLine() : RecordLine{};
   const int selectedPly = m_deps.navState ? m_deps.navState->currentPly() : 0;
   ```
   表示中の手数は分岐ツリーのカーソル（`KifuNavigationState`）が持つ。本譜を表示していても分岐を表示していても同じ経路で取る（[指し手データ・棋譜データ構造](move-and-record-data-structures.md) の 4.4）。

4. **対局の局面列・指し手列の作り直しと現在局面SFENの解決**
   ```cpp
   MainWindowGameStartService::PrepareDeps prep;
   prep.line = line;
   prep.sfenRecord = sfenRecord;          // MatchCoordinator::sfenRecordPtr()
   prep.gameMoves = m_deps.gameMoves;
   prep.currentSfenStr = m_deps.currentSfenStr;

   const MainWindowGameStartService gameStartService;
   gameStartService.prepareForInitializeGame(prep);
   ```
   `prepareForInitializeGame()` は次の2つを行う。
   - `rebuildMatchHistoryFromLine()`: 局面列を `line.sfens[0..ply]`（開始局面から開始する手数の後の局面まで）、指し手列を `line.moves[0..ply)` で作り直す（`ply = line.currentPly`）。0手目から始める場合も作り直す。手順が空なら何もしない
   - `resolveCurrentSfenForGameStart()`: 作り直した局面列の `ply` 番目（範囲外なら末尾）を現在局面SFENにする

   対局の局面列・指し手列は対局の作業用の履歴で、棋譜の読み込みでは書かない。ここで表示中の手順から作り直すので、前の対局の手や、読み込んだ棋譜の開始する手数より後の手は残らない。

5. **Ctxの構築とinitializeGame呼び出し**
   ```cpp
   MainWindowGameStartService::ContextDeps ctx;
   ctx.gc = m_deps.gameController;
   ctx.clock = match ? match->clock() : nullptr;
   ctx.sfenRecord = sfenRecord;
   ctx.kifuModel = m_deps.kifuModel;
   ctx.currentSfenStr = m_deps.currentSfenStr;
   ctx.startSfenStr = m_deps.startSfenStr;
   ctx.selectedPly = selectedPly;
   ctx.isReplayMode = m_deps.replayController ? m_deps.replayController->isReplayMode() : false;
   ctx.bottomSide = m_deps.bottomSide ? *m_deps.bottomSide : Turn::Black;

   GameStartCoordinator::Ctx c = gameStartService.buildContext(ctx);
   c.dialogData = dialogData;
   QString errorMessage;
   if (!gameStart->initializeGame(c, errorMessage) && !errorMessage.isEmpty()) {
       ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Warning, errorMessage);
   }
   ```
   `Deps::bottomSide` は MainWindow の `m_player.bottomSide`（`PlayerState`、`src/app/mainwindowstate.h`。盤の手前に表示している側）を指す。開始局面が対局に使えないとき（3章のステップ2）は、`initializeGame()` が返したメッセージを警告として出す。

### Ctx構造体の内容

| フィールド | 型 | 説明 |
|---|---|---|
| `gc` | `ShogiGameController*` | ゲームコントローラ |
| `clock` | `ShogiClock*` | 将棋時計（`MatchCoordinator::clock()`） |
| `dialogData` | `StartGameDialogData` | 対局開始ダイアログから取り出した値 |
| `sfenRecord` | `QStringList*` | 対局の局面列（手順 4 で作り直したもの） |
| `kifuModel` | `KifuRecordListModel*` | 棋譜欄モデル |
| `currentSfenStr` | `QString*` | 現在局面のSFEN文字列 |
| `startSfenStr` | `QString*` | 開始SFEN文字列 |
| `selectedPly` | `int` | 表示中の手数（`KifuNavigationState::currentPly()`） |
| `isReplayMode` | `bool` | 再生モード中か |
| `bottomSide` | `Turn` | 盤の手前に表示している側 |

### 対局開始ダイアログ（StartGameDialog）

**ファイル:** `src/dialogs/startgamedialog.cpp`, `src/dialogs/startgamedialog_settings.cpp`, `src/dialogs/startgamedialog_time.cpp`

- `StartGameDialog` をモーダルダイアログとして表示する
- ユーザーがキャンセルした場合はここで終了
- ダイアログ内では前回の設定が設定ファイルから復元される（`loadGameSettings()`。項目は `GameSettings::StartGame` の宣言の表）

#### StartGameDialogの主な設定項目

| カテゴリ | 設定項目 |
|---|---|
| 対局者 | 先手/後手の種別（人間/エンジン選択）、人間名、エンジン選択 |
| 持ち時間 | 持ち時間（時/分）、秒読み、フィッシャー加算（後手に別の時間を設定可） |
| 開始局面 | 現在の局面、平手、香落ち、右香落ち、角落ち…十枚落ち（手合割は14種類） |
| 対局オプション | 最大手数、連続対局数（両方エンジンのときだけ）、1局ごとに手番入替、人を手前に表示、棋譜自動保存、時間切れ負け、持将棋ルール |

#### ダイアログOKボタンの処理

OKボタン（「対局開始」）のクリックは `onStartRequested()` に接続され、次の順に処理する:
1. `updateGameSettingsFromDialog()` → UI値をメンバ変数にコピー
2. `confirmTimeSettings()` → 時間設定を確認する。片方だけ全項目0秒、またはエンジンが参加する時間無制限なら警告を出し、保存せずにダイアログを開いたままにする（[時間設定の仕様](game-time-control-spec.md)）
3. `saveGameSettings()` → `GameSettings::StartGame` の各項目に `.set()` で保存
4. `accept()` → ダイアログを閉じる

---

## 3. GameStartCoordinator::initializeGame(Ctx)

**ファイル:** `src/game/gamestartcoordinator_dialogflow.cpp`

対局開始の中核ロジック。ダイアログで決めた設定から対局開始までの一連のフローを実行する。ダイアログの表示と値の取り出しは呼び出し元（2章）が済ませており、ここでは `Ctx::dialogData` を使う。開始局面が対局に使えないときは何も変えずに false を返す。

### ステップ1: 編集済み開始局面の判定

`startSfenStr` が平手でも `"startpos"` でもなければそれを、そうでなければ局面列の先頭が編集済みの局面ならそれを、編集済みの開始局面（`editedStartSfen`）とする。これは、現在局面から開始する場合に開始SFENが決まらないときの予備にだけ使う。

### ステップ2: ダイアログ設定値の取得と開始局面の玉の検証

```cpp
const int initPosNo = c.dialogData.startingPositionNumber;
const bool p1Human   = c.dialogData.isHuman1;
const bool p2Human   = c.dialogData.isHuman2;
```

| 値 | 説明 |
|---|---|
| `initPosNo` | 開始局面番号（0=現在局面, 1=平手, 2-14=駒落ち） |
| `p1Human` | 先手が人間か |
| `p2Human` | 後手が人間か |

続いて、実際に対局に使う局面（プリセットならその局面、現在局面なら `currentSfenStr` → `startSfenStr` → 平手の順）を `validateKingsForGame()` で確かめる。先手・後手の玉が盤上に1枚ずつなければ、棋譜・時計・エンジンに触れる前にエラーメッセージを入れて false を返す。

```cpp
if (!validateKingsForGame(candidateSfen, errorMessage)) return false;
```

### ステップ3: 開始SFENの決定

#### 現在局面から開始する場合（`initPosNo == 0`）

```cpp
if (startingPosNumber == 0) {
    // 局面列の先頭（棋譜の開始局面）が startSfenStr と違えば、startSfenStr をそちらに合わせる
    ...
    prepareDataCurrentPosition(c);

    if (c.currentSfenStr && !c.currentSfenStr->isEmpty()) {
        startSfen = *(c.currentSfenStr);
    } else if (startSfen.isEmpty()) {
        startSfen = QStringLiteral("startpos");
    }
}
```

開始前の整理は `startSfenStr` を開始局面として分岐ツリーの根を作り直すので、読み込んだ棋譜などの開始局面に合わせておく。

`prepareDataCurrentPosition()`（`src/game/gamestartcoordinator.cpp`）の処理:
- 基準SFENを `currentSfenStr` → `startSfenStr` → `"startpos"` の順で決める（開始前の整理で `currentSfenStr` が書き換わるので、その前に決める）
- `requestPreStartCleanup()` シグナルを発行して開始前の整理を依頼する
- 基準SFENを盤に適用する（`"startpos"` なら `ViewHooks::initializeToFlatStartingPosition`、それ以外は `ViewHooks::applyResumePosition` で盤に `setSfen()`）。`currentSfenStr` も基準SFENにする
- 終局状態をクリア（`MatchCoordinator::clearGameOverState()`）
- ハイライトをクリア

その後、`c.currentSfenStr` から開始SFENを取得する。

#### 平手/駒落ちプリセットから開始する場合（`initPosNo >= 1`）

```cpp
startSfen = GameStartOptionsBuilder::startingPositionSfen(startingPosNumber);

prepareInitialPosition(c);

emit requestBranchTreeResetForNewGame();
```

プリセットのSFENは `GameStartOptionsBuilder::startingPositionSfen()`（`src/game/gamestartoptionsbuilder.cpp`）の表から取る。開始前の整理はここでは行わず、ステップ7の `prepare()` で1回だけ行う。

`prepareInitialPosition()` の処理:
- `startSfenStr` と `currentSfenStr` を選ばれた手合割のSFENにする
- 棋譜欄の先頭に「=== 開始局面 ===」の行が無ければ追加する
- 局面列をクリアして開始SFENを追加
- ハイライトをクリア

`prepareInitialPosition()` の後、`requestBranchTreeResetForNewGame` シグナルを発行する。`BranchNavigationWiring::onBranchTreeResetForNewGame()` が、前の対局のライブセッションが残っていれば破棄し、分岐ツリーの根を `startSfenStr`（選んだ手合割の開始局面）にしてカーソルを根に戻す。Ctx は棋譜読み込み担当（`KifuLoadCoordinator`）を持たない。

#### 開始局面プリセット一覧

| 番号 | 名称 | 手番 |
|---|---|---|
| 0 | 現在局面 | - |
| 1 | 平手 | 先手(b) |
| 2 | 香落ち | 後手(w) |
| 3 | 右香落ち | 後手(w) |
| 4 | 角落ち | 後手(w) |
| 5 | 飛車落ち | 後手(w) |
| 6 | 飛香落ち | 後手(w) |
| 7 | 二枚落ち | 後手(w) |
| 8 | 三枚落ち | 後手(w) |
| 9 | 四枚落ち | 後手(w) |
| 10 | 五枚落ち | 後手(w) |
| 11 | 左五枚落ち | 後手(w) |
| 12 | 六枚落ち | 後手(w) |
| 13 | 八枚落ち | 後手(w) |
| 14 | 十枚落ち | 後手(w) |

### ステップ3.5: SFEN正規化と局面列の整理

```cpp
const QString seedSfen = GameStartOptionsBuilder::canonicalizeSfen(startSfen);
```

- `"startpos"` や空文字列を平手の完全SFEN表記に正規化する
- 現在局面から開始の場合は、局面列の0手目から `selectedPly` 手目まで（棋譜欄の行数を超えない範囲）を残し、末尾を `seedSfen` に置き換える（局面列は2章の手順 4 で表示中の手順から作り直してある）。残した最後の手数を、対局開始後に選ぶ棋譜欄の行（`startingRow`）にする
- プリセットから開始の場合は、局面列を `seedSfen` の1件にする

### ステップ4: PlayModeの決定

```cpp
PlayMode mode = determinePlayModeAlignedWithTurn(initPosNo, p1Human, p2Human, seedSfen);
```

対局者タイプ（人間/エンジン）と開始局面番号から、対局モードを決定する（`GameStartOptionsBuilder::determinePlayModeAlignedWithTurn()`）。平手（`initPosNo == 1`）だけが `Even` 系になり、それ以外（現在局面から開始する場合を含む）は `Handicap` 系になる。人間とエンジンの対局では、先手が人間なら `HumanVsEngine`、先手がエンジンなら `EngineVsHuman` になる。

| PlayMode | 説明 |
|---|---|
| `HumanVsHuman` | 人間 vs 人間 |
| `EvenHumanVsEngine` | 平手・先手:人間 vs 後手:エンジン |
| `EvenEngineVsHuman` | 平手・先手:エンジン vs 後手:人間 |
| `HandicapHumanVsEngine` | 駒落ち・下手(先手):人間 vs 上手(後手):エンジン |
| `HandicapEngineVsHuman` | 駒落ち・下手(先手):エンジン vs 上手(後手):人間 |
| `EvenEngineVsEngine` | 平手・エンジン vs エンジン |
| `HandicapEngineVsEngine` | 駒落ち・エンジン vs エンジン |

エンジンの手番は、このモードで決まる（6.7）。

### ステップ5: StartOptionsの構築と盤の向きの調整

```cpp
MatchCoordinator::StartOptions opt =
    m_match->buildStartOptions(mode, seedSfen, c.sfenRecord, &c.dialogData);

m_match->ensureHumanAtBottomIfApplicable(&c.dialogData, c.bottomSide);
```

`MatchCoordinator::StartOptions` は `MatchStartOptions`（`src/game/startoptions.h`）の別名である。`buildStartOptions()` は `GameStartOrchestrator::buildStartOptions()`（static）に委譲し、ダイアログの値から StartOptions を構築する:

```cpp
struct MatchStartOptions {
    PlayMode mode = PlayMode::NotStarted;  // 対局モード
    QString  sfenStart;                    // 開始SFEN

    QString engineName1;                   // エンジン1表示名
    QString enginePath1;                   // エンジン1実行パス
    QString engineName2;                   // エンジン2表示名
    QString enginePath2;                   // エンジン2実行パス

    int maxMoves = 0;                      // 最大手数（0=無制限）

    bool autoSaveKifu = false;             // 棋譜自動保存フラグ
    QString kifuSaveDir;                   // 棋譜保存ディレクトリ

    QString humanName1;                    // 先手の人間対局者名
    QString humanName2;                    // 後手の人間対局者名
    bool resumeFromBreakOff = false;       // 中断した手の途中から再開する
};
```

- エンジンの表示名と実行パスは、ダイアログで選んだ番号（`engineNumber1/2`）で登録エンジン一覧（`engineList`）から引く
- ダイアログの値が無いとき（`dlg == nullptr`）は、`GameSettings::StartGame` に保存したエンジン名と `EngineListSettings::pathForName()` から引く
- エンジンが先手か後手かを表すフラグは持たない。エンジンの手番は対局モード（`mode`）で決まる

`ensureHumanAtBottomIfApplicable()`（`GameStartOrchestrator` へ委譲）は、ダイアログの「人を手前に表示する」がオンで、片方だけが人間のときに、人間の側（先手なら `Turn::Black`）と手前に表示している側（`bottomSide`）が違えば `MatchCoordinator::flipBoard()` で盤を1回反転する。反転は `MatchCoordinator::boardFlipped` → `MainWindowAppearanceController::onBoardFlipped()` と伝わり、`bottomSide` が入れ替わって盤の向きが変わる。

### ステップ6: 時間設定（TimeSettings）の構築

`GameStartOptionsBuilder::buildTimeControl()` がダイアログの値から作る。

```cpp
TimeControl rules;                       // src/core/timecontrol.h
rules.black.mainMs = minutesToMs(data.basicTimeHour1, data.basicTimeMinutes1);  // 持ち時間（時:分 → ms）
rules.white.mainMs = minutesToMs(data.basicTimeHour2, data.basicTimeMinutes2);
rules.black.byoyomiMs = secondsToMs(data.byoyomiSec1);                          // 秒読み（秒 → ms）
rules.white.byoyomiMs = secondsToMs(data.byoyomiSec2);
rules.black.incrementMs = secondsToMs(data.addEachMoveSec1);                    // フィッシャー加算（秒 → ms）
rules.white.incrementMs = secondsToMs(data.addEachMoveSec2);

TimeSettings tc;                         // GameStartCoordinator::TimeSettings
tc.loseOnTimeout = data.isLoseOnTimeout;
tc.rules = rules.normalized();
tc.enabled = !tc.rules.isUnlimited();
```

- ダイアログの時間設定をミリ秒に変換する
- 秒読み優先: どちらかの対局者に秒読みがあれば、`normalized()` が両者のフィッシャー加算を0にする（USI では秒読みと加算を同時に伝えられないため）
- どちらかの対局者に何らかの時間制限があれば `tc.enabled = true`

### ステップ7: prepare() → 時計の初期化

```cpp
Request req;
req.dialogData  = c.dialogData;
req.startSfen   = seedSfen;
req.clock       = c.clock ? c.clock : m_match->clock();
// 現在局面からの開始時は prepareDataCurrentPosition() で既にクリーンアップ済み。
// 手合割からの開始時は、ここで1回だけクリーンアップする
req.skipCleanup = (startingPosNumber == 0);

prepare(req);
```

プリセットから開始する場合は `skipCleanup` が false なので、`prepare()` の中で `requestPreStartCleanup` を発行する（プリセットからの開始で整理を行うのはこの1回だけ）。このとき開始局面と現在局面はどちらも手合割の局面で、カーソルは根（手数0）にあるので、`PreStartCleanupHandler::shouldStartFromCurrentPosition()` は現在局面からの開始と判定しない。整理は棋譜欄などをすべて消し、新しい根からライブセッションを始める。`prepare()` の内容は4章。

### ステップ8: 対局者名の通知

```cpp
emit playerNamesResolved(human1, human2, engine1, engine2, static_cast<int>(mode));
emit consecutiveGamesConfigured(consecutiveGames, switchTurn);
```

- 対局者名を通知する（`GameStartCoordinator::playerNamesResolved` → `PlayerInfoWiring::onMenuPlayerNamesResolved()`）。盤の対局者名と対局情報を更新する。EvEの初手で評価値グラフが動くので、`start()` より前に通知する
- 連続対局設定を通知する（`ConsecutiveGamesController::configure()`。連続対局はEvEのときだけ有効）

### ステップ9: 対局開始

```cpp
StartParams params;
params.opt  = opt;
params.tc   = tc;
params.autoStartEngineMove = false;  // 初手goはまだ呼ばない

start(params);
```

`start()` の内容は5章。その後、現在局面から開始した場合（`startingRow >= 0`）は `requestSelectKifuRow(startingRow)` を発行し、棋譜欄の開始行を選ばせる（`KifuNavigationCoordinator::selectKifuRow()`）。

### ステップ10: 時計開始とエンジン初手

```cpp
if (m_match) {
    if (c.clock && m_match->clock() != c.clock) {
        m_match->setClock(c.clock);
    }
    if (ShogiClock* clk = m_match->clock()) {
        clk->startClock();
    }
    // 初手がエンジン手番なら go を起動（1回だけ）
    m_match->startInitialEngineMoveIfNeeded();
}
return true;
```

- 時計をMatchCoordinatorに関連付けて開始する
- 手番の側がエンジンなら、初手の思考を要求する（8章）

---

## 4. GameStartCoordinator::prepare(Request)

**ファイル:** `src/game/gamestartcoordinator.cpp`

開始前の時計初期化と配線を行う軽量API。

### 処理内容

1. **UIプレクリア**（skipCleanupがfalseの場合）
   ```cpp
   emit requestPreStartCleanup();
   ```
   `SessionLifecycleCoordinator::performPreStartCleanup()` が受け、`PreStartCleanupHandler::performCleanup()` が棋譜欄・分岐・ログ・時計表示・評価値グラフの整理とライブセッションの開始を行う。

2. **ダイアログの値から時間設定を作る**
   ```cpp
   const TimeSettings tc = GameStartOptionsBuilder::buildTimeControl(req.dialogData);
   ```

3. **時計適用シグナルの発行**
   ```cpp
   emit requestApplyTimeControl(tc);
   ```
   `SessionLifecycleCoordinator::applyTimeControl()` が受け、連続対局用に時間設定を控え、`TimeControlController::applyTimeControl()`（持ち時間と開始日時の記録、時計への適用、時間表示の更新）を呼び、時間制限があれば対局情報に持ち時間を加える。

4. **時計への時間制御適用**
   ```cpp
   TimeControlUtil::applyToClock(clock, tc, req.startSfen, QString());
   ```
   時計は `req.clock`、無ければ `MatchCoordinator::clock()` を使う。どちらも無ければ警告を出して戻る。

5. **初期手番の設定と時計開始**
   ```cpp
   const Turn initialPlayer = req.startSfen.contains(QLatin1String(" w ")) ? Turn::White : Turn::Black;
   clock->setCurrentPlayer(initialPlayer);
   m_match->setClock(clock);
   clock->startClock();
   ```

---

## 5. GameStartCoordinator::start(StartParams)

**ファイル:** `src/game/gamestartcoordinator.cpp`

StartOptionsを受け取り、MatchCoordinatorへ対局開始を指示する。

### 処理内容

1. **バリデーション**
   ```cpp
   if (!validate(params, reason)) return;
   ```
   MatchCoordinator が無い、対局モードが `PlayMode::NotStarted`、開始局面の玉が先手・後手1枚ずつでない、のいずれかなら警告ログを出して戻る。

2. **時間の決まりの正規化**
   - `tc.rules = tc.rules.normalized()`（秒読み優先でフィッシャー加算を落とす）
   - `enabled` フラグの補正（値があるのにfalseの場合を救済）

3. **UI（時計）へ適用依頼**
   ```cpp
   if (tc.enabled || hasAny) {
       emit requestApplyTimeControl(tc);
   }
   ```

4. **時間の決まりは時計だけが持つ**
   - 司令塔（MatchCoordinator）へ時間の決まりを写すことはしない。エンジンへの `go` の時間は、送信時にエンジンにつないだ時計から `UsiGoTimeCalculator` が求める（[時間設定の仕様](game-time-control-spec.md) の「時間の決まりの持ち方」）

5. **対局者名の通知（`params.announcePlayerNames` のときだけ）**
   ```cpp
   emit playerNamesResolved(params.opt.humanName1, params.opt.humanName2,
                            params.opt.engineName1, params.opt.engineName2,
                            static_cast<int>(params.opt.mode));
   ```
   - 対局ダイアログを経ない連続対局の2局目以降（`ConsecutiveGamesController::launchPreparedNextGame()`）で使う。ダイアログ経由ではステップ8で通知済み。
   - 持ち時間と開始日時を記録した後、対局開始の前に通知し、盤の対局者名と対局情報（対局日・開始日時・対局者・持ち時間）を新しい対局に合わせる。

6. **対局開始**
   ```cpp
   m_match->configureAndStart(params.opt);
   ```

7. **初手go（`params.autoStartEngineMove` のときだけ）**
   ```cpp
   if (params.autoStartEngineMove) {
       m_match->startInitialEngineMoveIfNeeded();
   }
   ```
   ダイアログ経由では false で、初手はステップ10で要求する。連続対局の2局目以降は true。

8. **完了通知**
   ```cpp
   emit started(params.opt);
   ```
   `UiStatePolicyManager::transitionToDuringGame()`（メニューなどを対局中の状態にする）、`SessionLifecycleCoordinator::handleGameStarted()`（PlayMode と現在局面SFENの更新、定跡ウィンドウの更新、連続対局への通知）、`GameSessionOrchestrator::discardInterruptedGame()` がこの順に受ける（`MatchCoordinatorWiring::connectGameStartSignals()` がつないだ順）。

---

## 6. MatchCoordinator::configureAndStart(StartOptions)

**ファイル:** `src/game/matchcoordinator.cpp`, `src/game/gamestartorchestrator.cpp`

対局進行の司令塔（MatchCoordinator）による対局開始の中核処理。実際の手順は `GameStartOrchestrator`（非QObject、MatchCoordinator が `unique_ptr` で持つ）が、MatchCoordinator の状態への参照（Refs）とコールバック（Hooks）を使って行う。

`configureAndStart()` を呼ぶ経路には、ここで説明する `GameStartCoordinator::start()` のほかに `MatchCoordinator::prepareAndStartGame(mode, startSfen, sfenRecord, dlg, Turn bottomSide)` がある。こちらは `buildStartOptions()` → `ensureHumanAtBottomIfApplicable()` → `configureAndStart()` → `startInitialEngineMoveIfNeeded()` を続けて行い、起動時の `SessionLifecycleCoordinator::startNewGame()` → `MainWindowServiceRegistry::startGameSession()` がダイアログの値なし（`dlg == nullptr`）で呼ぶ。

### ステップ6.0: 中断した対局の破棄と指し手列の整理

```cpp
void MatchCoordinator::configureAndStart(const StartOptions& opt)
{
    discardInterruptedGame();
    m_lastStartOptions = opt;
    alignGameMovesWithSfenHistory();   // 指し手列を「局面列の件数 - 1」件に切り詰める
    ensureGameStartOrchestrator();
    m_gameStartOrchestrator->configureAndStart(opt);
}
```

- 中断した対局を再開するための状態を捨て、今回の StartOptions を控える
- 対局開始の準備で局面列は開始する局面までに切り詰められている。それより後の手（前の対局や読み込んだ棋譜の続き）が指し手列に残っていると、`moveCommitted` の手数や「待った」が前の手を数えてしまうので、ここで捨てる
- 以降の手順（6.1〜6.7）は `GameStartOrchestrator::configureAndStart()` が行う

```cpp
void GameStartOrchestrator::configureAndStart(const StartOptions& opt)
{
    const QString targetSfen = normalizeTargetSfen(opt.sfenStart);
    const HistorySearchResult searchResult = syncAndSearchGameHistory(targetSfen);  // 6.1, 6.2
    applyStartOptionsAndHooks(opt);                                                 // 6.3〜6.5
    buildPositionStringsFromHistory(opt, targetSfen, searchResult);                 // 6.6
    if (m_hooks.createAndStartModeStrategy) m_hooks.createAndStartModeStrategy(opt); // 6.7
}
```

### ステップ6.1: 前回対局履歴の保存

```cpp
if (!m_refs.positionStr1->isEmpty()
    && m_refs.positionStr1->startsWith(QLatin1String("position "))) {
    // positionStrHistory の末尾を positionStr1（直前の対局の最終状態）にそろえる
}
if (!m_refs.positionStrHistory->isEmpty()) {
    m_refs.allGameHistories->append(*m_refs.positionStrHistory);
    // 最大10件（kMaxGameHistories）に制限
}
```

- 前回対局のUSI position文字列を履歴に保存する（再対局時の位置一致検索用）

### ステップ6.2: 開始SFENの過去対局履歴検索

```cpp
// 開始SFENを正規化（"startpos"→平手SFEN、"position sfen ..."→素のSFEN）して、
// SfenPositionTracer で各対局の手順を1手ずつ進めながら一致する局面を探す
```

- 過去の対局履歴から同一開始局面を探し、最初に一致した対局のposition文字列をベースとして再利用する
- エンジンへ送信するposition文字列の継続性を保つための仕組み

### ステップ6.3: 終局状態クリア・設定保存

```cpp
if (m_hooks.clearGameOverState) m_hooks.clearGameOverState();

*m_refs.playMode = opt.mode;
*m_refs.maxMoves = opt.maxMoves;

*m_refs.autoSaveKifu = opt.autoSaveKifu;
*m_refs.kifuSaveDir = opt.kifuSaveDir;
```

`m_refs` の各項目は MatchCoordinator のメンバ（`m_playMode`、`m_maxMoves`、`m_autoSaveKifu`、`m_kifuSaveDir`）を指す。

### ステップ6.4: Hooks経由でUI初期化

```cpp
if (m_hooks.initializeNewGame) m_hooks.initializeNewGame(opt.sfenStart);
if (m_hooks.setPlayersNames)   m_hooks.setPlayersNames(QString(), QString());
if (m_hooks.setEngineNames)    m_hooks.setEngineNames(opt.engineName1, opt.engineName2);
```

`GameStartOrchestrator::Hooks` のこれらは、`MatchCoordinator::ensureGameStartOrchestrator()` が `MatchCoordinatorHooks`（`src/game/matchcoordinatorhooks.h`）の `game.initializeNewGame`・`ui.setPlayersNames`・`ui.setEngineNames` をそのまま渡したものである。`MatchCoordinatorHooks` は `MainWindowServiceRegistry::buildMatchHooks()`（`src/app/gamesubregistry_wiring.cpp`）が組み立てる:

| Hook | 呼び先 | 処理内容 |
|---|---|---|
| `game.initializeNewGame` | `MainWindowMatchAdapter::initializeNewGameHook()` | 盤をリセットして開始SFENを適用し、盤を描き直し、対局者名・エンジン名の表示を更新 |
| `ui.setPlayersNames` | `PlayerInfoWiring::onSetPlayersNames()` | 盤の対局者名ラベルと対局情報の先手・後手を設定（ここでは空文字列を渡す） |
| `ui.setEngineNames` | `PlayerInfoWiring::onSetEngineNames()` | エンジン名を通信ログ・思考タブ・評価値グラフに設定。対局のモードなら盤の対局者名と対局情報も更新 |

投了・中断などのメニューの有効・無効は Hook では切り替えず、`GameStartCoordinator::started` を受けた `UiStatePolicyManager::transitionToDuringGame()` が切り替える（5章）。

#### initializeNewGame Hookの詳細

`MainWindowMatchAdapter::initializeNewGameHook()`（`src/app/mainwindowmatchadapter.cpp`）は `MainWindowCoreInitCoordinator::initializeNewGame()` を呼び、盤を `ShogiView::applyBoardAndRender()` で描き直し、`PlayerInfoWiring` の `applyPlayersNamesForMode()`・`applyEngineNamesToLogModels()`・`applySecondEngineVisibility()` で表示名を更新する。

```cpp
void MainWindowCoreInitCoordinator::initializeNewGame(QString& startSfenStr)
{
    auto* gc = m_deps.gameController ? *m_deps.gameController : nullptr;
    if (gc) {
        gc->newGame(startSfenStr);
    }
}
```

- `ShogiGameController::newGame()` で盤を作り直して開始SFENを適用し、SFENの手番を `setCurrentPlayer()` で反映する。前の対局の最終手も消す

### ステップ6.5: 開始手番の決定

```cpp
const Turn startSide = decideStartSideFromSfen(opt.sfenStart);
if (m_refs.gc) m_refs.gc->setCurrentPlayer(startSide);
if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();

*m_refs.currentMoveIndex = 0;

*m_refs.currentTurn =
    (startSide == Turn::White) ? Turn::White
                               : Turn::Black;
if (m_hooks.updateTurnDisplay) m_hooks.updateTurnDisplay(*m_refs.currentTurn);
```

- SFENの手番フィールド（`b` or `w`。`"position sfen ..."` の形にも対応）から開始手番を判定する
- ShogiGameControllerに手番を設定し、盤面を描画する
- 手数を0に戻し、司令塔の手番（`MatchCoordinator::m_cur`）を開始手番にして手番表示を更新する（`MatchTurnHandler::updateTurnDisplay()` → `ui.updateTurnDisplay` → `MainWindow::onTurnManagerChanged()`）

### ステップ6.6: position文字列の初期化

過去対局履歴からの一致検索結果に基づいて、USI position文字列（`m_positionStr1`、`m_positionPonder1`、`m_positionStrHistory`）を初期化する。

- 履歴一致あり: 開始SFENの手数フィールド（N）から残す手数（N-1）を求め、一致した対局のposition文字列をその手数まで切り詰めて使う。履歴は切り詰めた1件から始める
- 履歴一致なし（または手数が足りない）: `MatchTurnHandler::initPositionStringsFromSfen()` でSFENから作る（平手の完全SFENなら `position startpos`、それ以外は `position sfen <SFEN>`）。開始SFENが平手の初期配置（先手番・持ち駒なし）で手数フィールドが2以上なら `position startpos` にする

### ステップ6.7: モード別の開始処理

`Hooks::createAndStartModeStrategy` は `MatchTurnHandler::createAndStartModeStrategy()`（`src/game/matchturnhandler.cpp`）を呼ぶ。PlayModeに応じて Strategy を作り、`start()` を呼ぶ:

```cpp
switch (*m_refs.playMode) {
case PlayMode::EvenEngineVsEngine:
case PlayMode::HandicapEngineVsEngine: {
    if (m_hooks.initEnginesForEvE)
        m_hooks.initEnginesForEvE(opt.engineName1, opt.engineName2);
    if (m_hooks.initAndStartEngine) {
        m_hooks.initAndStartEngine(Turn::Black, opt.enginePath1, opt.engineName1);
        m_hooks.initAndStartEngine(Turn::White, opt.enginePath2, opt.engineName2);
    }
    m_strategy = std::make_unique<EngineVsEngineStrategy>(
        *m_strategyCtx, opt, m_refs.mcAsParent);
    m_strategy->start();
    break;
}

case PlayMode::EvenEngineVsHuman:
case PlayMode::HandicapEngineVsHuman:
    m_strategy = std::make_unique<HumanVsEngineStrategy>(
        *m_strategyCtx, Turn::Black, opt.enginePath1, opt.engineName1);
    m_strategy->start();
    break;

case PlayMode::EvenHumanVsEngine:
case PlayMode::HandicapHumanVsEngine:
    m_strategy = std::make_unique<HumanVsEngineStrategy>(
        *m_strategyCtx, Turn::White, opt.enginePath2, opt.engineName2);
    m_strategy->start();
    break;

case PlayMode::HumanVsHuman:
    m_strategy = std::make_unique<HumanVsHumanStrategy>(*m_strategyCtx);
    m_strategy->start();
    break;
```

- エンジンの手番はモードで決まる。先手がエンジンのモードではエンジン1（`enginePath1`／`engineName1`）を `Turn::Black` で、後手がエンジンのモードではエンジン2（`enginePath2`／`engineName2`）を `Turn::White` で `HumanVsEngineStrategy` に渡す
- Strategy は `GameModeStrategy`（`src/game/gamemodestrategy.h`）を継承し、MatchCoordinator の状態には `MatchCoordinator::StrategyContext`（`src/game/strategycontext.h`）を通して触れる

---

## 7. モード別の開始処理

### 7.1 HumanVsHumanStrategy::start (HvH)

**ファイル:** `src/game/humanvshumanstrategy.cpp`

```cpp
const Turn side = m_ctx.gc() ? m_ctx.gc()->currentPlayer() : Turn::Black;

m_ctx.setCurrentTurn(side);

if (m_ctx.hooks().ui.renderBoardFromGc)
    m_ctx.hooks().ui.renderBoardFromGc();
m_ctx.updateTurnDisplay(m_ctx.currentTurn());
```

- GCから現在の手番を取得し、司令塔の手番（`m_cur`）を同期
- 盤面描画と手番表示を更新

エンジンの起動は不要。ユーザーのクリック入力を待つ。

### 7.2 HumanVsEngineStrategy::start (HvE)

**ファイル:** `src/game/humanvsenginestrategy.cpp`

```cpp
HumanVsEngineStrategy(MatchCoordinator::StrategyContext& ctx,
                      Turn engineSide,   // エンジンの手番（人間はその相手）
                      QString enginePath,
                      QString engineName);
```

1. **既存エンジンの破棄**
   ```cpp
   m_ctx.destroyEngines();
   ```

2. **思考モデル(通信ログ・思考情報)の準備**
   エンジン1の枠（`comm1`／`think1`）を使う。無ければ作る。通信ログにエンジンの表示名を設定する。

3. **USIエンジンの生成**
   ```cpp
   m_ctx.setUsi1(new Usi(comm, think, m_ctx.gc(), m_ctx.coordinatorAsParent()));
   m_ctx.setUsi2(nullptr);
   ```
   人間対エンジンでは、エンジンは手番によらず `usi1`（`MatchCoordinator::primaryEngine()`）に入り、`usi2` は空のままにする。

4. **投了・入玉宣言シグナルの配線**
   ```cpp
   m_ctx.wireResignToArbiter(m_ctx.usi1(), m_engineSide);
   m_ctx.wireWinToArbiter(m_ctx.usi1(), m_engineSide);
   ```
   `EngineLifecycleManager::wireResignToArbiter(engine, Turn side)` は、エンジンの `bestMoveResignReceived` を手番 `side` の投了として `MatchCoordinator::handleEngineResign(side)` へつなぐ（入玉宣言勝ちも同様に `handleEngineWin(side)`）。続いてログ識別（`[E1]` と手番）を設定し、`Usi::engineInitialized` → `onEngineInitialized()`、`Usi::matchMoveReady` → `onEngineMoveReady()` を接続する。

5. **USIエンジンの起動**
   ```cpp
   m_ctx.initializeAndStartEngineFor(m_engineSide, m_enginePath, m_engineName);
   ```
   起動は非同期で、ここでは完了を待たない（8章）。

6. **手番の確立と盤面描画**
   ```cpp
   const Turn side = m_ctx.gc() ? m_ctx.gc()->currentPlayer() : Turn::Black;
   m_ctx.setCurrentTurn(side);

   if (m_ctx.hooks().ui.renderBoardFromGc) m_ctx.hooks().ui.renderBoardFromGc();
   m_ctx.updateTurnDisplay(m_ctx.currentTurn());
   ```

### 7.3 EngineVsEngineStrategy::start (EvE)

**ファイル:** `src/game/enginevsenginestrategy.cpp`

`start()` の前に、6.7 で `EngineLifecycleManager::initEnginesForEvE()` が既存のエンジンを破棄して2つのエンジン（`usi1` を先手、`usi2` を後手）を作り、投了・入玉宣言を手番ごとに配線し、`initializeAndStartEngineFor(Turn::Black, ...)`／`(Turn::White, ...)` で両方を起動する。

1. **時計の開始**（EvEでは初手からタイマーを動かすため、ここで時計を開始）
2. **手番の同期**: GCの手番（開始SFENの手番）を司令塔の手番にして表示する。駒落ちのプリセットのように開始SFENが後手番なら、後手（上手）から始まる
3. **EvE用position文字列の初期化**（`initPositionStringsForEvE()`。中断からの再開でなければ）。`HandicapEngineVsEngine` で開始局面の盤が平手の初期配置でなければ `position sfen <SFEN> moves`、それ以外は `position startpos moves`
4. **両エンジンの `engineInitialized` と `matchMoveReady` を接続し、`kickNextEvETurn()` を呼ぶ**

`kickNextEvETurn()` では:
- どちらかのエンジンが初期化中なら時計を止めて戻り、`engineInitialized` を受けてから再開する
- 手番の側のエンジン（先手なら `usi1`、後手なら `usi2`）に `Usi::requestMatchMove()` で非同期に思考を要求する。USI時間パラメータは、エンジンにつないだ時計から `UsiGoTimeCalculator` が求める

`matchMoveReady` を受けた `onEngineMoveReady()` では:
- `ShogiGameController::applyMove()` でエンジンの指し手を盤面に適用（反則手なら指したエンジンの反則負け）
- 相手エンジンのposition文字列を同期
- 時計の消費時間を設定して着手を確定（`setMeasuredConsiderationTime()`、`commitMove()`）
- 棋譜に1行追記
- 盤面描画・ハイライト表示・手番表示を更新
- 千日手と最大手数を判定し、続くなら `QTimer::singleShot(0, ...)` で `kickNextEvETurn()` を呼んで交互に続行

---

## 8. エンジンの初期化と起動

### initializeAndStartEngineFor()

**ファイル:** `src/game/matchcoordinator_engine.cpp`, `src/game/enginelifecyclemanager.cpp`, `src/engine/usi.cpp`, `src/engine/usi_async.cpp`

```cpp
void MatchCoordinator::initializeAndStartEngineFor(Turn side,   // エンジンの手番
                                                   const QString& enginePathIn,
                                                   const QString& engineNameIn)
```

1. `EngineLifecycleManager::initializeAndStartEngineFor()` が、`side` が先手なら `usi1`、後手なら `usi2` を選ぶ。`usi2` が空で `usi1` があるとき（人間対エンジンでエンジンが後手）は `usi1` を使う
2. `Usi::errorOccurred` を `EngineLifecycleManager::engineError` につなぐ（`MatchCoordinator::onUsiError()` へキュー接続で届く）
3. `Usi::initializeAndStartEngineCommunication()` → `startAndInitializeEngineAsync()` がエンジンのオプション設定を読み、エンジンのプロセスを非同期に起動する（パスが空ならエラーを通知する）
4. プロセスが起動すると、USIプロトコルのハンドシェイク（`usi` → `usiok` → `setoption` → `isready` → `readyok` → `usinewgame`）を非同期に行い、終わると `Usi::engineInitialized` を発行する。初期化中に要求された処理は保留し、初期化の後に実行する
5. `wireResignToArbiter(eng, side)`／`wireWinToArbiter(eng, side)` で投了・入玉宣言をエンジンの手番に配線する
6. MatchCoordinator が両エンジンに `setMatchClock(clock())` で対局時計をつなぐ（`go` の時間はこの時計から求める）

### startInitialEngineMoveIfNeeded()

**ファイル:** `src/game/matchcoordinator.cpp`, `src/game/matchturnhandler.cpp`

```cpp
void MatchCoordinator::startInitialEngineMoveIfNeeded()
```

`MatchTurnHandler::startInitialEngineMoveIfNeeded()` を経て、現在の Strategy の `startInitialMoveIfNeeded()` を呼ぶ。

- HvH: 何もしない
- HvE（`HumanVsEngineStrategy::startInitialMoveIfNeeded()`）: GCの手番がエンジンの手番でなければ何もしない（人間の初手は計時を続ける）。エンジンが初期化中なら時計を止めて戻り、`engineInitialized` を受けた `onEngineInitialized()` が時計を再開してもう一度呼ぶ。初期化済みなら `Usi::requestMatchMove()` で初手の思考を要求する
- EvE: `kickNextEvETurn()` を呼ぶ（7.3）

思考の要求を受けた `UsiMatchHandler` は、エンジンにつないだ時計から `UsiGoTimeCalculator` で時間パラメータ（btime, wtime, byoyomi/binc・winc）を求め、`position` と `go` を送る。`bestmove` を受けると `Usi::matchMoveReady` で指し手を知らせ、Strategy がそれを `ShogiGameController::applyMove()` で盤面に適用する。

---

## 9. 対局中のターン管理

対局が開始された後のターン管理の仕組み。

### 人間の着手フロー（HvE）

```
ユーザーが盤面をクリック
    ↓
ShogiView::clicked シグナル（BoardSetupController::connectBoardClicks() が接続）
    ↓
BoardInteractionController::onLeftClick(pt)
    ├─ 1回目クリック: 駒を選択してハイライト表示
    └─ 2回目クリック: moveRequested(from, to) シグナル発行
    ↓
MainWindow::onMoveRequested(from, to)（MainWindowSignalRouter::connectMoveRequested() が接続）
    ↓
MainWindowServiceRegistry::handleMoveRequested(from, to)
    ↓
BoardSetupController::onMoveRequested(from, to)
    ↓
ShogiGameController::validateAndMove()
    ├─ EngineMoveValidatorで合法性検証
    ├─ 成り判定（成・不成の両方が合法なら選択ダイアログ）
    ├─ 盤面更新、指し手列・局面列に追加
    ├─ moveCommitted(Turn mover, int ply) シグナル発行
    └─ 手番変更 → currentPlayerChanged(Turn) シグナル発行
    ↓
MatchCoordinator::onHumanMove(from, to, prettyMove)
    → MatchTurnHandler::onHumanMove() → HumanVsEngineStrategy::onHumanMove()
    ├─ 人間の手のハイライト、時計で着手を確定（commitMove）、棋譜に1行追記
    ├─ 手番表示をエンジン側に切り替え
    ├─ 千日手・最大手数の判定
    └─ Usi::requestHumanReply() で人間の手をposition文字列に加えて思考を要求
       （時間パラメータはエンジンにつないだ時計から求める（UsiGoTimeCalculator））
    ↓
エンジンが bestmove を返す → Usi::matchMoveReady
    ↓
HumanVsEngineStrategy::onEngineMoveReady()
    ├─ ShogiGameController::applyMove() でエンジンの指し手を適用（反則手ならエンジンの反則負け）
    ├─ 時計で着手を確定、棋譜に1行追記、盤面・手番表示を更新
    ├─ appendEval(エンジンの手番) で評価値グラフに追記
    └─ 千日手・最大手数の判定
    ↓
手番が人間に戻る → 次のクリックを待つ
```

`MatchCoordinatorHooks::Game::appendEval(Turn side)` は、`side` が先手ならエンジン1の、後手ならエンジン2の評価値グラフを描き直す（`MainWindowServiceRegistry::buildMatchHooks()`）。

### ターン同期の仕組み

```
ShogiGameController::currentPlayerChanged(Turn)
    ↓
TurnManager::set(Turn)
    ↓
TurnManager::changed(Turn)
    ↓
MainWindow::onTurnManagerChanged(Turn)
    → MainWindowServiceRegistry::onTurnManagerChanged()
    → TurnStateSyncService::onTurnManagerChanged()
       時計の手番（ShogiClock::setCurrentPlayer(Turn)）、GCの手番、盤の手番表示（ShogiView::updateTurnIndicator()）を更新
```

`TurnSyncBridge::wire()` が GC → TurnManager と TurnManager → MainWindow の2つの接続を行い、GCの手番で初期同期する。`MainWindowServiceRegistry::prepareTurnSyncBridge()` が起動時に呼ぶ。

時計の時間切れは `ShogiClock::playerTimeOut(Turn)` で、`TimeControlController::onClockPlayerTimeOut()` が `MatchCoordinator::handlePlayerTimeOut(Turn)` に渡す。

---

## 10. 関連ファイル一覧

| コンポーネント | ヘッダ | 実装 |
|---|---|---|
| 対局ダイアログ | `src/dialogs/startgamedialog.h` | `src/dialogs/startgamedialog.cpp`, `src/dialogs/startgamedialog_settings.cpp`, `src/dialogs/startgamedialog_time.cpp` |
| ダイアログの値 | `src/game/startgamedatabridge.h` | （ヘッダーのみ） |
| MainWindow | `src/app/mainwindow.h` | `src/app/mainwindow.cpp` |
| 部品の生成と配線 | `src/app/mainwindowserviceregistry.h` | `src/app/mainwindowserviceregistry.cpp`, `src/app/gamesubregistry*.cpp`, `src/app/mainwindow*registry.cpp` |
| 対局ライフサイクル（対局開始の入口） | `src/app/gamesessionorchestrator.h` | `src/app/gamesessionorchestrator.cpp` |
| 対局開始前の局面列の作り直し・Ctx構築 | `src/app/mainwindowgamestartservice.h` | `src/app/mainwindowgamestartservice.cpp` |
| 対局開始前クリーンアップ | `src/app/prestartcleanuphandler.h` | `src/app/prestartcleanuphandler.cpp` |
| 開始前の整理・時間設定・開始通知の受け手 | `src/app/sessionlifecyclecoordinator.h` | `src/app/sessionlifecyclecoordinator.cpp` |
| 表示中の手順の値 | `src/kifu/recordline.h` | （ヘッダーのみ） |
| 対局開始コーディネータ | `src/game/gamestartcoordinator.h` | `src/game/gamestartcoordinator.cpp`, `src/game/gamestartcoordinator_dialogflow.cpp` |
| 時間設定・PlayMode・プリセット | `src/game/gamestartoptionsbuilder.h` | `src/game/gamestartoptionsbuilder.cpp` |
| 司令塔と開始コーディネータの配線 | `src/ui/wiring/matchcoordinatorwiring.h` | `src/ui/wiring/matchcoordinatorwiring.cpp` |
| 対局司令塔 | `src/game/matchcoordinator.h` | `src/game/matchcoordinator.cpp`, `src/game/matchcoordinator_engine.cpp` |
| 対局開始の設定値 | `src/game/startoptions.h` | （ヘッダーのみ） |
| 司令塔のフック | `src/game/matchcoordinatorhooks.h` | （ヘッダーのみ） |
| 対局開始オーケストレータ | `src/game/gamestartorchestrator.h` | `src/game/gamestartorchestrator.cpp` |
| ターン進行・Strategy の管理 | `src/game/matchturnhandler.h` | `src/game/matchturnhandler.cpp` |
| モード別の Strategy | `src/game/gamemodestrategy.h`, `src/game/strategycontext.h` | `src/game/humanvshumanstrategy.cpp`, `src/game/humanvsenginestrategy.cpp`, `src/game/enginevsenginestrategy.cpp` |
| エンジンの生成・起動・投了の配線 | `src/game/enginelifecyclemanager.h` | `src/game/enginelifecyclemanager.cpp` |
| 司令塔のフックの受け手 | `src/app/mainwindowmatchadapter.h` | `src/app/mainwindowmatchadapter.cpp` |
| 新しい対局の盤の初期化 | `src/app/mainwindowcoreinitcoordinator.h` | `src/app/mainwindowcoreinitcoordinator.cpp` |
| ゲームコントローラ | `src/game/shogigamecontroller.h` | `src/game/shogigamecontroller.cpp` |
| 手番の型 | `src/core/shogitypes.h` | （ヘッダーのみ） |
| 対局モード | `src/core/playmode.h` | （ヘッダーのみ） |
| ターンマネージャ | `src/game/turnmanager.h` | `src/game/turnmanager.cpp` |
| ターン同期ブリッジ | `src/game/turnsyncbridge.h` | `src/game/turnsyncbridge.cpp` |
| 手番の表示の同期 | `src/app/turnstatesyncservice.h` | `src/app/turnstatesyncservice.cpp` |
| 対局アクション配線 | `src/ui/wiring/gameactionswiring.h` | `src/ui/wiring/gameactionswiring.cpp` |
| 盤面操作コントローラ | `src/board/boardinteractioncontroller.h` | `src/board/boardinteractioncontroller.cpp` |
| 着手要求の処理 | `src/ui/controllers/boardsetupcontroller.h` | `src/ui/controllers/boardsetupcontroller.cpp` |
| USIエンジン | `src/engine/usi.h` | `src/engine/usi.cpp`, `src/engine/usi_async.cpp` |
| 対局の思考の要求 | `src/engine/usimatchhandler.h` | `src/engine/usimatchhandler.cpp` |
| goの時間の計算 | `src/engine/usigotime.h` | `src/engine/usigotime.cpp` |
| 時間の決まり | `src/core/timecontrol.h` | （ヘッダーのみ） |
| 時計への時間設定の適用 | `src/services/timecontrolutil.h` | `src/services/timecontrolutil.cpp` |
| 時計の管理 | `src/ui/controllers/timecontrolcontroller.h` | `src/ui/controllers/timecontrolcontroller.cpp` |
| 将棋時計 | `src/core/shogiclock.h` | `src/core/shogiclock.cpp` |
