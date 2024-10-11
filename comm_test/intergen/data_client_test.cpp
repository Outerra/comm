#include "ifc/component_ifc.h"


int callback(int x) {
    return x + 1;
}

void data_client_test()
{
    component_ifc* pd = component_ifc::creator();
    DASSERT_RET(pd);

    int v = 0;
    pd->set_int(33);
    pd->set_text("joo", &v);
    DASSERT(v == 33);

    pd->set_callback(&callback);

    pd->set_int(123);
    DASSERT(pd->get_int() == 124);
}
