
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
 * Ladislav Hrabcak
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

#include "logwriter.h"
#include "../atomic/pool.h"
#include "../atomic/pool_base.h"

#include "../binstream/stdstream.h"

using namespace coid;

#ifdef SYSTYPE_WIN
#define WIN32_MEAN_AND_LEAN
#include <Windows.h>

static void write_console_text( const charstr& text, int type )
{
    static HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
    static int curtype = logger::Info;

    if(type != curtype) {
        uint flg;

        switch(type) {
        case logger::Exception: flg = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
        case logger::Error:     flg = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
        case logger::Warning:   flg = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case logger::Info:      flg = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break;
        case logger::Highlight: flg = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
        case logger::Debug:     flg = FOREGROUND_GREEN | FOREGROUND_BLUE; break;
        case logger::Perf:      flg = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        default:                flg = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
        }

        curtype = type;
        SetConsoleTextAttribute(hstdout, flg);
    }

    fwrite(text.ptr(), 1, text.len(), stdout);
}

#else

static void write_console_text( const charstr& text, int type )
{
    fwrite(text.ptr(), 1, text.len(), stdout);
}

#endif

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

logger::logger( bool std_out )
	: _logfile(new logger_file)
    , _stdout(std_out)
{
	SINGLETON(log_writer);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

log_writer::log_writer() 
	: _thread()
	, _queue()
{
    //make sure the dependent singleton gets created
    logmsg::pool();
	//SINGLETON(policy_pooled<logmsg>::pool_type);

	_thread.create( thread_run_fn, this, 0, "log_writer" );
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

log_writer::~log_writer()
{
   _thread.cancel_and_wait(10000);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void* log_writer::thread_run()
{
	for ( ;; ) {
        flush();
		if ( coid::thread::self_should_cancel() ) break;
		coid::sysMilliSecondSleep(500);
	}
    flush();

	return 0;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void log_writer::flush()
{
	logmsg_ptr m;

	//int maxloop = 3000 / 20;

	while( _queue.pop(m) ) {
        DASSERT( m->str() );
		m->write_to_file();
		m.release();
	}
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

logger::logmsg_local::~logmsg_local()
{
	if(!_lm.is_empty() && _lm.refcount() == 1) {
        if(_stdout)
            write_console_text(_lm->str(), _lm->get_type());

        SINGLETON(log_writer).addmsg(_lm);
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void logger::flush()
{
    int maxloop = 3000 / 20;
    while(!SINGLETON(log_writer).is_empty() && maxloop-- > 0)
        sysMilliSecondSleep(20);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
