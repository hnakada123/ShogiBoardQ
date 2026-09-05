// 見やすい駒セットをフォント非依存のSVGとして生成する開発用ツール。
// ビルド・実行方法は resources/images/pieces/clear/README.md を参照。
#include <QGuiApplication>
#include <QRawFont>
#include <QPainterPath>
#include <QTransform>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QXmlStreamReader>

struct PieceShape {
    QString points;
    QString strokeWidth;
    QStringList transforms;
};

// 標準SVGの輪郭と先後それぞれの変換をそのまま使い、駒ごとの寸法を保つ。
static bool readPieceShape(const QString& path, PieceShape& shape)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QXmlStreamReader xml(&file);
    QStringList groups;
    int polygonCount = 0;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const auto attributes = xml.attributes();
            if (xml.name() == QLatin1String("g")) {
                groups.append(attributes.value(QLatin1String("transform")).toString());
            } else if (xml.name() == QLatin1String("polygon")) {
                ++polygonCount;
                shape.points = attributes.value(QLatin1String("points")).toString();
                shape.strokeWidth = attributes.value(QLatin1String("stroke-width")).toString();
                shape.transforms = groups;
                shape.transforms.append(attributes.value(QLatin1String("transform")).toString());
                shape.transforms.removeAll(QString());
            }
        } else if (xml.isEndElement() && xml.name() == QLatin1String("g")) {
            groups.removeLast();
        }
    }
    return !xml.hasError() && polygonCount == 1
        && !shape.points.isEmpty() && !shape.strokeWidth.isEmpty();
}

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
    if (app.arguments().size() != 4) {
        qCritical("Usage: generate_clear_pieces FONT_FILE STANDARD_DIRECTORY OUTPUT_DIRECTORY");
        return 1;
    }
    const QRawFont font(app.arguments().at(1), 1000, QFont::PreferNoHinting);
    if (!font.isValid()) {
        qCritical("Cannot load font");
        return 1;
    }
    const QDir standardDir(app.arguments().at(2));
    QDir dir(app.arguments().at(3));
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
        // 従来の100単位の文字配置を標準SVGの45単位に換算する。
        const QString lettering = glyphPath(font, label,
            QRectF((22 + piece.opticalOffsetX) * 0.45, 27 * 0.45, 56 * 0.45, 55 * 0.45));
        for (const bool gote : {false, true}) {
            const QString side = gote ? QStringLiteral("Gote_") : QStringLiteral("Sente_");
            const QString filename = side + QLatin1String(piece.file) + QStringLiteral("45.svg");
            PieceShape shape;
            if (!readPieceShape(standardDir.filePath(filename), shape)) {
                qCritical("Cannot read standard piece shape: %s", qPrintable(filename));
                return 1;
            }
            QFile file(dir.filePath(filename));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return 1;
            QTextStream out(&file);
            out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"45\" height=\"45\" viewBox=\"0 0 45 45\">\n"
                << "  <title>" << (gote ? "後手 " : "先手 ") << label << "</title>\n";
            for (const auto& transform : shape.transforms) {
                out << "  <g transform=\"" << transform << "\">\n";
            }
            out << "    <polygon points=\"" << shape.points
                << "\" fill=\"#fff3cf\" stroke=\"#493522\" stroke-width=\"" << shape.strokeWidth << "\"/>\n"
                << "    <path fill=\"" << (piece.promoted ? "#a51d26" : "#171717")
                << "\" fill-rule=\"nonzero\" d=\"" << lettering << "\"/>\n";
            for (qsizetype i = 0; i < shape.transforms.size(); ++i) out << "  </g>\n";
            out << "</svg>\n";
        }
    }
    return 0;
}
