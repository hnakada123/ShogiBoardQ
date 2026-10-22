/// @file tst_joseki_registration.cpp
/// @brief 思考の表・棋譜解析の結果から定跡に登録する操作（JosekiRegistrationController）のテスト

#include <QtTest>
#include <QAction>
#include <QMenu>

#include "josekibooksession.h"
#include "josekiregistrationcontroller.h"
#include "kifuanalysislistmodel.h"
#include "kifuanalysisrow.h"
#include "sfenreplay.h"
#include "sfenutils.h"
#include "shogienginethinkingmodel.h"
#include "thinkinginforow.h"

namespace {

KifuAnalysisRow analysisRow(int ply, const QString &sfen, const QString &usiPv, const UsiScore &blackScore, int depth)
{
    KifuAnalysisRow row;
    row.ply = ply;
    row.sfen = sfen;
    row.usiPv = usiPv;
    row.evaluation = blackScore;
    row.depth = depth;
    return row;
}

} // namespace

class TestJosekiRegistration : public QObject
{
    Q_OBJECT

private:
    static QString hirate() { return SfenUtils::hirateSfen(); }
    static QString afterBlack() { return SfenReplay::positionAfter(hirate(), QStringLiteral("7g7f")); }

    /// 0手目（先手番）・1手目（後手番）・2手目（定跡で読み筋なし）の解析結果
    static void fillAnalysis(KifuAnalysisListModel &model)
    {
        model.appendRow(analysisRow(0, hirate(), QStringLiteral("7g7f 3c3d"), UsiScore::cp(50), 12));
        model.appendRow(analysisRow(1, afterBlack(), QStringLiteral("3c3d 2g2f"), UsiScore::cp(80), 11));
        model.appendRow(analysisRow(2, SfenReplay::positionAfter(afterBlack(), QStringLiteral("3c3d")), QString(),
                                    UsiScore{}, -1));
    }

    bool m_canRegister = true;
    bool canRegister() const { return m_canRegister; }

    JosekiRegistrationController::Deps deps(JosekiBookSession *book, const KifuAnalysisListModel *model)
    {
        JosekiRegistrationController::Deps d;
        d.book = book;
        d.canRegister = std::bind(&TestJosekiRegistration::canRegister, this);
        d.analysisModel = model;
        return d;
    }

private slots:
    void init() { m_canRegister = true; }

    /// 結果の表の行は先手から見た評価値なので、後手番の局面では符号を直して登録する
    void analysisRows_registerWithSideToMoveValue()
    {
        KifuAnalysisListModel model;
        fillAnalysis(model);
        JosekiBookSession book;
        JosekiRegistrationController controller(deps(&book, &model));

        const JosekiRegistrationController::BatchResult result = controller.registerAnalysisRows(0, 2, true, true);
        QCOMPARE(result.added, 2);
        QCOMPARE(result.updated, 0);
        const JosekiMove black = book.movesAt(hirate()).constFirst();
        QCOMPARE(black.move, QStringLiteral("7g7f"));
        QCOMPARE(black.nextMove, QStringLiteral("3c3d"));
        QCOMPARE(black.value, 50);
        QCOMPARE(black.depth, 12);
        const JosekiMove white = book.movesAt(afterBlack()).constFirst();
        QCOMPARE(white.move, QStringLiteral("3c3d"));
        QCOMPARE(white.nextMove, QStringLiteral("2g2f"));
        QCOMPARE(white.value, -80);
        QCOMPARE(white.depth, 11);

        // もう一度登録すると書き換えになる
        const JosekiRegistrationController::BatchResult again = controller.registerAnalysisRows(0, 2, true, true);
        QCOMPARE(again.added, 0);
        QCOMPARE(again.updated, 2);
    }

    /// 手数の範囲と手番で絞る。登録できないとき（定跡ウィンドウの読み込み待ち・読み書き中）は何もしない
    void analysisRows_filterByRangeAndSide()
    {
        KifuAnalysisListModel model;
        fillAnalysis(model);
        JosekiBookSession book;
        JosekiRegistrationController controller(deps(&book, &model));

        QCOMPARE(controller.registerAnalysisRows(1, 2, true, false).added, 0);
        QCOMPARE(controller.registerAnalysisRows(0, 0, false, true).added, 0);
        QVERIFY(book.isEmpty());

        m_canRegister = false;
        QCOMPARE(controller.registerAnalysisRows(0, 2, true, true).added, 0);
        QVERIFY(book.isEmpty());
        m_canRegister = true;
        QCOMPARE(controller.registerAnalysisRows(1, 2, true, true).added, 1);
        QVERIFY(book.hasPosition(afterBlack()));
        QVERIFY(!book.hasPosition(hirate()));
    }

    /// 思考の表の右クリック: メニューを開いた行の読み筋を登録する（行が入れ替わっても開いたときの値）
    void thinkingRowMenu_registersFirstMove()
    {
        ShogiEngineThinkingModel thinking;
        ThinkingInfoRow info;
        info.depth = 24;
        info.score = UsiScore::cp(-30);
        info.usiPv = QStringLiteral("3c3d 2g2f");
        info.baseSfen = afterBlack();
        thinking.appendRow(info);
        JosekiBookSession book;
        JosekiRegistrationController controller(deps(&book, nullptr));

        QMenu menu;
        controller.onThinkingRowContextMenu(&thinking, 0, &menu);
        auto *action = menu.findChild<QAction *>(QStringLiteral("josekiRegisterPv"));
        QVERIFY(action);
        QVERIFY(action->isEnabled());
        QVERIFY2(action->text().contains(QStringLiteral("△３四歩")), qPrintable(action->text()));
        thinking.clear();
        action->trigger();
        const JosekiMove move = book.movesAt(afterBlack()).constFirst();
        QCOMPARE(move.move, QStringLiteral("3c3d"));
        QCOMPARE(move.nextMove, QStringLiteral("2g2f"));
        QCOMPARE(move.value, -30);
        QCOMPARE(move.depth, 24);

        // 登録できないとき・読み筋が無い行は押せない
        m_canRegister = false;
        thinking.appendRow(info);
        QMenu disabledMenu;
        controller.onThinkingRowContextMenu(&thinking, 0, &disabledMenu);
        QVERIFY(!disabledMenu.findChild<QAction *>(QStringLiteral("josekiRegisterPv"))->isEnabled());
        m_canRegister = true;
        info.usiPv.clear();
        thinking.appendRow(info);
        QMenu noPvMenu;
        controller.onThinkingRowContextMenu(&thinking, thinking.rowCount() - 1, &noPvMenu);
        QVERIFY(!noPvMenu.findChild<QAction *>(QStringLiteral("josekiRegisterPv"))->isEnabled());
    }

    /// 棋譜解析の結果の表の右クリックには、行の登録とまとめての登録を出す
    void analysisRowMenu_hasBothActions()
    {
        KifuAnalysisListModel model;
        fillAnalysis(model);
        JosekiBookSession book;
        JosekiRegistrationController controller(deps(&book, &model));
        QMenu menu;
        controller.onAnalysisRowContextMenu(1, &menu);
        QVERIFY(menu.findChild<QAction *>(QStringLiteral("josekiRegisterPv"))->isEnabled());
        QVERIFY(menu.findChild<QAction *>(QStringLiteral("josekiRegisterAnalysis"))->isEnabled());
        menu.findChild<QAction *>(QStringLiteral("josekiRegisterPv"))->trigger();
        QCOMPARE(book.movesAt(afterBlack()).constFirst().value, -80);

        // 読み筋の無い行（定跡）は、行の登録だけ押せない
        QMenu bookMenu;
        controller.onAnalysisRowContextMenu(2, &bookMenu);
        QVERIFY(!bookMenu.findChild<QAction *>(QStringLiteral("josekiRegisterPv"))->isEnabled());
        QVERIFY(bookMenu.findChild<QAction *>(QStringLiteral("josekiRegisterAnalysis"))->isEnabled());
    }
};

QTEST_MAIN(TestJosekiRegistration)
#include "tst_joseki_registration.moc"
