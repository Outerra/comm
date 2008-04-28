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

#include "singleton.h"
#include "str.h"
#include "hash/hashkeyset.h"


namespace coid {


///Global singleton registrator
struct GlobalSingleton
{
    GlobalSingleton() {
        last = 0;
    }

    void add( void* ptr, void (*fn_destroy)(void*) )
    {
        comm_mutex_guard<_comm_mutex> mxg(mx);

        Kill* k = new Kill(ptr, fn_destroy);
        k->next = last;

        last = k;
    }

    void destroy() {
        while(last) {
            Kill* tmp = last->next;

            last->destroy();
            delete last;

            last = tmp;
        }
    }

    ~GlobalSingleton() {
        destroy();
    }

private:
    struct Kill {
        void* ptr;
        void (*fn_destroy)(void*);

        Kill* next;

        void destroy() {
            fn_destroy(ptr);
        }

        Kill( void* ptr, void (*fn_destroy)(void*) )
            : ptr(ptr), fn_destroy(fn_destroy)
        {}
    };

    _comm_mutex mx;
    Kill* last;
};


////////////////////////////////////////////////////////////////////////////////
static GlobalSingleton& global() {
    static GlobalSingleton globs;
    return globs;
}

////////////////////////////////////////////////////////////////////////////////
void* singleton_register_instance( void* p, void (*fn_destroy)(void*) )
{
    global().add(p, fn_destroy);
    return p;
}

////////////////////////////////////////////////////////////////////////////////
void singletons_destroy()
{
    global().destroy();
}


} //namespace coid
