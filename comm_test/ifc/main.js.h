#pragma once

#ifndef __INTERGEN_GENERATED__main_JS_H__
#define __INTERGEN_GENERATED__main_JS_H__

//@file Javascript interface file for main interface generated by intergen
//See LICENSE file for copyright and license information

#include "main.h"

#include <comm/intergen/ifc.js.h>
#include <comm/token.h>

namespace ns {
namespace js {

class main
{
public:

    //@param scriptpath path to js script to bind to
    static iref<ns::main> create( const ::js::script_handle& script, const coid::token& bindvar = coid::token(), v8::Handle<v8::Context>* ctx=0 )
    {
        typedef iref<ns::main> (*fn_bind)(const ::js::script_handle&, const coid::token&, v8::Handle<v8::Context>*);
        static fn_bind binder = 0;
        static const coid::token ifckey = "ns::main.create@creator.js";

        if (!binder)
            binder = reinterpret_cast<fn_bind>(
                coid::interface_register::get_interface_creator(ifckey));

        if (!binder)
            throw coid::exception("interface binder inaccessible: ") << ifckey;

        return binder(script, bindvar, ctx);
    }

    //@param scriptpath path to js script to bind to
    static iref<ns::main> create_special( const ::js::script_handle& script, int a, iref<ns::other> b, int& c, iref<ns::other>& d, int e = -1, const coid::token& bindvar = coid::token(), v8::Handle<v8::Context>* ctx=0 )
    {
        typedef iref<ns::main> (*fn_bind)(const ::js::script_handle&, int, iref<ns::other>, int&, iref<ns::other>&, int, const coid::token&, v8::Handle<v8::Context>*);
        static fn_bind binder = 0;
        static const coid::token ifckey = "ns::main.create_special@creator.js";

        if (!binder)
            binder = reinterpret_cast<fn_bind>(
                coid::interface_register::get_interface_creator(ifckey));

        if (!binder)
            throw coid::exception("interface binder inaccessible: ") << ifckey;

        return binder(script, a, b, c, d, e, bindvar, ctx);
    }

    //@param scriptpath path to js script to bind to
    static iref<ns::main> create_wp( const ::js::script_handle& script, int a, int& b, int& c, int d = -1, const coid::token& bindvar = coid::token(), v8::Handle<v8::Context>* ctx=0 )
    {
        typedef iref<ns::main> (*fn_bind)(const ::js::script_handle&, int, int&, int&, int, const coid::token&, v8::Handle<v8::Context>*);
        static fn_bind binder = 0;
        static const coid::token ifckey = "ns::main.create_wp@creator.js";

        if (!binder)
            binder = reinterpret_cast<fn_bind>(
                coid::interface_register::get_interface_creator(ifckey));

        if (!binder)
            throw coid::exception("interface binder inaccessible: ") << ifckey;

        return binder(script, a, b, c, d, bindvar, ctx);
    }
};

} //namespace js
} //namespace


#endif //__INTERGEN_GENERATED__main_JS_H__
