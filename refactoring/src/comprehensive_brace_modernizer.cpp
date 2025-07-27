#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <vector>
#include <sstream>

class ComprehensiveBraceModernizer
{
  private:
    std::vector<std::regex>  transformationPatterns;
    std::vector<std::string> replacementPatterns;

    void initializePatterns()
    {
        // Pattern 1: auto var = expr; -> auto var{expr};
        transformationPatterns.emplace_back(R"(^(\s*)(auto\s+)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^=][^;]*);)");
        replacementPatterns.emplace_back("$1$2$3{$4};");

        // Pattern 2: const auto var = expr; -> const auto var{expr};
        transformationPatterns.emplace_back(R"(^(\s*)(const\s+auto\s+)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^=][^;]*);)");
        replacementPatterns.emplace_back("$1$2$3{$4};");

        // Pattern 3: Type var = expr; -> Type var{expr}; (for common types)
        transformationPatterns.emplace_back(
            R"(^(\s*)((?:int|long|short|char|bool|float|double|size_t|qsizetype|uint|uint32_t|uint64_t|int32_t|int64_t)\s+)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^=][^;]*);)");
        replacementPatterns.emplace_back("$1$2$3{$4};");

        // Pattern 4: const Type var = expr; -> const Type var{expr}; (for common types)
        transformationPatterns.emplace_back(
            R"(^(\s*)(const\s+(?:int|long|short|char|bool|float|double|size_t|qsizetype|uint|uint32_t|uint64_t|int32_t|int64_t)\s+)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^=][^;]*);)");
        replacementPatterns.emplace_back("$1$2$3{$4};");

        // Pattern 5: std::string var = expr; -> std::string var{expr};
        transformationPatterns.emplace_back(R"(^(\s*)(std::string\s+)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^=][^;]*);)");
        replacementPatterns.emplace_back("$1$2$3{$4};");

        // Pattern 6: const std::string var = expr; -> const std::string var{expr};
        transformationPatterns.emplace_back(
            R"(^(\s*)(const\s+std::string\s+)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^=][^;]*);)");
        replacementPatterns.emplace_back("$1$2$3{$4};");

        // Pattern 7: QString var = expr; -> QString var{expr};
        transformationPatterns.emplace_back(R"(^(\s*)(QString\s+)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^=][^;]*);)");
        replacementPatterns.emplace_back("$1$2$3{$4};");

        // Pattern 8: const QString var = expr; -> const QString var{expr};
        transformationPatterns.emplace_back(
            R"(^(\s*)(const\s+QString\s+)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^=][^;]*);)");
        replacementPatterns.emplace_back("$1$2$3{$4};");

        // Pattern 9: pointer var = expr; -> pointer var{expr}; (for pointer types)
        transformationPatterns.emplace_back(
            R"(^(\s*)([A-Za-z_][A-Za-z0-9_:<>,\s]*\*\s+)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^=][^;]*);)");
        replacementPatterns.emplace_back("$1$2$3{$4};");

        // Pattern 10: const pointer var = expr; -> const pointer var{expr};
        transformationPatterns.emplace_back(
            R"(^(\s*)(const\s+[A-Za-z_][A-Za-z0-9_:<>,\s]*\*\s+)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^=][^;]*);)");
        replacementPatterns.emplace_back("$1$2$3{$4};");
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

        // Skip function signatures and declarations
        if (std::regex_search(
                line, std::regex(R"(^\s*[a-zA-Z_][a-zA-Z0-9_:<>,\s]*\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\([^)]*\)\s*[;{])")))
        {
            return true;
        }

        // Skip operator= definitions
        if (std::regex_search(line, std::regex(R"(operator\s*=)")))
            return true;

        // Skip lambda assignments (already use {} syntax)
        if (std::regex_search(line, std::regex(R"(=\s*\[[^\]]*\]\s*\([^)]*\)\s*[{-])")))
            return true;

        // Skip assignments that already use brace initialization
        if (std::regex_search(line, std::regex(R"(=\s*\{[^}]*\})")))
            return true;

        // Skip assignments with function calls that might be constructors
        if (std::regex_search(line, std::regex(R"(=\s*[A-Za-z_][A-Za-z0-9_:<>,\s]*\([^)]*\))")))
            return true;

        // Skip assignments with array/container initializers
        if (std::regex_search(line, std::regex(R"(=\s*\{)")))
            return true;

        return false;
    }

    bool isInClassOrStructContext(const std::vector<std::string>& lines, size_t currentIndex)
    {
        // Look backwards to see if we're inside a class or struct
        int  braceLevel         = 0;
        bool foundClassOrStruct = false;

        for (int i = static_cast<int>(currentIndex); i >= 0; --i)
        {
            const std::string& line = lines[i];

            // Count braces to track nesting level
            for (char c : line)
                if (c == '{')
                    braceLevel++;
                else if (c == '}')
                    braceLevel--;

            // If we're at the top level and found a class/struct, we're in member context
            if (braceLevel == 1 && std::regex_search(line, std::regex(R"(^\s*(?:class|struct)\s+)")))
            {
                foundClassOrStruct = true;
                break;
            }

            // If we've gone past the class/struct definition, we're not in member context
            if (braceLevel <= 0 && i < static_cast<int>(currentIndex))
                break;
        }

        return foundClassOrStruct && braceLevel == 1;
    }

    bool isValidForBraceInit(const std::string& line)
    {
        // Additional checks for valid brace initialization candidates

        // Skip if it's a complex expression that might not work with braces
        if (std::regex_search(line, std::regex(R"(=\s*[^;]*\?\s*[^;]*:\s*[^;]*)")))
            return false; // Ternary operator

        // Skip if it's a cast expression
        if (std::regex_search(line, std::regex(R"(=\s*(?:static_cast|dynamic_cast|const_cast|reinterpret_cast)<)")))
            return false;

        // Skip if it's already using modern initialization
        if (std::regex_search(line, std::regex(R"(\{[^}]*\})")))
            return false;

        return true;
    }

  public:
    ComprehensiveBraceModernizer() { initializePatterns(); }

    bool transformFile(const std::string& filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            std::cerr << "Error: Cannot open file " << filename << std::endl;
            return false;
        }

        std::vector<std::string> lines;
        std::string              line;
        while (std::getline(file, line))
            lines.push_back(line);
        file.close();

        bool modified = false;

        for (size_t i = 0; i < lines.size(); ++i)
        {
            std::string& currentLine = lines[i];

            // Skip unsafe contexts
            if (isUnsafeContext(currentLine))
                continue;

            // Skip if we're in a class/struct member context
            if (isInClassOrStructContext(lines, i))
                continue;

            // Skip if not valid for brace initialization
            if (!isValidForBraceInit(currentLine))
                continue;

            // Apply transformation patterns
            for (size_t j = 0; j < transformationPatterns.size(); ++j)
            {
                std::string newLine =
                    std::regex_replace(currentLine, transformationPatterns[j], replacementPatterns[j]);
                if (newLine != currentLine)
                {
                    std::cout << "Transforming in " << filename << ":" << (i + 1) << std::endl;
                    std::cout << "  Before: " << currentLine << std::endl;
                    std::cout << "  After:  " << newLine << std::endl;
                    currentLine = newLine;
                    modified    = true;
                    break; // Only apply one transformation per line
                }
            }
        }

        if (modified)
        {
            std::ofstream outFile(filename);
            if (!outFile.is_open())
            {
                std::cerr << "Error: Cannot write to file " << filename << std::endl;
                return false;
            }

            for (const auto& line : lines)
                outFile << line << std::endl;
            outFile.close();

            std::cout << "Successfully modernized " << filename << std::endl;
        }

        return modified;
    }
};

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }

    ComprehensiveBraceModernizer modernizer;
    bool                         result = modernizer.transformFile(argv[1]);

    return result ? 0 : 1;
}
