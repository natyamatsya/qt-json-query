#include <iostream>
#include <json-query/json-path/JSONPath.hpp>
#include <json-query/json-path/JSONPathEvalError.hpp>
#include <json-query/json-path/JSONPathCompile.hpp>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

using namespace json_query;

void analyzeTerminology()
{
    std::cout << "=== RFC 9535 Terminology Compliance Audit ===" << std::endl;

    std::cout << "\n### CURRENT ERROR MESSAGES ###" << std::endl;

    // Evaluation Error Messages
    std::cout << "\n** Evaluation Errors (EvalError) **" << std::endl;
    std::cout << "TypeMismatchObject: \"" << json_path::to_string(json_path::EvalError::TypeMismatchObject) << "\""
              << std::endl;
    std::cout << "TypeMismatchArray:  \"" << json_path::to_string(json_path::EvalError::TypeMismatchArray) << "\""
              << std::endl;
    std::cout << "KeyNotFound:        \"" << json_path::to_string(json_path::EvalError::KeyNotFound) << "\""
              << std::endl;
    std::cout << "IndexOutOfRange:    \"" << json_path::to_string(json_path::EvalError::IndexOutOfRange) << "\""
              << std::endl;
    std::cout << "InvalidSlice:       \"" << json_path::to_string(json_path::EvalError::InvalidSlice) << "\""
              << std::endl;

    // Compilation Error Messages
    std::cout << "\n** Compilation Errors (Error) **" << std::endl;
    std::cout << "MissingRoot:        \"" << json_path::toString(json_path::Error::MissingRoot) << "\"" << std::endl;
    std::cout << "TrailingDot:        \"" << json_path::toString(json_path::Error::TrailingDot) << "\"" << std::endl;
    std::cout << "TrailingRecursive:  \"" << json_path::toString(json_path::Error::TrailingRecursive) << "\""
              << std::endl;
    std::cout << "EmptySegment:       \"" << json_path::toString(json_path::Error::EmptySegment) << "\"" << std::endl;
    std::cout << "BlankInKey:         \"" << json_path::toString(json_path::Error::BlankInKey) << "\"" << std::endl;
    std::cout << "UnmatchedBracket:   \"" << json_path::toString(json_path::Error::UnmatchedBracket) << "\""
              << std::endl;
    std::cout << "UnmatchedQuote:     \"" << json_path::toString(json_path::Error::UnmatchedQuote) << "\""
              << std::endl;
    std::cout << "UnsupportedFilter:  \"" << json_path::toString(json_path::Error::UnsupportedFilter) << "\""
              << std::endl;
    std::cout << "InvalidSlice:       \"" << json_path::toString(json_path::Error::InvalidSlice) << "\"" << std::endl;
    std::cout << "InvalidIndex:       \"" << json_path::toString(json_path::Error::InvalidIndex) << "\"" << std::endl;
    std::cout << "InvalidIdentifier:  \"" << json_path::toString(json_path::Error::InvalidIdentifier) << "\""
              << std::endl;
    std::cout << "UnexpectedAfterRoot:\"" << json_path::toString(json_path::Error::UnexpectedAfterRoot) << "\""
              << std::endl;

    std::cout << "\n### RFC 9535 TERMINOLOGY ANALYSIS ###" << std::endl;

    std::cout << "\n** RFC 9535 Key Terms **" << std::endl;
    std::cout << "- 'selector' (name-selector, index-selector, slice-selector, etc.)" << std::endl;
    std::cout << "- 'member name' (instead of 'key')" << std::endl;
    std::cout << "- 'member value' (value in name/value pair)" << std::endl;
    std::cout << "- 'element' (array element)" << std::endl;
    std::cout << "- 'index' (array index)" << std::endl;
    std::cout << "- 'segment' (child segment, descendant segment)" << std::endl;
    std::cout << "- 'nodelist' (result of selector)" << std::endl;
    std::cout << "- 'root identifier' ($ symbol)" << std::endl;
    std::cout << "- 'current node identifier' (@ symbol)" << std::endl;

    std::cout << "\n** RFC 9535 Error Semantics **" << std::endl;
    std::cout << "- Index selectors: 'Nothing is selected, and it is not an error, if the index lies outside the "
                 "range of the array'"
              << std::endl;
    std::cout << "- Name selectors: 'Nothing is selected from a value that is not an object'" << std::endl;
    std::cout << "- Slice selectors: 'It selects no nodes from a node that is not an array'" << std::endl;
    std::cout << "- Step = 0: 'When step = 0, no elements are selected, and the result array is empty'" << std::endl;

    std::cout << "\n### RECOMMENDATIONS ###" << std::endl;
    std::cout << "1. Consider using 'member name' instead of 'key' in error messages" << std::endl;
    std::cout << "2. Consider using 'selector' terminology in error messages" << std::endl;
    std::cout << "3. Align with RFC 9535's permissive language ('nothing is selected' vs 'error')" << std::endl;
    std::cout << "4. Use 'element' for array items, 'member value' for object values" << std::endl;
    std::cout << "5. Consider 'segment' terminology for path components" << std::endl;
}

int main()
{
    analyzeTerminology();
    return 0;
}
