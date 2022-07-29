#include "ifc/data_ifc.h"


void data_client_test()
{
    data_ifc* pd = data_ifc::creator();
    DASSERT(pd);

    int v = 0;
    pd->set_a(33);
    pd->set_b("joo", &v);
    DASSERT(v == 33);
}
