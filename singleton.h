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

#ifndef __COID_COMM_SINGLETON__HEADER_FILE__
#define __COID_COMM_SINGLETON__HEADER_FILE__

#include "namespace.h"
#include "assert.h"
#include <stdlib.h>

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
#define SINGLETON(T) \
    singleton< T >::instance()

#ifdef _DEBUG
#define DBGEXPR(exp)    exp;
#define DBGSTAT(exp)    exp
#else
#define DBGEXPR(exp)
#define DBGSTAT(exp)
#endif

////////////////////////////////////////////////////////////////////////////////
template <class T>
class singleton
{

public:
    struct creator
    {
        DBGEXPR( int init );

    public:
        creator()
        {
            DBGEXPR( init = 0 );
        }

        ~creator()
        {
            DASSERT( init == 0 );
        }

        static creator* instance()
        {
            if( _singleton )  return _singleton;
            _singleton = new creator;
            singleton<T>::instance();
            return _singleton;
        }
        
        void do_nothing()
        {
            DBGEXPR( ++init );
        }

        void destroy()
        {
            DASSERTX( init == 1, "Singleton deleted twice" );
            T* p = &singleton<T>::instance();
            delete p;
            DBGEXPR( --init );
        }
    };
    
private:

    friend struct creator;
    static creator* _singleton;

    static void _destroy()
    {
        _singleton->destroy();
    }

    singleton()    { }

public:

    static T& instance()
    {
        static T* node = 0;
        
        if(node)
            return *node;

        node = new T;

        creator::instance()->do_nothing();
        atexit( _destroy );

        return *node;
    }
};

template <class T>
typename singleton<T>::creator*  singleton<T>::_singleton = singleton<T>::creator::instance();

//M$VC doesn't initialize static objects when used by another static objects earlier
// in the pre-main initialization chain, so the above code works around it, but
// it works only when global pointers are preinitialized to zero (which M$VC does)
//Following lessmess code works in gcc.
/*
////////////////////////////////////////////////////////////////////////////////
template <class T>
class singleton
{

public:
    struct creator
    {
        DBGEXPR( int init );

    public:
        creator()
        {
            DBGEXPR( init = 0 );
            singleton<T>::instance();
        }

        ~creator()
        {
            DASSERT( init == 0 );
        }

        void do_nothing()
        {
            DBGEXPR( ++init );
        }

        void destroy()
        {
            DASSERTX( init == 1, "Singleton deleted twice" );
            T* p = &singleton<T>::instance();
            delete p;
            DBGEXPR( --init );
        }
    };
    
private:

    friend struct creator;
    static creator _singleton;

    static void _destroy()
    {
        _singleton.destroy();
    }

    singleton()    { }

public:

    static T& instance()
    {
        static T* node = 0;
        
        if(node)
            return *node;

        node = new T;

        _singleton.do_nothing();
        atexit( _destroy );

        return *node;
    }
};

template <class T>
typename singleton<T>::creator  singleton<T>::_singleton;

*/

COID_NAMESPACE_END

#endif //__COID_COMM_SINGLETON__HEADER_FILE__
