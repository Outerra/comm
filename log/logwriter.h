
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

#ifndef __COMM_LOGWRITTER_H__
#define __COMM_LOGWRITTER_H__

#include "../sync/queue.h"
#include "logger.h"
//#include "../pthreadx.h"

COID_NAMESPACE_BEGIN

class log_writer
{
protected:
    coid::thread _thread;
    coid::queue<logmsg_ptr> _queue;

public:
    log_writer();

    ~log_writer();

    static void* thread_run_fn( void* p ) {
        return static_cast<log_writer*>(p)->thread_run();
    }

    void* thread_run();

    void addmsg(logmsg_ptr&& m) {
        _queue.push(std::forward<logmsg_ptr>(m));
    }

    void flush();

    void terminate();

    bool is_empty() const {
        return _queue.is_empty();
    }

    bool is_running() 
    {
        return _thread.exists();
    }
};

COID_NAMESPACE_END

#endif // __COMM_LOGWRITTER_H__
