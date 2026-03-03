/// @file tst_sfen_collection.cpp
/// @brief 局面集ビューア (SfenCollectionDialog) テスト

// private メンバへのアクセスを許可するテスト用ハック
#define private public
#define protected public
#include "sfencollectiondialog.h"
#undef private
#undef protected

#include <QtTest>
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
        QVERIFY(!loadTextViaFile(dlg, QString()));
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
