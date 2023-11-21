
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
 * Copyright (C) 2007-2023. All Rights Reserved.
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

#include "../log.h"
#include "../str.h"
#include "../ref_s.h"
#include "../function.h"
#include "../alloc/slotalloc.h"
#include "../sync/mutex.h"

COID_NAMESPACE_BEGIN


struct log_filter
{
    typedef function<void(ref<logmsg>&)> filter_fun;
    filter_fun _filter_fun;
    charstr _module;
    log::level _log_level;

    log_filter(const filter_fun& fn, const token& module, log::level level)
        : _filter_fun(fn)
        , _module(module)
        , _log_level(level)
    {}

    log_filter(log_filter&& lf) {
        _filter_fun = lf._filter_fun;
        _module.takeover(lf._module);
        _log_level = lf._log_level;
    }
};


////////////////////////////////////////////////////////////////////////////////
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
protected:
    slotalloc<log_filter> _filters;
    ref<logger_file> _logfile;

    log::level _minlevel = log::level::last;

    bool _stdout = false;
    bool _allow_perf = false;

    coid::comm_mutex _mutex;
public:

    /// @param std_out true if messages should be printed to stdout as well
    /// @param cache_msgs true if messages should be cached until the log file is specified with open()
    logger(bool std_out, bool cache_msgs);
    virtual ~logger() {}

    static void terminate();

    void open(const token& filename);

    static void post(const token& msg, const token& from = token(), const void* inst = 0);

#ifdef COID_VARIADIC_TEMPLATES

    ///Formatted log message
    template<class ...Vs>
    void print(const token& fmt, Vs&&... vs) {
        ref<logmsg> msgr = create_msg(log::level::none, tokenhash());
        if (!msgr)
            return;

        charstr& str = msgr->str();
        str.print(fmt, std::forward<Vs>(vs)...);
    }

    ///Formatted log message
    template<class ...Vs>
    void print(log::level type, const tokenhash& hash, const void* inst, const token& fmt, Vs&&... vs)
    {
        ref<logmsg> msgr = create_msg(type, hash, inst);
        if (!msgr)
            return;

        charstr& str = msgr->str();
        str.print(fmt, std::forward<Vs>(vs)...);
    }

#endif

    /// @return logmsg, filling the prefix by the log type (e.g. ERROR: )
    //ref<logmsg> operator()(log::level type = log::level::info, log::target target = log::target::primary_log, const tokenhash& hash = "");

    /// @return an empty logmsg object
    //ref<logmsg> create_empty_msg(log::level type, log::target target, const tokenhash& hash);

    ///Creates logmsg object if given log message type is enabled
    /// @param type log level
    /// @param hash tokenhash identifying the client (interface) name
    /// @param inst optional instance id
    /// @return logmsg reference or null if not enabled
    ref<logmsg> create_msg(log::level type, log::target target, const tokenhash& hash, const void* inst);

    const ref<logger_file>& file() const { return _logfile; }

    virtual void enqueue(ref<logmsg>&& msg);

    void set_log_level(log::level minlevel = log::level::last, bool allow_perf = false);

    static void enable_debug_out(bool en);

    uints register_filter(log_filter&& filter);
    void unregister_filter(uints pos);

    void flush();
};

////////////////////////////////////////////////////////////////////////////////
///
class stdoutlogger : public logger
{
public:

    stdoutlogger() : logger(true, false)
    {}
};

COID_NAMESPACE_END


#endif // __COMM_LOGGER_H__
