#pragma once

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
 * Portions created by the Initial Developer are Copyright (C) 2023
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

#ifndef __COMM_LOG_H__
#define __COMM_LOG_H__

#include "str.h"
#include "ref_s.h"

 ////////////////////////////////////////////////////////////////////////////////
 //@{ Log message with specified severity
#define coidlog_exc(src, msg)     do{ ref<coid::logmsg> q = coid::log::openmsg(coid::log::level::exception, src); if (q) {q->str() << msg; }} while(0)
#define coidlog_error(src, msg)   do{ ref<coid::logmsg> q = coid::log::openmsg(coid::log::level::error, src); if (q) {q->str() << msg; }} while(0)
#define coidlog_warning(src, msg) do{ ref<coid::logmsg> q = coid::log::openmsg(coid::log::level::warning, src); if (q) {q->str() << msg; }} while(0)
#define coidlog_msg(src, msg)     do{ ref<coid::logmsg> q = coid::log::openmsg(coid::log::level::highlight, src); if (q) {q->str() << msg; }} while(0)
#define coidlog_info(src, msg)    do{ ref<coid::logmsg> q = coid::log::openmsg(coid::log::level::info, src); if (q) {q->str() << msg; }} while(0)
#define coidlog_debug(src, msg)   do{ ref<coid::logmsg> q = coid::log::openmsg(coid::log::level::debug, src); if (q) {q->str() << msg; }} while(0)
#define coidlog_perf(src, msg)    do{ ref<coid::logmsg> q = coid::log::openmsg(coid::log::level::perf, src); if (q) {q->str() << msg; }} while(0)
#define coidlog_none(src, msg)    do{ ref<coid::logmsg> q = coid::log::openmsg(coid::log::level::none, src); if (q) {q->str() << msg; }} while(0)

#define coidlog_loglevel(level, src, msg) do{ ref<coid::logmsg> q = coid::log::openmsg(level, src); if (q) {q->str() << msg; }} while(0)
//@}

///Log into file. The file is trucated initially and multiple messages are appended to it during the process duration
//@param file file name/path
#define coidlog_file(file, msg)   do{ ref<coid::logmsg> q = coid::log::filemsg(coid::log::level::info, file); if (q) {q->str() << msg; }} while(0)

///Post fading message (if fading handler is attached in the logger)
#define coidlog_fade(level, msg) do{ ref<coid::logmsg> q = coid::log::fademsg(level, ""); if (q) {q->str() << msg; }} while(0)

///Log message with severity specified at the beginning of msg
/// @param src source module
/// @param msg text message to log, containing a severity prefix (error: err: warning: warn: info: msg: debug: perf:
void coidlog_text(const coid::token& src, coid::token msg);


///Create a perf object that logs the time while the scope exists
#define coidlog_perf_scope(src, msg) \
   ref<coid::logmsg> perf##line = coid::log::openmsg(coid::log::level::perf, src); if (perf##line) perf##line->str() << msg

///Log fatal error and throw exception with the same message
#define coidlog_exception(src, msg)\
    do { ref<coid::logmsg> q = coid::log::openmsg(coid::log::level::exception, src); if (q) {q->str() << msg; throw coid::exception() << msg; }} while(0)

//@{ Log error if condition fails
#define coidlog_assert(test, src, msg)\
    do { if (!(test)) coidlog_error(src, msg); } while(0)

#define coidlog_assert_ret(test, src, msg, ...)\
    do { if (!(test)) { coidlog_error(src, msg); return __VA_ARGS__; } } while(0)
//@}

///Debug message existing only in debug builds
#ifdef _DEBUG
#define coidlog_devdbg(src, msg)  do{ ref<coid::logmsg> q = coid::log::openmsg(coid::log::level::debug, src); if (q) {q->str() << msg; }} while(0)
#else
#define coidlog_devdbg(src, msg)
#endif


COID_NAMESPACE_BEGIN

class logmsg;

namespace log {

    /// @brief log message severity levels
    enum class level {
        none = -1,                      ///< unspecified, determined from text
        exception = 0,                  ///< fatal
        error,                          ///< error
        warning,                        ///< warning
        highlight,                      ///< highlighted info
        info,                           ///< information
        debug,                          ///< debug
        perf,                           ///< performance

        last,
    };

    /// @brief limited backward compatibility
    using type = level;

    /// @brief target for the message
    enum class target {
        primary_log,                    ///< primary log file
        file,                           ///< message source determines the output file
        fade,                           ///< fading display
    };

    const char** names();
    const level* values();

    const char* name(level t);

    //@return logmsg object if given log type and source is currently allowed to log
    ref<logmsg> openmsg(level type, const tokenhash& hash = tokenhash(), const void* inst = 0);

    /// @brief Register file message
    ref<logmsg> filemsg(level type, const tokenhash& file, const void* inst = 0);

    /// @brief Register fading message
    ref<logmsg> fademsg(level type, const tokenhash& hash = tokenhash(), const void* inst = 0);

    void flush();

} //namespace log

class logger_file;
class logger;
class policy_msg;

/// @brief Return logging object for multi-line logging, ex: auto log = filelog("path"); log->str() << "bla" << 1;
/// @param file file name/path
inline ref<logmsg> filelog(log::level type, const tokenhash& file) {
    return log::filemsg(type, file);
}

#ifdef COID_VARIADIC_TEMPLATES

///Formatted log message
//@param type log level
//@param hash source identifier (used for filtering)
//@param fmt @see charstr.print
template<class ...Vs>
void printlog(log::level type, const tokenhash& hash, const token& fmt, Vs&&... vs);

#endif //COID_VARIADIC_TEMPLATES

////////////////////////////////////////////////////////////////////////////////

/*
 * Log message object, returned by the logger.
 * Message is written in policy_log::release() upon calling destructor of ref
 * with policy policy_log.
 */
class logmsg
    //: public policy_pooled<logmsg>
{
protected:

    friend class policy_msg;

    logger* _logger = 0;
    ref<logger_file> _logger_file;

    tokenhash _hash;
    log::level _type = log::level::none;
    log::target _target = log::target::primary_log;
    charstr _str;
    uint64 _time = 0;

public:

    COIDNEWDELETE(logmsg);

    logmsg();

    logmsg(logmsg&& other) noexcept
        : _type(other._type)
    {
        _logger = other._logger;
        other._logger = 0;

        _logger_file.takeover(other._logger_file);
        _str.takeover(other._str);
    }

    void set_target(log::target target) {
        _target = target;
    }

    void set_logger(logger* l) { _logger = l; }

    void reset() {
        _str.reset();
        _logger = 0;
        _logger_file.release();
    }

    void write();

    ///Consume type prefix from the message
    static log::level consume_type(token& msg)
    {
        log::level t = log::level::info;
        if (msg.consume_icase("err:"_T) || msg.consume_icase("error:"_T))
            t = log::level::error;
        else if (msg.consume_icase("warn:"_T) || msg.consume_icase("warning:"_T))
            t = log::level::warning;
        else if (msg.consume_icase("info:"_T))
            t = log::level::info;
        else if (msg.consume_icase("msg:"_T))
            t = log::level::highlight;
        else if (msg.consume_icase("dbg:"_T) || msg.consume_icase("debug:"_T))
            t = log::level::debug;
        else if (msg.consume_icase("perf:"_T))
            t = log::level::perf;
        else if (msg.consume_icase("fatal: "_T))
            t = log::level::exception;

        msg.skip_whitespace();
        return t;
    }

    static const token& type2tok(log::level t)
    {
        static token st[1 + int(log::level::last)] = {
            "",
            "FATAL: ",
            "ERROR: ",
            "WARNING: ",
            "INFO: ",
            "INFO: ",
            "DEBUG: ",
            "PERF: ",
        };
        static token empty;

        return t > log::level::none && t < log::level::last ? st[1 + int(t)] : empty;
    }

    log::level get_type() const { return _type; }
    log::target get_target() const { return _target; }

    void set_type(log::level t) { _type = t; }

    void set_time(uint64 ns) {
        _time = ns;
    }

    int64 get_time() const { return _time; }

    void set_hash(const tokenhash& hash) { _hash = hash; }
    const tokenhash& get_hash() { return _hash; }

    charstr& str() { return _str; }
    const charstr& str() const { return _str; }

protected:

    void finalize(policy_msg* p);
};

typedef ref<logmsg> logmsg_ptr;

////////////////////////////////////////////////////////////////////////////////

#ifdef COID_VARIADIC_TEMPLATES

///Formatted log message
//@param type log level
//@param hash source identifier (used for filtering)
//@param fmt @see charstr.print
template<class ...Vs>
inline void printlog(log::level type, const tokenhash& hash, const token& fmt, Vs&&... vs)
{
    ref<logmsg> msgr = log::openmsg(type, hash);
    if (!msgr)
        return;

    charstr& str = msgr->str();
    str.print(fmt, std::forward<Vs>(vs)...);
}

#endif //COID_VARIADIC_TEMPLATES

COID_NAMESPACE_END

#endif // __COMM_LOG_H__
