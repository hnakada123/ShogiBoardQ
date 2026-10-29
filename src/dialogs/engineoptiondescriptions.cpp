/// @file engineoptiondescriptions.cpp
/// @brief USIエンジンオプション説明データベースの実装

#include "engineoptiondescriptions.h"

// ============================================================
// 静的メンバ変数
// ============================================================

QHash<QString, QString> EngineOptionDescriptions::s_yaneuraouDescriptions;
QHash<QString, EngineOptionCategory> EngineOptionDescriptions::s_yaneuraouCategories;
QHash<QString, QString> EngineOptionDescriptions::s_gikouDescriptions;
QHash<QString, EngineOptionCategory> EngineOptionDescriptions::s_gikouCategories;
bool EngineOptionDescriptions::s_initialized = false;

// ============================================================
// エンジン判定
// ============================================================

bool EngineOptionDescriptions::isYaneuraOuEngine(const QString& engineName)
{
    return engineName.contains(QStringLiteral("YaneuraOu"), Qt::CaseInsensitive) ||
           engineName.contains(QStringLiteral("Yaneuraou"), Qt::CaseInsensitive) ||
           engineName.contains(QStringLiteral("やねうら王"), Qt::CaseInsensitive);
}

bool EngineOptionDescriptions::isGikouEngine(const QString& engineName)
{
    return engineName.contains(QStringLiteral("Gikou"), Qt::CaseInsensitive) ||
           engineName.contains(QStringLiteral("技巧"), Qt::CaseInsensitive);
}

// ============================================================
// 説明・カテゴリ取得
// ============================================================

QString EngineOptionDescriptions::description(const QString& engineName, const QString& optionName)
{
    if (!s_initialized) {
        initializeDescriptions();
        initializeCategories();
    }

    if (isYaneuraOuEngine(engineName)) {
        return s_yaneuraouDescriptions.value(optionName, QString());
    }

    if (isGikouEngine(engineName)) {
        return s_gikouDescriptions.value(optionName, QString());
    }

    return QString();
}

EngineOptionCategory EngineOptionDescriptions::category(const QString& engineName, const QString& optionName)
{
    if (!s_initialized) {
        initializeDescriptions();
        initializeCategories();
    }

    if (isYaneuraOuEngine(engineName)) {
        return s_yaneuraouCategories.value(optionName, EngineOptionCategory::Other);
    }

    if (isGikouEngine(engineName)) {
        return s_gikouCategories.value(optionName, EngineOptionCategory::Other);
    }

    return EngineOptionCategory::Other;
}

QString EngineOptionDescriptions::categoryDisplayName(EngineOptionCategory category)
{
    switch (category) {
    case EngineOptionCategory::Basic:
        return tr("基本設定");
    case EngineOptionCategory::Thinking:
        return tr("思考設定");
    case EngineOptionCategory::TimeControl:
        return tr("時間制御");
    case EngineOptionCategory::Book:
        return tr("定跡設定");
    case EngineOptionCategory::GameRule:
        return tr("対局ルール");
    case EngineOptionCategory::Other:
    default:
        return tr("その他");
    }
}

QList<EngineOptionCategory> EngineOptionDescriptions::allCategories()
{
    return {
        EngineOptionCategory::Basic,
        EngineOptionCategory::Thinking,
        EngineOptionCategory::TimeControl,
        EngineOptionCategory::Book,
        EngineOptionCategory::GameRule,
        EngineOptionCategory::Other
    };
}

// ============================================================
// 初期化（説明テキスト）
// ============================================================

void EngineOptionDescriptions::initializeDescriptions()
{
    // --- YaneuraOu: 基本設定 ---

    s_yaneuraouDescriptions["Threads"] = tr(
        "探索に使用するスレッド数");

    s_yaneuraouDescriptions["USI_Hash"] = tr(
        "置換表サイズ(MB)");

    s_yaneuraouDescriptions["MultiPV"] = tr(
        "出力する候補手の数");

    s_yaneuraouDescriptions["EvalDir"] = tr(
        "評価関数フォルダのパス");

    // --- YaneuraOu: Ponder ---

    s_yaneuraouDescriptions["USI_Ponder"] = tr(
        "相手の手番中の先読み");

    s_yaneuraouDescriptions["Stochastic_Ponder"] = tr(
        "Ponder時の確率的な応手予測");

    // --- YaneuraOu: 探索制限 ---

    s_yaneuraouDescriptions["DepthLimit"] = tr(
        "探索の最大深さ(0で無制限)");

    s_yaneuraouDescriptions["NodesLimit"] = tr(
        "探索ノード数の上限(0で無制限)");

    // --- YaneuraOu: 検討モード ---

    s_yaneuraouDescriptions["ConsiderationMode"] = tr(
        "検討モードの有効化");

    s_yaneuraouDescriptions["OutputFailLHPV"] = tr(
        "fail-low/fail-high時の読み筋出力");

    // --- YaneuraOu: 時間制御 ---

    s_yaneuraouDescriptions["NetworkDelay"] = tr(
        "通信遅延補正(ミリ秒)");

    s_yaneuraouDescriptions["NetworkDelay2"] = tr(
        "秒読み時の最大遅延補正(ミリ秒)");

    s_yaneuraouDescriptions["MinimumThinkingTime"] = tr(
        "最小思考時間(ミリ秒)");

    s_yaneuraouDescriptions["SlowMover"] = tr(
        "序盤の時間配分調整(100が標準)");

    s_yaneuraouDescriptions["RoundUpToFullSecond"] = tr(
        "消費時間の秒単位切り上げ");

    // --- YaneuraOu: 定跡 ---

    s_yaneuraouDescriptions["USI_OwnBook"] = tr(
        "エンジン内蔵定跡の使用");

    s_yaneuraouDescriptions["BookFile"] = tr(
        "使用する定跡ファイル");

    s_yaneuraouDescriptions["BookDir"] = tr(
        "定跡フォルダのパス");

    s_yaneuraouDescriptions["BookMoves"] = tr(
        "定跡を使用する手数の上限");

    s_yaneuraouDescriptions["BookIgnoreRate"] = tr(
        "定跡を無視する確率(%)");

    s_yaneuraouDescriptions["BookOnTheFly"] = tr(
        "定跡の部分読み込み");

    s_yaneuraouDescriptions["NarrowBook"] = tr(
        "最善手に近い定跡手のみの選択");

    s_yaneuraouDescriptions["BookEvalDiff"] = tr(
        "定跡の評価値差の許容範囲");

    s_yaneuraouDescriptions["BookEvalBlackLimit"] = tr(
        "先手番での定跡採用の評価値下限");

    s_yaneuraouDescriptions["BookEvalWhiteLimit"] = tr(
        "後手番での定跡採用の評価値下限");

    s_yaneuraouDescriptions["BookDepthLimit"] = tr(
        "定跡として採用する最小探索深さ（0で深さを問わない）");

    s_yaneuraouDescriptions["ConsiderBookMoveCount"] = tr(
        "定跡選択時の出現頻度考慮");

    s_yaneuraouDescriptions["BookPvMoves"] = tr(
        "定跡の読み筋出力手数");

    s_yaneuraouDescriptions["IgnoreBookPly"] = tr(
        "定跡の局面の手数を無視（手数違いの同じ局面にも定跡を使う）");

    s_yaneuraouDescriptions["FlippedBook"] = tr(
        "後手定跡の先手定跡からの反転生成");

    // --- YaneuraOu: 対局ルール ---

    s_yaneuraouDescriptions["EnteringKingRule"] = tr(
        "入玉時の勝敗判定ルール");

    s_yaneuraouDescriptions["MaxMovesToDraw"] = tr(
        "引き分けとなる手数(0で無効)");

    s_yaneuraouDescriptions["ResignValue"] = tr(
        "投了する評価値");

    s_yaneuraouDescriptions["DrawValueBlack"] = tr(
        "先手番での引き分けの評価値");

    s_yaneuraouDescriptions["DrawValueWhite"] = tr(
        "後手番での引き分けの評価値");

    // --- YaneuraOu: その他 ---

    s_yaneuraouDescriptions["NumaPolicy"] = tr(
        "NUMAメモリ割り当てポリシー");

    s_yaneuraouDescriptions["DebugLogFile"] = tr(
        "デバッグログの出力先ファイル");

    s_yaneuraouDescriptions["PvInterval"] = tr(
        "読み筋の出力間隔(ミリ秒)");

    s_yaneuraouDescriptions["GenerateAllLegalMoves"] = tr(
        "すべての合法手の生成");

    s_yaneuraouDescriptions["FV_SCALE"] = tr(
        "評価関数のスケーリング係数");

    // --- Gikou（技巧） ---

    s_gikouDescriptions["Threads"] = tr(
        "探索に使用するスレッド数");

    s_gikouDescriptions["MultiPV"] = tr(
        "候補手の数");

    s_gikouDescriptions["DepthLimit"] = tr(
        "読みの深さ（強さのレベル調節用）");

    s_gikouDescriptions["OwnBook"] = tr(
        "定跡の使用");

    s_gikouDescriptions["BookFile"] = tr(
        "定跡ファイル(戦型選択用)");

    s_gikouDescriptions["BookMaxPly"] = tr(
        "定跡使用の手数上限");

    s_gikouDescriptions["NarrowBook"] = tr(
        "低勝率の定跡手の除外");

    s_gikouDescriptions["TinyBook"] = tr(
        "低出現頻度の定跡手の除外");

    s_gikouDescriptions["MinThinkingTime"] = tr(
        "最小思考時間(ミリ秒)");

    s_gikouDescriptions["ByoyomiMargin"] = tr(
        "秒読み時の余裕(ミリ秒)");

    s_gikouDescriptions["SuddenDeathMargin"] = tr(
        "切れ負けルール時の余裕(秒)");

    s_gikouDescriptions["FischerMargin"] = tr(
        "フィッシャールール時の余裕(ミリ秒)");

    s_gikouDescriptions["ResignScore"] = tr(
        "技巧が投了する評価値");

    s_gikouDescriptions["DrawScore"] = tr(
        "千日手の評価値");
}

// ============================================================
// 初期化（カテゴリ分類）
// ============================================================

void EngineOptionDescriptions::initializeCategories()
{
    // --- YaneuraOu: 基本設定 ---
    s_yaneuraouCategories["Threads"] = EngineOptionCategory::Basic;
    s_yaneuraouCategories["USI_Hash"] = EngineOptionCategory::Basic;
    s_yaneuraouCategories["MultiPV"] = EngineOptionCategory::Basic;
    s_yaneuraouCategories["EvalDir"] = EngineOptionCategory::Basic;
    s_yaneuraouCategories["FV_SCALE"] = EngineOptionCategory::Basic;

    // --- YaneuraOu: 思考設定 ---
    s_yaneuraouCategories["USI_Ponder"] = EngineOptionCategory::Thinking;
    s_yaneuraouCategories["Stochastic_Ponder"] = EngineOptionCategory::Thinking;
    s_yaneuraouCategories["ConsiderationMode"] = EngineOptionCategory::Thinking;
    s_yaneuraouCategories["DepthLimit"] = EngineOptionCategory::Thinking;
    s_yaneuraouCategories["NodesLimit"] = EngineOptionCategory::Thinking;
    s_yaneuraouCategories["OutputFailLHPV"] = EngineOptionCategory::Thinking;
    s_yaneuraouCategories["GenerateAllLegalMoves"] = EngineOptionCategory::Thinking;
    s_yaneuraouCategories["PvInterval"] = EngineOptionCategory::Thinking;

    // --- YaneuraOu: 時間制御 ---
    s_yaneuraouCategories["NetworkDelay"] = EngineOptionCategory::TimeControl;
    s_yaneuraouCategories["NetworkDelay2"] = EngineOptionCategory::TimeControl;
    s_yaneuraouCategories["MinimumThinkingTime"] = EngineOptionCategory::TimeControl;
    s_yaneuraouCategories["SlowMover"] = EngineOptionCategory::TimeControl;
    s_yaneuraouCategories["RoundUpToFullSecond"] = EngineOptionCategory::TimeControl;

    // --- YaneuraOu: 定跡設定 ---
    s_yaneuraouCategories["USI_OwnBook"] = EngineOptionCategory::Book;
    s_yaneuraouCategories["BookFile"] = EngineOptionCategory::Book;
    s_yaneuraouCategories["BookDir"] = EngineOptionCategory::Book;
    s_yaneuraouCategories["BookMoves"] = EngineOptionCategory::Book;
    s_yaneuraouCategories["BookIgnoreRate"] = EngineOptionCategory::Book;
    s_yaneuraouCategories["BookOnTheFly"] = EngineOptionCategory::Book;
    s_yaneuraouCategories["NarrowBook"] = EngineOptionCategory::Book;
    s_yaneuraouCategories["BookEvalDiff"] = EngineOptionCategory::Book;
    s_yaneuraouCategories["BookEvalBlackLimit"] = EngineOptionCategory::Book;
    s_yaneuraouCategories["BookEvalWhiteLimit"] = EngineOptionCategory::Book;
    s_yaneuraouCategories["BookDepthLimit"] = EngineOptionCategory::Book;
    s_yaneuraouCategories["ConsiderBookMoveCount"] = EngineOptionCategory::Book;
    s_yaneuraouCategories["BookPvMoves"] = EngineOptionCategory::Book;
    s_yaneuraouCategories["IgnoreBookPly"] = EngineOptionCategory::Book;
    s_yaneuraouCategories["FlippedBook"] = EngineOptionCategory::Book;

    // --- YaneuraOu: 対局ルール ---
    s_yaneuraouCategories["EnteringKingRule"] = EngineOptionCategory::GameRule;
    s_yaneuraouCategories["MaxMovesToDraw"] = EngineOptionCategory::GameRule;
    s_yaneuraouCategories["ResignValue"] = EngineOptionCategory::GameRule;
    s_yaneuraouCategories["DrawValueBlack"] = EngineOptionCategory::GameRule;
    s_yaneuraouCategories["DrawValueWhite"] = EngineOptionCategory::GameRule;

    // --- YaneuraOu: その他 ---
    // 明示的に登録しなくてもOther扱いになるが、明示しておく
    s_yaneuraouCategories["NumaPolicy"] = EngineOptionCategory::Other;
    s_yaneuraouCategories["DebugLogFile"] = EngineOptionCategory::Other;

    // --- Gikou: 基本設定 ---
    s_gikouCategories["Threads"] = EngineOptionCategory::Basic;
    s_gikouCategories["MultiPV"] = EngineOptionCategory::Basic;
    s_gikouCategories["DepthLimit"] = EngineOptionCategory::Basic;

    // --- Gikou: 定跡設定 ---
    s_gikouCategories["OwnBook"] = EngineOptionCategory::Book;
    s_gikouCategories["BookFile"] = EngineOptionCategory::Book;
    s_gikouCategories["BookMaxPly"] = EngineOptionCategory::Book;
    s_gikouCategories["NarrowBook"] = EngineOptionCategory::Book;
    s_gikouCategories["TinyBook"] = EngineOptionCategory::Book;

    // --- Gikou: 時間制御 ---
    s_gikouCategories["MinThinkingTime"] = EngineOptionCategory::TimeControl;
    s_gikouCategories["ByoyomiMargin"] = EngineOptionCategory::TimeControl;
    s_gikouCategories["SuddenDeathMargin"] = EngineOptionCategory::TimeControl;
    s_gikouCategories["FischerMargin"] = EngineOptionCategory::TimeControl;

    // --- Gikou: 対局ルール ---
    s_gikouCategories["ResignScore"] = EngineOptionCategory::GameRule;
    s_gikouCategories["DrawScore"] = EngineOptionCategory::GameRule;

    s_initialized = true;
}
