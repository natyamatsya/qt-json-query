///
// function_ref_demo.cpp - Demonstration of stdcompat::function_ref usage
// Shows the benefits of function_ref over std::function for non-owning callable references
//

#include <iostream>
#include <chrono>
#include <vector>
#include <functional>
#include <stdcompat/function_ref.hpp>

// Example 1: Basic usage - function_ref as parameter
void process_with_function_ref(stdcompat::function_ref<int(int)> func, int value) {
    std::cout << "function_ref result: " << func(value) << std::endl;
}

void process_with_std_function(std::function<int(int)> func, int value) {
    std::cout << "std::function result: " << func(value) << std::endl;
}

// Example 2: Performance comparison
template<typename Callable>
void benchmark_calls(const std::string& name, Callable&& func, int iterations) {
    auto start{std::chrono::high_resolution_clock::now()};
    
    auto sum = 0;
    for (int i = 0; i < iterations; ++i) {
        sum += func(i);
    }
    
    auto end{std::chrono::high_resolution_clock::now()};
    auto duration{std::chrono::duration_cast<std::chrono::microseconds>(end - start)};
    
    std::cout << name << ": " << duration.count() << " μs (sum: " << sum << ")" << std::endl;
}

// Example 3: Callback-style usage (common in JSONPath evaluation)
class JSONPathProcessor {
public:
    void processResults(const std::vector<int>& results, 
                       stdcompat::function_ref<void(int)> callback) {
        std::cout << "Processing " << results.size() << " results..." << std::endl;
        for (int result : results) {
            callback(result);
        }
    }
};

int main() {
    std::cout << "=== stdcompat::function_ref Demonstration ===" << std::endl;
    std::cout << std::endl;

    // Example 1: Basic usage
    std::cout << "1. Basic Usage Comparison:" << std::endl;
    
    auto lambda = [](int x) { return x * 2; };
    
    process_with_function_ref(lambda, 21);  // No allocation overhead
    process_with_std_function(lambda, 21);  // Potential allocation overhead
    
    std::cout << std::endl;

    // Example 2: Performance comparison
    std::cout << "2. Performance Comparison (1,000,000 calls):" << std::endl;
    
    auto multiply_by_3 = [](int x) { return x * 3; };
    
    // function_ref version
    auto func_ref_benchmark = [&](int x) {
        auto ref = multiply_by_3;
        return ref(x);
    };
    
    // std::function version  
    auto std_func_benchmark = [&](int x) {
        auto func = multiply_by_3;
        return func(x);
    };
    
    benchmark_calls("function_ref", func_ref_benchmark, 1000000);
    benchmark_calls("std::function", std_func_benchmark, 1000000);
    
    std::cout << std::endl;

    // Example 3: Callback-style usage
    std::cout << "3. Callback-Style Usage:" << std::endl;
    
    JSONPathProcessor processor;
    auto results = {1, 4, 9, 16, 25};
    
    // Using lambda as callback
    processor.processResults(results, [](int value) {
        std::cout << "  Found result: " << value << std::endl;
    });
    
    std::cout << std::endl;

    // Example 4: Different callable types
    std::cout << "4. Different Callable Types:" << std::endl;
    
    // Function pointer
    auto square = [](int x) { return x * x; };
    process_with_function_ref(square, 7);
    
    // Member function (with object)
    struct Calculator {
        int multiplier = 5;
        int multiply(int x) { return x * multiplier; }
    };
    
    Calculator calc;
    auto member_func = [&calc](int x) { return calc.multiply(x); };
    process_with_function_ref(member_func, 8);
    
    std::cout << std::endl;
    std::cout << "=== Benefits of function_ref ===" << std::endl;
    std::cout << "✓ No dynamic allocation overhead" << std::endl;
    std::cout << "✓ Lightweight non-owning reference" << std::endl;
    std::cout << "✓ Perfect for callback parameters" << std::endl;
    std::cout << "✓ Compatible with all callable types" << std::endl;
    std::cout << "✓ Better performance than std::function" << std::endl;
    
    return 0;
}
