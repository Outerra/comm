
//@file  javascript interface dispatcher generated by intergen v8
//See LICENSE file for copyright and license information

#include "ifc/component_ifc.h"
#include "data.hpp"

#include <comm/intergen/ifc.js.h>
#include <comm/metastream/metastream.h>
#include <comm/metastream/fmtstream_v8.h>
#include <comm/binstream/filestream.h>
#include <comm/binstream/binstreambuf.h>

using namespace coid;


////////////////////////////////////////////////////////////////////////////////
//
// javascript handler of data interface component_ifc of class component
//
////////////////////////////////////////////////////////////////////////////////

namespace js {

////////////////////////////////////////////////////////////////////////////////
class component_ifc_js_dispatcher
    : public ::js::interface_context
{
public:

    v8::Handle<v8::Object> create_interface_object(v8::Handle<v8::Context> context);

    COIDNEWDELETE(component_ifc_js_dispatcher);

    //component_ifc_js_dispatcher()
    //{}

    explicit component_ifc_js_dispatcher(::component* host)
    {
        ifc = entman::get_versionid<::component>(host);
    }

    ~component_ifc_js_dispatcher()
    {}

    // --- creators ---

    static iref<component_ifc_js_dispatcher> creator(const ::js::script_handle& scriptpath, const coid::token& bindname, v8::Handle<v8::Context>*);

    static v8::Handle<v8::Value> v8creator_creator0(const v8::ARGUMENTS& args);

    // --- method wrappers ---

    static void v8_set_a0(const v8::ARGUMENTS& args);
    static void v8_set_b1(const v8::ARGUMENTS& args);

private:

    cref<::component> ifc;
};

////////////////////////////////////////////////////////////////////////////////
void component_ifc_js_dispatcher::v8_set_a0(const v8::ARGUMENTS& args)
{
    v8::Isolate* iso = args.GetIsolate();

    if (args.Length() < 1 || args.Length() > 1) { //in/inout arguments
        coid::charstr tmp = "Wrong number of arguments in ";
        tmp << "component_ifc.set_a";
        return v8::queue_js_exception(iso, v8::Exception::SyntaxError, tmp);
    }

    v8::HandleScope handle_scope__(iso);
    v8::Local<v8::Object> obj__ = args.Holder();
    if (obj__.IsEmpty() || obj__->InternalFieldCount() == 0)
        return (void)args.GetReturnValue().Set(v8::Undefined(iso));

    v8::Local<v8::Value> intobj__ = obj__->GetInternalField(0);
    if (!intobj__->IsExternal())
        return (void)args.GetReturnValue().Set(v8::Undefined(iso));

    component_ifc_js_dispatcher* disp_ = static_cast<component_ifc_js_dispatcher*>(v8::Handle<v8::External>::Cast(intobj__)->Value());
    ::component* R_ = disp_->ifc.ready();

    if (!R_) {
        coid::charstr tmp = "Null interface object in ";
        tmp << "component_ifc.set_a";
        return v8::queue_js_exception(iso, &v8::Exception::ReferenceError, tmp);
    }

    v8::Local<v8::Context> ctx = iso->GetCurrentContext();
    v8::Context::Scope context_scope(ctx);

    try {
        //stream the arguments in
        static_assert(coid::has_metastream_operator<int>::value, "missing metastream operator for 'int'");

        threadcached<int> b;
        write_from_v8(args[0], b);

        //invoke

        R_->set_a(b);

        //stream out
        v8::Handle<v8::Object> r__;

        args.GetReturnValue().Set(r__);

    } catch (const coid::exception& e) {
        return v8::queue_js_exception(iso, &v8::Exception::TypeError, e.text());
    }
}

////////////////////////////////////////////////////////////////////////////////
void component_ifc_js_dispatcher::v8_set_b1(const v8::ARGUMENTS& args)
{
    v8::Isolate* iso = args.GetIsolate();

    if (args.Length() < 1 || args.Length() > 1) { //in/inout arguments
        coid::charstr tmp = "Wrong number of arguments in ";
        tmp << "component_ifc.set_b";
        return v8::queue_js_exception(iso, v8::Exception::SyntaxError, tmp);
    }

    v8::HandleScope handle_scope__(iso);
    v8::Local<v8::Object> obj__ = args.Holder();
    if (obj__.IsEmpty() || obj__->InternalFieldCount() == 0)
        return (void)args.GetReturnValue().Set(v8::Undefined(iso));

    v8::Local<v8::Value> intobj__ = obj__->GetInternalField(0);
    if (!intobj__->IsExternal())
        return (void)args.GetReturnValue().Set(v8::Undefined(iso));

    component_ifc_js_dispatcher* disp_ = static_cast<component_ifc_js_dispatcher*>(v8::Handle<v8::External>::Cast(intobj__)->Value());
    ::component* R_ = disp_->ifc.ready();

    if (!R_) {
        coid::charstr tmp = "Null interface object in ";
        tmp << "component_ifc.set_b";
        return v8::queue_js_exception(iso, &v8::Exception::ReferenceError, tmp);
    }

    v8::Local<v8::Context> ctx = iso->GetCurrentContext();
    v8::Context::Scope context_scope(ctx);

    try {
        //stream the arguments in
        static_assert(coid::has_metastream_operator<coid::token>::value, "missing metastream operator for 'coid::token'");

        threadcached<coid::token> a;
        write_from_v8(args[0], a);

        //invoke
        int b;

        R_->set_b(a, &b);

        //stream out
        static_assert(coid::has_metastream_operator<int>::value, "missing metastream operator for 'int'");
        v8::Handle<v8::Value> r__ = read_to_v8(b);
 
        args.GetReturnValue().Set(r__);

    } catch (const coid::exception& e) {
        return v8::queue_js_exception(iso, &v8::Exception::TypeError, e.text());
    }
}



////////////////////////////////////////////////////////////////////////////////
v8::Handle<v8::Object> component_ifc_js_dispatcher::create_interface_object(v8::Handle<v8::Context> context)
{
    v8::Isolate* iso = v8::Isolate::GetCurrent();

    static v8::Persistent<v8::ObjectTemplate> _objtempl;
    if (_objtempl.IsEmpty())
    {
        v8::Local<v8::ObjectTemplate> ot = v8::ObjectTemplate::New(iso);

        ot->Set(v8::Symbol::GetToStringTag(iso), v8::string_utf8("component_ifc"));
        ot->SetInternalFieldCount(2);    //ptr and class hash id

        ot->Set(v8::symbol("set_a", iso), v8::FunctionTemplate::New(iso, &v8_set_a0));
        ot->Set(v8::symbol("set_b", iso), v8::FunctionTemplate::New(iso, &v8_set_b1));

        _objtempl.Reset(iso, ot);
    }

    v8::Context::Scope ctxscope(context);
    v8::Local<v8::Object> obj = _objtempl.Get(iso)->NewInstance(context).ToLocalChecked();

    v8::Handle<v8::External> map_ptr = v8::External::New(iso, this);
    obj->SetInternalField(0, map_ptr);
    v8::Handle<v8::External> hash_ptr = v8::External::New(iso, (void*)ints(889931507));
    obj->SetInternalField(1, hash_ptr);

    return obj;
}

////////////////////////////////////////////////////////////////////////////////
///Create JS interface from a host
static v8::Handle<v8::Value> create_js_object_from_host_component_ifc(::component* host, v8::Handle<v8::Context> ctx)
{
    // check that the orig points to an object
    v8::Isolate* iso = ctx.IsEmpty() ? v8::Isolate::GetCurrent() : ctx->GetIsolate();
    if (!host)
        return v8::Null(iso);

    if (ctx.IsEmpty())
        ctx = iso->GetCurrentContext();

    v8::Context::Scope context_scope(ctx);
    v8::EscapableHandleScope scope(iso);

    // create interface object
    js::component_ifc_js_dispatcher* ifc = new js::component_ifc_js_dispatcher(host);

    //stream out
    v8::Handle<v8::Value> r__ = v8::Handle<v8::Value>(ifc->create_interface_object(ctx));

    return scope.Escape(r__);
}

////////////////////////////////////////////////////////////////////////////////
static void register_binders_for_component_ifc(bool on)
{
    //maker from script from data host
    interface_register::register_interface_creator("component@dcmaker.js", on ? (void*)&create_js_object_from_host_component_ifc : nullptr, nullptr);
}

//auto-register the bind function
LOCAL_SINGLETON_DEF(ifc_autoregger) component_ifc_autoregger = new ifc_autoregger(&register_binders_for_component_ifc);


void* force_register_component_ifc() {
    LOCAL_SINGLETON_DEF(ifc_autoregger) autoregger = new ifc_autoregger(&register_binders_for_component_ifc);
    return autoregger.get();
}

} //namespace js

