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

#ifndef __COMM_STLSTREAM_HEADER_FILE__
#define __COMM_STLSTREAM_HEADER_FILE__

#include "../namespace.h"

#include <string>
#include <vector>
#include <list>
#include <deque>
#include <set>
#include <map>

#include <iterator>
#include "binstream.h"
#include "../str.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
///Container for reading from a stl container
template<class StlContainer>
struct binstream_container_stl_input_iterator : binstream_containerT<typename StlContainer::value_type>
{
    typedef typename StlContainer::value_type       T;

    const void* extract( uints n )
    {
        const T* p = &(*inpi++);
        return p;
    }

    void* insert( uints n )
    {
        DASSERT(0); //not defined for input
    }

    bool is_continuous() const      { return false; }


    binstream_container_stl_input_iterator( const StlContainer& cont, uints n )
        : binstream_containerT<T>(n), inpi(cont.begin()) {}

    void set( const StlContainer& cont, uints n )
    {
        inpi = cont.begin();
        this->_nelements = n;
    }

protected:
    typename StlContainer::const_iterator inpi;
};

///Container for writing to a stl container
template<class StlContainer>
struct binstream_container_stl_output_iterator : binstream_containerT<typename StlContainer::value_type>
{
    typedef typename StlContainer::value_type       T;

    const void* extract( uints n )
    {
        DASSERT(0); //not defined for output
    }

    void* insert( uints n )
    {
        T* p = &(*outpi);
        return p;
    }

    bool is_continuous() const      { return false; }


    binstream_container_stl_output_iterator( StlContainer& cont, uints n )
        : binstream_containerT<T>(n)
    {
        cont.clear();
        outpi = cont.end();
        this->_nelements = n;
    }

    void set( StlContainer& cont, uints n )
    {
        cont.clear();
        outpi = cont.end();
        this->_nelements = n;
    }

protected:
    typename StlContainer::iterator outpi;
};

////////////////////////////////////////////////////////////////////////////////
#define STD_BINSTREAM(CONT) \
template<class T, class A> \
inline binstream& operator << (binstream& out, const CONT<T,A>& v) \
{   binstream_container_stl_input_iterator< CONT<T,A> > c( v, UMAX ); \
    out.xwrite_array(c); \
	return out; \
} \
template<class T, class A> \
inline binstream& operator >> (binstream& out, CONT<T,A>& v) \
{   v.clear(); \
    binstream_container_stl_output_iterator<T> c( v, UMAX ); \
    out.xwrite_array(c); \
	return out; \
}

STD_BINSTREAM(std::list)
STD_BINSTREAM(std::deque)

STD_BINSTREAM(std::set)
STD_BINSTREAM(std::multiset)
STD_BINSTREAM(std::map)
STD_BINSTREAM(std::multimap)


////////////////////////////////////////////////////////////////////////////////
///Binstream container for vector, this one can be optimized more
template<class T, class A>
struct std_vector_binstream_container : public binstream_containerT<T>
{
    virtual const void* extract( uints n )
    {
        DASSERT( _pos+n <= _v.size() );
        const T* p = &_v[_pos];
        _pos += n;
        return p;
    }

    virtual const void* insert( uints n )
    {
        _v.resize( _pos+n );
        T* p = &_v[_pos];
        _pos += n;
        return p;
    }

    virtual bool is_continuous() const      { return true; }

    std_vector_binstream_container( std::vector<T,A>& v )
        : binstream_containerT<T>(v.size()), _v(v)
    {
        _pos = 0;
    }

protected:
    uints _pos;
    std::vector<T,A>& _v;
};


template<class T, class A>
inline binstream& operator << (binstream& out, const std::vector<T,A>& v)
{
    binstream_container_fixed_array<T> c( &v[0], v.size() );
    out.xwrite_array(c);
	return out;
}

template<class T, class A>
inline binstream& operator >> (binstream& in, std::vector<T,A>& v)
{
    std_vector_binstream_container<T,A> c(v);
    in.xread_array(c);
    return in;
}



////////////////////////////////////////////////////////////////////////////////
inline binstream& operator << (binstream& out, const std::string& p)
{
    binstream_container_fixed_array<char> c((char*)p.c_str(), p.size());
    out.xwrite_array(c);
	return out;
}

inline binstream& operator >> (binstream& in, std::string& p)
{
	charstr t;
	in >> t;
	p = t.ptr();
	return in;
}

////////////////////////////////////////////////////////////////////////////////
template <class F, class S>
inline binstream& operator << (binstream& out, const std::pair<F,S>& p)
{
	return out << p.first << p.second;
}

////////////////////////////////////////////////////////////////////////////////
template <class F, class S>
inline binstream& operator >> (binstream& in, std::pair<F,S>& p)
{
	return in >> p.first >> p.second;
}

COID_NAMESPACE_END

#endif

