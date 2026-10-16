#ifndef NYUGYOKUDECLARATIONHANDLER_H
#define NYUGYOKUDECLARATIONHANDLER_H

/// @file nyugyokudeclarationhandler.h
/// @brief 利用者の入玉宣言（確認・宣言・結果の表示）

#include <QObject>

class GameChannel;
class KifuDocument;
class QWidget;

/**
 * @brief 利用者の入玉宣言をローカル対局・通信対局で同じ手順で行う
 *
 * 手番の人間だけが宣言できる。確認してから対局相手の窓口（GameChannel）に宣言させ、
 * こちらで判定した対局（ローカル対局）は判定結果を知らせる。通信対局の成否はサーバーが判定して終局を知らせる。
 */
class NyugyokuDeclarationHandler : public QObject
{
    Q_OBJECT

public:
    /// parentWidget: ダイアログの親、document: 宣言する側を下手・上手と呼ぶか（対局情報の見出し）
    NyugyokuDeclarationHandler(QWidget* parentWidget, const KifuDocument* document, QObject* parent = nullptr);

    /**
     * @brief 入玉宣言する
     * @param channel 対局中の対局相手の窓口（対局していなければ nullptr）
     * @return 宣言したか（対局していない・手番でない・キャンセルなら false）
     */
    bool handleDeclaration(GameChannel* channel);

private:
    QWidget* m_parentWidget = nullptr;
    const KifuDocument* m_document = nullptr;
};

#endif // NYUGYOKUDECLARATIONHANDLER_H
