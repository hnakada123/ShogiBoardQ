/// @file p2pidentitydialog.cpp
/// @brief P2P対局のプレイヤー鍵を作る・読み込むダイアログの実装

#include "p2pidentitydialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include "dialogfontscale.h"
#include "networksettings.h"

namespace {

QLabel* wrappedLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}

} // namespace

QString P2pIdentityDialog::knownLimitsText()
{
    return tr("P2P対局は実験版です。次のことは防げません。\n"
              "・表示名は自己申告です。相手を確かめる根拠はプレイヤーIDと確認コードです。\n"
              "・負けた側のアプリが結果を確定しなければ（途中でアプリを終了するなど）、双方で確定した記録は残りません。\n"
              "・消費時間は自己申告で、少しの水増しと、切断を使った時間稼ぎは見分けられません。\n"
              "・エンジンが指した対局や、示し合わせた対局は見分けられません。\n"
              "・募集を出すと、IPアドレスは不特定の相手から見えます。対局相手はIPアドレスとプレイヤーIDを知ります。\n"
              "・同じ利用者の権限で動く悪意のあるプログラムからは、プレイヤー鍵を守れません。");
}

P2pIdentityDialog::P2pIdentityDialog(Mode mode, P2pIdentityStore* store, QWidget* parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_store(store)
{
    setWindowTitle(mode == Mode::Create ? tr("P2P対局の準備") : tr("プレイヤー鍵の読み込み"));
    auto* layout = new QVBoxLayout(this);

    if (mode == Mode::Create) {
        layout->addWidget(wrappedLabel(
            tr("P2P対局では、対局の開始と結果を、このアプリが作るプレイヤー鍵で自動的に確定します（操作は要りません）。"
               "公開鍵があなたのプレイヤーIDになります。"
               "実名や外部サービスのアカウントは使いません。鍵の保管の方法を選んでください。"),
            this));
        m_methods = new QButtonGroup(this);
        m_keychain = new QRadioButton(tr("OSの鍵の保管庫（推奨）"), this);
        m_passphraseFile = new QRadioButton(tr("パスフレーズで暗号化したファイル"), this);
        m_plainFile = new QRadioButton(tr("暗号化しないファイル（ファイルを読める人は鍵を使えます）"), this);
        m_methods->addButton(m_keychain, static_cast<int>(P2pIdentityStore::Method::Keychain));
        m_methods->addButton(m_passphraseFile, static_cast<int>(P2pIdentityStore::Method::Passphrase));
        m_methods->addButton(m_plainFile, static_cast<int>(P2pIdentityStore::Method::Plain));
        m_keychain->setEnabled(P2pIdentityStore::keychainSupported());
        (P2pIdentityStore::keychainSupported() ? m_keychain : m_passphraseFile)->setChecked(true);
        layout->addWidget(m_keychain);
        layout->addWidget(m_passphraseFile);
        layout->addWidget(m_plainFile);
        connect(m_methods, &QButtonGroup::idToggled, this, &P2pIdentityDialog::updateState);
    } else {
        layout->addWidget(wrappedLabel(tr("プレイヤー鍵はパスフレーズで暗号化されています。パスフレーズを入力してください。"), this));
    }

    auto* form = new QFormLayout;
    m_passphrase = new QLineEdit(this);
    m_passphrase->setEchoMode(QLineEdit::Password);
    form->addRow(tr("パスフレーズ"), m_passphrase);
    if (mode == Mode::Create) {
        m_confirm = new QLineEdit(this);
        m_confirm->setEchoMode(QLineEdit::Password);
        form->addRow(tr("パスフレーズ（確認）"), m_confirm);
        connect(m_confirm, &QLineEdit::textChanged, this, &P2pIdentityDialog::updateState);
    }
    connect(m_passphrase, &QLineEdit::textChanged, this, &P2pIdentityDialog::updateState);
    layout->addLayout(form);

    if (mode == Mode::Create) layout->addWidget(wrappedLabel(knownLimitsText(), this));

    m_status = wrappedLabel(QString(), this);
    layout->addWidget(m_status);
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttons->button(QDialogButtonBox::Ok)->setText(mode == Mode::Create ? tr("プレイヤー鍵を作る") : tr("読み込む"));
    connect(m_buttons, &QDialogButtonBox::accepted, this, &P2pIdentityDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &P2pIdentityDialog::reject);
    layout->addWidget(m_buttons);

    connect(m_store, &P2pIdentityStore::finished, this, &P2pIdentityDialog::onStoreFinished);
    DialogFontScale::install(this, NetworkSettings::p2pIdentityDialogFontSize);
    updateState();
}

void P2pIdentityDialog::updateState()
{
    const bool needsPassphrase = m_mode == Mode::Unlock || (m_passphraseFile && m_passphraseFile->isChecked());
    m_passphrase->setEnabled(needsPassphrase && !m_working);
    if (m_confirm) m_confirm->setEnabled(needsPassphrase && !m_working);
    bool ok = !m_working;
    if (needsPassphrase) ok = ok && !m_passphrase->text().isEmpty();
    if (m_mode == Mode::Create && needsPassphrase) ok = ok && m_passphrase->text() == m_confirm->text();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
    if (m_plainFile && m_plainFile->isChecked() && !m_working) {
        m_status->setText(tr("暗号化しないファイルは、このPCのファイルを読める人や持ち出した人が、あなたになりすまして対局できます。"));
    } else if (!m_working) {
        m_status->clear();
    }
}

void P2pIdentityDialog::accept()
{
    if (m_working) return;
    m_working = true;
    m_status->setText(m_mode == Mode::Create ? tr("鍵を作っています…") : tr("鍵を読み込んでいます…"));
    updateState();
    if (m_mode == Mode::Create) {
        const auto method = static_cast<P2pIdentityStore::Method>(m_methods->checkedId());
        m_store->create(method, method == P2pIdentityStore::Method::Passphrase ? m_passphrase->text() : QString());
    } else {
        m_store->unlock(m_passphrase->text());
    }
}

void P2pIdentityDialog::onStoreFinished(P2pIdentityStore::Failure failure)
{
    if (!m_working) return;
    m_working = false;
    m_passphrase->clear();
    if (m_confirm) m_confirm->clear();
    if (failure == P2pIdentityStore::Failure::None) {
        QDialog::accept();
        return;
    }
    updateState();
    m_status->setText(failureText(failure));
}

QString P2pIdentityDialog::failureText(P2pIdentityStore::Failure failure) const
{
    switch (failure) {
    case P2pIdentityStore::Failure::KeychainUnavailable:
        return tr("OSの鍵の保管庫を使えませんでした。パスフレーズで暗号化したファイルを選んでください。");
    case P2pIdentityStore::Failure::WrongPassphrase:
        return tr("パスフレーズが違うか、鍵のファイルが書き換えられています。");
    case P2pIdentityStore::Failure::CorruptFile:
        return tr("鍵のファイルを読めません。プレイヤー鍵を作り直してください。");
    case P2pIdentityStore::Failure::Storage:
        return tr("鍵のファイルを保存できませんでした。");
    case P2pIdentityStore::Failure::Crypto:
        return tr("暗号の処理に失敗しました。");
    case P2pIdentityStore::Failure::Busy:
    case P2pIdentityStore::Failure::None:
        break;
    }
    return QString();
}
