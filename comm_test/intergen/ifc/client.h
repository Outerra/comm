#pragma once

#ifndef _INTERGEN_GENERATED__client_H_
#define _INTERGEN_GENERATED__client_H_

//@file Interface file for client interface generated by intergen
//See LICENSE file for copyright and license information

//host class: ::ab::cd::host

#include <comm/commexception.h>
#include <comm/intergen/ifc.h>

// ***
// block placed before the generated client classes
// ***

struct flags {
    int a;
    float b;

    friend coid::metastream& operator || (coid::metastream& m, flags& d) {
        return m.compound_type(d, [&]() {
            m.member("a", d.a);
            m.member("b", d.b);
        });
    }
};

namespace ab {
// ***
// block placed inside namespace
// ***
}

namespace ab {
namespace cd {
    class host;
}}



////////////////////////////////////////////////////////////////////////////////
class client
    : public intergen_interface
{
public:
    typedef int sometype;

    virtual ~client() {
        if (_cleaner)
            _cleaner(this, 0);
    }

    // --- host helpers to check presence of event handlers in scripts ---

    enum class event {
        echo = 0,
        callforth = 1,
        required = 2,
    };

    virtual bool is_bound(event m) { return true; }

    // --- creators ---

    /// @brief interface creator
    /// @return host class instance
    static iref<client> creator() {
        return creator<client>(0);
    }

    template<class T>
    static iref<T> creator( T* _subclass_ );

    // --- interface methods ---

#pragma warning(push)
#pragma warning(disable : 4191)

    void set_def( const flags& flg = {.a = 1, .b = 2} )
    { return VT_CALL(void,(const flags&),0)(flg); }

    ///Setter
    void set( const coid::token& par )
    { return VT_CALL(void,(const coid::token&),1)(par); }

    ///Getter
    int get( ifc_out coid::charstr& par )
    { return VT_CALL(int,(coid::charstr&),2)(par); }

    ///Using a custom type
    sometype custom()
    { return VT_CALL(sometype,(),3)(); }

    const int* c_only_method( int k )
    { return VT_CALL(const int*,(int),4)(k); }

    void set_array( const float ar[3] )
    { return VT_CALL(void,(const float[3]),5)(ar); }

    void callback( void (*cbk)(int, const coid::token&) )
    { return VT_CALL(void,(void(*)(int, const coid::token&)),6)(cbk); }

    void memfn_callback( coid::callback<void(int, void*)>&& fn )
    { return VT_CALL(void,(coid::callback<void(int, void*)>&&),7)(std::forward<coid::callback<void(int, void*)>>(fn)); }

#pragma warning(pop)

protected:
    // --- interface events (callbacks from host to client) ---
    // ---       overload these to handle host events       ---

    friend class ::ab::cd::host;

    virtual void echo( int k ) {}

    virtual void callforth( void (*cbk)(int, const coid::token&) ) {}

    virtual int required( int x ) = 0;

    virtual void force_bind_script_events() {}

public:

    // --- internal helpers ---

    ///Interface revision hash
    static const int HASHID = 3567392906u;

    ///Interface name (full ns::class string)
    static const coid::tokenhash& IFCNAME() {
        static const coid::tokenhash _name = "client"_T;
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
        static constexpr coid::token _dc("client.creator@3567392906"_T);
        static constexpr coid::token _djs("client@wrapper.js"_T);
        static constexpr coid::token _djsc("client@wrapper.jsc"_T);
        static constexpr coid::token _dlua("client@wrapper.lua"_T);
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
    static iref<client> intergen_active_interface(::ab::cd::host* host);


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
        static_assert(std::is_base_of<client, C>::value, "not a base class");

        typedef intergen_interface* (*fn_client)();
        fn_client cc = []() -> intergen_interface* { return new C; };

        coid::token type = typeid(C).name();
        type.consume("class ");
        type.consume("struct ");

        coid::charstr tmp = "client"_T;
        tmp << "@client-3567392906"_T << '.' << type;

        coid::interface_register::register_interface_creator(tmp, cc);
        return 0;
    }

protected:

    static coid::comm_mutex& share_lock() {
        static coid::comm_mutex _mx(500, false);
        return _mx;
    }

    ///Cleanup routine called from ~client()
    static void _cleaner_callback(client* m, intergen_interface* ifc) {
        m->assign_safe(ifc, 0);
    }

    bool assign_safe(intergen_interface* client__, iref<client>* pout);

    typedef void (*cleanup_fn)(client*, intergen_interface*);
    cleanup_fn _cleaner = 0;

    bool set_host(policy_intrusive_base*, intergen_interface*, iref<client>* pout);
};

// ***
// block placed after the generated client classes
// ***

namespace ab {
namespace cd {
// ***
// block placed inside namespace after client class, specific interface
// ***
}
}

////////////////////////////////////////////////////////////////////////////////
template<class T>
inline iref<T> client::creator( T* _subclass_ )
{
    typedef iref<T> (*fn_creator)(client*);

    static fn_creator create = 0;
    static constexpr coid::token ifckey = "client.creator@3567392906"_T;

    if (!create)
        create = reinterpret_cast<fn_creator>(
            coid::interface_register::get_interface_creator(ifckey));

    if (!create) {
        log_mismatch("client"_T, "client.creator"_T, "@3567392906"_T);
        return 0;
    }

    return create(_subclass_);
}


#endif //_INTERGEN_GENERATED__client_H_
