#pragma once
#include <QJsonValue>
#include <QVector>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class JSONPointer
{
public:
    explicit JSONPointer(const QString& pointer);

    [[nodiscard]] QJsonValue evaluate(QJsonDocument const&) const;
    [[nodiscard]] QJsonValue evaluate(QJsonValue   const&) const;

    [[nodiscard]] bool    isValid()  const noexcept { return m_valid; }
    [[nodiscard]] QString toString() const;

private:
    struct Token {
        enum class Kind : quint8 { Key, Index };
        Kind       kind;
        qsizetype  index{};
        QString    key{};
    };

    bool             m_valid {true};
    QVector<Token>   m_tokens;

    void        parsePointer(QStringView);
    [[nodiscard]] QJsonValue  evaluateInternal(QJsonValue const&) const;
    static void decodeAndStore(QStringView raw, QVector<Token>& out);
};
