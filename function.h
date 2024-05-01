#pragma once
/* ***** BEGIN LICENSE BLOCK *****
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is COID/comm module.
*
* The Initial Developer of the Original Code is
* Outerra.
* Portions created by the Initial Developer are Copyright (C) 2018-2024
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
* Brano Kemen
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the MPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the MPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK ***** */

#include "trait.h"
#include "alloc//memtrack.h"

#include <tuple>

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
#ifdef COID_VARIADIC_TEMPLATES

#if SYSTYPE_MSVC > 0 && SYSTYPE_MSVC < 1900
//replacement for make_index_sequence
template <size_t... Ints>
struct index_sequence
{
    using type = index_sequence;
    using value_type = size_t;
};

// --------------------------------------------------------------

template <class Sequence1, class Sequence2>
struct _merge_and_renumber;

template <size_t... I1, size_t... I2>
struct _merge_and_renumber<index_sequence<I1...>, index_sequence<I2...>>
    : index_sequence<I1..., (sizeof...(I1) + I2)...>
{ };

// --------------------------------------------------------------

template <size_t N>
struct make_index_sequence
    : _merge_and_renumber<typename make_index_sequence<N / 2>::type,
    typename make_index_sequence<N - N / 2>::type>
{ };

template<> struct make_index_sequence<0> : index_sequence<> { };
template<> struct make_index_sequence<1> : index_sequence<0> { };

#else

    template<size_t Size>
using make_index_sequence = std::make_index_sequence<Size>;

template<size_t... Ints>
using index_sequence = std::index_sequence<Ints...>;

#endif

////////////////////////////////////////////////////////////////////////////////

template <typename Func, int K, typename A, typename ...Args> struct variadic_call_helper
{
    static void call(const Func& f, A&& a, Args&& ...args) {
        f(K, std::forward<A>(a));
        variadic_call_helper<Func, K + 1, Args...>::call(f, std::forward<Args>(args)...);
    }
};

template <typename Func, int K, typename A> struct variadic_call_helper<Func, K, A>
{
    static void call(const Func& f, A&& a) {
        f(K, std::forward<A>(a));
    }
};

///Invoke function on variadic arguments
//@param fn function to invoke, in the form (int k, auto&& p)
template<typename Func, typename ...Args>
inline void variadic_call(const Func& fn, Args&&... args) {
    variadic_call_helper<Func, 0, Args...>::call(fn, std::forward<Args>(args)...);
}

template<typename Func>
inline void variadic_call(const Func& fn) {}

////////////////////////////////////////////////////////////////////////////////

template <class Fn>
struct callback;


///Helper to get types and count of lambda arguments
/// http://stackoverflow.com/a/28213747/2435594

template <typename R, typename ...Args>
struct callable_base {
    virtual ~callable_base() {}
    virtual R operator()(Args ...args) const = 0;
    virtual callable_base* clone() const = 0;
    virtual size_t size() const = 0;
};

template <bool Const, bool Variadic, typename R, typename... Args>
struct closure_traits_base
{
    using arity = std::integral_constant<size_t, sizeof...(Args) >;
    using is_variadic = std::integral_constant<bool, Variadic>;
    using is_const = std::integral_constant<bool, Const>;
    using returns_void = std::is_void<R>;

    using result_type = R;

    template <size_t i>
    using arg = typename std::tuple_element<i, std::tuple<Args...>>::type;

    using callbase = callable_base<result_type, Args...>;

    template <typename Fn>
    struct callable : callbase
    {
        COIDNEWDELETE(callable);

        callable(const Fn& fn) : fn(fn) {}
        callable(Fn&& fn) : fn(std::forward<Fn>(fn)) {}

        R operator()(Args ...args) const override final {
            return fn(std::forward<Args>(args)...);
        }

        callbase* clone() const override final {
            return new callable<Fn>(fn);
        }

        size_t size() const override final {
            return sizeof(*this);
        }

    private:
        Fn fn;
    };

    struct function
    {
        template <typename Rx, typename... Argsx>
        function(Rx(*fn)(Argsx...)) : c(0) {
            if (fn)
                c = new callable<decltype(fn)>(fn);
        }

        template <typename Fn>
        function(const Fn& fn) : c(0) {
            c = new callable<Fn>(fn);
        }

        template <typename Fn>
        function(Fn&& fn) : c(0) {
            typedef std::remove_cv_t<std::remove_reference_t<Fn>> Fn_cv;
            c = new callable<Fn_cv>(std::forward<Fn>(fn));
        }

        function() : c(0) {}
        function(nullptr_t) : c(0) {}

        function(const function& other) : c(0) {
            if (other.c)
                c = other.c->clone();
        }

        function(function&& other) : c(0) {
            c = other.c;
            other.c = 0;
        }

        template <typename Fn>
        function(callback<Fn>&& fn);

        template <typename Fn>
        function(const callback<Fn>& fn);

        ~function() { if (c) delete c; }

        function& operator = (const function& other) {
            if (c) {
                delete c;
                c = 0;
            }
            if (other.c)
                c = other.c->clone();
            return *this;
        }

        function& operator = (function&& other) noexcept {
            if (c) delete c;
            c = other.c;
            other.c = 0;
            return *this;
        }

        R operator()(Args ...args) const {
            return (*c)(std::forward<Args>(args)...);
        }

        ///Automatic cast to unconvertible bool for checking via if
        explicit operator bool() const { return c != 0; }

        callbase* eject() {
            callbase* r = c;
            c = 0;
            return r;
        }

    protected:
        callbase* c;
    };
};

template <typename T>
struct closure_traits : closure_traits<decltype(&T::operator())> {};

template <class R, class... Args>
struct closure_traits<R(Args...)> : closure_traits_base<false, false, R, Args...>
{};

#define COID_REM_CTOR(...) __VA_ARGS__

#define COID_CLOSURE_TRAIT(cv, var, is_var)                                 \
template <typename C, typename R, typename... Args>                         \
struct closure_traits<R (C::*) (Args... COID_REM_CTOR var) cv>              \
    : closure_traits_base<std::is_const<int cv>::value, is_var, R, Args...> \
{};

COID_CLOSURE_TRAIT(const, (, ...), 1)
COID_CLOSURE_TRAIT(const, (), 0)
COID_CLOSURE_TRAIT(, (, ...), 1)
COID_CLOSURE_TRAIT(, (), 0)


template <typename Fn>
using function = typename closure_traits<Fn>::function;



////////////////////////////////////////////////////////////////////////////////

template <typename Lambda, typename Fn>
struct match_lambda;

template <typename Lambda, typename R, typename ...Args>
struct match_lambda<Lambda, R(Args...)>
{
    template< typename fn >
    struct ptmf_to_pf;

    template< typename r, typename c, typename ... a >
    struct ptmf_to_pf< r(c::*) (a ...) const >
    {
        typedef r(* type)(a ...);
    };

    static constexpr bool value = std::is_constructible<R(*)(Args...), typename ptmf_to_pf<decltype(&Lambda::operator())>::type>::value;
};




struct callback_context
{
    void* this__ = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
///A callback function that may contain a member function, a static one or a lambda.
/**
    callback<R(Args...)>

    callback<> is like function<>, except it can be also bound to member methods without captured this pointer, and can be invoked
    with different objects.

    It's possible to bind to:
    - static functions with (Args...) arguments
    - lambda functions with (Args...) arguments (capturing or optimized non-capturing)
    - member methods with (Args...) arguments and with an object reference
    - member methods with (Args...) arguments, that needs to be invoked via invoke_with_this, giving it an object to run on
    - static or lambda functions with (callback_context&, Args...) arguments that allows passing the object from invoke_with_this() call

    Size: 2 * sizeof(ptr_t)

    Usage:
        struct something
        {
            static int funs(int, void*) { return 0; }
            int funm(int, void*) { return value; }

            int value = 1;
        };

        int z = 2;
        callback<int(int, void*)> fns = &something::funs;
        callback<int(int, void*)> fnm = &something::funm;
        callback<int(int, void*)> fnl = [](int, void*) { return -1; };
        callback<int(int, void*)> fnz = [z](int, void*) { return z; };

        something s;

        DASSERT(fns.invoke_with_this(&s, 1, 0) == 0);
        DASSERT(fnm.invoke_with_this(&s, 1, 0) == 1);
        DASSERT(fnl.invoke_with_this(&s, 1, 0) == -1);
        DASSERT(fnz.invoke_with_this(&s, 1, 0) == 2);
        DASSERT(fnl(1, 0) == -1);
        DASSERT(fnz(1, 0) == 2);

**/
template <class Fn>
struct callback
{};

template <class R, class ...Args>
struct callback<R(Args...)>
{
private:

    union hybrid {
        void* ptr = 0;
        R(*function)(Args...);
        R(*function_context)(callback_context&, Args...);
        callable_base<R, Args...>* flambda;
        callable_base<R, callback_context&, Args...>* flambda_context;
    };

    hybrid _fn;
    R(*_caller)(const hybrid&, callback_context& ctx, Args...) = 0;

    static R call_static(const hybrid& h, callback_context& ctx, Args ...args) {
        return h.function(std::forward<Args>(args)...);
    }

    static R call_static_context(const hybrid& h, callback_context& ctx, Args ...args) {
        return h.function_context(ctx, std::forward<Args>(args)...);
    }

    static R call_flambda(const hybrid& h, callback_context& ctx, Args ...args) {
        return (*h.flambda)(std::forward<Args>(args)...);
    }

    static R call_flambda_context(const hybrid& h, callback_context& ctx, Args ...args) {
        return (*h.flambda_context)(ctx, std::forward<Args>(args)...);
    }


    template< typename fn >
    struct ptmf_to_pf;

    template< typename r, typename c, typename ... a >
    struct ptmf_to_pf< r(c::*) (a ...) const >
    {
        typedef r(* type)(a ...);
    };

public:

    callback() {}

    callback(nullptr_t) {}

    ///A plain function
    callback(R(*fn)(Args...)) {
        _fn.function = fn;
        _caller = &call_static;
    }

    ///A plain function with context to pass on
    callback(R(*fn)(callback_context&, Args...)) {
        _fn.function_context = fn;
        _caller = &call_static_context;
    }


    ///A member function pointer, needs to be invoked via invoke_with_this()
    template <class T>
    callback(R(T::* fn)(Args...))
    {
        union caster {
            R(*function)(Args...);
            R(T::* method)(Args...);
        };

        if coid_constexpr_if(sizeof(caster) == sizeof(caster::function)) {
            //optimized case of single-inheritance member function pointer
            caster tmp;
            tmp.method = fn;
            _fn.ptr = (void*)tmp.function;

            _caller = [](const hybrid& h, callback_context& ctx, Args ...args) {
                RASSERT_FATALX(ctx.this__ != nullptr, "member function callbacks require this pointer");
                caster v;
                v.function = (R(*)(Args...))h.ptr;
                return (static_cast<T*>(ctx.this__)->*(v.method))(std::forward<Args>(args)...);
            };
        }
        else {
            //a complex member pointer, wrap in a lambda
            function<R(callback_context&, Args...)> fnx = [fn](callback_context& ctx, Args ...args) -> R {
                RASSERT_FATALX(ctx.this__ != nullptr, "member function callbacks require this pointer");
                return (static_cast<T*>(ctx.this__)->*fn)(std::forward<Args>(args)...);
            };

            _fn.flambda_context = fnx.eject();
            _caller = &call_flambda_context;
        }
    }

    ///A const member function pointer, needs to be invoked via invoke_with_this()
    template <class T>
    callback(R(T::* fn)(Args...) const)
    {
        union caster {
            R(*function)(Args...);
            R(T::* method)(Args...) const;
        };

        if coid_constexpr_if(sizeof(caster) == sizeof(caster::function)) {
            //optimized case of single-inheritance member function pointer
            caster tmp;
            tmp.method = fn;
            _fn.ptr = (void*)tmp.function;

            _caller = [](const hybrid& h, callback_context& ctx, Args ...args) {
                RASSERT_FATALX(ctx.this__ != nullptr, "member function callbacks require this pointer");
                caster v;
                v.function = (R(*)(Args...))h.ptr;
                return (static_cast<const T*>(ctx.this__)->*(v.method))(std::forward<Args>(args)...);
            };
        }
        else {
            //a complex member pointer, wrap in a lambda
            function<R(callback_context&, Args...)> fnx = [fn](callback_context& ctx, Args ...args) -> R {
                RASSERT_FATALX(ctx.this__ != nullptr, "member function callbacks require this pointer");
                return (static_cast<const T*>(ctx.this__)->*fn)(std::forward<Args>(args)...);
            };

            _fn.flambda_context = fnx.eject();
            _caller = &call_flambda_context;
        }
    }

    ///A member function pointer capturing object ref
    template <class T>
    callback(R(T::* fn)(Args...), T& tptr)
    {
        function<R(Args ...)> fnx = [fn, &tptr](Args ...args) -> R {
            return (tptr.*fn)(std::forward<Args>(args)...);
        };
        _fn.flambda = fnx.eject();
        _caller = &call_flambda;
    }

    ///A const member function pointer capturing object ref
    template <class T>
    callback(R(T::* fn)(Args...) const, const T& tptr)
    {
        function<R(Args...)> fnx = [fn, &tptr](Args ...args) -> R {
            return (tptr.*fn)(std::forward<Args>(args)...);
        };

        _fn.flambda = fnx.eject();
        _caller = &call_flambda;
    }

    ///A direct function object
    callback(function<R(Args...)>&& fn) {
        _fn.flambda = fn.eject();
        _caller = &call_flambda;
    }

    ///A direct function object with context to pass on
    callback(function<R(callback_context&, Args...)>&& fn) {
        _fn.flambda_context = fn.eject();
        _caller = &call_flambda_context;
    }

    ///A non-capturing lambda
    template <class Fn, std::enable_if<std::is_constructible<R(*)(Args...), typename ptmf_to_pf<decltype(&Fn::operator())>::type>::value, bool>::type = true>
    callback(Fn&& lambda) {
        if constexpr (std::is_constructible<R(*)(Args...), Fn>::value) {
            _fn.function = lambda;
            _caller = &call_static;
        }
        else {
            function<R(Args...)> fn = std::move(lambda);
            _fn.flambda = fn.eject();
            _caller = &call_flambda;
        }
    }

    ///A non-capturing lambda
    template <class Fn, std::enable_if<std::is_constructible<R(*)(callback_context&, Args...), typename ptmf_to_pf<decltype(&Fn::operator())>::type>::value, bool>::type = true>
    callback(Fn&& lambda) {
        if constexpr (std::is_constructible<R(*)(callback_context&, Args...), Fn>::value) {
            _fn.function_context = lambda;
            _caller = &call_static_context;
        }
        else {
            function<R(callback_context&, Args...)> fn = std::move(lambda);
            _fn.flambda_context = fn.eject();
            _caller = &call_flambda_context;
        }
    }

    ~callback() {
        destroy();
    }

    callback(const callback& fn) {
        _caller = fn._caller;
        if (fn._fn.ptr) {
            if (_caller == &call_flambda)
                _fn.flambda = fn._fn.flambda->clone();
            else if (_caller == &call_flambda_context)
                _fn.flambda_context = fn._fn.flambda_context->clone();
            else
                _fn.ptr = fn._fn.ptr;
        }
    }

    callback(callback&& fn) noexcept {
        _fn.ptr = fn._fn.ptr;
        fn._fn.ptr = 0;
        _caller = fn._caller;
    }


    callback& operator = (nullptr_t) {
        destroy();
        _caller = 0;
        return *this;
    }

    callback& operator = (const callback& fn) {
        destroy();
        new(this) callback(fn);
        return *this;
    }

    callback& operator = (callback&& fn) {
        destroy();
        _fn.ptr = fn._fn.ptr;
        fn._fn.ptr = 0;
        _caller = fn._caller;
        return *this;
    }

    callback& operator = (R(*fn)(Args...)) {
        destroy();
        _fn.function = fn;
        _caller = &call_static;
        return *this;
    }

    callback& operator = (R(*fn)(callback_context&, Args...)) {
        destroy();
        _fn.function_context = fn;
        _caller = &call_static_context;
        return *this;
    }

    ///A member function pointer
    template <class T>
    callback& operator = (R(T::* fn)(Args...))
    {
        destroy();
        new(this) callback(fn);
        return *this;
    }

    ///A const member function pointer
    template <class T>
    callback& operator = (R(T::* fn)(Args...) const)
    {
        destroy();
        new(this) callback(fn);
        return *this;
    }

    ///A member function pointer with context
    template <class T>
    callback& operator = (R(T::* fn)(callback_context&, Args...))
    {
        destroy();
        new(this) callback(fn);
        return *this;
    }

    ///A const member function pointer with context
    template <class T>
    callback& operator = (R(T::* fn)(callback_context&, Args...) const)
    {
        destroy();
        new(this) callback(fn);
        return *this;
    }

    ///A direct function object
    callback& operator = (function<R(Args...)>&& fn) {
        destroy();
        _fn.flambda = fn.eject();
        _caller = &call_flambda;
        return *this;
    }

    ///A direct function object with context
    callback& operator = (function<R(callback_context&, Args...)>&& fn) {
        destroy();
        _fn.flambda_context = fn.eject();
        _caller = &call_flambda_context;
        return *this;
    }

    ///A non-capturing lambda
    template <class Fn, typename std::enable_if<std::is_constructible<R(*)(Args...), Fn>::value, bool>::type = true>
    callback& operator = (Fn&& lambda) {
        destroy();
        _fn.function = lambda;
        _caller = &call_static;
        return *this;
    }

    ///A capturing lambda
    template <class Fn, typename std::enable_if<!std::is_constructible<R(*)(Args...), Fn>::value, bool>::type = true>
    callback& operator = (Fn&& lambda) {
        destroy();
        function<R(Args...)> fn = std::move(lambda);
        _fn.flambda = fn.eject();
        _caller = &call_flambda;
        return *this;
    }

    ///A non-capturing lambda with context
    template <class Fn, typename std::enable_if<std::is_constructible<R(*)(callback_context&, Args...), Fn>::value, bool>::type = true>
    callback& operator = (Fn&& lambda) {
        destroy();
        _fn.function_context = lambda;
        _caller = &call_static_context;
        return *this;
    }

    ///A capturing lambda
    template <class Fn, typename std::enable_if<!std::is_constructible<R(*)(callback_context&, Args...), Fn>::value, bool>::type = true>
    callback& operator = (Fn&& lambda) {
        destroy();
        function<R(callback_context&, Args...)> fn = std::move(lambda);
        _fn.flambda_context = fn.eject();
        _caller = &call_flambda_context;
        return *this;
    }



    /// @brief Invoke callback
    /// @note fails if a member function was bound without captured `this'
    R operator()(Args ...args) const {
        callback_context ctx;
        return _caller(_fn, ctx, std::forward<Args>(args)...);
    }

    /// @brief Invoke callback, providing `this' pointer that is used in case a member function was bound without captured `this'
    /// @param this__ this pointer to use with member methods
    R invoke_with_this(const void* this__, Args ...args) const {
        callback_context ctx = {const_cast<void*>(this__)};
        return _caller(_fn, ctx, std::forward<Args>(args)...);
    }

    explicit operator bool() const { return _fn.ptr != 0; }

    bool operator == (const callback& other) const { return _caller == other._caller && _fn.ptr == other._fn.ptr; }

private:

    void destroy()
    {
        if (_fn.ptr) {
            if (_caller == &call_flambda)
                delete _fn.flambda;
            else if (_caller == &call_flambda_context)
                delete _fn.flambda_context;
        }
        _fn.ptr = 0;
    }
};



COID_NAMESPACE_END

#endif //COID_VARIADIC_TEMPLATES
