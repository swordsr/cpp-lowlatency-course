#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

namespace course {

/// Non-owning, type-erased reference to a callable: this course's
/// (pre-standard) std::function_ref. Two words, trivially copyable, never
/// allocates. Like string_view, it REFERENCES — the callable must outlive
/// every call through the FunctionRef.
template <typename Signature>
class FunctionRef;  // primary template deliberately left undefined:
                    // only the R(Args...) specialization below exists.

template <typename R, typename... Args>
class FunctionRef<R(Args...)> {
public:
    /// Empty ref: operator bool is false, calling is undefined behaviour.
    FunctionRef() = default;

    /// Bind to any callable compatible with the signature. The constraint
    /// is given (constraints get their full treatment in Module 4):
    /// `std::invocable` keeps wrong-shaped callables out of overload
    /// resolution, and the `same_as` exclusion stops this template from
    /// hijacking copy construction from another FunctionRef.
    template <typename F>
        requires std::invocable<F&, Args...> &&
                 (!std::same_as<std::remove_cvref_t<F>, FunctionRef>)
    FunctionRef(F&& f) {  // NOLINT(google-explicit-constructor) — implicit by design
        // TODO: erase the type. Point obj_ at f, and set trampoline_ to a
        // function that casts obj_ back to the concrete callable type and
        // invokes it with the arguments (Hint 5). Two lines when done.
        (void)f;
    }

    R operator()(Args... args) const {
        // TODO: dispatch through the trampoline, forwarding the arguments.
        // (Calling an empty FunctionRef is UB, exactly like std::function_ref
        // — no null check on the hot path.)
        ((void)args, ...);
        if constexpr (!std::is_void_v<R>) {
            return R{};  // placeholder so the stub compiles for any R
        }
    }

    // --- given ---
    explicit operator bool() const { return trampoline_ != nullptr; }

private:
    // The whole object: one pointer to the callable, one pointer to a
    // per-type trampoline function. This IS a hand-rolled single-entry
    // vtable (README THEORY §3).
    void* obj_ = nullptr;
    R (*trampoline_)(void*, Args...) = nullptr;
};

}  // namespace course
