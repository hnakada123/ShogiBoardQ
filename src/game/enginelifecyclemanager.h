#ifndef ENGINELIFECYCLEMANAGER_H
#define ENGINELIFECYCLEMANAGER_H

/// @file enginelifecyclemanager.h
/// @brief エンジンの枠 E1・E2（対局・検討・棋譜解析・通信対局で共有）のリースと取り上げの知らせ

#include <QObject>
#include <QPointer>
#include <QString>
#include <array>

class Usi;
class EngineLifecycleManager;

/**
 * @brief 枠のエンジンを使う権利（move-only）
 *
 * EngineLifecycleManager::acquire が返す。持ち主はこれを持っている間だけ engine() を使う。
 * 手放す（reset・破棄）と、枠のエンジンを止めて枠を空ける（思考情報・通信ログの表示は残す）。
 * ほかの持ち主が同じ枠を取った・全部の枠を破棄したときは、前の持ち主に
 * EngineLifecycleManager::engineRevoked で知らせ、このリースは空になる（engine() が nullptr）。
 */
class EngineLease
{
public:
    EngineLease() = default;
    ~EngineLease();
    EngineLease(const EngineLease&) = delete;
    EngineLease& operator=(const EngineLease&) = delete;
    EngineLease(EngineLease&& other) noexcept;
    EngineLease& operator=(EngineLease&& other) noexcept;

    /// 使えるエンジン（取り上げられた・手放したら nullptr）
    Usi* engine() const;
    explicit operator bool() const { return engine() != nullptr; }
    /// 手放す（枠を空ける）
    void reset();

private:
    friend class EngineLifecycleManager;
    EngineLease(EngineLifecycleManager* manager, Usi* engine);

    QPointer<EngineLifecycleManager> m_manager;
    QPointer<Usi> m_engine;
};

/**
 * @brief エンジンの枠（E1・E2）を持ち、使う権利（EngineLease）を貸す
 *
 * エンジンは表示の枠（E1・E2。思考情報と通信ログの表示先）に置く。人間対エンジンではエンジンは常に E1 に置く
 * （後手を指すときも）。表示のモデルには触れない（表示する側が enginesChanged を受けて枠のエンジンとモデルを結ぶ）。
 * 手番・持ち時間・投了などの対局の知らせは、対局で指すエンジン（MatchEngineSeat）が受け持つ。
 */
class EngineLifecycleManager : public QObject
{
    Q_OBJECT

public:
    explicit EngineLifecycleManager(QObject* parent = nullptr);
    ~EngineLifecycleManager() override;

    /**
     * @brief 枠（1=E1、2=E2）にエンジンを作り、owner に使わせる
     *
     * 前にあったエンジンは破棄し、その持ち主（owner と違えば）に engineRevoked で知らせる。
     * 起動は使う側が Usi::startEngine で行う。
     */
    [[nodiscard]] EngineLease acquire(QObject* owner, int slot, const QString& engineName);

    /// 枠のエンジン（1=E1、2=E2）
    Usi* engineInSlot(int slot) const;

    /// 全エンジンを破棄する（持ち主には engineRevoked で知らせる。clearDisplay なら思考情報・通信ログの表示も消すよう知らせる）
    void destroyEngines(bool clearDisplay = true);
    /// engine が枠に入っているか
    bool holds(const Usi* engine) const;

signals:
    /// 枠のエンジンが変わった（作った・破棄した）
    void enginesChanged();
    /// owner のエンジンを取り上げた（ほかの持ち主が同じ枠を取った・全部の枠を破棄した）。owner のリースは空になる
    void engineRevoked(QObject* owner);
    /// 前のエンジンの思考情報・通信ログの表示を消す（新しい対局・検討を始める）
    void displayClearRequested();

private:
    friend class EngineLease;

    struct Seat {
        Usi* engine = nullptr;
        QPointer<QObject> owner;
    };
    Seat& seat(int slot) { return m_seats[static_cast<size_t>(slot == 2 ? 1 : 0)]; }
    const Seat& seat(int slot) const { return m_seats[static_cast<size_t>(slot == 2 ? 1 : 0)]; }
    /// 枠を空ける。notifyOwner なら持ち主に engineRevoked で知らせる
    void destroySeat(Seat& seat, bool clearThinking, bool notifyOwner);
    /// リースを手放したとき（持ち主には知らせない）
    void release(Usi* engine);

    std::array<Seat, 2> m_seats;
};

#endif // ENGINELIFECYCLEMANAGER_H
