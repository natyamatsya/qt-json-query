#pragma once

#include <QJsonValue>
#include <QJsonArray>
#include <functional>
#include <expected>
#include <memory>
#include <concepts>
#include <stdcompat/function_ref.hpp>
#include "json-query/json-path/JSONPathEvalError.hpp"

namespace json_query::json_path::internal {

/**
 * @brief Concept defining the interface for result collectors
 * 
 * A ResultCollectorConcept must provide methods to collect values and handle errors.
 * This concept ensures compile-time type safety without runtime overhead.
 */
template<typename T>
concept ResultCollectorConcept = requires(T& collector, const QJsonValue& value, EvalError error) {
    collector.collect(value);                                    // Must have collect method
    { collector.hasError() } -> std::convertible_to<bool>;      // Must be able to check for errors
    { collector.canHandleErrors() } -> std::convertible_to<bool>; // Must indicate error handling capability
    collector.handleError(error);                               // Must have error handling method
};

/**
 * @brief Zero-overhead result streaming interface using concepts
 * 
 * ConceptResultStreamer provides direct member function calls without any
 * type erasure or function pointer overhead. It works with any type that
 * satisfies the ResultCollectorConcept.
 */
template<ResultCollectorConcept CollectorType>
class ConceptResultStreamer {
public:
    /**
     * @brief Construct a ConceptResultStreamer with a collector reference
     * @param collector Reference to the collector that will receive results
     */
    explicit constexpr ConceptResultStreamer(CollectorType& collector) noexcept
        : collector_(collector) {}

    /**
     * @brief Emit a single result value with zero overhead
     * @param value The JSON value to emit
     */
    constexpr void emitValue(const QJsonValue& value) const noexcept {
        collector_.collect(value);  // Direct member call - zero overhead!
    }

    /**
     * @brief Emit all values from a QJsonArray with zero overhead
     * @param array The array containing values to emit
     */
    void emitArray(const QJsonArray& array) const noexcept {
        for (const auto& value : array) {
            collector_.collect(value);  // Direct member call for each value
        }
    }

    /**
     * @brief Handle an error during streaming with compile-time optimization
     * @param error The error that occurred
     */
    constexpr void handleError(EvalError error) const noexcept {
        if constexpr (requires { collector_.canHandleErrors(); }) {
            if (collector_.canHandleErrors()) {
                collector_.handleError(error);
            }
        }
    }

    /**
     * @brief Check if the streamer can emit results (always true for concept-based design)
     * @return true (always, since collector reference is required)
     */
    [[nodiscard]] constexpr bool canEmit() const noexcept {
        return true;
    }

    /**
     * @brief Check if the streamer has error handling capability
     * @return true if the collector can handle errors
     */
    [[nodiscard]] constexpr bool canHandleErrors() const noexcept {
        if constexpr (requires { collector_.canHandleErrors(); }) {
            return collector_.canHandleErrors();
        } else {
            return false;
        }
    }

private:
    CollectorType& collector_;
};

/**
 * @brief Legacy ResultStreamer for backward compatibility with function_ref
 * 
 * This maintains the old interface for code that hasn't been migrated to
 * the concept-based approach yet.
 */
class ResultStreamer {
public:
    using EmitFunction = stdcompat::function_ref<void(const QJsonValue&)>;
    using ErrorHandler = stdcompat::function_ref<void(EvalError)>;

    /**
     * @brief Construct a ResultStreamer with emission callback only
     * @param emitFn Function called for each result value
     */
    constexpr explicit ResultStreamer(EmitFunction emitFn) noexcept
        : emitFn_(emitFn), hasErrorHandler_(false) {}

    /**
     * @brief Construct a ResultStreamer with emission and error handling callbacks
     * @param emitFn Function called for each result value
     * @param errorFn Function called when errors occur
     */
    constexpr ResultStreamer(EmitFunction emitFn, ErrorHandler errorFn) noexcept
        : emitFn_(emitFn), onError_(errorFn), hasErrorHandler_(true) {}

    /**
     * @brief Emit a single result value
     * @param value The JSON value to emit
     */
    constexpr void emitValue(const QJsonValue& value) const noexcept {
        emitFn_(value);
    }

    /**
     * @brief Emit all values from a QJsonArray
     * @param array The array containing values to emit
     */
    void emitArray(const QJsonArray& array) const noexcept {
        for (const auto& value : array) {
            emitFn_(value);
        }
    }

    /**
     * @brief Handle an error during streaming
     * @param error The error that occurred
     */
    constexpr void handleError(EvalError error) const noexcept {
        if (hasErrorHandler_) {
            onError_(error);
        }
    }

    /**
     * @brief Check if the streamer can emit results
     * @return true (always, since emitFn_ is required)
     */
    [[nodiscard]] constexpr bool canEmit() const noexcept {
        return true;
    }

    /**
     * @brief Check if the streamer has an error handler
     * @return true if the streamer can handle errors
     */
    [[nodiscard]] constexpr bool canHandleErrors() const noexcept {
        return hasErrorHandler_;
    }

private:
    EmitFunction emitFn_;
    ErrorHandler onError_{[](EvalError){}};
    bool hasErrorHandler_;
};

/**
 * @brief Concept-compliant result collector that accumulates values into a QJsonArray
 * 
 * This collector satisfies the ResultCollectorConcept and provides zero-overhead
 * streaming through the ConceptResultStreamer interface.
 */
class ResultCollector {
public:
    /**
     * @brief Construct a ResultCollector that accumulates into the provided array
     * @param results Pointer to QJsonArray where results will be accumulated
     */
    explicit ResultCollector(QJsonArray* results) noexcept
        : results_(results), hasErrorHandler_(true) {}  // Enable error handling by default

    /**
     * @brief Construct a ResultCollector with error handling
     * @param results Pointer to QJsonArray where results will be accumulated
     * @param errorFn Function called when errors occur
     */
    template<typename ErrorFn>
    ResultCollector(QJsonArray* results, ErrorFn&& errorFn) noexcept
        : results_(results), onError_(std::forward<ErrorFn>(errorFn)), hasErrorHandler_(true) {}

    /**
     * @brief Get a zero-overhead ConceptResultStreamer for this collector
     * @return ConceptResultStreamer with direct member function calls
     */
    [[nodiscard]] auto getConceptStreamer() noexcept {
        return ConceptResultStreamer<ResultCollector>{*this};
    }

    /**
     * @brief Get a legacy ResultStreamer for backward compatibility
     * @return ResultStreamer using function_ref (with potential dangling reference issues)
     */
    [[nodiscard]] ResultStreamer getStreamer() noexcept {
        // For backward compatibility, but this has the dangling reference issue
        auto collectFn = [this](const QJsonValue& value) noexcept { 
            this->collect(value); 
        };
        
        if (hasErrorHandler_) {
            auto errorFn = [this](EvalError error) noexcept { 
                this->handleError(error); 
            };
            return ResultStreamer{collectFn, errorFn};
        } else {
            return ResultStreamer{collectFn};
        }
    }

    /**
     * @brief Emit a single value (for compatibility with streaming interface)
     * @param value Value to emit
     */
    void emitValue(const QJsonValue& value) {
        collect(value);
    }

    /**
     * @brief Collect a single value into the results array (concept requirement)
     * @param value The value to add to the results
     */
    void collect(const QJsonValue& value) noexcept {
        if (results_) {
            results_->append(value);
        }
    }

    /**
     * @brief Handle an error during collection (concept requirement)
     * @param error The error that occurred
     */
    void handleError(EvalError error) noexcept {
        if (hasErrorHandler_ && onError_) {
            onError_(error);
        }
        lastError_ = error;
        hasError_ = true;
    }

    /**
     * @brief Check if the collector can handle errors (concept requirement)
     * @return true if the collector can handle errors
     */
    [[nodiscard]] bool canHandleErrors() const noexcept {
        return hasErrorHandler_;
    }

    /**
     * @brief Check if an error occurred during collection (concept requirement)
     * @return true if an error was handled
     */
    [[nodiscard]] bool hasError() const noexcept {
        return hasError_;
    }

    /**
     * @brief Get the last error that occurred
     * @return The last EvalError that was handled
     */
    [[nodiscard]] EvalError getLastError() const noexcept {
        return lastError_;
    }

    /**
     * @brief Get the number of collected results
     * @return The size of the results array
     */
    [[nodiscard]] qsizetype size() const noexcept {
        return results_ ? results_->size() : 0;
    }

    /**
     * @brief Check if any results have been collected
     * @return true if the results array is empty
     */
    [[nodiscard]] bool empty() const noexcept {
        return results_ ? results_->isEmpty() : true;
    }

private:
    QJsonArray* results_;
    std::function<void(EvalError)> onError_{[](EvalError){}};  // Only used if error handling needed
    bool hasErrorHandler_;
    EvalError lastError_ = EvalError::TypeMismatchObject;
    bool hasError_ = false;
};

} // namespace json_query::json_path::internal
