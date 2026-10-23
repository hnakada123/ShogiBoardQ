#include <QtTest>
#include <QItemSelectionModel>
#include <QSettings>
#include <QTableView>
#include <QTemporaryDir>

#include "analysisresultspanel.h"
#include "considerationpanel.h"
#include "thinkingtableview.h"
#include "kifuanalysislistmodel.h"
#include "numericrightaligncommadelegate.h"
#include "shogienginethinkingmodel.h"

class TestMemoryLifecycle : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_config.path());
    }

    void thinkingModelResetsDoNotAccumulateDelegates()
    {
        ShogiEngineThinkingModel first, second;
        ThinkingTableView view1(AnalysisSettings::ThinkingTable::Engine1);
        ThinkingTableView view2(AnalysisSettings::ThinkingTable::Engine2);
        view1.setThinkingModel(&first);
        view2.setThinkingModel(&second);
        for (int i = 0; i < 100; ++i) {
            first.clear();
            second.clear();
        }
        QCOMPARE(view1.findChildren<NumericRightAlignCommaDelegate*>().size(), 1);
        QCOMPARE(view2.findChildren<NumericRightAlignCommaDelegate*>().size(), 1);
    }

    void considerationSetupDoesNotAccumulateDelegates()
    {
        ShogiEngineThinkingModel model;
        ConsiderationPanel manager;
        for (int i = 0; i < 100; ++i)
            manager.setConsiderationThinkingModel(&model);
        QCOMPARE(manager.considerationView()->findChildren<NumericRightAlignCommaDelegate*>().size(), 1);
    }

    void thinkingModelSwitchReleasesSelectionModels()
    {
        ShogiEngineThinkingModel first, second;
        ThinkingTableView view(AnalysisSettings::ThinkingTable::Engine1);
        view.setThinkingModel(&first);
        for (int i = 0; i < 100; ++i) {
            view.setThinkingModel(i % 2 ? &first : &second);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        QCOMPARE(view.findChildren<QItemSelectionModel*>().size(), 1);
    }

    void considerationModelSwitchReleasesSelectionModels()
    {
        ShogiEngineThinkingModel first, second;
        ConsiderationPanel manager;
        for (int i = 0; i < 100; ++i) {
            manager.setConsiderationThinkingModel(i % 2 ? &first : &second);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        QCOMPARE(manager.considerationView()->findChildren<QItemSelectionModel*>().size(), 1);
    }

    void analysisModelSwitchReleasesSelectionModels()
    {
        KifuAnalysisListModel first, second;
        AnalysisResultsPanel presenter;
        QWidget* page = &presenter;
        for (int i = 0; i < 100; ++i) {
            presenter.setModel(i % 2 ? &first : &second);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        auto* view = page->findChild<QTableView*>();
        QVERIFY(view);
        QCOMPARE(view->findChildren<QItemSelectionModel*>().size(), 1);
    }
};

QTEST_MAIN(TestMemoryLifecycle)
#include "tst_memory_lifecycle.moc"
