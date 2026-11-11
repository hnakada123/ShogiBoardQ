/// @file tst_p2p_protocol.cpp
/// @brief P2P対局の形式と記録の試験（決定的CBOR・署名・表示名・規則・証跡の検証。実装仕様 13.1節）

#include <QtTest>

#include "p2p_test_util.h"
#include "p2pcbor.h"
#include "p2pdisplayname.h"
#include "p2pevidenceverifier.h"
#include "p2pframes.h"
#include "matchverificationservice.h"

using namespace P2p;

namespace {

QByteArray hex(const char* text) { return QByteArray::fromHex(text); }

/// 32バイトの小端の整数に群の位数 L を足す（正規の形でない S を作る）
QByteArray addGroupOrder(const QByteArray& s)
{
    const QByteArray order = hex("edd3f55c1a631258d69cf7a2def9de1400000000000000000000000000000010");
    QByteArray out(32, '\0');
    unsigned carry = 0;
    for (int i = 0; i < 32; ++i) {
        const unsigned sum = static_cast<quint8>(s.at(i)) + static_cast<quint8>(order.at(i)) + carry;
        out[i] = static_cast<char>(sum & 0xFFu);
        carry = sum >> 8;
    }
    return out;
}

} // namespace

class TestP2pProtocol : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QVERIFY(Crypto::initialize()); }

    // ------------------------------------------------------------ 決定的CBOR

    void cborRoundTrip()
    {
        const CborValue value = CborValue::array({CborValue::uint(0), CborValue::uint(23), CborValue::uint(24),
                                                  CborValue::uint(255), CborValue::uint(256), CborValue::uint(65535),
                                                  CborValue::uint(65536), CborValue::uint(0xFFFFFFFFULL),
                                                  CborValue::uint(0x100000000ULL), CborValue::bytes("ab"),
                                                  CborValue::text("SBQ"), CborValue::array({})});
        const QByteArray bytes = Cbor::encode(value);
        const std::optional<CborValue> decoded = Cbor::decode(bytes);
        QVERIFY(decoded);
        QVERIFY(*decoded == value);
        QCOMPARE(Cbor::encode(*decoded), bytes);
        QCOMPARE(Cbor::encode(CborValue::uint(24)), hex("1818"));
        QCOMPARE(Cbor::encode(CborValue::uint(0x100000000ULL)), hex("1b0000000100000000"));
    }

    void cborRejectsNonDeterministic_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("non-shortest uint") << hex("1817");
        QTest::newRow("non-shortest 2-byte") << hex("1900ff");
        QTest::newRow("indefinite array") << hex("9f01ff");
        QTest::newRow("indefinite bytes") << hex("5f41aaff");
        QTest::newRow("map") << hex("a10102");
        QTest::newRow("negative") << hex("20");
        QTest::newRow("tag") << hex("c001");
        QTest::newRow("float") << hex("f93c00");
        QTest::newRow("true") << hex("f5");
        QTest::newRow("trailing") << hex("0101");
        QTest::newRow("truncated") << hex("8301");
        QTest::newRow("bad utf8") << hex("62c328");
        QTest::newRow("overlong utf8") << hex("62c0af");
        QTest::newRow("surrogate") << hex("63eda080");
        QTest::newRow("huge length") << hex("5bffffffffffffffff");
        QTest::newRow("empty") << QByteArray();
        QByteArray deep;
        for (int i = 0; i < 17; ++i) deep.append('\x81');
        deep.append('\x00');
        QTest::newRow("depth 17") << deep;
    }

    void cborRejectsNonDeterministic()
    {
        QFETCH(QByteArray, bytes);
        QVERIFY(!Cbor::decode(bytes));
    }

    void cborAcceptsDepth16()
    {
        QByteArray deep;
        for (int i = 0; i < 15; ++i) deep.append('\x81');
        deep.append('\x00');
        QVERIFY(Cbor::decode(deep));
    }

    /// Rust の実装と同じ入力に同じ結果を返す（protocol/testvectors/cbor.txt）
    void cborSharedVectors()
    {
        const QList<QByteArray> lines = P2pTest::readVectorLines(QStringLiteral("cbor.txt"));
        QVERIFY2(!lines.isEmpty(), "protocol/testvectors/cbor.txt is missing");
        for (const QByteArray& line : lines) {
            const QList<QByteArray> parts = line.split(' ');
            QVERIFY2(parts.size() >= 2, line.constData());
            const bool expected = parts.at(0) == "valid";
            const QByteArray bytes = QByteArray::fromHex(parts.at(1));
            QVERIFY2(Cbor::decode(bytes).has_value() == expected, line.constData());
        }
    }

    void frameSharedVectors()
    {
        const QList<QByteArray> lines = P2pTest::readVectorLines(QStringLiteral("frames.txt"));
        QVERIFY2(!lines.isEmpty(), "protocol/testvectors/frames.txt is missing");
        for (const QByteArray& line : lines) {
            const QList<QByteArray> parts = line.split(' ');
            QVERIFY2(parts.size() >= 2, line.constData());
            const bool expected = parts.at(0) == "valid";
            QVERIFY2(Frame::isWellFormedEnvelope(QByteArray::fromHex(parts.at(1))) == expected, line.constData());
        }
    }

    void networkSharedVectors()
    {
        const QList<QByteArray> lines = P2pTest::readVectorLines(QStringLiteral("network.txt"));
        QVERIFY2(lines.size() >= 2, "protocol/testvectors/network.txt is missing");
        for (const QByteArray& line : lines) {
            const QList<QByteArray> parts = line.split(' ');
            QCOMPARE(parts.size(), 5);
            const QByteArray id = networkId(parts.at(0));
            QCOMPARE(id.toHex(), parts.at(1));
            QCOMPARE(Rules::v1Hash().toHex(), parts.at(2));
            QCOMPARE(lobbyKey(id, Rules::v1Hash()).toHex(), parts.at(3));
            QCOMPARE(lobbyTopic(id, Rules::v1Hash()).toLatin1(), parts.at(4));
            QVERIFY(findNetwork(QString::fromLatin1(parts.at(0))));
        }
    }

    // ------------------------------------------------------------ 表示名

    void displayNameSharedVectors()
    {
        const QList<QByteArray> lines = P2pTest::readVectorLines(QStringLiteral("displayname.txt"));
        QVERIFY2(!lines.isEmpty(), "protocol/testvectors/displayname.txt is missing");
        for (const QByteArray& line : lines) {
            const QList<QByteArray> parts = line.split(' ');
            QVERIFY2(parts.size() >= 2, line.constData());
            const bool expected = parts.at(0) == "accept";
            QVERIFY2(DisplayName::isValidUtf8(QByteArray::fromHex(parts.at(1))) == expected, line.constData());
        }
    }

    void displayNameRules()
    {
        QVERIFY(DisplayName::isValid(QStringLiteral("将棋太郎")));
        QVERIFY(DisplayName::isValid(QStringLiteral("Taro 2")));
        QVERIFY(!DisplayName::isValid(QString()));
        QVERIFY(!DisplayName::isValid(QStringLiteral(" Taro")));
        QVERIFY(!DisplayName::isValid(QStringLiteral("Taro ")));
        QVERIFY(!DisplayName::isValid((QStringLiteral("a") + QChar(0x200B) + QStringLiteral("b"))));     // ゼロ幅
        QVERIFY(!DisplayName::isValid((QStringLiteral("a") + QChar(0x202E) + QStringLiteral("b"))));     // 双方向の制御
        QVERIFY(!DisplayName::isValid((QStringLiteral("か") + QChar(0x3099))));     // 結合する濁点
        QVERIFY(!DisplayName::isValid(QStringLiteral("「名人」")));       // かぎ括弧
        QVERIFY(!DisplayName::isValid(QStringLiteral("a\nb")));
        QVERIFY(DisplayName::isValid(QString(32, QLatin1Char('a'))));
        QVERIFY(!DisplayName::isValid(QString(33, QLatin1Char('a'))));
        QCOMPARE(DisplayName::comparisonKey(QStringLiteral("ＡＢＣabc")), QStringLiteral("abcabc"));
    }

    // ------------------------------------------------------------ 署名

    void signatureRfc8032()
    {
        // RFC 8032 7.1 の TEST 1・TEST 2
        struct Vector { const char* seed; const char* pk; const char* msg; const char* sig; };
        const Vector vectors[] = {
            {"9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
             "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
             "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"},
            {"4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
             "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
             "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00"},
        };
        for (const Vector& v : vectors) {
            SecureBytes seed(32);
            const QByteArray seedBytes = hex(v.seed);
            std::copy(seedBytes.cbegin(), seedBytes.cend(), seed.data());
            const SigningKey key = SigningKey::fromSeed(seed);
            QVERIFY(key.isValid());
            QCOMPARE(key.publicKey(), hex(v.pk));
            const QByteArray message = hex(v.msg);
            QCOMPARE(key.sign(message), hex(v.sig));
            QVERIFY(Crypto::verify(hex(v.pk), message, hex(v.sig)));
        }
    }

    void signatureRejectsMalleableAndSmallOrder()
    {
        const QByteArray pk = hex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
        const QByteArray sig = hex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
        // S + L（正規の形でない S）
        const QByteArray malleable = sig.left(32) + addGroupOrder(sig.mid(32));
        QVERIFY(!Crypto::verify(pk, QByteArray(), malleable));
        // 位数の小さい公開鍵（単位元・位数8の点）
        const QByteArray identity = hex("0100000000000000000000000000000000000000000000000000000000000000");
        const QByteArray order8 = hex("c7176a703d4dd84fba3c0b760d10670f2a2053fa2c39ccc64ec7fd7792ac037a");
        QVERIFY(!Crypto::isValidPublicKey(identity));
        QVERIFY(!Crypto::isValidPublicKey(order8));
        const QByteArray smallOrderSignature = identity + QByteArray(32, '\0');
        QVERIFY(!Crypto::verify(identity, QByteArray("x"), smallOrderSignature));
        QVERIFY(!Crypto::verify(order8, QByteArray("x"), smallOrderSignature));
        // 位数の小さい R
        QVERIFY(!Crypto::verify(pk, QByteArray(), identity + sig.mid(32)));
        // 改変1バイト
        QByteArray tampered = sig;
        tampered[10] = static_cast<char>(tampered.at(10) ^ 1);
        QVERIFY(!Crypto::verify(pk, QByteArray(), tampered));
    }

    void signatureLabelsAreNotInterchangeable()
    {
        const P2pTest::Player black;
        const P2pTest::Player white;
        const StartAgreement a = P2pTest::makeAgreement(black, white, TimeControlTerms{});
        const QByteArray gameId = a.gameId();
        // StateApproval への署名を、同じ値の Control として通さない（ラベルが違う）
        const CborValue approval = stateApproval(gameId, 0, zeroHash());
        const QByteArray signature = black.sessionKey.sign(approval);
        Control c;
        c.gameId = gameId;
        c.connectionId = zeroHash();
        c.sequence = 1;
        c.bodyHash = zeroHash();
        QVERIFY(Crypto::verify(black.sessionKey.publicKey(), approval, signature));
        QVERIFY(!Crypto::verify(black.sessionKey.publicKey(), c.toCbor(), signature));
        // 署名者を入れ替えると通らない
        QVERIFY(!Crypto::verify(white.sessionKey.publicKey(), approval, signature));
        // 別の networkId の合意は別の gameId
        StartAgreement other = a;
        other.networkId = networkId("sbq-limited-1");
        QVERIFY(other.gameId() != gameId);
    }

    // ------------------------------------------------------------ 値

    void agreementRoundTripAndDistinctKeys()
    {
        const P2pTest::Player black;
        const P2pTest::Player white;
        StartAgreement a = P2pTest::makeAgreement(black, white, TimeControlTerms{});
        const std::optional<StartAgreement> decoded = StartAgreement::fromCbor(a.toCbor());
        QVERIFY(decoded);
        QCOMPARE(decoded->gameId(), a.gameId());
        // 同じプレイヤー同士・nonce の衝突は拒否する
        StartAgreement same = a;
        same.whitePlayerKey = same.blackPlayerKey;
        QVERIFY(!StartAgreement::fromCbor(same.toCbor()));
        StartAgreement nonce = a;
        nonce.whiteNonce = nonce.blackNonce;
        QVERIFY(!StartAgreement::fromCbor(nonce.toCbor()));
    }

    void timeControlBounds()
    {
        QVERIFY(TimeControlTerms::fromCbor(TimeControlTerms{600000, 30000}.toCbor()));
        QVERIFY(!TimeControlTerms::fromCbor(TimeControlTerms{0, 0}.toCbor()));
        QVERIFY(TimeControlTerms::fromCbor(TimeControlTerms{kMaxMainMs, kMaxByoyomiMs}.toCbor()));
        QVERIFY(!TimeControlTerms::fromCbor(TimeControlTerms{kMaxMainMs + 1, 0}.toCbor()));
        // 加算・遅延のある条件は v1 では拒否する
        const CborValue increment = CborValue::array({CborValue::text("SBQ-TIME"), CborValue::uint(1),
                                                      CborValue::uint(60000), CborValue::uint(0),
                                                      CborValue::uint(1000), CborValue::uint(0)});
        QVERIFY(!TimeControlTerms::fromCbor(increment));
    }

    void verificationCodeIsSymmetric()
    {
        const QByteArray a = Crypto::randomBytes(32);
        const QByteArray b = Crypto::randomBytes(32);
        const QStringList code = verificationCode(a, b);
        QCOMPARE(code.size(), 6);
        for (const QString& group : code) QCOMPARE(group.size(), 5);
        QCOMPARE(verificationCode(b, a), code);
    }

    void coinUsesBothNonces()
    {
        int applicantBlack = 0;
        const QByteArray nonceA = Crypto::randomBytes(32);
        for (int i = 0; i < 64; ++i) applicantBlack += applicantIsBlack(nonceA, Crypto::randomBytes(32)) ? 1 : 0;
        QVERIFY(applicantBlack > 0 && applicantBlack < 64);
        QCOMPARE(coinCommit(nonceA).size(), 32);
    }

    // ------------------------------------------------------------ 規則と時計

    void nextStateAndClock()
    {
        const TimeControlTerms terms{10000, 5000};
        const GameState s0 = GameRules::initialState(zeroHash(), terms);
        QCOMPARE(s0.sfen, Rules::initialSfen());
        // E < R + B なら指せる。基本持ち時間は max(0, R - E)
        const std::optional<GameState> s1 = GameRules::nextState(s0, UsiMove::fromString(u"7g7f"), 12000, terms);
        QVERIFY(s1);
        QCOMPARE(s1->blackMainMs, quint64(0));
        QCOMPARE(s1->whiteMainMs, quint64(10000));
        QCOMPARE(s1->sfen, QByteArray("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"));
        QCOMPARE(GameRules::checkNext(s0, *s1, terms), GameRules::StateCheck::Ok);
        // 境界と同値は時間切れ
        QVERIFY(!GameRules::nextState(s0, UsiMove::fromString(u"7g7f"), 15000, terms));
        QVERIFY(GameRules::nextState(s0, UsiMove::fromString(u"7g7f"), 14999, terms));
        // 秒読みの未使用分は持ち越さない: 基本持ち時間0の側は次も B だけ
        const std::optional<GameState> s2 = GameRules::nextState(*s1, UsiMove::fromString(u"3c3d"), 100, terms);
        QVERIFY(s2);
        QVERIFY(!GameRules::nextState(*s2, UsiMove::fromString(u"2g2f"), 5000, terms));
        // 非合法手・自分の時間の書き換え・SFEN の書き換えは検査で落とす
        QVERIFY(!GameRules::nextState(s0, UsiMove::fromString(u"7g7e"), 0, terms));
        GameState forgedClock = *s1;
        forgedClock.blackMainMs = 10000;
        QCOMPARE(GameRules::checkNext(s0, forgedClock, terms), GameRules::StateCheck::ClockMismatch);
        GameState forgedSfen = *s1;
        forgedSfen.sfen.replace("w - 2", "w - 3");
        QCOMPARE(GameRules::checkNext(s0, forgedSfen, terms), GameRules::StateCheck::SfenMismatch);
        GameState wrongPrevious = *s1;
        wrongPrevious.previousStateHash = zeroHash();
        QCOMPARE(GameRules::checkNext(s0, wrongPrevious, terms), GameRules::StateCheck::WrongPrevious);
    }

    void judgeTerminal()
    {
        GameState mated;
        mated.ply = 3;
        mated.sfen = "4k4/4G4/4S4/9/9/9/9/9/4K4 w - 4";
        const std::optional<GameRules::Terminal> mate = GameRules::judge({mated});
        QVERIFY(mate);
        QCOMPARE(mate->cause, Cause::Checkmate);
        QCOMPARE(mate->outcome, Outcome::BlackWins);

        const P2pTest::Player black;
        const P2pTest::Player white;
        const TimeControlTerms terms;
        const std::vector<SignedState> signedStates =
            P2pTest::playMoves(zeroHash(), terms, P2pTest::repetitionMoves(), black, white);
        QCOMPARE(signedStates.size(), std::size_t(13));
        std::vector<GameState> states;
        for (const SignedState& s : signedStates) states.push_back(s.state);
        const std::optional<GameRules::Terminal> repetition = GameRules::judge(states);
        QVERIFY(repetition);
        QCOMPARE(repetition->cause, Cause::Repetition);
        states.pop_back();
        QVERIFY(!GameRules::judge(states));

        GameState last;
        last.ply = kMaxPly;
        last.sfen = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 513";
        const std::optional<GameRules::Terminal> maxPly = GameRules::judge({last});
        QVERIFY(maxPly);
        QCOMPARE(maxPly->cause, Cause::MaxPly);
        QVERIFY(!GameRules::nextState(last, UsiMove::fromString(u"7g7f"), 0, terms));
    }

    // ------------------------------------------------------------ 証跡

    void evidenceMutuallySignedResign()
    {
        const P2pTest::Player black;
        const P2pTest::Player white;
        Evidence e = P2pTest::makeEvidence(black, white, TimeControlTerms{}, {QStringLiteral("7g7f"), QStringLiteral("3c3d")});
        e.result = P2pTest::resultFor(e, Outcome::WhiteWins, Cause::Resign);
        e.blackResultSignature = black.playerKey.sign(e.result->toCbor());
        e.whiteResultSignature = white.playerKey.sign(e.result->toCbor());

        const QByteArray bytes = e.encode();
        const EvidenceReport report = EvidenceVerifier::verify(bytes);
        QVERIFY2(report.isValid(), qPrintable(report.error));
        QCOMPARE(report.category, EvidenceReport::Category::MutuallySigned);
        QCOMPARE(report.lastMutualPly, 2);
        QCOMPARE(report.resultDigest, e.result->digest());
        QCOMPARE(report.networkName, QStringLiteral("sbq-local-1"));
        QCOMPARE(report.gameId, e.gameId());

        // 読み直した証跡は同じバイト列になる
        const std::optional<Evidence> decoded = Evidence::decode(bytes);
        QVERIFY(decoded);
        QCOMPARE(decoded->encode(), bytes);
    }

    void evidenceWithoutResultSignature()
    {
        const P2pTest::Player black;
        const P2pTest::Player white;
        // 投了は棋譜から決まらないので、相手の署名がなければ結果未確定
        Evidence e = P2pTest::makeEvidence(black, white, TimeControlTerms{}, {QStringLiteral("7g7f")});
        e.result = P2pTest::resultFor(e, Outcome::BlackWins, Cause::Resign);
        e.whiteResultSignature = white.playerKey.sign(e.result->toCbor());
        EvidenceReport report = EvidenceVerifier::verify(e.encode());
        QVERIFY2(report.isValid(), qPrintable(report.error));
        QCOMPARE(report.category, EvidenceReport::Category::Unconfirmed);

        // 千日手は棋譜から決まる（結果への署名がなくても）
        Evidence draw = P2pTest::makeEvidence(black, white, TimeControlTerms{}, P2pTest::repetitionMoves());
        report = EvidenceVerifier::verify(draw.encode());
        QVERIFY2(report.isValid(), qPrintable(report.error));
        QCOMPARE(report.category, EvidenceReport::Category::DeterminedByRecord);
        QVERIFY(report.terminal);
        QCOMPARE(report.terminal->cause, Cause::Repetition);

        // 千日手の局面に付いた投了は食い違いとして示す
        draw.result = P2pTest::resultFor(draw, Outcome::BlackWins, Cause::Resign);
        draw.blackResultSignature = black.playerKey.sign(draw.result->toCbor());
        draw.whiteResultSignature = white.playerKey.sign(draw.result->toCbor());
        report = EvidenceVerifier::verify(draw.encode());
        QVERIFY(report.resultConflictsWithRecord);
        QVERIFY(!report.isValid());
    }

    void evidenceRejectsTampering()
    {
        const P2pTest::Player black;
        const P2pTest::Player white;
        const P2pTest::Player stranger;
        const Evidence base = P2pTest::makeEvidence(black, white, TimeControlTerms{}, {QStringLiteral("7g7f"), QStringLiteral("3c3d")});

        // 承認の署名の改変
        Evidence e = base;
        e.states[1].whiteApproval[0] = static_cast<char>(e.states[1].whiteApproval.at(0) ^ 1);
        QVERIFY(!EvidenceVerifier::verify(e.encode()).signaturesValid);

        // 別の一局用鍵による承認
        e = base;
        e.states[2] = P2pTest::approveBoth(e.states[2].state, black, stranger);
        QVERIFY(!EvidenceVerifier::verify(e.encode()).signaturesValid);

        // 開始署名を別の鍵で
        e = base;
        e.whiteStartSignature = stranger.playerKey.sign(e.agreement.toCbor());
        QVERIFY(!EvidenceVerifier::verify(e.encode()).whiteStartSigned);

        // 他の対局の状態の流用
        const Evidence other = P2pTest::makeEvidence(black, white, TimeControlTerms{}, {QStringLiteral("7g7f"), QStringLiteral("3c3d")});
        e = base;
        e.states[2] = other.states[2];
        QVERIFY(!EvidenceVerifier::verify(e.encode()).recordLegal);

        // 承認のない状態の後に状態を続けない
        e = base;
        e.states[1].whiteApproval.clear();
        QVERIFY(!EvidenceVerifier::verify(e.encode()).recordLegal);

        // 末尾の1バイトの改変・上限を超える大きさ
        QByteArray bytes = base.encode();
        bytes[bytes.size() - 1] = static_cast<char>(bytes.at(bytes.size() - 1) ^ 1);
        QVERIFY(!EvidenceVerifier::verify(bytes).isValid());
        QVERIFY(!EvidenceVerifier::verify(QByteArray(Evidence::kMaxBytes + 1, '\0')).formatValid);
    }

    /// shogiboardq-cli verify-match の結果（ファイルから）
    void verifyMatchServiceReadsFile()
    {
        const P2pTest::Player black;
        const P2pTest::Player white;
        Evidence e = P2pTest::makeEvidence(black, white, TimeControlTerms{}, {QStringLiteral("7g7f")});
        e.result = P2pTest::resultFor(e, Outcome::BlackWins, Cause::Resign);
        e.blackResultSignature = black.playerKey.sign(e.result->toCbor());
        e.whiteResultSignature = white.playerKey.sign(e.result->toCbor());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("game.sbqmatch"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(e.encode());
        file.close();
        const auto outcome = MatchVerificationService::verifyFile(path);
        QVERIFY(std::holds_alternative<QJsonObject>(outcome));
        const QJsonObject obj = std::get<QJsonObject>(outcome);
        QCOMPARE(obj.value(QStringLiteral("valid")).toBool(), true);
        QCOMPARE(obj.value(QStringLiteral("category")).toString(), QStringLiteral("mutually_signed"));
        QCOMPARE(obj.value(QStringLiteral("game_id")).toString(), QString::fromLatin1(e.gameId().toHex()));
        QCOMPARE(obj.value(QStringLiteral("result")).toObject().value(QStringLiteral("result_digest")).toString(),
                 QString::fromLatin1(e.result->digest().toHex()));
        QVERIFY(std::holds_alternative<Failure>(MatchVerificationService::verifyFile(dir.filePath(QStringLiteral("none")))));
    }

    // ------------------------------------------------------------ フレーム

    void framePayloadsRoundTrip()
    {
        HelloPayload hello;
        hello.role = kRoleHost;
        hello.networkId = networkId("sbq-local-1");
        hello.peerId = QByteArray(38, '\x01');
        hello.connectionRandom = Crypto::randomBytes(32);
        hello.playerKey = Crypto::randomBytes(32);
        hello.sessionKey = Crypto::randomBytes(32);
        hello.displayName = QStringLiteral("将棋太郎");
        hello.offerId = zeroHash();
        hello.coin = Crypto::randomBytes(32);
        hello.timeControl = TimeControlTerms{};
        Frame frame;
        frame.type = FrameType::Hello;
        frame.requestId = 1;
        frame.gameId = zeroHash();
        frame.payload = hello.toCbor();
        const std::optional<Frame> decoded = Frame::decode(frame.encode());
        QVERIFY(decoded);
        const std::optional<HelloPayload> h = HelloPayload::fromCbor(decoded->payload);
        QVERIFY(h);
        QCOMPARE(h->displayName, hello.displayName);
        QVERIFY(h->timeControl);

        // 規則に合わない表示名・申込側の条件は拒否する
        HelloPayload badName = hello;
        badName.displayName = (QStringLiteral("a") + QChar(0x202E) + QStringLiteral("b"));
        QVERIFY(!HelloPayload::fromCbor(badName.toCbor()));
        // 先頭の BOM は QString にすると消えるが、受け取ったバイト列で判定するので通らない
        HelloPayload bom = hello;
        bom.displayName = QString(QChar(0xFEFF)) + QStringLiteral("a");
        QVERIFY(!HelloPayload::fromCbor(bom.toCbor()));
        HelloPayload applicantWithConditions = hello;
        applicantWithConditions.role = kRoleApplicant;
        QVERIFY(!HelloPayload::fromCbor(applicantWithConditions.toCbor()));

        // INVITE_TOKEN は対局・UI側では読まない
        Frame token;
        token.type = FrameType::InviteToken;
        token.gameId = zeroHash();
        token.payload = CborValue::array({CborValue::bytes(QByteArray(32, 'x'))});
        QVERIFY(Frame::isWellFormedEnvelope(token.encode()));
        QVERIFY(!Frame::decode(token.encode()));

        // CONTROL の bodyHash の不一致
        ControlPayload control;
        control.control.gameId = zeroHash();
        control.control.connectionId = zeroHash();
        control.control.sequence = 1;
        control.control.kind = ControlKind::Ready;
        control.body = ControlBody::ready(zeroHash());
        control.control.bodyHash = Cbor::hash(control.body);
        control.signature = QByteArray(64, '\0');
        QVERIFY(ControlPayload::fromCbor(control.toCbor()));
        control.body = ControlBody::ready(QByteArray(32, '\x01'));
        QVERIFY(!ControlPayload::fromCbor(control.toCbor()));
    }
};

QTEST_GUILESS_MAIN(TestP2pProtocol)
#include "tst_p2p_protocol.moc"
