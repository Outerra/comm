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
 * Portions created by the Initial Developer are Copyright (C) 2013-2023
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
#include "../typeseq.h"
#include "../local.h"
#include "../log/logger.h"
#include "../global.h"

namespace coid {
    class binstring;
}

//for profiler initialization in modules
namespace profiler {
    extern void init_backend_in_module();
}

///Interface class decoration keyword.
/// @param ifc_name desired name of the interface class, optionally with (virtual) base interface [ns::]name [: [ns::]baseifc] [ final]
/// @param dst_path relative path (and optional file name) where to generate the interface header file
/// @param ... optional relative path to the file containing the base interface
/// @example ifc_class(ns::client : ns::base, "../ifc")
#define ifc_class(ifc_name, dst_path, ...)
#define IFC_CLASS(ifc_name, dst_path, ...)

///Interface class decoration keyword for bi-directional interfaces with event methods that can be overriden by the client.
/// @param ifc_name desired name of the interface class, optionally with (virtual) base interface [ns::]name [: [ns::]baseifc] [ final]
/// @param dst_path relative path (and optional file name) of the interface header file
/// @param var name for the variable representing the connected client
/// @param base_ifc_name base interface
/// @note var is of type clean_ptr, an intentional weak link, since interface already holds ref to the host
/// @example ifc_class_var(ns::client : ns::base, "../ifc", _ifc)
#define ifc_class_var(ifc_name, dst_path, var, ...) coid::clean_ptr<intergen_interface> var
#define IFC_CLASS_VAR(ifc_name, dst_path, var, ...) coid::clean_ptr<intergen_interface> var

///Virtual base interface class decoration keyword for declaration of abstrac base interfaces
/// @param ifc_name desired name of the interface class, optionally with namespace
/// @param dst_path relative path (and optional file name) of the interface header file
/// @example ifc_class_virtual(ns:base, "../ifc")
/// @example ifc_class(ns::client : ns::base, "../ifc")
#define ifc_class_virtual(ifc_name, dst_path)
#define ifc_class_virtual_var(ifc_name, dst_path, var, ...) coid::clean_ptr<intergen_interface> var
#define IFC_CLASS_VIRTUAL(ifc_name, dst_path)
#define IFC_CLASS_VIRTUAL_VAR(ifc_name, dst_path, var, ...) coid::clean_ptr<intergen_interface> var

///Data interface (not refcounted)
/// @param ifc_name desired name of the interface class, optionally with namespace and base interface [ns:]name [: baseifc]
/// @param dst_path relative path (and optional file name) of the interface header file
#define ifc_struct(ifc_name, dst_path)
#define IFC_STRUCT(ifc_name, dst_path)


///Interface creator function (static)
/// @note should have format: static iref<class> name(...)  - for ifc_class interfaces
/// @note                 or: static class* name(...)       - for ifc_struct interfaces
/// @note a creator can be also declared using ifc_fn static iref<class> name(...) for backward compatibility
#define ifc_creator
#define ifc_creatorx(extname)

///Interface function decoration keyword: such decorated method will be added into the interface
/// @example ifc_fn void fn(int a, ifc_out int& b);
#define ifc_fn

///Interface function decoration keyword
/// @param extname an alternative name for the method used in the interface
/// @note special symbols:
/// @note ifc_fnx(~) marks a method that is invoked when the interface is being detached (client destroying)
/// @note ifc_fnx(!) or ifc_fnx(!name) marks an interface method that's not available to script clients
/// @note ifc_fnx(-) or ifc_fnx(-name) marks an interface method that is not captured by an interceptor (for net replication etc) if default was on
/// @note ifc_fnx(+) or ifc_fnx(+name) marks an interface method that is captured by an interceptor (for net replication etc) if default was off
/// @note ifc_fnx(@connect) marks a method that gets called upon successfull interface connection
/// @note ifc_fnx(@unload) marks a static method called when a registered client of given interface is unloading
/// @note ifc_fnx(ifc_class=...) marks a method that returns ifc_class type interface
/// @note ifc_fnx(ifc_struct=...) marks a method that returns ifc_struct type interface
/// @note ifc_fnx(... %singleton) marks a creator method that returns ifc_class type interface cached in local singleton
/// @example ifc_fnx(get) void get_internal();
#define ifc_fnx(extname)


///Interface event callback decoration
/// @note events are defined in the generated dispatcher code, so the method after this keyword should be just a declaration
/// @note ifc_eventx(@connect) marks an event that gets called upon successfull interface connection
/// @note ifc_eventx(=0) or ifc_eventx(extname=0) tells that client virtual method will be pure and has to be implemented
/// @example ifc_event void on_init(int a, ifc_out int& b);   //definition generated by intergen
#define ifc_event
#define ifc_eventx(extname)

///Statement to fill in as the default implementation of an event, in case the client doesn't implement the event handling.
///Optionally, should be specified after an ifc_event method declaration:
/// ifc_event bool event() const ifc_default_body(return false;);
/// @example ifc_event int on_init() ifc_default_body("return -1;");
#define ifc_default_body(x)
#define ifc_default_empty

#define ifc_evbody(x)       //obsolete

///Interface functon argument direction decoration keywords
#define ifc_in
#define ifc_inout
#define ifc_out

///Parameter hint that the storage for the type (usually array) may be external and valid only for the duration
/// of the (event) call or limited
#define ifc_volatile

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomment"
#endif
/* @note
  Use the following style comments to include the enclosed code in generated interface client header file:
    //ifc{
        ... code ...
    //}ifc

  or

    /*ifc{
    ... code ...
    }ifc* /

  to push the enclosed code block into the client header, or

    //ifc-dispatch{
        ... code ...
    //}ifc-dispatch

  or

    /*ifc-dispatch{
    ... code ...
    }ifc-dispatch* /

  to push the enclosed code block into the generated dispatch files.

  With multiple interfaces present in host class it's also possible to list which interface header files:
    //ifc{ ns::ifca ns::ifcb
    ...
    //}ifc

  Generated code is normally pushed before the client class definition. In case it needs to be added after,
  add a + sign:
    //ifc{+
   or
    //ifc{ ns::ifc1+
    ...
*/
#ifdef __clang__
#pragma clang diagnostic pop
#endif


////////////////////////////////////////////////////////////////////////////////

///Call interface vtable method
#define VT_CALL(R,F,I) ((*_host).*(reinterpret_cast<R(policy_intrusive_base::*)F>(_vtable[I])))

#define DT_CALL(R,F,I) (this->*(reinterpret_cast<R(intergen_data_interface::*)F>(_fn_table[I])))

////////////////////////////////////////////////////////////////////////////////

///Base class for intergen interfaces
class intergen_interface
    : public policy_intrusive_base
{
protected:
    typedef void (policy_intrusive_base::* ifn_t)();

    iref<policy_intrusive_base> _host;
    ifn_t* _vtable = 0;

    virtual ~intergen_interface() {
        _vtable = 0;
        _host.release();
    }

public:

    static const int VERSION = 9;

    typedef bool (*fn_unload_client)(const coid::token& client, const coid::token& module_name, coid::binstring* bstr);

    /// @return host class pointer
    /// @note T derived from policy_intrusive_base
    template<typename T>
    T* host() const { return static_cast<T*>(_host.get()); }

    /// @return interface class pointer
    template<typename T>
    const T* iface() const { return static_cast<const T*>(this); }

    /// @return interface class pointer
    template<typename T>
    T* iface() { return static_cast<T*>(this); }

    ///Invoke callback handler
    template <class R, class ...Args>
    R call(const coid::callback<R(Args...)>& cbk, Args ...args) const {
        return cbk(this, std::forward<Args>(args)...);
    }


    ifn_t* vtable() const { return _vtable; }

    /// @return hash of interface definition, serving for version checks
    virtual int intergen_hash_id() const = 0;

    /// @return true if this interface is derived from interface with given hash
    virtual bool iface_is_derived(int hash) const = 0;

    /// @return interface name
    virtual const coid::tokenhash& intergen_interface_name() const = 0;

    ///Supported interface client types (dispatcher back-ends)
    enum class backend : int8 {
        none,                           //< empty clients
        cxx,                            //< c++ back-end implementation
        js,                             //< javascript
        jsc,                            //< jsc (unsupported)
        lua,                            //< lua clients

        count_,
        unknown = -1
    };

    /// @return back-end implementation
    virtual backend intergen_backend() const = 0;

    /// @return wrapper creator for given back-end
    virtual void* intergen_wrapper(backend bck) const = 0;

    /// @return the name of the default creator
    virtual const coid::token& intergen_default_creator(backend bck) const = 0;

    ///Bind or unbind interface call interceptor handler for current interface and all future instances of the same interface class
    /// @param capture capture buffer, 0 to turn the capture off
    /// @param instid instance identifier of this interface
    /// @return false if the interface can't be bound (instid > max)
    virtual bool intergen_bind_capture(coid::binstring* capture, uint instid) { return false; }

    ///Dispatch a captured call
    virtual void intergen_capture_dispatch(uint mid, coid::binstring& bin) {}

    /// @return real interface in case this is a wrapper around an existing interface object
    virtual intergen_interface* intergen_real_interface() { return this; }

#ifdef COID_VARIADIC_TEMPLATES

    ///Interface log function with formatting
    /// @param type log type
    /// @param fmt format @see charstr.print
    /// @param vs variadic parameters
    template<class ...Vs>
    void ifclog(coid::log::level type, const coid::token& fmt, Vs&& ...vs) {
        ref<coid::logmsg> msgr = coid::interface_register::canlog(type, intergen_interface_name(), this, coid::log::target::primary_log);
        if (!msgr)
            return;

        msgr->str().print(fmt, std::forward<Vs>(vs)...);
    }

    ///Interface log function with formatting, log type inferred from message text prefix
    /// @param fmt format @see charstr.print
    /// @param vs variadic parameters
    template<class ...Vs>
    void ifclog(const coid::token& fmt, Vs&& ...vs) {
        ref<coid::logmsg> msgr = coid::interface_register::canlog(coid::log::level::none, intergen_interface_name(), this, coid::log::target::primary_log);
        if (!msgr)
            return;

        msgr->str().print(fmt, std::forward<Vs>(vs)...);
    }

#endif //COID_VARIADIC_TEMPLATES

    static void ifclog_ext(coid::log::level type, coid::log::target target, const coid::token& from, const void* inst, const coid::token& txt) {
        //deduce type if none set
        coid::token rest = txt;
        if (type == coid::log::level::none)
            type = coid::logmsg::consume_type(rest);

        ref<coid::logmsg> msgr = coid::interface_register::canlog(type, from, inst, target);
        if (!msgr)
            return;

        msgr->str() << rest;
    }

protected:

    static void log_mismatch(const coid::token& clsname, const coid::token& ifckey, const coid::token& hash)
    {
        //check if interface missing or different version
        coid::dynarray<coid::interface_register::creator> tmp;
        coid::interface_register::get_interface_creators(ifckey, "", tmp);

        ref<coid::logmsg> msg = coid::interface_register::canlog(coid::log::level::warning, clsname, 0, coid::log::target::primary_log);
        if (msg) {
            coid::charstr& str = msg->str();
            if (tmp.size() > 0) {
                str << "interface creator version mismatch (requested " << ifckey << hash << ", found ";

                uints n = tmp.size();
                str << tmp[0].name;
                for (uints i = 1; i < n; ++i)
                    str << ", " << tmp[i].name;

                str << ')';
            }
            else
                str << "interface creator not found (" << ifckey << ')';
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
struct intergen_data_interface
{
    typedef void (intergen_data_interface::* ifn_t)();
    typedef void (* icr_t)();

protected:

    static void log_mismatch(const coid::token& clsname, const coid::token& ifckey, const coid::token& hash)
    {
        //check if interface missing or different version
        coid::dynarray<coid::interface_register::creator> tmp;
        coid::interface_register::get_interface_creators(ifckey, "", tmp);

        ref<coid::logmsg> msg = coid::interface_register::canlog(coid::log::level::warning, clsname, 0, coid::log::target::primary_log);
        if (msg) {
            coid::charstr& str = msg->str();
            if (tmp.size() > 0) {
                str << "interface creator version mismatch (requested " << ifckey << hash << ", found ";

                uints n = tmp.size();
                str << tmp[0].name;
                for (uints i = 1; i < n; ++i)
                    str << ", " << tmp[i].name;

                str << ')';
            }
            else
                str << "interface creator not found (" << ifckey << ')';
        }
    }
};


COID_NAMESPACE_BEGIN

class ifcman
{
public:

    using ifn_t = intergen_data_interface::ifn_t;
    using icr_t = intergen_data_interface::icr_t;

    struct data_ifc_descriptor {
        const type_sequencer<ifcman>::entry* _type = 0;
        icr_t* _cr_table = 0;
        ifn_t* _fn_table = 0;

        uint64 _hash = 0;
        const meta::class_interface* _meta = 0;
    };


    /// @brief Get data ifc type interface descriptor, checking the version
    /// @tparam T data interface type
    /// @param hash version
    /// @return ptr to the descriptor, if it exists and its version matches, otherwise null
    template <class T>
    static const data_ifc_descriptor* get_type_ifc(uint64 hash) {
        ifcman& m = get();
        uint id = m._seq.type_id<T>();
        return id < m._clients.size() && m._clients[id]._hash == hash ? &m._clients[id] : nullptr;
    }

    /// @brief Get data ifc type interface descriptor
    /// @tparam T data interface type
    /// @return ptr to the descriptor if it exists, otherwise null
    template <class T>
    static const data_ifc_descriptor* get_type_ifc() {
        ifcman& m = get();
        uint id = m._seq.type_id<T>();
        return id < m._clients.size() ? &m._clients[id] : nullptr;
    }

    /// @brief Set info about data interface type
    /// @tparam T data interface type
    /// @param hash version
    /// @param cr_table table of creators
    /// @param fn_table table of methods
    /// @param meta meta info
    /// @return assigned type id
    template <class T>
    static int set_type_ifc(uint64 hash, icr_t* cr_table, ifn_t* fn_table, const meta::class_interface* meta)
    {
        ifcman& m = get();
        int ord = m._seq.assign<T>([&](int id, const type_sequencer<ifcman>::entry& en) {
            data_ifc_descriptor& dc = m._clients.get_or_add(id);
            dc._fn_table = fn_table;
            dc._cr_table = cr_table;
            dc._type = &en;
            dc._hash = hash;
            dc._meta = meta;
        });
        return ord;
    }

private:

    static ifcman& get() {
        LOCAL_FUNCTION_PROCWIDE_SINGLETON_DEF(ifcman) _m = new ifcman;
        return *_m;
    }

    ifcman() {
        _clients.reserve_virtual(8192);
    }

    type_sequencer<ifcman> _seq;
    dynarray32<data_ifc_descriptor> _clients;
};

COID_NAMESPACE_END

////////////////////////////////////////////////////////////////////////////////
///Helper struct for interface registration
struct ifc_autoregger
{
    COIDNEWDELETE(ifc_autoregger);

    typedef void (*register_fn)(bool);

    ifc_autoregger(register_fn fn) : _fn(fn) {
        _fn(true);
    }

    ~ifc_autoregger() {
        _fn(false);
    }

private:
    register_fn _fn;
};

// OBSOLETE: use UseLibraryDependencyInputs on project reference to force linking interface registrations

///Force registration of an interface declared in a statically-linked library
#define FORCE_REGISTER_LIBRARY_INTERFACE(ns,ifc) \
    namespace ns { [[deprecated("use UseLibraryDependencyInputs to force linking library interface registrations")]] void* force_register_##ifc(); static void* autoregger_##ifc = force_register_##ifc(); }

///Force registration of an interface declared in a statically-linked library
#define FORCE_REGISTER_LIBRARY_INTERFACE2(ifc) \
     [[deprecated("use UseLibraryDependencyInputs to force linking library interface registrations")]] void* force_register_##ifc(); static void* autoregger_##ifc = force_register_##ifc()


///Force registration of a script binder in a statically-linked library
#define FORCE_REGISTER_BINDER_INTERFACE(ns,ifc,script) \
    namespace ns { namespace script { [[deprecated("use UseLibraryDependencyInputs to force linking library interface registrations")]] void* force_register_##ifc(); static void* autoregger_##ifc = force_register_##ifc(); }}


/// Do not call these. Their purpose is only to expand __COUNTER__ value
#define IFC_REGISTER_CLIENT_Y__(client, ord) \
    static int _autoregger_##ord = client::register_client<client>()
#define IFC_REGISTER_CLIENT_X__(client, ord) IFC_REGISTER_CLIENT_Y__(client, ord)

///Register a derived client interface class
#define IFC_REGISTER_CLIENT(client) IFC_REGISTER_CLIENT_X__(client, __COUNTER__);

///Handler not implemented message
#ifdef _DEBUG
#define HANDLER_NOT_IMPLEMENTED_MESSAGE __FUNCTION__ " handler not implemented"
#else
#define HANDLER_NOT_IMPLEMENTED_MESSAGE "handler not implemented"
#endif //_DEBUG

#endif //__INTERGEN_IFC_H__
