/// @file tst_sfen_collection.cpp
/// @brief 局面集ビューア (SfenCollectionDialog) テスト

// private メンバへは SfenCollectionDialog の friend 宣言（SHOGIBOARDQ_TESTING 時のみ有効）経由でアクセスする
#include "sfencollectiondialog.h"

#include <QtTest>
#include <QAbstractButton>
#include <QApplication>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTextStream>

class TestSfenCollection : public QObject
{
    Q_OBJECT

private:
    /// テスト用 SFEN テキスト（3局面）
    static QString validSfenText()
    {
        return QStringLiteral(
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1\n"
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2\n"
            "lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3\n");
    }

    /// テスト用一時ファイルにテキストを書き出して loadFromFile する
    bool loadTextViaFile(SfenCollectionDialog& dlg, const QString& text)
    {
        QTemporaryFile tmp;
        tmp.setAutoRemove(true);
        if (!tmp.open()) return false;
        QTextStream out(&tmp);
        out << text;
        out.flush();
        tmp.close();
        return dlg.loadFromFile(tmp.fileName());
    }

    /// loadFromFile が出すエラーのメッセージボックスを記録して閉じる
    QString m_messageText;
    void closeMessageBox()
    {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box) return;
        m_messageText = box->text();
        box->buttons().constFirst()->click();
    }

private slots:
    // ── パーステスト ──────────────────────────────────────

    void parseSfenLines_validSfen_parsesCorrectCount()
    {
        SfenCollectionDialog dlg;
        dlg.parseSfenLines(validSfenText());
        QCOMPARE(dlg.m_sfenList.size(), 3);
    }

    void parseSfenLines_emptyLines_skipped()
    {
        SfenCollectionDialog dlg;
        QString text = QStringLiteral(
            "\n"
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1\n"
            "\n"
            "  \n"
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2\n"
            "\n");
        dlg.parseSfenLines(text);
        QCOMPARE(dlg.m_sfenList.size(), 2);
    }

    void parseSfenLines_singleLine_parsesOne()
    {
        SfenCollectionDialog dlg;
        dlg.parseSfenLines(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        QCOMPARE(dlg.m_sfenList.size(), 1);
    }

    void parseSfenLines_empty_noPositions()
    {
        SfenCollectionDialog dlg;
        dlg.parseSfenLines(QString());
        QCOMPARE(dlg.m_sfenList.size(), 0);
    }

    void parseSfenLines_sfenPrefix_stripped()
    {
        SfenCollectionDialog dlg;
        dlg.parseSfenLines(
            QStringLiteral("sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        QCOMPARE(dlg.m_sfenList.size(), 1);
        QVERIFY(dlg.m_sfenList.first().startsWith(QStringLiteral("lnsgkgsnl")));
    }

    void parseSfenLines_positionSfenPrefix_stripped()
    {
        SfenCollectionDialog dlg;
        dlg.parseSfenLines(QStringLiteral(
            "position sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        QCOMPARE(dlg.m_sfenList.size(), 1);
        QVERIFY(dlg.m_sfenList.first().startsWith(QStringLiteral("lnsgkgsnl")));
    }

    void parseSfenLines_invalidLine_skipped()
    {
        SfenCollectionDialog dlg;
        // 4パート未満（3ワード以下）のためスキップされる
        QString text = QStringLiteral(
            "not valid\n"
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1\n"
            "only three parts\n");
        dlg.parseSfenLines(text);
        QCOMPARE(dlg.m_sfenList.size(), 1);
    }

    void parseSfenLines_usiPositionCommands_applyMoves()
    {
        SfenCollectionDialog dlg;
        // startpos と USI の position コマンドは moves を指した後の局面にする
        const int skipped = dlg.parseSfenLines(QStringLiteral(
            "startpos\n"
            "position startpos moves 7g7f 3c3d\n"
            "position sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2 moves 3c3d\n"));
        QCOMPARE(skipped, 0);
        QCOMPARE(dlg.m_sfenList, QList<QString>({
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
            QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"),
            QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"),
        }));
    }

    void parseSfenLines_sfenWithSolution_keepsProblemPosition()
    {
        SfenCollectionDialog dlg;
        // 詰将棋局面生成が手順を含めて保存した行（SFEN の後に moves と解答手順）は SFEN の局面にする
        const int skipped = dlg.parseSfenLines(QStringLiteral(
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1 moves 7g7f 3c3d\n"
            "sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1 moves 2g2f\n"));
        QCOMPARE(skipped, 0);
        QCOMPARE(dlg.m_sfenList, QList<QString>({
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
        }));
    }

    void parseSfenLines_unreadablePositions_skippedAndCounted()
    {
        SfenCollectionDialog dlg;
        // 4語以上でも局面として読めない行（駒の記号・手番の誤り、指せない手）は数えて飛ばす
        const int skipped = dlg.parseSfenLines(QStringLiteral(
            "abc def ghi jkl\n"
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL x - 1\n"
            "position startpos moves 7g7e\n"
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1\n"));
        QCOMPARE(skipped, 3);
        QCOMPARE(dlg.m_sfenList.size(), 1);
    }

    void parseSfenLines_commentLines_skipped()
    {
        SfenCollectionDialog dlg;
        // 詰将棋局面生成のファイル保存が先頭に付けるコメント行（'#' 始まり）は
        // 4パート以上あっても局面として扱わない
        QString text = QStringLiteral(
            "# ShogiBoardQ 2026.09.25 詰将棋局面生成\n"
            "# 生成日時: 2026/09/25 08:21:54\n"
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1\n"
            "  # 目標手数: 3 手詰\n"
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2\n");
        dlg.parseSfenLines(text);
        QCOMPARE(dlg.m_sfenList.size(), 2);
        QVERIFY(dlg.m_sfenList.at(0).startsWith(QStringLiteral("lnsgkgsnl/")));
    }

    // ── ファイルロードテスト ──────────────────────────────

    void loadFromFile_validFile_loadsPositions()
    {
        SfenCollectionDialog dlg;
        QVERIFY(loadTextViaFile(dlg, validSfenText()));
        QCOMPARE(dlg.m_sfenList.size(), 3);
        QCOMPARE(dlg.m_currentIndex, 0);
    }

    void loadFromFile_emptyFile_returnsFalse()
    {
        SfenCollectionDialog dlg;
        m_messageText.clear();
        QTimer::singleShot(0, this, &TestSfenCollection::closeMessageBox);
        QVERIFY(!loadTextViaFile(dlg, QString()));
        QVERIFY(m_messageText.contains(QStringLiteral("局面がありません")));
    }

    void loadFromFile_noPositions_keepsCurrentCollection()
    {
        SfenCollectionDialog dlg;
        QVERIFY(loadTextViaFile(dlg, validSfenText()));
        dlg.onGoForward();
        const QString fileText = dlg.m_fileLabel->text();

        // 局面が1つもないファイルは、知らせたうえで表示中の局面集を残す
        m_messageText.clear();
        QTimer::singleShot(0, this, &TestSfenCollection::closeMessageBox);
        QVERIFY(!loadTextViaFile(dlg, QStringLiteral("hello\nworld\n")));
        QVERIFY(!m_messageText.isEmpty());
        QCOMPARE(dlg.m_sfenList.size(), 3);
        QCOMPARE(dlg.m_currentIndex, 1);
        QCOMPARE(dlg.m_fileLabel->text(), fileText);
        QVERIFY(dlg.m_btnSelect->isEnabled());
    }

    void loadFromFile_skippedLines_shownWithFileName()
    {
        SfenCollectionDialog dlg;
        QVERIFY(loadTextViaFile(dlg, validSfenText() + QStringLiteral("abc def ghi jkl\n")));
        QCOMPARE(dlg.m_sfenList.size(), 3);
        QVERIFY(dlg.m_fileLabel->text().contains(QStringLiteral("読めない 1 行を飛ばしました")));

        QVERIFY(loadTextViaFile(dlg, validSfenText()));
        QVERIFY(!dlg.m_fileLabel->text().contains(QStringLiteral("飛ばしました")));
    }

    // ── ナビゲーションテスト ─────────────────────────────

    void navigation_goLast_selectsLast()
    {
        SfenCollectionDialog dlg;
        dlg.parseSfenLines(validSfenText());
        dlg.m_currentIndex = 0;
        dlg.onGoLast();
        QCOMPARE(dlg.m_currentIndex, 2);
    }

    void navigation_goFirst_selectsFirst()
    {
        SfenCollectionDialog dlg;
        dlg.parseSfenLines(validSfenText());
        dlg.m_currentIndex = 2;
        dlg.onGoFirst();
        QCOMPARE(dlg.m_currentIndex, 0);
    }

    void navigation_goForward_advances()
    {
        SfenCollectionDialog dlg;
        dlg.parseSfenLines(validSfenText());
        dlg.m_currentIndex = 0;
        dlg.onGoForward();
        QCOMPARE(dlg.m_currentIndex, 1);
    }

    void navigation_goBack_retreats()
    {
        SfenCollectionDialog dlg;
        dlg.parseSfenLines(validSfenText());
        dlg.m_currentIndex = 2;
        dlg.onGoBack();
        QCOMPARE(dlg.m_currentIndex, 1);
    }

    void navigation_goForward_atEnd_staysAtEnd()
    {
        SfenCollectionDialog dlg;
        dlg.parseSfenLines(validSfenText());
        dlg.m_currentIndex = 2;
        dlg.onGoForward();
        QCOMPARE(dlg.m_currentIndex, 2);
    }

    void navigation_goBack_atStart_staysAtStart()
    {
        SfenCollectionDialog dlg;
        dlg.parseSfenLines(validSfenText());
        dlg.m_currentIndex = 0;
        dlg.onGoBack();
        QCOMPARE(dlg.m_currentIndex, 0);
    }

    // ── シグナルテスト ───────────────────────────────────

    void positionSelected_emitsCorrectSfen()
    {
        SfenCollectionDialog dlg;
        dlg.parseSfenLines(validSfenText());
        dlg.m_currentIndex = 1;

        QSignalSpy spy(&dlg, &SfenCollectionDialog::positionSelected);
        dlg.onSelectClicked();

        QCOMPARE(spy.count(), 1);
        QString emitted = spy.first().first().toString();
        QVERIFY(emitted.contains(QStringLiteral("2P6")));
    }

    void positionSelected_emptyList_noEmit()
    {
        SfenCollectionDialog dlg;

        QSignalSpy spy(&dlg, &SfenCollectionDialog::positionSelected);
        dlg.onSelectClicked();

        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestSfenCollection)
#include "tst_sfen_collection.moc"
