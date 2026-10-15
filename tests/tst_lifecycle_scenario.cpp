/// @brief 本体のサービスで、開始・着手・終局・保存・リセット・再開始を検証する。
#include <QtTest>
#include <QTemporaryDir>
#include "mainwindow.h"
#include "mainwindowcomposer.h"
#include "sessionlifecyclecoordinator.h"
#include "gamerecordupdateservice.h"
#include "gamerecordmodel.h"
#include "kifubranchtree.h"
#include "kifurecordlistmodel.h"
#include "livegamesession.h"
#include "sfenpositiontracer.h"
#include "sfenutils.h"
#include "appsettings.h"

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
        auto* service = parts.gameRecordUpdateService.get();
        auto* session = parts.liveGameSession;
        auto* tree = parts.branchTree;
        auto* rows = parts.kifuRecord;
        auto* model = parts.gameRecord;
        QVERIFY(service);
        QVERIFY(session);
        QVERIFY(model);
        parts.sessionLifecycle->resetToInitialState();
        const QString initial = SfenUtils::hirateSfen();
        const QStringList usi{"7g7f", "3c3d"};
        const auto moves = SfenPositionTracer::buildGameMoves(initial, usi);
        const auto positions = SfenPositionTracer::buildSfenRecord(initial, usi, false);
        for (int game = 0; game < 2; ++game) {
            parts.sessionLifecycle->performPreStartCleanup(
                {GameStartPreparation::Mode::NewGame, initial, -1});
            QVERIFY(session->canAddMove());
            QSignalSpy inserted(rows, &QAbstractItemModel::rowsInserted);
            QSignalSpy changed(tree, &KifuBranchTree::treeChanged);
            for (int i = 0; i < 2; ++i) {
                RecordEntry entry;
                entry.move = moves.at(i);
                entry.displayText = i == 0 ? QStringLiteral("▲７六歩(77)") : QStringLiteral("△３四歩(33)");
                entry.sfen = positions.at(i + 1);
                entry.mover = i == 0 ? Turn::Black : Turn::White;
                entry.time = RecordTime::fromMilliseconds(1250, 1250);
                service->appendRecord(entry);
                QCOMPARE(session->totalPly(), i + 1);
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
            const auto kif = model->toKifLines({}).join('\n');
            const auto csa = model->toCsaLines({}, usi).join('\n');
            const auto jkf = model->toJkfLines({}).join('\n');
            QVERIFY(kif.contains(QStringLiteral("７六歩")));
            QVERIFY(kif.contains(QStringLiteral("投了")));
            QVERIFY(csa.contains("+7776FU"));
            QVERIFY(csa.contains("%TORYO"));
            QVERIFY(csa.contains("T1.250"));
            QVERIFY(jkf.contains("TORYO"));
            model->clearDirty();
            parts.sessionLifecycle->resetToInitialState();
            QCOMPARE(rows->rowCount(), 1);
            QCOMPARE(tree->nodeCount(), 1);
            QCOMPARE(parts.gameRecordUpdateService.get(), service);
        }
        model->clearDirty();
    }
    void recordCannotStartSession()
    {
        MainWindow window;
        auto& parts = window.composer().parts();
        parts.sessionLifecycle->resetToInitialState();
        parts.gameRecord->clearDirty();
        parts.gameRecordUpdateService->appendRecord(
            RecordEntry::ending(TerminalType::Resign, Turn::Black));
        QVERIFY(!parts.liveGameSession->isActive());
        QVERIFY(!parts.gameRecord->isDirty());
        QCOMPARE(parts.branchTree->nodeCount(), 1);
        QCOMPARE(parts.kifuRecord->rowCount(), 1);
    }
};
QTEST_MAIN(TestLifecycleScenario)
#include "tst_lifecycle_scenario.moc"
