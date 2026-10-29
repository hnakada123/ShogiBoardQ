#ifndef USICOMMANDCONTROLLER_H
#define USICOMMANDCONTROLLER_H

/// @file usicommandcontroller.h
/// @brief USIコマンド送信コントローラクラスの定義


#include <QObject>
#include <QString>


class EngineSlots;
class UsiLogPanel;

/**
 * @brief USI手動送信を担当するコントローラ
 *
 * 解析タブの USI コマンド欄から、動いているエンジンへコマンドを送り、送れたか・送れなかったかを
 * 通信ログに残す。
 */
class UsiCommandController : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        EngineSlots* engines = nullptr;   ///< 送信先のエンジンの枠（E1・E2。対局・検討で共有）
        UsiLogPanel* usiLog = nullptr;               ///< 送れなかった知らせを出す通信ログ
    };

    explicit UsiCommandController(const Deps& deps, QObject* parent = nullptr);

public slots:
    /**
     * @brief USIコマンドを手動送信
     * @param target 0=E1, 1=E2, 2=両方
     * @param command 送信するUSI文字列
     */
    void sendCommand(int target, const QString& command);

private:
    void appendStatus(const QString& text);

    Deps m_deps;
};

#endif // USICOMMANDCONTROLLER_H
