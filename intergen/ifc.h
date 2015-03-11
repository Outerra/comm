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
 * Portions created by the Initial Developer are Copyright (C) 2013
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

#ifndef __INTERGEN_IFC_H__
#define __INTERGEN_IFC_H__

#include "../ref.h"
#include "../interface.h"
#include "../binstring.h"

///Interface class decoration keyword
//@param name desired name of the interface class, optionally with namespace. With + prefix generates also the capture code for non-const methods by default
//@param path relative path (and optional file name) of the interface header file
#define ifc_class(name,path)

///Interface class decoration keyword with callback hook
//@param name desired name of the interface class, optionally with namespace
//@param path relative path (and optional file name) of the interface header file
//@param var name for the variable representing the connected client
#define ifc_class_var(name,path,var) iref<intergen_interface> var

///Virtual base interface class decoration keyword
//@param name desired name of the interface class, optionally with namespace
//@param path relative path (and optional file name) of the interface header file
#define ifc_class_virtual(name,path)

///Interface function decoration keyword
#define ifc_fn

///Interface function decoration keyword
//@param extname an alternative name for the method used in the interface
//@note special symbols: ifc_fnx(~) marks a method that is invoked when the interface is being detached (client destroying)
//@note ifc_fnx(!) or ifc_fnx(!name) marks an interface method that's not available to script clients
//@note ifc_fnx(-) or ifc_fnx(-name) marks an interface method that is not captured by an interceptor (for net replication etc) if default was on
//@note ifc_fnx(+) or ifc_fnx(+name) marks an interface method that is captured by an interceptor (for net replication etc) if default was off
#define ifc_fnx(extname)

///Interface callback decoration
#define ifc_event
#define ifc_eventx(extname)

///Statement to fill in as the default implementation of an event, in case the client doesn't implement the event handling.
///Optionally, should be specified after an ifc_event method declaration:
/// ifc_event bool event() const ifc_evbody(return false;);
#define ifc_evbody(x)

///Interface functon argument decoration keywords
#define ifc_in
#define ifc_inout
#define ifc_out

///Declare out argument that will be converted to given interface
//@param ifc interface to return to clients
//#define ifc_ret(ifc)

///Parameter hint that the storage for the type (usually array) may be external and valid only for the duration
/// of the call or limited
#define ifc_volatile

////////////////////////////////////////////////////////////////////////////////

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
    
    //@return host class pointer
    //@note T derived from policy_intrusive_base
    template<typename T>
    T* host() const { return static_cast<T*>(_host.get()); }

    //@return interface class pointer
    template<typename T>
    const T* iface() const { return static_cast<const T*>(this); }

    //@return interface class pointer
    template<typename T>
    T* iface() { return static_cast<T*>(this); }

    ifn_t* vtable() const { return _vtable; }

    //@return hash of interface definition, serving for version checks
    virtual int intergen_hash_id() const = 0;

    //@return interface name
    virtual const coid::token& intergen_interface_name() const = 0;

    //@return name of default creator
    virtual const coid::token& intergen_default_creator() const = 0;

    //@return js wrapper creator for underlying object
    virtual void* intergen_wrapper_js() const = 0;

    ///Bind or unbind interface call interceptor handler for current interface and all future instances of the same interface class
    //@param capture capture buffer, 0 to turn the capture off
    //@param instid instance identifier of this interface
    //@return false if the interface can't be bound (instid > max)
    virtual bool intergen_bind_capture( coid::binstring* capture, uint instid ) { return false; }

    ///Dispatch a captured call
    virtual void intergen_capture_dispatch( uint mid, coid::binstring& bin ) {}
};

////////////////////////////////////////////////////////////////////////////////

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
