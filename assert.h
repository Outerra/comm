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

#ifndef __COID_COMM_ASSERT__HEADER_FILE__
#define __COID_COMM_ASSERT__HEADER_FILE__

#include "namespace.h"
#include "retcodes.h"

/** \file assert.h
    This header defines following assert macros:



*/
COID_NAMESPACE_BEGIN


#define LINESTR(L)  //#L


////////////////////////////////////////////////////////////////////////////////
#define CLAIM(expr, ret)            if(!(expr)) return (ret)

#define XASSERT_1(expr)             do{ if(expr) break;  opcd __assert_e =
#define XASSERT_2                   if(__assert_e) throw __assert_e;

#define RASSERT(expr)               XASSERT_1(expr) __rassert(0,ersEXCEPTION __FILE__ ":" LINESTR(__LINE__) " " #expr,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)
#define RASSERTX(expr,txt)          XASSERT_1(expr) __rassert(txt,ersEXCEPTION __FILE__ ":" LINESTR(__LINE__) " " #expr "\n" txt,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)

#define RASSERTE(expr,exc)          XASSERT_1(expr) __rassert(0,exc __FILE__ ":" LINESTR(__LINE__) " " #expr,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)
#define RASSERTXE(expr,txt,exc)     XASSERT_1(expr) __rassert(txt,exc __FILE__ ":" LINESTR(__LINE__) " " #expr "\n" txt,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)

#define RASSERTN(expr)              XASSERT_1(expr) __rassert(0,0,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)
#define RASSERTXN(expr,txt)         XASSERT_1(expr) __rassert(txt,0,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)


////////////////////////////////////////////////////////////////////////////////
#ifdef _DEBUG

#define DASSERT(expr)               XASSERT_1(expr) __rassert(0,ersEXCEPTION __FILE__ ":" LINESTR(__LINE__) " " #expr,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)
#define DASSERTX(expr,txt)          XASSERT_1(expr) __rassert(txt,ersEXCEPTION __FILE__ ":" LINESTR(__LINE__) " " #expr "\n" txt,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)

#define DASSERTE(expr,exc)          XASSERT_1(expr) __rassert(0,exc __FILE__ ":" LINESTR(__LINE__) " " #expr,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)
#define DASSERTXE(expr,txt,exc)     XASSERT_1(expr) __rassert(txt,exc __FILE__ ":" LINESTR(__LINE__) " " #expr "\n" txt,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)

#define DASSERTN(expr)              XASSERT_1(expr) __rassert(0,0,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)
#define DASSERTXN(expr,txt)         XASSERT_1(expr) __rassert(txt,0,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)


#define EASSERT(expr)               XASSERT_1(expr) __rassert(0,ersEXCEPTION __FILE__ ":" LINESTR(__LINE__) " " #expr,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)
#define EASSERTX(expr,txt)          XASSERT_1(expr) __rassert(txt,ersEXCEPTION __FILE__ ":" LINESTR(__LINE__) " " #expr "\n" txt,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)

#define EASSERTE(expr,exc)          XASSERT_1(expr) __rassert(0,exc __FILE__ ":" LINESTR(__LINE__) " " #expr,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)
#define EASSERTXE(expr,txt,exc)     XASSERT_1(expr) __rassert(txt,exc __FILE__ ":" LINESTR(__LINE__) " " #expr "\n" txt,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)

#define EASSERTN(expr)              XASSERT_1(expr) __rassert(0,0,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)
#define EASSERTXN(expr,txt)         XASSERT_1(expr) __rassert(txt,0,__FILE__,__LINE__,#expr); XASSERT_2 } while(0)

#else

#define DASSERT(expr)
#define DASSERTX(expr,txt)

#define DASSERTE(expr,exc)
#define DASSERTXE(expr,txt,exc)

#define DASSERTN(expr)
#define DASSERTXN(expr,txt)


#define EASSERT(expr)               expr
#define EASSERTX(expr,txt)          expr

#define EASSERTE(expr,exc)          expr
#define EASSERTXE(expr,txt,exc)     expr

#define EASSERTN(expr)              expr
#define EASSERTXN(expr,txt)         expr


#endif //_DEBUG


struct opcd;

////////////////////////////////////////////////////////////////////////////////
opcd __rassert( const char* txt, opcd exc, const char* file, int line, const char* expr );


COID_NAMESPACE_END

#endif  //!__COID_COMM_ASSERT__HEADER_FILE__
