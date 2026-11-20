#ifndef P2PCONNECTIONPANEL_H
#define P2PCONNECTIONPANEL_H

/// @file p2pconnectionpanel.h
/// @brief P2P対局のウィンドウの「接続」タブ（相手のいる場所・相手とのつなぎ方・協力ノード）

#include <QString>
#include <QStringList>
#include <QWidget>

#include <optional>

class P2pChoiceCard;
class QFrame;
class QLabel;
class QListWidget;
class QPushButton;

/**
 * @brief 相手とどうつなぐかを決める欄
 *
 * 決めるのは3つ: 相手のいる場所（同じPC・同じLAN か、インターネット）、相手とのつなぎ方（直接つなぐか、協力ノードの
 * 中継だけを使うか）、協力ノードのアドレス。選択肢のどちらにも、選ぶと何が起きるか（経路・相手に伝わるもの・
 * 要るもの）を並べ、選んだ組み合わせでどうなるか（対局できる相手・招待の形・IPアドレスが見える人）を下に出す。
 * つながらない組み合わせ（「インターネット」で協力ノードなし、など）は、理由と直し方を出す。協力ノードの欄は、
 * 要る組み合わせのときと、登録があるときだけ出す。
 *
 * 値を持つだけで、保存と接続は受け手（P2pWindow）が行う。自分や相手のアドレス（IP）は出さない（出すのは、利用者が
 * 入れた協力ノードのアドレスだけ）。
 */
class P2pConnectionPanel : public QWidget
{
    Q_OBJECT
public:
    /// 接続している間の、協力ノードの使われ方
    struct NodeStatus {
        int relayReservations = 0;   ///< 協力ノードに取れた中継の予約の数
        int meetNodes = 0;           ///< つながっている、合言葉の待ち合わせに対応した協力ノードの数
    };

    /// 登録できる協力ノードの数（通信プロセスが受け付ける数と同じ）
    static constexpr int kMaxNodes = 16;

    explicit P2pConnectionPanel(QWidget* parent = nullptr);

    /// 参加するネットワークの名前（「同じPC・同じLAN」は sbq-local-1、「インターネット」は sbq-limited-1）
    QString networkName() const;
    void setNetworkName(const QString& name);
    /// 登録した協力ノードのアドレス
    QStringList bootstrap() const;
    void setBootstrap(const QStringList& addresses);
    /// 協力ノードの中継だけを使う（相手と直接はつながない）
    bool relayOnly() const;
    void setRelayOnly(bool relayOnly);

    /// 今の設定を1行にした文（接続していない間、状態の帯に出す）
    QString settingSummary() const;
    /// 今の設定で作る招待に、何が入るか（「招待」タブに出す）
    QString inviteContentsText() const;

    /// 接続している間は変えられない（変えるときは切断する）
    void setLocked(bool locked);
    /// 接続している間の、協力ノードの使われ方（接続していなければ nullopt）
    void setNodeStatus(const std::optional<NodeStatus>& status);

signals:
    /// 利用者が設定を変えた
    void settingsChanged();
    /// 中身の高さが変わった（収まらなければ、受け手がウィンドウを広げる）
    void contentResized();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onChoiceToggled(bool checked);
    void onAddNode();
    void onPasteNodes();
    void onRemoveNode();
    void onShowNodes();
    void updateNodeButtons();

private:
    QWidget* createNodeSection();
    QWidget* createNodeCollapsed();
    QWidget* createSummary();
    QWidget* createWarning();
    bool isLan() const;
    /// 選択肢の説明・協力ノードの欄・下の説明を、今の値に合わせる
    void updateTexts();
    /// アドレスを足す（空の行・同じアドレス・形の違うもの・上限を超える分は足さない）。足した数を返す
    int addAddresses(const QStringList& addresses);
    void updateNodeListHeight();

    P2pChoiceCard* m_lan = nullptr;
    P2pChoiceCard* m_internet = nullptr;
    P2pChoiceCard* m_direct = nullptr;
    P2pChoiceCard* m_relay = nullptr;
    QLabel* m_lockNote = nullptr;
    QWidget* m_nodeSection = nullptr;      ///< 協力ノードの欄（要るとき・登録があるとき）
    QWidget* m_nodeCollapsed = nullptr;    ///< 協力ノードが要らないときの1行
    QLabel* m_nodeCaption = nullptr;
    QListWidget* m_nodes = nullptr;
    QPushButton* m_addNode = nullptr;
    QPushButton* m_pasteNodes = nullptr;
    QPushButton* m_removeNode = nullptr;
    QPushButton* m_showNodes = nullptr;
    QLabel* m_nodeStatus = nullptr;
    QFrame* m_summary = nullptr;
    QLabel* m_summaryWho = nullptr;
    QLabel* m_summaryInvite = nullptr;
    QLabel* m_summarySeen = nullptr;
    QFrame* m_warning = nullptr;
    QLabel* m_warningTitle = nullptr;
    QLabel* m_warningBody = nullptr;
    std::optional<NodeStatus> m_status;
    bool m_locked = false;
    bool m_nodesOpened = false;   ///< 要らない設定のときに、利用者が協力ノードの欄を開いた
    bool m_loading = false;       ///< 値を外から入れている間（変更の知らせを出さない）
};

#endif // P2PCONNECTIONPANEL_H
