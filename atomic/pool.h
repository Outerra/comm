#pragma once


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
 * Outerra s.r.o
 * Portions created by the Initial Developer are Copyright (C) 2023
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Ladislav Hrabcak
 * Cyril Gramblicka
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

#include "stack_base.h"

namespace coid
{
template<class T>
class pool
    : protected atomic::stack_base<T>
{
    using stack_t = atomic::stack_base<T>;

public:
    pool()
        : atomic::stack_base<T>()
    {}

    /// return global pool for type T
    static pool& global() { return SINGLETON(pool<T>); }

    /// @brief Get item from pool
    /// @return item from pool or nullptr
    T* get_item() {
        return stack_t::pop();
    }

    /// @brief Create instance or take one from pool
    T* create_item() {
        T* inst = stack_t::pop();
        if (!inst)
            inst = new T;
        return inst;
    }

    /// return instance to pool
    void release_item(T*& o) {
        // create trait for reset !!!
        stack_t::push(o);
        o = 0;
    }
};

}; // end of namespace coid