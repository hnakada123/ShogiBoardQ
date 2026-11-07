#ifndef KIFUCLIPBOARDCONTROLLER_H
#define KIFUCLIPBOARDCONTROLLER_H

/// @file kifuclipboardcontroller.h
/// @brief 棋譜・局面のクリップボードへのコピー

#include <QObject>
#include <QString>

#include "kifuformat.h"

class KifuDocument;
class KifuNavigationState;
class ShogiBoard;

/**
 * @brief 編集メニューの「コピー」（棋譜を形式で・USI で表示中の手まで・局面を SFEN と BOD で）
 *
 * 貼り付けは棋譜を置き換えるので KifuFileController が受け持つ。失うものの知らせは保存の確認と同じ表で決める。
 */
class KifuClipboardController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        KifuDocument* document = nullptr;               ///< 開いている棋譜（必須）
        const ShogiBoard* board = nullptr;              ///< 盤（局面のコピー。局面編集の変更も含む。必須）
        const KifuNavigationState* navState = nullptr;  ///< 表示中の手（必須）
    };

    explicit KifuClipboardController(const Deps& deps, QObject* parent = nullptr);

public slots:
    bool copyKifToClipboard() { return copyRecord(KifuFormat::Kif); }
    bool copyKi2ToClipboard() { return copyRecord(KifuFormat::Ki2); }
    bool copyCsaToClipboard() { return copyRecord(KifuFormat::Csa); }
    bool copyUsiToClipboard() { return copyRecord(KifuFormat::Usi); }
    bool copyJkfToClipboard() { return copyRecord(KifuFormat::Jkf); }
    bool copyUsenToClipboard() { return copyRecord(KifuFormat::Usen); }
    /// USI 形式で、表示中の手順の表示中の手までをコピーする
    bool copyUsiCurrentToClipboard();
    /// 盤の局面を SFEN でコピーする
    bool copySfenToClipboard();
    /// 盤の局面を BOD（局面図）でコピーする
    bool copyBodToClipboard();

signals:
    /// ステータスバーに出す結果の文
    void statusMessage(const QString& message, int timeoutMs);

private:
    bool copyRecord(KifuFormat format);
    bool setClipboardText(const QString& text, const QString& successMsg);

    Deps m_deps;
};

#endif // KIFUCLIPBOARDCONTROLLER_H
