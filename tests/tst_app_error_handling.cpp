/// @file tst_app_error_handling.cpp
/// @brief app層の異常系・防御パターンの構造的契約テスト
///
/// 初期化失敗、依存未生成、二重実行防止の3パターンについて
/// ソースコード解析で構造的契約を検証する。
/// 加えて SFEN パース等の純粋関数に対する異常系ユニットテストも含む。
/// 既存の tst_app_lifecycle_pipeline / tst_wiring_contracts と同パターン。

#include <QtTest>

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include "shogiboard.h"
#include "shogiposition.h"

#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined by CMake"
#endif

class TestAppErrorHandling : public QObject
{
    Q_OBJECT

private:
    /// ソースファイルの全テキストを返す
    static QString readSourceFile(const QString& relPath)
    {
        QFile file(QStringLiteral(SOURCE_DIR "/") + relPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QTextStream(&file).readAll();
    }

    /// ソースファイルの各行リストを返す
    static QStringList readSourceLines(const QString& relPath)
    {
        QFile file(QStringLiteral(SOURCE_DIR "/") + relPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        QStringList lines;
        QTextStream in(&file);
        while (!in.atEnd())
            lines << in.readLine();
        return lines;
    }

    /// 関数本体の行範囲（開始行, 終了行）を返す。見つからなければ (-1,-1)
    static QPair<int, int> findFunctionBody(const QStringList& lines, const QString& signature)
    {
        const int sz = int(lines.size());

        int sigLine = -1;
        for (int i = 0; i < sz; ++i) {
            if (lines[i].contains(signature)) {
                sigLine = i;
                break;
            }
        }
        if (sigLine < 0)
            return {-1, -1};

        int braceStart = -1;
        for (int i = sigLine; i < sz; ++i) {
            if (lines[i].contains(QLatin1Char('{'))) {
                braceStart = i;
                break;
            }
        }
        if (braceStart < 0)
            return {-1, -1};

        int depth = 0;
        for (int i = braceStart; i < sz; ++i) {
            for (const QChar& ch : lines[i]) {
                if (ch == QLatin1Char('{'))
                    ++depth;
                else if (ch == QLatin1Char('}'))
                    --depth;
            }
            if (depth == 0)
                return {braceStart, i};
        }
        return {-1, -1};
    }

    /// 指定範囲の行を結合して返す
    static QString bodyText(const QStringList& lines, int startLine, int endLine)
    {
        QStringList body;
        for (int i = startLine; i <= endLine && i < int(lines.size()); ++i)
            body << lines[i];
        return body.join(QLatin1Char('\n'));
    }

private slots:
    // ================================================================
    // 4. 二重実行防止パターン
    // ================================================================

    /// 終了処理（MainWindowComposer::shutdown）が二重実行で安全で、司令塔の有無を確かめること（構造確認）
    void shutdownDoubleCallProtectionPattern()
    {
        const QStringList lines = readSourceLines(QStringLiteral("src/app/mainwindowcomposer.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read composer source");

        const auto range = findFunctionBody(lines, QStringLiteral("MainWindowComposer::shutdown()"));
        QVERIFY2(range.first >= 0, "shutdown() function not found");

        const QString body = bodyText(lines, range.first, range.second);
        QVERIFY2(body.contains(QStringLiteral("if (m_shutdownDone) return;")),
                 "shutdown must return on the second call");
        QVERIFY2(body.contains(QStringLiteral("if (m_c.engines)")),
                 "shutdown should check the engine seats before destroyEngines()");

        const QString header = readSourceFile(QStringLiteral("src/app/mainwindowcomposer.h"));
        QVERIFY2(header.contains(QStringLiteral("bool m_shutdownDone = false;")),
                 "m_shutdownDone should be initialized to false in header");
    }

    // ================================================================
    // 6. SFEN パース異常系ユニットテスト
    // ================================================================

    /// parseSfen が空文字列を拒否すること
    void sfenParseRejectsEmptyString()
    {
        const auto result = Position::fromSfen(QString());
        QVERIFY2(!result.has_value(), "parseSfen should reject empty string");
    }

    /// parseSfen がフィールド数不足の文字列を拒否すること
    void sfenParseRejectsInsufficientParts()
    {
        // 3フィールド（手数なし）
        const auto r1 = Position::fromSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -"));
        QVERIFY2(!r1.has_value(), "parseSfen should reject 3-part SFEN");

        // 1フィールドのみ
        const auto r2 = Position::fromSfen(QStringLiteral("lnsgkgsnl"));
        QVERIFY2(!r2.has_value(), "parseSfen should reject 1-part SFEN");

        // 5フィールド（余分）
        const auto r3 = Position::fromSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1 extra"));
        QVERIFY2(!r3.has_value(), "parseSfen should reject 5-part SFEN");
    }

    /// parseSfen が不正な手番文字を拒否すること
    void sfenParseRejectsInvalidTurn()
    {
        // "x" は "b" でも "w" でもない
        const auto r1 = Position::fromSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL x - 1"));
        QVERIFY2(!r1.has_value(), "parseSfen should reject invalid turn 'x'");

        // 空手番
        const auto r2 = Position::fromSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL  - 1"));
        QVERIFY2(!r2.has_value(), "parseSfen should reject empty turn field");
    }

    /// parseSfen が不正な手数を拒否すること
    void sfenParseRejectsInvalidMoveNumber()
    {
        // 手数 0
        const auto r1 = Position::fromSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 0"));
        QVERIFY2(!r1.has_value(), "parseSfen should reject move number 0");

        // 手数が負数
        const auto r2 = Position::fromSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - -5"));
        QVERIFY2(!r2.has_value(), "parseSfen should reject negative move number");

        // 手数が非数値
        const auto r3 = Position::fromSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - abc"));
        QVERIFY2(!r3.has_value(), "parseSfen should reject non-numeric move number");
    }

    /// setSfen が不正な入力でクラッシュしないこと
    void sfenSetSfenHandlesCorruptInput()
    {
        ShogiBoard board;

        // 正常な初期配置を設定してから不正入力を渡す
        const QString validSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        board.setSfen(validSfen);
        QCOMPARE(board.currentPlayer(), Turn::Black);

        // 空文字列 → 盤面は変わらない（クラッシュしない）
        board.setSfen(QString());
        QCOMPARE(board.currentPlayer(), Turn::Black);

        // バイナリっぽいゴミデータ → クラッシュしない
        board.setSfen(QStringLiteral("\x00\x01\x02\x03"));
        QCOMPARE(board.currentPlayer(), Turn::Black);

        // 不正な駒文字を含む → クラッシュしない
        board.setSfen(QStringLiteral("XXXXXXXXX/XXXXXXXXX/XXXXXXXXX/XXXXXXXXX/XXXXXXXXX/XXXXXXXXX/XXXXXXXXX/XXXXXXXXX/XXXXXXXXX b - 1"));
        QCOMPARE(board.currentPlayer(), Turn::Black);

        // ランク数不足 → クラッシュしない
        board.setSfen(QStringLiteral("9/9/9 b - 1"));
        QCOMPARE(board.currentPlayer(), Turn::Black);

        // 不正な駒台文字列 → クラッシュしない
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b ZZZ 1"));
        QCOMPARE(board.currentPlayer(), Turn::Black);
    }

    /// parseSfen が正常な入力を正しくパースすること（正常系の対照テスト）
    void sfenParseAcceptsValidInput()
    {
        // 標準初期局面
        const auto r1 = Position::fromSfen(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        QVERIFY2(r1.has_value(), "parseSfen should accept valid initial SFEN");
        QCOMPARE(r1->sideToMove(), Turn::Black);
        QCOMPARE(r1->moveNumber(), 1);
        QCOMPARE(r1->handsSfen(), QStringLiteral("-"));

        // 後手番・持ち駒あり
        const auto r2 = Position::fromSfen(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w 2P 10"));
        QVERIFY2(r2.has_value(), "parseSfen should accept valid SFEN with pieces in hand");
        QCOMPARE(r2->sideToMove(), Turn::White);
        QCOMPARE(r2->moveNumber(), 10);
        QCOMPARE(r2->handsSfen(), QStringLiteral("2P"));
    }

    // ================================================================
    // 8. CSA クライアント 状態ガード契約テスト（ソースコード構造検証）
    // ================================================================

    /// CsaClient の状態依存メソッドが不正状態で早期リターンすること
    void csaClientStateGuardsExist()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/network/csaclient.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read csaclient source");

        // 各メソッドが状態チェックガードを持つこと
        struct MethodGuard {
            QString signature;
            QString expectedStateCheck;
        };

        const QList<MethodGuard> guards = {
            {QStringLiteral("CsaClient::connectToServer("),
             QStringLiteral("Disconnected")},
            {QStringLiteral("CsaClient::agree("),
             QStringLiteral("GameReady")},
            {QStringLiteral("CsaClient::sendMove("),
             QStringLiteral("InGame")},
            {QStringLiteral("CsaClient::reject("),
             QStringLiteral("GameReady")},
        };

        for (const auto& guard : std::as_const(guards)) {
            const auto range = findFunctionBody(lines, guard.signature);
            QVERIFY2(range.first >= 0,
                      qPrintable(QStringLiteral("Method '%1' not found").arg(guard.signature)));

            const QString body = bodyText(lines, range.first, range.second);

            // 状態チェック (if (m_connectionState != State) { ... return; }) が存在すること
            QVERIFY2(body.contains(QStringLiteral("m_connectionState"))
                         && body.contains(guard.expectedStateCheck),
                      qPrintable(QStringLiteral("Method '%1' lacks state guard for %2")
                                     .arg(guard.signature, guard.expectedStateCheck)));

            // errorOccurred シグナルまたは早期リターンがあること
            QVERIFY2(body.contains(QStringLiteral("errorOccurred"))
                         || body.contains(QStringLiteral("return")),
                      qPrintable(QStringLiteral("Method '%1' lacks error signal or early return")
                                     .arg(guard.signature)));
        }
    }

    // ================================================================
    // 12. CSA ダイアログの機密情報保存ポリシー
    // ================================================================

    /// CsaGameDialog がパスワードを設定ファイルへ保存しないこと
    void csaGameDialogDoesNotPersistPassword()
    {
        const QString body = readSourceFile(QStringLiteral("src/dialogs/csagamedialog.cpp"));
        QVERIFY2(!body.isEmpty(), "Failed to read csagamedialog source");

        QVERIFY2(!body.contains(QStringLiteral("setValue(\"password\"")),
                 "CsaGameDialog must not persist password in QSettings");
        QVERIFY2(!body.contains(QStringLiteral("value(\"password\"")),
                 "CsaGameDialog must not read password from QSettings");
        QVERIFY2(body.contains(QStringLiteral("lineEditPassword->clear()")),
                 "CsaGameDialog should clear password field on restore/history apply");
    }
};

QTEST_MAIN(TestAppErrorHandling)

#include "tst_app_error_handling.moc"
