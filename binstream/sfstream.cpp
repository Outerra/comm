/*
    Copyright (C) 2003  Jozef Moravcik

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "sfstream.h"

namespace coid {

void safos::commit()
{
    //bool al_right = false;

    if( (_sg_->need_commit(_sg_id)) ) return;

	if( exist_wf() )
	{
        if( exist_old() )
        {
                if( remove(_old_p) != 0 )
                {
                    _oldfname.get_nextname();
                    _old_p = _sg_->Get_Path(); _old_p << _oldfname;
                }
        }

        while( rename(_ori_p, _old_p) != 0 ){
                _oldfname.get_nextname();
                _old_p = _sg_->Get_Path(); _old_p << _oldfname;
        }

      /*
        if( remove(_ori_p) != 0 )
        {
            _wfname.get_nextname();
            _ori_p = _sg_->Get_Path(); _ori_p << _wfname;
        }
      */
	}

    while( rename(_tmp_p, _ori_p) != 0 ) {
        _wfname.get_nextname();
        _ori_p = _sg_->Get_Path(); _ori_p << _wfname;
    }

    remove(_tmp_p);

    _sg_->commit_file(_sg_id);

    _wfname.clear();
    _oldfname.clear();
    _tempfname.clear();
}

void safos::close()
{
    if (str) fclose(str);  str = 0;

    _sg_->close_file(_sg_id);

    //_wfname.clear();
    //_oldfname.clear();
    //_tempfname.clear();
}


opcd safos::open(const char* _s_, ulong flg)
{
    _wfname = _s_;

    if(!_sg_->open_file(*this, _sg_id))
    {
	    gettempname();
        getoldname();

        _tmp_p = _sg_->Get_Path(); _tmp_p << _tempfname;
        _old_p = _sg_->Get_Path(); _old_p << _oldfname;
        _ori_p = _sg_->Get_Path(); _ori_p << _wfname;

        str = fopen (_tmp_p, "wb");
        return 0;
    }
    else
        return ersUNREACHABLE;
}



} // namespace coid
