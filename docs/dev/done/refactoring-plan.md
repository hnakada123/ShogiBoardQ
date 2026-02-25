# ShogiBoardQ リファクタリング計画

## 概要

コードベース解析（全91,600行）に基づき、保守性・可読性・拡張性の改善が必要な箇所を優先度順にまとめる。
各ステップは独立して実施可能で、`/clear` 後にAIへ依頼するプロンプトも付記する。

---

## Phase 1: EngineAnalysisTab の分割（優先度: 最高）

**現状**: 3,085行 / 120メソッド / 74メンバ変数。6つの独立した責務が1クラスに混在。

### Step 1-1: BranchTreeManager の抽出

**対象メンバ変数:**
- `m_branchTree`, `m_scene`, `m_branchTreeViewport`
- `m_rows`, `m_nodeIndex`, `m_prevSelected`, `m_branchTreeClickEnabled`
- `m_nodeIdByRowPly`, `m_nodesById`, `m_prevIds`, `m_nextIds`, `m_rowEntryNode`, `m_nextNodeId`
- `BranchGraphNode` 構造体

**対象メソッド:**
- `setBranchTreeRows()`, `highlightBranchTreeAt()`, `clearBranchGraph()`
- `registerNode()`, `linkEdge()`, `nodeIdFor()`
- `rebuildBranchTree()` (227行), `addNode()`, `addEdge()`
- `resolveParentRowForVariation()`, `graphFallbackToPly()`, `highlightNodeId()`
- `createBranchTreePage()` のツリー構築部分
- `eventFilter()` のうちブランチツリークリック処理

**対象シグナル:**
- `branchNodeActivated(int row, int ply)`

**推定削減:** ~400行

**注意点:**
- `KifuLoadCoordinator`, `KifuDisplayCoordinator`, `AnalysisCoordinator` が `setBranchTreeRows()` / `highlightBranchTreeAt()` を使用
- `AnalysisTabWiring` が `branchNodeActivated` シグナルを接続
- `EngineAnalysisTab` は `BranchTreeManager` を所有し、公開メソッドで委譲

<details>
<summary>AIプロンプト</summary>

```
src/widgets/engineanalysistab.h/.cpp からブランチツリー管理の責務を
新クラス BranchTreeManager（src/widgets/branchtreemanager.h/.cpp）に抽出してください。

【抽出対象のメンバ変数】
m_branchTree, m_scene, m_branchTreeViewport, m_rows, m_nodeIndex,
m_prevSelected, m_branchTreeClickEnabled, m_nodeIdByRowPly, m_nodesById,
m_prevIds, m_nextIds, m_rowEntryNode, m_nextNodeId, BranchGraphNode構造体

【抽出対象のメソッド】
setBranchTreeRows(), highlightBranchTreeAt(), clearBranchGraph(),
registerNode(), linkEdge(), nodeIdFor(), rebuildBranchTree(),
addNode(), addEdge(), resolveParentRowForVariation(),
graphFallbackToPly(), highlightNodeId()

【抽出対象のシグナル】
branchNodeActivated(int row, int ply)

【設計方針】
- BranchTreeManager は QObject を継承し、branchNodeActivated シグナルを持つ
- createBranchTreePage() は EngineAnalysisTab に残し、
  QGraphicsView の作成後に BranchTreeManager へ渡す
- eventFilter() のブランチツリークリック処理部分を BranchTreeManager に移動
- EngineAnalysisTab は m_branchTreeManager を所有し、
  公開メソッド（setBranchTreeRows 等）は委譲で維持
- 既存の外部接続（KifuLoadCoordinator, KifuDisplayCoordinator 等）は
  EngineAnalysisTab 経由の委譲で変更不要にする
- connect() にラムダを使わないこと（CLAUDE.md準拠）

実装後、ビルドが通ることを確認してください。
```

</details>

---

### Step 1-2: ConsiderationTabManager の抽出

**対象メンバ変数 (20個+):**
- `m_considerationInfo`, `m_considerationView`, `m_considerationModel`
- `m_considerationTabIndex`, `m_considerationToolbar`
- `m_btnConsiderationFontDecrease`, `m_btnConsiderationFontIncrease`
- `m_engineComboBox`, `m_btnEngineSettings`
- `m_unlimitedTimeRadioButton`, `m_considerationTimeRadioButton`
- `m_byoyomiSecSpinBox`, `m_byoyomiSecUnitLabel`
- `m_elapsedTimeLabel`, `m_elapsedTimer`, `m_elapsedSeconds`
- `m_considerationTimeLimitSec`, `m_multiPVLabel`, `m_multiPVComboBox`
- `m_btnStopConsideration`, `m_showArrowsCheckBox`
- `m_considerationFontSize`, `m_considerationRunning`

**対象メソッド:**
- `createConsiderationPage()` の UI 構築部分
- `setConsiderationThinkingModel()`, `setConsiderationMultiPV()`, `considerationMultiPV()`
- `setConsiderationTimeLimit()`, `setConsiderationEngineName()`
- `isUnlimitedTime()`, `byoyomiSec()`, `selectedEngineIndex()`, `selectedEngineName()`
- `isShowArrowsChecked()`, `loadEngineList()`, `loadConsiderationTabSettings()`, `saveConsiderationTabSettings()`
- `startElapsedTimer()`, `stopElapsedTimer()`, `resetElapsedTimer()`
- `setConsiderationRunning()`, `switchToConsiderationTab()`
- `onConsiderationViewClicked()`, `onConsiderationFontIncrease/Decrease()`
- `onMultiPVComboBoxChanged()`, `onEngineComboBoxChanged()`, `onEngineSettingsClicked()`
- `onTimeSettingChanged()`, `onElapsedTimerTick()`

**対象シグナル:**
- `considerationMultiPVChanged(int)`, `stopConsiderationRequested()`, `startConsiderationRequested()`
- `engineSettingsRequested(int, const QString&)`, `considerationTimeSettingsChanged(bool, int)`
- `considerationEngineChanged(int, const QString&)`, `showArrowsChanged(bool)`

**推定削減:** ~500行

**注意点:**
- `ConsiderationModeUIController` が最大の利用者（11箇所の直接呼び出し）
- `DialogCoordinator` が状態クエリに使用（selectedEngineName, byoyomiSec 等）
- `ConsiderationWiring` がシグナル接続に使用

<details>
<summary>AIプロンプト</summary>

```
src/widgets/engineanalysistab.h/.cpp から検討モードタブの責務を
新クラス ConsiderationTabManager（src/widgets/considerationtabmanager.h/.cpp）に抽出してください。

【抽出対象のメンバ変数】
m_considerationInfo, m_considerationView, m_considerationModel,
m_considerationTabIndex, m_considerationToolbar,
m_btnConsiderationFontDecrease, m_btnConsiderationFontIncrease,
m_engineComboBox, m_btnEngineSettings,
m_unlimitedTimeRadioButton, m_considerationTimeRadioButton,
m_byoyomiSecSpinBox, m_byoyomiSecUnitLabel,
m_elapsedTimeLabel, m_elapsedTimer, m_elapsedSeconds,
m_considerationTimeLimitSec, m_multiPVLabel, m_multiPVComboBox,
m_btnStopConsideration, m_showArrowsCheckBox,
m_considerationFontSize, m_considerationRunning

【抽出対象のメソッド】
createConsiderationPage() の UI構築ロジック,
setConsiderationThinkingModel(), setConsiderationMultiPV(), considerationMultiPV(),
setConsiderationTimeLimit(), setConsiderationEngineName(),
isUnlimitedTime(), byoyomiSec(), selectedEngineIndex(), selectedEngineName(),
isShowArrowsChecked(), loadEngineList(), loadConsiderationTabSettings(),
saveConsiderationTabSettings(), startElapsedTimer(), stopElapsedTimer(),
resetElapsedTimer(), setConsiderationRunning(), switchToConsiderationTab(),
onConsiderationViewClicked(), onConsiderationFontIncrease(), onConsiderationFontDecrease(),
onMultiPVComboBoxChanged(), onEngineComboBoxChanged(), onEngineSettingsClicked(),
onTimeSettingChanged(), onElapsedTimerTick()

【抽出対象のシグナル】
considerationMultiPVChanged, stopConsiderationRequested, startConsiderationRequested,
engineSettingsRequested, considerationTimeSettingsChanged,
considerationEngineChanged, showArrowsChanged, pvRowClicked（検討用のみ）

【設計方針】
- ConsiderationTabManager は QObject を継承
- EngineAnalysisTab は m_considerationTabManager を所有
- EngineAnalysisTab の公開メソッド（setConsiderationMultiPV 等）は委譲で維持し、
  外部コード（ConsiderationModeUIController, DialogCoordinator, ConsiderationWiring）は変更不要
- createConsiderationPage() は EngineAnalysisTab に残すが、
  内部で ConsiderationTabManager::buildConsiderationUi(parentWidget) を呼ぶ形にする
- connect() にラムダを使わないこと
- pvRowClicked シグナルの engineIndex==2 の場合のハンドリングも移動する

実装後、ビルドが通ることを確認してください。
```

</details>

---

### Step 1-3: CommentEditorPanel の抽出

**対象メンバ変数:**
- `m_comment`, `m_commentViewport`, `m_commentToolbar`
- `m_btnFontIncrease`, `m_btnFontDecrease`
- `m_btnCommentUndo`, `m_btnCommentRedo`, `m_btnCommentCut`, `m_btnCommentCopy`, `m_btnCommentPaste`
- `m_btnUpdateComment`, `m_editingLabel`
- `m_currentFontSize`, `m_currentMoveIndex`, `m_originalComment`, `m_isCommentDirty`

**対象メソッド:**
- `createCommentPage()` のUI構築部分
- `setCommentText()`, `setCommentHtml()`, `setCurrentMoveIndex()`, `currentMoveIndex()`
- `hasUnsavedComment()`, `confirmDiscardUnsavedComment()`, `clearCommentDirty()`
- `onFontIncrease()`, `onFontDecrease()`, `onUpdateCommentClicked()`
- `onCommentTextChanged()`, `onCommentUndo/Redo/Cut/Copy/Paste()`
- `updateEditingIndicator()`, `convertUrlsToLinks()`
- `eventFilter()` のうち URL クリック処理

**対象シグナル:**
- `commentUpdated(int moveIndex, const QString& newComment)`
- `requestApplyStart()`, `requestApplyMainAtPly(int ply)`

**推定削減:** ~300行

**注意点:**
- `CommentCoordinator` が `setCommentHtml()`, `setCurrentMoveIndex()` を使用
- `requestApplyStart()` / `requestApplyMainAtPly()` はコメント内の `shogimove://` リンククリックで発火

<details>
<summary>AIプロンプト</summary>

```
src/widgets/engineanalysistab.h/.cpp からコメント編集の責務を
新クラス CommentEditorPanel（src/widgets/commenteditorpanel.h/.cpp）に抽出してください。

【抽出対象のメンバ変数】
m_comment, m_commentViewport, m_commentToolbar,
m_btnFontIncrease, m_btnFontDecrease,
m_btnCommentUndo, m_btnCommentRedo, m_btnCommentCut, m_btnCommentCopy, m_btnCommentPaste,
m_btnUpdateComment, m_editingLabel,
m_currentFontSize, m_currentMoveIndex, m_originalComment, m_isCommentDirty

【抽出対象のメソッド】
setCommentText(), setCommentHtml(), setCurrentMoveIndex(), currentMoveIndex(),
hasUnsavedComment(), confirmDiscardUnsavedComment(), clearCommentDirty(),
onFontIncrease(), onFontDecrease(), onUpdateCommentClicked(),
onCommentTextChanged(), onCommentUndo(), onCommentRedo(),
onCommentCut(), onCommentCopy(), onCommentPaste(),
updateEditingIndicator(), convertUrlsToLinks(),
eventFilter() の URL クリック処理部分,
buildCommentToolbar() に相当するUI構築ロジック

【抽出対象のシグナル】
commentUpdated(int moveIndex, const QString& newComment),
requestApplyStart(), requestApplyMainAtPly(int ply)

【設計方針】
- CommentEditorPanel は QObject を継承
- EngineAnalysisTab は m_commentEditor を所有し、公開メソッドは委譲
- createCommentPage() は EngineAnalysisTab に残すが、
  内部で CommentEditorPanel::buildCommentUi(parent) を呼ぶ
- convertUrlsToLinks() は static ヘルパーとして CommentEditorPanel に移動
- CommentCoordinator からの呼び出しは EngineAnalysisTab 経由の委譲で変更不要
- connect() にラムダを使わないこと

実装後、ビルドが通ることを確認してください。
```

</details>

---

### Step 1-4: LogViewFontManager による重複排除

**問題:** 4つのタブ（Thinking, Consideration, USI Log, CSA Log）で同一パターンのフォント管理コードが重複:
- `onXxxFontIncrease()` / `onXxxFontDecrease()` が8メソッド
- `updateXxxFontSize(int delta)` が4メソッド
- いずれも同一ロジック: `fontSize = qBound(8, fontSize + delta, 24); widget->setFont(...)`

**推定削減:** ~120行（8メソッド → 共通化）

<details>
<summary>AIプロンプト</summary>

```
src/widgets/engineanalysistab.cpp にあるフォントサイズ管理の重複コードを
共通ヘルパークラス LogViewFontManager（src/widgets/logviewfontmanager.h/.cpp）で解消してください。

【現状の重複パターン】
以下の4系統で同一ロジックが繰り返されている:
1. Thinking: m_thinkingFontSize, onThinkingFontIncrease/Decrease
2. Consideration: m_considerationFontSize, onConsiderationFontIncrease/Decrease
3. USI Log: m_usiLogFontSize, onUsiLogFontIncrease/Decrease
4. CSA Log: m_csaLogFontSize, onCsaLogFontIncrease/Decrease

各メソッドのロジック: fontSize = qBound(8, fontSize + delta, 24); widget->setFont(...)

【設計方針】
- LogViewFontManager はシンプルなヘルパークラス（QObject不要）
- メンバ: int& m_fontSize (参照), QWidget* m_targetWidget
- メソッド: void increase(), void decrease(), void apply()
- EngineAnalysisTab は各タブ用の LogViewFontManager インスタンスを保持
- EngineAnalysisTab の onXxxFontIncrease/Decrease スロットは維持するが、
  実装を m_xxxFontManager.increase() への1行委譲にする
- Note: Step 1-2 で ConsiderationTabManager に移動済みの場合は、
  そちらにも同様に適用する
- connect() にラムダを使わないこと

実装後、ビルドが通ることを確認してください。
```

</details>

---

## Phase 2: ShogiView の責務分離（優先度: 高）

**現状**: 3,714行 / 128メソッド / 48+メンバ変数。描画・レイアウト・操作・ハイライトが混在。

### Step 2-1: ShogiViewLayout の抽出

**対象メンバ変数:**
- `m_squareSize`, `m_fieldSize`, `kSquareAspectRatio`
- `m_boardMarginPx`, `m_param1`, `m_param2`, `m_offsetX`, `m_offsetY`
- `m_labelGapPx`, `m_labelBandPx`, `m_labelFontPt`
- `m_nameFontScale`, `m_rankFontScale`, `m_standGapCols`, `m_standGapPx`
- `m_flipMode`

**対象メソッド:**
- `recalcLayoutParams()`, `fitBoardToWidget()`
- `calculateSquareRectangleBasedOnBoardState()`, `calculateRectangleForRankOrFileLabel()`
- `blackStandBoundingRect()`, `whiteStandBoundingRect()`
- `boardLeftPx()`, `boardRightPx()`, `standInnerEdgePx()`, `minGapForRankLabelsPx()`
- `setStandGapCols()`, `setNameFontScale()`, `setRankFontScale()`

**推定削減:** ~500行

<details>
<summary>AIプロンプト</summary>

```
src/views/shogiview.h/.cpp からレイアウト計算の責務を
新クラス ShogiViewLayout（src/views/shogiviewlayout.h/.cpp）に抽出してください。

【抽出対象のメンバ変数】
m_squareSize, m_fieldSize, kSquareAspectRatio,
m_boardMarginPx, m_param1, m_param2, m_offsetX, m_offsetY,
m_labelGapPx, m_labelBandPx, m_labelFontPt,
m_nameFontScale, m_rankFontScale, m_standGapCols, m_standGapPx,
m_flipMode

【抽出対象のメソッド】
recalcLayoutParams(), fitBoardToWidget(),
calculateSquareRectangleBasedOnBoardState(),
calculateRectangleForRankOrFileLabel(),
blackStandBoundingRect(), whiteStandBoundingRect(),
boardLeftPx(), boardRightPx(), standInnerEdgePx(), minGapForRankLabelsPx(),
setStandGapCols(), setNameFontScale(), setRankFontScale()

【設計方針】
- ShogiViewLayout は非QObjectの純粋なクラス（シグナル不要）
- ShogiView は m_layout を値メンバとして保持（std::unique_ptr でも可）
- ShogiView の描画メソッドは m_layout 経由で座標を取得
- ShogiView の公開メソッド（setStandGapCols 等）は委譲
- flipMode の get/set も ShogiViewLayout に移動
- relayoutTurnLabels() は ShogiView に残す（QLabel 操作のため）が、
  座標計算部分は ShogiViewLayout のメソッドを利用する

実装後、ビルドが通ることを確認してください。
```

</details>

---

### Step 2-2: ShogiViewHighlighting の抽出

**対象メンバ変数:**
- `m_highlights`, `m_arrows`
- `m_highlightBg`, `m_highlightFgOn`, `m_highlightFgOff`
- 全 urgency 系カラー変数（`kTurnBg`, `kWarn*Bg/Fg/Border`, `m_urgencyBg/Fg*`）
- `m_blackActive`, `m_urgency`

**対象メソッド:**
- `addHighlight()`, `removeHighlight()`, `removeHighlightAllData()`, `highlight()`, `highlightCount()`
- `setHighlightStyle()`, `clearTurnHighlight()`, `applyTurnHighlight()`
- `setArrows()`, `clearArrows()`
- `setActiveSide()`, `setUrgencyVisuals()`, `setActiveIsBlack()`
- `drawHighlights()`, `drawArrows()`（描画処理）

**推定削減:** ~400行

<details>
<summary>AIプロンプト</summary>

```
src/views/shogiview.h/.cpp からハイライト・矢印・ターン表示の責務を
新クラス ShogiViewHighlighting（src/views/shogiviewhighlighting.h/.cpp）に抽出してください。

【抽出対象のメンバ変数】
m_highlights, m_arrows,
m_highlightBg, m_highlightFgOn, m_highlightFgOff,
全 urgency カラー変数（kTurnBg, kTurnFg, kWarn10Bg, kWarn10Fg, kWarn5Bg, kWarn5Fg,
kWarn10Border, kWarn5Border, m_urgencyBgWarn10, m_urgencyFgWarn10, m_urgencyBgWarn5, m_urgencyFgWarn5）,
m_blackActive, m_urgency (Urgency enum含む)

【抽出対象のメソッド】
addHighlight(), removeHighlight(), removeHighlightAllData(), highlight(), highlightCount(),
setHighlightStyle(), clearTurnHighlight(), applyTurnHighlight(),
setArrows(), clearArrows(),
setActiveSide(), setUrgencyVisuals(), setActiveIsBlack(),
drawHighlights(), drawArrows()

【対象シグナル】
highlightsCleared()

【設計方針】
- ShogiViewHighlighting は QObject を継承（highlightsCleared シグナルのため）
- ShogiView は m_highlighting を所有
- drawHighlights() / drawArrows() は QPainter& と ShogiViewLayout& を引数に取る
- ShogiView::paintEvent() 内で m_highlighting->drawHighlights(painter, m_layout) を呼ぶ
- ShogiView の公開メソッド（addHighlight 等）は委譲
- BoardInteractionController が接続する highlightsCleared は
  ShogiViewHighlighting のシグナルを ShogiView が再emit するか、
  直接 ShogiViewHighlighting のシグナルに接続するか選択
  （後者は highlighting() アクセサを公開する必要あり）
- connect() にラムダを使わないこと

実装後、ビルドが通ることを確認してください。
```

</details>

---

### Step 2-3: ShogiViewInteraction の抽出

**対象メンバ変数:**
- `m_mouseClickMode`, `m_positionEditMode`
- `m_dragging`, `m_dragFrom`, `m_dragPiece`, `m_dragPos`, `m_dragFromStand`
- `m_tempPieceStandCounts`

**対象メソッド:**
- `getClickedSquare()`, `getClickedSquareInDefaultState()`, `getClickedSquareInFlippedState()`
- `startDrag()`, `endDrag()`, `drawDraggingPiece()`
- `setMouseClickMode()`, `setPositionEditMode()`

**推定削減:** ~300行

<details>
<summary>AIプロンプト</summary>

```
src/views/shogiview.h/.cpp からマウス操作・ドラッグの責務を
新クラス ShogiViewInteraction（src/views/shogiviewinteraction.h/.cpp）に抽出してください。

【抽出対象のメンバ変数】
m_mouseClickMode, m_positionEditMode,
m_dragging, m_dragFrom, m_dragPiece, m_dragPos, m_dragFromStand,
m_tempPieceStandCounts

【抽出対象のメソッド】
getClickedSquare(), getClickedSquareInDefaultState(), getClickedSquareInFlippedState(),
startDrag(), endDrag(), drawDraggingPiece(),
setMouseClickMode(), setPositionEditMode()

【設計方針】
- ShogiViewInteraction は非QObjectクラス
- ShogiView は m_interaction を値メンバとして保持
- getClickedSquare() は ShogiViewLayout& を引数に取る（座標変換に必要）
- drawDraggingPiece() は QPainter&, ShogiBoard*, ShogiViewLayout& を引数に取る
- mouseMoveEvent / mouseReleaseEvent は ShogiView に残すが、
  座標変換は m_interaction 経由で行う
- startDrag() は ShogiBoard* を引数で受け取り、
  ドラッグ中の駒情報を取得する

実装後、ビルドが通ることを確認してください。
```

</details>

---

## Phase 3: MatchCoordinator の Strategy 化（優先度: 高）

**現状**: 3,526行 / 106メソッド / 50メンバ変数。HvE, EvE, HvH の全モード処理が1クラスに集約。

### Step 3-1: GameModeStrategy インターフェースと HumanVsHuman の抽出

**対象メソッド:**
- `startHumanVsHuman()` (~18行)
- `onHumanMove_HvH()` とその関連メソッド
- `armTurnTimerIfNeeded()`, `finishTurnTimerAndSetConsiderationFor()`
- HvH 固有のメンバ: `m_turnTimer`, `m_turnTimerArmed`

<details>
<summary>AIプロンプト</summary>

```
src/game/matchcoordinator.h/.cpp のゲームモード処理を Strategy パターンで
リファクタリングする第一段階として、以下を実施してください。

【Step A: GameModeStrategy インターフェース作成】
新ファイル: src/game/gamemodestrategy.h

class GameModeStrategy {
public:
    virtual ~GameModeStrategy() = default;
    virtual void start(const StartOptions& opt) = 0;
    virtual void onHumanMove(const QPoint& from, const QPoint& to,
                             const QString& prettyMove) = 0;
    virtual bool needsEngine() const = 0;
};

【Step B: HumanVsHumanStrategy の抽出】
新ファイル: src/game/humanvshumanstrategy.h/.cpp

MatchCoordinator から以下を移動:
- startHumanVsHuman() → HumanVsHumanStrategy::start()
- onHumanMove_HvH() → HumanVsHumanStrategy::onHumanMove()
- armTurnTimerIfNeeded(), finishTurnTimerAndSetConsiderationFor()
- m_turnTimer, m_turnTimerArmed

【設計方針】
- HumanVsHumanStrategy は MatchCoordinator への参照（またはインターフェース）を持ち、
  共通処理（setGameOver 等）は MatchCoordinator のメソッドを呼ぶ
- MatchCoordinator は m_strategy (std::unique_ptr<GameModeStrategy>) を保持
- configureAndStart() の PlayMode::HumanVsHuman 分岐で
  m_strategy = std::make_unique<HumanVsHumanStrategy>(...) を生成
- MatchCoordinator::onHumanMove() は m_strategy->onHumanMove() へ委譲
- connect() にラムダを使わないこと
- 他のモード（HvE, EvE）は次ステップで対応するため、今回は既存のまま残す

実装後、ビルドが通ることを確認してください。
```

</details>

---

### Step 3-2: HumanVsEngineStrategy の抽出

**対象メソッド:**
- `startHumanVsEngine()` (~63行)
- `onHumanMove_HvE()` (2つのオーバーロード)
- `armHumanTimerIfNeeded()`, `finishHumanTimerAndSetConsideration()`, `disarmHumanTimerIfNeeded()`
- `startInitialEngineMoveIfNeeded()`, `startInitialEngineMoveFor()`
- HvE 固有のメンバ: `m_humanTurnTimer`, `m_humanTimerArmed`

<details>
<summary>AIプロンプト</summary>

```
Step 3-1 で導入した GameModeStrategy に続き、HumanVsEngineStrategy を
src/game/humanvsenginestrategy.h/.cpp に抽出してください。

【抽出対象のメソッド】
startHumanVsEngine() → start(),
onHumanMove_HvE(from, to) と onHumanMove_HvE(from, to, prettyMove) → onHumanMove(),
armHumanTimerIfNeeded(), finishHumanTimerAndSetConsideration(), disarmHumanTimerIfNeeded(),
startInitialEngineMoveIfNeeded(), startInitialEngineMoveFor()

【抽出対象のメンバ変数】
m_humanTurnTimer, m_humanTimerArmed

【設計方針】
- HumanVsEngineStrategy は GameModeStrategy を継承
- コンストラクタで MatchCoordinator& を受け取り、共通処理を委譲
- engineIsP1 フラグをメンバとして保持（start() で設定）
- MatchCoordinator の configureAndStart() で
  HvE 系 PlayMode の場合に m_strategy を差し替え
- engineThinkApplyMove() は MatchCoordinator に残す（共通処理）
- connect() にラムダを使わないこと

実装後、ビルドが通ることを確認してください。
```

</details>

---

### Step 3-3: EngineVsEngineStrategy の抽出

**対象メソッド:**
- `startEngineVsEngine()`, `initEnginesForEvE()`
- `startEvEFirstMoveByBlack()`, `startEvEFirstMoveByWhite()`, `kickNextEvETurn()`
- EvE 固有のメンバ: `m_eveSfenRecord`, `m_eveGameMoves`, `m_eveMoveIndex`

<details>
<summary>AIプロンプト</summary>

```
Step 3-1, 3-2 に続き、EngineVsEngineStrategy を
src/game/enginevsenginestrategy.h/.cpp に抽出してください。

【抽出対象のメソッド】
startEngineVsEngine() → start(),
initEnginesForEvE(),
startEvEFirstMoveByBlack(), startEvEFirstMoveByWhite(),
kickNextEvETurn()

【抽出対象のメンバ変数】
m_eveSfenRecord, m_eveGameMoves, m_eveMoveIndex

【設計方針】
- EngineVsEngineStrategy は GameModeStrategy を継承
- onHumanMove() は空実装（EvE では人間の手はない）
- needsEngine() は true を返す
- kickNextEvETurn() は QTimer::singleShot で自身を再呼び出しするため、
  QObject 継承が必要 → GameModeStrategy を QObject にするか、
  EngineVsEngineStrategy のみ QObject を継承する
- MatchCoordinator の configureAndStart() で
  EvE 系 PlayMode の場合に m_strategy を差し替え
- connect() にラムダを使わないこと

これで configureAndStart() の mode-specific 分岐はすべて Strategy に委譲される。
残る共通処理（setGameOver, handleEngineResign 等）は MatchCoordinator に維持。

実装後、ビルドが通ることを確認してください。
```

</details>

---

## Phase 4: GameRecordModel のエクスポート分離（優先度: 中）

**現状**: 2,847行。データモデルにCSA/JKF等のエクスポートロジックが混在。

### Step 4-1: CsaExporter / JkfExporter の抽出

**対象メソッド:**
- `toCsaLines()` (336行) → `CsaExporter::exportLines()`
- `convertMoveToJkf()` (182行) → `JkfExporter::convertMove()`
- 関連ヘルパー群

<details>
<summary>AIプロンプト</summary>

```
src/kifu/gamerecordmodel.cpp の棋譜エクスポートロジックを分離してください。

【Step A: CsaExporter の抽出】
新ファイル: src/kifu/formats/csaexporter.h/.cpp
- toCsaLines() (約336行) を GameRecordModel から移動
- static メソッドとし、必要なデータは const GameRecordModel& で受け取る
- GameRecordModel::toCsaLines() は CsaExporter::exportLines(*this) への委譲に変更

【Step B: JkfExporter の抽出】
新ファイル: src/kifu/formats/jkfexporter.h/.cpp
- convertMoveToJkf() (約182行) を GameRecordModel から移動
- JKF 関連のヘルパーメソッドも一緒に移動
- GameRecordModel 側は委譲メソッドを維持

【設計方針】
- 既存の呼び出し元（KifuExportController 等）のインターフェースは変更しない
- GameRecordModel のデータアクセスに必要な public メソッドが足りなければ追加する
- formats/ ディレクトリに配置（既存の *ToSfenConverter と同階層）

実装後、ビルドが通ることを確認してください。
```

</details>

---

## Phase 5: KIF/KI2 パーサーのネスト平坦化（優先度: 中）

**現状**: 各1,600-1,700行。最大7段のネスト、250-300行のメソッドが複数。

### Step 5-1: KifToSfenConverter::buildInitialSfenFromBod の分割

**対象:** 296行のメソッドにラムダで定義された6つのヘルパー関数

<details>
<summary>AIプロンプト</summary>

```
src/kifu/formats/kiftosfenconverter.cpp の buildInitialSfenFromBod() メソッド（約296行）を
リファクタリングして可読性を改善してください。

【現状の問題】
- 6つのラムダ関数がメソッド内にネストされている:
  isSpaceLike, parseBodRowCommon, rowTokensToSfen,
  kanjiDigit, parseKanjiNumberString, parseCountSuffixFlexible, parseHandsLine, zenk2ascii
- parseBodRowCommon 内で6-7段のネスト

【修正方針】
1. ラムダ群を KifToSfenConverter の private static メソッドまたは
   無名名前空間の自由関数に昇格
2. parseBodRowCommon を分割:
   - extractFramedContent() : フレーム行から内容文字列を抽出
   - tokenizeRowContent() : 内容文字列をトークン列に変換
3. Early return パターンでネストを平坦化
4. buildInitialSfenFromBod() 本体は各ヘルパーの呼び出しに簡素化

【注意】
- 既存のパース結果が変わらないよう注意
- 全角・半角文字、Unicode BOX DRAWING 文字の処理を壊さない
- KI2 パーサーの buildInitialSfenFromBod() も同一ロジックを使っている場合は
  共通ユーティリティとして抽出可能か確認する

実装後、ビルドが通ることを確認してください。
```

</details>

---

### Step 5-2: Ki2ToSfenConverter::inferSourceSquare の平坦化

**対象:** 179行のメソッド、候補収集→多段フィルタリング

<details>
<summary>AIプロンプト</summary>

```
src/kifu/formats/ki2tosfenconverter.cpp の inferSourceSquare() メソッド（約179行）を
リファクタリングして可読性を改善してください。

【現状の問題】
- 9x9 盤面スキャンによる候補収集で4段ネスト
- 右/左/上/引/寄/直 の6種フィルターが順次適用され5-6段ネスト
- 各フィルターのロジックが似ているが微妙に異なる

【修正方針】
1. 候補収集を独立メソッドに:
   collectCandidates(toFile, toRank, pieceType, isBlack, isPromoted) → QVector<Candidate>
2. 各方向フィルターを独立メソッドに:
   filterByDirection(candidates, modifier, isBlack) → QVector<Candidate>
   内部で右/左/上/引/寄/直 を分岐
3. inferSourceSquare() 本体は:
   candidates = collectCandidates(...)
   if (candidates.size() == 1) return candidates[0]
   candidates = filterByDirection(candidates, modifier, isBlack)
   if (candidates.size() == 1) return candidates[0]
   // error handling
4. Early return パターンでネストを平坦化

実装後、ビルドが通ることを確認してください。
```

</details>

---

### Step 5-3: parseWithVariations の分割

**対象:** KifToSfenConverter::parseWithVariations() 270行

<details>
<summary>AIプロンプト</summary>

```
src/kifu/formats/kiftosfenconverter.cpp の parseWithVariations() メソッド（約270行）を
リファクタリングして可読性を改善してください。

【現状の問題】
- メインライン抽出、変化ブロック解析、分岐ポイント探索が1メソッドに混在
- 変化ブロック内の指し手処理で6段ネスト

【修正方針】
1. メソッドを3つのフェーズに分割:
   - extractMainLine(lines) → KifVariation (メインライン)
   - findBranchPoint(variations, mainLine, branchPly) → 分岐元の特定
   - parseVariationBlock(blockLines, branchPointMoves) → KifVariation
2. parseWithVariations() 本体はフェーズ呼び出しのみ
3. 変化ブロック内の指し手処理は extractMovesFromBlock() に切り出し

実装後、ビルドが通ることを確認してください。
```

</details>

---

## Phase 6: EngineAnalysisTab トランプデータの解消（優先度: 中-低）

**現状**: 11以上のクラスに `setAnalysisTab(EngineAnalysisTab*)` が存在。

### Step 6-1: 用途別インターフェースの導入

**注意:** Phase 1 の分割が完了してから実施すること。
Phase 1 で BranchTreeManager, ConsiderationTabManager, CommentEditorPanel が分離されていれば、
各消費者クラスは具体的なサブマネージャーへの参照を受け取る形に移行可能。

<details>
<summary>AIプロンプト</summary>

```
Phase 1 で EngineAnalysisTab から分離された各マネージャークラスを活用して、
setAnalysisTab(EngineAnalysisTab*) のトランプデータパターンを解消してください。

【現在のsetAnalysisTab利用状況と移行先】

1. KifuLoadCoordinator: setBranchTreeRows(), highlightBranchTreeAt() のみ使用
   → setAnalysisTab() を廃止し、setBranchTreeManager(BranchTreeManager*) に変更

2. KifuDisplayCoordinator: highlightBranchTreeAt() のみ使用
   → 同上

3. AnalysisCoordinator: highlightBranchTreeAt() のみ使用
   → 同上

4. CommentCoordinator: setCommentHtml(), setCurrentMoveIndex() のみ使用
   → setAnalysisTab() を廃止し、setCommentEditor(CommentEditorPanel*) に変更

5. ConsiderationModeUIController: 検討タブ操作のみ
   → setAnalysisTab() を廃止し、
     setConsiderationTabManager(ConsiderationTabManager*) に変更

6. DialogCoordinator: 検討タブ状態クエリのみ
   → setConsiderationTabManager(ConsiderationTabManager*) に変更

7. PlayerInfoWiring: エンジン名設定のみ
   → setAnalysisTab() は残すが、将来的にエンジン名モデルに移行

8. UsiCommandController: appendUsiLogStatus() のみ
   → setAnalysisTab() は残す（1メソッドのため抽出の利益が薄い）

9. DockCreationService: ページ作成メソッド群を使用
   → setAnalysisTab() は残す（UIファクトリー用途のため）

【設計方針】
- 各クラスの Deps 構造体を更新し、AnalysisTab* → 具体マネージャー* に変更
- MainWindow の ensure*() / updateDeps() を修正して新しい依存を注入
- connect() にラムダを使わないこと

実装後、ビルドが通ることを確認してください。
```

</details>

---

## Phase 7: MainWindow の継続的削減（優先度: 低）

**現状**: 4,281行 / 184メソッド / 111メンバ変数（リファクタリング済みだが依然として最大）

### Step 7-1: initMatchCoordinator の分離

**対象:** `initMatchCoordinator()` 186行 — MatchCoordinator の初期化とシグナル接続

<details>
<summary>AIプロンプト</summary>

```
src/app/mainwindow.cpp の initMatchCoordinator() メソッド（約186行）を
既存の wiring クラスに移動してください。

【現状】
initMatchCoordinator() は MatchCoordinator の生成と、
MatchCoordinator のシグナルを各コントローラーのスロットに接続する処理。

【修正方針】
- 新ファイル src/ui/wiring/matchcoordinatorwiring.h/.cpp を作成
- MatchCoordinatorWiring クラスに Deps 構造体パターンを適用
- initMatchCoordinator() の connect() 群を
  MatchCoordinatorWiring::wireConnections() に移動
- MainWindow::initMatchCoordinator() は
  ensureMatchCoordinatorWiring() + m_matchWiring->wireConnections() の委譲に変更
- connect() にラムダを使わないこと

実装後、ビルドが通ることを確認してください。
```

</details>

---

## 実施順序の推奨

```
Phase 1 (EngineAnalysisTab) ← 独立性が高く、他に影響しない
  Step 1-1 → 1-2 → 1-3 → 1-4（順序依存あり）

Phase 2 (ShogiView) ← Phase 1 と独立して並行可能
  Step 2-1 → 2-2 → 2-3（順序依存あり）

Phase 3 (MatchCoordinator) ← Phase 1, 2 と独立
  Step 3-1 → 3-2 → 3-3（順序依存あり）

Phase 4 (GameRecordModel) ← 任意のタイミングで実施可能
  Step 4-1

Phase 5 (KIF/KI2 パーサー) ← 任意のタイミングで実施可能
  Step 5-1, 5-2, 5-3 は独立して実施可能

Phase 6 (トランプデータ) ← Phase 1 完了後に実施
  Step 6-1

Phase 7 (MainWindow) ← 他の Phase と独立
  Step 7-1
```

## 各 Phase の推定効果

| Phase | 対象クラス | 現行行数 | 推定削減行数 | 削減率 |
|-------|-----------|---------|------------|--------|
| 1 | EngineAnalysisTab | 3,085 | ~1,320 | ~43% |
| 2 | ShogiView | 3,714 | ~1,200 | ~32% |
| 3 | MatchCoordinator | 3,526 | ~700 | ~20% |
| 4 | GameRecordModel | 2,847 | ~520 | ~18% |
| 5 | KIF/KI2 parsers | 3,363 | ~200 (ネスト改善) | ~6% |
| 6 | EngineAnalysisTab coupling | - | (結合度改善) | - |
| 7 | MainWindow | 4,281 | ~200 | ~5% |
| **合計** | | | **~4,140** | |
