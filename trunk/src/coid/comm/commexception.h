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
 * Portions created by the Initial Developer are Copyright (C) 2009
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


#ifndef __COID_COMM_EXCEPTION__HEADER_FILE__
#define __COID_COMM_EXCEPTION__HEADER_FILE__

#include "namespace.h"
#include "str.h"

COID_NAMESPACE_BEGIN

struct exception
{
    exception( token stext )
        : _stext(stext)
    {}

    exception()
        : _stext("unknown exception")
    {}


    exception& operator << (const char *czstr)  { _dtext += (czstr); return *this; }
    exception& operator << (const token& tok)   { _dtext += (tok);   return *this; }
    exception& operator << (const charstr& str) { _dtext += (str);   return *this; }
    exception& operator << (char c)             { _dtext += (c);     return *this; }

    exception& operator << (int8 i)             { _dtext.append_num(10,(int)i);  return *this; }
    exception& operator << (uint8 i)            { _dtext.append_num(10,(uint)i); return *this; }
    exception& operator << (int16 i)            { _dtext.append_num(10,(int)i);  return *this; }
    exception& operator << (uint16 i)           { _dtext.append_num(10,(uint)i); return *this; }
    exception& operator << (int32 i)            { _dtext.append_num(10,(int)i);  return *this; }
    exception& operator << (uint32 i)           { _dtext.append_num(10,(uint)i); return *this; }
    exception& operator << (int64 i)            { _dtext.append_num(10,i);       return *this; }
    exception& operator << (uint64 i)           { _dtext.append_num(10,i);       return *this; }

#ifdef SYSTYPE_WIN
# ifdef SYSTYPE_32
    exception& operator << (ints i)             { _dtext.append_num(10,(ints)i);  return *this; }
    exception& operator << (uints i)            { _dtext.append_num(10,(uints)i); return *this; }
# else //SYSTYPE_64
    exception& operator << (int i)              { _dtext.append_num(10,i); return *this; }
    exception& operator << (uint i)             { _dtext.append_num(10,i); return *this; }
# endif
#elif defined(SYSTYPE_32)
    exception& operator << (long i)             { _dtext.append_num(10,(ints)i);  return *this; }
    exception& operator << (ulong i)            { _dtext.append_num(10,(uints)i); return *this; }
#endif //SYSTYPE_WIN

    exception& operator << (double d)           { _dtext += (d); return *this; }

    ///Formatted numbers
    template<int WIDTH, int ALIGN, class NUM>
    exception& operator << (const num_fmt<WIDTH,ALIGN,NUM> v) {
        append_num(10, v.value, WIDTH, (EAlignNum)ALIGN);
        return *this;
    }

    exception& operator << (const opcd_formatter& f)
    {
        _dtext << f.e.error_desc();
        if( f.e.text() && f.e.text()[0] )
            _dtext << " : " << f.e.text();
        return *this;
    }



    token text() const {
        return _dtext.is_empty() ? _stext : (token)_dtext;
    }

protected:

    token _stext;
    charstr _dtext;
};


COID_NAMESPACE_END

#endif //__COID_COMM_EXCEPTION__HEADER_FILE__
