#include <QtTest>
#include <QItemSelectionModel>
#include <QSettings>
#include <QTableView>
#include <QTemporaryDir>

#include "analysisresultspresenter.h"
#include "considerationtabmanager.h"
#include "engineanalysispresenter.h"
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
        QTableView view1, view2;
        EngineAnalysisPresenter presenter;
        presenter.setViews(&view1, &view2, nullptr, nullptr);
        presenter.setModels(&first, &second);
        for (int i = 0; i < 100; ++i) {
            first.clearAllItems();
            second.clearAllItems();
        }
        QCOMPARE(view1.findChildren<NumericRightAlignCommaDelegate*>().size(), 1);
        QCOMPARE(view2.findChildren<NumericRightAlignCommaDelegate*>().size(), 1);
    }

    void considerationSetupDoesNotAccumulateDelegates()
    {
        ShogiEngineThinkingModel model;
        QWidget page;
        ConsiderationTabManager manager;
        manager.buildConsiderationUi(&page);
        for (int i = 0; i < 100; ++i)
            manager.setConsiderationThinkingModel(&model);
        QCOMPARE(manager.considerationView()->findChildren<NumericRightAlignCommaDelegate*>().size(), 1);
    }

    void thinkingModelSwitchReleasesSelectionModels()
    {
        ShogiEngineThinkingModel first, second;
        QTableView view;
        EngineAnalysisPresenter presenter;
        presenter.setViews(&view, nullptr, nullptr, nullptr);
        presenter.setModels(&first, nullptr);
        for (int i = 0; i < 100; ++i) {
            presenter.setEngine1ThinkingModel(i % 2 ? &first : &second);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        QCOMPARE(view.findChildren<QItemSelectionModel*>().size(), 1);
    }

    void considerationModelSwitchReleasesSelectionModels()
    {
        ShogiEngineThinkingModel first, second;
        QWidget page;
        ConsiderationTabManager manager;
        manager.buildConsiderationUi(&page);
        for (int i = 0; i < 100; ++i) {
            manager.setConsiderationThinkingModel(i % 2 ? &first : &second);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        QCOMPARE(manager.considerationView()->findChildren<QItemSelectionModel*>().size(), 1);
    }

    void analysisModelSwitchReleasesSelectionModels()
    {
        KifuAnalysisListModel first, second;
        AnalysisResultsPresenter presenter;
        std::unique_ptr<QWidget> page(presenter.containerWidget());
        for (int i = 0; i < 100; ++i) {
            presenter.showWithModel(i % 2 ? &first : &second);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        auto* view = page->findChild<QTableView*>();
        QVERIFY(view);
        QCOMPARE(view->findChildren<QItemSelectionModel*>().size(), 1);
    }
};

QTEST_MAIN(TestMemoryLifecycle)
#include "tst_memory_lifecycle.moc"
