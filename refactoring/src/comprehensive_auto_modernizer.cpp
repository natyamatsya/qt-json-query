// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

// Comprehensive Auto Modernization Tool
// Systematically replaces explicit types with 'auto' where safe and beneficial
// Based on the successful comprehensive_variable_modernizer.cpp approach

#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <vector>

struct AutoTransformation
{
    std::regex  pattern;
    std::string replacement;
    std::string description;
    bool        isSafe;
};

class ComprehensiveAutoModernizer
{
  private:
    std::vector<AutoTransformation> transformations;

  public:
    ComprehensiveAutoModernizer() { initializeTransformations(); }

    void initializeTransformations()
    {
        // COMPREHENSIVE SAFE PATTERNS for auto modernization

        // 1. Static const explicit types with assignment
        transformations.push_back(
            {std::regex(
                 R"(^(\s*)static\s+const\s+((?:std::)?(?:string|vector|array|map|set|unordered_map|unordered_set|pair|tuple|shared_ptr|unique_ptr|optional|QJsonObject|QJsonArray|QJsonValue|QString|QStringView|QByteArray|QVariant|qsizetype|size_t|int|double|float|bool|uint32_t|uint64_t|int32_t|int64_t)(?:<[^>]*>)?(?:\s*\*)?)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);$)"),
             "$1static const auto $3 = $4;",
             "static const explicit type",
             true});

        // 2. Const explicit types with assignment
        transformations.push_back(
            {std::regex(
                 R"(^(\s*)const\s+((?:std::)?(?:string|vector|array|map|set|unordered_map|unordered_set|pair|tuple|shared_ptr|unique_ptr|optional|QJsonObject|QJsonArray|QJsonValue|QString|QStringView|QByteArray|QVariant|qsizetype|size_t|int|double|float|bool|uint32_t|uint64_t|int32_t|int64_t)(?:<[^>]*>)?(?:\s*\*)?)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);$)"),
             "$1const auto $3 = $4;",
             "const explicit type",
             true});

        // 3. Basic explicit types with assignment (non-const)
        transformations.push_back(
            {std::regex(
                 R"(^(\s*)((?:std::)?(?:string|vector|array|map|set|unordered_map|unordered_set|pair|tuple|shared_ptr|unique_ptr|optional|QJsonObject|QJsonArray|QJsonValue|QString|QStringView|QByteArray|QVariant|qsizetype|size_t|int|double|float|bool|uint32_t|uint64_t|int32_t|int64_t)(?:<[^>]*>)?(?:\s*\*)?)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);$)"),
             "$1auto $3 = $4;",
             "explicit type",
             true});

        // 4. Iterator types (very common and safe)
        transformations.push_back(
            {std::regex(
                 R"(^(\s*)([a-zA-Z_][a-zA-Z0-9_:]*::(?:const_)?iterator)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);$)"),
             "$1auto $3 = $4;",
             "iterator type",
             true});

        // 5. Function pointer types with explicit casting
        transformations.push_back(
            {std::regex(
                 R"(^(\s*)([a-zA-Z_][a-zA-Z0-9_:]*\*)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*((?:static_cast|reinterpret_cast|const_cast|dynamic_cast)<[^>]+>\([^)]+\));$)"),
             "$1auto $3 = $4;",
             "cast to pointer type",
             true});

        // 6. Template instantiation types
        transformations.push_back(
            {std::regex(R"(^(\s*)([a-zA-Z_][a-zA-Z0-9_:]*<[^>]+>)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);$)"),
             "$1auto $3 = $4;",
             "template instantiation",
             true});

        // 7. Smart pointer types
        transformations.push_back(
            {std::regex(
                 R"(^(\s*)((?:std::)?(?:shared_ptr|unique_ptr|weak_ptr)<[^>]+>)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);$)"),
             "$1auto $3 = $4;",
             "smart pointer type",
             true});

        // 8. Qt types (common in this codebase)
        transformations.push_back(
            {std::regex(
                 R"(^(\s*)(Q(?:JsonObject|JsonArray|JsonValue|String|StringView|ByteArray|Variant))\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);$)"),
             "$1auto $3 = $4;",
             "Qt type",
             true});

        // 9. Standard library container types
        transformations.push_back(
            {std::regex(
                 R"(^(\s*)(std::(?:vector|array|map|set|unordered_map|unordered_set|list|deque|forward_list|stack|queue|priority_queue)<[^>]+>)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);$)"),
             "$1auto $3 = $4;",
             "STL container type",
             true});

        // 10. Numeric types with explicit casting
        transformations.push_back(
            {std::regex(
                 R"(^(\s*)((?:int|double|float|bool|size_t|qsizetype|uint32_t|uint64_t|int32_t|int64_t))\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*(static_cast<[^>]+>\([^)]+\));$)"),
             "$1auto $3 = $4;",
             "numeric type with cast",
             true});
    }

    bool isUnsafeContext(const std::string& line)
    {
        // Skip lines that are in unsafe contexts

        // Skip if/while/for conditions (could change semantics)
        if (std::regex_search(line, std::regex(R"(^\s*(?:if|while|for)\s*\()")))
            return true;

        // Skip function parameters
        if (std::regex_search(line, std::regex(R"(^\s*[a-zA-Z_][a-zA-Z0-9_]*\s*\([^)]*$)")))
            return true;

        // Skip template parameters
        if (std::regex_search(line, std::regex(R"(^\s*template\s*<)")))
            return true;

        // Skip typedef/using declarations
        if (std::regex_search(line, std::regex(R"(^\s*(?:typedef|using)\s+)")))
            return true;

        // Skip return statements (could change return type deduction)
        if (std::regex_search(line, std::regex(R"(^\s*return\s+)")))
            return true;

        return false;
    }

    std::pair<std::string, int> processFile(const std::string& filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            std::cerr << "Error: Could not open file " << filename << std::endl;
            return {"", 0};
        }

        std::string content;
        std::string line;
        int         changesApplied  = 0;
        bool        inStructOrClass = false;
        int         braceDepth      = 0;

        while (std::getline(file, line))
        {
            std::string originalLine = line;

            // Track struct/class context
            if (std::regex_search(line, std::regex(R"(^\s*(?:struct|class)\s+[a-zA-Z_][a-zA-Z0-9_]*)")))
            {
                inStructOrClass = true;
                braceDepth      = 0;
            }

            // Count braces to track nesting
            for (char c : line)
            {
                if (c == '{')
                    braceDepth++;
                else if (c == '}')
                {
                    braceDepth--;
                    if (braceDepth <= 0 && inStructOrClass)
                    {
                        inStructOrClass = false;
                        braceDepth      = 0;
                    }
                }
            }

            // Skip unsafe contexts
            if (isUnsafeContext(line))
            {
                content += line + "\n";
                continue;
            }

            // Skip if we're inside a struct/class and this looks like a member variable
            if (inStructOrClass && braceDepth > 0 &&
                std::regex_search(line, std::regex(R"(^\s*[a-zA-Z_][a-zA-Z0-9_:]*\s+[a-zA-Z_][a-zA-Z0-9_]*\s*=)")))
            {
                content += line + "\n";
                continue;
            }

            // Try each transformation
            bool transformed = false;
            for (const auto& transform : transformations)
            {
                if (transform.isSafe && std::regex_match(line, transform.pattern))
                {
                    std::string newLine = std::regex_replace(line, transform.pattern, transform.replacement);
                    if (newLine != line)
                    {
                        line = newLine;
                        changesApplied++;
                        transformed = true;
                        std::cout << "  " << transform.description << ": " << originalLine.substr(0, 60) << "..."
                                  << std::endl;
                        break;
                    }
                }
            }

            content += line + "\n";
        }

        file.close();
        return {content, changesApplied};
    }

    bool writeFile(const std::string& filename, const std::string& content)
    {
        std::ofstream file(filename);
        if (!file.is_open())
        {
            std::cerr << "Error: Could not write to file " << filename << std::endl;
            return false;
        }

        file << content;
        file.close();
        return true;
    }
};

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }

    std::string                 filename = argv[1];
    ComprehensiveAutoModernizer modernizer;

    std::cout << " Processing: " << filename << std::endl;

    auto [content, changes] = modernizer.processFile(filename);
    if (content.empty())
        return 1;

    if (changes > 0)
    {
        if (modernizer.writeFile(filename, content))
        {
            std::cout << " Applied " << changes << " auto modernizations to " << filename << std::endl;
        }
        else
        {
            std::cerr << " Failed to write changes to " << filename << std::endl;
            return 1;
        }
    }
    else
    {
        std::cout << "  No auto modernizations needed for " << filename << std::endl;
    }

    std::cout << "Total changes applied: " << changes << std::endl;
    return 0;
}
