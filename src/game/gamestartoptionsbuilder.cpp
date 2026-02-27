/// @file gamestartoptionsbuilder.cpp
/// @brief ダイアログ値解釈・PlayMode判定・開始局面プリセットの実装

#include "gamestartoptionsbuilder.h"
#include "logcategories.h"
#include "playernameservice.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "shogigamecontroller.h"
#include "startgamedialog.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QObject>
#include <QVariant>
#include <QWidget>
#include <array>

// ============================================================
// ダイアログ property 読み取りヘルパ
// ============================================================

int GameStartOptionsBuilder::readIntProperty(const QObject* root,
                                             const char* objectName,
                                             const char* prop,
                                             int def)
{
    if (!root) return def;
    if (auto obj = root->findChild<const QObject*>(QLatin1String(objectName))) {
        bool ok = false;
        const int n = obj->property(prop).toInt(&ok);
        if (ok) return n;
    }
    return def;
}

bool GameStartOptionsBuilder::readBoolProperty(const QObject* root,
                                               const char* objectName,
                                               const char* prop,
                                               bool def)
{
    if (!root) return def;
    if (auto obj = root->findChild<const QObject*>(QLatin1String(objectName))) {
        const QVariant v = obj->property(prop);
        if (v.isValid()) return v.toBool();
    }
    return def;
}

// ============================================================
// TimeControl 構築
// ============================================================

GameStartOptionsBuilder::TimeControl
GameStartOptionsBuilder::extractTimeControl(const QWidget* dlg)
{
    TimeControl tc;

    const int p1h  = readIntProperty (dlg, "p1HoursSpin");
    const int p1m  = readIntProperty (dlg, "p1MinutesSpin");
    const int p2h  = readIntProperty (dlg, "p2HoursSpin");
    const int p2m  = readIntProperty (dlg, "p2MinutesSpin");
    const int byo1 = readIntProperty (dlg, "byoyomiSec1");
    const int byo2 = readIntProperty (dlg, "byoyomiSec2");
    const int inc1 = readIntProperty (dlg, "addEachMoveSec1");
    const int inc2 = readIntProperty (dlg, "addEachMoveSec2");
    const bool limited = readBoolProperty(dlg, "limitedTimeCheck");

    auto toMs = [](int h, int m){ return (qMax(0,h)*3600 + qMax(0,m)*60) * 1000LL; };

    tc.p1.baseMs = toMs(p1h, p1m);
    tc.p2.baseMs = toMs(p2h, p2m);

    // byoyomi / increment は排他
    if (byo1 > 0 || byo2 > 0) {
        tc.p1.byoyomiMs   = qMax(0, byo1) * 1000LL;
        tc.p2.byoyomiMs   = qMax(0, byo2) * 1000LL;
        tc.p1.incrementMs = 0;
        tc.p2.incrementMs = 0;
    } else if (inc1 > 0 || inc2 > 0) {
        tc.p1.byoyomiMs = qMax(0, inc1) * 1000LL;
        tc.p2.byoyomiMs = qMax(0, inc2) * 1000LL;
        tc.p1.byoyomiMs = 0;
        tc.p2.byoyomiMs = 0;
    } else {
        tc.p1.byoyomiMs = 0;
        tc.p2.byoyomiMs = 0;
        tc.p1.incrementMs = 0;
        tc.p2.incrementMs = 0;
    }

    if (dlg) {
        tc.enabled = limited;
    } else {
        tc.enabled = (tc.p1.baseMs > 0 || tc.p2.baseMs > 0);
    }

    return tc;
}

GameStartOptionsBuilder::TimeControl
GameStartOptionsBuilder::buildTimeControl(QDialog* startDlg)
{
    TimeControl tc;

    int h1=0, m1=0, h2=0, m2=0;
    int byo1=0, byo2=0;
    int inc1=0, inc2=0;

    auto propInt = [&](const char* name)->int {
        if (!startDlg) return 0;
        bool ok=false; const int v = startDlg->property(name).toInt(&ok);
        return ok ? v : 0;
    };

    // StartGameDialog の型が使えるなら直接 getter を呼ぶ
    if (auto dlg = qobject_cast<StartGameDialog*>(startDlg)) {
        h1   = dlg->basicTimeHour1();
        m1   = dlg->basicTimeMinutes1();
        h2   = dlg->basicTimeHour2();
        m2   = dlg->basicTimeMinutes2();
        byo1 = dlg->byoyomiSec1();
        byo2 = dlg->byoyomiSec2();
        inc1 = dlg->addEachMoveSec1();
        inc2 = dlg->addEachMoveSec2();
    } else {
        // フォールバック: プロパティ名で取得
        h1   = propInt("basicTimeHour1");
        m1   = propInt("basicTimeMinutes1");
        h2   = propInt("basicTimeHour2");
        m2   = propInt("basicTimeMinutes2");
        byo1 = propInt("byoyomiSec1");
        byo2 = propInt("byoyomiSec2");
        inc1 = propInt("addEachMoveSec1");
        inc2 = propInt("addEachMoveSec2");
    }

    const qint64 base1Ms = qMax<qint64>(0, (static_cast<qint64>(h1)*3600 + m1*60) * 1000);
    const qint64 base2Ms = qMax<qint64>(0, (static_cast<qint64>(h2)*3600 + m2*60) * 1000);

    const qint64 byo1Ms  = qMax<qint64>(0, static_cast<qint64>(byo1) * 1000);
    const qint64 byo2Ms  = qMax<qint64>(0, static_cast<qint64>(byo2) * 1000);

    const qint64 inc1Ms  = qMax<qint64>(0, static_cast<qint64>(inc1) * 1000);
    const qint64 inc2Ms  = qMax<qint64>(0, static_cast<qint64>(inc2) * 1000);

    // byoyomi と increment は排他運用（byoyomi 優先）
    const bool useByoyomi = (byo1Ms > 0) || (byo2Ms > 0);

    tc.p1.baseMs      = base1Ms;
    tc.p2.baseMs      = base2Ms;
    tc.p1.byoyomiMs   = useByoyomi ? byo1Ms : 0;
    tc.p2.byoyomiMs   = useByoyomi ? byo2Ms : 0;
    tc.p1.incrementMs = useByoyomi ? 0      : inc1Ms;
    tc.p2.incrementMs = useByoyomi ? 0      : inc2Ms;

    tc.enabled = (tc.p1.baseMs > 0) || (tc.p2.baseMs > 0) ||
                 (tc.p1.byoyomiMs > 0) || (tc.p2.byoyomiMs > 0) ||
                 (tc.p1.incrementMs > 0) || (tc.p2.incrementMs > 0);

    qCDebug(lcGame).noquote()
        << "buildTimeControl:"
        << " enabled=" << tc.enabled
        << " P1{base=" << tc.p1.baseMs << " byo=" << tc.p1.byoyomiMs << " inc=" << tc.p1.incrementMs << "}"
        << " P2{base=" << tc.p2.baseMs << " byo=" << tc.p2.byoyomiMs << " inc=" << tc.p2.incrementMs << "}";

    return tc;
}

// ============================================================
// PlayMode 判定
// ============================================================

PlayMode GameStartOptionsBuilder::determinePlayMode(const int initPositionNumber,
                                                    const bool isPlayer1Human,
                                                    const bool isPlayer2Human)
{
    // 平手（=1）と駒落ち（!=1）で分岐
    const bool isEven = (initPositionNumber == 1);

    if (isEven) {
        if (isPlayer1Human && isPlayer2Human)  return PlayMode::HumanVsHuman;
        if (isPlayer1Human && !isPlayer2Human) return PlayMode::EvenHumanVsEngine;
        if (!isPlayer1Human && isPlayer2Human) return PlayMode::EvenEngineVsHuman;
        if (!isPlayer1Human && !isPlayer2Human) return PlayMode::EvenEngineVsEngine;
    } else {
        if (isPlayer1Human && isPlayer2Human)  return PlayMode::HumanVsHuman;
        if (isPlayer1Human && !isPlayer2Human) return PlayMode::HandicapHumanVsEngine;
        if (!isPlayer1Human && isPlayer2Human) return PlayMode::HandicapEngineVsHuman;
        if (!isPlayer1Human && !isPlayer2Human) return PlayMode::HandicapEngineVsEngine;
    }

    return PlayMode::PlayModeError;
}

PlayMode GameStartOptionsBuilder::determinePlayModeFromDialog(QDialog* startDlg)
{
    int  initPositionNumber = 1;
    bool isHuman1 = false, isHuman2 = false;
    bool isEngine1 = false, isEngine2 = false;

    if (startDlg) {
        if (auto dlg = qobject_cast<StartGameDialog*>(startDlg)) {
            initPositionNumber = dlg->startingPositionNumber();
            isHuman1           = dlg->isHuman1();
            isHuman2           = dlg->isHuman2();
            isEngine1          = dlg->isEngine1();
            isEngine2          = dlg->isEngine2();
        } else {
            bool ok=false;
            const QVariant pn = startDlg->property("startingPositionNumber");
            initPositionNumber = pn.isValid() ? pn.toInt(&ok) : 1;
            if (!ok) initPositionNumber = 1;

            isHuman1  = startDlg->property("isHuman1").toBool();
            isHuman2  = startDlg->property("isHuman2").toBool();
            isEngine1 = startDlg->property("isEngine1").toBool();
            isEngine2 = startDlg->property("isEngine2").toBool();
        }
    }

    // 「Human と Engine の排他」を整形
    const bool p1Human = (isHuman1  && !isEngine1);
    const bool p2Human = (isHuman2  && !isEngine2);

    const PlayMode mode = determinePlayMode(initPositionNumber, p1Human, p2Human);

    if (mode == PlayMode::PlayModeError) {
        qCWarning(lcGame).noquote() << "determinePlayModeFromDialog: PlayMode::PlayModeError"
                             << "initPos=" << initPositionNumber
                             << " p1Human=" << p1Human << " p2Human=" << p2Human
                             << " (raw human/engine: "
                             << isHuman1 << "/" << isEngine1 << ", "
                             << isHuman2 << "/" << isEngine2 << ")";
    }

    return mode;
}

// ============================================================
// SFEN手番抽出
// ============================================================

QChar GameStartOptionsBuilder::turnFromSfen(const QString& sfen)
{
    const QString s = sfen.trimmed();
    if (s.isEmpty()) return QChar();

    // 典型: "<board> b - 1" / "<board> w - 1"
    const QStringList toks = s.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (toks.size() >= 2) {
        const QString t = toks.at(1).toLower();
        if (t == QLatin1String("b")) return QLatin1Char('b');
        if (t == QLatin1String("w")) return QLatin1Char('w');
    }
    if (s.contains(QLatin1String(" b "))) return QLatin1Char('b');
    if (s.contains(QLatin1String(" w "))) return QLatin1Char('w');
    return QChar();
}

// ============================================================
// PlayMode判定（SFEN手番整合版）
// ============================================================

PlayMode GameStartOptionsBuilder::determinePlayModeAlignedWithTurn(
    int initPositionNumber, bool isPlayer1Human, bool isPlayer2Human, const QString& startSfen)
{
    const QChar turn = turnFromSfen(startSfen);
    const bool isEven = (initPositionNumber == 1);
    const bool hvh = (isPlayer1Human && isPlayer2Human);
    const bool eve = (!isPlayer1Human && !isPlayer2Human);
    const bool oneVsEngine = !hvh && !eve;

    if (isEven) {
        if (hvh) return PlayMode::HumanVsHuman;
        if (eve) return PlayMode::EvenEngineVsEngine;
        if (oneVsEngine) {
            if (turn == QLatin1Char('b')) {
                return isPlayer1Human ? PlayMode::EvenHumanVsEngine : PlayMode::EvenEngineVsHuman;
            }
            if (turn == QLatin1Char('w')) {
                return isPlayer2Human ? PlayMode::EvenEngineVsHuman : PlayMode::EvenHumanVsEngine;
            }
            // turnが取れない場合は座席で決定
            return (isPlayer1Human && !isPlayer2Human) ? PlayMode::EvenHumanVsEngine : PlayMode::EvenEngineVsHuman;
        }
        return PlayMode::NotStarted;
    } else {
        if (hvh) return PlayMode::HumanVsHuman;
        if (eve) return PlayMode::HandicapEngineVsEngine;
        if (oneVsEngine) {
            if (turn == QLatin1Char('b')) {
                return isPlayer1Human ? PlayMode::HandicapHumanVsEngine : PlayMode::HandicapEngineVsHuman;
            }
            if (turn == QLatin1Char('w')) {
                return isPlayer2Human ? PlayMode::HandicapEngineVsHuman : PlayMode::HandicapHumanVsEngine;
            }
            return (isPlayer1Human && !isPlayer2Human) ? PlayMode::HandicapHumanVsEngine : PlayMode::HandicapEngineVsHuman;
        }
        return PlayMode::NotStarted;
    }
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
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
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
        return QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    }
    return t;
}

// ============================================================
// ユーティリティ
// ============================================================

void GameStartOptionsBuilder::applyPlayersNamesForMode(ShogiView* view,
                                                       PlayMode mode,
                                                       const QString& human1,
                                                       const QString& human2,
                                                       const QString& engine1,
                                                       const QString& engine2)
{
    if (!view) return;
    const PlayerNameMapping names =
        PlayerNameService::computePlayers(mode, human1, human2, engine1, engine2);
    view->setBlackPlayerName(names.p1);
    view->setWhitePlayerName(names.p2);
}

void GameStartOptionsBuilder::applyResumePositionIfAny(ShogiGameController* gc,
                                                       ShogiView* view,
                                                       const QString& resumeSfen)
{
    if (!gc || resumeSfen.isEmpty()) return;

    if (auto* b = gc->board()) {
        b->setSfen(resumeSfen);
        if (view) view->applyBoardAndRender(b);
    }
}
