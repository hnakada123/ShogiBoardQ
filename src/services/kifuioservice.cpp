#include "kifuioservice.h"

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

// ファイル名に使えない/避けたい文字を置換し、空ならプレースホルダを返す
QString sanitizeForFileName(const QString& s, const QString& placeholder)
{
    QString t = s;
    if (t.isEmpty()) return placeholder;

    static const QRegularExpression badChars(R"([\\/:*?"<>|\r\n\t])");
    t.replace(badChars, QStringLiteral("_"));
    t = t.simplified();
    t.replace(QLatin1Char(' '), QLatin1Char('_'));
    return t;
}

} // namespace

QString KifuIoService::makeDefaultSaveFileName(PlayMode mode,
                                               const QString& human1,
                                               const QString& human2,
                                               const QString& engine1,
                                               const QString& engine2,
                                               const QDateTime& now)
{
    const QDateTime ts = now.isValid() ? now : QDateTime::currentDateTime();
    const QString stamp = ts.toString(QStringLiteral("yyyyMMdd_HHmmss"));

    auto mk = [&](const QString& p1, const QString& p2, const QString& tag) -> QString {
        const QString a = sanitizeForFileName(p1, QStringLiteral("P1"));
        const QString b = sanitizeForFileName(p2, QStringLiteral("P2"));
        return QStringLiteral("%1_%2_%3vs%4.kifu").arg(stamp, tag, a, b);
    };

    switch (mode) {
    case PlayMode::HumanVsHuman:
        return mk(human1, human2, QStringLiteral("HvH"));
    case PlayMode::EvenHumanVsEngine:      // 平手 P1: Human, P2: Engine
        return mk(human1, engine2, QStringLiteral("HvE"));
    case PlayMode::EvenEngineVsHuman:      // 平手 P1: Engine, P2: Human
        return mk(engine1, human2, QStringLiteral("EvH"));
    case PlayMode::EvenEngineVsEngine:     // 平手/駒落ち P1: Engine, P2: Engine
        return mk(engine1, engine2, QStringLiteral("EvE"));
    case PlayMode::HandicapEngineVsHuman:  // 駒落ち P1: Engine(下手), P2: Human(上手)
        return mk(engine1, human2, QStringLiteral("Handi_EvH"));
    case PlayMode::HandicapHumanVsEngine:  // 駒落ち P1: Human(下手), P2: Engine(上手)
        return mk(human1, engine2, QStringLiteral("Handi_HvE"));
    case PlayMode::HandicapEngineVsEngine: // 駒落ち P1: Engine(下手), P2: Engine(上手)
        return mk(engine1, engine2, QStringLiteral("Handi_EvE"));
    case PlayMode::AnalysisMode:
        return QStringLiteral("%1_Analysis.kifu").arg(stamp);
    case PlayMode::ConsiderationMode:
        return QStringLiteral("%1_Consideration.kifu").arg(stamp);
    case PlayMode::TsumiSearchMode:
        return QStringLiteral("%1_TsumiSearch.kifu").arg(stamp);
    case PlayMode::NotStarted:
    case PlayMode::PlayModeError:
    default:
        return QStringLiteral("%1_Session.kifu").arg(stamp);
    }
}

bool KifuIoService::writeKifuFile(const QString& filePath,
                                  const QStringList& kifuLines,
                                  QString* errorText)
{
    if (filePath.isEmpty()) {
        if (errorText) *errorText = QObject::tr("File path is empty.");
        return false;
    }

    const QFileInfo fi(filePath);
    if (!fi.absolutePath().isEmpty()) {
        QDir dir;
        if (!dir.mkpath(fi.absolutePath())) {
            if (errorText) {
                *errorText = QObject::tr("Failed to create directory: %1").arg(fi.absolutePath());
            }
            return false;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorText) {
            *errorText = QObject::tr("Could not open the file for writing: %1").arg(file.errorString());
        }
        return false;
    }

    QTextStream out(&file);
    // Qt6 の QTextStream は既定で UTF-8。特に設定不要。
    for (const QString& line : kifuLines) {
        out << line << QLatin1Char('\n');
    }
    out.flush();
    file.close();
    return true;
}
