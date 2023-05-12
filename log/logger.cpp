
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
 //#include "../atomic/pool.h"
#include "../atomic/pool_base.h"

#include "../binstream/filestream.h"
#include "../binstream/stdstream.h"

#include "../hash/slothash.h"
#include "../interface.h"
#include "../timer.h"
#include "../net_ul.h"

using namespace coid;

static bool _enable_debug_out = false;

#ifdef SYSTYPE_WIN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static void write_console_text(const logmsg& msg)
{
    const charstr& text = msg.str();
    log::type type = msg.get_type();

    static HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);

    if (type != log::info) {
        uint flg;

        switch (type) {
        case log::exception: flg = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
        case log::error:     flg = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
        case log::warning:   flg = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case log::info:      flg = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break;
        case log::highlight: flg = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
        case log::debug:     flg = FOREGROUND_GREEN | FOREGROUND_BLUE; break;
        case log::perf:      flg = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        default:             flg = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
        }

        SetConsoleTextAttribute(hstdout, flg);
    }

    fwrite(text.ptr(), 1, text.len(), stdout);

    if (type != log::info)
        SetConsoleTextAttribute(hstdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

    if (_enable_debug_out)
        stdoutstream::debug_out(text.c_str());
}

#else

static void write_console_text(const logmsg& msg)
{
    fwrite(msg.str().ptr(), 1, msg.str().len(), stdout);
}

#endif

////////////////////////////////////////////////////////////////////////////////

void coidlog_text(const token& src, token msg)
{
    logger::post(msg, src);
}


namespace coid {
namespace log {

const type* values() {
    static type _values[] = {
        none,
        exception,
        error,
        warning,
        highlight,
        info,
        debug,
        perf,
    };
    return _values;
}

const char** names() {
    static const char* _names[] = {
        "none",
        "exception",
        "error",
        "warning",
        "highlight",
        "info",
        "debug",
        "perf",
        0
    };
    return _names;
}

const char* name(type t)
{
    return t >= none && t <= last ? names()[t + 1] : 0;
}


ref<logmsg> openmsg(log::type type, const tokenhash& hash, const void* inst)
{
    return interface_register::canlog(type, hash, inst);
}

void flush()
{
    interface_register::getlog()->flush();
}

} //namespace log

////////////////////////////////////////////////////////////////////////////////

class policy_msg : public policy_base
{
public:
    COIDNEWDELETE(policy_msg);

    typedef pool<policy_msg> pool_type;

protected:

    pool_type* _pool;
    logmsg* _obj;

protected:

    ///
    explicit policy_msg(logmsg* const obj, pool_type* const p = 0)
        : _pool(p)
        , _obj(obj)
    {}

public:

    logmsg* get() const { return _obj; }

    virtual void _destroy() override
    {
        DASSERT(_pool != 0);

        if (_obj->_logger) {
            //first destroy just queues the message
            logger* x = _obj->_logger;
            _obj->finalize(this);
        }
        else {
            //back to the pool
            policy_msg* t = this;
            _pool->release_item(t);
        }
    }

    ///
    static policy_msg* create()
    {
        pool_type& pool = pool_singleton();
        policy_msg* p = pool.get_item();

        if (!p)
            p = new policy_msg(new logmsg, &pool);
        else
            p->get()->reset();

        return p;
    }

    static pool_type& pool_singleton() {
        LOCAL_PROCWIDE_SINGLETON_DEF(pool_type) pool;
        return *pool;
    }
};


////////////////////////////////////////////////////////////////////////////////
class logger_file
{
    bofstream _logfile;
    charstr _logbuf;
    charstr _logpath;
    bool _stdout;


    bool check_file_open()
    {
        if (_logfile.is_open() || !_logpath)
            return _logfile.is_open();

        //check if it wasn't already opened in this instance
        LOCAL_SINGLETON_DEF(slothash<charstr, charstr>) reg;

        const charstr* oldv = reg->find_value(_logpath);
        if (!oldv)
            reg->push(_logpath);

        opcd e = _logfile.open(_logpath, oldv ? "wc+" : "wct");
        if (!e) {
            _logfile.xwrite_token_raw(_logbuf);
            _logbuf.free();
        }

        return e == 0;
    }

public:
    explicit logger_file(bool std_out) : _stdout(std_out) {}
    logger_file(const token& path, bool std) : _logpath(path), _stdout(std)
    {}

    ///Open physical log file
    /// @param filename file name/path to open
    /// @note Only notes the file name, the file is opened with the next log msg because of potential MT clashes
    void open(charstr&& filename, bool std) {
        _logpath = std::move(filename);
        _stdout = std;
    }

    void write_to_file(const logmsg& lm)
    {
        if (check_file_open())
            _logfile.xwrite_token_raw(lm.str());
        else
            _logbuf << lm.str();

        if (_stdout)
            write_console_text(lm);
    }
};

} //namespace coid

////////////////////////////////////////////////////////////////////////////////
logmsg::logmsg()
{
    //reserve memory from process allocator
    _str.reserve(128, PROCWIDE_SINGLETON(comm_array_mspace).msp);
}

////////////////////////////////////////////////////////////////////////////////
void logmsg::write()
{
    if (!_str.ends_with('\n'))
        _str.append('\n');

    if (_logger_file)
        _logger_file->write_to_file(*this);
    else
        write_console_text(*this);
}

////////////////////////////////////////////////////////////////////////////////
void logmsg::finalize(policy_msg* p)
{
    if (_type == log::perf) {
        int64 ns = nsec_timer::current_time_ns() - _time;
        _str << " (" << (ns * 1.0e-6f) << "ms)";
    }

    if (_type == log::none) {
        token tok = _str;
        _type = consume_type(tok);
        if (tok.len() < _str.len())
            _str.del(0, _str.len() - tok.len());
    }

    //bool flush = _str.last_char() == '\r';

    if (_type == log::file)
        _logger_file.create(new logger_file(_hash, false));
    else 
        _logger_file = _logger->file();
    _logger->enqueue(ref<logmsg>(p));
    _logger = 0;

    //return flush;
}




////////////////////////////////////////////////////////////////////////////////
logger::logger(bool std_out, bool cache_msgs)
    : _stdout(std_out)
    , _mutex(512, false)
{
    SINGLETON(log_writer);

    if (cache_msgs)
        _logfile = ref<logger_file>(new logger_file(std_out));
}

////////////////////////////////////////////////////////////////////////////////
void logger::terminate()
{
    SINGLETON(log_writer).terminate();
}

////////////////////////////////////////////////////////////////////////////////
void logger::enable_debug_out(bool en)
{
    _enable_debug_out = en;
}

////////////////////////////////////////////////////////////////////////////////
uints logger::register_filter(log_filter&& filter)
{
    GUARDTHIS(_mutex);
    log_filter * slot = _filters.push(std::move(filter));
    return _filters.get_item_id(slot);
}

////////////////////////////////////////////////////////////////////////////////
void logger::unregister_filter(uints pos)
{
    GUARDTHIS(_mutex);
    _filters.del(pos);
}

////////////////////////////////////////////////////////////////////////////////
void logger::set_log_level(log::type minlevel, bool allow_perf)
{
    _minlevel = minlevel;
    _allow_perf = allow_perf || _minlevel >= log::perf;
}

////////////////////////////////////////////////////////////////////////////////
ref<logmsg> logger::create_msg(log::type type, const tokenhash& hash, const void* inst, bool append_time)
{
    //TODO check hash, inst
    bool filelog = type == log::type::file;
    if (!filelog && type > _minlevel && (!_allow_perf || type != log::perf))
        return ref<logmsg>();

    ref<logmsg> rmsg = operator()(type, hash, append_time);
    if (!filelog && hash)
        rmsg->str() << '[' << hash << "] ";

    return rmsg;
}

////////////////////////////////////////////////////////////////////////////////
ref<logmsg> logger::operator()(log::type t, const tokenhash& hash, bool append_time)
{
    ref<logmsg> msg = create_msg(t, hash, append_time);
    if (!msg)
        return msg;

    charstr& str = msg->str();

    if (append_time) {
        int64 ms = nsec_timer::day_time_ns() / 1000000;
        str.append_time_formatted(ms, true, 3);
        str.append(' ');
    }

    str << logmsg::type2tok(t);

    return msg;
}

////////////////////////////////////////////////////////////////////////////////
ref<logmsg> logger::create_msg(log::type type, const tokenhash& hash, bool append_time)
{
    if (type != log::type::file && type > _minlevel && (!_allow_perf || type != log::perf))
        return ref<logmsg>();

    ref<logmsg> msg = ref<logmsg>(policy_msg::create());
    msg->set_type(type);
    msg->set_hash(hash);
    msg->set_logger(this);

    if (type == log::perf)
        msg->set_time(nsec_timer::current_time_ns());

    if (append_time) {
        int64 ms = nsec_timer::day_time_ns() / 1000000;
        charstr& str = msg->str();
        str.append_time_formatted(ms, true, 3);
        str.append(' ');
    }

    return msg;
}

////////////////////////////////////////////////////////////////////////////////
void logger::enqueue(ref<logmsg>&& msg)
{
    {
        GUARDTHIS(_mutex);

        _filters.for_each([&](const log_filter& f) {
            if (msg->get_type() <= (log::type)f._log_level && (f._module.is_empty() || f._module.cmpeq(msg->get_hash())))
                f._filter_fun(msg);
        });

    }
    SINGLETON(log_writer).addmsg(std::forward<ref<logmsg>>(msg));
}

////////////////////////////////////////////////////////////////////////////////
void logger::post(const token& txt, const token& from, const void* inst)
{
    token msg = txt;
    log::type type = logmsg::consume_type(msg);

    ref<logmsg> msgr = interface_register::canlog(type, from, inst);
    if (msgr)
        msgr->str() << msg;
}

////////////////////////////////////////////////////////////////////////////////
void logger::open(const token& filename)
{
    if (!_logfile)
        _logfile = ref<logger_file>(new logger_file(_stdout));

    _logfile->open(filename, _stdout);
}

////////////////////////////////////////////////////////////////////////////////
void logger::flush()
{
    int maxloop = 3000 / 20;
    while (!SINGLETON(log_writer).is_empty() && maxloop-- > 0)
        sysMilliSecondSleep(20);
}

////////////////////////////////////////////////////////////////////////////////
log_writer::log_writer()
    : _thread()
    , _queue()
{
    //make sure the dependent singleton gets created
    policy_msg::pool_singleton();
    //policy_pooled<logmsg>::default_pool();

    _thread.create(thread_run_fn, this, 0, "log_writer");
}

////////////////////////////////////////////////////////////////////////////////
log_writer::~log_writer()
{
    terminate();
}

////////////////////////////////////////////////////////////////////////////////
void log_writer::terminate()
{
    _thread.cancel_and_wait(10000);
}

////////////////////////////////////////////////////////////////////////////////
void* log_writer::thread_run()
{
    while (!coid::thread::self_should_cancel()) {
        flush();

        coid::sysMilliSecondSleep(1);
    }

    flush();

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
void log_writer::flush()
{
    ref<logmsg> m;

    //int maxloop = 3000 / 20;

    while (_queue.pop(m)) {
        DASSERT(m->str());
        m->write();
        m.release();
    }
}
