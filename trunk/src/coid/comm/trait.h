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
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#ifndef __COID_COMM_TRAIT__HEADER_FILE__
#define __COID_COMM_TRAIT__HEADER_FILE__

#include "namespace.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
#if defined(_MSC_VER) && _MSC_VER < 1300

template< int COND, class ttA, class ttB >
struct type_select
{
    template< int CONDX >
    struct selector {typedef ttA type;};

    template<>
    struct selector< false >
    {typedef ttB type;};

    typedef typename selector< COND >::type  type;
};

#else

template< int, typename ttA, typename >
struct type_select
{
    typedef ttA type;
};

template< typename ttA, typename ttB >
struct type_select<false,ttA,ttB>
{
    typedef ttB type;
};

#endif

////////////////////////////////////////////////////////////////////////////////
template<class T>
struct type_dereference {typedef void type;};

template<class K>
struct type_dereference<K*> {typedef K type;};

////////////////////////////////////////////////////////////////////////////////
template <class T>
struct type_moving_constructor_trivial
{
    static void move( T& dst, T* oldp )     { }
};

template <class T>
struct type_moving_constructor
{
    static void move( T& dst, T* oldp )     { dst = *oldp; }
};


////////////////////////////////////////////////////////////////////////////////
template <class T>
struct type_trait
{
    enum {
        trivial_constr = false,
        trivial_moving_constr = true,
    };

    typedef typename type_select< trivial_moving_constr,
        type_moving_constructor_trivial<T>,
        type_moving_constructor<T> >::type  moving;
};

#if defined(_MSC_VER)

#define TYPE_TRIVIAL(t) \
template<> struct type_trait<t> { \
    enum { trivial_constr = true, trivial_moving_constr = true, }; \
    typedef type_select< trivial_moving_constr, \
        type_moving_constructor_trivial<t>, \
        type_moving_constructor<t> >::type  moving; \
}

#else

#define TYPE_TRIVIAL(t) namespace coid { \
template<> struct type_trait<t> { \
    enum { trivial_constr = true, trivial_moving_constr = true, }; \
    typedef type_select< trivial_moving_constr, \
        type_moving_constructor_trivial<t>, \
        type_moving_constructor<t> >::type  moving; \
}; }

#endif

COID_NAMESPACE_END

#endif //__COID_COMM_TRAIT__HEADER_FILE__
