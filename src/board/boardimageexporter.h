#ifndef BOARDIMAGEEXPORTER_H
#define BOARDIMAGEEXPORTER_H

#include <QObject>
class QWidget;
class QImage;

class BoardImageExporter final {
public:
    static void copyToClipboard(QWidget* boardWidget);
    static void saveImage(QWidget* parent, QWidget* boardWidget);
};

#endif // BOARDIMAGEEXPORTER_H
