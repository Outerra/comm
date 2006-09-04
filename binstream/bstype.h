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
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#ifndef __COID_COMM_BSTYPE__HEADER_FILE__
#define __COID_COMM_BSTYPE__HEADER_FILE__

#include "../namespace.h"

#include "../comm.h"
#include "../commtime.h"
#include "../token.h"



COID_NAMESPACE_BEGIN

class binstream;

namespace bstype {

////////////////////////////////////////////////////////////////////////////////
struct key
{
    char k;
};


struct STRUCT_OPEN          { };
struct STRUCT_CLOSE         { };

struct ARRAY_OPEN           { uints size; };
struct ARRAY_CLOSE          { };
struct ARRAY_ELEMENT        { };

struct BINSTREAM_FLUSH      { };
struct BINSTREAM_ACK        { };
struct BINSTREAM_ACK_EAT    { };
/*
struct SEPARATOR
{
    ucs4 _s;
    SEPARATOR(char s) : _s(s) {}
};
*/
////////////////////////////////////////////////////////////////////////////////
///Object type descriptor
struct type
{
    ushort _size;       ///<byte size of the element
    uchar  _type;       ///<type enum
    uchar  _ctrl;       ///<control flags

    ///Type enum
    enum {
        T_BINARY=0,
        T_INT,
        T_UINT,
        T_FLOAT,
        T_BOOL,
        T_CHAR,                                     ///< character data - strings
        T_ERRCODE,
        T_TIME,

        T_KEY,                                      ///< unformatted characters
        T_STRUCTBGN, T_STRUCTEND,
        T_SEPARATOR,

        COUNT,

        T_COMPOUND                  = 0xff,
    };

    ///Control flags
    enum {
        xELEMENT                    = 0x0f,
        fARRAY_ELEMENT              = 0x01,         ///< array element flag
        fARRAY_ELEMENT_NEXT         = 0x02,         ///< always in addition to fARRAY_ELEMENT, all after first
        fARRAY_UNSPECIFIED_SIZE     = 0x04,         ///< size of the array is not specified in advance

        //xCONTROL_TYPE               = 0xf0,
        fARRAY_BEGIN                = 0x10,         ///< array start mark
        fARRAY_END                  = 0x20,         ///< array end mark
        //ARRAY_BEGIN                 = 0x10,         ///< array start mark
        //ARRAY_END                   = 0x20,         ///< array end mark
    };

    type() : _size(0), _type(T_COMPOUND), _ctrl(0) {}
    type( uchar btype, ushort size, uchar ctrl=0 ) : _size(size), _type(btype), _ctrl(ctrl) {}
    explicit type( uchar btype ) : _size(0), _type(btype), _ctrl(0) {}


    bool operator == ( type t ) const       { return *(uint32*)this == *(uint32*)&t; }

    bool is_no_size() const                 { return _size == 0; }
    bool is_primitive() const               { return _type < T_COMPOUND; }

    ///@return true if the type is array control token
    bool is_array_control_type() const      { return (_ctrl & (fARRAY_BEGIN|fARRAY_END)) != 0; }

    //@{ Create array control types from current type
    type get_array_begin() const            { type t=*this; t._ctrl=fARRAY_BEGIN; t._size=sizeof(uints); return t; }
    type get_array_end() const              { type t=*this; t._ctrl=fARRAY_END; t._size=0; return t; }
    
    type get_array_element() const          { type t=*this; t._ctrl=fARRAY_ELEMENT; t._size=_size; return t; }

    static void mask_array_element_first_flag( type& t )     { t._ctrl |= fARRAY_ELEMENT_NEXT; }

    void set_array_unspecified_size()       { _ctrl |= fARRAY_UNSPECIFIED_SIZE; }
    bool is_array_unspecified_size() const  { return (_ctrl & fARRAY_UNSPECIFIED_SIZE) != 0; }
    //@}

    bool is_array_element() const           { return (_ctrl & fARRAY_ELEMENT) != 0; }
    bool is_next_array_element() const      { return (_ctrl & fARRAY_ELEMENT_NEXT) != 0; }
    
    bool should_place_separator() const     { return !(_ctrl & fARRAY_END)  &&  ((_ctrl & fARRAY_ELEMENT_NEXT) || !(_ctrl & fARRAY_ELEMENT)); }


    ///Get byte size of primitive element
    ushort get_size() const                 { return _size; }
};


////////////////////////////////////////////////////////////////////////////////
template<class T>
struct t_type : type
{
    typedef T   value;
};

#define DEF_TYPE(t,basic_type) \
template<> struct t_type<t> : type { t_type() : type(type::basic_type,sizeof(t)) {} }

#define DEF_TYPE2(t,basic_type,size) \
template<> struct t_type<t> : type { t_type() : type(type::basic_type,size) {} }

DEF_TYPE2(  void,               T_BINARY, 1);
DEF_TYPE(   bool,               T_BOOL);
DEF_TYPE(   uint8,              T_UINT);
DEF_TYPE(   int8,               T_INT);
DEF_TYPE(   uint16,             T_UINT);
DEF_TYPE(   int16,              T_INT);
DEF_TYPE(   uint32,             T_UINT);
DEF_TYPE(   int32,              T_INT);
DEF_TYPE(   uint64,             T_UINT);
DEF_TYPE(   int64,              T_INT);

DEF_TYPE(   char,               T_CHAR);

#ifdef _MSC_VER                 
DEF_TYPE(   uint,               T_UINT);
DEF_TYPE(   int,                T_INT);
#else                           
DEF_TYPE(   ulong,              T_UINT);
DEF_TYPE(   long,               T_INT);
#endif //_MSC_VER
                            
DEF_TYPE(   float,              T_FLOAT);
DEF_TYPE(   double,             T_FLOAT);
DEF_TYPE(   long double,        T_FLOAT);

DEF_TYPE(   key,                T_KEY);

DEF_TYPE(   timet,              T_TIME);

///opcd uses just 2 bytes to transfer the error code
DEF_TYPE2(  opcd,               T_ERRCODE, 2 );

DEF_TYPE2(  STRUCT_OPEN,        T_STRUCTBGN, 0);
DEF_TYPE2(  STRUCT_CLOSE,       T_STRUCTEND, 0);

//DEF_TYPE(   SEPARATOR,          T_SEPARATOR);


////////////////////////////////////////////////////////////////////////////////
///Wrapper class for binstream type key
template<class T>
struct t_key : public T
{
    static t_key& from( T& k )              { return (key&)k; }
    static const t_key& from( const T& k )  { return (const key&)k; }


    friend binstream& operator >> (binstream& in, t_key<T>& )           { return in; }
    friend binstream& operator << (binstream& out, const t_key<T>& )    { return out; }
};

} //namespace bstype


TYPE_TRIVIAL(bstype::key);


COID_NAMESPACE_END

#endif //__COID_COMM_BSTYPE__HEADER_FILE__
