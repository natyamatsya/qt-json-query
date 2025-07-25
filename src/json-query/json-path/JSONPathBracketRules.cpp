#include "json-query/json-path/JSONPathBracketRules.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathParseUtils.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/json-path/JSONPathFilter.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"
#include "json-query/json-path/internal/QtHash.hpp"

#include <QDebug>
#include <QJsonValue>

namespace json_query::json_path::detail
{

// ──────────────────────────────────────────────────────────────────────
//  BracketSink Implementation
// ──────────────────────────────────────────────────────────────────────

std::expected<void, Error> BracketSink::key(QString key, bool allow)
{
    // Create token with bracket group ID
    if (!allow && key.contains(u' '))
        return std::unexpected(Error::BlankInKey);
    Token t{Token::Kind::Key, 0, {}, qt_hash(key), key};
    t.bracketGroupId = currentBracketGroupId;
    tk.append(std::move(t));
    return {};
}

void BracketSink::keyList(const QVector<QString>& keys)
{
    if (keys.isEmpty()) return;

    Token t;
    t.kind = Token::Kind::KeyList;
    t.bracketGroupId = currentBracketGroupId; // Set bracket group ID

    // Pack the keys into a single QString separated by '\n' so that the
    // evaluator can split them later without ambiguity.
    QStringList list;
    list.reserve(keys.size());
    for (const QString& k : keys)
        list.append(k);

    t.key = list.join(u"\n");
    tk.append(std::move(t));
}

void BracketSink::wild()
{
    Token t{Token::Kind::Wildcard};
    t.bracketGroupId = currentBracketGroupId;
    tk.append(t);
}

void BracketSink::slice(const Slice& s)
{
    Token t{Token::Kind::Slice, 0, s, 0u};
    t.bracketGroupId = currentBracketGroupId;
    tk.append(t);
}

void BracketSink::index(int i)
{
    qCDebug(jsonPathLog) << "BracketSink::index emit" << i;
    Token t{Token::Kind::Index, i};
    t.bracketGroupId = currentBracketGroupId;
    tk.append(t);
}

void BracketSink::pushFilter(const Token& t)
{
    Token copy = t;
    copy.bracketGroupId = currentBracketGroupId;
    tk.append(copy);
}

// ──────────────────────────────────────────────────────────────────────
//  Rule Matcher Functions Implementation
// ──────────────────────────────────────────────────────────────────────

namespace matchers {

bool matchesUnionComma(QStringView content)
{
    return content.contains(u',');
}

bool matchesWildcard(QStringView content)
{
    return content.trimmed() == u"*";
}

bool matchesSingleIndex(QStringView content)
{
    return isValidIndexLiteral(content);
}

bool matchesIndexList(QStringView content)
{
    if (!content.contains(u',')) return false;
    
    const auto parts = content.split(u',');
    if (parts.size() < 2) return false;
    
    for (const auto& part : parts) {
        if (!isValidIndexLiteral(part)) return false;
    }
    return true;
}

bool matchesSlice(QStringView content)
{
    return content.contains(u':') && makeSlice(content).has_value();
}

bool matchesFilterWithParens(QStringView content)
{
    return content.trimmed().startsWith(u"?(") && content.trimmed().endsWith(u")");
}

bool matchesFilterWithoutParens(QStringView content)
{
    QStringView trimmed = content.trimmed();
    if (!trimmed.startsWith(u'?')) return false;
    if (trimmed.startsWith(u"?(")) return false; // This should be handled by matchesFilterWithParens
    
    // Check for nested brackets and quotes to distinguish top-level commas from nested commas
    int bracketLevel = 0;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escaped = false;
    
    for (qsizetype i = 0; i < trimmed.size(); ++i) {
        QChar c = trimmed[i];
        
        if (escaped) {
            escaped = false;
            continue;
        }
        
        if (c == u'\\') {
            escaped = true;
            continue;
        }
        
        if (!inSingleQuote && !inDoubleQuote) {
            if (c == u'[') {
                bracketLevel++;
            } else if (c == u']') {
                bracketLevel--;
            } else if (c == u'\'') {
                inSingleQuote = true;
            } else if (c == u'"') {
                inDoubleQuote = true;
            } else if (c == u',' && bracketLevel == 0) {
                // Top-level comma found - this should be handled as union, not filter
                return false;
            }
        } else if (inSingleQuote && c == u'\'') {
            inSingleQuote = false;
        } else if (inDoubleQuote && c == u'"') {
            inDoubleQuote = false;
        }
    }
    
    return true;
}

bool matchesPlaceholder(QStringView content)
{
    QStringView trimmed = content.trimmed();
    if (trimmed == u"?") return true;
    
    // Check for multiple placeholders separated by commas
    const auto parts = trimmed.split(u',');
    for (const auto& part : parts) {
        if (part.trimmed() != u"?") return false;
    }
    return true;
}

bool matchesQuotedKey(QStringView content)
{
    QStringView trimmed = content.trimmed();
    return (trimmed.startsWith(u'\'') && trimmed.endsWith(u'\'')) ||
           (trimmed.startsWith(u'"') && trimmed.endsWith(u'"'));
}

bool matchesUnquotedKey(QStringView /*content*/)
{
    // Unquoted keys are forbidden by RFC 9535
    return true; // This will be the fallback that returns an error
}

} // namespace matchers

// ──────────────────────────────────────────────────────────────────────
//  Rule Handler Functions Implementation
// ──────────────────────────────────────────────────────────────────────

namespace handlers {

std::expected<void, Error> handleWildcard(QStringView /*content*/, BracketSink& out)
{
    out.wild();
    return {};
}

std::expected<void, Error> handleSingleIndex(QStringView content, BracketSink& out)
{
    QStringView trimmed = content.trimmed();
    bool ok = false;
    int index = trimmed.toInt(&ok);
    if (!ok) {
        qCDebug(jsonPathLog) << "handleSingleIndex: failed to parse" << trimmed;
        return std::unexpected(Error::InvalidIndex);
    }
    
    qCDebug(jsonPathLog) << "handleSingleIndex: parsed index" << index;
    out.index(index);
    return {};
}

std::expected<void, Error> handleIndexList(QStringView content, BracketSink& out)
{
    const auto parts = content.split(u',');
    for (const auto& part : parts) {
        QStringView trimmed = part.trimmed();
        bool ok = false;
        int index = trimmed.toInt(&ok);
        if (!ok) {
            qCDebug(jsonPathLog) << "handleIndexList: failed to parse" << trimmed;
            return std::unexpected(Error::InvalidIndex);
        }
        out.index(index);
    }
    return {};
}

std::expected<void, Error> handleSlice(QStringView content, BracketSink& out)
{
    auto slice = makeSlice(content);
    if (!slice) {
        return std::unexpected(Error::InvalidSlice);
    }
    out.slice(*slice);
    return {};
}

std::expected<void, Error> handleFilterWithParens(QStringView content, BracketSink& out)
{
    QStringView trimmed = content.trimmed();
    // Extract the full expression after the '?' prefix
    QString expr = trimmed.mid(1).toString(); // Remove '?' prefix
    
    qCDebug(jsonPathLog) << "handleFilterWithParens: processing expression" << expr;
    
    if (auto token = compileContextFilter(expr, out.contextFilters, out.filters)) {
        out.pushFilter(*token);
        return {};
    } else if (auto token = compileFilter(expr, out.filters)) {
        out.pushFilter(*token);
        return {};
    } else {
        qCDebug(jsonPathLog) << "handleFilterWithParens: failed to compile filter" << expr;
        return std::unexpected(Error::UnsupportedFilter);
    }
}

std::expected<void, Error> handleFilterWithoutParens(QStringView content, BracketSink& out)
{
    QStringView trimmed = content.trimmed();
    // Extract the full expression after the '?' prefix
    QString expr = trimmed.mid(1).toString(); // Remove '?' prefix
    
    qCDebug(jsonPathLog) << "handleFilterWithoutParens: processing expression" << expr;
    
    if (auto token = compileContextFilter(expr, out.contextFilters, out.filters)) {
        out.pushFilter(*token);
        return {};
    } else if (auto token = compileFilter(expr, out.filters)) {
        out.pushFilter(*token);
        return {};
    } else {
        qCDebug(jsonPathLog) << "handleFilterWithoutParens: failed to compile filter" << expr;
        return std::unexpected(Error::UnsupportedFilter);
    }
}

std::expected<void, Error> handlePlaceholder(QStringView content, BracketSink& out)
{
    QStringView trimmed = content.trimmed();
    
    if (trimmed == u"?") {
        // Single placeholder - create a filter that always returns true
        FilterFn alwaysTrue = [](const QJsonValue&) { return true; };
        out.filters.append(alwaysTrue);
        
        Token filterToken;
        filterToken.kind = Token::Kind::Filter;
        filterToken.filterId = out.filters.size() - 1;
        out.pushFilter(filterToken);
        return {};
    }
    
    // Multiple placeholders - handle each one
    const auto parts = trimmed.split(u',');
    for (const auto& part : parts) {
        if (part.trimmed() == u"?") {
            FilterFn alwaysTrue = [](const QJsonValue&) { return true; };
            out.filters.append(alwaysTrue);
            
            Token filterToken;
            filterToken.kind = Token::Kind::Filter;
            filterToken.filterId = out.filters.size() - 1;
            out.pushFilter(filterToken);
        }
    }
    return {};
}

std::expected<void, Error> handleQuotedKey(QStringView content, BracketSink& out)
{
    QStringView trimmed = content.trimmed();
    
    if (trimmed.size() < 2) {
        return std::unexpected(Error::InvalidSlice);
    }
    
    QChar quote = trimmed.front();
    QStringView keyContent = trimmed.mid(1, trimmed.size() - 2);
    
    QuoteStyle style = (quote == u'\'') ? QuoteStyle::Single : QuoteStyle::Double;
    if (!isValidQuotedKey(keyContent, style)) {
        return std::unexpected(Error::InvalidSlice);
    }
    
    QString unescapedKey = unescapeQuotedKey(keyContent);
    return out.key(unescapedKey, true); // Allow spaces in quoted keys
}

std::expected<void, Error> handleUnquotedKey(QStringView /*content*/, BracketSink& /*out*/)
{
    // Unquoted keys are forbidden by RFC 9535
    return std::unexpected(Error::UnsupportedFilter);
}

// Forward declaration for union handler
std::expected<void, Error> handleUnionComma(QStringView content, BracketSink& out)
{
    qCDebug(jsonPathLog) << "handleUnionComma: processing" << content.toString();
    
    auto segmentsResult = splitTopLevelMultiple(content, QLatin1StringView(","));
    if (!segmentsResult) {
        qCDebug(jsonPathLog) << "handleUnionComma: failed to split content";
        return std::unexpected(Error::UnsupportedFilter);
    }
    
    const auto& segments = *segmentsResult;
    qCDebug(jsonPathLog) << "handleUnionComma: split into" << segments.size() << "segments";
    
    for (const QString& segment : segments) {
        QStringView segmentView(segment);
        qCDebug(jsonPathLog) << "handleUnionComma: processing segment" << segmentView.toString();
        
        // Use dispatcher to process each segment, but exclude union rule to prevent recursion
        auto result = BracketRuleDispatcher::processSegmentExcludingUnion(segmentView, out);
        if (!result) {
            qCDebug(jsonPathLog) << "handleUnionComma: failed to process segment" << segmentView.toString();
            return result;
        }
    }
    
    qCDebug(jsonPathLog) << "handleUnionComma: successfully processed all segments";
    return {};
}

} // namespace handlers

// ──────────────────────────────────────────────────────────────────────
//  BracketRuleDispatcher Implementation
// ──────────────────────────────────────────────────────────────────────

std::vector<BracketRuleMetadata> BracketRuleDispatcher::createRules()
{
    return {
        {
            .name = "filter_with_parens",
            .priority = 1200,  // Highest priority for filter expressions
            .matcher = matchers::matchesFilterWithParens,
            .handler = handlers::handleFilterWithParens,
            .description = "Filter expression with parentheses (e.g., '?(@.a == 1)')"
        },
        {
            .name = "filter_without_parens",
            .priority = 1100,  // Second highest priority for filter expressions
            .matcher = matchers::matchesFilterWithoutParens,
            .handler = handlers::handleFilterWithoutParens,
            .description = "Filter expression without parentheses (e.g., '?@.a == 1')"
        },
        {
            .name = "union_comma",
            .priority = 1000,
            .matcher = matchers::matchesUnionComma,
            .handler = handlers::handleUnionComma,
            .description = "Union with comma-separated selectors (e.g., 'a,b,c')"
        },
        {
            .name = "wildcard",
            .priority = 900,
            .matcher = matchers::matchesWildcard,
            .handler = handlers::handleWildcard,
            .description = "Wildcard selector (*)"
        },
        {
            .name = "index_list",
            .priority = 850,
            .matcher = matchers::matchesIndexList,
            .handler = handlers::handleIndexList,
            .description = "Comma-separated index list (e.g., '1,2,3')"
        },
        {
            .name = "single_index",
            .priority = 800,
            .matcher = matchers::matchesSingleIndex,
            .handler = handlers::handleSingleIndex,
            .description = "Single array index (e.g., '123')"
        },
        {
            .name = "slice",
            .priority = 700,
            .matcher = matchers::matchesSlice,
            .handler = handlers::handleSlice,
            .description = "Array slice (e.g., '1:3:2')"
        },
        {
            .name = "placeholder",
            .priority = 500,
            .matcher = matchers::matchesPlaceholder,
            .handler = handlers::handlePlaceholder,
            .description = "Placeholder filter (e.g., '?' or '?,?')"
        },
        {
            .name = "quoted_key",
            .priority = 400,
            .matcher = matchers::matchesQuotedKey,
            .handler = handlers::handleQuotedKey,
            .description = "Quoted string key (e.g., \"'key'\" or '\"key\"')"
        },
        {
            .name = "unquoted_key",
            .priority = 100,
            .matcher = matchers::matchesUnquotedKey,
            .handler = handlers::handleUnquotedKey,
            .description = "Unquoted key (forbidden by RFC 9535)"
        }
    };
}

const std::vector<BracketRuleMetadata>& BracketRuleDispatcher::getRules()
{
    static const auto rules = createRules();
    return rules;
}

std::expected<void, Error> BracketRuleDispatcher::dispatch(QStringView content, BracketSink& sink)
{
    qCDebug(jsonPathLog) << "BracketRuleDispatcher::dispatch content=" << content.toString();
    
    // Apply rules in priority order using monadic error handling
    for (const auto& rule : getRules()) {
        qCDebug(jsonPathLog) << "Trying rule:" << rule.name << "(priority:" << rule.priority << ")";
        
        if (rule.matcher(content)) {
            qCDebug(jsonPathLog) << "Rule" << rule.name << "matched, applying handler";
            auto result = rule.handler(content, sink);
            
            if (result) {
                qCDebug(jsonPathLog) << "Rule" << rule.name << "succeeded";
                return {};
            } else {
                qCDebug(jsonPathLog) << "Rule" << rule.name << "failed with error" << static_cast<int>(result.error());
                return result;
            }
        } else {
            qCDebug(jsonPathLog) << "Rule" << rule.name << "did not match";
        }
    }
    
    qCDebug(jsonPathLog) << "No rules matched, returning UnsupportedFilter";
    return std::unexpected(Error::UnsupportedFilter);
}

std::expected<void, Error> BracketRuleDispatcher::processSegmentExcludingUnion(QStringView content, BracketSink& sink)
{
    // Apply all rules except union_comma to prevent recursion
    for (const auto& rule : getRules()) {
        if (std::string_view(rule.name) == "union_comma") continue; // Skip union rule
        
        if (rule.matcher(content)) {
            return rule.handler(content, sink);
        }
    }
    return std::unexpected(Error::UnsupportedFilter);
}

const BracketRuleMetadata* BracketRuleDispatcher::findRuleByName(const char* name)
{
    for (const auto& rule : getRules()) {
        if (std::string_view(rule.name) == name) {
            return &rule;
        }
    }
    return nullptr;
}

} // namespace json_query::json_path::detail
