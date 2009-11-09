 
#include <afx.h>
//#include <windows.h>
#include <atlbase.h>
#include <math.h>
#include <sstream>
#include <iomanip>

typedef struct tagDEBUGHELPER
{
	DWORD dwVersion;
	BOOL (WINAPI *ReadDebuggeeMemory)( struct tagDEBUGHELPER *pThis, DWORD dwAddr, DWORD nWant, VOID* pWhere, DWORD *nGot );
	// from here only when dwVersion >= 0x20000
	DWORDLONG (WINAPI *GetRealAddress)( struct tagDEBUGHELPER *pThis );
	BOOL (WINAPI *ReadDebuggeeMemoryEx)( struct tagDEBUGHELPER *pThis, DWORDLONG qwAddr, DWORD nWant, VOID* pWhere, DWORD *nGot );
	int (WINAPI *GetProcessorType)( struct tagDEBUGHELPER *pThis );
} DEBUGHELPER;

inline DWORDLONG GetAddress( DEBUGHELPER *pHelper, DWORD address )
{
	return (( pHelper->dwVersion < 0x20000) ? address: pHelper->GetRealAddress(pHelper));
}

inline bool ReadMem( DEBUGHELPER *pHelper, DWORDLONG dwlAddress, size_t size, void *mem )
{
	DWORD nGot;
	if( pHelper->dwVersion < 0x20000)
	{
		if( pHelper->ReadDebuggeeMemory( pHelper, (DWORD)dwlAddress, size, mem, &nGot) != S_OK )
			return false;
	}
	else
	{
		//this is a part for VS NET
		if (pHelper->ReadDebuggeeMemoryEx(pHelper, dwlAddress, size, mem, &nGot)!=S_OK)
			return false;
	}

	return nGot == size;
}
template< typename T>
inline bool ReadMem(DEBUGHELPER *pHelper, DWORDLONG dwlAddress, T *mem)
{
	return ReadMem(pHelper, dwlAddress, sizeof(T), (void *)mem);
}


//extern "C" {
HRESULT WINAPI DateTime_Evaluate( DWORD dwAddress, DEBUGHELPER *pHelper, int nBase, BOOL bUniStrings, char *pResult, size_t maxlen, DWORD reserved )
{
	DATE Time;
	DWORDLONG address = GetAddress( pHelper, dwAddress);

	if( !ReadMem( pHelper, address, &Time) )
		return E_FAIL;

	if(Time == 0.)
	{
		strcpy(pResult, "zero");
		return S_OK;
	}
	if(Time == ( 65535. * 2147483647. ) || Time == (2147483647.))
	{
		strcpy(pResult, "invalid");
		return S_OK;
	}
	if(Time == ( 65535. * 2147483648. ) || Time == (2147483648.))
	{
		strcpy(pResult, "null");
		return S_OK;
	}

	CComBSTR strDate ;
	HRESULT hr = VarBstrFromDate(Time, LOCALE_USER_DEFAULT, 0, &strDate);
	if (FAILED(hr)) return hr;
	
	if(strDate != 0)
		WideCharToMultiByte(CP_ACP, 0, strDate, strDate.Length(), pResult, maxlen , NULL, NULL);

	return S_OK;
	
}

HRESULT WINAPI Period_Evaluate( DWORD dwAddress, DEBUGHELPER *pHelper, int nBase, BOOL bUniStrings, char *pResult, size_t maxlen, DWORD reserved )
{
	double pd;
	DWORDLONG address = GetAddress( pHelper, dwAddress);

	if( !ReadMem( pHelper, address, &pd) )
		return E_FAIL;

	if(pd == 0.)
	{
		strcpy(pResult, "0");
		return S_OK;
	}
	bool neg = pd < 0;
	if(neg) pd = -pd;

	long days,hours, minutes,seconds;
	double int_part;
	double rest = (modf(pd,&int_part)*24);
	days=(long)(int_part);
	rest = (modf(rest,&int_part)*60 );
	hours = (long)int_part;
	rest = (modf(rest,&int_part)*60);
	minutes = (long)int_part;
	seconds = (long)rest;
	std::stringstream str;
	if(neg) str << "-";
	if( days >0)
		str << days << "d";
	if(hours >0)
		str << hours << "h";
	if(minutes >0)
		str << minutes << "m";
	if(seconds >0)
		str << seconds << "s";
	strncpy( pResult,  str.str().c_str(), maxlen);
	pResult[maxlen-1]=0;
	return S_OK;
}

bool date_as_string(DATE Time, std::string *ret)
{
	if(Time == 0.)
	{
		*ret= "null_time";
		true;
	}
	CComBSTR strDate ;
	if(FAILED( VarBstrFromDate(Time, LOCALE_USER_DEFAULT, 0, &strDate) )) return false;

	char buf[50];
	buf[0]='0';
	if(strDate != 0)
		WideCharToMultiByte(CP_ACP, 0, strDate, strDate.Length(), buf, 50 , NULL, NULL);
	buf[49]='\0';

	*ret = buf;
	return true;
}

std::string get_desc_of( const IID &iid)
{
	std::string desc; 
	LPOLESTR iid_str=NULL;
	try
	{
		USES_CONVERSION;
		StringFromCLSID(iid, &iid_str);

		desc.assign(OLE2CT(iid_str));

	}
	catch(...)
	{
	}
	CoTaskMemFree(iid_str);

	HKEY hKey = NULL;

	std::string key(_T("INTERFACE\\") );
	key+=desc;

	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, key.c_str(), 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
	{
		TCHAR buf[30];
		DWORD size=30;
		buf[0]=(TCHAR)0;
		if( RegQueryValueEx(hKey, buf, NULL,NULL, (LPBYTE)buf, &size) == ERROR_SUCCESS )
		{
			desc = buf;
#if 0
			desc.insert(0,_T("["));
			desc.insert(0,buf);
			desc.append(_T("]"));
#endif  //0

		}
		RegCloseKey(hKey);
	}
	return desc;
}

std::string desc_from_vartype( VARTYPE vt, const std::string &extra )
{
	std::ostringstream os;
	bool need_close=false;
	if ( vt & VT_VECTOR) {
		os << "VECTOR(";
		need_close = true;
	}
	if (vt & VT_ARRAY) {
		os << "SAFEARRAY(";
		need_close = true;
	}
	if (vt & VT_RESERVED)
	{
		return "Reserved";
	}
	
	switch (vt & VT_TYPEMASK ) {
    case VT_I2: os << "short"; break;
    case VT_I4: os << "long"; break;
    case VT_I8: os << "__int64"; break;
    case VT_R4: os << "float"; break;
    case VT_R8: os << "double"; break;
    case VT_BSTR: os << "BSTR"; break;
    case VT_UNKNOWN:
    case VT_DISPATCH:
		  if(!extra.empty())
			  os << extra;
		  else os << ((vt == VT_UNKNOWN )?"IUnknown": "IDispatch");
		  os << " *";
		  break;
    case VT_BOOL: os << "VARIANT_BOOL"; break;
    case VT_VARIANT: os << "VARIANT"; break;
	case VT_DATE: os << "DATE"; break;
	case VT_CY: os << "CURRENCY"; break;
    case VT_I1: os << "char"; break;
    case VT_UI1: os << "uchar"; break;
    case VT_UI2: os << "unsigned short"; break;
    case VT_UI4: os << "unsigned long"; break;
    case VT_UI8: os << "unsigned __int64"; break;
    case VT_INT: os << "int"; break;
    case VT_UINT: os << "unsigned int"; break;
    case VT_VOID: os << "void"; break;
    case VT_HRESULT: os << "HRESULT"; break;
    case VT_LPSTR: os << "char*"; break;
    case VT_LPWSTR: os << "wchar_t*"; break;
    case VT_EMPTY: os << "<empty>"; break;
    case VT_NULL: os << "<null>"; break;
    case VT_ERROR: os << "HRESULT"; break;
    case VT_PTR: os << "VT_PTR"; break;
    case VT_FILETIME: os << "VT_FILETIME"; break;
    case VT_DECIMAL: os << "VT_DECIMAL"; break;
    case VT_CLSID: os << "VT_CLSID"; break;
	default: os << "???";
	}
	if (vt & VT_BYREF) 
		os << "*";
	if(need_close)
		os << ')';
	return os.str();
}
long vartype_len(VARTYPE vt)
{
	if ( vt & VT_VECTOR) return 0;
	if (vt & VT_RESERVED) return 0;
	if (vt & VT_ARRAY) return sizeof(SAFEARRAY);
	if (vt & VT_BYREF) return sizeof(void *);
	
	switch (vt & VT_TYPEMASK ) {
    case VT_I2: return sizeof(short);
    case VT_I4: return sizeof(long);
    case VT_I8: return sizeof(__int64);
    case VT_R4: return sizeof(float);
    case VT_R8: return sizeof(double);
    case VT_BSTR: return sizeof(BSTR);
    case VT_UNKNOWN:
    case VT_DISPATCH: return sizeof(void *);
    case VT_BOOL: return sizeof(VARIANT_BOOL);
	case VT_DATE: return sizeof(DATE);
	case VT_CY: return sizeof(CURRENCY);
    case VT_I1: return sizeof(char);
    case VT_UI1: return sizeof(unsigned char);
    case VT_UI2: return sizeof(unsigned short);
    case VT_UI4: return sizeof(unsigned long);
    case VT_UI8: return sizeof(unsigned __int64);
    case VT_INT: return sizeof(int);
    case VT_UINT: return sizeof(unsigned int);
    case VT_HRESULT: return sizeof(HRESULT);
    case VT_LPSTR: return sizeof(char*);
    case VT_LPWSTR: return sizeof(wchar_t*);
    case VT_ERROR: return sizeof(HRESULT);
	case VT_VARIANT: return sizeof(VARIANT);
	default:
		   return 0;
	}
}

bool safearray_as_string(DEBUGHELPER *pH,  DWORDLONG llAddr, std::string *ret  );

template<typename IT>
bool string_as_string( IT begin, IT end, std::string *ret)
{
	std::ostringstream os;
	for ( IT it = begin; it != end; ++it)
	{
		wchar_t ch = *it;
		if (ch > 255)
			os << "\\x" << std::hex <<std::setw(4) << std::setfill('0') << (long)(ch);
		else
			switch ( static_cast<char>(ch&0xff))
			{
				case '\t': os << "\\t"; break;
				case '\n': os << "\\n\n"; break;
				case '\r': os << "\\r"; break;
				case '\0': os << "\\0"; break;
				case '\\': os << "\\\\"; break;
				case '\a': os << "\\a"; break;
				case '\b': os << "\\b"; break;
				case '\f': os << "\\f"; break;
				case '\v': os << "\\v"; break;
				case '\"': os << "\\\""; break;
				default:
				{
					char cch = (ch&0xff);
					if (ch <= 27)
						os << "\\x" << std::hex << std::setfill('0') << std::setw(2) << (long)(cch );
					else
						os << cch;
				}
			}
	}
	*ret = os.str();
	return true;
}

bool
bstr_as_string(DEBUGHELPER *pH,  DWORDLONG addr, std::string *ret )
{
	if( addr == 0)
	{
		*ret = "@\"\"";
	}
	else
	{
		long len;
		if(!ReadMem(pH, addr - sizeof(len), &len) ) return false;
		std::ostringstream os;
		os << '\"';
		if(len >0 )
		{
			char *val = new char[len+2];
			if(!ReadMem(pH, addr, len, val) ) return false;

			// Null terminate
			val[len]='\0'; val[len+1]='\0';
			//
			wchar_t *wval = reinterpret_cast<wchar_t *>(val);
			std::string sret;
			if (!string_as_string( wval, wval+(len/2), &sret ) ) return false;

			delete [] val;
			os << sret;
		}
		os << '\"';
		*ret = os.str();
	}
	return true;
}

bool
variant_as_string( DEBUGHELPER *pH, const VARIANT &var, std::string *ret  )
{
	if (var.vt & VT_VECTOR) return false;
	if (var.vt & VT_RESERVED) return false;
	if (var.vt & VT_ARRAY)
		return safearray_as_string( pH, reinterpret_cast<DWORDLONG>(var.parray), ret);

	if (var.vt & VT_BYREF)
	{
		// Construct a fake variant with the byref-value in it.
		VARTYPE vt = var.vt & ~VT_BYREF;
		long size = vartype_len(vt);
		VARIANT var2;
		var2.vt = vt;
		long source=reinterpret_cast<long>(var.byref);
		void *dest;
		if( vt == VT_VARIANT)
			dest = &var2;
		else
		{
			var2.vt=vt;
			dest = &(var2.bVal);
		}
		if(!ReadMem(pH,source, size, dest)) return false;

		std::string retval;
		if( ! variant_as_string( pH, var2, &retval)) return false;

		retval += "]";
		*ret = "[" + retval;
		return true;
	}
	
	std::ostringstream os;
	switch (var.vt & VT_TYPEMASK )
	{
		case VT_I2: os << V_I2(&var); break;
		case VT_I4: os << V_I4(&var); break;
		//case VT_I8: os << V_I8(&var); break;
		case VT_R4: os << V_R4(&var); break;
		case VT_R8: os << V_R8(&var); break;

		case VT_UNKNOWN:
		case VT_DISPATCH: os <<  "0x" << std::hex << (long )V_DISPATCH(&var); break;
		case VT_BOOL: os << (V_BOOL(&var)==VARIANT_FALSE?"False":"True"); break;
		case VT_I1: os << "'" << V_I1(&var) << "'"  ; break;
		case VT_UI1: os << "'" << V_UI1(&var) << "'" ; break;
		case VT_UI2: os << V_UI2(&var); break;
		case VT_UI4: os << V_UI4(&var); break;
		case VT_INT: os << V_INT(&var); break;
		case VT_UINT: os << V_UINT(&var); break;
		case VT_ERROR: os << "error"; break;

		case VT_CY: os << (((double)(V_CY(&var).int64))/10000.); break;
		case VT_DATE: return date_as_string(V_DATE(&var), ret); break;

		case VT_BSTR:
		{

			long pBSTR = reinterpret_cast<long>( V_BSTR(&var) );
//			if (!ReadMem(pH, reinterpret_cast<DWORDLONG>( V_BSTR(&var) ), &pBSTR)) return false;

			std::string ret;
			if (!bstr_as_string( pH, pBSTR , &ret )) return false;
			os << ret;
		}break;
		case VT_EMPTY: os << '@'; break;
		case VT_NULL: os << "null"; break;
		break;
		default:
			return false;
	}

	*ret =  os.str();
	return true;
	
}

std::string
or_append(const std::string &str,const std::string &str2, bool &orred)
{
	if(str2.empty())
		return str;
	if(!str.empty())
	{
		orred = true;
		return str + " | " + str2;
	}
	return str2;
}
std::string
cond_bracket(const std::string &str, bool bracket)
{
	if(!bracket) return str;
	return "(" + str + ")";
}
std::string vt_as_string(VARTYPE vt) 
{
	std::string s = "";
	std::string byref;
	if (vt & VT_BYREF) byref = "VT_BYREF";
	bool orred=false;

	if (vt & VT_VECTOR) s = "VT_VECTOR";
	if (vt & VT_ARRAY) s =or_append(s,"VT_ARRAY",orred);
	if (vt & VT_RESERVED) s =or_append(s,"VT_RESERVED",orred);

#define VT2STRING(x) case x: s = or_append(s , #x,orred); break
	
	switch (vt & ~(VT_VECTOR|VT_ARRAY|VT_BYREF) ) {
    VT2STRING(VT_I2); VT2STRING(VT_I4); VT2STRING(VT_R4); VT2STRING(VT_R8); VT2STRING(VT_BSTR);
    VT2STRING(VT_DISPATCH); VT2STRING(VT_BOOL); VT2STRING(VT_VARIANT); VT2STRING(VT_UNKNOWN); VT2STRING(VT_I1);
    VT2STRING(VT_UI1); VT2STRING(VT_UI2); VT2STRING(VT_UI4); VT2STRING(VT_INT); VT2STRING(VT_UINT);
    VT2STRING(VT_VOID); VT2STRING(VT_HRESULT); VT2STRING(VT_LPSTR); VT2STRING(VT_LPWSTR); VT2STRING(VT_EMPTY);
    VT2STRING(VT_CY); VT2STRING(VT_ERROR); VT2STRING(VT_DATE); VT2STRING(VT_I8); VT2STRING(VT_UI8);
    VT2STRING(VT_PTR); VT2STRING(VT_SAFEARRAY); VT2STRING(VT_CARRAY); VT2STRING(VT_USERDEFINED); VT2STRING(VT_RECORD);
    VT2STRING(VT_FILETIME); VT2STRING(VT_BLOB); VT2STRING(VT_DECIMAL); VT2STRING(VT_STREAM); VT2STRING(VT_STORAGE);
    VT2STRING(VT_STREAMED_OBJECT); VT2STRING(VT_STORED_OBJECT); VT2STRING(VT_BLOB_OBJECT); VT2STRING(VT_CF); VT2STRING(VT_CLSID);
    VT2STRING(VT_NULL);
	default:
		return ("?VARTYPE?");
	}
	s = or_append(s , byref,orred);
	return cond_bracket(s,orred);
}

bool
safearray_as_string(DEBUGHELPER *pH,  DWORDLONG llAddr, std::string *ret  )
{
	SAFEARRAY sa;

	if(!ReadMem(pH, llAddr, &sa) ) return false;

	std::ostringstream os;

	VARTYPE vt = VT_UNKNOWN;
	std::string iid_desc;

	if(sa.fFeatures & FADF_AUTO) os << "auto ";
	if(sa.fFeatures & FADF_STATIC) os << "static ";
	if(sa.fFeatures & FADF_EMBEDDED) os << "embed ";
	if(sa.fFeatures & FADF_FIXEDSIZE) os << "const ";
	if(sa.fFeatures & FADF_BSTR) vt = VT_BSTR;
	if(sa.fFeatures & FADF_UNKNOWN) vt = VT_UNKNOWN;
	if(sa.fFeatures & FADF_DISPATCH) vt = VT_DISPATCH;
	if(sa.fFeatures & FADF_VARIANT) vt = VT_VARIANT;
	if(sa.fFeatures & FADF_RECORD) vt = VT_RECORD;


	if(sa.fFeatures & FADF_HAVEIID)
	{
		IID iid;
		if(!ReadMem(pH, llAddr-16, &iid) ) return false;

		iid_desc = get_desc_of(iid);
	}
	if(sa.fFeatures & FADF_HAVEVARTYPE)
	{
		if(!ReadMem(pH, llAddr-4, &vt) ) return false;
	}


	os << desc_from_vartype( vt, iid_desc);

	SAFEARRAYBOUND *rgsabound = new SAFEARRAYBOUND[ sa.cDims ];
	if(!ReadMem( pH, llAddr + offsetof(SAFEARRAY, rgsabound), sizeof(SAFEARRAYBOUND) * sa.cDims,  rgsabound ) )
	{
		delete [] rgsabound;
		return false;
	}

	for( USHORT d = 0 ; d < sa.cDims; ++d)
	{
		SAFEARRAYBOUND &bound=rgsabound[d];
		if( bound.lLbound > 0 ) os << '[' << bound.lLbound << ".." << (bound.lLbound + bound.cElements) << ']';
		else os << '[' << bound.cElements << ']';
	}
	delete [] rgsabound;
	long elements = sa.rgsabound[0].cElements;
	if( sa.pvData != 0 && sa.cDims == 1 && elements <= 20)
	{
		long len = vartype_len(vt);
		if(len > 0)
		{
			VARIANT var;
			void *dest;
			if( vt == VT_VARIANT)
			{
				dest = &var;
			}
			else
			{
				var.vt=vt;
				dest = &(var.bVal);
			}
			long source=reinterpret_cast<long>(sa.pvData);
			os << "={";
			for(long i=0; i< elements; ++i)
			{
				if( i>0) os << ',';
				if(!ReadMem( pH, source+(i*len), len ,dest ) ) return false;

				std::string varstr;
				if( ! variant_as_string( pH, var,&varstr ) ) return false;
				os << varstr;
			}
			os << "}";
		}
	}

	*ret = os.str();
	return true;
}

HRESULT WINAPI SAFEARRAY_Eval( DWORD addr, DEBUGHELPER *pH, int nBase, BOOL bUniStrings, char *pResult, size_t maxlen, DWORD reserved )
{
	DWORDLONG llAddr =GetAddress(pH,addr); 

	std::string res;

	if( !safearray_as_string(pH, llAddr, &res )) return E_FAIL; 

	strncpy(pResult,res.c_str(),  maxlen);
	pResult[maxlen-1] ='\0';
	return S_OK;
}


HRESULT WINAPI VARIANT_Eval( DWORD addr, DEBUGHELPER *pH, int nBase, BOOL bUniStrings, char *pResult, size_t maxlen, DWORD reserved )
{
	VARIANT var;
	DWORDLONG llAddr =GetAddress(pH,addr);
	if(!ReadMem(pH, llAddr, &var) ) return E_FAIL;

	std::string res;
	if(! variant_as_string(pH, var, &res ) ) return E_FAIL;

	std::ostringstream os;
	if(( var.vt & VT_ARRAY ) ==0) // arrays have their own type display.
		os << vt_as_string(var.vt) << ' ';
	os << res;

	strncpy(pResult, os.str().c_str(),  maxlen);
	pResult[maxlen-1] ='\0';
	return S_OK;

}
HRESULT WINAPI DISPPARAMS_Eval( DWORD addr, DEBUGHELPER *pH, int nBase, BOOL bUniStrings, char *pResult, size_t maxlen, DWORD reserved )
{
	DISPPARAMS dparam;
	DWORDLONG llAddr =GetAddress(pH,addr); 
	if(!ReadMem(pH, llAddr, &dparam) ) return E_FAIL;


	std::ostringstream os;
	if( dparam.cArgs == 0 )
	{
		os << "()";
	}
	else if(  dparam.cArgs > 20)
	{
		os << "(.." << dparam.cArgs << "..)";
	}
	else
	{
		VARIANT *vars= new VARIANT[dparam.cArgs];

		try
		{
			if(!ReadMem(pH, reinterpret_cast<DWORDLONG>( dparam.rgvarg ) , sizeof(VARIANT) * dparam.cArgs, vars ) )
			{
				delete [] vars;
				return E_FAIL;
			}
			bool first = true;
			os << '(';
			for( long i = dparam.cArgs-1; i>= 0 ; --i)
			{
				std::string ret;
				if(! variant_as_string(pH, vars[i], &ret) )
				{
					delete [] vars;  return false;
				}

				if(!first) os << ',' ; first = false;
				os << ret;

			}
			os << ')';
		} catch (...){}

		delete [] vars;
	}

	strncpy(pResult, os.str().c_str(),  maxlen);
	pResult[maxlen-1] ='\0';
	return S_OK;
}

HRESULT WINAPI PSAFEARRAY_Eval( DWORD addr, DEBUGHELPER *pH, int nBase, BOOL bUniStrings, char *pResult, size_t maxlen, DWORD reserved )
{
	DWORDLONG llAddr =GetAddress(pH,addr); 
	DWORDLONG pSA;

	if(!ReadMem(pH, llAddr, &pSA) ) return E_FAIL;

	std::string res;

	if( !safearray_as_string(pH, pSA, &res )) return E_FAIL;

	strncpy(pResult,res.c_str(),  maxlen);
	pResult[maxlen-1] ='\0';
	return S_OK;
}

HRESULT WINAPI BSTR_Eval( DWORD addr, DEBUGHELPER *pH, int nBase, BOOL bUniStrings, char *pResult, size_t maxlen, DWORD reserved )
{
	DWORDLONG llAddr =GetAddress(pH,addr); 

	long pBSTR;
	if (!ReadMem(pH, llAddr, &pBSTR)) return E_FAIL;

	std::string res;
	if (!bstr_as_string(pH, pBSTR, &res)) return E_FAIL;

	strncpy(pResult,res.c_str(),  maxlen);
	pResult[maxlen-1] ='\0';
	return S_OK;
}

HRESULT WINAPI OLEDateTime_Eval( DWORD dwAddress, DEBUGHELPER *pHelper, int nBase, BOOL bUniStrings, char *pResult, size_t maxlen, DWORD reserved )
{
	DATE Time;
	DWORDLONG address = GetAddress( pHelper, dwAddress);
	long status;
	if( !ReadMem( pHelper, address+sizeof(DATE), &status) )
		return E_FAIL;

	switch (status)
	{
		case 1:
			strcpy(pResult, "invalid");
			return S_OK;
		case 2:
			strcpy(pResult, "null");
			return S_OK;
	}

	if( !ReadMem( pHelper, address, &Time) )
		return E_FAIL;

	if(Time == 0.)
	{
		strcpy(pResult, "zero");
		return S_OK;
	}

	CComBSTR strDate ;
	HRESULT hr = VarBstrFromDate(Time, LOCALE_USER_DEFAULT, 0, &strDate);
	if (FAILED(hr)) return hr;
	
	if(strDate != 0)
		WideCharToMultiByte(CP_ACP, 0, strDate, strDate.Length(), pResult, maxlen , NULL, NULL);

	return S_OK;
	
}
HRESULT WINAPI OLEDateTimeSpan_Eval( DWORD dwAddress, DEBUGHELPER *pHelper, int nBase, BOOL bUniStrings, char *pResult, size_t maxlen, DWORD reserved )
{
	DWORDLONG address = GetAddress( pHelper, dwAddress);
	long status;
	if( !ReadMem( pHelper, address+sizeof(long), &status) )
		return E_FAIL;

	switch (status)
	{
		case 1:
			strcpy(pResult, "invalid");
			return S_OK;
		case 2:
			strcpy(pResult, "null");
			return S_OK;
	}
	// Same as Period.
	return Period_Evaluate(dwAddress, pHelper, nBase, bUniStrings, pResult, maxlen, reserved);
}

HRESULT WINAPI wstring_Eval( DWORD dwAddress, DEBUGHELPER *pHelper, int nBase, BOOL bUniStrings, char *pResult, size_t maxlen, DWORD reserved )
{
	DWORDLONG address = GetAddress( pHelper, dwAddress);

	long begin,end;
	if (!ReadMem( pHelper, address, &begin) || !ReadMem( pHelper, address + sizeof(long), &end) ) return E_FAIL;

	long size  =(end-begin);
	if ( begin == 0 )
	{
		strcpy(pResult, "@");
		return S_OK;
	}
	if( size == 0)
	{
		strcpy(pResult, "\"\"");
		return S_OK;
	}

	unsigned char *eval = new unsigned char[size+2];
	if (!ReadMem(pHelper, begin, size, eval) )
	{
		delete [] eval;
		return E_FAIL;
	}
	wchar_t *chbegin = reinterpret_cast<wchar_t *>(eval);
	wchar_t *chend = reinterpret_cast<wchar_t *>(eval + size);

	std::string res;
	if (!string_as_string( chbegin, chend, &res) )
	{
		delete [] eval;
		return E_FAIL;
	}
	delete [] eval;
	res.insert(res.begin(), '\"');
	res += '\"';
	strncpy(pResult,res.c_str(),  maxlen);
	pResult[maxlen-1] ='\0';

	return S_OK;
}
HRESULT WINAPI string_Eval( DWORD dwAddress, DEBUGHELPER *pHelper, int nBase, BOOL bUniStrings, char *pResult, size_t maxlen, DWORD reserved )
{
	DWORDLONG address = GetAddress( pHelper, dwAddress);

	long begin,end;
	if (!ReadMem( pHelper, address, &begin) || !ReadMem( pHelper, address + sizeof(long), &end) ) return E_FAIL;

	long size  =(end-begin);
	if ( begin == 0 )
	{
		strcpy(pResult, "@");
		return S_OK;
	}
	if( size == 0)
	{
		strcpy(pResult, "\"\"");
		return S_OK;
	}

	unsigned char *eval = new unsigned char[size+2];
	if (!ReadMem(pHelper, begin, size, eval) )
	{
		delete [] eval;
		return E_FAIL;
	}
	char *chbegin = reinterpret_cast<char *>(eval);
	char *chend = reinterpret_cast<char *>(eval + size);

	std::string res;
	if (!string_as_string( chbegin, chend, &res) )
	{
		delete [] eval;
		return E_FAIL;
	}
	delete [] eval;
	res.insert(res.begin(), '\"');
	res += '\"';
	strncpy(pResult,res.c_str(),  maxlen);
	pResult[maxlen-1] ='\0';

	return S_OK;
}

HRESULT WINAPI ms_wstring_Eval( DWORD dwAddress, DEBUGHELPER *pHelper, int nBase, BOOL bUniStrings, char *pResult, size_t maxlen, DWORD reserved )
{
	DWORDLONG address = GetAddress( pHelper, dwAddress);

	long begin,size;
	if (!ReadMem( pHelper, address, &begin) || !ReadMem( pHelper, address + sizeof(long), &size) ) return E_FAIL;

	if ( begin == 0 )
	{
		strcpy(pResult, "@");
		return S_OK;
	}
	if( size == 0)
	{
		strcpy(pResult, "\"\"");
		return S_OK;
	}

	unsigned char *eval = new unsigned char[size+2];
	if (!ReadMem(pHelper, begin, size, eval) )
	{
		delete [] eval;
		return E_FAIL;
	}
	wchar_t *chbegin = reinterpret_cast<wchar_t *>(eval);
	wchar_t *chend = reinterpret_cast<wchar_t *>(eval + size);

	std::string res;
	if (!string_as_string( chbegin, chend, &res) )
	{
		delete [] eval;
		return E_FAIL;
	}
	delete [] eval;
	res.insert(res.begin(), '\"');
	res += '\"';
	strncpy(pResult,res.c_str(),  maxlen);
	pResult[maxlen-1] ='\0';

	return S_OK;
}
HRESULT WINAPI ms_string_Eval( DWORD dwAddress, DEBUGHELPER *pHelper, int nBase, BOOL bUniStrings, char *pResult, size_t maxlen, DWORD reserved )
{
	DWORDLONG address = GetAddress( pHelper, dwAddress);

	long begin,size;
	if (!ReadMem( pHelper, address, &begin) || !ReadMem( pHelper, address + sizeof(long), &size) ) return E_FAIL;

	if ( begin == 0 )
	{
		strcpy(pResult, "@");
		return S_OK;
	}
	if( size == 0)
	{
		strcpy(pResult, "\"\"");
		return S_OK;
	}

	unsigned char *eval = new unsigned char[size+2];
	if (!ReadMem(pHelper, begin, size, eval) )
	{
		delete [] eval;
		return E_FAIL;
	}
	char *chbegin = reinterpret_cast<char *>(eval);
	char *chend = reinterpret_cast<char *>(eval + size);

	std::string res;
	if (!string_as_string( chbegin, chend, &res) )
	{
		delete [] eval;
		return E_FAIL;
	}
	delete [] eval;
	res.insert(res.begin(), '\"');
	res += '\"';
	strncpy(pResult,res.c_str(),  maxlen);
	pResult[maxlen-1] ='\0';

	return S_OK;
}


bool
date_from_absdate_(long daysAbsolute ,  int *tm_year, int *tm_mon, int *tm_mday)
{
	typedef long date_int_type;
	typedef int year_type;
	date_int_type dayNumber = daysAbsolute;
	date_int_type a = dayNumber + 32044 /*+ 1721060*/;
	date_int_type b = (4*a + 3)/146097;
	date_int_type c = a-((146097*b)/4);
	date_int_type d = (4*c + 3)/1461;
	date_int_type e = c - (1461*d)/4;
	date_int_type m = (5*e + 2)/153;
	*tm_mday = static_cast<unsigned short>(e - ((153*m + 2)/5) + 1);
	*tm_mon = static_cast<unsigned short>(m + 3 - 12 * (m/10));
	*tm_year = static_cast<unsigned short>(100*b + d - 4800 + (m/10));
	return true;
}

HRESULT WINAPI DateOnly_Evaluate( DWORD dwAddress, DEBUGHELPER *pHelper, int nBase, BOOL bUniStrings, char *pResult, size_t maxlen, DWORD reserved )
{
	long DateOnly;
	DWORDLONG address = GetAddress( pHelper, dwAddress);

	if( !ReadMem( pHelper, address, &DateOnly) )
		return E_FAIL;

	if(DateOnly == 0)
	{
		strcpy(pResult, "zero");
		return S_OK;
	}
	if( DateOnly == 2147483647)
	{
		strcpy(pResult, "invalid");
		return S_OK;
	}
	if( DateOnly == 2147483648)
	{
		strcpy(pResult, "null");
		return S_OK;
	}
	int tm_year,  tm_mon, tm_mday;
	if (!date_from_absdate_(DateOnly, &tm_year, &tm_mon, &tm_mday))
	{
		strcpy(pResult, "n/a");
		return S_OK;
	}
	int tm_wday = (int)((DateOnly + 1) % 7L);
	static const char *wday[]={"Su", "Mo","Tu","We","Th","Fr","Sa"};

	std::ostringstream os;
	os << tm_year << "." << tm_mon << "." << tm_mday << " " << wday[tm_wday];

	strncpy(pResult, os.str().c_str(),  maxlen);
	pResult[maxlen-1] ='\0';

	return S_OK;
	
}

