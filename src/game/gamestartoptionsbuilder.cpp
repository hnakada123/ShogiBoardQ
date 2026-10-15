/// @file gamestartoptionsbuilder.cpp
/// @brief 対局開始ダイアログの値の解釈（時間・対局者）と開始局面プリセットの実装

#include "gamestartoptionsbuilder.h"

#include <array>

#include "logcategories.h"
#include "sfenutils.h"

// ============================================================
// TimeControl 構築
// ============================================================

GameStartOptionsBuilder::TimeSettings
GameStartOptionsBuilder::buildTimeControl(const StartGameDialogData& data)
{
    const auto minutesToMs = [](int hours, int minutes) {
        return qMax<qint64>(0, (static_cast<qint64>(hours) * 3600 + minutes * 60) * 1000);
    };
    const auto secondsToMs = [](int seconds) { return qMax<qint64>(0, static_cast<qint64>(seconds) * 1000); };

    TimeControl rules;
    rules.black.mainMs = minutesToMs(data.basicTimeHour1, data.basicTimeMinutes1);
    rules.white.mainMs = minutesToMs(data.basicTimeHour2, data.basicTimeMinutes2);
    rules.black.byoyomiMs = secondsToMs(data.byoyomiSec1);
    rules.white.byoyomiMs = secondsToMs(data.byoyomiSec2);
    rules.black.incrementMs = secondsToMs(data.addEachMoveSec1);
    rules.white.incrementMs = secondsToMs(data.addEachMoveSec2);

    TimeSettings tc;
    tc.loseOnTimeout = data.isLoseOnTimeout;
    tc.rules = rules.normalized();
    tc.enabled = !tc.rules.isUnlimited();

    qCDebug(lcGame).noquote()
        << "buildTimeControl:"
        << " enabled=" << tc.enabled
        << " P1{base=" << tc.rules.black.mainMs << " byo=" << tc.rules.black.byoyomiMs << " inc=" << tc.rules.black.incrementMs << "}"
        << " P2{base=" << tc.rules.white.mainMs << " byo=" << tc.rules.white.byoyomiMs << " inc=" << tc.rules.white.incrementMs << "}";

    return tc;
}

// ============================================================
// 対局者
// ============================================================

MatchPlayers GameStartOptionsBuilder::playersFromDialog(const StartGameDialogData& data)
{
    MatchPlayers players;
    // 人間とエンジンの両方に印があるときは人間とする（ダイアログは排他だが、設定の読み違いに備える）
    players.blackIsEngine = data.isEngine1 && !data.isHuman1;
    players.whiteIsEngine = data.isEngine2 && !data.isHuman2;
    players.blackHumanName = data.humanName1;
    players.whiteHumanName = data.humanName2;
    const auto& engines = data.engineList;
    if (data.engineNumber1 >= 0 && data.engineNumber1 < engines.size()) players.blackEngineName = data.engineName1;
    if (data.engineNumber2 >= 0 && data.engineNumber2 < engines.size()) players.whiteEngineName = data.engineName2;
    return players;
}

// ============================================================
// 開始局面プリセット
// ============================================================

const QString& GameStartOptionsBuilder::startingPositionSfen(int index)
{
    // 純SFENテーブル（1=平手, 2=香落ち, …, 14=十枚落ち）
    static const auto& kPresets = *[]() {
        static const std::array<QString, 14> arr = {{
            //  1: 平手
            SfenUtils::hirateSfen(),
            //  2: 香落ち
            QStringLiteral("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
            //  3: 右香落ち
            QStringLiteral("1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
            //  4: 角落ち
            QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
            //  5: 飛車落ち
            QStringLiteral("lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
            //  6: 飛香落ち
            QStringLiteral("lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
            //  7: 二枚落ち
            QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
            //  8: 三枚落ち
            QStringLiteral("lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
            //  9: 四枚落ち
            QStringLiteral("1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
            // 10: 五枚落ち
            QStringLiteral("2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
            // 11: 左五枚落ち
            QStringLiteral("1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
            // 12: 六枚落ち
            QStringLiteral("2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
            // 13: 八枚落ち
            QStringLiteral("3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
            // 14: 十枚落ち
            QStringLiteral("4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1")
        }};
        return &arr;
    }();

    const int idx = qBound(1, index, kStartingPositionCount) - 1;
    return kPresets[static_cast<size_t>(idx)];
}

// ============================================================
// SFEN 正規化
// ============================================================

QString GameStartOptionsBuilder::canonicalizeSfen(const QString& sfen)
{
    QString t = sfen.trimmed();
    if (t.isEmpty() || t == QLatin1String("startpos")) {
        return SfenUtils::hirateSfen();
    }
    return t;
}

// ============================================================
// 開始の設定
// ============================================================

MatchStartOptions GameStartOptionsBuilder::buildStartOptions(const QString& startSfen, const StartGameDialogData& dlg)
{
    MatchStartOptions opt;
    opt.players = playersFromDialog(dlg);
    opt.sfenStart = startSfen;
    const auto& engines = dlg.engineList;
    if (dlg.engineNumber1 >= 0 && dlg.engineNumber1 < engines.size()) opt.enginePath1 = engines.at(dlg.engineNumber1).path;
    if (dlg.engineNumber2 >= 0 && dlg.engineNumber2 < engines.size()) opt.enginePath2 = engines.at(dlg.engineNumber2).path;
    opt.maxMoves = dlg.maxMoves;
    opt.autoSaveKifu = dlg.autoSaveKifu;
    opt.kifuSaveDir = dlg.kifuSaveDir;
    return opt;
}

bool GameStartOptionsBuilder::needsHumanAtBottomFlip(const StartGameDialogData& dlg, Turn bottomSide)
{
    // 人間とエンジンの対局だけが対象（人間同士・エンジン同士はそのまま）
    if (!dlg.isShowHumanInFront || dlg.isHuman1 == dlg.isHuman2) return false;
    const Turn humanSide = dlg.isHuman1 ? Turn::Black : Turn::White;
    return humanSide != bottomSide;
}
