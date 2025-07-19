#pragma once
#include <QJsonValue>
#include <QVector>
#include <optional>
#include <expected>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace json_query
{

class JSONPointer
{
public:
    enum class Error {
        InvalidSyntax
    };

    // Factory function mirroring JSONPath
    static std::expected<JSONPointer, Error> create(QStringView pointer);

#ifdef JSONQUERY_ENABLE_LEGACY_CONSTRUCTORS
    [[deprecated("Use JSONPointer::create instead")]]
    explicit JSONPointer(const QString& pointer) { parsePointer(pointer); }
#endif

    [[nodiscard]] QJsonValue evaluate(QJsonDocument const&) const;
    [[nodiscard]] QJsonValue evaluate(QJsonValue   const&) const;

    [[nodiscard]] QString toString() const;

private:
    JSONPointer() = default;  // internal default ctor for factory

    struct Token {
        enum class Kind : quint8 { Key, Index };
        Kind       kind;
        qsizetype  index{};
        QString    key{};
    };

    QVector<Token>   m_tokens;

    [[nodiscard]] bool        parsePointer(QStringView);
    [[nodiscard]] QJsonValue  evaluateInternal(QJsonValue const&) const;
    static void decodeAndStore(QStringView raw, QVector<Token>& out);
};

} // namespace json_query
