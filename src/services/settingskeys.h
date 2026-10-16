#ifndef SETTINGSKEYS_H
#define SETTINGSKEYS_H

/// @file settingskeys.h
/// @brief 処理が要る設定項目のキー文字列
///
/// 単純な項目のキーは各ドメイン別ヘッダーの Setting<T> の宣言に直接書きます。
/// ここには、範囲の制限・複数キーの構造体・配列・動的キーなど、
/// 関数として読み書きする項目のキーだけを置きます。

namespace SettingsKeys {

// --- UI ---
inline constexpr char kPieceStyle[]                      = "UI/pieceStyle";
inline constexpr char kBoardWoodGrain[]                  = "BoardVisuals/woodGrain";
inline constexpr char kStandWoodGrain[]                  = "BoardVisuals/standWoodGrain";
inline constexpr char kAppearanceSection[]               = "UI/appearanceSection";
inline constexpr char kBoardPieceShadow[]                = "BoardVisuals/pieceShadow";
inline constexpr char kBoardPieceScale[]                 = "BoardVisuals/pieceScale";
inline constexpr char kBoardBackgroundColor[]            = "BoardColors/background";
inline constexpr char kBoardSurfaceColor[]               = "BoardColors/board";
inline constexpr char kBoardStandColor[]                 = "BoardColors/stand";
inline constexpr char kBoardGridColor[]                  = "BoardColors/grid";
inline constexpr char kBoardCardBackgroundColor[] = "BoardColors/cardBackground";
inline constexpr char kBoardCardBorderColor[] = "BoardColors/cardBorder";
inline constexpr char kBoardActiveCardBorderColor[] = "BoardColors/activeCardBorder";
inline constexpr char kBoardTurnBackgroundColor[] = "BoardColors/turnBackground";
inline constexpr char kBoardTurnBorderColor[] = "BoardColors/turnBorder";
inline constexpr char kBoardTurnTextColor[] = "BoardColors/turnText";
inline constexpr char kBoardNameBackgroundColor[] = "BoardColors/nameBackground";
inline constexpr char kBoardNameBorderColor[] = "BoardColors/nameBorder";
inline constexpr char kBoardNameTextColor[] = "BoardColors/nameText";
inline constexpr char kBoardClockBackgroundColor[] = "BoardColors/clockBackground";
inline constexpr char kBoardClockBorderColor[] = "BoardColors/clockBorder";
inline constexpr char kBoardClockTextColor[] = "BoardColors/clockText";
inline constexpr char kBoardClockWarningTextColor[] = "BoardColors/clockWarningText";
inline constexpr char kBoardClockCriticalTextColor[] = "BoardColors/clockCriticalText";

// --- Sound ---

// --- EngineInfo (動的キー: QString(...).arg(idx) で使用) ---
inline constexpr char kEngineInfoColumnWidthsFmt[]       = "EngineInfo/columnWidths%1";

// --- ThinkingView (動的キー) ---
inline constexpr char kThinkingViewColumnWidthsFmt[]     = "ThinkingView/columnWidths%1";

// --- 配列内サブキー ---
inline constexpr char kArrayWidth[]                      = "width";

// --- KifuAnalysis ---
inline constexpr char kKifuAnalysisColumnWidths[]        = "KifuAnalysis/columnWidths";
inline constexpr char kKifuAnalysisEngineName[]          = "KifuAnalysis/engineName";
/// 以前の版が保存していたエンジンの番号（読むだけ）
inline constexpr char kKifuAnalysisEngineIndex[]         = "KifuAnalysis/engineIndex";

// --- JosekiWindow ---

// --- Consideration ---
inline constexpr char kConsiderationEngineName[]         = "Consideration/engineName";
/// 以前の版が保存していたエンジンの番号（読むだけ）
inline constexpr char kConsiderationEngineIndex[]        = "Consideration/engineIndex";

// --- TsumeSearch ---
inline constexpr char kTsumeSearchEngineName[]           = "TsumeSearch/engineName";
/// 以前の版が保存していたエンジンのパス（読むだけ）
inline constexpr char kTsumeSearchEnginePath[]           = "TsumeSearch/enginePath";

// --- CsaGameSettings ---
inline constexpr char kCsaEngineName[]                   = "CsaGameSettings/engineName";
/// 以前の版が保存していたエンジンの番号（読むだけ）
inline constexpr char kCsaEngineNumber[]                 = "CsaGameSettings/engineNumber";

// --- CustomDockLayouts ---
inline constexpr char kCustomDockLayoutsGroup[]          = "CustomDockLayouts";

// --- TsumeCollection / TsumePlay ---
/// 判定エンジンの名前（内蔵エンジンなら空の文字列）
inline constexpr char kTsumeCollectionEngineName[] = "TsumeCollection/engineName";
/// 以前の版が保存していた判定エンジンのパス（内蔵なら "@hayanagi"。読むだけ）
inline constexpr char kTsumeCollectionEngine[] = "TsumeCollection/engine";
// --- TsumeshogiGenerator ---
inline constexpr char kTsumeshogiGeneratorEngineName[]        = "TsumeshogiGenerator/engineName";
/// 以前の版が保存していたエンジンの番号（読むだけ）
inline constexpr char kTsumeshogiGeneratorEngineIndex[]       = "TsumeshogiGenerator/engineIndex";

} // namespace SettingsKeys

#endif // SETTINGSKEYS_H
