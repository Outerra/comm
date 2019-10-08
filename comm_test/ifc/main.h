#pragma once

#ifndef __INTERGEN_GENERATED__main_H__
#define __INTERGEN_GENERATED__main_H__

//@file Interface file for main interface generated by intergen
//See LICENSE file for copyright and license information

//host class: ::ns1::main_cls

#include <comm/commexception.h>
#include <comm/intergen/ifc.h>

#include "luatest_ifc_cfg.h"
#include <comm/str.h>
namespace ns {
    class other;
    class main;
};

namespace ns1 {
    class main_cls;
}

namespace ns {

////////////////////////////////////////////////////////////////////////////////
class main
    : public intergen_interface
{
public:

    // --- interface methods ---

    iref<ns::other> some_get( ifc_out coid::charstr& a );

    int get_a();

    void set_a( int a );

    void fun1( int a, const ns1::dummy& b, float* c, ifc_out ns1::dummy& d, ifc_out int* e, ifc_out iref<ns::other>& f, const coid::charstr& g );

    coid::charstr fun2( int a, iref<ns::other> b, ifc_out int& c, ifc_out iref<ns::other>& d );


protected:
    // --- interface events (callbacks from host to client) ---
    // ---       overload these to handle host events       ---

    friend class ::ns1::main_cls;

    virtual void evt1( int a, int* b, iref<ns::other>& d ) {}

    virtual coid::charstr evt2( int a, int* b, ns1::dummy& c, iref<ns::other>& d, iref<ns::other> e ){ throw coid::exception("handler not implemented"); }

    virtual iref<ns::other> evt3( const coid::token& msg ){ throw coid::exception("handler not implemented"); }

    virtual iref<ns::main> evt4( int a, iref<ns::other> b, int& c, iref<ns::other>& d, int e = -1 ){ throw coid::exception("handler not implemented"); }

    virtual void force_bind_script_events() {}

public:
    // --- host helpers to check presence of handlers in scripts ---

    virtual bool is_bound_evt1() { return true; }
    virtual bool is_bound_evt2() { return true; }
    virtual bool is_bound_evt3() { return true; }
    virtual bool is_bound_evt4() { return true; }

public:
    // --- creators ---

    static iref<main> create() {
        return create<main>(0);
    }

    template<class T>
    static iref<T> create( T* _subclass_ );

    static iref<main> create_special( int a, iref<ns::other> b, int& c, iref<ns::other>& d, int e = -1 ) {
        return create_special<main>(0, a, b, c, d, e);
    }

    template<class T>
    static iref<T> create_special( T* _subclass_, int a, iref<ns::other> b, int& c, iref<ns::other>& d, int e = -1 );

    static iref<main> create_wp( int a, int& b, int& c, int d = -1 ) {
        return create_wp<main>(0, a, b, c, d);
    }

    template<class T>
    static iref<T> create_wp( T* _subclass_, int a, int& b, int& c, int d = -1 );

    // --- internal helpers ---

    virtual ~main() {
        if (_cleaner)
            _cleaner(this, 0);
    }

    ///Interface revision hash
    static const int HASHID = 3187450219;

    ///Interface name (full ns::class string)
    static const coid::tokenhash& IFCNAME() {
        static const coid::tokenhash _name = "ns::main"_T;
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
        static const coid::token _dc("ns::main.create@3187450219"_T);
        static const coid::token _djs("ns::main@wrapper.js"_T);
        static const coid::token _djsc("ns::main@wrapper.jsc"_T);
        static const coid::token _dlua("ns::main@wrapper.lua"_T);
        static const coid::token _dnone;

        switch(bck) {
        case backend::cxx: return _dc;
        case backend::js:  return _djs;
        case backend::jsc: return _djsc;
        case backend::lua: return _dlua;
        default: return _dnone;
        }
    }

    //@return cached active interface of given host class
    //@note host side helper
    static iref<main> intergen_active_interface(::ns1::main_cls* host);

    template<enum class backend B>
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
        static_assert(std::is_base_of<main, C>::value, "not a base class");

        typedef iref<intergen_interface> (*fn_client)(void*, intergen_interface*);
        fn_client cc = [](void*, intergen_interface*) -> iref<intergen_interface> { return new C; };

        coid::token type = typeid(C).name();
        type.consume("class ");
        type.consume("struct ");

        coid::charstr tmp = "ns::main"_T;
        tmp << "@client-3187450219"_T << '.' << type;

        coid::interface_register::register_interface_creator(tmp, cc);
        return 0;
    }

protected:

    static coid::comm_mutex& share_lock() {
        static coid::comm_mutex _mx(500, false);
        return _mx;
    }

    ///Cleanup routine called from ~main()
    static void _cleaner_callback(main* m, intergen_interface* ifc) {
        m->assign_safe(ifc, 0);
    }

    bool assign_safe(intergen_interface* client, iref<main>* pout);

    typedef void (*cleanup_fn)(main*, intergen_interface*);
    cleanup_fn _cleaner;

    bool set_host(policy_intrusive_base* host, intergen_interface* client, iref<main>* pout);

    main() : _cleaner(0)
    {}
};

////////////////////////////////////////////////////////////////////////////////
template<class T>
inline iref<T> main::create( T* _subclass_ )
{
    typedef iref<T> (*fn_creator)(main*);

    static fn_creator create = 0;
    static const coid::token ifckey = "ns::main.create@3187450219"_T;

    if (!create)
        create = reinterpret_cast<fn_creator>(
            coid::interface_register::get_interface_creator(ifckey));

    if (!create) {
        log_mismatch("create"_T, "ns::main.create"_T, "@3187450219"_T);
        return 0;
    }

    return create(_subclass_);
}

////////////////////////////////////////////////////////////////////////////////
template<class T>
inline iref<T> main::create_special( T* _subclass_, int a, iref<ns::other> b, int& c, iref<ns::other>& d, int e )
{
    typedef iref<T> (*fn_creator)(main*, int, iref<ns::other>, int&, iref<ns::other>&, int);

    static fn_creator create = 0;
    static const coid::token ifckey = "ns::main.create_special@3187450219"_T;

    if (!create)
        create = reinterpret_cast<fn_creator>(
            coid::interface_register::get_interface_creator(ifckey));

    if (!create) {
        log_mismatch("create_special"_T, "ns::main.create_special"_T, "@3187450219"_T);
        return 0;
    }

    return create(_subclass_, a, b, c, d, e);
}

////////////////////////////////////////////////////////////////////////////////
template<class T>
inline iref<T> main::create_wp( T* _subclass_, int a, int& b, int& c, int d )
{
    typedef iref<T> (*fn_creator)(main*, int, int&, int&, int);

    static fn_creator create = 0;
    static const coid::token ifckey = "ns::main.create_wp@3187450219"_T;

    if (!create)
        create = reinterpret_cast<fn_creator>(
            coid::interface_register::get_interface_creator(ifckey));

    if (!create) {
        log_mismatch("create_wp"_T, "ns::main.create_wp"_T, "@3187450219"_T);
        return 0;
    }

    return create(_subclass_, a, b, c, d);
}

#pragma warning(push)
#pragma warning(disable : 4191)

inline iref<ns::other> main::some_get( coid::charstr& a )
{ return VT_CALL(iref<ns::other>,(coid::charstr&),0)(a); }

inline int main::get_a()
{ return VT_CALL(int,(),1)(); }

inline void main::set_a( int a )
{ return VT_CALL(void,(int),2)(a); }

inline void main::fun1( int a, const ns1::dummy& b, float* c, ns1::dummy& d, int* e, iref<ns::other>& f, const coid::charstr& g )
{ return VT_CALL(void,(int,const ns1::dummy&,float*,ns1::dummy&,int*,iref<ns::other>&,const coid::charstr&),3)(a,b,c,d,e,f,g); }

inline coid::charstr main::fun2( int a, iref<ns::other> b, int& c, iref<ns::other>& d )
{ return VT_CALL(coid::charstr,(int,iref<ns::other>,int&,iref<ns::other>&),4)(a,b,c,d); }

#pragma warning(pop)

} //namespace

#endif //__INTERGEN_GENERATED__main_H__
