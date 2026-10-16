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

namespace {
/// エンジンの score mate の表記（"5"・"-5"・"+"・"-"・"0"）から、手番側から見た評価値を作る
UsiScore mateScore(const QString& text)
{
    if (text == QLatin1String("+") || text == QLatin1String("-")) {
        UsiScore score;
        score.kind = UsiScore::Kind::Mate;
        score.mateWin = text == QLatin1String("+");
        return score;
    }
    return UsiScore::mate(text.toInt());
}
} // namespace

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
        widget.appendScoreP1(1, UsiScore::cp(120));
        widget.appendScoreP1(10, UsiScore::cp(450));
        widget.appendScoreP1(11, UsiScore::mate(5));
        QCOMPARE(widget.yAxisLimit(), 1000);
        QCOMPARE(widget.xAxisLimit(), 20);
        QCOMPARE(series(widget)->at(2).y(), 1000.0);
        QVERIFY(view(widget)->summary.contains(QStringLiteral("5")));
        QVERIFY(view(widget)->summary.contains(QStringLiteral("先手勝ち")));
        widget.appendScoreP1(21, UsiScore::cp(1800));
        QCOMPARE(widget.yAxisLimit(), 2000);
        QCOMPARE(widget.xAxisLimit(), 30);
        // 大きな通常評価値を勝手に詰みへ変換しない。
        widget.appendScoreP1(22, UsiScore::cp(31111));
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
        widget.appendScoreP1(2, UsiScore::cp(4500));
        widget.appendScoreP2(3, UsiScore::cp(-6500));
        QCOMPARE(widget.yAxisLimit(), 1000);
        QCOMPARE(series(widget)->at(0).y(), 1000.0);
        QCOMPARE(series(widget, 1)->at(0).y(), -1000.0);
        QVERIFY(view(widget)->summary.contains(QStringLiteral("-6500")));
        QVERIFY(view(widget)->summary.contains(QStringLiteral("表示範囲外")));
        widget.setYAxisLimit(10000);
        QCOMPARE(series(widget)->at(0).y(), 4500.0);
        QCOMPARE(series(widget, 1)->at(0).y(), -6500.0);
        widget.appendScoreP1(100, UsiScore::cp(0));
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
        const UsiScore score = mateScore(mate);
        widget.appendScoreP2(12, invert ? score.negated() : score);
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
        widget.appendScoreP1Buffered(1, UsiScore::cp(100));
        widget.appendScoreP1Buffered(2, UsiScore::cp(200));
        widget.appendScoreP2Buffered(3, UsiScore::cp(300));
        widget.trimToPly(1);
        widget.flushPendingScores();
        QCOMPARE(widget.countP1(), 1);
        QCOMPARE(widget.countP2(), 0);
        widget.appendScoreP1Buffered(2, UsiScore::cp(200));
        widget.removeLastP1();
        QCOMPARE(widget.countP1(), 1);
        widget.appendScoreP1Buffered(1, UsiScore::cp(500));
        widget.flushPendingScores();
        QCOMPARE(widget.countP1(), 1);
        QCOMPARE(series(widget)->at(0).y(), 500.0);
        widget.appendScoreP1Buffered(5, UsiScore::cp(100));
        widget.clearAll();
        widget.flushPendingScores();
        QCOMPARE(widget.countP1(), 0);
    }

    void pendingScoresPreserveNavigation()
    {
        EvaluationChartWidget widget;
        widget.appendScoreP1Buffered(4, UsiScore::cp(100));
        widget.setCurrentPly(5); // 次の局面を解析中。
        widget.flushPendingScores();
        QCOMPARE(widget.currentPly(), 5);
        QCOMPARE(widget.countP1(), 1);
        widget.appendScoreP1Buffered(5, UsiScore::cp(150));
        widget.setCurrentPly(1); // 解析直後にユーザーが過去の手へ移動。
        widget.flushPendingScores();
        QCOMPARE(widget.currentPly(), 1);
        QCOMPARE(widget.countP1(), 2);
    }

    // 棋譜の現在行が動くたび（対局中は1手ごとに2回）に、点と表示範囲が変わらなければ作り直さず、
    // 現在の手の線と表示だけを更新する。結果は作り直した場合と同じ
    void currentPlyMoveKeepsSeries()
    {
        const auto fill = [](EvaluationChartWidget& widget) {
            widget.resize(900, 360);
            for (int ply = 1; ply <= 14; ++ply) widget.appendScoreP1(ply, UsiScore::cp(ply * 173 - 900));
            widget.appendScoreP2(15, mateScore(QStringLiteral("+")));
        };
        EvaluationChartWidget widget;
        fill(widget);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        QSignalSpy replaced(series(widget), &QXYSeries::pointsReplaced);
        QSignalSpy xAxis(&widget, &EvaluationChartWidget::xAxisSettingsChanged);

        widget.setRecordLength(15);
        widget.setCurrentPly(6);
        widget.setRecordLength(15);
        widget.setCurrentPly(9);
        QCOMPARE(replaced.count(), 0);
        QCOMPARE(xAxis.count(), 0);
        QCOMPARE(widget.currentPly(), 9);

        // 作り直した場合（新しく作って同じ手へ移動）と、点・現在の手の印・表示・線が同じ
        EvaluationChartWidget rebuilt;
        fill(rebuilt);
        rebuilt.trimToPly(15);
        rebuilt.setCurrentPly(9);
        QCOMPARE(view(widget)->summary, view(rebuilt)->summary);
        QCOMPARE(view(widget)->currentPly, view(rebuilt)->currentPly);
        QCOMPARE(view(widget)->markers.size(), view(rebuilt)->markers.size());
        for (qsizetype i = 0; i < view(widget)->markers.size(); ++i) {
            QCOMPARE(view(widget)->markers.at(i).value, view(rebuilt)->markers.at(i).value);
            QCOMPARE(view(widget)->markers.at(i).selected, view(rebuilt)->markers.at(i).selected);
        }
        const auto lines = [](EvaluationChartWidget& chart) {
            QList<QList<QPointF>> points;
            for (auto* item : view(chart)->chart()->series()) {
                if (auto* line = qobject_cast<QLineSeries*>(item))
                    points.append(line->points() << QPointF(line->isVisible(), 0));
            }
            return points;
        };
        QCOMPARE(lines(widget), lines(rebuilt));

        // 点を足したときは作り直す。記録が伸びただけ（対局中の棋譜欄の行追加）のときは、
        // 横の表示範囲は広げるが、点は変わらないので系列は作り直さない
        widget.appendScoreP1(16, UsiScore::cp(50));
        QCOMPARE(replaced.count(), 1);
        QCOMPARE(series(widget)->count(), 15);
        widget.setCurrentPly(16);
        QCOMPARE(replaced.count(), 1);
        widget.setRecordLength(25);
        QCOMPARE(replaced.count(), 1);
        QCOMPARE(widget.xAxisLimit(), 30);
        QCOMPARE(series(widget)->count(), 15);
        // 点を収める縦の表示範囲が変わるときは、同じ点でも表示する値が変わるので作り直す
        widget.setAutomaticRange(false);
        widget.setYAxisLimit(500);
        QVERIFY(replaced.count() >= 2);
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
        widget.appendScoreP1(5, UsiScore::cp(4500));
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
        widget.appendScoreP1(3, UsiScore::cp(50));
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
            widget.appendScoreP1(static_cast<int>(i * 2 + 1), UsiScore::cp(scores[i]));
            widget.appendScoreP2(static_cast<int>(i * 2 + 2), UsiScore::cp(scores[i] - 100));
        }
        widget.appendScoreP2(42, UsiScore::mate(-7));
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
