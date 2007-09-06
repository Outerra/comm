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

COID_NAMESPACE_BEGIN

class binstream;
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
struct binstream_container
{
    virtual ~binstream_container() {}

    ///Provide a pointer to next object that should be streamed
    ///@param n number of objects to allocate the space for
    virtual const void* extract( uints n ) = 0;
    virtual void* insert( uints n ) = 0;

    ///@return true if the storage is continuous in memory
    virtual bool is_continuous() const = 0;

    typedef opcd (*fnc_stream)(binstream&, void*,binstream_container&);


    binstream_container( uints n, bstype::kind t, fnc_stream fout, fnc_stream fin )
        : _stream_in(fin),_stream_out(fout),_type(t),_nelements(n)
    {
    }

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


    fnc_stream _stream_in;
    fnc_stream _stream_out;

    ///Type information about streamed object
    bstype::kind _type;

    ///Number of objects to stream, UMAX if not known in advance
    uints _nelements;
};


///Structure with streaming functions for abstract type
template<class T>
struct binstream_streamfunc
{
    static opcd stream_in( binstream& bin, void* p, binstream_container& )
    {
        try { bin >> *(T*)p; }
        catch( opcd e ) { return e; }
        return 0;
    }

    static opcd stream_out( binstream& bin, void* p, binstream_container& )
    {
        try { bin << *(const T*)p; }
        catch( opcd e ) { return e; }
        return 0;
    }
};

////////////////////////////////////////////////////////////////////////////////
///Templatized base container
///@param T type held by the container
template<class T>
struct binstream_containerT : binstream_container
{
    enum { ELEMSIZE = sizeof(T) };
    typedef T       TData;

    binstream_containerT( uints n ) : binstream_container(n,bstype::t_type<T>(),
        &binstream_streamfunc<T>::stream_out,
        &binstream_streamfunc<T>::stream_in)
    {}
    binstream_containerT( uints n, fnc_stream fout, fnc_stream fin )
        : binstream_container(n,bstype::t_type<T>(),fout,fin)
    {}
};

///
template<>
struct binstream_containerT<void> : binstream_container
{
    enum { ELEMSIZE = 1 };
    typedef void    TData;

    binstream_containerT( uints n ) : binstream_container(n,bstype::kind(bstype::kind::T_BINARY,1),0,0)
    {}
};

////////////////////////////////////////////////////////////////////////////////
///Adapter class used to retrieve appropriate binstream container type for 
/// specific container, for writing to the data container
///@param CONT data container
///@param BINCONT corresponding binstream container
template<class CONT, class BINCONT>
struct binstream_container_writable
{
    ///Define following methods when specializing, changing container-type to 
    /// something appropriate
    typedef BINCONT     TBinstreamContainer;

    static TBinstreamContainer create( CONT& a, uints n = UMAX )
    {   return TBinstreamContainer(a,n);    }
};

///Adapter class used to retrieve appropriate binstream container type for 
/// specific container, for reading from the data container
///@param CONT data container
///@param BINCONT corresponding binstream container
template<class CONT, class BINCONT>
struct binstream_container_readable
{
    ///Define following methods when specializing, changing container-type to 
    /// something appropriate
    typedef BINCONT     TBinstreamContainer;

    static TBinstreamContainer create( CONT& a )
    {   return TBinstreamContainer(a);      }
};

///this would be declared using the macros below
template<class CONT>
struct binstream_adapter_writable
{
    typedef CONT        TContainer;
    //typedef BINCONT     TBinstreamContainer;    ///< Override BINCONT here
};

template<class CONT>
struct binstream_adapter_readable
{
    typedef CONT        TContainer;
    //typedef BINCONT     TBinstreamContainer;    ///< Override BINCONT here
};


#define PAIRUP_CONTAINERS_WRITABLE(CONT) \
    template<class T, class A> struct binstream_adapter_writable< CONT<T,A> > { \
    typedef CONT<T,A>   TContainer; \
    typedef typename CONT<T,A>::binstream_container TBinstreamContainer; };

#define PAIRUP_CONTAINERS_READABLE(CONT) \
    template<class T> struct binstream_adapter_readable< CONT<T> > { \
    typedef CONT<T,A>   TContainer; \
    typedef typename CONT<T,A>::binstream_container TBinstreamContainer; };

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
template<class T>
struct binstream_dereferencing_containerT : binstream_containerT<T>
{
    enum { ELEMSIZE = sizeof(T) };

    typedef binstream_container::fnc_stream    fnc_stream;


    virtual const void* extract( uints n )      { return *(T**)_bc.extract(n); }
    virtual void* insert( uints n )
    {
        T** p = (T**)_bc.insert(n);
        *p = new T;
        return *p;
    }

    ///@return true if the storage is continuous in memory
    virtual bool is_continuous() const          { return false; }


    binstream_dereferencing_containerT( binstream_container& bc )
        : binstream_containerT<T>( bc._nelements,
        &binstream_streamfunc<T>::stream_out,
        &binstream_streamfunc<T>::stream_in),
        _bc(bc)
    {}

    binstream_dereferencing_containerT( binstream_container& bc, fnc_stream fout, fnc_stream fin )
        : binstream_containerT<T>( bc._nelements, fout, fin ), _bc(bc)
    {}

protected:
    binstream_container& _bc;
};

///Dereferencing container for containers holding pointers, source container embedded here
template<class T,class CONT>
struct binstream_dereferencing_containerTC : binstream_containerT<T>
{
    enum { ELEMSIZE = sizeof(T) };

    typedef binstream_container::fnc_stream    fnc_stream;


    virtual const void* extract( uints n )      { return *(T**)_bc.extract(n); }
    virtual void* insert( uints n )
    {
        T** p = (T**)_bc.insert(n);
        *p = new T;
        return *p;
    }

    ///@return true if the storage is continuous in memory
    virtual bool is_continuous() const          { return false; }


    binstream_dereferencing_containerTC( const CONT& bc )
        : binstream_containerT<T>( bc._nelements,
        &binstream_streamfunc<T>::stream_out,
        &binstream_streamfunc<T>::stream_in),
        _bc(bc)
    {}

    binstream_dereferencing_containerTC( const CONT& bc, fnc_stream fout, fnc_stream fin )
        : binstream_containerT<T>( bc._nelements, fout, fin ), _bc(bc)
    {}

protected:
    CONT _bc;
};



////////////////////////////////////////////////////////////////////////////////
///Primitive abstract base container
struct binstream_container_primitive : binstream_container
{
    binstream_container_primitive( uints n, bstype::kind t ) : binstream_container(n,t,0,0)
    {}

};

////////////////////////////////////////////////////////////////////////////////
///Container for writting from a prepared array
template<class T>
struct binstream_container_fixed_array : binstream_containerT<T>
{
    const void* extract( uints n )
    {
        T* p=_ptr;
        _ptr = ptr_byteshift(_ptr,n*binstream_containerT<T>::ELEMSIZE);
        return p;
    }

    void* insert( uints n )
    {
        T* p=_ptr;
        _ptr = ptr_byteshift(_ptr,n*binstream_containerT<T>::ELEMSIZE);
        return p;
    }

    bool is_continuous() const      { return true; }

    typedef binstream_container::fnc_stream    fnc_stream;

    binstream_container_fixed_array( T* ptr, uints n ) : binstream_containerT<T>(n), _ptr(ptr) {}
    binstream_container_fixed_array( const T* ptr, uints n ) : binstream_containerT<T>(n), _ptr((T*)ptr) {}
    binstream_container_fixed_array( T* ptr, uints n, fnc_stream fout, fnc_stream fin ) : binstream_containerT<T>(n,fout,fin), _ptr(ptr) {}
    binstream_container_fixed_array( const T* ptr, uints n, fnc_stream fout, fnc_stream fin ) : binstream_containerT<T>(n,fout,fin), _ptr((T*)ptr) {}

    void set( const T* ptr, uints n )
    {
        this->_nelements = n;
        _ptr = (T*)ptr;
    }

protected:
    T* _ptr;
};


////////////////////////////////////////////////////////////////////////////////
///Container for writting from a prepared array
struct binstream_container_char_array : binstream_containerT<char>
{
    const void* extract( uints n )
    {
        char* p = _ptr;
        _ptr += n;
        return p;
    }

    void* insert( uints n )
    {
        uints nres = align_value_to_power2( _size, 5 );   //32B blocks
        if( n+_size > nres ) {
            nres = align_value_to_power2(_size+n,5);
            _ptr = (char*) realloc(_ptr,nres);
        }
        char* p = _ptr + _size-1;
        p[n] = 0;
        _size += n;
        return p;
    }

    bool is_continuous() const      { return true; }

    binstream_container_char_array( uints n ) : binstream_containerT<char>(n)
    {
        _ptr = (char*)malloc(1<<5);
        _ptr[0] = 0;
        _size = 1;
    }
    binstream_container_char_array( const char* ptr, uints n ) : binstream_containerT<char>(n), _ptr((char*)ptr)
    {
        _size = 0;
    }

    void set( const char* ptr, uints n )
    {
        this->_nelements = n;
        _ptr = (char*)ptr;
        _size = 0;
    }

    const char* get() const         { return _ptr; }

protected:
    char* _ptr;
    uints _size;
};
/*
////////////////////////////////////////////////////////////////////////////////
///Container for r/w from fixed array, but using different type for streaming
template<class WRAPPER, class CONTENT>
struct binstream_typechanging_container : public binstream_containerT<WRAPPER>
{
    const void* extract( uints n )
    {
        return (const void*)_ptr++;
    }

    void* insert( uints n )
    {
        return (void*)_ptr++;
    }

    bool is_continuous() const      { return false; }

    binstream_typechanging_container( const CONTENT* ptr, uints n )
        : binstream_containerT<WRAPPER>(n,&stream_out,&stream_in), _ptr(ptr) {}

    void set( const CONTENT* ptr, uints n )
    {
        this->_nelements = n;
        _ptr = (CONTENT*)ptr;
    }

protected:
    static opcd stream_out( binstream& bin, void* p, binstream_container& )
    {
        WRAPPER w( *(const CONTENT*)p );
        try { bin << w; }
        catch( opcd e ) { return e; }
        return 0;
    }

    static opcd stream_in( binstream& bin, void* p, binstream_container& )
    {
        WRAPPER w( *(CONTENT*)p );
        try { bin >> w; }
        catch( opcd e ) { return e; }
        return 0;
    }

protected:
    const CONTENT* _ptr;
};
*/

COID_NAMESPACE_END

#endif //__COID_COMM_BINSTREAM_CONTAINER__HEADER_FILE__
