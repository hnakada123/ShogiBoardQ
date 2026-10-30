/// @brief 本体のサービスで、開始・着手・終局・保存・リセット・再開始を検証する。
#include <QtTest>
#include <QTemporaryDir>
#include "mainwindow.h"
#include "mainwindowactions.h"
#include "mainwindowcomposer.h"
#include "gamesession.h"
#include "josekibooksession.h"
#include "josekiwindow.h"
#include "josekiwindowcontroller.h"
#include <QCheckBox>
#include "kifudocument.h"
#include "kifuexport.h"
#include "kifubranchtree.h"
#include "kifurecordlistmodel.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
#include "sessionresetcoordinator.h"
#include "sfenutils.h"
#include "appsettings.h"
#include "test_replay.h"

class TestLifecycleScenario : public QObject {
    Q_OBJECT
    QTemporaryDir m_config;
private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        AppSettings::pieceSoundEnabled.set(false);
    }
    void recordSaveResetAndRestart()
    {
        MainWindow window;
        auto& parts = window.composer().parts();
        auto* service = parts.gameSession;
        auto* session = parts.liveGameSession;
        auto* tree = parts.branchTree;
        auto* rows = parts.kifuRecord;
        auto* document = parts.document;
        QVERIFY(service);
        QVERIFY(session);
        QVERIFY(document);
        parts.actions->startNewRecord();
        const QString initial = SfenUtils::hirateSfen();
        const QStringList usi{"7g7f", "3c3d"};
        const auto moves = TestReplay::buildGameMoves(initial, usi);
        const auto positions = TestReplay::buildSfenRecord(initial, usi, false);
        for (int game = 0; game < 2; ++game) {
            QVERIFY(parts.sessionReset->prepareGameStart({GameStartPreparation::Mode::NewGame, initial, -1}));
            QVERIFY(session->canAddMove());
            QSignalSpy inserted(rows, &QAbstractItemModel::rowsInserted);
            QSignalSpy changed(tree, &KifuBranchTree::nodeAppended);
            for (int i = 0; i < 2; ++i) {
                RecordEntry entry;
                entry.move = moves.at(i);
                entry.displayText = i == 0 ? QStringLiteral("▲７六歩(77)") : QStringLiteral("△３四歩(33)");
                entry.sfen = positions.at(i + 1);
                entry.mover = i == 0 ? Turn::Black : Turn::White;
                entry.time = RecordTime::fromMilliseconds(1250, 1250);
                service->appendRecord(entry);
                QCOMPARE(session->liveNode()->ply(), i + 1);
                QCOMPARE(parts.navState->currentNode(), session->liveNode());
                QCOMPARE(rows->rowCount(), i + 2);
                QCOMPARE(session->liveNode()->sfen(), positions.at(i + 1));
            }
            service->appendRecord(RecordEntry::ending(TerminalType::Resign, Turn::Black));
            QCOMPARE(rows->rowCount(), 4);
            QCOMPARE(inserted.count(), 3);
            QCOMPARE(changed.count(), 3);
            QVERIFY(!session->canAddMove());
            QCOMPARE(session->liveNode()->terminalType(), TerminalType::Resign);
            QCOMPARE(session->liveNode()->sfen(), positions.last());
            service->appendRecord(RecordEntry::ending(TerminalType::Interrupt, Turn::Black));
            QCOMPARE(rows->rowCount(), 4);
            QVERIFY(session->commit());
            QVERIFY(!session->isActive());
            const auto kif = KifuExport::lines(*document, KifuFormat::Kif).join('\n');
            const auto csa = KifuExport::lines(*document, KifuFormat::Csa).join('\n');
            const auto jkf = KifuExport::lines(*document, KifuFormat::Jkf).join('\n');
            QVERIFY(kif.contains(QStringLiteral("７六歩")));
            QVERIFY(kif.contains(QStringLiteral("投了")));
            QVERIFY(csa.contains("+7776FU"));
            QVERIFY(csa.contains("%TORYO"));
            QVERIFY(csa.contains("T1.250"));
            QVERIFY(jkf.contains("TORYO"));
            document->markSaved(QString());
            parts.actions->startNewRecord();
            QCOMPARE(rows->rowCount(), 1);
            QCOMPARE(tree->nodeCount(), 1);
            QCOMPARE(parts.gameSession, service);
        }
        document->markSaved(QString());
    }
    /// 定跡ウィンドウの「盤の手を登録」がオンなら、対局で指された手（両者とも）も定跡に登録する
    void gameMovesAreRecordedToJoseki()
    {
        MainWindow window;
        auto& parts = window.composer().parts();
        parts.actions->startNewRecord();
        auto* recordMoves = parts.joseki->josekiWindow()->findChild<QCheckBox*>(QStringLiteral("josekiRecordBoardMoves"));
        QVERIFY(recordMoves);
        const QString initial = SfenUtils::hirateSfen();
        const QStringList usi{"7g7f", "3c3d"};
        const auto moves = TestReplay::buildGameMoves(initial, usi);
        const auto positions = TestReplay::buildSfenRecord(initial, usi, false);
        QSignalSpy recorded(parts.gameSession, &GameSession::gameMoveRecorded);
        const auto playGame = [&]() {
            QVERIFY(parts.sessionReset->prepareGameStart({GameStartPreparation::Mode::NewGame, initial, -1}));
            for (int i = 0; i < usi.size(); ++i) {
                RecordEntry entry;
                entry.move = moves.at(i);
                entry.displayText = i == 0 ? QStringLiteral("▲７六歩(77)") : QStringLiteral("△３四歩(33)");
                entry.sfen = positions.at(i + 1);
                entry.mover = i == 0 ? Turn::Black : Turn::White;
                parts.gameSession->appendRecord(entry);
            }
            parts.gameSession->appendRecord(RecordEntry::ending(TerminalType::Resign, Turn::Black));
        };

        // オフ（起動したとき）なら登録しない。手は指す前の局面と一緒に知らせる（終局の行は知らせない）
        playGame();
        QCOMPARE(recorded.count(), 2);
        QCOMPARE(recorded.at(0).at(0).toString(), positions.at(0));
        QCOMPARE(recorded.at(1).at(1).toString(), QStringLiteral("3c3d"));
        QVERIFY(parts.josekiBook->isEmpty());

        parts.document->markSaved(QString());
        parts.actions->startNewRecord();
        recordMoves->setChecked(true);
        playGame();
        QCOMPARE(parts.josekiBook->movesAt(positions.at(0)).size(), 1);
        QCOMPARE(parts.josekiBook->movesAt(positions.at(1)).size(), 1);
        QCOMPARE(parts.josekiBook->movesAt(positions.at(0)).constFirst().move, QStringLiteral("7g7f"));
        QCOMPARE(parts.josekiBook->movesAt(positions.at(1)).constFirst().move, QStringLiteral("3c3d"));
        QVERIFY(parts.josekiBook->isModified());
        parts.josekiBook->setModified(false);
        parts.document->markSaved(QString());
    }

    void recordCannotStartSession()
    {
        MainWindow window;
        auto& parts = window.composer().parts();
        parts.actions->startNewRecord();
        parts.document->markSaved(QString());
        parts.gameSession->appendRecord(
            RecordEntry::ending(TerminalType::Resign, Turn::Black));
        QVERIFY(!parts.liveGameSession->isActive());
        QVERIFY(!parts.document->isModified());
        QCOMPARE(parts.branchTree->nodeCount(), 1);
        QCOMPARE(parts.kifuRecord->rowCount(), 1);
    }
};
QTEST_MAIN(TestLifecycleScenario)
#include "tst_lifecycle_scenario.moc"
