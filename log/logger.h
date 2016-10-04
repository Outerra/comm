
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
 * Ladislav Hrabcak
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
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

#ifndef __COMM_LOGGER_H__
#define __COMM_LOGGER_H__

#include "../binstream/filestream.h"
#include "../binstream/txtstream.h"
#include "../str.h"
#include "../ref.h"
#include "../singleton.h"
#include "policy_log.h"
#include "../atomic/queue_base.h"
#include "../atomic/pool_base.h"

COID_NAMESPACE_BEGIN

class logmsg;
class logger;

class logger_file
{
	bofstream _logfile;
    charstr _logbuf;
    charstr _logpath;

    bool check_file_open()
    {
        if(_logfile.is_open() || !_logpath)
            return _logfile.is_open();

        opcd e = _logfile.open(_logpath);
        if(!e) {
            _logfile.xwrite_token_raw(_logbuf);
            _logbuf.free();
        }

        return e==0;
    }

public:
    logger_file() {}
    logger_file(const token& path) : _logpath(path)
    {}
        
    ///Open physical log file. @note Only notes the file name, the file is opened with the next log msg because of potential MT clashes
    void open(charstr filename) {
        std::swap(_logpath, filename);
    }

	void write_to_file(const charstr& lm) {
        if(check_file_open())
            _logfile.xwrite_token_raw(lm);
        else
            _logbuf << lm;
    }
};

typedef ref<logger_file> logger_file_ptr;

/* 
 * Log message object, returned by the logger.
 * Message is written in policy_log::release() upon calling destructor of ref
 * with policy policy_log.
 */
class logmsg 
    : public policy_pooled_i<logmsg>
{
protected:
	logger_file_ptr _lf;
	int _type;
    charstr _str;

public:
	void set_log_file(logger_file_ptr& lf,const int t) { _lf=lf; _type=t; }

	void reset() { _str.reset(); _lf.release(); }

	void write_to_file() { _lf->write_to_file(_str); }

	int get_type() const { return _type; }

	void set_type(const int t) { _type=t; }

    charstr& str() { return _str; }
};

typedef iref<logmsg> logmsg_ptr;

/* 
 * USAGE :
 *
 * class fwmlogger : public logger
 * {
 * public:
 *     fwmlogger() : logger("fwm.log") {}
 * };
 * #define fwmlog(msg) (*SINGLETON(fwmlogger)()<<msg)
 * 
 * USAGE:
 * 
 * fwmlog("ugh" << x << "asd")
 */
class logger
{
    friend class logmsg_local;

public:
	enum ELogType {
		Exception=0,
		Error,
		Warning,
		Info,
        Highlight,
		Debug,
        Perf,
		Last,
        None,
	};

	class logmsg_local
	{
    protected:
		logmsg_ptr _lm;
        bool _stdout;

	public:
		logmsg_local(logger& lf, const ELogType t)
            : _lm(CREATE_POOLED)
            , _stdout(lf._stdout && t != Debug)
        {
            _lm->set_log_file(lf._logfile, t);
        }

		logmsg_local()
            : _lm(), _stdout(false)
        {}

		~logmsg_local();

        template<class T> charstr& operator << (const T& o) {
            DASSERT(!_lm.is_empty());
            return _lm->str() << (o);
        }
	};

public:

    static const token& type2tok( const ELogType t )
	{
		static token st[]={
			"EXCEPTION: ",
			"ERROR: ",
			"WARNING: ",
			"INFO: ",
            "INFO: ",
			"DEBUG: ",
            "PERF: ",
		};
        static token empty = token();

		return t<Last ? st[t] : empty;
	}

protected:
	logger_file_ptr _logfile;

    bool _stdout;

public:
	logger( bool std_out=false );

    void open(const token& filename) {
        _logfile->open(filename);
    }

public:
    logmsg_local operator()() {
        logmsg_local lm(*this, Info);
        return lm;
    }

    logmsg_local operator()(const ELogType t) {
        logmsg_local lm(*this, t);
        lm << type2tok(t);
        return lm;
    }

    logmsg_local operator()(const ELogType t, const char* fnc) {
        logmsg_local lm(*this, t);
        lm << type2tok(t) << fnc << ' ';
        return lm;
    }

    logmsg_local operator()(const ELogType t, const char* fnc, const int line) {
        logmsg_local lm(*this, t);
        lm << type2tok(t) << fnc << '(' << line << ')' << ' ';
        return lm;
    }

    void post(token msg)
    {
        static token ERR = "error:";
        static token WARN = "warning:";
        static token INFO = "info:";
        static token MSG = "msg:";
        static token DBG1 = "dbg:";
        static token DBG2 = "debug:";
        static token PERF = "perf:";

        ELogType t = Info;
        if(msg.consume_icase(ERR))
            t = Error;
        else if(msg.consume_icase(WARN))
            t = Warning;
        else if(msg.consume_icase(INFO))
            t = Info;
        else if(msg.consume_icase(MSG))
            t = Highlight;
        else if(msg.consume_icase(DBG1) || msg.consume_icase(DBG2))
            t = Debug;
        else if(msg.consume_icase(PERF))
            t = Perf;

        msg.skip_whitespace();

        logmsg_local lm(*this, t);
	    lm << type2tok(t) << msg;
    }

	void flush();
};

COID_NAMESPACE_END

#endif // __COMM_LOGGER_H__
