#ifndef USIOPTIONLINEPARSER_H
#define USIOPTIONLINEPARSER_H

/// @file usioptionlineparser.h
/// @brief USI option 行パーサの宣言

#include <QString>
#include <QStringList>

struct ParsedOptionLine {
    QString name;
    QString type;
    QString defaultValue;
    QString minValue;
    QString maxValue;
    QStringList comboValues;
};

bool parseUsiOptionLine(const QString& rawLine, ParsedOptionLine& parsed, QString* errorMessage);

#endif // USIOPTIONLINEPARSER_H
