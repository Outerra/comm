#pragma once

#ifndef _INTERGEN_GENERATED__thingface_H_
#define _INTERGEN_GENERATED__thingface_H_

//@file Interface file for thingface interface generated by intergen
//See LICENSE file for copyright and license information

//host class: ::n1::n2::thing

#include <comm/commexception.h>
#include <comm/intergen/ifc.h>

namespace n1 {
namespace n2 {
    class thing;
}}


namespace ifc1 {
namespace ifc2 {

////////////////////////////////////////////////////////////////////////////////
class thingface
    : public intergen_interface
{
public:

    // --- host helpers to check presence of event handlers in scripts ---

    enum class event {
        boo = 0,
    };

    virtual bool is_bound(event m) { return true; }

    // --- creators ---

    static iref<thingface> get() {
        return get<thingface>(0);
    }

    template<class T>
    static iref<T> get( T* _subclass_ );

    // --- interface methods ---

#pragma warning(push)
#pragma warning(disable : 4191)

    int hallo( int a, const coid::token& b, ifc_out coid::charstr& c )
    { return VT_CALL(int,(int,const coid::token&,coid::charstr&),1)(a,b,c); }

    coid::charstr fallo( bool b, const char* str )
    { return VT_CALL(coid::charstr,(bool,const char*),2)(b,str); }

#pragma warning(pop)

protected:
    // --- interface events (callbacks from host to client) ---
    // ---       overload these to handle host events       ---

    friend class ::n1::n2::thing;

    virtual void boo( const char* key ) {}

    virtual void force_bind_script_events() {}

public:

    // --- internal helpers ---

    ///Interface revision hash
    static const int HASHID = 1739587767u;

    ///Interface name (full ns::class string)
    static const coid::tokenhash& IFCNAME() {
        static const coid::tokenhash _name = "ifc1::ifc2::thingface"_T;
        return _name;
    }

    int intergen_hash_id() const override final { return HASHID; }

    bool iface_is_derived( int hash ) const override final {
        return hash == HASHID;
    }

    const coid::tokenhash& intergen_interface_name() const override final {
        return IFCNAME();
    }

    static const coid::token& intergen_default_creator_static( backend bck ) {
        static constexpr coid::token _dc("ifc1::ifc2::thingface.get@1739587767"_T);
        static constexpr coid::token _djs("ifc1::ifc2::thingface@wrapper.js"_T);
        static constexpr coid::token _djsc("ifc1::ifc2::thingface@wrapper.jsc"_T);
        static constexpr coid::token _dlua("ifc1::ifc2::thingface@wrapper.lua"_T);
        static constexpr coid::token _dnone;

        switch(bck) {
        case backend::cxx: return _dc;
        case backend::js:  return _djs;
        case backend::jsc: return _djsc;
        case backend::lua: return _dlua;
        default: return _dnone;
        }
    }

    /// @return cached active interface of given host class
    /// @note host side helper
    static iref<thingface> intergen_active_interface(::n1::n2::thing* host);


#if _MSC_VER == 0 || _MSC_VER >= 1920
    template<enum backend B>
#else
    template<enum class backend B>
#endif
    static void* intergen_wrapper_cache() {
        static void* _cached_wrapper=0;
        if (!_cached_wrapper) {
            const coid::token& tok = intergen_default_creator_static(B);
            _cached_wrapper = coid::interface_register::get_interface_creator(tok);
        }
        return _cached_wrapper;
    }

    void* intergen_wrapper( backend bck ) const override final {
        switch(bck) {
        case backend::js:  return intergen_wrapper_cache<backend::js>();
        case backend::jsc: return intergen_wrapper_cache<backend::jsc>();
        case backend::lua: return intergen_wrapper_cache<backend::lua>();
        default: return 0;
        }
    }

    backend intergen_backend() const override { return backend::cxx; }

    const coid::token& intergen_default_creator( backend bck ) const override final {
        return intergen_default_creator_static(bck);
    }

    ///Client registrator
    template<class C>
    static int register_client()
    {
        static_assert(std::is_base_of<thingface, C>::value, "not a base class");

        typedef intergen_interface* (*fn_client)();
        fn_client cc = []() -> intergen_interface* { return new C; };

        coid::token type = typeid(C).name();
        type.consume("class ");
        type.consume("struct ");

        coid::charstr tmp = "ifc1::ifc2::thingface"_T;
        tmp << "@client-1739587767"_T << '.' << type;

        coid::interface_register::register_interface_creator(tmp, cc);
        return 0;
    }

protected:

    static coid::comm_mutex& share_lock() {
        static coid::comm_mutex _mx(500, false);
        return _mx;
    }

    ///Cleanup routine called from ~thingface()
    static void _cleaner_callback(thingface* m, intergen_interface* ifc) {
        m->assign_safe(ifc, 0);
    }

    bool assign_safe(intergen_interface* client__, iref<thingface>* pout);

    typedef void (*cleanup_fn)(thingface*, intergen_interface*);
    cleanup_fn _cleaner = 0;

    bool set_host(policy_intrusive_base*, intergen_interface*, iref<thingface>* pout);

    ~thingface() {
        VT_CALL(void,(),0)();
        if (_cleaner)
            _cleaner(this, 0);
    }
};

////////////////////////////////////////////////////////////////////////////////
template<class T>
inline iref<T> thingface::get( T* _subclass_ )
{
    typedef iref<T> (*fn_creator)(thingface*);

    static fn_creator create = 0;
    static constexpr coid::token ifckey = "ifc1::ifc2::thingface.get@1739587767"_T;

    if (!create)
        create = reinterpret_cast<fn_creator>(
            coid::interface_register::get_interface_creator(ifckey));

    if (!create) {
        log_mismatch("thingface"_T, "ifc1::ifc2::thingface.get"_T, "@1739587767"_T);
        return 0;
    }

    return create(_subclass_);
}

} //namespace
} //namespace

#endif //_INTERGEN_GENERATED__thingface_H_
