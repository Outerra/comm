
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
 * The Original Code is coid/comm module.
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

#ifndef __COID_COMM__TREENODE_HEADER__
#define __COID_COMM__TREENODE_HEADER__

#include "dynarray.h"

////////////////////////////////////////////////////////////////////////////////
//T must be derived from treenode<T>
template<class T>
struct treenode
{
    T* up() const           { return super; }
    T* next() const
    {
        if(!super)  return 0;
        const dynarray<pT>& sub = super->lower;
        int k = dynarray_contains( sub, this );
        if( k<0 || k+1>=sub.size() )  return 0;
        return sub[k+1];
    }

    T** add_children( uint n, segchunker<T>& allocator = SINGLETON(segchunker<T>) )
    {
        T** p = lower.add(n);
        for( uint i=0; i<n; ++i ) {
            p[i] = allocator.alloc();
            p[i]->super = (T*)this;
        }
        return p;
    }

    void destroy_children()
    {
        uint n = lower.size();
        for( uint i=0; i<n; ++i )
        {
            lower[i]->~T();
            segchunker<T>::free( lower[i] );
        }
        lower.reset();
    }

    treenode()
    {
        super = 0;
    }

    ~treenode()
    {
        destroy_children();
    }

protected:
    dynarray<T*> lower;
    T* super;
};

#endif //__COID_COMM__TREENODE_HEADER__
