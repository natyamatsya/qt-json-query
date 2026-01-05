// ──────────────────────────────────────────────────────────────────────
//  JSONPath Filter Parser Template Implementations
// ──────────────────────────────────────────────────────────────────────
//
// This file contains template function implementations for the JSONPath
// filter parser module. It is included by JSONPathFilterParsers.hpp to
// provide template instantiation while keeping the header clean.
//
// Template functions must be defined in headers (or included files) for
// proper instantiation, but separating them into .inl files improves
// organization and compilation hygiene.
// ──────────────────────────────────────────────────────────────────────

#pragma once

// ──────────────────────────────────────────────────────────────────────
//  Self-Value Comparison Templates
// ──────────────────────────────────────────────────────────────────────

template <auto PAT>
[[nodiscard]] std::optional<Token>
parseSelfValue(QString s, std::vector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s)))
    {
        const auto op  = to_qt_s(match.template get<1>().to_view());
        const auto rhs = to_qt_s(match.template get<2>().to_view());

        return parseRhsValue(op, rhs).and_then(
            [&](const ComparisonContext& ctx) -> std::optional<Token>
            {
                Builder b{out};
                return b.add(
                    [ctx](const QJsonValue& j)
                    {
                        return ctx.compare(j);
                    },
                    s);
            });
    }
    return std::nullopt;
}

// ──────────────────────────────────────────────────────────────────────
//  Property Comparison Templates
// ──────────────────────────────────────────────────────────────────────

template <auto PAT>
[[nodiscard]] std::optional<Token>
parseCompare1(QString s, std::vector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s)))
    {
        const auto prop = to_qt_s(match.template get<1>().to_view());
        const auto op   = to_qt_s(match.template get<2>().to_view());
        const auto rhs  = to_qt_s(match.template get<3>().to_view());

        return parseRhsValue(op, rhs).and_then(
            [&](const ComparisonContext& ctx) -> std::optional<Token>
            {
                Builder b{out};
                return b.add(
                    [prop, ctx](const QJsonValue& j)
                    {
                        const auto obj{j.toObject()};
                        const auto v{obj.value(prop)};
                        return ctx.compare(v);
                    },
                    prop);
            });
    }
    return std::nullopt;
}

template <auto PAT>
[[nodiscard]] std::optional<Token>
parseCompareIndex(QString s, std::vector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s)))
    {
        const auto prop = to_qt_s(match.template get<1>().to_view());
        const auto op   = to_qt_s(match.template get<2>().to_view());
        const auto rhs  = to_qt_s(match.template get<3>().to_view());

        return parseRhsValue(op, rhs).and_then(
            [&](const ComparisonContext& ctx) -> std::optional<Token>
            {
                Builder b{out};
                return b.add(
                    [prop, ctx](const QJsonValue& j)
                    {
                        bool ok;
                        auto index{prop.toInt(&ok)};
                        if (!ok)
                            return false; // Invalid index

                        if (j.isArray())
                        {
                            const auto arr{j.toArray()};
                            if (index < 0 || index >= arr.size())
                            {
                                // Out of bounds: compare with undefined/null
                                QJsonValue undefined; // QJsonValue::Undefined
                                return ctx.compare(undefined);
                            }
                            else
                            {
                                const auto v{arr[index]};
                                return ctx.compare(v);
                            }
                        }
                        else
                        {
                            // Non-arrays don't have array indices: compare with undefined/null
                            QJsonValue undefined; // QJsonValue::Undefined
                            return ctx.compare(undefined);
                        }
                    },
                    QString("@[%1]").arg(prop));
            });
    }
    return std::nullopt;
}

// ──────────────────────────────────────────────────────────────────────
//  Regex Pattern Templates
// ──────────────────────────────────────────────────────────────────────

template <auto PAT>
std::optional<Token> parseRegex1(QString s, std::vector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s)))
    {
        const auto prop = to_qt_s(match.template get<1>().to_view());
        auto pattern = to_qt_s(match.template get<2>().to_view());

        // Validate regex pattern
        const QRegularExpression regex(pattern);
        if (!regex.isValid())
            return std::nullopt;

        return Builder{out}.add(
            [prop, regex](const QJsonValue& j)
            {
                const auto obj{j.toObject()};
                const auto v{obj.value(prop)};
                if (!v.isString())
                    return false;
                return regex.match(v.toString()).hasMatch();
            },
            prop);
    }
    return std::nullopt;
}

// ──────────────────────────────────────────────────────────────────────
//  Self-Comparison Templates
// ──────────────────────────────────────────────────────────────────────

template <auto PAT>
[[nodiscard]] std::optional<Token>
parseSelfCompare(QString s, std::vector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s)))
    {
        const auto prop = to_qt_s(match.template get<1>().to_view());
        const auto op   = to_qt_s(match.template get<2>().to_view());

        Builder b{out};
        return b.add(
            [prop, op](const QJsonValue& j)
            {
                const auto obj{j.toObject()};
                const auto v{obj.value(prop)};
                return performComparison(v, op, j);
            },
            QString("%1%2@").arg(prop, op));
    }
    return std::nullopt;
}

template <auto PAT>
[[nodiscard]] std::optional<Token>
parseSelfCompareIndex(QString s, std::vector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s)))
    {
        const auto prop = to_qt_s(match.template get<1>().to_view());
        const auto op   = to_qt_s(match.template get<2>().to_view());

        Builder b{out};
        return b.add(
            [prop, op](const QJsonValue& j)
            {
                bool ok;
                auto index{prop.toInt(&ok)};
                if (!ok)
                    return false; // Invalid index

                if (j.isArray())
                {
                    const auto arr{j.toArray()};
                    if (index < 0 || index >= arr.size())
                    {
                        // Out of bounds: compare with undefined
                        QJsonValue undefined; // QJsonValue::Undefined
                        return performComparison(undefined, op, j);
                    }
                    else
                    {
                        const auto v{arr[index]};
                        return performComparison(v, op, j);
                    }
                }
                else
                {
                    // Non-arrays don't have array indices: compare with undefined
                    QJsonValue undefined; // QJsonValue::Undefined
                    return performComparison(undefined, op, j);
                }
            },
            QString("@[%1]%2@").arg(prop, op));
    }
    return std::nullopt;
}

// ──────────────────────────────────────────────────────────────────────
//  Null Comparison Templates
// ──────────────────────────────────────────────────────────────────────

template <auto PAT>
[[nodiscard]] std::optional<Token>
parseNullCompare(QString s, std::vector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s)))
    {
        const auto prop = to_qt_s(match.template get<1>().to_view());
        const auto op   = to_qt_s(match.template get<2>().to_view());

        Builder b{out};
        return b.add(
            [prop, op](const QJsonValue& j)
            {
                const auto obj{j.toObject()};
                const auto v{obj.value(prop)};
                return performComparison(v, op, QJsonValue{QJsonValue::Null});
            },
            prop);
    }
    return std::nullopt;
}

template <auto PAT>
[[nodiscard]] std::optional<Token>
parseNullCompareIndex(QString s, std::vector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s)))
    {
        const auto prop = to_qt_s(match.template get<1>().to_view());
        const auto op   = to_qt_s(match.template get<2>().to_view());

        Builder b{out};
        return b.add(
            [prop, op](const QJsonValue& j)
            {
                bool ok;
                auto index{prop.toInt(&ok)};
                if (!ok)
                    return false; // Invalid index

                if (j.isArray())
                {
                    const auto arr{j.toArray()};
                    if (index < 0 || index >= arr.size())
                    {
                        // Out of bounds: compare with undefined/null
                        QJsonValue undefined; // QJsonValue::Undefined
                        return performComparison(undefined, op, QJsonValue{QJsonValue::Null});
                    }
                    else
                    {
                        const auto v{arr[index]};
                        return performComparison(v, op, QJsonValue{QJsonValue::Null});
                    }
                }
                else
                {
                    // Non-arrays don't have array indices: compare with undefined/null
                    QJsonValue undefined; // QJsonValue::Undefined
                    return performComparison(undefined, op, QJsonValue{QJsonValue::Null});
                }
            },
            QString("@[%1]").arg(prop));
    }
    return std::nullopt;
}

// ──────────────────────────────────────────────────────────────────────
//  Embedded Filter Parser Templates (Zero-Overhead)
// ──────────────────────────────────────────────────────────────────────

template <ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedCompare1(const QString& s)
{
    if (auto m = ctre::match<Pattern>(to_sv(s)))
    {
        const auto prop = to_qt_s(m.template get<1>().to_view());
        const auto op   = to_qt_s(m.template get<2>().to_view());
        const auto rhs  = to_qt_s(m.template get<3>().to_view());

        // Parse RHS value using existing comparison context logic
        auto ctx{parseRhsValue(op, rhs)};
        if (!ctx)
            return std::nullopt;

        Token token;
        token.kind = Token::Kind::Filter;
        token.key  = s;

        // Embed filter directly into token
        token.embedFilter(
            [prop, ctx = *ctx](const QJsonValue& j)
            {
                const auto obj{j.toObject()};
                const auto v{obj.value(prop)};
                return ctx.compare(v);
            });

        return token;
    }
    return std::nullopt;
}

template <ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedCompareIndex(const QString& s)
{
    if (auto m = ctre::match<Pattern>(to_sv(s)))
    {
        const auto prop = to_qt_s(m.template get<1>().to_view());
        const auto op   = to_qt_s(m.template get<2>().to_view());
        const auto rhs  = to_qt_s(m.template get<3>().to_view());

        // Parse RHS value using existing comparison context logic
        auto ctx{parseRhsValue(op, rhs)};
        if (!ctx)
            return std::nullopt;

        Token token;
        token.kind = Token::Kind::Filter;
        token.key  = s;

        // Embed filter directly into token
        token.embedFilter(
            [prop, ctx = *ctx](const QJsonValue& j)
            {
                bool ok;
                auto index{prop.toInt(&ok)};
                if (!ok)
                    return false; // Invalid index

                if (j.isArray())
                {
                    const auto arr{j.toArray()};
                    if (index < 0 || index >= arr.size())
                    {
                        // Out of bounds: compare with undefined/null
                        QJsonValue undefined; // QJsonValue::Undefined
                        return ctx.compare(undefined);
                    }
                    else
                    {
                        const auto v{arr[index]};
                        return ctx.compare(v);
                    }
                }
                else
                {
                    // Non-arrays don't have array indices: compare with undefined/null
                    QJsonValue undefined; // QJsonValue::Undefined
                    return ctx.compare(undefined);
                }
            });

        return token;
    }
    return std::nullopt;
}

template <ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedSelfValue(const QString& s)
{
    if (auto m = ctre::match<Pattern>(to_sv(s)))
    {
        const auto op  = to_qt_s(m.template get<1>().to_view());
        const auto rhs = to_qt_s(m.template get<2>().to_view());

        // Parse RHS value using existing comparison context logic
        auto ctx{parseRhsValue(op, rhs)};
        if (!ctx)
            return std::nullopt;

        Token token;
        token.kind = Token::Kind::Filter;
        token.key  = s;

        // Embed filter directly into token
        token.embedFilter([ctx = *ctx](const QJsonValue& j) { return ctx.compare(j); });

        return token;
    }
    return std::nullopt;
}

template <ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedRegex1(const QString& s)
{
    if (auto m = ctre::match<Pattern>(to_sv(s)))
    {
        const auto prop = to_qt_s(m.template get<1>().to_view());
        auto pattern = to_qt_s(m.template get<2>().to_view());

        // Validate regex pattern
        const QRegularExpression regex(pattern);
        if (!regex.isValid())
            return std::nullopt;

        Token token;
        token.kind = Token::Kind::Filter;
        token.key  = s;

        // Embed filter directly into token
        token.embedFilter(
            [prop, regex](const QJsonValue& j)
            {
                const auto obj{j.toObject()};
                const auto v{obj.value(prop)};
                if (!v.isString())
                    return false;
                return regex.match(v.toString()).hasMatch();
            });

        return token;
    }
    return std::nullopt;
}
