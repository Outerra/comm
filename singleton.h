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

#include "sync/mutex.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
#define SINGLETON(T) \
    singleton< T >::instance()

#define SINGLETON_ALIVE(T) \
    singleton< T >::instance_alive()

#define FORCE_CREATE_SINGLETON(T) \
    static T& __singleton##T = SINGLETON(T)


#define MXSINGLETON(T) \
    mxsingleton< T >::instance()

#define MXSINGLETON_ALIVE(T) \
    mxsingleton< T >::instance_alive()

#define MXSINGLETON_T(T)        mxsingleton< T >::Instance

#define FORCE_CREATE_MXSINGLETON(T) \
    static mxsingleton< T >::Instance& __mxsingleton##T = MXSINGLETON(T)


////////////////////////////////////////////////////////////////////////////////
///Global singleton registrator
struct GlobalSingleton
{
    static _comm_mutex& instance()
    {
        static _comm_mutex mx;
        return mx;
    }
};


////////////////////////////////////////////////////////////////////////////////
template <class T>
class singleton
{
    static void destroy()
    {
        int& st = status_ref();
        st = -1;
        instance();
    }

    singleton()    { }

public:

    static bool being_destroyed()
    {
        return status_ref() < 0;
    }

    static T& instance_alive()
    {
        if( status_ref() >= 0 )
            return instance();
        throw ersDENIED;
    }

    static T& instance()
    {
        static int& status = status_ref();
        static T* node = 0;
        
        if(status>0)
            return *node;

        comm_mutex_guard<_comm_mutex> mxg( GlobalSingleton::instance() );

        if(status>0) {
            mxg.unlock();
            return *node;
        }

        if( status < 0 )
        {
            if(node)
            {
                if( status-- < -1 )     //instance() called from node destructor again
                    return *node;

                delete node;
                node = 0;
                return *(T*)0;
            }
            else
                throw ersALREADY_DELETED;
        }
        else
        {
            node = new T;
            atexit( &destroy );

            status = 1;
        }

        mxg.unlock();
        return *node;
    }

private:
    static int& status_ref()
    {
        static int status = 0;
        return status;
    }
};


////////////////////////////////////////////////////////////////////////////////
template<class T, class MUTEX=comm_mutex>
class mxsingleton
{
    static void destroy()
    {
        int& st = status_ref();
        st = -1;
        instance();
    }

    mxsingleton() { }

public:

    class Instance
    {
        friend class mxsingleton;

        MUTEX& _mx;
        T* _obj;
    public:

        Instance( MUTEX& mx, T* obj, bool lock=true ) : _mx(mx), _obj(obj)
        {
            if(lock)
                _mx.lock();
        }
        ~Instance()                         { _mx.unlock(); }

        T* operator -> ()                   { return _obj; }
        const T* operator -> () const       { return _obj; }
    };


    static bool being_destroyed()
    {
        return status_ref() < 0;
    }

    static T& instance_alive()
    {
        if( status_ref() >= 0 )
            return instance();
        throw ersDENIED;
    }

    static Instance instance()
    {
        static int& status = status_ref();
        static T* node = 0;
        static MUTEX* mutex = 0;
        
        if(status>0)
            return Instance(*mutex,node);

        comm_mutex_guard<_comm_mutex> mxg( GlobalSingleton::instance() );

        if(status>0) {
            mxg.unlock();
            return Instance(*mutex,node);
        }

        if( status < 0 )
        {
            if(node)
            {
                mutex->lock();
                if( status-- < -1 )
                    return Instance(*mutex,node,false);   //instance() called from node destructor again

                delete node;
                node = 0;
                return Instance(*mutex,0,false);
            }
            else
                throw ersALREADY_DELETED;
        }
        else
        {
            node = new T;
            mutex = new MUTEX;
            atexit( &destroy );

            status = 1;
        }

        mxg.unlock();
        return Instance(*mutex,node);
    }

private:

    static int& status_ref()
    {
        static int status = 0;
        return status;
    }
};





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
