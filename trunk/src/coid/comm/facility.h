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

#ifndef __COID_COMM_FACILITY__HEADER_FILE__
#define __COID_COMM_FACILITY__HEADER_FILE__

#include "namespace.h"

#include "coid/comm/dynarray.h"
#include <algorithm>

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
struct GenericFacility
{
    virtual ~GenericFacility() {}
    virtual uint get_facility_uid() const = 0;
    virtual const char* get_facility_name() const = 0;
};

////////////////////////////////////////////////////////////////////////////////
struct GenericFacilityUID
{
    const char* _name;
    const char* get_name() const            { return _name; }

    GenericFacilityUID( const char* name )  { _name = name; }
};

////////////////////////////////////////////////////////////////////////////////
#define GENERIC_FACILITY(ifac) \
    virtual uint get_facility_uid() const           { return (uint)&get_facilityUID(); } \
    virtual const char* get_facility_name() const   { return get_facilityUID()._name; } \
    static GenericFacilityUID& get_facilityUID()    { static GenericFacilityUID _uid(#ifac);  return _uid; }

////////////////////////////////////////////////////////////////////////////////
/// Implement facility without modifications
#define IMPLEMENTS_FACILITY(ifac) \
struct __##ifac : ifac { };

/// Implement overloaded facility, begin
#define IMPLEMENTS_FACILITY_BEGIN(ifac) \
struct __##ifac : ifac {

/// End implement overloaded facility
#define IMPLEMENTS_FACILITY_END(ifac) \
};

/// Register the facility
#define REGISTER_FACILITY(ifac) \
    FacilityContainer::insert( new __##ifac )

/// Register the facility with arguments, pass the arguments in ()
#define REGISTER_FACILITY_WITHARGS(ifac,args) \
    FacilityContainer::insert( new __##ifac args )

////////////////////////////////////////////////////////////////////////////////
struct FacilityContainer
{
    dynarray<GenericFacility*>  _facilities;

    // less comparator
    struct FacilityCompare : std::binary_function<GenericFacility*, GenericFacility*, bool>
    {
        bool operator() ( const GenericFacility* a, const GenericFacility* b ) const
        {
            return a->get_facility_uid() < b->get_facility_uid();
        }

        bool operator() ( const GenericFacility* a, const uint b ) const
        {
            return a->get_facility_uid() < b;
        }
    };

    // find if container implements facility
    template <typename FACILITY>
    int implements( FACILITY*& fac ) const
    {
        uint key = (uint)&fac->get_facilityUID();
        dynarray<GenericFacility*>::const_iterator pi =
            std::lower_bound( _facilities.begin(), _facilities.end(), key, FacilityCompare() );
        if( pi < _facilities.end()  &&  (*pi)->get_facility_uid() == key )
        {
            fac = (FACILITY*) *pi;
            return 1;
        }
        return 0;
    }

    // insert facility
    void insert( GenericFacility* f )
    {
        dynarray<GenericFacility*>::const_iterator pi =
            std::lower_bound( _facilities.begin(), _facilities.end(), f->get_facility_uid(), FacilityCompare() );
        *_facilities.ins( &(*pi) - _facilities.ptr() ) = f;
    }
};



COID_NAMESPACE_END


#endif //#ifndef __COID_COMM_FACILITY__HEADER_FILE__
