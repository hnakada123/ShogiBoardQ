// 見やすい駒セットをフォント非依存のSVGとして生成する開発用ツール。
// ビルド・実行方法は resources/images/pieces/clear/README.md を参照。
#include <QGuiApplication>
#include <QRawFont>
#include <QPainterPath>
#include <QTransform>
#include <QDir>
#include <QFile>
#include <QTextStream>

static QString svgPath(const QPainterPath& path)
{
    QString data;
    QTextStream out(&data);
    out.setRealNumberNotation(QTextStream::FixedNotation);
    out.setRealNumberPrecision(3);
    for (int i = 0; i < path.elementCount(); ++i) {
        const auto e = path.elementAt(i);
        if (e.isMoveTo()) out << "M" << e.x << " " << e.y;
        else if (e.isLineTo()) out << "L" << e.x << " " << e.y;
        else if (e.type == QPainterPath::CurveToElement) {
            const auto c = path.elementAt(++i);
            const auto end = path.elementAt(++i);
            out << "C" << e.x << " " << e.y << " " << c.x << " " << c.y
                << " " << end.x << " " << end.y;
        }
    }
    return data;
}

static QString glyphPath(const QRawFont& font, const QString& text, const QRectF& box)
{
    const auto glyphs = font.glyphIndexesForString(text);
    Q_ASSERT(glyphs.size() == 1 && glyphs.first() != 0);
    const auto path = font.pathForGlyph(glyphs.first());
    const auto bounds = path.boundingRect();
    const qreal scale = qMin(box.width() / bounds.width(), box.height() / bounds.height());
    QTransform transform;
    transform.translate(box.center().x(), box.center().y());
    transform.scale(scale, scale);
    transform.translate(-bounds.center().x(), -bounds.center().y());
    return svgPath(transform.map(path));
}

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    if (app.arguments().size() != 3) {
        qCritical("Usage: generate_clear_pieces FONT_FILE OUTPUT_DIRECTORY");
        return 1;
    }
    const QRawFont font(app.arguments().at(1), 1000, QFont::PreferNoHinting);
    if (!font.isValid()) {
        qCritical("Cannot load font");
        return 1;
    }
    QDir dir(app.arguments().at(2));
    if (!dir.mkpath(QStringLiteral("."))) return 1;

    struct Piece {
        const char* file;
        const char* label;
        bool promoted;
        qreal opticalOffsetX = 0.0; // 字形の見た目の重心に合わせた横位置補正
    };
    const Piece pieces[] = {
        {"fu", "歩", false}, {"kyou", "香", false}, {"kei", "桂", false},
        {"gin", "銀", false}, {"kin", "金", false}, {"kaku", "角", false, -2.0},
        {"hi", "飛", false}, {"ou", "王", false}, {"gyoku", "玉", false},
        {"to", "と", true}, {"narikyou", "杏", true}, {"narikei", "圭", true},
        {"narigin", "全", true}, {"uma", "馬", true, -2.0}, {"ryuu", "龍", true}
    };
    for (const auto& piece : pieces) {
        const QString label = QString::fromUtf8(piece.label);
        const QString lettering = glyphPath(font, label,
            QRectF(22 + piece.opticalOffsetX, 27, 56, 55));
        for (const bool gote : {false, true}) {
            const QString side = gote ? QStringLiteral("Gote_") : QStringLiteral("Sente_");
            QFile file(dir.filePath(side + QLatin1String(piece.file) + QStringLiteral("45.svg")));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return 1;
            QTextStream out(&file);
            out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"45\" height=\"45\" viewBox=\"0 0 100 100\">\n"
                << "  <title>" << (gote ? "後手 " : "先手 ") << label << "</title>\n"
                << "  <g" << (gote ? " transform=\"rotate(180 50 50)\"" : "") << ">\n"
                << "    <path d=\"M50 5L79 18L94 92H6L21 18Z\" fill=\"#fff3cf\" stroke=\"#493522\" stroke-width=\"2.8\" stroke-linejoin=\"round\"/>\n"
                << "    <path fill=\"" << (piece.promoted ? "#a51d26" : "#171717")
                << "\" fill-rule=\"nonzero\" d=\"" << lettering << "\"/>\n"
                << "  </g>\n</svg>\n";
        }
    }
    return 0;
}
