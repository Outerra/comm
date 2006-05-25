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

#ifndef __COID_COMM_RADIX__HEADER_FILE__
#define __COID_COMM_RADIX__HEADER_FILE__

#include "namespace.h"

#include "dynarray.h"

COID_NAMESPACE_BEGIN

///Return ptr to int data used for sorting
template <class INT>
struct T_GET_INT
{
    INT operator() (const void* p) const        { return *(INT*)p; }
};


////////////////////////////////////////////////////////////////////////////////
///Radix sort template with index output
/**
    @param T        class type to sort through
    @param INT_IDX  an int type to use as index, default uints
    @param INT_DAT  an int type to use as
**/
template <class T, class INT_IDX = uints, class INT_DAT = INT_IDX, class GETINT = T_GET_INT<INT_DAT> >
class radixi {
    uints _aucnts[256];
    dynarray<INT_IDX> _puidxa;
    INT_IDX* _puidxb;
    GETINT  _getint;

public:
    radixi()
    {
        memset(_aucnts, 0, sizeof(_aucnts) );
    }

    radixi( const GETINT& gi ) : _getint(gi)
    {
        memset(_aucnts, 0, sizeof(_aucnts) );
    }

    void set_getint( const GETINT& gi )
    {
        _getint = gi;
    }

    const INT_IDX * ret_index() const
    {
        return _puidxa.ptr();
    }

    uints size() const      { return _puidxa.size(); }

    INT_IDX* sortasc( const T* psort, uints nitems )
    {
        _puidxa.need_new( nitems<<1 );
        _puidxb= _puidxa.ptr() + nitems;
        sortone(psort);
        sorttwo(psort, _puidxb, _puidxa.ptr(), 8);
        if (sizeof(INT_DAT) > 2)
        {
            sorttwo(psort, _puidxa.ptr(), _puidxb, 16);
            sorttwo(psort, _puidxb, _puidxa.ptr(), 24);
        }
        return _puidxa.ptr();
    }

    INT_IDX* sortdsc( const T* psort, uints nitems )
    {
        _puidxa.need_new( nitems<<1 );
        _puidxb= _puidxa.ptr() + nitems;

        sortoned(psort);
        sorttwod(psort, _puidxb, _puidxa.ptr(), 8);
        if (sizeof(INT_DAT) > 2)
        {
            sorttwod(psort, _puidxa.ptr(), _puidxb, 16);
            sorttwod(psort, _puidxb, _puidxa.ptr(), 24);
        }
        return _puidxa.ptr();
    }

    T* bin_search_a( const T* psort, INT_DAT ufind ) const {
        uints i, j, m;
        i=0;
        j= _puidxa.size() >> 1;
        for(;j>i;) {
            m= (i+j)>>1;
            if(*(INT_DAT*)(psort+_puidxa[m]) == ufind)  return psort+_puidxa[m];
            if(*(INT_DAT*)(psort+_puidxa[m]) > ufind) j=m;
            else i=m+1;
        }
        return 0;
    }

    T* bin_search_d( const T* psort, INT_DAT ufind ) const {
        uints i, j, m;
        i=0;
        j= _puidxa.size() >> 1;
        for(;j>i;) {
            m= (i+j)>>1;
            if(*(INT_DAT*)(psort+_puidxa[m]) == ufind)  return psort+_puidxa[m];
            if(*(INT_DAT*)(psort+_puidxa[m]) > ufind) i=m+1;
            else j=m;
        }
        return 0;
    }

private:
    void sortone( const T* psrc )
    {
        uints i, j, nit;
        uints audsti[256];
        INT_IDX* puidx = _puidxb;

        nit = _puidxa.size() >> 1;
        for( i=0; i<nit; ++i )  ++_aucnts[(uchar)_getint(psrc+i)];

        for( j=0, i=0; i<256; ++i ) {
            audsti[i]= j;
            j += _aucnts[i];
            _aucnts[i]= 0;
        }

        for( i=0; i<nit; ++i )
        {
            uchar k = (uchar)_getint(psrc+i);
            puidx[audsti[k]] = (INT_IDX)i;
            ++audsti[k];
        }
    }

    void sorttwo( const T* psrc, INT_IDX* pusrcidx, INT_IDX *pudstidx, uints uoffs )
    {
        uints i, j, nit;
        uints audsti[256];

        nit = _puidxa.size() >> 1;
        for( i=0; i<nit; ++i )  ++_aucnts[(_getint(psrc+i) >> uoffs) & 0xff];

        for( j=0, i=0; i<256; ++i ) {
            audsti[i] = j;
            j += _aucnts[i];
            _aucnts[i] = 0;
        }

        for( i=0; i<nit; ++i ) {
            uchar k = (_getint(psrc+pusrcidx[i]) >> uoffs) & 0xff;
            pudstidx[audsti[k]] = pusrcidx[i];
            ++audsti[k];
        }
    }

    void sortoned( const T* psrc )
    {
        ints i, j, nit;
        uints audsti[256];
        INT_IDX* puidx = _puidxb;

        nit = _puidxa.size() >> 1;
        for( i=0; i<nit; ++i )  ++_aucnts[(uchar)_getint(psrc+i)];

        for( j=0, i=255; i>=0; --i ) {
            audsti[i] = j;
            j += _aucnts[i];
            _aucnts[i] = 0;
        }

        for( i=0; i<nit; ++i ) {
            uchar k = (uchar)_getint(psrc+i);
            puidx[audsti[k]] = (INT_IDX)i;
            ++audsti[k];
        }
    }

    void sorttwod( const T* psrc, INT_IDX* pusrcidx, INT_IDX *pudstidx, uints uoffs )
    {
        ints i, j, nit;
        uints audsti[256];

        nit = _puidxa.size() >> 1;
        for( i=0; i<nit; ++i )  ++_aucnts[(_getint(psrc+i) >> uoffs) & 0xff];

        for( j=0, i=255; i>=0; --i ) {
            audsti[i] = j;
            j += _aucnts[i];
            _aucnts[i] = 0;
        }

        for( i=0; i<nit; ++i ) {
            uchar k = (uchar) ( (_getint(psrc+pusrcidx[i]) >> uoffs) & 0xff );
            pudstidx[audsti[k]] = pusrcidx[i];
            ++audsti[k];
        }
    }
};


////////////////////////////////////////////////////////////////////////////////
///Radix sort on 32 bit values
template <class T>
class radix {
    uints _aucnts[256];
    dynarray<T> _ptemp;

public:
    radix() {memset(_aucnts, 0, sizeof(_aucnts));}

    void sort(T* psort, uints nitems) {
        _ptemp.need_new(nitems);
        sortone(psort, _ptemp, 0);
        sortone(_ptemp, psort, 8);
        sortone(psort, _ptemp, 16);
        sortone(_ptemp, psort, 24);
    }

    uints size() const { return _ptemp._nused; }

    T* bin_search(const T* psort, uints ufind, uints ufirst=0, uints ulast=UMAX) const {
        uints i, j, m;
        i= ufirst;
        j= ulast==UMAX ? _ptemp.size() : ulast;
        for(;j>i;) {
            m= (i+j)>>1;
            if(*(uints*)(psort+m) == ufind)  return (T*)psort+m;
            if(*(uints*)(psort+m) < ufind) i=m+1;
            else j=m;
        }
        return 0;
    }

private:
    void sortone(T* psrc, T* pdst, uints uoffs) {
        uints i, j, nit;
        T* apdst[256];

        nit= _ptemp.size ();
        for(i=0; i<nit; ++i)  ++_aucnts[((*(uints*)(psrc+i)) >> uoffs) & 0xff];

        for(j=0, i=0; i<256; ++i) {
            apdst[i]= pdst +j;
            j+= _aucnts[i];
            _aucnts[i]= 0;
        }

        for(i=0; i<nit; ++i) {
            j= ((*(uints*)(psrc+i)) >> uoffs) & 0xff;
            *apdst[j]= psrc[i];
            ++apdst[j];
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
template<class T>
class radix_index
{
    const T* _p;

public:
    void set(const T* p) {
        _p= p;
    }

    const T* get() const {
        return _p;
    }

    bool operator < (const radix_index& idx) const {
        return _p->operator < (*idx._p);
    }

    bool operator == (const radix_index& idx) const {
        return _p->operator == (*idx._p);
    }
};

COID_NAMESPACE_END

#endif //__COID_COMM_RADIX__HEADER_FILE__
