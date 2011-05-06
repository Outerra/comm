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
    This header defines various assert macros. The assert macros normally log
    the failed assertion to the assert.log file and throw exception
    ersEXCEPTION afterwards.

    DASSERT*    debug-build only assertions
    EASSERT*    debug-build only assertion, but the expression alone gets evaluated in release build too
    RASSERT*    release and debug assertions

    *ASSERTX    provide additional text that will be logged upon failed assertion
    *ASSERTE[X] provide custom exception value that will be thrown upon failed assertion
    *ASSERTL[X] do not throw exceptions at all, only logs the failed assertion
    ASSERT_RET  assert in debug build, log in release, on failed assertion causes return from function where used
*/

////////////////////////////////////////////////////////////////////////////////
#ifdef SYSTYPE_MSVC
# ifdef _DEBUG
#define XASSERT                     if(__assert_e) __debugbreak();
# else
#define XASSERT                     if(__assert_e) throw ersABORT;
# endif
#else
#include <assert.h>
#define XASSERT                     assert(__assert_e);
#endif

#define XASSERTE(expr)              do{ if(expr) break;  coid::opcd __assert_e =

//@{ Runtime assertions
#define RASSERT(expr)               XASSERTE(expr) coid::__rassert(0,ersEXCEPTION,__FILE__,__LINE__,#expr); XASSERT } while(0)
#define RASSERTX(expr,txt)          XASSERTE(expr) coid::__rassert(txt,ersEXCEPTION,__FILE__,__LINE__,#expr); XASSERT } while(0)

#define RASSERTE(expr,exc)          XASSERTE(expr) coid::__rassert(0,exc,__FILE__,__LINE__,#expr); if(__assert_e) throw exc; } while(0)
#define RASSERTEX(expr,exc,txt)     XASSERTE(expr) coid::__rassert(txt,exc,__FILE__,__LINE__,#expr); if(__assert_e) throw exc; } while(0)
#define RASSERTXE(expr,exc,txt)     RASSERTEX(expr,exc,txt)

#define RASSERTL(expr)              XASSERTE(expr) coid::__rassert(0,0,__FILE__,__LINE__,#expr); XASSERT } while(0)
#define RASSERTLX(expr,txt)         XASSERTE(expr) coid::__rassert(txt,0,__FILE__,__LINE__,#expr); XASSERT } while(0)
#define RASSERTXL(expr,txt)         RASSERTLX(expr,txt)
//@}

////////////////////////////////////////////////////////////////////////////////
#ifdef _DEBUG

//@{ Debug-only assertions, release build doesn't see anything from it
#define DASSERT(expr)               XASSERTE(expr) coid::__rassert(0,ersEXCEPTION,__FILE__,__LINE__,#expr); XASSERT } while(0)
#define DASSERTX(expr,txt)          XASSERTE(expr) coid::__rassert(txt,ersEXCEPTION,__FILE__,__LINE__,#expr); XASSERT } while(0)

#define DASSERTL(expr)              XASSERTE(expr) coid::__rassert(0,0,__FILE__,__LINE__,#expr); } while(0)
#define DASSERTLX(expr,txt)         XASSERTE(expr) coid::__rassert(txt,0,__FILE__,__LINE__,#expr); } while(0)
//@}

//@{ Debug assertion, but in release build the expression \a expr is still evaluated
#define EASSERT(expr)               XASSERTE(expr) coid::__rassert(0,ersEXCEPTION,__FILE__,__LINE__,#expr); XASSERT } while(0)
#define EASSERTX(expr,txt)          XASSERTE(expr) coid::__rassert(txt,ersEXCEPTION,__FILE__,__LINE__,#expr); XASSERT } while(0)

#define EASSERTL(expr)              XASSERTE(expr) coid::__rassert(0,0,__FILE__,__LINE__,#expr); } while(0)
#define EASSERTLX(expr,txt)         XASSERTE(expr) coid::__rassert(txt,0,__FILE__,__LINE__,#expr); } while(0)
//@}

//@{ Assert in debug, log in release, return \a ret on failed assertion
#define ASSERT_RET(expr,ret)        XASSERTE(expr) coid::__rassert(0,ersEXCEPTION,__FILE__,__LINE__,#expr); XASSERT return ret; } while(0)
#define ASSERT_RETVOID(expr)        XASSERTE(expr) coid::__rassert(0,ersEXCEPTION,__FILE__,__LINE__,#expr); XASSERT return; } while(0)

#define DASSERT_RET(expr,ret)       ASSERT_RET(expr,ret)
#define DASSERT_RETVOID(expr)       ASSERT_RETVOID(expr)
//@}

#else

#define DASSERT(expr)
#define DASSERTX(expr,txt)

#define DASSERTE(expr,exc)
#define DASSERTEX(expr,exc,txt)

#define DASSERTN(expr)
#define DASSERTNX(expr,txt)


#define EASSERT(expr)               expr
#define EASSERTX(expr,txt)          expr

#define EASSERTE(expr,exc)          expr
#define EASSERTEX(expr,exc,txt)     expr

#define EASSERTN(expr)              expr
#define EASSERTNX(expr,txt)         expr


#define ASSERT_RET(expr,ret)        XASSERTE(expr) coid::__rassert(0,0,__FILE__,__LINE__,#expr); return ret; } while(0)
#define ASSERT_RETVOID(expr)        XASSERTE(expr) coid::__rassert(0,0,__FILE__,__LINE__,#expr); return; } while(0)

#define DASSERT_RET(expr,ret)
#define DASSERT_RETVOID(expr)

#endif //_DEBUG


///Compile-time assertion
#define STATIC_ASSERT_(B) \
	typedef coid::static_assert_test<coid::static_assertion_failure<(bool)(B)> >\
	coid_static_assert_typedef_##__COUNTER__;


////////////////////////////////////////////////////////////////////////////////
COID_NAMESPACE_BEGIN

struct opcd;

opcd __rassert( const char* txt, opcd exc, const char* file, int line, const char* expr );


template <bool x> struct static_assertion_failure;

template <> struct static_assertion_failure<true>
{ enum { value = 1 }; };

template<int x> struct static_assert_test{};

COID_NAMESPACE_END

#endif  //!__COID_COMM_ASSERT__HEADER_FILE__
