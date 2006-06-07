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

#ifndef __COID_COMM_LOCAL__HEADER_FILE__
#define __COID_COMM_LOCAL__HEADER_FILE__

#include "namespace.h"

#include "commtypes.h"
#include "binstream/binstream.h"
//#include "alloc.h"


COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
//Forward declarations
#ifndef SYSTYPE_MSVC6
template<class T> class str_ptr;
template<class T> binstream& operator << (binstream&, const str_ptr<T>& );
#endif //SYSTYPE_MSVC

///Template class for streaming of objects pointed to by pointers
template <class T>
class str_ptr
{
    T* _p;

public:
    str_ptr()  {}
    str_ptr(const T* p) {_p= p;}

    operator T*(void) const     { return _p; }
    operator T**(void) const    { return &_p; }

    int operator==(const T* ptr) const  { return ptr==_p; }
    T& operator*(void) const    { return *_p; }
#ifdef SYSTYPE_MSVC
#pragma warning (disable : 4284)
#endif //SYSTYPE_MSVC
    T* operator->(void) const   { return _p; }
#ifdef SYSTYPE_MSVC
#pragma warning (default : 4284)
#endif //SYSTYPE_MSVC
    T** operator&(void) const   { return (T**)&_p; }

    str_ptr& operator=(const str_ptr<T> &p) {
        _p= p._p;
        return *this;
    }
    str_ptr& operator=(const T* p) {
        _p= (T*)p;
        return *this;
    }

#ifdef SYSTYPE_MSVC8plus
    template< typename T >
    friend binstream& operator << (binstream &out, const str_ptr<T> &obj);
#else
    friend binstream& operator << TEMPLFRIEND(binstream &out, const str_ptr<T> &obj);
#endif

	bool is_set() const		    { return _p != 0; }
};

template <class T>
binstream& operator << (binstream &out, const str_ptr<T> &obj)
{
    out << *obj._p;
    return out;
}

////////////////////////////////////////////////////////////////////////////////
//Forward declarations
#ifndef SYSTYPE_MSVC6
template<class T> class local;
template<class T> binstream& operator << (binstream&, const local<T>& );
template<class T> binstream& operator >> (binstream&, local<T>& );
#endif //SYSTYPE_MSVC

///Template making pointers to any class behave like local objects, that is to be automatically destructed when the maintainer (class or method) is destructed/exited
template <class T>
class local
{
    T* _p;
public:
    local()  {_p=0;}
    ~local() {if(_p)  {delete _p;  _p=0;} }
    
    local(T* p) {_p= p;}
    
    local( const local& p )
    {
//        throw ersUNAVAILABLE "can't copy local<> (just move)";
        _p = new T(*p._p);
    }

    T* ptr() const                      { return _p; }

    operator T*(void) const             { return _p; }

    int operator==(const T* ptr) const  { return ptr==_p; }
    int operator!=(const T* ptr) const  { return ptr!=_p; }

    T& operator*(void)                  { return *_p; }
    const T& operator*(void) const      { return *_p; }

#ifdef SYSTYPE_MSVC
#pragma warning (disable : 4284)
#endif //SYSTYPE_MSVC
    T* operator->(void)                 { return _p; }
    const T* operator->(void) const     { return _p; }
#ifdef SYSTYPE_MSVC
#pragma warning (default : 4284)
#endif //SYSTYPE_MSVC

    local& operator=(const local& p)
    {
//        throw ersUNAVAILABLE "can't copy local<> (just move)";
        if(_p) {delete _p;  _p=0;}
        _p = new T(*p._p);
        return *this;
    }
    local& operator=(const T* p) {
        if(_p) delete _p;
        _p= (T*)p;
        return *this;
    }

    T*& get_ptr () const        { return (T*)_p; }

#ifdef SYSTYPE_MSVC8plus
    template< typename T >
    friend binstream& operator << (binstream &out, const local<T> &loca);
    template< typename T >
    friend binstream& operator >> (binstream &in, local<T> &loca);
#else
    friend binstream& operator << TEMPLFRIEND(binstream &out, const local<T> &loca);
    friend binstream& operator >> TEMPLFRIEND(binstream &in, local<T> &loca);
#endif

	bool is_set() const		{ return _p != 0; }

    T* eject() { T* tmp=_p; _p=0; return tmp; }
    void destroy ()
    {
        if (_p)
            delete _p;
        _p = 0;
    }
};

template <class T>
binstream& operator << (binstream &out, const local<T> &loca) {
    if (loca.is_set())
        out << (uint)1;
    else
        out << (uint)0x00;
    if(loca._p)  out << *loca._p;
    return out;
}

template <class T>
binstream& operator >> (binstream &in, local<T> &loca) {
    uint is;
    in >> is;
    if (is) {
        loca._p = new T;
        in >> *loca._p;
    }
    return in;
}

////////////////////////////////////////////////////////////////////////////////
///Template making pointers to any class behave like local objects, that is to be
/// automatically destructed when the maintainer (class or method) is destructed/exited
template <class T, bool USE_MALLOC=false, bool NO_DESTROY=false>
class localarray
{
    T* _p;
public:
    localarray()  {_p=0;}

    operator T*(void) const { return _p; }

    int operator==(const T* ptr) const { return ptr==_p; }
    int operator!=(const T* ptr) const { return ptr!=_p; }

    T& operator*(void)                  { return *_p; }
    const T& operator*(void) const      { return *_p; }

#ifdef SYSTYPE_MSVC
#pragma warning (disable : 4284)
#endif //SYSTYPE_MSVC
    T* operator->(void)                 { return _p; }
    const T* operator->(void) const     { return _p; }
#ifdef SYSTYPE_MSVC
#pragma warning (default : 4284)
#endif //SYSTYPE_MSVC

    struct alloc_new
    {
        static T* alloc( T** pp, uints n )   { return (*pp = new T[n]); }
        static void free( T** pp )          { if(*pp) delete[] *pp;  *pp = 0; }
    };

    struct alloc_malloc
    {
        static T* alloc( T** pp, uints n )   { return (*pp = (T*)::malloc( n * sizeof(T) )); }
        static void free( T** pp )          { if(*pp) ::free(*pp);  *pp = 0; }
    };

    struct alloc_null
    {
        static T* alloc( T** pp, uints n )   { return 0; }
        static void free( T** pp )          { *pp = 0; }
    };

    T* alloc( uints n )  { return type_select<USE_MALLOC,alloc_new,alloc_malloc>::type::alloc( &_p, n ); }
    void free()         { return type_select<USE_MALLOC,alloc_new,alloc_malloc>::type::free( &_p ); }

    ~localarray()
    {
        typedef typenamex type_select<USE_MALLOC,alloc_new,alloc_malloc>::type   Ta;
        type_select<NO_DESTROY,alloc_null,Ta>::type::free(&_p);
    }

    T*& get_ptr () const{ return (T*)_p; }

    bool is_set() const	{ return _p != 0; }

    T* eject()          { T* tmp=_p;  _p=0;  return tmp; }
};



COID_NAMESPACE_END

#endif // __COID_COMM_LOCAL__HEADER_FILE__
