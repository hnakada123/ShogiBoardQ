#ifndef KIFU_TEST_HELPER_H
#define KIFU_TEST_HELPER_H

#include <QtTest>
#include <QByteArray>
#include <QTemporaryFile>
#include <QDir>

namespace KifuTestHelper {

// QTest データ行に境界値入力パターンを追加する。
// 呼び出し前に QTest::addColumn<QByteArray>("fileContent") が必要。
inline void addBoundaryInputRows()
{
    // ---- 空入力・ホワイトスペース ----
    QTest::newRow("empty_content") << QByteArray();
    QTest::newRow("whitespace_spaces") << QByteArray("            ");
    QTest::newRow("whitespace_tabs") << QByteArray("\t\t\t\t");
    QTest::newRow("whitespace_mixed") << QByteArray("   \t  \n  \t  \n");

    // ---- BOM ----
    QTest::newRow("utf8_bom_only") << QByteArray("\xEF\xBB\xBF");
    QTest::newRow("utf8_bom_with_text") << QByteArray("\xEF\xBB\xBF" "content after BOM\n");
    QTest::newRow("utf16le_bom") << QByteArray("\xFF\xFE", 2);
    QTest::newRow("utf16be_bom") << QByteArray("\xFE\xFF", 2);

    // ---- 制御文字 ----
    QTest::newRow("control_chars_low")
        << QByteArray("\x01\x02\x03\x04\x05\x06\x07\x08\x0E\x0F");
    QTest::newRow("control_chars_high")
        << QByteArray("\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F");
    QTest::newRow("null_in_text") << QByteArray("abc\x00" "def\x00" "ghi", 11);
    QTest::newRow("escape_sequences") << QByteArray("\x1B[31mcolored\x1B[0m\n");

    // ---- 改行バリエーション ----
    QTest::newRow("cr_only") << QByteArray("\r\r\r\r");
    QTest::newRow("crlf_lines") << QByteArray("line1\r\nline2\r\nline3\r\n");
    QTest::newRow("mixed_line_endings") << QByteArray("line1\r\nline2\rline3\n");

    // ---- 長大行 (10KB以上) ----
    QTest::newRow("long_line_15k") << QByteArray(15000, 'X');
    QTest::newRow("long_line_with_utf8")
        << (QByteArray(5000, 'A') + QByteArray("日本語テスト") + QByteArray(5000, 'B'));

    // ---- バイナリデータ ----
    QTest::newRow("png_header") << QByteArray("\x89PNG\r\n\x1A\n\x00\x00\x00\rIHDR", 16);
    QTest::newRow("zip_header") << QByteArray("PK\x03\x04\x14\x00\x00\x00\x08\x00", 10);

    // ---- フォーマット混合（他形式のデータ） ----
    QTest::newRow("kif_format_data")
        << QByteArray("手合割：平手\n手数----指手---------消費時間--\n"
                      "   1 ７六歩(77)   ( 0:01/00:00:01)\n");
    QTest::newRow("csa_format_data")
        << QByteArray("PI\n+\n+7776FU\n-3334FU\n%TORYO\n");
    QTest::newRow("usi_format_data")
        << QByteArray("position startpos moves 7g7f 3c3d\n");
    QTest::newRow("json_format_data")
        << QByteArray(R"({"moves": [{}, {"move": {"to": {"x": 7, "y": 6}}}]})");
    QTest::newRow("usen_format_data")
        << QByteArray("~0.7ku2jm6y236e5t24be9qc\n");

    // ---- 大量行 ----
    {
        QByteArray manyLines;
        manyLines.reserve(3000);
        for (int i = 0; i < 500; ++i)
            manyLines.append("line\n");
        QTest::newRow("500_short_lines") << manyLines;
    }
}

// KIF形式: 大量手数の棋譜生成
inline QByteArray generateLongKifContent(int moveCount)
{
    QByteArray content = "手合割：平手\n手数----指手---------消費時間--\n";
    for (int i = 1; i <= moveCount; ++i) {
        content.append("   ");
        content.append(QByteArray::number(i));
        content.append(" ７六歩(77)   ( 0:01/00:00:01)\n");
    }
    return content;
}

// KI2形式: 大量手数の棋譜生成
inline QByteArray generateLongKi2Content(int moveCount)
{
    QByteArray content = "手合割：平手\n";
    for (int i = 0; i < moveCount; ++i) {
        content.append(i % 2 == 0 ? "▲７六歩　" : "△３四歩　");
        if ((i + 1) % 6 == 0)
            content.append("\n");
    }
    content.append("\n");
    return content;
}

// CSA形式: 大量手数の棋譜生成
inline QByteArray generateLongCsaContent(int moveCount)
{
    QByteArray content = "PI\n+\n";
    for (int i = 0; i < moveCount; ++i) {
        content.append(i % 2 == 0 ? "+7776FU\n" : "-3334FU\n");
    }
    return content;
}

// USI形式: 大量手数の棋譜生成
inline QByteArray generateLongUsiContent(int moveCount)
{
    QByteArray content = "position startpos moves";
    for (int i = 0; i < moveCount; ++i) {
        content.append(i % 2 == 0 ? " 7g7f" : " 3c3d");
    }
    content.append("\n");
    return content;
}

// JKF形式: 大量手数の棋譜生成
inline QByteArray generateLongJkfContent(int moveCount)
{
    QByteArray content = R"({"moves": [{})";
    for (int i = 0; i < moveCount; ++i) {
        if (i % 2 == 0) {
            content.append(R"(, {"move": {"to": {"x": 7, "y": 6}, "from": {"x": 7, "y": 7}, "piece": "FU", "color": 0}})");
        } else {
            content.append(R"(, {"move": {"to": {"x": 3, "y": 4}, "from": {"x": 3, "y": 3}, "piece": "FU", "color": 1}})");
        }
    }
    content.append("]}");
    return content;
}

// USEN形式: 大量のbase36トリプレット生成
inline QByteArray generateLongUsenContent(int tripletCount)
{
    QByteArray content = "~0.";
    for (int i = 0; i < tripletCount; ++i) {
        content.append("7ku");
    }
    content.append("\n");
    return content;
}

// 一時ファイルにコンテンツを書き込み
inline bool writeToTempFile(QTemporaryFile& tmp, const QByteArray& content, const QString& ext)
{
    tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_boundary_XXXXXX.") + ext);
    if (!tmp.open())
        return false;
    tmp.write(content);
    tmp.close();
    return true;
}

} // namespace KifuTestHelper

#endif // KIFU_TEST_HELPER_H
