#include "engineoptiondescriptions.h"
#include <QObject>

// 静的メンバ変数の定義
QHash<QString, QString> EngineOptionDescriptions::s_descriptions;
QHash<QString, EngineOptionCategory> EngineOptionDescriptions::s_categories;
bool EngineOptionDescriptions::s_initialized = false;

QString EngineOptionDescriptions::getDescription(const QString& optionName)
{
    if (!s_initialized) {
        initializeDescriptions();
        initializeCategories();
    }

    return s_descriptions.value(optionName, QString());
}

bool EngineOptionDescriptions::hasDescription(const QString& optionName)
{
    if (!s_initialized) {
        initializeDescriptions();
        initializeCategories();
    }

    return s_descriptions.contains(optionName);
}

EngineOptionCategory EngineOptionDescriptions::getCategory(const QString& optionName)
{
    if (!s_initialized) {
        initializeDescriptions();
        initializeCategories();
    }

    return s_categories.value(optionName, EngineOptionCategory::Other);
}

QString EngineOptionDescriptions::getCategoryDisplayName(EngineOptionCategory category)
{
    switch (category) {
    case EngineOptionCategory::Basic:
        return QObject::tr("基本設定");
    case EngineOptionCategory::Thinking:
        return QObject::tr("思考設定");
    case EngineOptionCategory::TimeControl:
        return QObject::tr("時間制御");
    case EngineOptionCategory::Book:
        return QObject::tr("定跡設定");
    case EngineOptionCategory::GameRule:
        return QObject::tr("対局ルール");
    case EngineOptionCategory::Other:
    default:
        return QObject::tr("その他");
    }
}

QList<EngineOptionCategory> EngineOptionDescriptions::getAllCategories()
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

void EngineOptionDescriptions::initializeDescriptions()
{
    // 基本設定
    s_descriptions["Threads"] = QObject::tr(
        "探索に使用するスレッド数です。CPUの論理コア数に合わせて設定すると最も効率的です。");

    s_descriptions["USI_Hash"] = QObject::tr(
        "置換表(トランスポジションテーブル)に使用するメモリサイズ(MB)です。"
        "大きいほど長時間の探索で有利になりますが、搭載メモリ量を超えないように注意してください。");

    s_descriptions["MultiPV"] = QObject::tr(
        "探索時に出力する候補手の数です。検討モードでは複数の候補手を比較するために増やすと便利です。");

    s_descriptions["EvalDir"] = QObject::tr(
        "評価関数ファイルが格納されているフォルダのパスです。エンジンの実行ファイルからの相対パスで指定します。");

    // Ponder関連
    s_descriptions["USI_Ponder"] = QObject::tr(
        "相手の手番中に先読み(Ponder)を行うかどうかです。"
        "有効にすると対局時に強くなりますが、CPU使用率が高くなります。");

    s_descriptions["Stochastic_Ponder"] = QObject::tr(
        "Ponder時に相手の応手を確率的に予測する機能です。"
        "通常のPonderは最善手のみを予測しますが、この機能では複数の候補手を考慮します。");

    // 探索制限
    s_descriptions["DepthLimit"] = QObject::tr(
        "探索の最大深さを制限します。0の場合は無制限です。デバッグや検討用途で使用します。");

    s_descriptions["NodesLimit"] = QObject::tr(
        "探索するノード数の上限を設定します。0の場合は無制限です。");

    // 検討モード
    s_descriptions["ConsiderationMode"] = QObject::tr(
        "検討モードを有効にします。有効にすると、読み筋の出力形式が検討向けに最適化されます。");

    s_descriptions["OutputFailLHPV"] = QObject::tr(
        "fail-low/fail-high時にも読み筋(PV)を出力するかどうかです。検討時に有効にすると途中経過が分かりやすくなります。");

    // 時間制御
    s_descriptions["NetworkDelay"] = QObject::tr(
        "ネットワーク対局時の通信遅延を補正するための値(ミリ秒)です。"
        "この時間分だけ早めに指し手を返します。");

    s_descriptions["NetworkDelay2"] = QObject::tr(
        "秒読み時など時間に余裕がない場合の最大遅延補正値(ミリ秒)です。"
        "NetworkDelayより大きい値を設定してください。");

    s_descriptions["MinimumThinkingTime"] = QObject::tr(
        "最小思考時間(ミリ秒)です。どんなに持ち時間が少なくても、この時間は考えます。");

    s_descriptions["SlowMover"] = QObject::tr(
        "序盤の時間配分を調整します。100が標準で、大きくすると序盤により多くの時間を使います。");

    s_descriptions["RoundUpToFullSecond"] = QObject::tr(
        "秒単位で時間を切り上げて消費するかどうかです。"
        "多くの将棋サーバーでは秒未満が切り上げられるため、有効にすることを推奨します。");

    // 定跡関連
    s_descriptions["USI_OwnBook"] = QObject::tr(
        "エンジン内蔵の定跡を使用するかどうかです。無効にするとGUI側の定跡のみを使用します。");

    s_descriptions["BookFile"] = QObject::tr(
        "使用する定跡ファイルを選択します。no_bookを選ぶと定跡を使用しません。");

    s_descriptions["BookDir"] = QObject::tr(
        "定跡ファイルが格納されているフォルダのパスです。");

    s_descriptions["BookMoves"] = QObject::tr(
        "定跡を使用する手数の上限です。この手数を超えると定跡を参照せずに探索します。");

    s_descriptions["BookIgnoreRate"] = QObject::tr(
        "定跡を無視する確率(%)です。0なら常に定跡を使用、100なら定跡を使用しません。");

    s_descriptions["BookOnTheFly"] = QObject::tr(
        "定跡ファイルを必要な部分だけ読み込むモードです。"
        "メモリ使用量を抑えられますが、読み込みが遅くなる場合があります。");

    s_descriptions["NarrowBook"] = QObject::tr(
        "定跡の候補手を狭めるかどうかです。有効にすると最善手に近い手のみを選択します。");

    s_descriptions["BookEvalDiff"] = QObject::tr(
        "定跡の候補手として採用する評価値の差の許容範囲です。"
        "この値以内の評価値差の手が候補になります。");

    s_descriptions["BookEvalBlackLimit"] = QObject::tr(
        "先手番で定跡を採用する評価値の下限です。これより悪い評価の定跡手は採用しません。");

    s_descriptions["BookEvalWhiteLimit"] = QObject::tr(
        "後手番で定跡を採用する評価値の下限です。これより悪い評価の定跡手は採用しません。");

    s_descriptions["BookDepthLimit"] = QObject::tr(
        "定跡として採用する最小探索深さです。これより浅い探索で登録された定跡は使用しません。");

    s_descriptions["ConsiderBookMoveCount"] = QObject::tr(
        "定跡選択時に出現頻度を考慮するかどうかです。有効にすると頻出する手を優先します。");

    s_descriptions["BookPvMoves"] = QObject::tr(
        "定跡の読み筋として出力する手数です。");

    s_descriptions["IgnoreBookPly"] = QObject::tr(
        "定跡の手数制限を無視するかどうかです。");

    s_descriptions["FlippedBook"] = QObject::tr(
        "後手番の定跡を先手番の定跡から反転して生成するかどうかです。");

    // 対局ルール関連
    s_descriptions["EnteringKingRule"] = QObject::tr(
        "入玉時の勝敗判定ルールを選択します。\n"
        "CSARule27: CSAルール(27点法)\n"
        "CSARule24: CSAルール(24点法)\n"
        "TryRule: トライルール");

    s_descriptions["MaxMovesToDraw"] = QObject::tr(
        "この手数に達すると引き分けとします。0の場合は手数による引き分けはありません。"
        "256手ルールを適用する場合は256を設定します。");

    s_descriptions["ResignValue"] = QObject::tr(
        "この評価値以下になると投了します。99999の場合は自動投了しません。");

    s_descriptions["DrawValueBlack"] = QObject::tr(
        "先手番での引き分けの評価値です。通常はわずかにマイナスに設定して引き分けを避けます。");

    s_descriptions["DrawValueWhite"] = QObject::tr(
        "後手番での引き分けの評価値です。通常はわずかにマイナスに設定して引き分けを避けます。");

    // その他
    s_descriptions["NumaPolicy"] = QObject::tr(
        "NUMAアーキテクチャでのメモリ割り当てポリシーです。"
        "autoで自動設定、その他にbind/interleave/noneが指定できます。");

    s_descriptions["DebugLogFile"] = QObject::tr(
        "デバッグログを出力するファイルのパスです。空の場合はログを出力しません。");

    s_descriptions["PvInterval"] = QObject::tr(
        "読み筋(PV)を出力する間隔(ミリ秒)です。0にすると深さが更新されるたびに出力します。");

    s_descriptions["GenerateAllLegalMoves"] = QObject::tr(
        "合法手生成時にすべての合法手を生成するかどうかです。通常はfalseで問題ありません。");

    s_descriptions["FV_SCALE"] = QObject::tr(
        "評価関数のスケーリング係数です。評価関数に合わせて調整しますが、通常は変更不要です。");
}

void EngineOptionDescriptions::initializeCategories()
{
    // 基本設定
    s_categories["Threads"] = EngineOptionCategory::Basic;
    s_categories["USI_Hash"] = EngineOptionCategory::Basic;
    s_categories["MultiPV"] = EngineOptionCategory::Basic;
    s_categories["EvalDir"] = EngineOptionCategory::Basic;
    s_categories["FV_SCALE"] = EngineOptionCategory::Basic;

    // 思考設定
    s_categories["USI_Ponder"] = EngineOptionCategory::Thinking;
    s_categories["Stochastic_Ponder"] = EngineOptionCategory::Thinking;
    s_categories["ConsiderationMode"] = EngineOptionCategory::Thinking;
    s_categories["DepthLimit"] = EngineOptionCategory::Thinking;
    s_categories["NodesLimit"] = EngineOptionCategory::Thinking;
    s_categories["OutputFailLHPV"] = EngineOptionCategory::Thinking;
    s_categories["GenerateAllLegalMoves"] = EngineOptionCategory::Thinking;
    s_categories["PvInterval"] = EngineOptionCategory::Thinking;

    // 時間制御
    s_categories["NetworkDelay"] = EngineOptionCategory::TimeControl;
    s_categories["NetworkDelay2"] = EngineOptionCategory::TimeControl;
    s_categories["MinimumThinkingTime"] = EngineOptionCategory::TimeControl;
    s_categories["SlowMover"] = EngineOptionCategory::TimeControl;
    s_categories["RoundUpToFullSecond"] = EngineOptionCategory::TimeControl;

    // 定跡設定
    s_categories["USI_OwnBook"] = EngineOptionCategory::Book;
    s_categories["BookFile"] = EngineOptionCategory::Book;
    s_categories["BookDir"] = EngineOptionCategory::Book;
    s_categories["BookMoves"] = EngineOptionCategory::Book;
    s_categories["BookIgnoreRate"] = EngineOptionCategory::Book;
    s_categories["BookOnTheFly"] = EngineOptionCategory::Book;
    s_categories["NarrowBook"] = EngineOptionCategory::Book;
    s_categories["BookEvalDiff"] = EngineOptionCategory::Book;
    s_categories["BookEvalBlackLimit"] = EngineOptionCategory::Book;
    s_categories["BookEvalWhiteLimit"] = EngineOptionCategory::Book;
    s_categories["BookDepthLimit"] = EngineOptionCategory::Book;
    s_categories["ConsiderBookMoveCount"] = EngineOptionCategory::Book;
    s_categories["BookPvMoves"] = EngineOptionCategory::Book;
    s_categories["IgnoreBookPly"] = EngineOptionCategory::Book;
    s_categories["FlippedBook"] = EngineOptionCategory::Book;

    // 対局ルール
    s_categories["EnteringKingRule"] = EngineOptionCategory::GameRule;
    s_categories["MaxMovesToDraw"] = EngineOptionCategory::GameRule;
    s_categories["ResignValue"] = EngineOptionCategory::GameRule;
    s_categories["DrawValueBlack"] = EngineOptionCategory::GameRule;
    s_categories["DrawValueWhite"] = EngineOptionCategory::GameRule;

    // その他（明示的に登録しなくてもOther扱いになるが、明示しておく）
    s_categories["NumaPolicy"] = EngineOptionCategory::Other;
    s_categories["DebugLogFile"] = EngineOptionCategory::Other;

    s_initialized = true;
}
