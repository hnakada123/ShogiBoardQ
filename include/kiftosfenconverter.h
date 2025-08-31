#ifndef KIFTOSFENCONVERTER_H
#define KIFTOSFENCONVERTER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QPoint>

struct KifDisplayItem {
    QString prettyMove;
    QString timeText;
};

class KifToSfenConverter
{
public:
    KifToSfenConverter();

    QStringList convertFile(const QString& kifPath, QString* errorMessage = nullptr);
    static QList<KifDisplayItem> extractMovesWithTimes(const QString& kifPath,
                                                       QString* errorMessage = nullptr);
    static QString detectInitialSfenFromFile(const QString& kifPath, QString* detectedLabel = nullptr);
    static QString mapHandicapToSfen(const QString& label);

private:
    bool convertMoveLine(const QString& line, QString& usiMove);
    static int asciiDigitToInt(QChar c);
    static int zenkakuDigitToInt(QChar c);
    static int kanjiDigitToInt(QChar c);
    static bool findDestination(const QString& line, int& toFile, int& toRank, bool& isSameAsPrev);

private:
    QPoint m_lastDest;
};

#endif // KIFTOSFENCONVERTER_H
