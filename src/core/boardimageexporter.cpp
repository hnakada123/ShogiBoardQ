#include "boardimageexporter.h"
#include <QClipboard>
#include <QApplication>
#include <QImageWriter>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QImage>
#include <QPixmap>
#include <QMessageBox>

static bool hasFormat(const QSet<QString>& fmts, const QString& key) {
    if (key == "jpeg") return fmts.contains("jpeg") || fmts.contains("jpg");
    return fmts.contains(key);
}

void BoardImageExporter::copyToClipboard(QWidget* boardWidget) {
    if (!boardWidget) return;
    QApplication::clipboard()->setPixmap(boardWidget->grab());
}

void BoardImageExporter::saveImage(QWidget* parent, QWidget* boardWidget) {
    if (!boardWidget) return;

    QSet<QString> fmts;
    for (const QByteArray& f : QImageWriter::supportedImageFormats())
        fmts.insert(QString::fromLatin1(f).toLower());
    if (fmts.isEmpty()) {
        QMessageBox::critical(parent, QObject::tr("Error"),
                              QObject::tr("No writable image formats are available."));
        return;
    }

    struct F { QString key; QString filter; QString ext; };
    const QVector<F> cands = {
                              {"png","PNG (*.png)","png"},{"tiff","TIFF (*.tiff *.tif)","tiff"},
                              {"jpeg","JPEG (*.jpg *.jpeg)","jpg"},{"webp","WebP (*.webp)","webp"},
                              {"bmp","BMP (*.bmp)","bmp"},{"ppm","PPM (*.ppm)","ppm"},
                              {"pgm","PGM (*.pgm)","pgm"},{"pbm","PBM (*.pbm)","pbm"},
                              {"xbm","XBM (*.xbm)","xbm"},{"xpm","XPM (*.xpm)","xpm"},
                              };
    QStringList filters; QMap<QString,F> map;
    for (auto& c : cands) if (hasFormat(fmts, c.key)) { filters<<c.filter; map[c.filter]=c; }

    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString base = QStringLiteral("ShogiBoard_%1").arg(stamp);
    const QString dir  = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString defExt = hasFormat(fmts, "png") ? "png" : map[filters.first()].ext;
    QString selFilter = filters.isEmpty() ? QString() : filters.first();

    QString path = QFileDialog::getSaveFileName(parent, QObject::tr("Output the image"),
                                                QDir(dir).filePath(base + "." + defExt), filters.join(";;"), &selFilter);
    if (path.isEmpty()) return;

    QString suffix = QFileInfo(path).suffix().toLower();
    QString fmt;
    if (suffix.isEmpty()) {
        const auto f = map.value(selFilter);
        path += "." + f.ext; fmt = f.key;
    } else { if (suffix=="jpg") suffix="jpeg"; fmt=suffix; if (!hasFormat(fmts, fmt)) {
            QMessageBox::critical(parent, QObject::tr("Error"),
                                  QObject::tr("This image format is not available: %1").arg(suffix));
            return; } }

    const QImage img = boardWidget->grab().toImage();
    QImageWriter w(path, fmt.toLatin1());
    if (fmt=="jpeg" || fmt=="webp") w.setQuality(95);
    if (!w.write(img)) {
        QMessageBox::critical(parent, QObject::tr("Error"),
                              QObject::tr("Failed to save the image: %1").arg(w.errorString()));
    }
}
