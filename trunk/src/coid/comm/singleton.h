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
#include "commtypes.h"


#ifdef _DEBUG

# define SINGLETON(T) \
    coid::singleton<T>::instance(#T,__FILE__,__LINE__)

#else

# define SINGLETON(T) \
    coid::singleton<T>::instance()

#endif

///Used for function-local static objects.
/// LOCAL_SINGLETON(class) name = initialization
#define LOCAL_SINGLETON(T) \
    static coid::local_singleton<T>


////////////////////////////////////////////////////////////////////////////////
COID_NAMESPACE_BEGIN

void* singleton_register_instance(
    void* p,
    void (*fn_destroy)(void*),
    const char* type,
    const char* file,
    int line);

void singletons_destroy();

////////////////////////////////////////////////////////////////////////////////
template <class T>
class singleton
{
	singleton()    { }

public:

	static T& instance()
	{
		static T* node = 0;

		if(node)
			return *node;

        node = (T*)singleton_register_instance(new T, &destroy, 0, 0, 0);
        return *node;
	}

	static T& instance(const char* type, const char* file, int line)
	{
		static T* node = 0;

		if(node)
			return *node;

        node = (T*)singleton_register_instance(new T, &destroy, type, file, line);
        return *node;
	}

private:
    static void destroy(void* p) {
        delete (T*)p;
    }
};

////////////////////////////////////////////////////////////////////////////////
template<class T>
class local_singleton
{
public:

    T* operator -> () {
        return p;
    }

    T& operator * () {
        return *p;
    }

    local_singleton(T* p)
    {
        this->p = (T*)singleton_register_instance(p, &destroy, 0, 0, 0);
    }

    local_singleton()
    {
        this->p = (T*)singleton_register_instance(new T, &destroy, 0, 0, 0);
    }

private:

    T* p;

    static void destroy(void* p) {
        delete p;
    }
};

COID_NAMESPACE_END

#endif //__COID_COMM_SINGLETON__HEADER_FILE__
