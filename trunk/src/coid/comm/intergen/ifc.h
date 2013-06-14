
#ifndef __INTERGEN_IFC_H__
#define __INTERGEN_IFC_H__

#include "../ref.h"
#include "../interface.h"

///Interface class decoration keyword
//@param name desired name of the interface class, optionally with namespace
//@param path relative path (and optional file name) of the interface header file
#define ifc_class(name,path)

///Interface class decoration keyword with callback hook
//@param name desired name of the interface class, optionally with namespace
//@param path relative path (and optional file name) of the interface header file
//@param var name for the variable representing the connected client
#define ifc_class_var(name,path,var) coid::clean_ptr<intergen_interface> var

///Virtual base interface class decoration keyword
//@param name desired name of the interface class, optionally with namespace
//@param path relative path (and optional file name) of the interface header file
#define ifc_class_virtual(name,path)

///Interface function decoration keyword
#define ifc_fn
#define ifc_fnx(extname)

///Interface callback decoration
#define ifc_event
#define ifc_eventx(extname)

///Interface functon argument decoration keywords
#define ifc_in
#define ifc_inout
#define ifc_out


///Base class for intergen interfaces
class intergen_interface
    : public policy_intrusive_base
{
protected:
    typedef void (policy_intrusive_base::*ifn_t)();

    iref<policy_intrusive_base> _host;
    ifn_t* _vtable;

    intergen_interface() : _vtable(0)
    {}

    virtual ~intergen_interface() {
        _vtable = 0;
    }

public:
    policy_intrusive_base* host() const { return _host.get(); }
};

///Call interface vtable method
#define VT_CALL(R,F,I) ((*_host).*(reinterpret_cast<R(policy_intrusive_base::*)F>(_vtable[I])))


///Force registration of an interface declared in a statically-linked library
#define FORCE_REGISTER_LIBRARY_INTERFACE(ns,ifc) \
    namespace ns { void* force_register_##ifc(); static void* autoregger_##ifc = force_register_##ifc(); }
    
///Force registration of an interface declared in a statically-linked library
#define FORCE_REGISTER_LIBRARY_INTERFACE2(ifc) \
    void* force_register_##ifc(); static void* autoregger_##ifc = force_register_##ifc()

    
///Force registration of a script binder in a statically-linked library
#define FORCE_REGISTER_BINDER_INTERFACE(ns,ifc,script) \
    namespace ns { namespace script { void* register_binders_for_##ifc(); static void* autoregger_##ifc = register_binders_for_##ifc(); }}
    

#endif //__INTERGEN_IFC_H__
