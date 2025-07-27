// Comprehensive Variable Assignment Modernization Tool
// Safely refactors 'const auto var = expr' to 'const auto var{expr}' style
// while preserving references and avoiding control statements

#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <vector>

struct VariableTransformation
{
    std::regex  pattern;
    std::string replacement;
    std::string description;
    bool        preservesReferences;
};

class ComprehensiveVariableModernizer
{
  private:
    std::vector<VariableTransformation> transformations;

  public:
    ComprehensiveVariableModernizer() { initializeTransformations(); }

    void initializeTransformations()
    {
        // COMPREHENSIVE SAFE PATTERNS for variable assignment modernization

        // 1. Static const auto (very common pattern)
        transformations.push_back(
            {std::regex(R"(^(\s*)static\s+const\s+auto\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);$)"),
             "$1static const auto $2{$3};",
             "static const auto variable",
             false});

        // 2. Const auto (basic pattern)
        transformations.push_back({std::regex(R"(^(\s*)const\s+auto\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);$)"),
                                   "$1const auto $2{$3};",
                                   "const auto variable",
                                   false});

        // 3. Auto (basic pattern)
        transformations.push_back({std::regex(R"(^(\s*)auto\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);$)"),
                                   "$1auto $2{$3};",
                                   "auto variable",
                                   false});

        // 4. REFERENCE PRESERVATION: const auto&
        transformations.push_back({std::regex(R"(^(\s*)const\s+auto&\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);$)"),
                                   "$1const auto& $2{$3};",
                                   "const auto& reference",
                                   true});

        // 5. REFERENCE PRESERVATION: auto&
        transformations.push_back({std::regex(R"(^(\s*)auto&\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);$)"),
                                   "$1auto& $2{$3};",
                                   "auto& reference",
                                   true});

        // 6. REFERENCE PRESERVATION: auto&&
        transformations.push_back({std::regex(R"(^(\s*)auto&&\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);$)"),
                                   "$1auto&& $2{$3};",
                                   "auto&& universal reference",
                                   true});
    }

    bool isUnsafeContext(const std::string& line)
    {
        // Check for contexts that should be avoided

        // Control statements
        if (std::regex_search(line, std::regex(R"(\b(if|while|for|switch|catch)\s*\()")))
            return true;

        // Function signatures/parameters
        if (std::regex_search(line, std::regex(R"(\b(return|throw)\s+)")))
            return true;

        // Already has braces (avoid double transformation)
        if (line.find("{") != std::string::npos && line.find("}") != std::string::npos)
            return true;

        return false;
    }

    bool isComplexExpression(const std::string& expression)
    {
        // Avoid transforming expressions that might have different semantics with braces
        return expression.find("new ") != std::string::npos || expression.find("static_cast") != std::string::npos ||
               expression.find("dynamic_cast") != std::string::npos ||
               expression.find("reinterpret_cast") != std::string::npos ||
               expression.find("const_cast") != std::string::npos ||
               expression.find("{") != std::string::npos || // Already has braces
               expression.find("?") != std::string::npos;   // Ternary operator
    }

    bool transformFile(const std::string& filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            std::cout << "Cannot open file: " << filename << std::endl;
            return false;
        }

        std::vector<std::string> lines;
        std::string              line;
        while (std::getline(file, line))
            lines.push_back(line);
        file.close();

        bool hasChanges   = false;
        int  totalChanges = 0;

        // Process each line carefully
        for (size_t i = 0; i < lines.size(); ++i)
        {
            std::string& currentLine = lines[i];

            // Skip unsafe contexts
            if (isUnsafeContext(currentLine))
                continue;

            // Apply each transformation
            for (const auto& transform : transformations)
            {
                std::smatch match;
                if (std::regex_match(currentLine, match, transform.pattern))
                {
                    // Additional safety check for complex expressions
                    if (match.size() >= 4)
                    {
                        std::string expression = match[3].str();
                        if (isComplexExpression(expression))
                            continue; // Skip complex expressions
                    }

                    std::string newLine = std::regex_replace(currentLine, transform.pattern, transform.replacement);
                    if (newLine != currentLine)
                    {
                        std::cout << "  ✅ Applied change for: " << transform.description << std::endl;
                        std::cout << "    Before: " << currentLine << std::endl;
                        std::cout << "    After:  " << newLine << std::endl;
                        currentLine = newLine;
                        totalChanges++;
                        hasChanges = true;
                        break; // Only apply one transformation per line
                    }
                }
            }
        }

        if (hasChanges)
        {
            // Write back to file
            std::ofstream outFile(filename);
            if (outFile.is_open())
            {
                for (const auto& line : lines)
                    outFile << line << std::endl;
                outFile.close();
                std::cout << "Total changes applied: " << totalChanges << std::endl;
                return true;
            }
            else
            {
                std::cout << "Error: Could not write to file: " << filename << std::endl;
                return false;
            }
        }
        else
        {
            std::cout << "No changes needed for: " << filename << std::endl;
            return true;
        }
    }
};

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " <filename>" << std::endl;
        std::cout << "Comprehensive Variable Assignment Modernizer" << std::endl;
        std::cout << "Transforms 'const auto var = expr' to 'const auto var{expr}'" << std::endl;
        std::cout << "while preserving references and avoiding unsafe contexts." << std::endl;
        return 1;
    }

    ComprehensiveVariableModernizer modernizer;
    if (modernizer.transformFile(argv[1]))
        return 0;
    else
        return 1;
}
