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

#ifndef __COID_COMM_HASHFUNC__HEADER_FILE__
#define __COID_COMM_HASHFUNC__HEADER_FILE__


#include "../namespace.h"
#include "../dynarray.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
template<class KEY> struct hash
{
    typedef KEY     type_key;
};

inline uints __coid_hash_string( const char* s )
{
  uints h = 0;
  for( ; *s; ++s)
    h = (h ^ *s) + (h<<26) + (h>>6);
//    h = 3141592653UL*h + *s;

  return uints(h);
}


inline uints __coid_hash_string( const char* s, uints n )
{
  uints h = 0;
  for( ; n>0; ++s,--n )
    h = (h ^ *s) + (h<<26) + (h>>6);
//    h = 3141592653UL*h + *s;

  return uints(h);
}


////////////////////////////////////////////////////////////////////////////////
template<> struct hash<char*>
{
    typedef char* type_key;
    uints operator()(const char* s) const { return __coid_hash_string(s); }
};

template<> struct hash<const char*>
{
    typedef const char* type_key;
    uints operator()(const char* s) const { return __coid_hash_string(s); }
};

template<> struct hash<char> {
    typedef char type_key;
    uints operator()(char x) const { return x; }
};
template<> struct hash<unsigned char> {
    typedef unsigned char type_key;
    uints operator()(unsigned char x) const { return x; }
};
template<> struct hash<signed char> {
    typedef signed char type_key;
    uints operator()(unsigned char x) const { return x; }
};

template<> struct hash<short> {
    typedef short type_key;
    uints operator()(short x) const { return x; }
};
template<> struct hash<unsigned short> {
    typedef unsigned short type_key;
    uints operator()(unsigned short x) const { return x; }
};
template<> struct hash<int> {
    typedef int type_key;
    uints operator()(int x) const { return x; }
};
template<> struct hash<unsigned int> {
    typedef unsigned int type_key;
    uints operator()(unsigned int x) const { return x; }
};
template<> struct hash<long> {
    typedef long type_key;
    uints operator()(long x) const { return x; }
};
template<> struct hash<unsigned long> {
    typedef unsigned long type_key;
    uints operator()(unsigned long x) const { return x; }
};

template<> struct hash<int64> {
    typedef int64 type_key;
    uints operator()(int64 x) const { return (uints)x; }
};
template<> struct hash<uint64> {
    typedef uint64 type_key;
    uints operator()(uint64 x) const { return (uints)x; }
};

COID_NAMESPACE_END

#endif //__COID_COMM_HASHFUNC__HEADER_FILE__
