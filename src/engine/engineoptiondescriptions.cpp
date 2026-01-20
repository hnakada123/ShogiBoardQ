#include "engineoptiondescriptions.h"
#include <QObject>

// 静的メンバ変数の定義
QHash<QString, QString> EngineOptionDescriptions::s_yaneuraouDescriptions;
QHash<QString, EngineOptionCategory> EngineOptionDescriptions::s_yaneuraouCategories;
QHash<QString, QString> EngineOptionDescriptions::s_gikouDescriptions;
QHash<QString, EngineOptionCategory> EngineOptionDescriptions::s_gikouCategories;
bool EngineOptionDescriptions::s_initialized = false;

bool EngineOptionDescriptions::isYaneuraOuEngine(const QString& engineName)
{
    // エンジン名に "YaneuraOu" が含まれているかどうかで判定（大文字小文字を区別しない）
    return engineName.contains(QStringLiteral("YaneuraOu"), Qt::CaseInsensitive) ||
           engineName.contains(QStringLiteral("Yaneuraou"), Qt::CaseInsensitive) ||
           engineName.contains(QStringLiteral("やねうら王"), Qt::CaseInsensitive);
}

bool EngineOptionDescriptions::isGikouEngine(const QString& engineName)
{
    // エンジン名に "Gikou" または "技巧" が含まれているかどうかで判定
    return engineName.contains(QStringLiteral("Gikou"), Qt::CaseInsensitive) ||
           engineName.contains(QStringLiteral("技巧"), Qt::CaseInsensitive);
}

bool EngineOptionDescriptions::isEngineSupported(const QString& engineName)
{
    // YaneuraOuとGikouに対応
    return isYaneuraOuEngine(engineName) || isGikouEngine(engineName);
}

QString EngineOptionDescriptions::getDescription(const QString& engineName, const QString& optionName)
{
    if (!s_initialized) {
        initializeDescriptions();
        initializeCategories();
    }

    // YaneuraOu系エンジンの場合
    if (isYaneuraOuEngine(engineName)) {
        return s_yaneuraouDescriptions.value(optionName, QString());
    }

    // Gikou系エンジンの場合
    if (isGikouEngine(engineName)) {
        return s_gikouDescriptions.value(optionName, QString());
    }

    // その他のエンジンは説明なし
    return QString();
}

bool EngineOptionDescriptions::hasDescription(const QString& engineName, const QString& optionName)
{
    if (!s_initialized) {
        initializeDescriptions();
        initializeCategories();
    }

    // YaneuraOu系エンジンの場合
    if (isYaneuraOuEngine(engineName)) {
        return s_yaneuraouDescriptions.contains(optionName);
    }

    // Gikou系エンジンの場合
    if (isGikouEngine(engineName)) {
        return s_gikouDescriptions.contains(optionName);
    }

    return false;
}

EngineOptionCategory EngineOptionDescriptions::getCategory(const QString& engineName, const QString& optionName)
{
    if (!s_initialized) {
        initializeDescriptions();
        initializeCategories();
    }

    // YaneuraOu系エンジンの場合
    if (isYaneuraOuEngine(engineName)) {
        return s_yaneuraouCategories.value(optionName, EngineOptionCategory::Other);
    }

    // Gikou系エンジンの場合
    if (isGikouEngine(engineName)) {
        return s_gikouCategories.value(optionName, EngineOptionCategory::Other);
    }

    // その他のエンジンは全て「その他」カテゴリ
    return EngineOptionCategory::Other;
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
    // ===== YaneuraOu用の説明 =====

    // 基本設定
    s_yaneuraouDescriptions["Threads"] = QObject::tr(
        "探索に使用するスレッド数です。CPUの論理コア数に合わせて設定すると最も効率的です。");

    s_yaneuraouDescriptions["USI_Hash"] = QObject::tr(
        "置換表(トランスポジションテーブル)に使用するメモリサイズ(MB)です。"
        "大きいほど長時間の探索で有利になりますが、搭載メモリ量を超えないように注意してください。");

    s_yaneuraouDescriptions["MultiPV"] = QObject::tr(
        "探索時に出力する候補手の数です。検討モードでは複数の候補手を比較するために増やすと便利です。");

    s_yaneuraouDescriptions["EvalDir"] = QObject::tr(
        "評価関数ファイルが格納されているフォルダのパスです。エンジンの実行ファイルからの相対パスで指定します。");

    // Ponder関連
    s_yaneuraouDescriptions["USI_Ponder"] = QObject::tr(
        "相手の手番中に先読み(Ponder)を行うかどうかです。"
        "有効にすると対局時に強くなりますが、CPU使用率が高くなります。");

    s_yaneuraouDescriptions["Stochastic_Ponder"] = QObject::tr(
        "Ponder時に相手の応手を確率的に予測する機能です。"
        "通常のPonderは最善手のみを予測しますが、この機能では複数の候補手を考慮します。");

    // 探索制限
    s_yaneuraouDescriptions["DepthLimit"] = QObject::tr(
        "探索の最大深さを制限します。0の場合は無制限です。デバッグや検討用途で使用します。");

    s_yaneuraouDescriptions["NodesLimit"] = QObject::tr(
        "探索するノード数の上限を設定します。0の場合は無制限です。");

    // 検討モード
    s_yaneuraouDescriptions["ConsiderationMode"] = QObject::tr(
        "検討モードを有効にします。有効にすると、読み筋の出力形式が検討向けに最適化されます。");

    s_yaneuraouDescriptions["OutputFailLHPV"] = QObject::tr(
        "fail-low/fail-high時にも読み筋(PV)を出力するかどうかです。検討時に有効にすると途中経過が分かりやすくなります。");

    // 時間制御
    s_yaneuraouDescriptions["NetworkDelay"] = QObject::tr(
        "ネットワーク対局時の通信遅延を補正するための値(ミリ秒)です。"
        "この時間分だけ早めに指し手を返します。");

    s_yaneuraouDescriptions["NetworkDelay2"] = QObject::tr(
        "秒読み時など時間に余裕がない場合の最大遅延補正値(ミリ秒)です。"
        "NetworkDelayより大きい値を設定してください。");

    s_yaneuraouDescriptions["MinimumThinkingTime"] = QObject::tr(
        "最小思考時間(ミリ秒)です。どんなに持ち時間が少なくても、この時間は考えます。");

    s_yaneuraouDescriptions["SlowMover"] = QObject::tr(
        "序盤の時間配分を調整します。100が標準で、大きくすると序盤により多くの時間を使います。");

    s_yaneuraouDescriptions["RoundUpToFullSecond"] = QObject::tr(
        "秒単位で時間を切り上げて消費するかどうかです。"
        "多くの将棋サーバーでは秒未満が切り上げられるため、有効にすることを推奨します。");

    // 定跡関連
    s_yaneuraouDescriptions["USI_OwnBook"] = QObject::tr(
        "エンジン内蔵の定跡を使用するかどうかです。無効にするとGUI側の定跡のみを使用します。");

    s_yaneuraouDescriptions["BookFile"] = QObject::tr(
        "使用する定跡ファイルを選択します。no_bookを選ぶと定跡を使用しません。");

    s_yaneuraouDescriptions["BookDir"] = QObject::tr(
        "定跡ファイルが格納されているフォルダのパスです。");

    s_yaneuraouDescriptions["BookMoves"] = QObject::tr(
        "定跡を使用する手数の上限です。この手数を超えると定跡を参照せずに探索します。");

    s_yaneuraouDescriptions["BookIgnoreRate"] = QObject::tr(
        "定跡を無視する確率(%)です。0なら常に定跡を使用、100なら定跡を使用しません。");

    s_yaneuraouDescriptions["BookOnTheFly"] = QObject::tr(
        "定跡ファイルを必要な部分だけ読み込むモードです。"
        "メモリ使用量を抑えられますが、読み込みが遅くなる場合があります。");

    s_yaneuraouDescriptions["NarrowBook"] = QObject::tr(
        "定跡の候補手を狭めるかどうかです。有効にすると最善手に近い手のみを選択します。");

    s_yaneuraouDescriptions["BookEvalDiff"] = QObject::tr(
        "定跡の候補手として採用する評価値の差の許容範囲です。"
        "この値以内の評価値差の手が候補になります。");

    s_yaneuraouDescriptions["BookEvalBlackLimit"] = QObject::tr(
        "先手番で定跡を採用する評価値の下限です。これより悪い評価の定跡手は採用しません。");

    s_yaneuraouDescriptions["BookEvalWhiteLimit"] = QObject::tr(
        "後手番で定跡を採用する評価値の下限です。これより悪い評価の定跡手は採用しません。");

    s_yaneuraouDescriptions["BookDepthLimit"] = QObject::tr(
        "定跡として採用する最小探索深さです。これより浅い探索で登録された定跡は使用しません。");

    s_yaneuraouDescriptions["ConsiderBookMoveCount"] = QObject::tr(
        "定跡選択時に出現頻度を考慮するかどうかです。有効にすると頻出する手を優先します。");

    s_yaneuraouDescriptions["BookPvMoves"] = QObject::tr(
        "定跡の読み筋として出力する手数です。");

    s_yaneuraouDescriptions["IgnoreBookPly"] = QObject::tr(
        "定跡の手数制限を無視するかどうかです。");

    s_yaneuraouDescriptions["FlippedBook"] = QObject::tr(
        "後手番の定跡を先手番の定跡から反転して生成するかどうかです。");

    // 対局ルール関連
    s_yaneuraouDescriptions["EnteringKingRule"] = QObject::tr(
        "入玉時の勝敗判定ルールを選択します。\n"
        "CSARule27: CSAルール(27点法)\n"
        "CSARule24: CSAルール(24点法)\n"
        "TryRule: トライルール");

    s_yaneuraouDescriptions["MaxMovesToDraw"] = QObject::tr(
        "この手数に達すると引き分けとします。0の場合は手数による引き分けはありません。"
        "256手ルールを適用する場合は256を設定します。");

    s_yaneuraouDescriptions["ResignValue"] = QObject::tr(
        "この評価値以下になると投了します。99999の場合は自動投了しません。");

    s_yaneuraouDescriptions["DrawValueBlack"] = QObject::tr(
        "先手番での引き分けの評価値です。通常はわずかにマイナスに設定して引き分けを避けます。");

    s_yaneuraouDescriptions["DrawValueWhite"] = QObject::tr(
        "後手番での引き分けの評価値です。通常はわずかにマイナスに設定して引き分けを避けます。");

    // その他
    s_yaneuraouDescriptions["NumaPolicy"] = QObject::tr(
        "NUMAアーキテクチャでのメモリ割り当てポリシーです。"
        "autoで自動設定、その他にbind/interleave/noneが指定できます。");

    s_yaneuraouDescriptions["DebugLogFile"] = QObject::tr(
        "デバッグログを出力するファイルのパスです。空の場合はログを出力しません。");

    s_yaneuraouDescriptions["PvInterval"] = QObject::tr(
        "読み筋(PV)を出力する間隔(ミリ秒)です。0にすると深さが更新されるたびに出力します。");

    s_yaneuraouDescriptions["GenerateAllLegalMoves"] = QObject::tr(
        "合法手生成時にすべての合法手を生成するかどうかです。通常はfalseで問題ありません。");

    s_yaneuraouDescriptions["FV_SCALE"] = QObject::tr(
        "評価関数のスケーリング係数です。評価関数に合わせて調整しますが、通常は変更不要です。");

    // ===== Gikou（技巧）用の説明 =====

    s_gikouDescriptions["Threads"] = QObject::tr(
        "探索に使用するスレッド数です。CPUの論理コア数に合わせて設定すると効率的です。");

    s_gikouDescriptions["MultiPV"] = QObject::tr(
        "探索時に出力する候補手の数です。検討モードで複数の候補手を比較する際に増やすと便利です。");

    s_gikouDescriptions["OwnBook"] = QObject::tr(
        "エンジン内蔵の定跡を使用するかどうかです。");

    s_gikouDescriptions["BookFile"] = QObject::tr(
        "使用する定跡ファイルです。戦型選択に使用されます。");

    s_gikouDescriptions["BookMaxPly"] = QObject::tr(
        "定跡を何手目まで使用するかを設定します。この手数を超えると定跡を参照せずに探索します。");

    s_gikouDescriptions["NarrowBook"] = QObject::tr(
        "勝率が相対的に低い定跡手を選ばないようにします。より安定した指し手を選択します。");

    s_gikouDescriptions["TinyBook"] = QObject::tr(
        "出現頻度の低い定跡手を選ばないようにします。よく指される定跡手のみを使用します。");

    s_gikouDescriptions["DepthLimit"] = QObject::tr(
        "探索の深さ（読みの深さ）を制限します。強さのレベル調節に使用できます。"
        "100が最大で、小さい値にすると弱くなります。");

    s_gikouDescriptions["ResignScore"] = QObject::tr(
        "この評価値以下になると技巧が投了します。負の値で設定します。");

    s_gikouDescriptions["DrawScore"] = QObject::tr(
        "千日手の評価値です。0が標準で、正の値にすると千日手を狙いやすくなります。");

    s_gikouDescriptions["MinThinkingTime"] = QObject::tr(
        "最小思考時間(ミリ秒)です。どんなに時間が少なくても、最低この時間は考えます。");

    s_gikouDescriptions["ByoyomiMargin"] = QObject::tr(
        "秒読み時の余裕(ミリ秒)です。時間切れを防ぐためのマージンを設定します。");

    s_gikouDescriptions["SuddenDeathMargin"] = QObject::tr(
        "切れ負けルール時の余裕(秒)です。時間切れを防ぐためのマージンを設定します。");

    s_gikouDescriptions["FischerMargin"] = QObject::tr(
        "フィッシャールール（加算秒あり）時の余裕(ミリ秒)です。時間切れを防ぐためのマージンを設定します。");
}

void EngineOptionDescriptions::initializeCategories()
{
    // ===== YaneuraOu用のカテゴリ =====

    // 基本設定
    s_yaneuraouCategories["Threads"] = EngineOptionCategory::Basic;
    s_yaneuraouCategories["USI_Hash"] = EngineOptionCategory::Basic;
    s_yaneuraouCategories["MultiPV"] = EngineOptionCategory::Basic;
    s_yaneuraouCategories["EvalDir"] = EngineOptionCategory::Basic;
    s_yaneuraouCategories["FV_SCALE"] = EngineOptionCategory::Basic;

    // 思考設定
    s_yaneuraouCategories["USI_Ponder"] = EngineOptionCategory::Thinking;
    s_yaneuraouCategories["Stochastic_Ponder"] = EngineOptionCategory::Thinking;
    s_yaneuraouCategories["ConsiderationMode"] = EngineOptionCategory::Thinking;
    s_yaneuraouCategories["DepthLimit"] = EngineOptionCategory::Thinking;
    s_yaneuraouCategories["NodesLimit"] = EngineOptionCategory::Thinking;
    s_yaneuraouCategories["OutputFailLHPV"] = EngineOptionCategory::Thinking;
    s_yaneuraouCategories["GenerateAllLegalMoves"] = EngineOptionCategory::Thinking;
    s_yaneuraouCategories["PvInterval"] = EngineOptionCategory::Thinking;

    // 時間制御
    s_yaneuraouCategories["NetworkDelay"] = EngineOptionCategory::TimeControl;
    s_yaneuraouCategories["NetworkDelay2"] = EngineOptionCategory::TimeControl;
    s_yaneuraouCategories["MinimumThinkingTime"] = EngineOptionCategory::TimeControl;
    s_yaneuraouCategories["SlowMover"] = EngineOptionCategory::TimeControl;
    s_yaneuraouCategories["RoundUpToFullSecond"] = EngineOptionCategory::TimeControl;

    // 定跡設定
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

    // 対局ルール
    s_yaneuraouCategories["EnteringKingRule"] = EngineOptionCategory::GameRule;
    s_yaneuraouCategories["MaxMovesToDraw"] = EngineOptionCategory::GameRule;
    s_yaneuraouCategories["ResignValue"] = EngineOptionCategory::GameRule;
    s_yaneuraouCategories["DrawValueBlack"] = EngineOptionCategory::GameRule;
    s_yaneuraouCategories["DrawValueWhite"] = EngineOptionCategory::GameRule;

    // その他（明示的に登録しなくてもOther扱いになるが、明示しておく）
    s_yaneuraouCategories["NumaPolicy"] = EngineOptionCategory::Other;
    s_yaneuraouCategories["DebugLogFile"] = EngineOptionCategory::Other;

    // ===== Gikou（技巧）用のカテゴリ =====

    // 基本設定
    s_gikouCategories["Threads"] = EngineOptionCategory::Basic;
    s_gikouCategories["MultiPV"] = EngineOptionCategory::Basic;
    s_gikouCategories["DepthLimit"] = EngineOptionCategory::Basic;

    // 定跡設定
    s_gikouCategories["OwnBook"] = EngineOptionCategory::Book;
    s_gikouCategories["BookFile"] = EngineOptionCategory::Book;
    s_gikouCategories["BookMaxPly"] = EngineOptionCategory::Book;
    s_gikouCategories["NarrowBook"] = EngineOptionCategory::Book;
    s_gikouCategories["TinyBook"] = EngineOptionCategory::Book;

    // 時間制御
    s_gikouCategories["MinThinkingTime"] = EngineOptionCategory::TimeControl;
    s_gikouCategories["ByoyomiMargin"] = EngineOptionCategory::TimeControl;
    s_gikouCategories["SuddenDeathMargin"] = EngineOptionCategory::TimeControl;
    s_gikouCategories["FischerMargin"] = EngineOptionCategory::TimeControl;

    // 対局ルール
    s_gikouCategories["ResignScore"] = EngineOptionCategory::GameRule;
    s_gikouCategories["DrawScore"] = EngineOptionCategory::GameRule;

    s_initialized = true;
}
