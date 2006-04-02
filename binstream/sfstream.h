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

#ifndef __COID_COMM_SFSTREAM__HEADER_FILE__
#define __COID_COMM_SFSTREAM__HEADER_FILE__

#include "../namespace.h"

#include <io.h>
#include <stdio.h>
#include <direct.h>
#include "../retcodes.h"
#include "binstream.h"
#include "binstreamf.h"
#include "txtparstream.h"
#include "../dynarray.h"

COID_NAMESPACE_BEGIN

class safe_group;

typedef class safe_group SgrP;

////////////////////////////////////////////////////////////////////////////////
// file name manager ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class safname
{
	charstr sf_name;
	charstr sf_exten;
	charstr work_name;
	ulong   attrib;
	int     lastfcnt;

	enum {
		LSF_have_exten = 0x00000001,
	};

public:

    safname() {
        lastfcnt = 0;
        attrib = 0;
    }

    void clear(){
        sf_name = "";
        sf_exten = "";
        work_name = "";
        lastfcnt = 0;
        attrib = 0;
    }

	int init(const char* _s_, const char* _ex_ = 0)
	{
		sf_name  = _s_;
		if(_ex_){
			sf_exten = _ex_;
			attrib |= LSF_have_exten;
		}

        create_work_name();
		get_lastename();

        return 0;
	}

	bool sf_exist() { return !_access(work_name, 0); }
	bool identify(const char* _s_) { return (sf_name == _s_); }

	charstr& get_name() { return work_name; }

    void create_work_name()
    {
		work_name = "#+"; work_name << lastfcnt << "+#_" << sf_name;
		if(attrib & LSF_have_exten) work_name << ".#"<< sf_exten << '#';
    }

	charstr& get_nextname()
	{
		++lastfcnt;
		work_name = "#+"; work_name << lastfcnt << "+#_" << sf_name;
		if(attrib & LSF_have_exten) work_name << ".#"<< sf_exten << '#';

		return work_name;
	}

	charstr& get_prevname()
	{
		if(!lastfcnt) return work_name;
        --lastfcnt;
		work_name = "#+"; work_name << lastfcnt << "+#_" << sf_name;
		if(attrib & LSF_have_exten) work_name << ".#"<< sf_exten << '#';

		return work_name;
	}

	charstr& get_lastename()
	{
		while( sf_exist() )	{ get_nextname(); }
		get_prevname();

		return work_name;
	}

    const char* Get_O_name(){
        return (const char *)sf_name;
    }

    operator const char *() {
        return (const char*)work_name;
    }

    safname& operator = (const char * __s_) {
       init(__s_);
       return *this;
    }
};

////////////////////////////////////////////////////////////////////////////////
// safe input stream
////////////////////////////////////////////////////////////////////////////////
class safis : public binstream
{
    safname   _rfname;
    //safe_group* _sg_;       //pointer na gruppu do ktorej patri
    SgrP* _sg_;
    FILE* str;

public:

    virtual uint binstream_attributes( bool in0out1 ) const
    {
        return in0out1 ? fATTR_NO_OUTPUT_FUNCTION : 0;
    }

    safis()
    {
        str = 0;
    }

    safis(safe_group* _tosg_, const char* _name_)
    {
        _sg_ = _tosg_;
        _rfname = _name_;
    }

    virtual opcd read_raw( void* p, uint& len )
    {
        len -= fread (p, 1, len, str);
        if( len > 0 )
            return ersNO_MORE;
        return 0;
    }

    virtual opcd write_raw( const void* p, uint& len )   {
        return ersUNAVAILABLE;
    }

    virtual bool is_open () const       { return str != 0; }
    virtual void flush ()               { }
    virtual void acknowledge (bool eat=false)         { }

    void reset() { }

    void close () {
        if(str) fclose (str);
        str = 0;
    }

    void open (const char* _s_, ulong flg=0)
    {
        str = fopen (_s_, "rb");
    }

    ~safis() {
        close();
    }

};

////////////////////////////////////////////////////////////////////////////////
// safe output stream
////////////////////////////////////////////////////////////////////////////////
class safos : public binstream
{
	safname   _wfname;      //output file name
	safname   _tempfname;   //temporary file name
	safname   _oldfname;    //old file name
    ulong     _sg_id;      //identifikator streamu v grupe

    charstr _tmp_p;
    charstr _old_p;
    charstr _ori_p;

    SgrP*  _sg_;
    //safe_group* _sg_;   //pointer na gruppu do ktorej patri
    FILE* str;

	int exist_wf()  { return !_access(_ori_p, 0); }
	int exist_old() { return !_access(_old_p, 0); }

public:

    virtual uint binstream_attributes( bool in0out1 ) const
    {
        return in0out1 ? 0 : fATTR_NO_INPUT_FUNCTION;
    }

    safos(safe_group* _tosg_){
        _sg_ = _tosg_;
    }

    safos(safe_group* _tosg_, const char* _name_){
        _sg_  = _tosg_;
        open(_name_);
    }

    ~safos(){
        close();
    }

    virtual opcd write_raw( const void* p, uint& len )   {
        len -= fwrite (p, 1, len, str);
        return 0;
    }

    virtual opcd read_raw( void* p, uint& len )
    {
        return ersUNAVAILABLE;
    }

    virtual bool is_open () const       { return str != 0; }
    virtual void flush ()               { fflush (str); }
    virtual void acknowledge (bool eat=false)         { }

    void reset() { }

    const char* Get_O_name() {
        return _wfname.Get_O_name();
    }

	void getoldname() {
		_oldfname.init(_wfname.Get_O_name(), "old");
	}

    void gettempname() {
		_tempfname.init(_wfname.Get_O_name(), "temp");
	}

    ///////////////////////////////////////////////////////////////////
	void commit();
    void close();
    opcd open(const char* _s_, ulong flg=0);

};

////////////////////////////////////////////////////////////////////////////////
// Klassa riadiaca prislusnu "safe" skupinu suborov
////////////////////////////////////////////////////////////////////////////////
class safe_group
{
	typedef struct manager{

        charstr _fe_name;
		int fopened;
		int fedited;
		safos* _fstr;

		manager():fopened(0), fedited(0) {}

        inline friend binstream& operator << (binstream &out, const manager &_msf){
        	out << _msf._fe_name << STRUCT_NL;
        	return out;
        }

        inline friend binstream& operator >> (binstream &in, manager &_msf){
        	in >> _msf._fe_name;
        	return in;
        }

	} SafeManagerFile;


    enum {
        atAUTO_ADDING    = 0x00000001,

        rtALREADY_OPENED  = 0x00000100,
        rtALREADY_THERE   = 0x00000101,
        rtALREADY_EDITED  = 0x00000102,
        rtNOT_IN_GROUP    = 0x00000103,
        rtFILE_NOT_EXIST  = 0x00000104,
        rtDIR_NOT_CREATED = 0x00000105,
    };

	safname managerf;
	charstr grp_name;
    charstr grp_path;
	ulong grp_fcount;
    ulong sg_attrib;
    dynarray<SafeManagerFile> grp_files;

	ulong Im_In(const char* _s_) {

		for(ulong __i=0; __i<grp_fcount; __i++)
			if(grp_files[__i]._fe_name == _s_) return __i;

		return (ulong)(-1);
	}

public:

    safe_group(const char* _gpn, const char* _path) : sg_attrib(atAUTO_ADDING)
	{
		grp_name  = _gpn;
        grp_path  = _path;
        init();
	}

	safe_group(const char* _gpn,  const char* _path, ulong _attr)
	{
        sg_attrib = _attr;
		grp_name  = _gpn;
        grp_path  = _path;
        init();
	}

    ~safe_group(){
        commit_group();
        deinit();
    }

    int init()
    {
        if(grp_path.len() > 0){
            if( grp_path[grp_path.len()-1] != '\\' ) grp_path << '\\';
        }

        grp_path << grp_name;
        if(_access(grp_path, 0))
        {
            if(_mkdir(grp_name)) return rtDIR_NOT_CREATED;
        }
        grp_path << '\\';

        charstr _opn_s;
        _opn_s << grp_path << "#_manager_#.$#$";

        if(!_access(_opn_s, 0))
        {
            bifstream _ifst(_opn_s);
            txtparstream minf(_ifst);
            minf >> grp_fcount;
            if(grp_fcount)
                minf >> grp_files;
        }
        else
        {
            bofstream _ofst(_opn_s);
            txtparstream minf(_ofst);
            grp_fcount = 0;
            minf << 0;
        }

        return 0;
    }

    int deinit()
    {
        charstr _opn_s;
        _opn_s << grp_path << "#_manager_#.$#$";

        bofstream _ofst(_opn_s);
        txtparstream minf(_ofst);
        minf << grp_fcount;
        minf << grp_files;

        return 0;
    }

    ///////////////////////////////////////////////////////////////////
    int add_file(safos& _adsf)
    {
        ulong __Id = Im_In(_adsf.Get_O_name());

        if( __Id != (ulong)(-1) )
        {
            return rtALREADY_THERE;
        }
        else
        {
            grp_files.add(1);
            grp_files[grp_fcount]._fe_name = _adsf.Get_O_name();
            ++grp_fcount;
        }

        return 0;
    }

    ///////////////////////////////////////////////////////////////////
    int add_file(const char* _fname)
    {
        ulong __Id = Im_In(_fname);

        if( __Id != (ulong)(-1) )
        {
            return rtALREADY_THERE;
        }
        else
        {
            grp_files.add(1);
            grp_files[grp_fcount]._fe_name = _fname;
            ++grp_fcount;
        }
        return 0;
    }

    ///////////////////////////////////////////////////////////////////
    int open_file(safos& _ofstr, ulong& _sf_ID)
    {
        ulong __Id = Im_In(_ofstr.Get_O_name() );
        if( __Id != (ulong)(-1) )
        {
            if(grp_files[__Id].fedited) return rtALREADY_EDITED;

            if(grp_files[__Id].fopened) return rtALREADY_OPENED;
            else
            {
                grp_files[__Id].fopened = 1;
                grp_files[__Id].fedited = 1;
                grp_files[__Id]._fstr = &_ofstr;
                _sf_ID = __Id;
                return 0;
            }
        }
        else
        {
            if(sg_attrib & atAUTO_ADDING)
            {
                add_file(_ofstr);
                grp_files[grp_fcount-1].fopened = 1;
                grp_files[grp_fcount-1].fedited = 1;
                grp_files[grp_fcount-1]._fstr = &_ofstr;
                _sf_ID = grp_fcount-1;
                return 0;
            }
            else
                return rtNOT_IN_GROUP;
        }
    }

    ///////////////////////////////////////////////////////////////////
    int need_commit(ulong _sf_ID)
    {
		if(_sf_ID >= grp_fcount) return rtFILE_NOT_EXIST;
        return (int)!(grp_files[_sf_ID].fedited && !(grp_files[_sf_ID].fopened));
    }

    ///////////////////////////////////////////////////////////////////
    int commit_file(ulong& _sf_ID)
    {
		if(_sf_ID >= grp_fcount) return rtFILE_NOT_EXIST;
		grp_files[_sf_ID].fedited = 0;
        _sf_ID = (ulong)(-1);
        return 0;
    }

    ///////////////////////////////////////////////////////////////////
	int close_file(/*safos& _cstrm,*/ ulong& _sf_ID)
	{
		if(_sf_ID >= grp_fcount) return rtFILE_NOT_EXIST;
		grp_files[_sf_ID].fopened = 0;
		return 0;
	}

    ///////////////////////////////////////////////////////////////////
	void close_group()
	{
		for(ulong __i=0; __i<grp_fcount; __i++){
			if(grp_files[__i].fopened==1){
				grp_files[__i]._fstr->close();
			}
		}
	}

    ///////////////////////////////////////////////////////////////////
	int commit_group()
	{
		close_group();

		for(ulong __i=0; __i<grp_fcount; __i++)
		{
			if(grp_files[__i].fedited==1)
            {
				grp_files[__i]._fstr->commit();
                grp_files[__i].fedited = 0;
			}
		}

        return 0;
	}

    const char* Get_Path() { return (const char*)grp_path; }

	void restore()
    {
        safname _old_r;
        safname _orig_r;
        safname _temp_r;

        for(ulong __i=0; __i<grp_fcount; __i++)
        {
        }
    }
};

COID_NAMESPACE_END

#endif //__COID_COMM_SFSTREAM__HEADER_FILE__
