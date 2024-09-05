#include "ifc/component_ifc.h"


void data_client_test()
{
    component_ifc* pd = component_ifc::creator();
    DASSERT_RET(pd);

    int v = 0;
    pd->set_a(33);
    pd->set_b("joo", &v);
    DASSERT(v == 33);
}
