
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
 * PosAm.
 * Portions created by the Initial Developer are Copyright (C) 2003-2023
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

#ifndef __COID_COMM_INTERFACE__HEADER_FILE__
#define __COID_COMM_INTERFACE__HEADER_FILE__

#include "namespace.h"
#include "str.h"
#include "dynarray.h"
#include "regex.h"
#include "log/logger.h"
#include "ref_i.h"

class intergen_interface;

COID_NAMESPACE_BEGIN

class binstring;

namespace meta {

template <typename T>
struct stream_op {
    static void fn(metastream& m, void* v) {
        if constexpr (std::is_enum_v<T>)
            m || *(std::underlying_type_t<T>*)v;
        else if constexpr (!has_metastream_operator<T>::value)
            ;
        else
            m || *(T*)v;
    }
};

struct arg
{
    const token name;                   //< parameter name
    const token type;                   //< parameter type (stripped of const qualifier)
    const token basetype;               //< base type (stripped of the last ptr/ref)
    const token argsize;                //< size expression if the parameter is an array, including [ ]
    const token fnargs;                 //< argument list of a function-type argument
    const token memfnclass;             //< member fn class
    const token defval;                 //< default value expression

    void (*fn_metastream)(metastream&, void*);

    /// @brief Explicit usage of 'enum', 'struct' or 'class' in argument declaration
    enum class ex_type : uint8 {
        unspecified,
        enuma,
        structa,
        classa
    };

    /// @brief Argument mapping to an interface type
    enum class ifc_type : uint8 {
        none,
        ifc_class,
        ifc_struct,
    };

    ifc_type ifctype = ifc_type::none;

    bool bspecptr       = false;        //< special type where pointer is not separated (e.g const char*)
    bool bptr           = false;        //< true if the type is a pointer
    bool bref           = false;        //< true if the type is a reference
    bool bxref          = false;        //< true if the type is xvalue reference
    bool biref          = false;
    bool bconst         = false;        //< true if the type had const qualifier
    bool binarg         = true;         //< input type argument
    bool boutarg        = false;        //< output type argument
    bool bvolatile      = false;
    bool bnojs          = false;        //< not used in script, use default val
    bool bfnarg         = false;        //< function type arg

    const token doc;
};


struct method
{
    const token name;                   //< method name

    enum class flg : uint {
        bstatic        = 0x0001,        //< a static (creator) method
        bptr           = 0x0002,        //< ptr instead of ref
        biref          = 0x0004,        //< iref instead of ref
        bifccr         = 0x0008,        //< ifc returning creator (not returning host)
        bconst         = 0x0010,        //< const method
        boperator      = 0x0020,        //<
        binternal      = 0x0040,        //< internal method, invisible to scripts (starts with an underscore)
        bcapture       = 0x0080,        //< method captured when interface is in capturing mode
        bimplicit      = 0x0100,        //< an implicit event/method
        bdestroy       = 0x0200,        //< a method to call on interface destroy
        bnoevbody      = 0x0400,        //< mandatory event
        bpure          = 0x0800,        //< pure virtual on client
        bduplicate     = 0x1000,        //< a duplicate method/event from another interface of the host
        binherit       = 0x2000,        //< method inherited from base interface
    };

    flg flags = flg(0);                 //< interface flags

    uint nargs = 0;                     //< total number of arguments
    uint ninargs = 0;                   //< number of input arguments
    uint noutargs = 0;                  //< number of output arguments

    const arg* args = 0;                //< return value and arguments

    const token comments;               //< doc paragraphs
};

inline method::flg operator | (method::flg a, method::flg b) { return method::flg(uint(a) | uint(b)); }
inline method::flg& operator |= (method::flg& a, method::flg b) { return a = method::flg(uint(a) | uint(b)); }
inline bool operator & (method::flg a, method::flg b) { return (uint(a) & uint(b)) != 0; }


/// @brief meta-data for interfaces to classes
struct class_interface
{
    const token nsname;                 //< fully qualified interface name (with namespaces)
    const token hdrfile;
    const token storage;

    const token baseclass;              //< only the base class name

    uint hash;

    uint ncreators = 0;
    uint nmethods = 0;
    uint nevents = 0;
    const method* creators = 0;
    const method* methods = 0;
    const method* events = 0;

    int destroy_method = -1;
    int default_creator_method = -1;
    int oper_get = -1;
    int oper_set = -1;

    enum class flg : uint {
        bvirtual = 1,
        bdataifc = 2,
    };

    flg flags = flg(0);                 //< interface flags

    const token comments;
};

inline class_interface::flg operator | (class_interface::flg a, class_interface::flg b) { return class_interface::flg(uint(a) | uint(b)); }
inline class_interface::flg& operator |= (class_interface::flg& a, class_interface::flg b) { return a = class_interface::flg(uint(a) | uint(b)); }
inline bool operator & (class_interface::flg a, class_interface::flg b) { return (uint(a) & uint(b)) != 0; }


} //namespace meta

////////////////////////////////////////////////////////////////////////////////
///A global register for interfaces, used by intergen
class interface_register
{
public:

    static bool register_interface(const meta::class_interface& ifcmeta, const void* func);

    static bool register_interface_creator(const token& ifcname, void* creator, const meta::class_interface* ifcmeta);

    static void* get_interface_creator(const token& ifcname);

    /// @param curpath current path
    /// @param incpath absolute path or path relative to curpath
    /// @param dst [out] result path
    /// @param relpath [out] gets relative path from root. If null, relative incpath can only refer to a sub-path below curpath
    static bool include_path(const token& curpath, const token& incpath, charstr& dst, token* relpath);

    static const charstr& root_path();


    typedef ref<logmsg>(*fn_log_t)(log::level, const token&, const void*, log::target);
    typedef bool(*fn_acc_t)(const token&);
    typedef logger*(*fn_getlog_t)();

    static void setup(const token& path, fn_log_t log, fn_acc_t access, fn_getlog_t getlogfn);

    typedef iref<intergen_interface>(*host_to_ifc_fn)(void*, intergen_interface*);
    typedef intergen_interface* (*client_fn)();

    struct creator {
        token name;
        union {
            void* creator_ptr = 0;
            host_to_ifc_fn fn;              //< creates interface from host, (host*, interface* opt)
            client_fn fn_client;
        };
    };

    ///Get interface-to-host connector/creator matching the given interface name
    /// @param name interface creator name in the format [ns1::[ns2:: ...]]::class
    /// @return function that creates interface from an existing host
    static host_to_ifc_fn get_interface_connector(const token& name);

    ///Get data interface-to-host connector/creator matching the given interface name
    /// @param name interface creator name in the format [ns1::[ns2:: ...]]::class
    /// @return function that creates data interface from an existing host
    static void* get_interface_dcconnector(const token& name);

    ///Get interface wrapper function that creates script interface from another interface
    /// @param name interface creator name in the format [ns1::[ns2:: ...]]::class
    /// @param script script type
    /// @return function that creates script handle (bound to a script interface) from another interface
    static void* get_script_interface_wrapper(const token& name, const token& script);

    ///Get interface maker creator matching the given interface name
    /// @param name interface creator name in the format [ns1::[ns2:: ...]]::class
    /// @param script script type
    /// @return maker function that creates interface from an existing host
    static void* get_script_interface_maker(const token& name, const token& script);

    ///Get interface data wrapper matching the given interface name
    /// @param name interface creator name in the format [ns1::[ns2:: ...]]::class
    /// @param script script type
    /// @return function returning script interface, with first parameter const coref<ifc>&
    static void* get_script_interface_dcwrapper(const token& name, const token& script);

    ///Get interface data maker creator matching the given interface name
    /// @param name interface creator name in the format [ns1::[ns2:: ...]]::class
    /// @param script script type
    /// @return function returning script-specific handle, with first parameter host*
    static void* get_script_interface_dcmaker(const token& name, const token& script);

    ///Get client interface creator matching the given interface name
    /// @param client client name
    /// @param iface interface name in the format [ns1::[ns2:: ...]]::class
    /// @param module required module to match
    static client_fn get_interface_client(const token& client, const token& iface, uint hash, const token& module);

    ///Get client interface creators matching the given interface name
    /// @param iface interface name in the format [ns1::[ns2:: ...]]::class
    /// @param module required module to match
    static dynarray<creator>& get_interface_clients(const token& iface, uint hash, dynarray<creator>& dst);

    template <typename T>
    static dynarray<creator>& get_interface_clients(dynarray<creator>& dst) {
        static_assert(std::is_base_of<intergen_interface, T>::value, "expected intergen_interface derived class");

        return get_interface_clients(T::IFCNAME(), T::HASHID, dst);
    }

    ///Get interface creators matching the given interface name
    /// @param name interface creator name in the format [ns1::[ns2:: ...]]::class[.creator]
    /// @param script script type (""=c++, "js", "lua" ...), if empty/null anything matches
    /// @return array of interface creators for given script type (with script_handle argument)
    static dynarray<creator>& get_interface_creators(const token& name, const token& script, dynarray<creator>& dst);

    ///Get script interface creators matching the given interface name
    /// @param name interface creator name in the format [ns1::[ns2:: ...]]::class[.creator]
    /// @param script script type (""=c++, "js", "lua" ...), if empty/null anything matches
    /// @return array of interface creators for given script type (with native script lib argument)
    static dynarray<creator>& get_script_interface_creators(const token& name, const token& script, dynarray<creator>& dst);

    ///Find interfaces containing given string
    static dynarray<creator>& find_interface_creators(const regex& str, dynarray<creator>& dst);

    static dynarray<const meta::class_interface*>& find_interface_meta_info(const regex& str, dynarray<const meta::class_interface*>& dst);

    struct unload_entry {
        charstr ifcname;
        uint bstrofs = 0;                   //< position in binstring
        uint bstrlen = 0;
    };

    ///Send notification about client handlers unloading
    /// @param handle dll handle to unload, or 0 if sending notifications about reload
    /// @param bstr ptr to binstring object for persisting the state
    /// @param ens list of unloaded entries
    static bool notify_module_unload(uints handle, binstring* bstr, dynarray<unload_entry>& ens);

    static ref<logmsg> canlog(log::level type, const token& src, const void* inst, log::target target);
    static logger* getlog();

#ifdef COID_VARIADIC_TEMPLATES

    ///Formatted log message
    /// @param type log level
    /// @param from source identifier (used for filtering)
    /// @param fmt @see charstr.print
    template<class ...Vs>
    static void print(log::level type, const token& from, const token& fmt, Vs&&... vs)
    {
        ref<logmsg> msgr = canlog(type, from);
        if (!msgr)
            return;

        charstr& str = msgr->str();
        str.print(fmt, std::forward<Vs>(vs)...);
    }

#endif //COID_VARIADIC_TEMPLATES
};


COID_NAMESPACE_END

#endif //__COID_COMM_INTERFACE__HEADER_FILE__
