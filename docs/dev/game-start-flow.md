# 対局ダイアログから対局開始までのフロー

本ドキュメントでは、GUIの「対局」メニューから「対局開始」をクリックし、実際に対局が始まるまでに実行される関数とその内容を時系列で説明する。

## 全体フロー図

```
ユーザー操作: メニュー「対局」→「対局開始」クリック
    │
    ▼
UiActionsWiring::wire()
    actionStartGame::triggered → MainWindow::initializeGame()
    │
    ▼
MainWindow::initializeGame()
    │  現在局面SFENの解決、コンテキスト(Ctx)の構築
    │
    ▼
GameStartCoordinator::initializeGame(Ctx)
    │  1) StartGameDialogの生成・表示（モーダル）
    │  2) ダイアログ設定値の取得
    │  3) 開始SFENの決定（現在局面 or プリセット）
    │  4) PlayModeの決定
    │  5) StartOptionsの構築
    │  6) TimeControlの構築
    │  7) prepare() → 時計の初期化
    │  8) playerNamesResolved シグナル発行
    │  9) start(StartParams) → 対局開始
    │  10) 時計開始 + エンジン初手
    │
    ▼
GameStartCoordinator::start(StartParams)
    │  TimeControlの正規化・適用
    │  MatchCoordinator::setTimeControlConfig()
    │
    ▼
MatchCoordinator::configureAndStart(StartOptions)
    │  1) 前回対局履歴の保存
    │  2) 終局状態のクリア
    │  3) 対局設定の保存
    │  4) Hooks経由でUI初期化
    │  5) 開始手番の決定（SFEN解析）
    │  6) position文字列の初期化
    │  7) モード別の開始処理（HvH/HvE/EvE）
    │
    ▼
対局開始（エンジン初手goが必要なら発行）
```

---

## 1. メニューアクションの接続

**ファイル:** `src/ui/wiring/uiactionswiring.cpp:32`

```cpp
QObject::connect(ui->actionStartGame, &QAction::triggered,
                 mw, &MainWindow::initializeGame, Qt::UniqueConnection);
```

ユーザーがメニュー「対局」→「対局開始」をクリックすると、`actionStartGame` の `triggered` シグナルが発火し、`MainWindow::initializeGame()` スロットが呼ばれる。

---

## 2. MainWindow::initializeGame()

**ファイル:** `src/app/mainwindow.cpp:1068-1128`

対局開始フローのエントリポイント。GameStartCoordinatorに渡すコンテキスト（Ctx）を準備する。

### 処理内容

1. **GameStartCoordinatorの遅延初期化**
   ```cpp
   ensureGameStartCoordinator();
   ```

2. **開始SFENのクリア**
   ```cpp
   m_startSfenStr.clear();
   ```
   平手SFENが優先されてしまう問題の対策として、明示的にクリアする。

3. **現在局面SFENの解決**
   ```cpp
   const QString sfen = resolveCurrentSfenForGameStart().trimmed();
   if (!sfen.isEmpty()) {
       m_currentSfenStr = sfen;
   }
   ```
   棋譜レコードから現在の局面SFENを最優先で取得する。

4. **分岐ツリーからのSFEN再構築**（該当する場合）
   ```cpp
   if (m_branchTree != nullptr && m_navState != nullptr
       && m_sfenRecord != nullptr && m_currentSelectedPly > 0) {
       // 現在のラインのSFENでm_sfenRecordを再構築
   }
   ```
   分岐ツリーから途中局面で再対局する場合、異なる分岐のSFENが混在する問題を防ぐ。

5. **Ctxの構築とinitializeGame呼び出し**
   ```cpp
   GameStartCoordinator::Ctx c;
   c.view            = m_shogiView;
   c.gc              = m_gameController;
   c.clock           = m_timeController ? m_timeController->clock() : nullptr;
   c.sfenRecord      = m_sfenRecord;
   c.kifuModel       = m_kifuRecordModel;
   c.kifuLoadCoordinator = m_kifuLoadCoordinator;
   c.currentSfenStr  = &m_currentSfenStr;
   c.startSfenStr    = &m_startSfenStr;
   c.selectedPly     = m_currentSelectedPly;
   c.isReplayMode    = m_replayController ? m_replayController->isReplayMode() : false;
   c.bottomIsP1      = m_bottomIsP1;

   m_gameStart->initializeGame(c);
   ```

### Ctx構造体の内容

| フィールド | 型 | 説明 |
|---|---|---|
| `view` | `ShogiView*` | 盤面ビュー |
| `gc` | `ShogiGameController*` | ゲームコントローラ |
| `clock` | `ShogiClock*` | 将棋時計 |
| `sfenRecord` | `QStringList*` | SFEN履歴リスト |
| `kifuModel` | `KifuRecordListModel*` | 棋譜欄モデル |
| `kifuLoadCoordinator` | `KifuLoadCoordinator*` | 分岐構造の設定用 |
| `currentSfenStr` | `QString*` | 現在局面のSFEN文字列 |
| `startSfenStr` | `QString*` | 開始SFEN文字列（空） |
| `selectedPly` | `int` | 選択中の手数 |
| `isReplayMode` | `bool` | 再生モード中か |
| `bottomIsP1` | `bool` | 手前が先手か |

---

## 3. GameStartCoordinator::initializeGame(Ctx)

**ファイル:** `src/game/gamestartcoordinator.cpp:698-991`

対局開始の中核ロジック。ダイアログ表示から対局開始までの一連のフローを実行する。

### ステップ1: ダイアログ生成・表示（行706-713）

```cpp
StartGameDialog* dlg = new StartGameDialog;
if (dlg->exec() != QDialog::Accepted) {
    delete dlg;
    return;
}
```

- `StartGameDialog` をモーダルダイアログとして表示する
- ユーザーがキャンセルした場合はここで終了
- ダイアログ内では前回の設定が `QSettings` から復元される

#### StartGameDialogの主な設定項目

| カテゴリ | 設定項目 |
|---|---|
| **対局者** | 先手/後手の種別（人間/エンジン選択）、人間名、エンジン選択 |
| **持ち時間** | 持ち時間（時/分）、秒読み、フィッシャー加算（先手/後手別設定可） |
| **開始局面** | 平手、香落ち、角落ち、飛車落ち、二枚落ち...十枚落ち（14種類） |
| **対局オプション** | 最大手数、連続対局数（EvEのみ）、棋譜自動保存、時間切れ負け、持将棋ルール |

#### ダイアログOKボタンの処理

OKボタンクリック時、以下の3つの処理が接続されている:
1. `accept()` → ダイアログを閉じる
2. `saveGameSettings()` → QSettingsに永続化
3. `updateGameSettingsFromDialog()` → UI値をメンバ変数にコピー

### ステップ2: ダイアログ設定値の取得（行715-718）

```cpp
const int  initPosNo = dlg->startingPositionNumber();
const bool p1Human   = dlg->isHuman1();
const bool p2Human   = dlg->isHuman2();
```

| 値 | 説明 |
|---|---|
| `initPosNo` | 開始局面番号（0=現在局面, 1=平手, 2-14=駒落ち） |
| `p1Human` | 先手が人間か |
| `p2Human` | 後手が人間か |

### ステップ3: 開始SFENの決定（行722-824）

#### 現在局面から開始する場合（`initPosNo == 0`）

```cpp
Ctx c2 = c;
c2.startDlg = dlg;
prepareDataCurrentPosition(c2);
```

`prepareDataCurrentPosition()` の処理:
- `requestPreStartCleanup()` シグナルを発行してUIをクリア
- 現在の盤面位置を適用
- 終局状態をクリア
- ハイライトをクリア

その後、`c.currentSfenStr` から開始SFENを取得する。

#### 平手/駒落ちプリセットから開始する場合（`initPosNo >= 1`）

```cpp
static const std::array<QString, 15> presets = {{
    QString(),
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),  // 平手
    QStringLiteral("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),  // 香落ち
    // ... 他の駒落ちSFEN
}};
startSfen = presets[idx];

Ctx c2 = c;
c2.startDlg = dlg;
prepareInitialPosition(c2);
```

`prepareInitialPosition()` の処理:
- 選択された駒落ちのSFENを設定
- 棋譜に「=== 開始局面 ===」ヘッダを追加
- SFEN記録をクリアして開始SFENを追加
- 分岐ツリーを `resetBranchTreeForNewGame()` でリセット

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

### ステップ3.5: SFEN正規化とsfenRecord seed（行826-874）

```cpp
const QString seedSfen = canonicalizeStart(startSfen);
```

- `"startpos"` や空文字列を平手の完全SFEN表記に正規化する
- `sfenRecord` に開始SFENをseedとして設定する
- 現在局面から開始の場合は、0手目からselectedPlyまでを保全し末尾を `seedSfen` に置換

### ステップ4: PlayModeの決定（行877）

```cpp
PlayMode mode = determinePlayModeAlignedWithTurn(initPosNo, p1Human, p2Human, seedSfen);
```

SFENの手番（`b`=先手/`w`=後手）と対局者タイプ（人間/エンジン）から、対局モードを決定する。

| PlayMode | 説明 |
|---|---|
| `HumanVsHuman` | 人間 vs 人間 |
| `EvenHumanVsEngine` | 平手・人間先手 vs エンジン後手 |
| `EvenEngineVsHuman` | 平手・エンジン先手 vs 人間後手 |
| `HandicapHumanVsEngine` | 駒落ち・人間上手 vs エンジン下手 |
| `HandicapEngineVsHuman` | 駒落ち・エンジン上手 vs 人間下手 |
| `EvenEngineVsEngine` | 平手・エンジン vs エンジン |
| `HandicapEngineVsEngine` | 駒落ち・エンジン vs エンジン |

### ステップ5: StartOptionsの構築（行880-888）

```cpp
MatchCoordinator::StartOptions opt =
    m_match->buildStartOptions(mode, seedSfen, c.sfenRecord, dlg);
m_match->ensureHumanAtBottomIfApplicable(dlg, c.bottomIsP1);
```

`buildStartOptions()` はダイアログからStartOptionsを構築する:

```cpp
struct StartOptions {
    PlayMode mode;              // 対局モード
    QString  sfenStart;         // 開始SFEN
    QString  engineName1;       // エンジン1表示名
    QString  enginePath1;       // エンジン1実行パス
    QString  engineName2;       // エンジン2表示名
    QString  enginePath2;       // エンジン2実行パス
    bool     engineIsP1;        // エンジンが先手か（HvE）
    bool     engineIsP2;        // エンジンが後手か（HvE）
    int      maxMoves;          // 最大手数（0=無制限）
    bool     autoSaveKifu;      // 棋譜自動保存フラグ
    QString  kifuSaveDir;       // 棋譜保存ディレクトリ
    QString  humanName1;        // 先手の人間対局者名
    QString  humanName2;        // 後手の人間対局者名
};
```

`ensureHumanAtBottomIfApplicable()` は、ダイアログの「人間を手前に表示」設定に応じて盤面を反転する。

### ステップ6: TimeControlの構築（行890-934）

```cpp
TimeControl tc;
tc.p1.baseMs      = hms_to_ms(h1, m1);  // 持ち時間（時:分 → ms）
tc.p2.baseMs      = hms_to_ms(h2, m2);
tc.p1.byoyomiMs   = sec_to_ms(byo1);    // 秒読み（秒 → ms）
tc.p2.byoyomiMs   = sec_to_ms(byo2);
tc.p1.incrementMs = sec_to_ms(inc1);     // フィッシャー加算（秒 → ms）
tc.p2.incrementMs = sec_to_ms(inc2);
```

- ダイアログの時間設定をミリ秒に変換する
- **秒読み優先**: 秒読みとフィッシャー加算の両方が設定されている場合、秒読みが優先されフィッシャー加算は0になる
- いずれかの時間値が設定されていれば `tc.enabled = true`

### ステップ7: prepare() → 時計の初期化（行936-944）

```cpp
Request req;
req.startDialog = dlg;
req.startSfen   = seedSfen;
req.clock       = c.clock ? c.clock : m_match->clock();
req.skipCleanup = (startingPosNumber == 0);
prepare(req);
```

### ステップ8: 対局者名の通知（行946-958）

```cpp
emit playerNamesResolved(human1, human2, engine1, engine2, static_cast<int>(mode));
emit consecutiveGamesConfigured(consecutiveGames, switchTurn);
```

- MainWindowに対局者名を通知（UI表示更新）
- EvE対局の場合、連続対局設定を通知

### ステップ9: 対局開始（行963-970）

```cpp
StartParams params;
params.opt  = opt;
params.tc   = tc;
params.autoStartEngineMove = false;  // 初手goはまだ呼ばない
start(params);
```

### ステップ10: 時計開始とエンジン初手（行978-988）

```cpp
if (m_match) {
    if (c.clock && m_match->clock() != c.clock) {
        m_match->setClock(c.clock);
    }
    if (ShogiClock* clk = m_match->clock()) {
        clk->startClock();
    }
    m_match->startInitialEngineMoveIfNeeded();
}
delete dlg;
```

- 時計をMatchCoordinatorに関連付けて開始する
- 先手がエンジンの場合、初手の `go` コマンドを発行する
- ダイアログを解放する

---

## 4. GameStartCoordinator::prepare(Request)

**ファイル:** `src/game/gamestartcoordinator.cpp:134-177`

開始前の時計初期化と配線を行う軽量API。

### 処理内容

1. **UIプレクリア**（skipCleanupがfalseの場合）
   ```cpp
   emit requestPreStartCleanup();
   ```

2. **ダイアログから時間設定を抽出**
   ```cpp
   const TimeControl tc = extractTimeControlFromDialog(req.startDialog);
   ```

3. **時計適用シグナルの発行**
   ```cpp
   emit requestApplyTimeControl(tc);
   emit applyTimeControlRequested(tc);
   ```

4. **時計への時間制御適用**
   ```cpp
   TimeControlUtil::applyToClock(clock, tc, req.startSfen, QString());
   ```

5. **初期手番の設定と時計開始**
   ```cpp
   const int initialPlayer = (req.startSfen.contains(" w ") ? 2 : 1);
   clock->setCurrentPlayer(initialPlayer);
   m_match->setClock(clock);
   clock->startClock();
   ```

---

## 5. GameStartCoordinator::start(StartParams)

**ファイル:** `src/game/gamestartcoordinator.cpp:61-128`

StartOptionsを受け取り、MatchCoordinatorへ対局開始を指示する。

### 処理内容

1. **バリデーション**
   ```cpp
   if (!validate(params, reason)) return;
   ```

2. **TimeControlの正規化**
   - `enabled` フラグの補正（値があるのにfalseの場合を救済）
   - 秒読み優先でフィッシャー加算を落とす

3. **UI（時計）へ適用依頼**
   ```cpp
   emit requestApplyTimeControl(tc);
   emit applyTimeControlRequested(tc);
   ```

4. **MatchCoordinatorへ時間制御を直接反映**
   ```cpp
   m_match->setTimeControlConfig(
       useByoyomi,
       tc.p1.byoyomiMs, tc.p2.byoyomiMs,
       tc.p1.incrementMs, tc.p2.incrementMs,
       loseOnTimeout
   );
   m_match->refreshGoTimes();
   ```

5. **対局開始**
   ```cpp
   m_match->configureAndStart(params.opt);
   ```

6. **完了通知**
   ```cpp
   emit started(params.opt);
   ```

---

## 6. MatchCoordinator::configureAndStart(StartOptions)

**ファイル:** `src/game/matchcoordinator.cpp:697-1038`

対局進行の司令塔（MatchCoordinator）による対局開始の中核処理。

### ステップ6.1: 前回対局履歴の保存（行699-721）

```cpp
if (!m_positionStr1.isEmpty()) {
    // m_positionStrHistoryに最終状態を確定
}
if (!m_positionStrHistory.isEmpty()) {
    m_allGameHistories.append(m_positionStrHistory);
    // 最大10件に制限
}
```

- 前回対局のUSI position文字列を履歴に保存する（再対局時の位置一致検索用）

### ステップ6.2: 開始SFENの過去対局履歴検索（行723-801）

```cpp
// 開始SFENを正規化して過去のゲーム履歴から一致する位置を探す
QString targetSfen = opt.sfenStart.trimmed();
// SfenPositionTracer を使って全履歴を走査
```

- 過去の対局履歴から同一開始局面を探し、一致した場合はそのposition文字列をベースとして再利用する
- エンジンへ送信するposition文字列の継続性を保つための仕組み

### ステップ6.3: 終局状態クリア・設定保存（行803-815）

```cpp
clearGameOverState();
m_playMode = opt.mode;
m_maxMoves = opt.maxMoves;
m_autoSaveKifu = opt.autoSaveKifu;
m_kifuSaveDir = opt.kifuSaveDir;
m_humanName1 = opt.humanName1;
m_humanName2 = opt.humanName2;
m_engineNameForSave1 = opt.engineName1;
m_engineNameForSave2 = opt.engineName2;
```

### ステップ6.4: Hooks経由でUI初期化（行817-830）

```cpp
if (m_hooks.initializeNewGame) m_hooks.initializeNewGame(opt.sfenStart);
if (m_hooks.setPlayersNames)   m_hooks.setPlayersNames(QString(), QString());
if (m_hooks.setEngineNames)    m_hooks.setEngineNames(opt.engineName1, opt.engineName2);
if (m_hooks.setGameActions)    m_hooks.setGameActions(true);
```

各Hookの実体はMainWindowで設定される:

| Hook | MainWindowメソッド | 処理内容 |
|---|---|---|
| `initializeNewGame` | `initializeNewGameHook()` | ShogiGameController::newGame()で盤面リセット、盤描画 |
| `setPlayersNames` | `setPlayersNamesForMode()` | PlayModeに応じて対局者名を盤面ビューに設定 |
| `setEngineNames` | `setEngineNamesBasedOnMode()` | エンジン名を思考タブ等に設定 |
| `setGameActions` | `enableGameActions()` | 投了・中断等のメニューアクションを有効化 |

#### initializeNewGame Hookの詳細

`MainWindow::initializeNewGame()` (`mainwindow.cpp`) が呼ばれ:

```cpp
void MainWindow::initializeNewGame(QString& startSfenStr)
{
    if (m_gameController) {
        m_gameController->newGame(startSfenStr);
    }
    GameStartCoordinator::applyResumePositionIfAny(
        m_gameController, m_shogiView, m_resumeSfenStr);
}
```

- `ShogiGameController::newGame()` で盤面をリセットし開始SFENを適用する
- 途中局面からの再開の場合は、その局面を盤面に適用する

### ステップ6.5: 開始手番の決定（行832-871）

```cpp
auto decideStartSideFromSfen = [](const QString& sfen) -> ShogiGameController::Player {
    // SFEN文字列から手番（'b'=先手 / 'w'=後手）を解析
};
const ShogiGameController::Player startSide = decideStartSideFromSfen(opt.sfenStart);
if (m_gc) m_gc->setCurrentPlayer(startSide);
if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
```

- SFENの2番目のフィールド（`b` or `w`）から開始手番を判定する
- ShogiGameControllerに手番を設定する
- 盤面を描画する

### ステップ6.6: position文字列の初期化（行872-985）

過去対局履歴からの一致検索結果に基づいて、USI position文字列を初期化する。

- **履歴一致あり**: 過去の対局のposition文字列をトリムして再利用
- **履歴一致なし**: `initializePositionStringsForStart()` でSFENから新規構築

### ステップ6.7: モード別の開始処理（行987-1038）

PlayModeに応じて、適切な開始関数を呼び出す:

```cpp
switch (m_playMode) {
case PlayMode::EvenEngineVsEngine:
case PlayMode::HandicapEngineVsEngine:
    initEnginesForEvE(opt.engineName1, opt.engineName2);
    initializeAndStartEngineFor(P1, opt.enginePath1, opt.engineName1);
    initializeAndStartEngineFor(P2, opt.enginePath2, opt.engineName2);
    startEngineVsEngine(opt);
    break;

case PlayMode::EvenEngineVsHuman:
    startHumanVsEngine(opt, /*engineIsP1=*/true);
    break;

case PlayMode::EvenHumanVsEngine:
    startHumanVsEngine(opt, /*engineIsP1=*/false);
    break;

case PlayMode::HandicapEngineVsHuman:
    startHumanVsEngine(opt, /*engineIsP1=*/true);
    break;

case PlayMode::HandicapHumanVsEngine:
    startHumanVsEngine(opt, /*engineIsP1=*/false);
    break;

case PlayMode::HumanVsHuman:
    startHumanVsHuman(opt);
    break;
}
```

---

## 7. モード別の開始処理

### 7.1 startHumanVsHuman (HvH)

**ファイル:** `src/game/matchcoordinator.cpp:1044-1061`

```cpp
void MatchCoordinator::startHumanVsHuman(const StartOptions& /*opt*/)
```

- GCから現在の手番を取得（NoPlayerの場合は先手に設定）
- `m_cur` を同期
- 盤面描画と手番表示を更新

エンジンの起動は不要。ユーザーのクリック入力を待つ。

### 7.2 startHumanVsEngine (HvE)

**ファイル:** `src/game/matchcoordinator.cpp:1065-1128`

```cpp
void MatchCoordinator::startHumanVsEngine(const StartOptions& opt, bool engineIsP1)
```

1. **既存エンジンの破棄**
   ```cpp
   destroyEngines();
   ```

2. **エンジンパス・名前の選択**
   ```cpp
   const QString& enginePath = engineIsP1 ? opt.enginePath1 : opt.enginePath2;
   const QString& engineName = engineIsP1 ? opt.engineName1 : opt.engineName2;
   ```

3. **思考モデル(通信ログ・思考情報)の準備**
   ```cpp
   m_usi1 = new Usi(comm, think, m_gc, m_playMode, this);
   m_usi2 = nullptr;
   ```

4. **投了・入玉宣言シグナルの配線**
   ```cpp
   wireResignToArbiter(m_usi1, engineIsP1);
   wireWinToArbiter(m_usi1, engineIsP1);
   ```

5. **USIエンジンの起動**
   ```cpp
   initializeAndStartEngineFor(engineSide, enginePath, engineName);
   ```

6. **手番の確立と盤面描画**
   ```cpp
   m_cur = (side == ShogiGameController::Player2) ? P2 : P1;
   if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
   updateTurnDisplay(m_cur);
   ```

### 7.3 startEngineVsEngine (EvE)

**ファイル:** `src/game/matchcoordinator.cpp:1131-1163`

```cpp
void MatchCoordinator::startEngineVsEngine(const StartOptions& opt)
```

1. **時計の開始**（EvEでは即座に時計を開始）
2. **EvE用position文字列の初期化**
3. **駒落ちの場合は後手（上手）から、平手の場合は先手から開始**

```cpp
if (isHandicap && whiteToMove) {
    startEvEFirstMoveByWhite();
} else {
    startEvEFirstMoveByBlack();
}
```

`startEvEFirstMoveByBlack()` / `startEvEFirstMoveByWhite()` では:
- `computeGoTimes()` でUSI時間パラメータを計算
- `Usi::handleEngineVsHumanOrEngineMatchCommunication()` でエンジンに思考を指示（ブロッキング）
- `ShogiGameController::validateAndMove()` でエンジンの指し手を盤面に適用
- 時計の消費時間を設定
- 棋譜に1行追記
- 盤面描画・ハイライト表示を更新
- 相手エンジンにも同様の処理を行い、以降 `kickNextEvETurn()` で交互に続行

---

## 8. エンジンの初期化と起動

### initializeAndStartEngineFor()

**ファイル:** `src/game/matchcoordinator.cpp`

```cpp
void MatchCoordinator::initializeAndStartEngineFor(Player side,
                                                   const QString& enginePathIn,
                                                   const QString& engineNameIn)
```

1. エンジン実行ファイルのパスからUSIエンジンを起動
2. USIプロトコルのハンドシェイク（`usi` → `usiok` → `isready` → `readyok`）
3. エンジンオプションの設定
4. `go` コマンドを受け付けられる状態にする

### startInitialEngineMoveIfNeeded()

**ファイル:** `src/game/matchcoordinator.cpp`

```cpp
void MatchCoordinator::startInitialEngineMoveIfNeeded()
```

- 現在の手番がエンジン側であれば、初手の `go` コマンドを発行する
- 時間パラメータ（btime, wtime, byoyomi/increment）を計算してエンジンに送信
- エンジンの `bestmove` 応答を待つ
- 受信した指し手を `validateAndMove()` で盤面に適用する

---

## 9. 対局中のターン管理

対局が開始された後のターン管理の仕組み。

### 人間の着手フロー（HvE）

```
ユーザーが盤面をクリック
    ↓
ShogiView::clicked シグナル
    ↓
BoardInteractionController::onLeftClick(pt)
    ├─ 1回目クリック: 駒を選択してハイライト表示
    └─ 2回目クリック: moveRequested(from, to) シグナル発行
    ↓
BoardSetupController::onMoveRequested(from, to)
    ↓
ShogiGameController::validateAndMove()
    ├─ MoveValidatorで合法性検証
    ├─ 成り判定（人間の場合はダイアログ表示）
    ├─ 盤面更新
    ├─ moveCommitted(moverBefore, ply) シグナル発行
    └─ 手番変更 → currentPlayerChanged シグナル発行
    ↓
MatchCoordinator::onHumanMove_HvE(from, to, prettyMove)
    ├─ position文字列を更新
    ├─ computeGoTimesForUSI() で時間パラメータを計算
    └─ エンジンに go コマンドを送信
    ↓
エンジンが bestmove を返す
    ↓
エンジンの指し手を validateAndMove() で適用
    ↓
手番が人間に戻る → 次のクリックを待つ
```

### ターン同期の仕組み

```
ShogiGameController::currentPlayerChanged(Player)
    ↓
TurnManager::setFromGc(Player)
    ↓
TurnManager::changed(Player)
    ↓
MainWindow::onTurnManagerChanged(Player)
    → 手番表示の更新、アクションの有効/無効切替
```

`TurnSyncBridge` がこの3段階の接続を管理する。

---

## 10. 関連ファイル一覧

| コンポーネント | ヘッダ | 実装 |
|---|---|---|
| 対局ダイアログ | `src/dialogs/startgamedialog.h` | `src/dialogs/startgamedialog.cpp` |
| MainWindow | `src/app/mainwindow.h` | `src/app/mainwindow.cpp` |
| 対局開始コーディネータ | `src/game/gamestartcoordinator.h` | `src/game/gamestartcoordinator.cpp` |
| 対局司令塔 | `src/game/matchcoordinator.h` | `src/game/matchcoordinator.cpp` |
| ゲームコントローラ | `src/game/shogigamecontroller.h` | `src/game/shogigamecontroller.cpp` |
| ターンマネージャ | `src/game/turnmanager.h` | `src/game/turnmanager.cpp` |
| ターン同期ブリッジ | `src/game/turnsyncbridge.h` | `src/game/turnsyncbridge.cpp` |
| UIアクション配線 | `src/ui/wiring/uiactionswiring.h` | `src/ui/wiring/uiactionswiring.cpp` |
| 盤面操作コントローラ | `src/board/boardinteractioncontroller.h` | `src/board/boardinteractioncontroller.cpp` |
| USIエンジン | `src/engine/usi.h` | `src/engine/usi.cpp` |
| 将棋時計 | `src/services/shogiclock.h` | `src/services/shogiclock.cpp` |
