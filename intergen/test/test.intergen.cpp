
//@file  interface dispatcher generated by intergen v4

#include "ifc/thingface.h"
#include "test.hpp"

#include <comm/ref.h>
#include <comm/singleton.h>
#include <comm/binstring.h>
#include <type_traits>

using namespace coid;

static_assert( std::is_base_of<policy_intrusive_base, n1::n2::thing>::value, "class 'thing' must be derived from coid::policy_intrusive_base");

////////////////////////////////////////////////////////////////////////////////
// interface thingface of class thing

namespace ifc1 {
namespace ifc2 {

///
class thingface_dispatcher : public thingface
{
private:

    static coid::binstring* _capture;
    static uint16 _instid;
    static ifn_t* _vtable1;
    static ifn_t* _vtable2;

    static ifn_t* get_vtable()
    {
        if(_vtable1) return _vtable1;

        _vtable1 = new ifn_t[3];
        _vtable1[0] = reinterpret_cast<ifn_t>(static_cast<void(policy_intrusive_base::*)()>(&n1::n2::thing::destroy));
        _vtable1[1] = reinterpret_cast<ifn_t>(static_cast<int(policy_intrusive_base::*)(int,const coid::token&,coid::charstr&)>(&n1::n2::thing::hallo));
        _vtable1[2] = reinterpret_cast<ifn_t>(static_cast<coid::charstr(policy_intrusive_base::*)(bool,const char*)>(&n1::n2::thing::fallo));
        return _vtable1;
    }
    
    #define VT_CALL2(R,F,I) ((*reinterpret_cast<policy_intrusive_base*>(this)).*(reinterpret_cast<R(policy_intrusive_base::*)F>(_vtable1[I])))
    

    static ifn_t* get_vtable_intercept()
    {
        if(_vtable2) return _vtable2;
        ifn_t* vtable1 = get_vtable();

        _vtable2 = new ifn_t[3];
        _vtable2[0] = vtable1[0];
        _vtable2[1] = vtable1[1];
        _vtable2[2] = vtable1[2];
        return _vtable2;
    }
    
protected:

    thingface_dispatcher()
    {}
    
    bool intergen_bind_capture( coid::binstring* capture, uint instid ) override
    {
        if(instid >= 0xffU)
            return false;
    
        _instid = uint16(instid << 8U);
        _capture = capture;
        _vtable = _capture ? get_vtable_intercept() : get_vtable();
        return true;
    }    

    void intergen_capture_dispatch( uint mid, coid::binstring& bin ) override
    {
        switch(mid) {
        case UMAX32:
        default: throw coid::exception("unknown method id in thingface capture dispatcher");
        }
    }

    ///Cleanup routine called from ~thingface()
    static void _cleaner_callback( thingface* m, intergen_interface* ifc ) {
        n1::n2::thing* host = m->host<n1::n2::thing>();
        if(host) host->_ifc = ifc;
    }

public:

    // creator methods
    
    static iref<thingface> get( thingface* __here__ )
    {
        iref<n1::n2::thing> host = n1::n2::thing::get_thing();
        if(!host)
            return 0;

        thingface_dispatcher* __disp__ = static_cast<thingface_dispatcher*>(__here__);
        if(!__disp__)
            __disp__ = new thingface_dispatcher;

        __disp__->_host.create(host.get());
        __disp__->_vtable = _capture ? get_vtable_intercept() : get_vtable();
        if(!host->_ifc) {
            __disp__->_cleaner = &_cleaner_callback;
            host->_ifc = __disp__;
        }
        
        return iref<thingface>(__disp__);
    }

    ///Register interface creators in the global registry
    static void* register_interfaces()
    {
        interface_register::register_interface_creator(
            "ifc1::ifc2::thingface.get@324072501", (void*)&get);

        return (void*)&register_interfaces;
    }
};

coid::binstring* thingface_dispatcher::_capture = 0;
uint16 thingface_dispatcher::_instid = 0xffffU;
intergen_interface::ifn_t* thingface_dispatcher::_vtable2 = 0;
intergen_interface::ifn_t* thingface_dispatcher::_vtable1 = 0;


//auto-register the available interface creators
static void* thingface_autoregger = thingface_dispatcher::register_interfaces();

void* force_register_thingface() {
    return thingface_dispatcher::register_interfaces();
}

} //namespace ifc1
} //namespace ifc2

// events

namespace n1 {
namespace n2 {

void thing::boo( const char* key )
{
	if(!_ifc) 
        throw coid::exception("handler not implemented");
    else
        return static_cast<ifc1::ifc2::thingface&>(*_ifc).boo(key);
}

} //namespace n1
} //namespace n2
