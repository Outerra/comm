
#include "interface.h"
#include "commexception.h"
#include "hash/hashkeyset.h"
#include "sync/mutex.h"

COID_NAMESPACE_BEGIN

struct entry
{
    token ifcname;
    token classname;
    token creatorname;
    token ns;

    void* creator_ptr;
    bool script_creator;

    operator const token&() const { return ifcname; }
};

////////////////////////////////////////////////////////////////////////////////
class interface_register_impl : public interface_register
{
public:

    hash_keyset<entry, _Select_GetRef<entry,token> > _hash;
    comm_mutex _mx;

    static interface_register_impl& get();

    interface_register_impl() : _mx(500, false)
    {}
};

////////////////////////////////////////////////////////////////////////////////
void* interface_register::get_interface_creator( const token& ifcname )
{
    interface_register_impl& reg = interface_register_impl::get();
    GUARDTHIS(reg._mx);

    const entry* en = reg._hash.find_value(ifcname);

    return en ? en->creator_ptr : 0;
}

////////////////////////////////////////////////////////////////////////////////
dynarray<interface_register::creator>& interface_register::get_interface_creators( const token& name, dynarray<interface_register::creator>& dst )
{
    //interface creator names:
    // [ns1::[ns2:: ...]]::class.creator
    static token SEP = "::";
    token ns = name;
    token classname = ns.cut_right_group_back(SEP);
    token creatorname = classname.cut_right('.', token::cut_trait_remove_sep_default_empty());

    interface_register_impl& reg = interface_register_impl::get();
    GUARDTHIS(reg._mx);

    auto i = reg._hash.begin();
    auto ie = reg._hash.end();
    for(; i!=ie; ++i) {
        if(i->script_creator)
            continue;

        if(i->classname != classname)
            continue;

        if(creatorname && i->creatorname != creatorname)
            continue;

        if(ns && i->ns != ns)
            continue;

        creator* p = dst.add();
        p->creator_ptr = i->creator_ptr;
        p->name = token(i->ifcname);
    }

    return dst;
}

////////////////////////////////////////////////////////////////////////////////
dynarray<interface_register::creator>& interface_register::find_interface_creators( const regex& name, dynarray<interface_register::creator>& dst )
{
    //interface creator names:
    // [ns1::[ns2:: ...]]::class.creator

    interface_register_impl& reg = interface_register_impl::get();
    GUARDTHIS(reg._mx);

    auto i = reg._hash.begin();
    auto ie = reg._hash.end();
    for(; i!=ie; ++i) {
        if(i->script_creator)
            continue;

        if(name.match(i->ifcname)) {
            creator* p = dst.add();
            p->creator_ptr = i->creator_ptr;
            p->name = token(i->ifcname);
        }
    }

    return dst;
}

////////////////////////////////////////////////////////////////////////////////
void interface_register::register_interface_creator( const token& ifcname, void* creator_ptr )
{
    static token SEP = "::";

    token ns = ifcname;
    token classname = ns.cut_right_group_back(SEP);
    token creatorname = classname.cut_right('.');
    token script = creatorname.cut_right('@', token::cut_trait_remove_sep_default_empty());

    interface_register_impl& reg = interface_register_impl::get();
    GUARDTHIS(reg._mx);

    entry* en = reg._hash.insert_value_slot(ifcname);
    if(en) {
        en->creator_ptr = creator_ptr;
        en->ifcname = ifcname;
        en->ns = ns;
        en->classname = classname;
        en->creatorname = creatorname;
        en->script_creator = !script.is_empty();
    }
}

////////////////////////////////////////////////////////////////////////////////

//#ifdef _DEBUG
//#define INTERGEN_GLOBAL_REGISTRAR iglo_registrard
//#else
#define INTERGEN_GLOBAL_REGISTRAR iglo_registrar
//#endif



#ifdef SYSTYPE_WIN

typedef int (__stdcall *proc_t)();

extern "C"
__declspec(dllimport) proc_t __stdcall GetProcAddress (
    void* hmodule,
    const char* procname
    );

typedef interface_register_impl* (*ireg_t)();

#define MAKESTR(x) STR(x)
#define STR(x) #x

////////////////////////////////////////////////////////////////////////////////
interface_register_impl& interface_register_impl::get()
{
    static interface_register_impl* _this=0;

    if(!_this) {
        const char* s = MAKESTR(INTERGEN_GLOBAL_REGISTRAR);
        ireg_t p = (ireg_t)GetProcAddress(0, s);
        if(!p) throw exception() << "no intergen entry point found";
        _this = p();
    }

    return *_this;
}


extern "C" __declspec(dllexport) interface_register_impl* INTERGEN_GLOBAL_REGISTRAR()
{
    return &SINGLETON(interface_register_impl);
}


#else
/*
extern "C" __attribute__ ((visibility("default"))) interface_register_impl* INTERGEN_GLOBAL_REGISTRAR();
{
    return &SINGLETON(interface_register_impl);
}*/

interface_register_impl& interface_register_impl::get()
{
    static interface_register_impl _this;

    return _this;
}

#endif

COID_NAMESPACE_END
