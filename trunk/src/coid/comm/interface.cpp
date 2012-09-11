
#include "interface.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
void* interface_register::get_interface_creator( const token& ifcname )
{
    interface_register& reg = interface_register::get();
    GUARDTHIS(reg._mx);

    const entry* en = reg._hash.find_value(ifcname);

    return en ? en->creator : 0;
}

////////////////////////////////////////////////////////////////////////////////
void interface_register::register_interface_creator( const token& ifcname, void* creator )
{
    interface_register& reg = interface_register::get();
    GUARDTHIS(reg._mx);

    entry* en = reg._hash.insert_value_slot(ifcname);
    if(en) {
        en->creator = creator;
        en->ifcname = ifcname;
    }
}

////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define INTERGEN_GLOBAL_REGISTRAR iglo_registrard
#else
#define INTERGEN_GLOBAL_REGISTRAR iglo_registrar
#endif



#ifdef SYSTYPE_WIN

typedef int (__stdcall *proc_t)();

extern "C"
__declspec(dllimport) proc_t __stdcall GetProcAddress (
    void* hmodule,
    const char* procname
    );

typedef interface_register* (*ireg_t)();

#define MAKESTR(x) STR(x)
#define STR(x) #x

////////////////////////////////////////////////////////////////////////////////
interface_register& interface_register::get()
{
    static interface_register* _this=0;

    if(!_this) {
        const char* s = MAKESTR(INTERGEN_GLOBAL_REGISTRAR);
        ireg_t p = (ireg_t)GetProcAddress(0, s);
        _this = p();
    }

    return *_this;
}


extern "C" __declspec(dllexport) interface_register* INTERGEN_GLOBAL_REGISTRAR()
{
    return &SINGLETON(interface_register);
}


#else
/*
extern "C" __attribute__ ((visibility("default"))) interface_register* INTERGEN_GLOBAL_REGISTRAR();
{
    return &SINGLETON(interface_register);
}*/

interface_register& interface_register::get()
{
    static interface_register _this;

    return _this;
}

#endif

COID_NAMESPACE_END
