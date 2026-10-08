#include <QtTest>
#include <QChart>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QLabel>
#include <QLineSeries>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include "evaluationchartwidget.h"
#include "evaluationchartview.h"
#include "analysissettings.h"
#include "settingscommon.h"

class TestEvaluationChart : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir m_config;
    bool m_accept = true;
    static EvaluationChartView* view(EvaluationChartWidget& widget)
    {
        return static_cast<EvaluationChartView*>(widget.chartViewWidget());
    }
    static QLineSeries* series(EvaluationChartWidget& widget, int side = 0)
    {
        for (auto* item : view(widget)->chart()->series()) {
            if (item->objectName() == (side == 0 ? QStringLiteral("evalSeries1") : QStringLiteral("evalSeries2")))
                return qobject_cast<QLineSeries*>(item);
        }
        return nullptr;
    }
    void editSettings()
    {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        dialog->findChild<QSpinBox*>(QStringLiteral("evalYLimit"))->setValue(32000);
        dialog->findChild<QSpinBox*>(QStringLiteral("evalYInterval"))->setValue(16000);
        dialog->findChild<QSpinBox*>(QStringLiteral("evalXLimit"))->setValue(120);
        dialog->findChild<QSpinBox*>(QStringLiteral("evalFontSize"))->setValue(11);
        dialog->findChild<QComboBox*>()->setCurrentIndex(1);
        dialog->resize(500, 380);
        if (m_accept) dialog->accept();
        else dialog->reject();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        QCoreApplication::setApplicationName(QStringLiteral("evaluationchart-test"));
        QCoreApplication::setOrganizationName(QStringLiteral("ShogiBoardQ-tests"));
    }
    void init() { SettingsCommon::openSettings().clear(); }

    void automaticRangeExcludesMate()
    {
        EvaluationChartWidget widget;
        widget.appendScoreP1(1, 120);
        widget.appendScoreP1(10, 450);
        widget.appendScoreP1(11, 31111, false, QStringLiteral("5"));
        QCOMPARE(widget.yAxisLimit(), 1000);
        QCOMPARE(widget.xAxisLimit(), 20);
        QCOMPARE(series(widget)->at(2).y(), 1000.0);
        QVERIFY(view(widget)->summary.contains(QStringLiteral("5")));
        QVERIFY(view(widget)->summary.contains(QStringLiteral("先手勝ち")));
        widget.appendScoreP1(21, 1800);
        QCOMPARE(widget.yAxisLimit(), 2000);
        QCOMPARE(widget.xAxisLimit(), 30);
        // 大きな通常評価値を勝手に詰みへ変換しない。
        widget.appendScoreP1(22, 31111);
        QVERIFY(widget.yAxisLimit() >= 31111);
        QVERIFY(!view(widget)->summary.contains(QStringLiteral("詰み")));
        widget.clearAll();
        QCOMPARE(widget.yAxisLimit(), 1000);
        QCOMPARE(widget.xAxisLimit(), 20);
    }

    void fixedRangePreservesRawScores()
    {
        EvaluationChartWidget widget;
        widget.setYAxisLimit(1000);
        widget.setXAxisLimit(20);
        widget.appendScoreP1(2, 4500);
        widget.appendScoreP2(3, 6500, true);
        QCOMPARE(widget.yAxisLimit(), 1000);
        QCOMPARE(series(widget)->at(0).y(), 1000.0);
        QCOMPARE(series(widget, 1)->at(0).y(), -1000.0);
        QVERIFY(view(widget)->summary.contains(QStringLiteral("-6500")));
        QVERIFY(view(widget)->summary.contains(QStringLiteral("表示範囲外")));
        widget.setYAxisLimit(10000);
        QCOMPARE(series(widget)->at(0).y(), 4500.0);
        QCOMPARE(series(widget, 1)->at(0).y(), -6500.0);
        widget.appendScoreP1(100, 0);
        QCOMPARE(widget.xAxisLimit(), 20);
    }

    void matePerspective_data()
    {
        QTest::addColumn<QString>("mate");
        QTest::addColumn<bool>("invert");
        QTest::addColumn<int>("direction");
        QTest::newRow("sente-mate") << QStringLiteral("5") << false << 1;
        QTest::newRow("gote-mate") << QStringLiteral("5") << true << -1;
        QTest::newRow("gote-mated") << QStringLiteral("-5") << true << 1;
        QTest::newRow("unknown-win") << QStringLiteral("+") << false << 1;
        QTest::newRow("unknown-loss") << QStringLiteral("-") << false << -1;
        QTest::newRow("mated-now") << QStringLiteral("0") << false << -1;
        QTest::newRow("gote-mated-now") << QStringLiteral("0") << true << 1;
    }
    void matePerspective()
    {
        QFETCH(QString, mate);
        QFETCH(bool, invert);
        QFETCH(int, direction);
        EvaluationChartWidget widget;
        widget.appendScoreP2(12, 31111, invert, mate);
        QCOMPARE(series(widget, 1)->at(0).y(), double(direction * widget.yAxisLimit()));
        QVERIFY(view(widget)->summary.contains(direction > 0 ? QStringLiteral("先手勝ち") : QStringLiteral("後手勝ち")));
        QVERIFY(view(widget)->markers.first().edge);
    }

    void resizeKeepsLabelsReadable()
    {
        EvaluationChartWidget widget;
        widget.setRecordLength(120);
        widget.resize(1200, 400);
        widget.show();
        QTest::qWait(30);
        const int wideInterval = widget.xAxisInterval();
        widget.resize(380, 300);
        QTest::qWait(30);
        QVERIFY(widget.xAxisInterval() > wideInterval);
        QCOMPARE(widget.xAxisLimit(), 120);
        const qreal tickSlots = double(widget.xAxisLimit()) / widget.xAxisInterval();
        QVERIFY(view(widget)->chart()->plotArea().width() / tickSlots >= 50);
    }

    void pendingScoresRespectUndoAndTrim()
    {
        EvaluationChartWidget widget;
        widget.appendScoreP1Buffered(1, 100);
        widget.appendScoreP1Buffered(2, 200);
        widget.appendScoreP2Buffered(3, 300);
        widget.trimToPly(1);
        widget.flushPendingScores();
        QCOMPARE(widget.countP1(), 1);
        QCOMPARE(widget.countP2(), 0);
        widget.appendScoreP1Buffered(2, 200);
        widget.removeLastP1();
        QCOMPARE(widget.countP1(), 1);
        widget.appendScoreP1Buffered(1, 500);
        widget.flushPendingScores();
        QCOMPARE(widget.countP1(), 1);
        QCOMPARE(series(widget)->at(0).y(), 500.0);
        widget.appendScoreP1Buffered(5, 100);
        widget.clearAll();
        widget.flushPendingScores();
        QCOMPARE(widget.countP1(), 0);
    }

    void pendingScoresPreserveNavigation()
    {
        EvaluationChartWidget widget;
        widget.appendScoreP1Buffered(4, 100);
        widget.setCurrentPly(5); // 次の局面を解析中。
        widget.flushPendingScores();
        QCOMPARE(widget.currentPly(), 5);
        QCOMPARE(widget.countP1(), 1);
        widget.appendScoreP1Buffered(5, 150);
        widget.setCurrentPly(1); // 解析直後にユーザーが過去の手へ移動。
        widget.flushPendingScores();
        QCOMPARE(widget.currentPly(), 1);
        QCOMPARE(widget.countP1(), 2);
    }

    void settingsPersistAndCancel()
    {
        {
            EvaluationChartWidget widget;
            widget.resize(1000, 420);
            widget.show();
            QTest::qWait(30);
            m_accept = false;
            QTimer::singleShot(0, this, &TestEvaluationChart::editSettings);
            widget.findChild<QPushButton*>(QStringLiteral("evalDisplaySettings"))->click();
            QCOMPARE(widget.yAxisLimit(), 1000);
            QCOMPARE(widget.labelFontSize(), 10);
            m_accept = true;
            QTimer::singleShot(0, this, &TestEvaluationChart::editSettings);
            widget.findChild<QPushButton*>(QStringLiteral("evalDisplaySettings"))->click();
            QCOMPARE(widget.yAxisLimit(), 32000);
            QCOMPARE(widget.yAxisInterval(), 16000);
        }
        EvaluationChartWidget restored;
        restored.resize(1000, 420);
        restored.show();
        QTest::qWait(30);
        QCOMPARE(restored.yAxisLimit(), 32000);
        QCOMPARE(restored.yAxisInterval(), 16000);
        QCOMPARE(restored.xAxisLimit(), 120);
        QCOMPARE(restored.labelFontSize(), 11);
        QCOMPARE(AnalysisSettings::evalChartSettingsSize(), QSize(500, 380));
        restored.findChild<QComboBox*>(QStringLiteral("evalRangeMode"))->setCurrentIndex(0);
        QCOMPARE(restored.yAxisLimit(), 1000);
    }

    void navigationAndHover()
    {
        EvaluationChartWidget widget;
        widget.resize(1000, 400);
        widget.appendScoreP1(5, 4500);
        widget.setYAxisLimit(1000);
        widget.show();
        QTest::qWait(30);
        auto* chartView = view(widget);
        QSignalSpy spy(&widget, &EvaluationChartWidget::plyClicked);
        // タイトル領域をクリックしても棋譜を移動しない。
        QTest::mouseClick(chartView->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
        QCOMPARE(spy.count(), 0);
        const QPoint point = chartView->mapFromScene(chartView->chart()->mapToScene(
            chartView->chart()->mapToPosition(QPointF(5, 500), series(widget))));
        QTest::mouseClick(chartView->viewport(), Qt::LeftButton, Qt::NoModifier, point);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toInt(), 5);
        QTest::mouseMove(chartView->viewport(), point);
        auto* tooltip = chartView->findChild<QLabel*>(QStringLiteral("evalTooltip"));
        QTRY_VERIFY(tooltip->isVisible());
        QVERIFY(tooltip->text().contains(QStringLiteral("4500")));
        widget.setCurrentPly(6);
        QVERIFY(chartView->summary.contains(QStringLiteral("評価値なし")));
    }

    void navigationButtons()
    {
        EvaluationChartWidget widget;
        widget.resize(1000, 400);
        widget.show();
        QTest::qWait(30);
        const QList<QPushButton*> buttons = {widget.firstButton(), widget.back10Button(), widget.prevButton(),
                                             widget.nextButton(), widget.fwd10Button(), widget.lastButton()};
        const QStringList tips = {QStringLiteral("最初に戻る"), QStringLiteral("10手戻る"), QStringLiteral("1手戻る"),
                                  QStringLiteral("1手進む"), QStringLiteral("10手進む"), QStringLiteral("最後に進む")};
        auto* settings = widget.findChild<QPushButton*>(QStringLiteral("evalDisplaySettings"));
        auto* range = widget.findChild<QComboBox*>(QStringLiteral("evalRangeMode"));
        QVERIFY(settings && range);
        const auto rect = [&widget](QWidget* child) { return QRect(child->mapTo(&widget, QPoint()), child->size()); };
        for (qsizetype i = 0; i < buttons.size(); ++i) {
            QVERIFY(buttons[i] && buttons[i]->isVisible());
            QCOMPARE(buttons[i]->toolTip(), tips[i]);
            if (i > 0) QVERIFY(rect(buttons[i - 1]).right() < rect(buttons[i]).left());
        }
        // 横長では表示範囲・表示設定と同じ行の中央に並べる。
        const QRect first = rect(buttons.first());
        const QRect last = rect(buttons.last());
        QVERIFY(qAbs(first.center().y() - rect(settings).center().y()) <= 1);
        QVERIFY(rect(range).right() < first.left());
        QVERIFY(last.right() < rect(settings).left());
        QVERIFY(qAbs((first.left() + last.right()) / 2 - widget.width() / 2) <= 2);
        const int wideChartTop = rect(widget.chartViewWidget()).top();

        // 幅が足りないときは2行目の中央に回し、どのボタンも欠けない。
        widget.resize(340, 300);
        QTest::qWait(30);
        QVERIFY(rect(buttons.first()).top() > rect(settings).bottom());
        QVERIFY(rect(buttons.first()).top() > rect(range).bottom());
        for (auto* button : buttons) QVERIFY(widget.rect().contains(rect(button)));
        QVERIFY(qAbs((rect(buttons.first()).left() + rect(buttons.last()).right()) / 2 - widget.width() / 2) <= 2);
        QVERIFY(rect(widget.chartViewWidget()).top() > rect(buttons.first()).bottom());
        widget.resize(1000, 400);
        QTest::qWait(30);
        QCOMPARE(rect(widget.chartViewWidget()).top(), wideChartTop);

        // 棋譜欄の矢印ボタンと同じく、対局中などは無効にできる。
        widget.setNavigationEnabled(false);
        for (auto* button : buttons) QVERIFY(!button->isEnabled());
        widget.setNavigationEnabled(true);
        for (auto* button : buttons) QVERIFY(button->isEnabled());
    }

    void analysisNavigationKeepsSourceLineUntilCleared()
    {
        EvaluationChartWidget widget;
        widget.resize(1000, 400);
        widget.appendScoreP1(3, 50);
        widget.setAnalysisLineIndex(2);
        widget.show();
        QTest::qWait(30);
        auto* chartView = view(widget);
        QSignalSpy normal(&widget, &EvaluationChartWidget::plyClicked);
        QSignalSpy analysis(&widget, &EvaluationChartWidget::analysisPlyClicked);
        const QPoint point = chartView->mapFromScene(chartView->chart()->mapToScene(
            chartView->chart()->mapToPosition(QPointF(3, 0), series(widget))));
        QTest::mouseClick(chartView->viewport(), Qt::LeftButton, Qt::NoModifier, point);
        QCOMPARE(normal.count(), 0);
        QCOMPARE(analysis.count(), 1);
        QCOMPARE(analysis.at(0).at(0).toInt(), 2);
        QCOMPARE(analysis.at(0).at(1).toInt(), 3);
        widget.clearAll();
        QTest::mouseClick(chartView->viewport(), Qt::LeftButton, Qt::NoModifier, point);
        QCOMPARE(normal.count(), 1);
        QCOMPARE(analysis.count(), 1);
    }

    void renderExamples()
    {
        const QString directory = qEnvironmentVariable("EVAL_CHART_SCREENSHOTS");
        EvaluationChartWidget widget;
        widget.resize(1100, 400);
        widget.setEngine1Name(QStringLiteral("Hayanagi 1.5.0"));
        widget.setEngine2Name(QStringLiteral("YaneuraOu"));
        const QList<int> scores{50, 90, 30, -100, -220, -120, 80, 350, 270, 500, 850, 600, 250, -80, -330, -570, -420, -800, -900, -750};
        for (qsizetype i = 0; i < scores.size(); ++i) {
            widget.appendScoreP1(static_cast<int>(i * 2 + 1), scores[i]);
            widget.appendScoreP2(static_cast<int>(i * 2 + 2), scores[i] - 100);
        }
        widget.appendScoreP2(42, -31111, false, QStringLiteral("-7"));
        widget.setCurrentPly(25);
        widget.show();
        QTest::qWait(30);
        QVERIFY(view(widget)->chart()->plotArea().height() > 150);
        const QPixmap graph = widget.chartViewWidget()->grab();
        QVERIFY(!graph.isNull());
        if (!directory.isEmpty()) {
            QDir().mkpath(directory);
            QVERIFY(widget.grab().save(directory + QStringLiteral("/evaluation-chart.png")));
            widget.setCurrentPly(42);
            QVERIFY(widget.grab().save(directory + QStringLiteral("/evaluation-chart-mate.png")));
            widget.resize(400, 300);
            QTest::qWait(30);
            QVERIFY(widget.grab().save(directory + QStringLiteral("/evaluation-chart-narrow.png")));
        }
    }
};
QTEST_MAIN(TestEvaluationChart)
#include "tst_evaluationchart.moc"
