/// @file nyugyokudeclarationhandler.cpp
/// @brief 利用者の入玉宣言の実装

#include "nyugyokudeclarationhandler.h"

#include "dialogutils.h"
#include "gamechannel.h"
#include "kifudocument.h"
#include "nyugyokujudgement.h"

#include <QMessageBox>

NyugyokuDeclarationHandler::NyugyokuDeclarationHandler(QWidget* parentWidget, const KifuDocument* document,
                                                       QObject* parent)
    : QObject(parent)
    , m_parentWidget(parentWidget)
    , m_document(document)
{
}

bool NyugyokuDeclarationHandler::handleDeclaration(GameChannel* channel)
{
    if (!channel) {
        QMessageBox::warning(m_parentWidget, tr("入玉宣言"), tr("対局中ではありません。"));
        return false;
    }

    // 宣言できるのは手番の人間だけ。エンジンの手番で押してもエンジンの代わりには宣言しない
    if (!channel->isHumanToMove()) {
        QMessageBox::information(m_parentWidget, tr("入玉宣言"), tr("入玉宣言は自分の手番で行います。"));
        return false;
    }

    // こちらで判定する対局は、持将棋ルール（対局ダイアログの設定）が要る
    if (channel->judgesDeclaration()) {
        if (!NyugyokuJudgement::configuredRule()) {
            QMessageBox::warning(m_parentWidget, tr("入玉宣言"),
                tr("持将棋ルールが「なし」に設定されています。\n"
                   "対局ダイアログで「24点法」または「27点法」を選択してください。"));
            return false;
        }
    }

    // 誤操作で負けないよう確認する。駒落ちは対局情報の見出しに合わせて下手・上手と呼ぶ
    const bool handicapNames = m_document && m_document->usesHandicapNames();
    const QString declarerName = NyugyokuJudgement::declarerName(channel->sideToMove() == Turn::Black, handicapNames);
    if (!DialogUtils::confirmAction(
            m_parentWidget,
            tr("入玉宣言確認"),
            tr("%1が入玉宣言を行います。\n\n"
               "宣言条件を満たさない場合は宣言側の負けとなります。\n"
               "本当に宣言しますか？").arg(declarerName),
            tr("宣言する"))) {
        return false;
    }

    // ローカル対局は盤面から判定して終局した結果を知らせる（棋譜は終局の行まで更新済み）
    if (const std::optional<NyugyokuJudgement::Result> result = channel->declareNyugyoku())
        QMessageBox::information(m_parentWidget, NyugyokuJudgement::resultTitle(), result->message);
    return true;
}
