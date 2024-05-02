/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is COID/comm module.
 *
 * The Initial Developer of the Original Code is
 * PosAm.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brano Kemen
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __COID_COMM_BINSTREAM_CONTAINER__HEADER_FILE__
#define __COID_COMM_BINSTREAM_CONTAINER__HEADER_FILE__

#include "../namespace.h"

#include "bstype.h"
#include "../alloc/_malloc.h"
 
COID_NAMESPACE_BEGIN

class binstream;
class metastream;
struct opcd;


////////////////////////////////////////////////////////////////////////////////
///Base class for writting and reading multiple objects to and from binstream
/**
    The container object provides pointers to objects that should be streamed to or
    from the binstream, via the next() method. In order to be able to optimize
    operations with the container, it provides also the is_continuous() method, that
    says whether the storage is continuous so that the pointer returned first can
    be used to address successive objects too.
**/
struct binstream_container_base
{
    virtual ~binstream_container_base() {}

    ///Provide a pointer to next object that should be streamed
    /// @param n number of objects to allocate the space for
    virtual const void* extract(uints n) = 0;
    virtual void* insert(uints n, const void* defval) = 0;

    /// @return true if the storage is continuous in memory
    virtual bool is_continuous() const = 0;

    /// @return number of items in container (for reading), UMAXS if unknown in advance
    virtual uints count() const = 0;

    typedef void (*fnc_stream)(metastream*, void*, binstream_container_base*);


    binstream_container_base(bstype::kind t, fnc_stream fout, fnc_stream fin)
        : _stream_in(fin)
        , _stream_out(fout)
        , _type(t)
    {}

    ///Set flag in _type to inform binstream methods that the array didn't specify
    /// its size in advance.
    ///This is used in binary streams to write and read prefix marks before objects
    /// to figure out where the array ends.
    bstype::kind set_array_needs_separators()
    {
        _type.set_array_unspecified_size();
        return _type;
    }

    bool array_needs_separators() const
    {
        return _type.is_array_unspecified_size();
    }

    opcd stream_in(metastream* m, void* p) {
        _stream_in(m, p, this);
        return ersNOERR;
    }

    opcd stream_out(metastream* m, void* p) {
        _stream_out(m, p, this);
        return ersNOERR;
    }


    fnc_stream _stream_in;
    fnc_stream _stream_out;

    ///Type information about streamed object
    bstype::kind _type;
};


///
template<class COUNT>
struct binstream_container : binstream_container_base
{
    typedef COUNT   count_t;

    binstream_container(bstype::kind t, fnc_stream fout, fnc_stream fin)
        : binstream_container_base(t, fout, fin)
    {}
};

////////////////////////////////////////////////////////////////////////////////
///Forward declaration, definition in metastream.h
template<class T>
struct type_streamer {
    static void fn(metastream* m, void* p, binstream_container_base*);
};

///Templatized base container
//@param T type held by the container
//@param COUNT unsigned integer type for storing count
template<class T, class COUNT = uints>
struct binstream_containerT : binstream_container<COUNT>
{
    typedef T       data_t;
    typedef binstream_container_base::fnc_stream    fnc_stream;

    binstream_containerT()
        : binstream_container<COUNT>(bstype::t_type<T>(), &type_streamer<T>::fn, &type_streamer<T>::fn)
    {}

    binstream_containerT(fnc_stream fout, fnc_stream fin)
        : binstream_container<COUNT>(bstype::t_type<T>(), fout, fin)
    {}
    /*
        //template<class T>
        static void fnstream( metastream* m, void* p, binstream_container_base& bc ) {
            *m || *static_cast<typename resolve_enum<T>::type*>(p);
        }*/
};


///this would be declared using the macros below
template<class CONTAINER>
struct binstream_adapter_writable
{
    typedef CONTAINER   TContainer;
    //typedef BINCONT     TBinstreamContainer;    //< Override BINCONT here
};

template<class CONTAINER>
struct binstream_adapter_readable
{
    typedef CONTAINER   TContainer;
    //typedef BINCONT     TBinstreamContainer;    //< Override BINCONT here
};
/*
#define PAIRUP_CONTAINERS_WRITABLE(CONT) \
    template<class T, class A> struct binstream_adapter_writable< CONT<T,A> > { \
    typedef CONT<T,A>   TContainer; \
    typedef typename CONT<T,A>::binstream_container TBinstreamContainer; };

#define PAIRUP_CONTAINERS_READABLE(CONT) \
    template<class T> struct binstream_adapter_readable< CONT<T> > { \
    typedef CONT<T,A>   TContainer; \
    typedef typename CONT<T,A>::binstream_container TBinstreamContainer; };*/

#define PAIRUP_CONTAINERS_WRITABLE2(CONT,BINCONT) \
    template<class T, class A> struct binstream_adapter_writable< CONT<T,A> > { \
    typedef CONT<T,A>       TContainer; \
    typedef BINCONT<TContainer>    TBinstreamContainer; };

#define PAIRUP_CONTAINERS_READABLE2(CONT,BINCONT) \
    template<class T, class A> struct binstream_adapter_readable< CONT<T,A> > { \
    typedef CONT<T,A>       TContainer; \
    typedef BINCONT<TContainer>    TBinstreamContainer; };

    ////////////////////////////////////////////////////////////////////////////////
    ///Generic dereferencing container for containers holding pointers
template<class T, class COUNT>
struct binstream_dereferencing_containerT
    : binstream_containerT<T, COUNT>
{
    typedef binstream_container_base::fnc_stream    fnc_stream;


    virtual const void* extract(uints n) override
    {
        return *(T**)_bc.extract(n);
    }

    virtual void* insert(uints n, const void* defval) override
    {
        T** p = (T**)_bc.insert(n, defval);
        *p = new T;
        return *p;
    }

    /// @return true if the storage is continuous in memory
    virtual bool is_continuous() const override { return false; }

    virtual uints count() const override { return _bc.count(); }


    binstream_dereferencing_containerT(binstream_container<COUNT>& bc)
        : binstream_containerT<T, COUNT>()
        , _bc(bc)
    {}

    binstream_dereferencing_containerT(binstream_container<COUNT>& bc, fnc_stream fout, fnc_stream fin)
        : binstream_containerT<T, COUNT>(fout, fin)
        , _bc(bc)
    {}

protected:
    binstream_container<COUNT>& _bc;
};

///Generic dereferencing container for containers holding ref<T>
template<class T, class RefT, class COUNT>
struct binstream_dereferencing_containerRefT
    : binstream_containerT<T, COUNT>
{
    typedef binstream_container_base::fnc_stream    fnc_stream;


    virtual const void* extract(uints n) override
    {
        return &(**(RefT*)_bc.extract(n));
    }

    virtual void* insert(uints n, const void* defval) override
    {
        RefT* p = (RefT*)_bc.insert(n, defval);
        *p = RefT(new T);
        return &(**p);
    }

    /// @return true if the storage is continuous in memory
    virtual bool is_continuous() const override { return false; }

    virtual uints count() const override { return _bc.count(); }


    binstream_dereferencing_containerRefT(binstream_container<COUNT>& bc)
        : binstream_containerT<T, COUNT>()
        , _bc(bc)
    {}

    binstream_dereferencing_containerRefT(binstream_container<COUNT>& bc, fnc_stream fout, fnc_stream fin)
        : binstream_containerT<T, COUNT>(fout, fin)
        , _bc(bc)
    {}

protected:
    binstream_container<COUNT>& _bc;
};

////////////////////////////////////////////////////////////////////////////////
///Container for writting from a prepared array
template<class T, class COUNT>
struct binstream_container_fixed_array : binstream_containerT<T, COUNT>
{
    virtual const void* extract(uints n) override
    {
        if (n > uints(_pte - _ptr))
            throw ersNO_MORE;

        T* p = _ptr;
        _ptr = ptr_advance(_ptr, n);
        return p;
    }

    virtual void* insert(uints n, const void* defval) override
    {
        if (n > uints(_pte - _ptr))
            throw ersNO_MORE;

        T* p = _ptr;
        if (defval) {
            for (uints i = 0; i < n; ++i)
                p[i] = *static_cast<const T*>(defval);
        }

        _ptr = ptr_advance(_ptr, n);
        return p;
    }

    virtual bool is_continuous() const override { return true; }

    virtual uints count() const override { return _pte - _ptr; }

    typedef typename binstream_container_base::fnc_stream    fnc_stream;

    binstream_container_fixed_array(T* ptr, uints n)
        : binstream_containerT<T, COUNT>(), _ptr(ptr), _pte(ptr + n) {}
    binstream_container_fixed_array(const T* ptr, uints n)
        : binstream_containerT<T, COUNT>(), _ptr((T*)ptr), _pte((T*)ptr + n) {}
    binstream_container_fixed_array(T* ptr, uints n, fnc_stream fout, fnc_stream fin)
        : binstream_containerT<T, COUNT>(fout, fin), _ptr(ptr), _pte(ptr + n) {}
    binstream_container_fixed_array(const T* ptr, uints n, fnc_stream fout, fnc_stream fin)
        : binstream_containerT<T, COUNT>(fout, fin), _ptr((T*)ptr), _pte((T*)ptr + n) {}

    void set(const T* ptr, uints n)
    {
        _ptr = (T*)ptr;
        _pte = _ptr + n;
    }

protected:
    T* _ptr;
    T* _pte;
};


////////////////////////////////////////////////////////////////////////////////
///Container for writting from a prepared array
template<class COUNT>
struct binstream_container_char_array : binstream_containerT<char, COUNT>
{
    virtual const void* extract(uints n) override
    {
        char* p = _ptr;
        _ptr += n;
        return p;
    }

    virtual void* insert(uints n, const void* defval) override
    {
        uints nres = align_value_to_power2(_size, 5);   //32B blocks
        if (n + _size > nres) {
            nres = align_value_to_power2(_size + n, 5);
            _ptr = (char*)::dlrealloc(_ptr, nres);
        }
        char* p = _ptr + _size - 1;
        p[n] = 0;
        _size += n;
        return p;
    }

    virtual bool is_continuous() const override { return true; }

    virtual uints count() const override { return _size; }

    binstream_container_char_array(uints n)
    {
        _ptr = (char*)::dlmalloc(1 << 5);
        _ptr[0] = 0;
        _size = 1;
    }
    binstream_container_char_array(const char* ptr, uints n)
        : _ptr((char*)ptr)
        , _size(n)
    {
    }

    void set(const char* ptr, uints n)
    {
        _ptr = (char*)ptr;
        _size = n;
    }

    const char* get() const { return _ptr; }

protected:
    char* _ptr;
    uints _size;
};


COID_NAMESPACE_END

#endif //__COID_COMM_BINSTREAM_CONTAINER__HEADER_FILE__
