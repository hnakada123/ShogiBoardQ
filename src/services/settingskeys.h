#ifndef SETTINGSKEYS_H
#define SETTINGSKEYS_H

/// @file settingskeys.h
/// @brief 設定キー文字列の一元管理
///
/// settingsservice.cpp で使用する全ての QSettings キーを
/// inline constexpr 定数として定義します。

namespace SettingsKeys {

// --- SizeRelated ---
inline constexpr char kMainWindowSize[]                  = "SizeRelated/mainWindowSize";
inline constexpr char kSquareSize[]                      = "SizeRelated/squareSize";
inline constexpr char kPvBoardDialogSize[]               = "SizeRelated/pvBoardDialogSize";
inline constexpr char kSfenCollectionDialogSize[]        = "SizeRelated/sfenCollectionDialogSize";
inline constexpr char kStartGameDialogSize[]             = "SizeRelated/startGameDialogSize";
inline constexpr char kKifuPasteDialogSize[]             = "SizeRelated/kifuPasteDialogSize";
inline constexpr char kCsaLogWindowSize[]                = "SizeRelated/csaLogWindowSize";
inline constexpr char kCsaGameDialogSize[]               = "SizeRelated/csaGameDialogSize";
inline constexpr char kKifuAnalysisDialogSize[]          = "SizeRelated/kifuAnalysisDialogSize";
inline constexpr char kJosekiMoveDialogSize[]            = "SizeRelated/josekiMoveDialogSize";
inline constexpr char kTsumeshogiGeneratorDialogSize[]   = "SizeRelated/tsumeshogiGeneratorDialogSize";

// --- General ---
inline constexpr char kLastKifuDirectory[]               = "General/lastKifuDirectory";
inline constexpr char kLastKifuSaveDirectory[]           = "General/lastKifuSaveDirectory";
inline constexpr char kLanguage[]                        = "General/language";
inline constexpr char kSettingsVersion[]                 = "General/settingsVersion";

// --- FontSize ---
inline constexpr char kFontSizeUsiLog[]                  = "FontSize/usiLog";
inline constexpr char kFontSizeComment[]                 = "FontSize/comment";
inline constexpr char kFontSizeGameInfo[]                = "FontSize/gameInfo";
inline constexpr char kFontSizeThinking[]                = "FontSize/thinking";
inline constexpr char kFontSizeKifuPane[]                = "FontSize/kifuPane";
inline constexpr char kFontSizeCsaLog[]                  = "FontSize/csaLog";
inline constexpr char kFontSizeCsaWaitingDialog[]        = "FontSize/csaWaitingDialog";
inline constexpr char kFontSizeCsaGameDialog[]           = "FontSize/csaGameDialog";
inline constexpr char kFontSizeJishogiScore[]            = "FontSize/jishogiScore";
inline constexpr char kFontSizeEngineSettings[]          = "FontSize/engineSettings";
inline constexpr char kFontSizeEngineRegistration[]      = "FontSize/engineRegistration";
inline constexpr char kFontSizeConsiderationDialog[]     = "FontSize/considerationDialog";
inline constexpr char kFontSizeStartGameDialog[]         = "FontSize/startGameDialog";
inline constexpr char kFontSizeKifuPasteDialog[]         = "FontSize/kifuPasteDialog";
inline constexpr char kFontSizeTsumeshogiGenerator[]     = "FontSize/tsumeshogiGenerator";

// --- UI ---
inline constexpr char kLastSelectedTabIndex[]            = "UI/lastSelectedTabIndex";
inline constexpr char kToolbarVisible[]                  = "UI/toolbarVisible";

// --- SfenCollection ---
inline constexpr char kSfenCollectionRecentFiles[]       = "SfenCollection/recentFiles";
inline constexpr char kSfenCollectionSquareSize[]        = "SfenCollection/squareSize";
inline constexpr char kSfenCollectionLastDirectory[]     = "SfenCollection/lastDirectory";

// --- EvalChart ---
inline constexpr char kEvalChartYLimit[]                 = "EvalChart/yLimit";
inline constexpr char kEvalChartXLimit[]                 = "EvalChart/xLimit";
inline constexpr char kEvalChartXInterval[]              = "EvalChart/xInterval";
inline constexpr char kEvalChartLabelFontSize[]          = "EvalChart/labelFontSize";

// --- EngineInfo (動的キー: QString(...).arg(idx) で使用) ---
inline constexpr char kEngineInfoColumnWidthsFmt[]       = "EngineInfo/columnWidths%1";

// --- ThinkingView (動的キー) ---
inline constexpr char kThinkingViewColumnWidthsFmt[]     = "ThinkingView/columnWidths%1";

// --- 配列内サブキー ---
inline constexpr char kArrayWidth[]                      = "width";

// --- KifuAnalysis ---
inline constexpr char kKifuAnalysisFontSize[]            = "KifuAnalysis/fontSize";
inline constexpr char kKifuAnalysisResultsWindowSize[]   = "KifuAnalysis/resultsWindowSize";
inline constexpr char kKifuAnalysisByoyomiSec[]          = "KifuAnalysis/byoyomiSec";
inline constexpr char kKifuAnalysisEngineIndex[]         = "KifuAnalysis/engineIndex";
inline constexpr char kKifuAnalysisFullRange[]           = "KifuAnalysis/fullRange";
inline constexpr char kKifuAnalysisStartPly[]            = "KifuAnalysis/startPly";
inline constexpr char kKifuAnalysisEndPly[]              = "KifuAnalysis/endPly";

// --- JosekiWindow ---
inline constexpr char kJosekiWindowFontSize[]            = "JosekiWindow/fontSize";
inline constexpr char kJosekiWindowSfenFontSize[]        = "JosekiWindow/sfenFontSize";
inline constexpr char kJosekiWindowLastFilePath[]        = "JosekiWindow/lastFilePath";
inline constexpr char kJosekiWindowSize[]                = "JosekiWindow/size";
inline constexpr char kJosekiWindowAutoLoadEnabled[]     = "JosekiWindow/autoLoadEnabled";
inline constexpr char kJosekiWindowDisplayEnabled[]      = "JosekiWindow/displayEnabled";
inline constexpr char kJosekiWindowSfenDetailVisible[]   = "JosekiWindow/sfenDetailVisible";
inline constexpr char kJosekiWindowRecentFiles[]         = "JosekiWindow/recentFiles";
inline constexpr char kJosekiWindowMoveDialogFontSize[]  = "JosekiWindow/moveDialogFontSize";
inline constexpr char kJosekiWindowColumnWidths[]        = "JosekiWindow/columnWidths";
inline constexpr char kJosekiWindowMergeDialogFontSize[] = "JosekiWindow/mergeDialogFontSize";
inline constexpr char kJosekiWindowMergeDialogSize[]     = "JosekiWindow/mergeDialogSize";

// --- JishogiScore ---
inline constexpr char kJishogiScoreDialogSize[]          = "JishogiScore/dialogSize";

// --- MenuWindow ---
inline constexpr char kMenuWindowFavorites[]             = "MenuWindow/favorites";
inline constexpr char kMenuWindowSize[]                  = "MenuWindow/size";
inline constexpr char kMenuWindowButtonSize[]            = "MenuWindow/buttonSize";
inline constexpr char kMenuWindowFontSize[]              = "MenuWindow/fontSize";

// --- EngineSettings ---
inline constexpr char kEngineSettingsDialogSize[]        = "EngineSettings/dialogSize";

// --- EngineRegistration ---
inline constexpr char kEngineRegistrationDialogSize[]    = "EngineRegistration/dialogSize";

// --- Consideration ---
inline constexpr char kConsiderationEngineIndex[]        = "Consideration/engineIndex";
inline constexpr char kConsiderationUnlimitedTime[]      = "Consideration/unlimitedTime";
inline constexpr char kConsiderationByoyomiSec[]         = "Consideration/byoyomiSec";
inline constexpr char kConsiderationMultiPV[]            = "Consideration/multiPV";

// --- ConsiderationTab ---
inline constexpr char kConsiderationTabFontSize[]        = "ConsiderationTab/fontSize";

// --- EvalChartDock ---
inline constexpr char kEvalChartDockState[]              = "EvalChartDock/state";
inline constexpr char kEvalChartDockFloating[]           = "EvalChartDock/floating";
inline constexpr char kEvalChartDockGeometry[]           = "EvalChartDock/geometry";
inline constexpr char kEvalChartDockVisible[]            = "EvalChartDock/visible";

// --- RecordPaneDock ---
inline constexpr char kRecordPaneDockFloating[]          = "RecordPaneDock/floating";
inline constexpr char kRecordPaneDockGeometry[]          = "RecordPaneDock/geometry";
inline constexpr char kRecordPaneDockVisible[]           = "RecordPaneDock/visible";

// --- AnalysisTabDock ---
inline constexpr char kAnalysisTabDockFloating[]         = "AnalysisTabDock/floating";
inline constexpr char kAnalysisTabDockGeometry[]         = "AnalysisTabDock/geometry";
inline constexpr char kAnalysisTabDockVisible[]          = "AnalysisTabDock/visible";

// --- BoardDock ---
inline constexpr char kBoardDockFloating[]               = "BoardDock/floating";
inline constexpr char kBoardDockGeometry[]               = "BoardDock/geometry";
inline constexpr char kBoardDockVisible[]                = "BoardDock/visible";

// --- MenuWindowDock ---
inline constexpr char kMenuWindowDockFloating[]          = "MenuWindowDock/floating";
inline constexpr char kMenuWindowDockGeometry[]          = "MenuWindowDock/geometry";
inline constexpr char kMenuWindowDockVisible[]           = "MenuWindowDock/visible";

// --- JosekiWindowDock ---
inline constexpr char kJosekiWindowDockFloating[]        = "JosekiWindowDock/floating";
inline constexpr char kJosekiWindowDockGeometry[]        = "JosekiWindowDock/geometry";
inline constexpr char kJosekiWindowDockVisible[]         = "JosekiWindowDock/visible";

// --- CustomDockLayouts ---
inline constexpr char kCustomDockLayoutsGroup[]          = "CustomDockLayouts";

// --- DockLayout ---
inline constexpr char kStartupDockLayoutName[]           = "DockLayout/startupLayoutName";

// --- KifuAnalysisResultsDock ---
inline constexpr char kKifuAnalysisResultsDockFloating[] = "KifuAnalysisResultsDock/floating";
inline constexpr char kKifuAnalysisResultsDockGeometry[] = "KifuAnalysisResultsDock/geometry";
inline constexpr char kKifuAnalysisResultsDockVisible[]  = "KifuAnalysisResultsDock/visible";

// --- Dock ---
inline constexpr char kDocksLocked[]                     = "Dock/docksLocked";

// --- RecordPane ---
inline constexpr char kRecordPaneTimeColumnVisible[]     = "RecordPane/timeColumnVisible";
inline constexpr char kRecordPaneBookmarkColumnVisible[] = "RecordPane/bookmarkColumnVisible";
inline constexpr char kRecordPaneCommentColumnVisible[]  = "RecordPane/commentColumnVisible";

// --- TsumeshogiGenerator ---
inline constexpr char kTsumeshogiGeneratorLastSaveDirectory[] = "TsumeshogiGenerator/lastSaveDirectory";
inline constexpr char kTsumeshogiGeneratorEngineIndex[]       = "TsumeshogiGenerator/engineIndex";
inline constexpr char kTsumeshogiGeneratorTargetMoves[]       = "TsumeshogiGenerator/targetMoves";
inline constexpr char kTsumeshogiGeneratorMaxAttackPieces[]   = "TsumeshogiGenerator/maxAttackPieces";
inline constexpr char kTsumeshogiGeneratorMaxDefendPieces[]   = "TsumeshogiGenerator/maxDefendPieces";
inline constexpr char kTsumeshogiGeneratorAttackRange[]       = "TsumeshogiGenerator/attackRange";
inline constexpr char kTsumeshogiGeneratorTimeoutSec[]        = "TsumeshogiGenerator/timeoutSec";
inline constexpr char kTsumeshogiGeneratorMaxPositions[]      = "TsumeshogiGenerator/maxPositions";

// --- Settings version ---
inline constexpr int kCurrentSettingsVersion = 1;

} // namespace SettingsKeys

#endif // SETTINGSKEYS_H
