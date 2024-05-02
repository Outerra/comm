#pragma once

#include "../dynarray.h"
#include "binstream.h"
#include "container.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
template< class T, class COUNT = uints, class A = comm_array_allocator>
struct dynarray_binstream_container : public binstream_containerT<T, COUNT>
{
    using fnc_stream = binstream_container_base::fnc_stream;


    virtual const void* extract(uints n) override
    {
        DASSERT(_pos + n <= _v.size());
        const T* p = &_v[_pos];
        _pos += n;
        return p;
    }

    virtual void* insert(uints n, const void* defval) override
    {
        _v._assert_allowed_size(n);
        T* p = _v.add(COUNT(n));
        if (defval) {
            for (uints i = 0; i < n; ++i)
                p[i] = *static_cast<const T*>(defval);
        }
        return p;
    }

    virtual bool is_continuous() const override { return true; }

    virtual uints count() const override { return _v.size(); }

    dynarray_binstream_container(const dynarray<T, COUNT, A>& v)
        : _v(const_cast<dynarray<T, COUNT, A>&>(v))
    {
        _pos = 0;
    }

    dynarray_binstream_container(const dynarray<T, COUNT, A>& v, fnc_stream fout, fnc_stream fin)
        : binstream_containerT<T, COUNT>(fout, fin)
        , _v(const_cast<dynarray<T, COUNT, A>&>(v))
    {
        _pos = 0;
    }

protected:
    dynarray<T, COUNT, A>& _v;
    uints _pos;
};

///
template< class T, class COUNT = uints, class A = comm_array_allocator>
struct dynarray_binstream_dereferencing_container : public binstream_containerT<T, COUNT>
{
    using fnc_stream = binstream_container_base::fnc_stream;

    virtual const void* extract(uints n) override
    {
        DASSERT(n == 1);
        const T* p = _v[_pos];
        _pos += n;
        return p;
    }

    virtual void* insert(uints n, const void* defval) override
    {
        DASSERT(n == 1);
        return defval ? new (_v.add()) T(*static_cast<const T*>(defval)) : new(_v.add()) T;
    }

    virtual bool is_continuous() const override {
        return false;
    }

    virtual uints count() const override { return _v.size(); }

    dynarray_binstream_dereferencing_container(const dynarray<T*>& v)
        : _v(const_cast<dynarray<T*>&>(v))
    {
        _pos = 0;
    }

    dynarray_binstream_dereferencing_container(const dynarray<T*>& v, fnc_stream fout, fnc_stream fin)
        : binstream_containerT<T, COUNT>(fout, fin)
        , _v(const_cast<dynarray<T*>&>(v))
    {
        _pos = 0;
    }

protected:
    dynarray<T*>& _v;
    uints _pos;
};


////////////////////////////////////////////////////////////////////////////////
template <class T, class COUNT, class A>
inline binstream& operator << (binstream& out, const dynarray<T, COUNT, A>& dyna)
{
    binstream_container_fixed_array<T, COUNT> c(dyna.ptr(), dyna.size());
    return out.xwrite_array(c);
}

template <class T, class COUNT, class A>
inline binstream& operator >> (binstream& in, dynarray<T, COUNT, A>& dyna)
{
    dyna.reset();
    dynarray_binstream_container<T, COUNT, A> c(dyna);

    return in.xread_array(c);
}

template <class T, class COUNT, class A>
inline binstream& operator << (binstream& out, const dynarray<T*, COUNT, A>& dyna)
{
    binstream_container_fixed_array<T*, COUNT> c(dyna.ptr(), dyna.size(), 0, 0);
    binstream_dereferencing_containerT<T, COUNT> dc(c);
    return out.xwrite_array(dc);
}

template <class T, class COUNT, class A>
inline binstream& operator >> (binstream& in, dynarray<T*, COUNT, A>& dyna)
{
    dyna.reset();
    dynarray_binstream_container<T*, COUNT, A> c(dyna, UMAXS, 0, 0);
    binstream_dereferencing_containerT<T, COUNT> dc(c);

    return in.xread_array(dc);
}


////////////////////////////////////////////////////////////////////////////////

template <class T, class COUNT, class A>
struct binstream_adapter_writable< dynarray<T, COUNT, A> > {
    typedef dynarray<T, COUNT, A> TContainer;
    typedef dynarray_binstream_container<T, COUNT, A> TBinstreamContainer;
};

template <class T, class COUNT, class A>
struct binstream_adapter_readable< dynarray<T, COUNT, A> > {
    typedef const dynarray<T, COUNT, A> TContainer;
    typedef dynarray_binstream_container<T, COUNT, A> TBinstreamContainer;
};



COID_NAMESPACE_END
