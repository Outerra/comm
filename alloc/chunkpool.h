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

#ifndef __COID_COMM_CHUNKPOOL__HEADER_FILE__
#define __COID_COMM_CHUNKPOOL__HEADER_FILE__


#include "chunkpage.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
///Creator template that allows reusing of the elements
template<class T>
struct ReusingElementCreator
{
    static const uints size = sizeof(T);

    void set_size( uints bytes )                    { }
    static T* create( void* p, bool firsttime )     { return firsttime ? new(p) T : (T*)p; }
    static void destroy( void* p, bool final )      { if(final)  delete (T*)p; }
};

////////////////////////////////////////////////////////////////////////////////
template<class T>
class chunkpool : public chunkpage<T, ReusingElementCreator<T> >
{
public:
    typedef chunkpage<T, ReusingElementCreator<T> >     TBase;

    chunkpool()
    {
    }

    chunkpool( uints pagesize ) : TBase(pagesize)
    {
    }
};



COID_NAMESPACE_END

#endif //#ifndef __COID_COMM_CHUNKPOOL__HEADER_FILE__

