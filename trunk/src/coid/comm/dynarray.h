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

#ifndef __COID_COMM_DYNARRAY__HEADER_FILE__
#define __COID_COMM_DYNARRAY__HEADER_FILE__

#include "namespace.h"

#include "commtypes.h"
#include "binstream/binstream.h"
#include "alloc/commalloc.h"

#include <iterator>
#include <algorithm>

//define NO_DYNARRAY_DEBUG_FILL symbol in projects when you don't want additional
// memset


COID_NAMESPACE_BEGIN

//helper insert/delete fnc
template <class T> T* __move( ints nitemsfwd, T* ptr, uints nitems) {
    if(nitems)
        memmove(ptr+nitemsfwd, ptr, nitems*sizeof(T));
    return ptr;
}
template <> inline void* __move( ints nitemsfwd, void* ptr, uints nitems) { return __move<char>(nitemsfwd,(char*)ptr,nitems); }

template <class T> T* __ins(T* &ptr, uints nfrom, uints nlen, uints nins=1) {
    return __move<T>(nins, ptr+nfrom, nlen-nfrom);
}
template <> inline void* __ins(void* &ptr, uints nfrom, uints nlen, uints nins) { return __ins<char>((char*&)ptr,nfrom,nlen,nins); }

template <class T> T* __del(T* &ptr, uints nfrom, uints nlen, uints ndel=1) {
    memmove(ptr+nfrom, ptr+nfrom+ndel, (nlen-ndel-nfrom)*sizeof(T));
    return ptr;
}
template <> inline void* __del(void* &ptr, uints nfrom, uints nlen, uints ndel) { return __del<char>((char*&)ptr,nfrom,nlen,ndel); }


////////////////////////////////////////////////////////////////////////////////
template<class T>
struct _dynarray_eptr : std::iterator<std::random_access_iterator_tag, T>
{
    T*  _p;             ///<ptr to the managed item

    typedef T          value_type;
    typedef ptrdiff_t  difference_type;

    //operator value_type () const            { return *_p; }
    //operator const value_type () const      { return *_p; }

    difference_type operator - (const _dynarray_eptr& p) const     { return _p - p._p; }

    bool operator < (const _dynarray_eptr& p) const    { return _p < p._p; }
    bool operator <= (const _dynarray_eptr& p) const   { return _p <= p._p; }

    int operator == (const T* p) const  { return p == _p; }

    int operator == (const _dynarray_eptr& p) const    { return p._p == _p; }
    int operator != (const _dynarray_eptr& p) const    { return p._p != _p; }

    T& operator *(void) const           { return *_p; }

#ifdef SYSTYPE_MSVC
#pragma warning( disable : 4284 )
#endif //SYSTYPE_MSVC
    T* operator ->(void) const          { return _p; }

    _dynarray_eptr& operator ++()       { ++_p;  return *this; }
    _dynarray_eptr& operator --()       { --_p;  return *this; }

    _dynarray_eptr  operator ++(int) { _dynarray_eptr x(_p);  ++_p;  return x; }
    _dynarray_eptr  operator --(int) { _dynarray_eptr x(_p);  --_p;  return x; }

    _dynarray_eptr& operator += (ints n){ _p += n;  return *this; }

    _dynarray_eptr  operator + (ints n) const { return _dynarray_eptr(_p+n); }
    _dynarray_eptr  operator - (ints n) const { return _dynarray_eptr(_p-n); }

    const T& operator [] (ints i) const { return _p[i]; }
    T& operator [] (ints i)             { return _p[i]; }

    T* ptr() const                      { return _p; }

    _dynarray_eptr() { _p = 0; }
    _dynarray_eptr( T* p ) : _p(p)  { }
};

////////////////////////////////////////////////////////////////////////////////
template<class T>
struct _dynarray_const_eptr : std::iterator<std::random_access_iterator_tag, T>
{
    const T*  _p;             ///<ptr to the managed item

    typedef T          value_type;
    typedef ptrdiff_t  difference_type;

    //operator value_type () const            { return *_p; }
    //operator const value_type () const      { return *_p; }

    difference_type operator - (const _dynarray_const_eptr& p) const     { return _p - p._p; }

    bool operator < (const _dynarray_const_eptr& p) const    { return _p < p._p; }
    bool operator <= (const _dynarray_const_eptr& p) const   { return _p <= p._p; }

    int operator == (const T* p) const  { return p == _p; }

    int operator == (const _dynarray_const_eptr& p) const    { return p._p == _p; }
    int operator != (const _dynarray_const_eptr& p) const    { return p._p != _p; }

    const T& operator *(void) const     { return *_p; }

#ifdef SYSTYPE_MSVC
#pragma warning( disable : 4284 )
#endif //SYSTYPE_MSVC
    const T* operator ->(void) const    { return _p; }

    _dynarray_const_eptr& operator ++()         { ++_p;  return *this; }
    _dynarray_const_eptr& operator --()         { --_p;  return *this; }

    _dynarray_const_eptr  operator ++(int) { _dynarray_const_eptr x(_p);  ++_p;  return x; }
    _dynarray_const_eptr  operator --(int) { _dynarray_const_eptr x(_p);  --_p;  return x; }

    _dynarray_const_eptr& operator += (ints n)  { _p += n;  return *this; }

    _dynarray_const_eptr  operator + (ints n) const { return _dynarray_const_eptr(_p+n); }
    _dynarray_const_eptr  operator - (ints n) const { return _dynarray_const_eptr(_p-n); }

    const T& operator [] (ints i) const { return _p[i]; }

    const T* ptr() const                        { return _p; }

    _dynarray_const_eptr() { _p = 0; }
    _dynarray_const_eptr( const T* p ) : _p(p)  { }

    _dynarray_const_eptr( const _dynarray_eptr<T>& i )
    {
        _p = i._p;
    }
};


////////////////////////////////////////////////////////////////////////////////
//Fw
#ifndef SYSTYPE_MSVC6
template<class T, class A> class dynarray;
template<class T, class A> binstream& operator << (binstream&, const dynarray<T,A>& );
template<class T, class A> binstream& operator >> (binstream&, dynarray<T,A>& );
#endif //SYSTYPE_MSVC


///Template managing arrays of objects
template< class T, class A=comm_array_allocator<T> >
class dynarray
{
    enum {
        CHUNKSIZE               = 4
    };
    uints _count() const        { return A::count(_ptr); }
    uints _size() const         { return A::size(_ptr); }
    void _set_count(uints n)    { A::set_count(_ptr, n); }

public:

    typedef T                   value_type;

    COIDNEWDELETE

    T* _ptr;

    dynarray() {
        A::instance();
        _ptr = 0;
    }

    ///Reserve specified number of items in constructor
    explicit dynarray( uints reserve_count ) {
        A::instance();
        _ptr = 0;
        reserve( reserve_count, false );
    }

    ~dynarray() {
        discard();
    }

    ///copy constructor
    dynarray( const dynarray& p )
    {
        //A::instance();
        //_ptr = p._ptr;
        _ptr = 0;
        uints n = p.size();
        alloc(n);
        for( uints i=0; i<n; ++i ) {
            _ptr[i] = p._ptr[i];
        }
    }

    ///assignment operator - duplicate
    dynarray& operator = ( const dynarray& p )
    {
        uints n = p.size();
        alloc(n);
        for( uints i=0; i<n; ++i )
            _ptr[i] = p._ptr[i];

        return *this;
    }

    ///Assign a pointer that fulfills dynarray ptr requirements.
    ///If you don't know what it means, do not use it.
    dynarray& set_dynarray_conforming_ptr( T* ptr )
    {
        _ptr = ptr;
        return *this;
    }

    ///take control over buffer controlled by \a dest dynarray, \a dest ptr will be set to zero
    dynarray& takeover( dynarray& src )
    {
        discard();
        _ptr = src._ptr;
        src._ptr = 0;
        return *this;
    }

    ///Swap content with another dynarray
    dynarray& swap( dynarray& dest )
    {
        T* t = dest._ptr;
        dest._ptr = _ptr;
        _ptr = t;
        return *this;
    }
/*
    ///Share control over buffer controlled by \a dest dynarray.
    dynarray& share( dynarray& dest )
    {
        discard();
        _ptr = dest._ptr;
        return *this;
    }

    ///release shared array
    dynarray& unshare()
    {
        _ptr = 0;
        return *this;
    }
*/
    T* ptr()                    { return _ptr; }
    T* ptre()                   { return _ptr + _count(); }

    const T* ptr() const        { return _ptr; }
    const T* ptre() const       { return _ptr + _count(); }

#ifdef SYSTYPE_MSVC8plus
    template< typename T, typename A >
    friend binstream& operator << (binstream &out, const dynarray<T,A> &dyna);
    template< typename T, typename A >
    friend binstream& operator >> (binstream &in, dynarray<T,A> &dyna);
#else
    friend binstream& operator << TEMPLFRIEND (binstream &out, const dynarray<T,A> &dyna);
    friend binstream& operator >> TEMPLFRIEND (binstream &in, dynarray<T,A> &dyna);
#endif

    typedef binstream_container::fnc_stream	fnc_stream;

    ////////////////////////////////////////////////////////////////////////////////
    struct binstream_container : public binstream_containerT<T>
    {
        virtual const void* extract( uints n )
        {
            const T* p = &_v[_pos];
            _pos += n;
            return p;
        }

        virtual void* insert( uints n )
        {
            return _v.add(n);
        }

        virtual bool is_continuous() const      { return true; }
		binstream_container( dynarray<T>& v, uints n=UMAXS )
            : binstream_containerT<T>(n), _v(v)
        {
            _pos = 0;
        }

		binstream_container( dynarray<T>& v, uints n, fnc_stream fout, fnc_stream fin )
            : binstream_containerT<T>(n,fout,fin), _v(v)
        {
            _pos = 0;
        }

    protected:
        dynarray<T>& _v;
        uints _pos;
    };

    
    ///Debug checks
#ifdef _DEBUG
# define DYNARRAY_CHECK_BOUNDS_S(k)      __check_bounds((ints)k);
# define DYNARRAY_CHECK_BOUNDS_U(k)      __check_bounds((uints)k);
#else
# define DYNARRAY_CHECK_BOUNDS_S(k)
# define DYNARRAY_CHECK_BOUNDS_U(k)
#endif

    void __check_bounds(ints k) const    { DASSERT( k>=0 && (uints)k<_count() ); }
    void __check_bounds(uints k) const   { DASSERT( k<_count() ); }

    const T& operator [] (uints k) const { DYNARRAY_CHECK_BOUNDS_U(k)  return *(_ptr+k); }
    T& operator [] (uints k)             { DYNARRAY_CHECK_BOUNDS_U(k)  return *(_ptr+k); }


    bool operator == ( const dynarray<T>& a ) const
    {
        if( size() != a.size() )  return false;
        for( uints i=0; i<size(); ++i )
            if( _ptr[i] != a._ptr[i] )  return false;
        return true;
    }

    bool operator != ( const dynarray<T>& a ) const
    {
        return !operator == (a);
    }

    ///Get fresh array with \a nitems of elements
    /** Destroys all elements of array and adjusts the array to the required size
        @param nitems number of items to resize to
        @return pointer to the first element of array */
    T* alloc( uints nitems )
    {
        _destroy();
        uints n = _count();
        uints nalloc = nitems;

        if( nalloc > n  &&  nalloc*sizeof(T) > _size() ) {
            if( nalloc < 2*n )
                nalloc = 2*n;

            A::free(_ptr);

            _ptr = A::alloc(nalloc);
        }

        if(nitems) {
            if( !type_trait<T>::trivial_constr )
                for( uints i=0; i<nitems; ++i )  ::new(_ptr+i) T;
        }

#if defined(_DEBUG) && !defined(NO_DYNARRAY_DEBUG_FILL)
        ::memset( _ptr+nitems, 0xcd, (nalloc - nitems)*sizeof(T) );
#endif

        if(_ptr)  _set_count(nitems);
        return _ptr;
    };

    ///Get fresh array with \a nitems of elements and clear the memory
    /** Destroys all elements of array and adjusts the array to the required size
        @param nitems number of items to resize to
        @param toones fill memory with ones (true) or zeros (false) before calling default constructor of element
        @return pointer to the first element of array */
    T* calloc( uints nitems, bool toones=false )
    {
        _destroy();
        uints n = _count();
        uints nalloc = nitems;

        if( nalloc > n  &&  nalloc*sizeof(T) > _size() ) {
            if( nalloc < 2*n )
                nalloc = 2*n;

            A::free(_ptr);

            _ptr = A::alloc(nalloc);
        }

        if(nitems) {
            ::memset( _ptr, toones ? 0xff : 0x00, nitems * sizeof(T) );

            if( !type_trait<T>::trivial_constr )
                for( uints i=0; i<nitems; ++i )  ::new(_ptr+i) T;
        }

#if defined(_DEBUG) && !defined(NO_DYNARRAY_DEBUG_FILL)
        ::memset( _ptr+nitems, 0xcd, (nalloc - nitems)*sizeof(T) );
#endif

        if(_ptr)  _set_count(nitems);
        return _ptr;
    };


    ///Resize array to \a nitems of elements
    /** Truncates array if currently larger than desired count, or enlarges it if it is smaller
        @param nitems number of elements to resize to
        @return pointer to the first element of array */
    T* realloc( uints nitems )
    {
        uints n = _count();

        if( nitems == n )  return _ptr;
        if( nitems < n ) {
            if( !type_trait<T>::trivial_constr )
                for( uints i=n-1; i>nitems; --i )  _ptr[i].~T();
            _set_count(nitems);
            return _ptr;
        }
        uints nalloc = nitems;

        if( nalloc*sizeof(T) > _size() )
            nalloc = _realloc(nalloc, n);

#if defined(_DEBUG) && !defined(NO_DYNARRAY_DEBUG_FILL)
        ::memset( _ptr+n, 0xcd, (nalloc - n)*sizeof(T) );
#endif

        if( !type_trait<T>::trivial_constr )
            for( uints i=n; i<nitems; ++i )  ::new(_ptr+i) T;

        if(_ptr)  _set_count(nitems);
        return _ptr;
    };

    ///Resize array to \a nitems of elements
    /** Truncates array if currently larger than desired count, or enlarges it if it is smaller
        @param nitems number of elements to resize to
        @param toones fill memory with ones (true) or zeros (false) before calling default constructor of element
        @return pointer to the first element of array */
    T* crealloc( uints nitems, bool toones = false )
    {
        uints n = _count();

        if( nitems == n )  return _ptr;
        if( nitems < n ) {
            for( uints i=n-1; i>nitems; --i )
                _ptr[i].~T();
            _set_count(nitems);
            return _ptr;
        }

        uints nalloc = nitems;

        if( nalloc*sizeof(T) > _size() )
            nalloc = _realloc(nalloc, n);

        ::memset( _ptr+n, toones ? 0xff : 0x00, (nitems - n)*sizeof(T) );

#if defined(_DEBUG) && !defined(NO_DYNARRAY_DEBUG_FILL)
        ::memset( _ptr+nitems, 0xcd, (nalloc - nitems)*sizeof(T) );
#endif

        if( !type_trait<T>::trivial_constr )
            for( uints i=n; i<nitems; ++i )  ::new(_ptr+i) T;

        if(_ptr)  _set_count(nitems);
        return _ptr;
    };

    //@{ alternative names
    T* need_new( uints nitems )                 { return alloc(nitems); }
    T* need_newc( uints nitems, bool toones=false ) { return calloc(nitems, toones); }

    T* need( uints nitems )                     { return realloc(nitems); }
    T* needc( uints nitems, bool toones=false ) { return crealloc(nitems, toones); }
    //@} alternative names


    ///Cut to specified length, negative numbers cut abs(len) from the end
    dynarray<T,A>& resize( ints len )
    {
        if( len < 0 )
        {
            ints k = _count() + len;
            if( k <= 0 )
                reset();
            else
                realloc(k);
        }
        else
            realloc(len);

        return *this;
    }


    ///Add \a nitems of elements on the end
    /** @param nitems count of items to add
        @return pointer to the first added element */
    T* add( uints nitems=1 )
    {
        uints n = _count();

        if(!nitems)  return _ptr + n;
        uints nto = nitems + n;
        uints nalloc = nto;

        if( nalloc*sizeof(T) > _size() )
            nalloc = _realloc(nalloc, n);

#if defined(_DEBUG) && !defined(NO_DYNARRAY_DEBUG_FILL)
        ::memset( _ptr+n, 0xcd, (nalloc-n)*sizeof(T) );
#endif
        if( !type_trait<T>::trivial_constr )
            for( uints i=n; i<nto; ++i )  ::new(_ptr+i) T;

        _set_count(nto);
        return _ptr + n;
    };

    ///Add \a nitems of elements on the end and clear the memory
    /** @param nitems count of items to add
        @param toones fill memory with ones (true) or zeros (false) before calling default constructor of element
        @return pointer to the first added element */
    T* addc( uints nitems=1, bool toones = false )
    {
        uints n = _count();

        if(!nitems)  return _ptr + n;
        uints nto = nitems + n;
        uints nalloc = nto;

        if( nalloc*sizeof(T) > _size() )
            nalloc = _realloc(nto, n);

        ::memset( _ptr+n, toones ? 0xff : 0, (nto-n)*sizeof(T) );

#if defined(_DEBUG) && !defined(NO_DYNARRAY_DEBUG_FILL)
        ::memset( _ptr+nto, 0xcd, (nalloc-nto)*sizeof(T) );
#endif

        if( !type_trait<T>::trivial_constr )
            for( uints i=n; i<nto; ++i )  ::new(_ptr+i) T;

        _set_count(nto);
        return _ptr + n;
    };

    ///Add n new elements on position where key would be inserted.
    /// Uses either operator T<T or a functor(T,T)
    /// add_sortT() can use a different key type than T, provided an operator T<K exists, or functor(T,K) was provided.
    ///@param key key to use to find the element position in sorted array
    ///@param n number of elements to insert
    /** this uses the provided \a key just to find the position, doesn't insert the key
        @see push_sort() **/
    //@{
    T* add_sort( const T& key, uints n=1 )                  { return add_sortT<T>(key,n); }

    template<class FUNC>
    T* add_sort( const T& key, const FUNC& fn, uints n=1 )  { return add_sortT<T,FUNC>(key,fn,n); }

    template<class K>
    T* add_sortT( const K& key, uints n=1 )
    {
        uints i = lower_boundT<K>(key);
        return ins( i, n );
    }

    template<class K, class FUNC>
    T* add_sortT( const K& key, const FUNC& fn, uints n=1 )
    {
        uints i = lower_boundT<K,FUNC>(key,fn);
        return ins( i, n );
    }
    //@}


    ///Append an empty element to the end
    //@return pointer to the last element (the one appended)
    T* push()
    {
        return add();
    }

    ///Append element to the array through copy constructor
    T* push( const T& v )
    {
        T* ptr = add(1);
		*ptr = v;
        return ptr;
    };

    ///Append the same element n-times to the array through copy constructor
    T* pushn( const T& v, uints n )
    {
        T* ptr = add(n);
		for( T* p=ptr; n>0; --n,++p )
            *p = v;
        return ptr;
    };

    ///Append elements to the array through copy constructor
    T* pusha( uints n, const T* p )
    {
        T* ptr = add(n);
		for( uints i=0; i<n; ++i )  ptr[i] = p[i];
        return ptr;
    };

    ///Insert an element to the array at sorted position (using < operator)
    T* push_sort( const T& v )
    {
        uints i = lower_bound(v);
        ins( i, v );
        return _ptr+i;
    }

    ///Pop the last element, copying it to the \a dest
    //@return true if the array wasn't empty and the element was copied
    bool pop( T& dest )
    {
        uints n = _count();
        if( n > 0 ) {
            dest = *last();
            realloc(n-1);
            return true;
        }
        return false;
    }

    ///Pop the last element from the array, returning pointer to new last element.
    //@return pointer to the new last element or null if there's nothing left
    T* pop()
    {
        uints cnt = _count();
        if(!cnt)  return 0;

        realloc(cnt-1);

        return last();
    }

    ///Pop n elements from the array
    void popn( uints n )
    {
        if( n > _count() )
            throw ersOUT_OF_RANGE "cannot pop that much";

        del( _count()-n, n );
    }

    ///Append copy of another array to the end
    dynarray<T,A>& append( const dynarray<T,A>& a )
    {
        uints c = a.size();
        T* p = add(c);
        for (uints i=0; i<c; ++i)
            p[i] = a._ptr[i];
        return *this;
    }

    ///Raw fill the array
    /** @param in pointer to the source array from where to copy the data
        @param n number of elements to get
        @return pointer to the first element of array */
    T* copy_bin_from( const T* pin, uints n ) {
        T* p = alloc(n);
        xmemcpy( p, pin, n*sizeof(T) );
        return p;
    }

    ///Raw append the array
    /** @param in pointer to the source array from where to copy the data
        @param n number of elements to get
        @return pointer to the first element of array */
    T* add_bin_from( const T* pin, uints n ) {
        T* p = add(n);
        xmemcpy( p, pin, n*sizeof(T) );
        return p;
    }

    ///Raw copy content of the array
    /** @param out pointer to the target array whereto copy the data
        @param n number of elements to output
        @return pointer to the first element of the target array */
    T* copy_bin_to( T* pout, uints n ) const {
        xmemcpy( pout, _ptr, n<=_count() ? n*sizeof(T) : _count()*sizeof(T) );
        return pout;
    }

    ///Normal memcpy but with debug check if the source lies within the array
    void* memcpy_from( void* dst, const void* src, uints size )
    {
        DASSERT( src >= (const void*)ptr()  &&  (char*)src+size <= (char*)ptre() );
        return ::memcpy( dst, src, size );
    }

    ///Normal memcpy but with debug check if the destination lies within the array
    void* memcpy_to( void* dst, const void* src, uints size )
    {
        DASSERT( dst >= (void*)ptr()  &&  (char*)dst+size <= (char*)ptre() );
        return ::memcpy( dst, src, size );
    }

    ///Normal memset but with debug check if the destination lies within the array
    void* memset( void* dst, int v, uints size )
    {
        DASSERT( dst >= (void*)ptr()  &&  (char*)dst+size <= (char*)ptre() );
        return ::memset( dst, v, size );
    }

/*
    void clear( uints nitems=UMAXS, uints ufrm=0 )
    {
        RASSERTX( ufrm*sizeof(T) < _size(), "offset out of range" );
        if( (nitems+ufrm)*sizeof(T) > _size() )
            nitems = _size() - ufrm*sizeof(T);
        ::memset (_ptr+ufrm, 0, nitems);
    }
*/

    ///Reserve \a nitems of elements
    /** @param nitems number of items to reserve
        @param ikeep keep existing elements (true) or do not necessarily keep them (false)
        @return pointer to the first item of array */
    T* reserve( uints nitems, bool ikeep )
    {
        uints n = _count();

        if( nitems > n  &&  nitems*sizeof(T) > _size() )
        {
            if(!ikeep)
                _destroy();

            _realloc(nitems, n);
            _set_count(n);
        }
        return _ptr;
    }

    ///Get value from array or a default one if index is out of range
    T get_safe( uints i, const T& def ) const
    {
        return i < _count()  ?  _ptr[i]  :  def;
    }

    ///Get reference to an element or add if out of range
    T& get_or_add( uints i )
    {
        if( i < _count() )
            return _ptr[i];
        realloc(i+1);
        return _ptr[i];
    }

    ///Get reference to an element or add if out of range
    T& get_or_addc( uints i, bool toones=false )
    {
        if( i < _count() )
            return _ptr[i];
        crealloc( i+1, toones );
        return _ptr[i];
    }

    ///Set value at specified index, enlarging the array if index is out of range
    void set_safe( uints i, const T& val )
    {
        if( i >= _count() )
            realloc(i+1);
        _ptr[i] = val;
    }


    //{@ Bit-manipulating routines.
    ///Append n bits to dynarray
    T* realloc_bits( uints n )
    {
        uints nit = align_to_chunks(n,sizeof(T)*8);
        return realloc(nit);
    }

    ///Allocate n new bits in dynarray
    T* alloc_bits( uints n )
    {
        uints nit = align_to_chunks(n,sizeof(T)*8);
        return alloc(nit);
    }

    ///Append n bits to dynarray while setting them to requested value
    T* crealloc_bits( uints n, bool toones = false )
    {
        uints nit = align_to_chunks(n,sizeof(T)*8);
        return crealloc( nit, toones );
    }

    //Allocate n new bits in dynarray while setting them to requested value
    T* calloc_bits( uints n, bool toones = false )
    {
        uints nit = align_to_chunks(n,sizeof(T)*8);
        return calloc( nit, toones );
    }

    ///Set i-th bit 
    void set_bit( uints i, char t )
    {
        uints n = i >> 3;
        uints b = i & 0x07;
        ((uchar*)_ptr)[n] ^= (-t ^ ((uchar*)_ptr)[n]) & (1<<b);
    }

    ///Get i-th bit
    bool get_bit( uints i ) const
    {
        uints n = i >> 3;
        uints b = i & 0x07;
        return (((uchar*)_ptr)[n] & (1<<b)) != 0;
    }

    ///Set i-th bit in array, resizing the array if out of bounds
    void set_bit_safe( uints i, char t )
    {
        uints n = i >> 3;
        uints b = i & 0x07;
        crealloc( (n+sizeof(T))/sizeof(T) );
        ((uchar*)_ptr)[n] ^= (-t ^ ((uchar*)_ptr)[n]) & (1<<b);
    }

    ///Get i-th bit or default value if out of bounds
    bool get_bit_safe( uints i, bool def ) const
    {
        uints n = i >> 3;
        uints b = i & 0x07;
        if( n >= _count() )  return def;
        return (((uchar*)_ptr)[n] & (1<<b)) != 0;
    }
    //@}

    ///Linear search whether array contains element comparable with \a key
    ///@return -1 if not contained, otherwise index to the key
    //@{
    ints contains( const T& key ) const             { return containsT<T>(key); }

    template<class K>
    ints containsT( const K& key ) const
    {
        uints c = _count();
        for( uints i=0; i<c; ++i )
            if( _ptr[i] == key )  return i;
        return -1;
    }
    //@}

    ///Linear search (backwards) whether array contains element comparable with \a key
    ///@return -1 if not contained, otherwise index to the key
    //@{
    ints contains_back( const T& key ) const        { return contains_backT<T>(key); }

    template<class K>
    ints contains_backT( const K& key ) const
    {
        uints c = _count();
        for(; c>0; )
        {
            --c;
            if( _ptr[c] == key )  return c;
        }
        return -1;
    }
    //@}

    ///Binary search whether sorted array contains element comparable to \a key
    /// Uses operator T<K or functor(T,K) to search for the element, and operator T==K for equality comparison
    //@{
    ints contains_sorted( const T& key ) const                  { return contains_sortedT<T>(key); }

    template<class FUNC>
    ints contains_sorted( const T& key, const FUNC& fn ) const  { return contains_sortedT<T,FUNC>(key,fn); }

    template<class K>
    ints contains_sortedT( const K& key ) const
    {
        uints lb = lower_boundT<K>(key);
        if( lb >= size()
            || !(_ptr[lb] == key) )  return -1;
        return lb;
    }

    template<class K, class FUNC>
    ints contains_sortedT( const K& key, const FUNC& fn ) const
    {
        uints lb = lower_boundT<K,FUNC>(key,fn);
        if( lb >= size()
            || !(_ptr[lb] == key) )  return -1;
        return lb;
    }
    //@}


    ///Binary search sorted array using < operator (comparing T<T)
    uints lower_bound( const T& key ) const                     { return lower_boundT<T>(key); }

    ///Binary search sorted array using function object f(T,T) for comparing T<T
    template<class FUNC>
    uints lower_bound( const T& key, const FUNC& fn ) const     { return lower_boundT<T,FUNC>(key,fn); }

    ///Binary search sorted array using different type of key
    ///@note there must exist < operator able to do (T < K) comparison
    template<class K>
    uints lower_boundT( const K& key ) const
    {
        // k<m -> top = m
        // k>m -> bot = m+1
        // k==m -> top = m
        uints i, j, m;
        i = 0;
        j = _count();
        for( ; j>i; )
        {
            m = (i+j)>>1;
            if( _ptr[m] < key )
                i = m+1;
            else
                j = m;
        }
        return i;
    }

    ///Binary search sorted array using function object f(T,K) for comparing T<K
    template<class K, class FUNC>
    uints lower_boundT( const K& key, const FUNC& fn ) const
    {
        // k<m -> top = m
        // k>m -> bot = m+1
        // k==m -> top = m
        uints i, j, m;
        i = 0;
        j = _count();
        for( ; j>i; )
        {
            m = (i+j)>>1;
            if( fn(_ptr[m], key) )
                i = m+1;
            else
                j = m;
        }
        return i;
    }

    ///Return ptr to last member or null if empty
    T* last() const
    {
        uints s = _count();
        if (s == 0)
            return 0;

        return _ptr+s-1;
    }

    ///Insert empty elements into array
    /** Inserts \a nitems of empty elements at given position \a pos
        @param pos position where to insert, if larger than actual count, appends to the end
        @param nitems number of elements to insert
        @return pointer to the first inserted element */
    T* ins( uints pos, uints nitems=1 )
    {
        DASSERT( pos != UMAXS );
        if( pos > _count() )
        {
            uints ov = pos - _count();
            return add(ov + nitems) + ov;
        }
        addnc(nitems);
        T* p = __ins( _ptr, pos, _count()-nitems, nitems );
        
        if( !type_trait<T>::trivial_constr )
            for(; nitems>0; --nitems, ++p)  ::new (p) T;
        return _ptr + pos;
    }

    T* insc( uints pos, uints nitems=1 )
    {
        DASSERT( pos != UMAXS );
    	if( pos > _count() ) {
            uints ov = pos - _count();
            return addc(ov + nitems) + ov;
        }
        addnc(nitems);
        T* p = __ins( _ptr, pos, _count()-nitems, nitems );
        ::memset( p, 0, nitems*sizeof(T) );
        return p;
    }

    void ins_value( uints pos, const T& v )
    {
        DASSERT( pos != UMAXS );
        T* p;
        if( pos > _count() )
        {
            uints ov = pos - _count();
            p = add(ov+1) + ov;
        }
        else
        {
            addnc(1);
            p = __ins( _ptr, pos, _count()-1, 1 );
        }
        
        *p = v;
    }

    ///Delete elements from given position
    /** @param pos position from what to delete
        @param nitems number of items to delete */
    void del( uints pos, uints nitems=1 ) {
        if (pos+nitems > _count())  return;
        uints i = nitems;
        if( !type_trait<T>::trivial_constr )
            for( T* p = _ptr+pos; i>0; --i, ++p )  p->~T();
        __del (_ptr, pos, _count(), nitems);
        _set_count( _count() - nitems );
    }

    ///Test whether array contains given element \a key and delete it
    /// @return number of deleted items
    uints del_key( const T& key, uints n=1 )
    {
        uints c = _count();
        uints m = 0;
        for( uints i=0; i<c; ++i )
            if (key == _ptr[i])  { ++m;  del(i);  if(!--n) return m;  --i; }
        return m;
    }

    ///Test whether array contains given element \a key and delete it
    /// @return number of deleted items
    uints del_key_back( const T& key, uints n=1 )
    {
        uints c = _count();
        uints m = 0;
        for( ; c>0; )
        {
            --c;
            if( key == _ptr[c] )  { ++m;  del(c);  if(!--n) return m; }
        }
        return m;
    }

    ///Delete element in sorted array
    /// Uses either operator T<K or a functor(T,K) for binary search, and operator T==K for equality comparison
    ///@return number of deleted items
    ///@param key key to localize the first item to delete
    ///@param n maximum number of items to delete
    //@{
    uints del_sort( const T& key, uints n=1 )                   { return del_sortT<T>(key,n); }

    template<class FUNC>
    uints del_sort( const T& key, const FUNC& fn, uints n=1 )   { return del_sortT<T,FUNC>(key,fn,n); }

    template<class K>
    uints del_sortT( const K& key, uints n=1 )
    {
        uints c = lower_boundT<K>(key);
        uints i, m = _count();
        for( i=c; i<m && n>0; ++i,--n ) {
            if( !(_ptr[i] == key) )
                break;
        }
        if( i>c )
            del( c, i-c );
        return i-c;
    }

    template<class K, class FUNC>
    uints del_sortT( const K& key, const FUNC& fn, uints n=1 )
    {
        uints c = lower_boundT<K,FUNC>(key,fn);
        uints i, m = _count();
        for( i=c; i<m && n>0; ++i,--n ) {
            if( !(_ptr[i] == key) )
                break;
        }
        if( i>c )
            del( c, i-c );
        return i-c;
    }
    //@}

    T* move_temp (uints from, uints to, uints num, void* buf)
    {
        if (from+num > _count())  return 0;
        if (to >= _count())  return 0;

        //assume num <= rem (optimize for this case)
        if (from < to)
        {
            if (to < from+num)  return 0;
            xmemcpy (buf, _ptr+from, num*sizeof(T));

            __move (-(ints)num, _ptr+from+num, to-from-num);
            return _ptr+to-num;
        }
        else if (to < from)
        {
            xmemcpy (buf, _ptr+from, num*sizeof(T));

            __move (num, _ptr+to, from-to);
            return _ptr+to;
        }
        return 0;
    }

    T* move (uints from, uints to, uints num)
    {
        if (from+num > _count())  return 0;
        if (to >= _count())  return 0;

        if (from < to  &&  to < from+num)  return 0;

        if (num < 256)
        {
            T buf[256];
            T* p = move_temp( from, to, num, buf );
            xmemcpy (p, buf, num*sizeof(T));
            return p;
        }
        else
        {
            dynarray<uchar> buf;
            buf.alloc( num*sizeof(T) );
            T* p = move_temp( from, to, num, buf.ptr() );
            xmemcpy( p, buf.ptr(), num*sizeof(T) );
            return p;
        }

        return 0;
    }

    ///Delete content but keep the memory reserved
    ///@return previous size of the array
    uints reset()
    {
        uints n=_count();
        if(_ptr) {
            _destroy();
            _set_count(0);
        }
        return n;
    }

    void clear()                        { reset(); }

    ///Delete content and _destroy the memory
    void discard() {
        if(_ptr) {
            _destroy();
            A::free(_ptr);
            _ptr=0;
        }
    }

    ///Get number of elements in the array
    uints size() const                  { return _count(); }

    ///Hard set number of elements
    //@warn Doesn't execute either destructors for the removed elements or constructors for added ones.
    uints set_size( uints n )
    {
        DASSERT( n*sizeof(T) <= _size() );
        if(_ptr) _set_count(n);
        return n;
    }

    uints byte_size() const             { return _count() * sizeof(T); }

    ///Return number of remaining reserved bytes
    uints reserved_remaining() const    { return A::size(_ptr) - sizeof(T)*A::count(_ptr); }
    uints reserved_total() const        { return A::size(_ptr); }


    typedef _dynarray_eptr<T>           iterator;
    typedef _dynarray_const_eptr<T>     const_iterator;

    iterator begin()                    { return _dynarray_eptr<T>(_ptr); }
    iterator end()                      { return _dynarray_eptr<T>(_ptr+_count()); }

    const_iterator begin() const        { return _dynarray_const_eptr<T>(_ptr); }
    const_iterator end() const          { return _dynarray_const_eptr<T>(_ptr+_count()); }


    binstream_container get_container_to_write( uints n=UMAXS )
    {
        return binstream_container( *this, n );
    }

    binstream_container get_container_to_read()
    {
        return binstream_container( *this, size() );
    }

private:
    void _destroy() {
#if defined(_DEBUG) && defined(DYNARRAY_CHECK_PTR)
        if (_ptr  &&  !A::is_valid_ptr(_ptr))
            throw ersEXCEPTION "invalid pointer";
#endif
        uints c = _count();
        if( !type_trait<T>::trivial_constr )
            for( uints i=0; i<c; ++i )  _ptr[i].~T();
    }

    uints _realloc( uints newsize, uints oldsize )
    {
        uints nalloc = newsize;
        if( nalloc < 2 * oldsize )
            nalloc = 2 * oldsize;

        T* op = _ptr;
        _ptr = A::realloc(_ptr, nalloc);
        _set_count(newsize);

        if( !type_trait<T>::trivial_moving_constr  &&  op != _ptr )
        {
            for( uints i=0; i<oldsize; ++i )
                type_trait<T>::moving::move( _ptr[i], op+i );
        }

        return nalloc;
    }

protected:
    ///Add \a nitems of elements on the end but do not initialize them with default constructor
    /** @param nitems count of items to add
        @param toones fill memory with ones (true) or zeros (false) before calling default constructor of element
        @return pointer to the first added element */
    T* addnc( uints nitems )
    {
        uints n = _count();

        if(!nitems)  return _ptr + n;
        uints nto = nitems + n;
        uints nalloc = nto;

        if( nalloc*sizeof(T) > _size() )
            nalloc = _realloc(nalloc, n);

        _set_count(nto);
        return _ptr + n;
    };
};


////////////////////////////////////////////////////////////////////////////////
///take control over buffer controlled by \a dest dynarray, \a dest ptr will be set to zero
/// destination dynarray must have sizeof type == 1
template< class SRC, class DST >
dynarray<DST>& dynarray_takeover( dynarray<SRC>& src, dynarray<DST>& dst )
{
    DASSERT( sizeof(DST) == 1 );
    
    dst.discard();
    dst._ptr = (DST*)src._ptr;
    dst.set_size( src.size() * sizeof(SRC) );
    src._ptr = 0;
    return dst;
}

////////////////////////////////////////////////////////////////////////////////
template< class SRC, class DST >
dynarray<DST>& dynarray_swap( dynarray<SRC>& src, dynarray<DST>& dst )
{
    DASSERT( sizeof(DST) == 1 );
    
    dst.reset();
    SRC* p = (SRC*) dst._ptr;

    dst._ptr = (DST*)src._ptr;
    dst.set_size( src.size() * sizeof(SRC) );
    
    src._ptr = p;
    return dst;
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
inline binstream& operator << ( binstream &out, const dynarray<T>& dyna )
{
    binstream_container_fixed_array<T> c(dyna.ptr(), dyna.size());
    return out.xwrite_array(c);
}

template <class T>
inline binstream& operator >> ( binstream &in, dynarray<T>& dyna )
{
    dyna.reset();
    typename dynarray<T>::binstream_container c(dyna);

    return in.xread_array(c);
}

template <class T>
inline binstream& operator << ( binstream &out, const dynarray<T*>& dyna )
{
    binstream_container_fixed_array<T*> c( dyna.ptr(), dyna.size(), 0, 0 );
    binstream_dereferencing_containerT<T> dc(c);
    return out.xwrite_array(dc);
}

template <class T>
inline binstream& operator >> ( binstream &in, dynarray<T*>& dyna )
{
    dyna.reset();
    typename dynarray<T*>::binstream_container c( dyna, UMAXS, 0, 0 );
    binstream_dereferencing_containerT<T> dc(c);

    return in.xread_array(dc);
}


////////////////////////////////////////////////////////////////////////////////
template<class T, class A>
struct binstream_adapter_writable< dynarray<T,A> > {
    typedef dynarray<T,A>   TContainer;
    typedef typename dynarray<T,A>::binstream_container TBinstreamContainer;
};

template<class T, class A>
struct binstream_adapter_readable< dynarray<T,A> > {
    typedef dynarray<T,A>   TContainer;
    typedef typename dynarray<T,A>::binstream_container TBinstreamContainer;
};

COID_NAMESPACE_END


#endif // __COID_COMM_DYNARRAY__HEADER_FILE__
