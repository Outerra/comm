#pragma once

#include <comm/intergen/ifc.h>


struct component
{
    ifc_struct(component_ifc, "ifc/");

    ifc_fn static component* creator() {
        return new component;
    }

    ifc_fn void set_a(int b) {
        _b = b;
    }

    ifc_fn void set_b(const coid::token& a, ifc_out int* b) {
        _a = a;
        *b = _b;
    }

    int _b;
    coid::charstr _a;
};
