#include <iostream>

#define NEW_REFS

#ifdef NEW_REFS
#include <comm/ref.h>
#else
#endif

struct base0
{
    int a;
    virtual ~base0()
    {
        std::cout << "base0 destructor called!" << std::endl;
    };
};

struct base1
{
    float b;
    virtual ~base1()
    {
        std::cout << "base1 destructor called!" << std::endl;
    };
};

struct inherited : base0, base1
{
    virtual ~inherited()
    {
        std::cout << "inherited destructor called!" << std::endl;
    };
};


void refs_test()
{
    ref<inherited> i;
    i.create(new inherited);
    
    ref<base0> b0 = i;
    ref<base0> b1 = i;

    i.release();
    b0.release();
    b1.release();
}