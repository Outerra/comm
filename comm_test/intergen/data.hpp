#pragma once

#include <comm/intergen/ifc.h>


struct component
{
    IFC_STRUCT(component_ifc, "ifc/");

    ifc_callback(fn_callback, int (*)(int));

    ifc_fn static component* creator() {
        return new component;
    }

    ifc_fn void set_int(int b) {
        _b = b;
        if (_fn)
            _b = _fn(b);
    }

    ifc_fn void set_text(const coid::token& a, ifc_out int* b) {
        _a = a;
        *b = _b;
    }

    ifc_fn int get_int() const { return _b; }

    ifc_fn coid::token get_text() const { return _a; }

    ifc_fn void set_callback(fn_callback fn) {
        _fn = fn;
    }

    int _b;
    coid::charstr _a;

    fn_callback _fn = nullptr;
};
