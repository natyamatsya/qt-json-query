///
// stdcompat/function_ref.hpp - Wrapper for tl::function_ref
// Provides std::function_ref-like interface in stdcompat namespace
//
// This header wraps tartan llama's function_ref implementation and imports it
// into the stdcompat namespace for consistent API usage throughout the codebase.
//

#ifndef STDCOMPAT_FUNCTION_REF_HPP
#define STDCOMPAT_FUNCTION_REF_HPP

#include <tl/function_ref.hpp>

namespace stdcompat
{

/// A lightweight, non-owning reference to a callable.
///
/// This is a wrapper around tl::function_ref that provides a std::function_ref-like
/// interface. Unlike std::function, function_ref does not own the callable and
/// has no dynamic allocation overhead.
///
/// Example usage:
/// ```cpp
/// void process_data(stdcompat::function_ref<int(int)> func) {
///     return func(42);
/// }
///
/// auto lambda = [](int x) { return x * 2; };
/// process_data(lambda);  // No allocation, direct reference
/// ```
///
/// @tparam Signature Function signature (e.g., int(int, float))
template <typename Signature>
using function_ref = tl::function_ref<Signature>;

} // namespace stdcompat

#endif // STDCOMPAT_FUNCTION_REF_HPP
