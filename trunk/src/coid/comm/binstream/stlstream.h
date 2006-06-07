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
#include "binstream.h"
#include "../str.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
inline binstream& operator << (binstream& out, const std::string& p)
{
    binstream_container_fixed_array<char> c((char*)p.c_str(), p.size());
    out.xwrite_array(c);
	return out;
}

////////////////////////////////////////////////////////////////////////////////
inline binstream& operator >> (binstream& in, std::string& p)
{
	charstr t;
	in >> t;
	p = t.ptr();
	return in;
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
inline binstream& operator << (binstream& out, const std::vector<T>& v)
{
    binstream_container_fixed_array<T> c( &v[0], v.size() );
    out.xwrite_array(c);
	return out;
}
/*
////////////////////////////////////////////////////////////////////////////////
template<class Container>
struct stl_container : public binstream_container
{
    virtual const void* extract( uints n )
    {
        DASSERT( n == 1 );
        DASSERT( _begin != _end );
        value_type* v = &(*_begin);
        ++_begin;
        return v;
    }

    virtual void* insert( uints n )
    {
        DASSERT(0);
        return 0;
    }

    virtual bool is_continuous() const      { return false; }


    typedef Container::const_iterator       const_iterator;
    typedef Container::value_type           value_type;

    stl_container( Container& co )
        : binstream_container(UMAX,bstype::t_type<value_type>()), _co(co)
    {
        _begin = _co.begin();
        _end = _co.end();
    }

protected:
    Container& _co;
    const_iterator _begin, _end;
}

////////////////////////////////////////////////////////////////////////////////
template<class StlSequence>
struct stl_sequence : public binstream_container
{
    virtual const void* extract( uints n )
    {
        DASSERT( n == 1 );
        DASSERT( _begin != _end );
        value_type* v = &(*_begin);
        ++_begin;
        return v;
    }

    virtual void* insert( uints n )
    {
        DASSERT( n == 1 );
        _co.insert( _begin, value_type() );
        value_type* v = &(*_begin);
        ++_begin;
        return v;
    }

    virtual bool is_continuous() const      { return false; }


    typedef Container::const_iterator       const_iterator;
    typedef Container::value_type           value_type;

    stl_sequence( Container& co )
        : binstream_container(UMAX,bstype::t_type<value_type>()), _co(co)
    {
        _begin = _co.begin();
        _end = _co.end();
    }

protected:
    Container& _co;
    const_iterator _begin, _end;
}
*/
////////////////////////////////////////////////////////////////////////////////
template<class T>
struct std_vector_binstream_container : public binstream_container
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

    std_vector_binstream_container( std::vector<T>& v )
        : binstream_container(v.size(),bstype::t_type<T>()), _v(v)
    {
        _pos = 0;
    }

protected:
    uints _pos;
    std::vector<T>& _v;
};

template<class T>
inline binstream& operator >> (binstream& in, std::vector<T>& v)
{
    std_vector_binstream_container<T> c(v);
    in.xread_array(c);
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

