/*
 * Copyright © 2000-2004 Sofus Mortensen, Michael Geddes
 *
 * This material is provided "as is", with absolutely no warranty
 * expressed or implied. Any use is at your own risk. Permission to
 * use or copy this software for any purpose is hereby granted without
 * fee, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is
 * granted, provided the above notices are retained, and a notice that
 * the code was modified is included with the above copyright notice.
 */

#include "std.h"

#define COMET_COPYRIGHT	 "Copyright © 2000-2004 Sofus Mortensen, Michael Geddes"

#define for if(0);else for

#ifdef NDEBUG
#define DEBUG(n,str)
#else
#define DEBUG(n,str) if ( g_debug < n) ; else std::cout << str
#endif

using namespace TLI;

using std::string;
using std::vector;
#define COMET_BUILD_VERSION_ONLY
#include "../../include/comet/config.h"

#define COMET_STR__(x) #x
#define COMET_STR_(x) COMET_STR__(x)

std::set<string> required_tlbs;

#ifndef NDEBUG
long g_debug = 0;
#endif // NDEBUG

bool do_log_interfaces = false;

const char s_amp[] = "&";
const char s_htmlamp[] = "&amp;";

static inline const char *ampersand(bool html)
{
	return html?s_htmlamp:s_amp;
}

class tlb2h_error : public std::runtime_error
{
	public:
	tlb2h_error( const std::string &msg)
		:msg_(msg), std::runtime_error(msg )
	{
	}
	tlb2h_error( const tlb2h_error &err, const std::string &msg)
		:std::runtime_error(std::string(err.msg_) + " of " + msg)
		, msg_(err.msg_+ " of " + msg)
	{
	}
	tlb2h_error( const std::exception &err, const std::string &msg)
		:std::runtime_error(std::string(err.what()) + " in " + msg)
	{

		msg_= err.what();
		msg_ += " in ";
		msg_ += msg;
	}
	tlb2h_error (const _com_error &err, const std::string &msg)
		:std::runtime_error(std::string(err.Description())+ " in " + msg)
	{
		msg_ = err.Description();
		if( msg_.empty()) msg_ = err.ErrorMessage();
		msg_ += " in ";
		msg_ += msg;
	}
	const char *msg()
	{
		return msg_.c_str();
	}
	private:
		std::string msg_;

};
#define CATCH_ALL( msg ) \
	catch( tlb2h_error &err) {\
		std::stringstream emsg; emsg<< msg;  throw tlb2h_error( err, emsg.str()); } \
	catch( std::exception &err) { \
		std::stringstream emsg; emsg << msg; throw tlb2h_error( err, emsg.str()); } \
	catch( _com_error &err) { \
		std::stringstream emsg; emsg << msg; throw tlb2h_error( err, emsg.str()); } \
	catch( ... ) { \
		std::stringstream emsg; emsg << "Unknown error: " << msg; \
		throw tlb2h_error( emsg.str()); }


class TypeLibrary;

class NamespaceOptions
{
	public:
		NamespaceOptions() : ignore_namespace_(false), inside_namespace_(0){}
		void add_translate_namespace(const std::string &str1, const std::string &str2)
		{
			namespaces_[str1] = str2;
		}

		std::string trans_symbol( const std::string &str) const
		{
			NAMESPACEMAP::const_iterator it = namespaces_.find(str);
			if(it == namespaces_.end())
			{
				return str;
			}
			return (*it).second;
		}
		std::string trans_symbol( const _bstr_t &str) const
		{
			return trans_symbol(std::string(static_cast<const char *>(str)));
		}
		std::string trans_symbol( const char * str)const
		{
			return trans_symbol( std::string(str));
		}

		std::string trans_interface_method( const std::string &interfaceName, const _bstr_t &methodName) const
		{
			return trans_interface_method( interfaceName, std::string(static_cast<const char *>(methodName)));
		}

		std::string trans_interface_method( const std::string &interfaceName, const std::string &methodName) const
		{
			std::ostringstream ifm;
			ifm << interfaceName << "." << methodName;

			NAMESPACEMAP::const_iterator it = namespaces_.find(ifm.str());
			if(it == namespaces_.end())
			{
				it = namespaces_.find(methodName);
				if(it == namespaces_.end())
				{
					return methodName;
				}
			}
			return (*it).second;
		}

		std::string foreign_namespace(const std::string &str) const
		{
			if(str == lib_name_)
				return namespace_;

			NAMESPACEMAP::const_iterator it = namespaces_.find(str);
			if(it == namespaces_.end())
			{
				return str;
			}
			return (*it).second;
		}
		void translate_foreign_namespace(std::string &str) const
		{
			if(str == lib_name_)
				str = namespace_;

			NAMESPACEMAP::const_iterator it = namespaces_.find(str);
			if(it != namespaces_.end())
			{
				str = (*it).second;
			}
		}
		void set_library_name( const string &lib_name)
		{
			lib_name_ = "";
			namespace_ = foreign_namespace( lib_name);
			lib_name_ = lib_name;
		}

		void set_namespace_name( const string &ns_name)
		{
			if( !ns_name.empty())
				namespace_=  ns_name;
		}
		string namespace_name() const
		{
			return namespace_;
		}

		string get_ns( const TypeInfoPtr& ti) const
		{
			string ns = ti->Parent->Name;

			if(ns == lib_name_)
				ns = namespace_;
			translate_foreign_namespace(ns); // Apply any mappings.
			return ns;
		}

		string get_name_with_ns(const TypeInfoPtr& ti, bool force_use_of_ns = false, const char* prefix = 0) const
		{
			if (ti == 0) {
#ifndef NDEBUG
				return "<<ASSERTION FAILED>>";
#endif
				throw std::logic_error("Assertion failed");
			}
			string ns = get_ns(ti);

			string name = trans_symbol(ti->Name);

			if (prefix) name = prefix + name;

			if (!force_use_of_ns &&
					(ignore_namespace_ ||((inside_namespace_>0) && ns == namespace_)))
				return name;
			if (ns == "stdole") return "::" + name;
			if (ns != namespace_)
			{
				required_tlbs.insert(ns);
			}
			return ns + "::" + name;
		}

		void enter_namespace() { ++inside_namespace_;}
		void leave_namespace() { --inside_namespace_; }

		void ignore_namespace( bool ignore = true){ ignore_namespace_ = ignore; }

		static  void strip_whitespace( string &newval)
		{
			string::size_type pos = newval.find_last_not_of(" \t");
			newval.replace(pos+1,string::npos,"");

			pos = newval.find_first_not_of(" \t");
			if( pos != string::npos && pos != 0)
			{
				newval.replace(0, pos-1, "");
			}
		}
		void add_namespace_mapping( const std::string &lhs, const std::string &rhs)
		{
			namespaces_[lhs] = rhs;
		}
		std::string get_namespace_entry( const std::string &str)
		{
			NAMESPACEMAP::const_iterator it = namespaces_.find(str);
			if(it == namespaces_.end())
			{
				return std::string();
			}
			return (*it).second;
		}

		bool add_namespace_mapping( const std::string &newval)
		{
			string::size_type pos = newval.find_first_of('=');

			if( pos != string::npos)
			{
				string lhs(newval,0, pos);
				string rhs(newval,pos+1,string::npos);
				strip_whitespace(lhs);
				strip_whitespace(rhs);
				namespaces_[lhs] = rhs;
				DEBUG(1,'\''<< lhs << "' maps to '" << rhs << "'\n");
				return true;
			}
			else
			{
				std::cout << "Skipping :"<< newval << '\n';
				return false;
			}
		}

		void load_namespaces(const std::string &filename)
		{
			DEBUG(1, "Loading namespaces from :" << filename << '\n');
			std::ifstream in(filename.c_str());
			if(in)
			{
				while (!in.eof())
				{
					string newval;
					std::getline(in, newval);
					strip_whitespace(newval);
					if(newval.empty() ||  newval[0]==';' || newval[0]=='#')
						continue;

					add_namespace_mapping(newval);

				}
			}
		}

	private:
		bool ignore_namespace_;
		long inside_namespace_;
		typedef std::map<string,string> NAMESPACEMAP;
		NAMESPACEMAP namespaces_;

		string namespace_;
		string lib_name_;
};

NamespaceOptions nso;

string escape_backslash(const string& s)
{
	string rv;
	rv.reserve(s.size()*3/2); // Avoid re-allocating string
	for (string::const_iterator it = s.begin(); it != s.end(); ++it)
	{
		switch (*it)
		{
			case '\a':  rv += "\\a";   break;
			case '\b':  rv += "\\b";   break;
			case '\f':  rv += "\\f";   break;
			case '\n':  rv += "\\n";   break;
			case '\r':  rv += "\\r";   break;
			case '\t':  rv += "\\t";   break;
			case '\v':  rv += "\\v";   break;
			case '\?':  rv += "\\?";   break;
			case '\'':  rv += "\\'";   break;
			case '\"':  rv += "\\\"";  break;
			case '\\':  rv += "\\\\";  break;
			default:    rv += *it;
		}
	}
	return rv;
}

template<typename IT> void delete_all(IT begin, IT last)
{
	for (;begin != last; ++begin) delete *begin;
}

template<typename IT> void delete_all_2nd(IT begin, IT last)
{
	for (;begin != last; ++begin) delete begin->second;
}

string make_guid(const string& id)
{
	std::ostringstream os;
	os << "{ 0x" << id.substr(0, 8) << ", ";
	os << "0x" << id.substr(9, 4) << ", 0x" << id.substr(14, 4) << ", ";
	os << "{ 0x" << id.substr(19, 2) << ", 0x" << id.substr(21, 2);
	os << ", 0x" << id.substr(24, 2);
	os << ", 0x" << id.substr(26, 2);
	os << ", 0x" << id.substr(28, 2);
	os << ", 0x" << id.substr(30, 2);
	os << ", 0x" << id.substr(32, 2);
	os << ", 0x" << id.substr(34, 2);
	os << " } }";

	return os.str();
}

template <int Maxelt>
class output_typelist_t
{
	public:
	output_typelist_t( std::ostream &os): os_(os), count_(0)
	{
	}

	output_typelist_t &operator <<( const std::string &str)
	{
		++count_;
		if( count_ % Maxelt == 1)
		{
			if( !elements_.empty())
			{
				os_ << "\ntypelist::append< make_list<" << elements_ << ">::result, ";
				close_+=" > ";
			}
			elements_ = str;
		}
		else
		{
			elements_+= ", ";
			elements_+= str;
		}

		return *this;
	}
	bool end()
	{
		bool ret =!elements_.empty() ;
		if( ret || count_ == 0 )
		{
			os_ << " make_list<" << elements_ << ">::result ";
		}
		os_ << close_;
		count_ =0;
		elements_ = "";
		close_ = "";
		return ret;
	}
	private:

		int count_;
		std::string elements_;
		std::string close_;

		std::ostream& os_;
};
typedef output_typelist_t<10> output_typelist;


struct oneoff_output_to_stream
{
	public:
	oneoff_output_to_stream( std::ostream &par_stream, const std::string &str)
		:stream_(par_stream),str_(str),done_(false)
	{
	}
	std::ostream &operator <<( const std::string &str )
	{
		if( !str.empty())
		{
			if( !done_ )
			{
				done_=true;
				stream_ << str_ ;
			}
			stream_ << str;
		}
		return stream_;
	}
	bool did_output(){ return done_;}
	void close( const std::string &str )
	{
		if( done_ )
		{
			stream_ << str;
		}
	}
	void open()
	{
		done_ = false;
	}
	void open( const std::string &str)
	{
		done_=false;
		str_ = str;
	}
private:
	void do_output_open()
	{
	}
	std::ostream &stream_;
	std::string str_;
	bool done_;
};

struct oneoff_output
{
	oneoff_output( const std::string &str):str_(str),done_(false)
	{
	}

	std::ostream &operator()( std::ostream &strm)
	{
		if(!done_)
		{
			done_=true;
			strm << str_;
		}
		return strm;
	}
	bool did_output(){ return done_;}
private:
	std::string str_;
	bool done_;
};


struct require_interface
{
	virtual void operator()( const std::string &name)=0;
};

string vt2string(VARTYPE vt);


string vt2refmacro(VARTYPE vt);

int pointer_level(VarTypeInfoPtr vti)
{
	if (vti->VarType != TLI::VT_EMPTY) return vti->PointerLevel;
	TypeInfoPtr ti = vti->TypeInfo;

	switch (ti->GetTypeKind()) {
	case TLI::TKIND_INTERFACE:
	case TLI::TKIND_DISPATCH:
	case TLI::TKIND_COCLASS:
		return vti->PointerLevel - 1;
		break;
	case TLI::TKIND_ALIAS:
		return pointer_level( ti->ResolvedType );
	default:
		return vti->PointerLevel;
	}
}

class Type{
	VarTypeInfoPtr vti_;
	int pl_mod_;
public:
	Type() : pl_mod_(0) {}
	explicit Type(const VarTypeInfoPtr& vti,  int pl_mod = 0)
	{ vti_ = vti; pl_mod_ = pl_mod; }

	bool is_void_or_hresult() const
	{
		return vti_->PointerLevel == 0 && (vti_->VarType == TLI::VT_VOID || vti_->VarType == TLI::VT_HRESULT);
	}

/*	Type& operator=(const VarTypeInfoPtr& vti) {
		vti_ = vti;
		return *this;
	}*/

	string as_name_with_ns() const
	{
		TypeInfoPtr ti = vti_->TypeInfo;
		return nso.get_name_with_ns(ti);
	}
	string as_name() const
	{
		return nso.trans_symbol(vti_->TypeInfo->Name);
	}
	string as_raw(unsigned&, bool UseVariantType = false) const;
	string as_raw_retval(bool with_ns = false) const;

	string as_cpp_struct_type(unsigned&) const;

	string as_cpp_in(unsigned&, bool html = false) const;
	string as_cpp_out(bool html = false) const;
	string as_cpp_inout(bool html = false) const;
	string as_cpp_retval(bool html = false) const;

	string get_return_constructor(const string&, bool cast_itf) const;
	string get_impl_in_constructor(const string&, bool asVariantType = false, bool ignore_pointer_level = false ) const;
	string get_impl_inout_constructor(const string&, bool asVariantType = false) const;

	string default_constructor() const
	{
		VARTYPE vt = get_unaliased_vartype();//vti_->VarType;

		switch (vt) {
		case ::VT_BOOL:
			return "(false)";
		default:
			return "";
		};
	};

	string cast_variant_member_to_raw() const
	{
		VARTYPE vt = get_unaliased_vartype();//vti_->VarType;
		if (vt == TLI::VT_EMPTY)
		{
			TypeKinds tk = get_unaliased_typekind();//vti_->TypeInfo->TypeKind
			if (tk == TLI::TKIND_ENUM)
			{
				unsigned p = 0;;
				string r = "(" + as_raw(p) + ")";
				if (p != 0) throw std::runtime_error("cast_variant_member_to_raw failed");
				return r;
			}
		}
		return "";
	}

	string cast_raw_to_variant_member() const
	{
		VARTYPE vt = get_unaliased_vartype();//vti_->VarType;
		if (vt == TLI::VT_EMPTY)
		{
			TypeKinds tk = get_unaliased_typekind();//vti_->TypeInfo->TypeKind
			if (tk == TLI::TKIND_ENUM)
			{
				string r = "(long";
				for (int i=0; i<vti_->PointerLevel; ++i) r += "*";
				r += ")";
				return r;
			}
		}
		return "";
	}

	bool is_struct_or_union(string& name) const
	{
		if (get_unaliased_vartype() != TLI::VT_EMPTY) return false;
		TypeInfoPtr ti = vti_->TypeInfo;

		switch (get_unaliased_typekind())
		{
		case TLI::TKIND_RECORD:
		case TLI::TKIND_UNION:
			name = nso.get_name_with_ns(ti);
			return true;
			break;
		default:
			return false;
		}
	};

	string default_value(const _variant_t& var) const
	{
		VARTYPE vt = get_unaliased_vartype(); //vti_->VarType;

		TypeInfoPtr ti = vti_->TypeInfo;
		if (vt == ::VT_EMPTY && ti->TypeKind == TLI::TKIND_ALIAS)
		{
			vt = ti->ResolvedType->VarType;
		}

		switch (vt) {
		case ::VT_LPSTR:
			return string("\"") + escape_backslash(string(_bstr_t(var))) + string("\"");
		case ::VT_BSTR:
			return string("L\"") + escape_backslash(string(_bstr_t(var))) + string("\"");
		case ::VT_UNKNOWN:
			if (var.punkVal != 0) throw std::runtime_error("Cannot handle non-zero defaultvalue for interfaces");
			return "0";
		case ::VT_DISPATCH:
			if (var.pdispVal != 0) throw std::runtime_error("Cannot handle non-zero defaultvalue for interfaces");
			return "0";
		case ::VT_BOOL:
			return ((bool)var)?"true":"false";
		case ::VT_VARIANT:
		case ::VT_INT:
		case ::VT_I4:
		case ::VT_I1:
		case ::VT_I2:
		case ::VT_UI4:
		case ::VT_UI1:
		case ::VT_UI2:
		case ::VT_R4:
		case ::VT_R8:
		case ::VT_CY:
			return (const char*)_bstr_t(var);
		case ::VT_DATE:
		{
			std::ostringstream os;
			os << "datetime_t(" << std::ios_base::showpoint << (double)var.date << ')';
			return os.str();
		}

		case TLI::VT_EMPTY:
			{
				switch (get_unaliased_typekind()) //ti->GetTypeKind())
				{
				case TLI::TKIND_INTERFACE:
				case TLI::TKIND_DISPATCH:
					{
						short r;
						try {r = var;} catch (...) { r = 1; }
						if (r != 0) throw std::runtime_error("Cannot handle non-zero defaultvalue for interfaces");
						string s = "com_ptr<";
						s += nso.get_name_with_ns(ti);
						s += ">(0)";
						return s;
					}
				case TLI::TKIND_COCLASS:
					{
						short r;
						try {r = var;} catch (...) { r = 1; }
						if (r != 0) throw std::runtime_error("Cannot handle non-zero defaultvalue for interfaces");
						string s = "com_ptr<";
						s += nso.trans_symbol(ti->DefaultInterface->Name);
						s += ">(0)";
						return s;
					}
				case TLI::TKIND_ENUM:
					{
						string s = is_aliased_type()?"":"enum ";
						s += nso.get_name_with_ns(ti);
						s += "(" + bstr_t(var) + ")";
						return s;
					}
				case TLI::TKIND_RECORD:
					{
						int l = 1;
					}
				case TLI::TKIND_ALIAS:
					{
						return "<<UNKNOWN ALIAS>>";
					}
				default:
					int x = ti->GetTypeKind();
					throw std::runtime_error("cannot handle default type");
				}
				break;
			}
		default:
			throw std::runtime_error("cannot handle default type");
		}
	}

	string default_value() const
	{
		VARTYPE vt = get_unaliased_vartype(); //vti_->VarType;
		if (vt & ::VT_ARRAY)
		{
			unsigned z0;
			bool z1;
			return get_cpp(0, z0, z1) + "()";
		}
		switch (vt) {
		case ::VT_VARIANT:
			return "variant_t::missing()";
		case ::VT_BSTR:
			return "(BSTR)0";
		case ::VT_DATE:
			return "datetime_t::(0.)";
		case ::VT_CY:
		case ::VT_UNKNOWN:
		case ::VT_DISPATCH:
			return "0";
		case ::VT_BOOL:
			return "false";
		case ::VT_I4:
		case ::VT_I1:
		case ::VT_I2:
			return "0";
		case ::VT_R4:
			return "0.0f";
		case ::VT_R8:
			return "0.0";
		case TLI::VT_EMPTY:
			{
				TypeInfoPtr ti = vti_->TypeInfo;
				switch (get_unaliased_typekind()) //ti->GetTypeKind()) {
				{
				case TLI::TKIND_INTERFACE:
				case TLI::TKIND_DISPATCH:
					{
						string s = "com_ptr<";
						s += nso.get_name_with_ns(ti);
						s += ">(0)";
						return s;
					}
				case TLI::TKIND_COCLASS:
					{
						string s = "com_ptr<";
						s += nso.trans_symbol(ti->DefaultInterface->Name);
						s += ">(0)";
						return s;
					}
				case TLI::TKIND_ENUM:
					{
						string s = is_aliased_type()?"":"enum ";
						s += nso.trans_symbol(ti->GetName());
						s += "(0)";
						return s;
					}
				default:
					throw std::runtime_error("cannot handle default type");
				}
				break;
			}
		default:
			throw std::runtime_error("cannot handle default type");
		}
	}

	string get_in_converter(const string& name) const
	{
		VARTYPE vt = get_unaliased_vartype();
		if (vt & TLI::VT_ARRAY) {
			if (get_pointer_level() == 1) return name + ".in_ptr()";
			return name + ".in()";
		} else
		switch (vt) {
		case ::VT_DATE:
		case ::VT_VARIANT:
		case ::VT_CY:
		case ::VT_BSTR:
			if (get_pointer_level() == 1) return name + ".in_ptr()";
		case ::VT_UNKNOWN:
		case ::VT_DISPATCH:
			return name + ".in()";
		case ::VT_BOOL:
			return name + " ? COMET_VARIANT_TRUE : COMET_VARIANT_FALSE";
		case TLI::VT_EMPTY:
			{
				TypeInfoPtr ti =vti_->TypeInfo;
				switch ( get_unaliased_typekind() ) {
					case TLI::TKIND_INTERFACE:
					case TLI::TKIND_DISPATCH:
					case TLI::TKIND_COCLASS:
						return name + ".in()";
					case TLI::TKIND_RECORD:
					{
						string unwrapped_type = nso.get_name_with_ns(get_unaliased_typeinfo(), true);
						if ( unwrapped_type == "::GUID")
						{
							if (get_pointer_level() == 1) return name + ".in_ptr()";
							return name + ".in()";
						}
						else
						{
							if (get_pointer_level() == 1) return name + ".comet_in_ptr()";
							return name + ".comet_in()";
						}
						//						return name;
					}
					case TLI::TKIND_ALIAS:
					default:
						return name;
				}
				break;
			}
		default:
			return name;
		}
	}
	string get_out_converter(const string& name) const
	{
		return get_out_converter_(name, false,-1);
	}
	string get_out_converter(const string& name, bool asVariantType , long member_id) const
	{
		return get_out_converter_(name, asVariantType, member_id);
	}

	string get_out_converter_(const string& name, bool asVariantType, long member_id) const
	{
		VARTYPE vt = get_unaliased_vartype();
		if (vt & TLI::VT_ARRAY) {
			return name + ".out()";
		} else
		switch (vt) {
		case ::VT_DATE:
		case ::VT_CY:
		case ::VT_VARIANT:
		case ::VT_BSTR:
		case ::VT_UNKNOWN:
		case ::VT_DISPATCH:
			return name + ".out()";
		case ::VT_BOOL:
			{
				std::ostringstream os;
				if( asVariantType )
				{
					if (member_id == -1)
					{
						 os << "&" << name;
					}
					else
						os << "bool_out_" << member_id;
					return os.str();
				}
				else
					os << "bool_out(" << name << ")";
				return os.str();
			}
		case TLI::VT_EMPTY:
			{
				TypeInfoPtr ti =vti_->TypeInfo;
				switch ( get_unaliased_typekind() ) {
				case TLI::TKIND_ENUM:
					return std::string("&") + name;
				case TLI::TKIND_INTERFACE:
					if(asVariantType)
					{
						return "(IUnknown**)" + name + ".out()";
					}
					return name + ".out()";
				case TLI::TKIND_DISPATCH:
					if(asVariantType)
					{
						return "(IDispatch**)" + name + ".out()";
					}
					return name + ".out()";
				case TLI::TKIND_COCLASS:
					return name + ".out()";
				case TLI::TKIND_RECORD:
				{
					string unwrapped_type = nso.get_name_with_ns(get_unaliased_typeinfo(), true);
					if ( unwrapped_type == "::GUID")
						return name + ".out()";
					else
						return name + ".comet_out()";
					//						return name;
				}
				case TLI::TKIND_ALIAS:
				default:
					return name;
				}
				break;
			}
		default:
			return name;
		}
	}

	string constructed_converter(const string &indent, const string &name, bool inout, long member_id ) const
	{
		if( (get_unaliased_vartype() == ::VT_BOOL) )
		{
			std::ostringstream os;
			os << indent << (inout?"bool_inout bool_inout_":"bool_out bool_out_") << member_id << "(" << name << ");\n";
			return os.str();
		}
		return "";
	}

	string get_inout_converter(const string& name, bool asVariantType = false, long member_id =-1) const
	{
		VARTYPE vt =  get_unaliased_vartype();
		if (vt & TLI::VT_ARRAY) {
			return name + ".inout()";
		} else
		switch (vt) {
		case ::VT_DATE:
		case ::VT_CY:
		case ::VT_VARIANT:
		case ::VT_BSTR:
		case ::VT_UNKNOWN:
		case ::VT_DISPATCH:
			return name + ".inout()";
		case ::VT_BOOL:
			if( asVariantType )
			{
				std::ostringstream os;
				os << "bool_inout_";
				if( member_id == -1 ) os << "retval"; else os << member_id;
				return os.str();
			}
			else
				return "bool_inout(" + name + ")";
		case TLI::VT_EMPTY:
			{
				TypeInfoPtr ti =vti_->TypeInfo;
				switch ( get_unaliased_typekind() ) {
				case TLI::TKIND_INTERFACE:
					if(asVariantType)
					{
						return "(IUnknown**)" + name + ".inout()";
					}
					return name + ".inout()";
				case TLI::TKIND_DISPATCH:
					if(asVariantType)
					{
						return "(IDispatch**)" + name + ".inout()";
					}
					return name + ".inout()";
				case TLI::TKIND_COCLASS:
					return name + ".inout()";
				case TLI::TKIND_ENUM:
					return (asVariantType?"(long *)&":"&") + name;
				case TLI::TKIND_RECORD:
				{
					string unwrapped_type = nso.get_name_with_ns(get_unaliased_typeinfo(), true);
					if ( unwrapped_type == "::GUID")
						return name + ".inout()";
					else
						return name + ".comet_inout()";
//						return name;
				}
				default:
					return name;
				}
				break;
			}
		default:
			return name;
		}
	}

	string get_variant_member( const std::string &obj, bool pointer = false, bool bump_pointer_level = false) const
	{
		VARTYPE vt = get_variant_vartype(bump_pointer_level);
		std::ostringstream os;
		if(vt == ::VT_VARIANT)
		{
			if( pointer )
				os << "*";
			os << obj;
		}
		else
		{
			os << vt2refmacro(vt) << "(";
			if(!pointer) os << "&";
			os << obj << ")";
		}

		return os.str();
	}
	string get_variant_type( bool bump_pointer_level = false) const
	{
		return vt2string(get_variant_vartype( bump_pointer_level));
	}

	bool ok_as_out() const
	{
		int pl = get_pointer_level();
		if (pl == 0) return false;
		if (pl == 1)
		{
			switch (vti_->GetVarType())
			{
			case TLI::VT_EMPTY:
				{
					TypeInfoPtr ti = vti_->TypeInfo;
					switch (ti->GetTypeKind()) {
					case TLI::TKIND_INTERFACE:
					case TLI::TKIND_DISPATCH:
					case TLI::TKIND_COCLASS:
						return false;
					default:
						return true;
					}
				}
			case TLI::VT_VOID:
				return false;
			default:
				return true;
			}
		}
		return true;
	}

	string get_cpp_attach(const string& r) const;
	string get_cpp_detach(const string&) const;

	bool requires_wrapper() const;
	bool requires_pointer_initialise() const;
	string get_pointer_initialise( const string &var) const;

	bool is_special_pointer() const
	{
		if( get_unaliased_vartype() == TLI::VT_EMPTY) //vti_->VarType == TLI::VT_EMPTY )
		{
			TypeKinds tk = get_unaliased_typekind(); //vti_->TypeInfo->GetTypeKind();
			return (tk== TLI::TKIND_INTERFACE || tk== TLI::TKIND_DISPATCH );
		}
		return false;
	}

	VARTYPE get_unaliased_vartype() const
	{
		VARTYPE vt = vti_->VarType;
		if( vt == TLI::VT_EMPTY)
		{
			TypeInfoPtr ti = vti_->TypeInfo;
			TypeKinds tk = ti->GetTypeKind();

			if( tk == TLI::TKIND_ALIAS )
			{
				VarTypeInfoPtr ti2 = ti->ResolvedType;
				vt = ti2->VarType;

			}
		}
		return vt;
	}

	bool is_aliased_type() const
	{
		VARTYPE vt = vti_->VarType;
		if(vt == TLI::VT_EMPTY)
		{
			TypeInfoPtr ti = vti_->TypeInfo;
			TypeKinds tk = ti->GetTypeKind();

			return tk == TLI::TKIND_ALIAS;
		}
		return false;
	}

	TypeInfoPtr get_unaliased_typeinfo() const
	{
		TypeInfoPtr ti = vti_->TypeInfo;

		TypeKinds tk = ti->GetTypeKind();

		if( tk == TLI::TKIND_ALIAS)
		{
			VarTypeInfoPtr ti2 = ti->ResolvedType;
			VARTYPE vt2 = ti2->VarType;
			if(vt2 == TLI::VT_EMPTY)
			{
				ti= ti2->TypeInfo;
			}
		}
		return ti;
	}
	TypeKinds get_unaliased_typekind() const
	{
		return get_unaliased_typeinfo()->GetTypeKind();
	}

	bool is_pointer_alias( bool *is_dispatch=NULL) const
	{
		if(is_dispatch!=NULL) *is_dispatch=false;

		if( vti_->VarType == TLI::VT_EMPTY)
		{
			TypeInfoPtr ti = vti_->TypeInfo;

			TypeKinds tk = ti->GetTypeKind();

			if( tk == TLI::TKIND_ALIAS )
			{
				VarTypeInfoPtr ti2 = ti->ResolvedType;
				VARTYPE vt2 = ti2->VarType;

				// Check if the alias is a com pointer-type
				switch( vt2 )
				{
					case TLI::VT_DISPATCH :
						if(is_dispatch!=NULL) *is_dispatch=true;
					case TLI::VT_UNKNOWN :
						return true;
					case TLI::VT_EMPTY:
						{
							TypeInfoPtr ti3 = ti2->TypeInfo;
							TypeKinds tk3 = ti3->GetTypeKind();

							switch (tk3)
							{
								case TLI::TKIND_DISPATCH:
									if(is_dispatch!=NULL) *is_dispatch=true;
								case TLI::TKIND_INTERFACE:
									return true;

							}
						}
				}
			}
		}
		return false;
	}

	int get_pointer_level(bool conpensate_for_quirky_types = true) const {
		if (conpensate_for_quirky_types) {
			if (vti_->VarType == TLI::VT_VOID)
				return vti_->PointerLevel - 1 + pl_mod_;
		}
		return vti_->PointerLevel + pl_mod_;
	}
	void get_library_includes( std::set<string> &library_includes) const
	{
		VARTYPE vt = vti_->VarType;
		if (vt & TLI::VT_ARRAY) {
			library_includes.insert("comet/safearray.h");
			vt &= ~TLI::VT_ARRAY;
		}
		switch (vt) {
		case ::VT_DATE:
			library_includes.insert("comet/datetime.h");
			break;
		case ::VT_CY:
			library_includes.insert("comet/currency.h");
			break;
		case ::VT_VARIANT:
			library_includes.insert("comet/variant.h");
			break;
		case ::VT_BSTR:
			library_includes.insert("comet/bstr.h");
			break;
		}
	}
	bool is_variant(bool bump_pointer_level = false) const
	{
		return (get_variant_vartype(bump_pointer_level) == ::VT_VARIANT);
	}
private:
	string get_raw(int, unsigned&, bool use_variant_type = false, bool force_use_of_ns = false) const;
	string get_cpp(int pl, unsigned& vsz, bool& special, bool no_ref = false, bool is_definition = true, bool html = false, bool is_struct = false) const;
	VARTYPE get_variant_vartype(bool bump_pointer_level = false) const
	{
		int ptrLevelOffset = 0;
		VARTYPE vt = get_unaliased_vartype(); //vti_->VarType;

		if( vt == ::VT_EMPTY )
		{
			TypeInfoPtr ti;
			ti = vti_->TypeInfo;

			enum TypeKinds tk = get_unaliased_typekind();//ti->GetTypeKind();

			while (tk == TLI::TKIND_ALIAS && ti != 0)
			{
				VarTypeInfoPtr vti2 = ti->ResolvedType;
				vt = vti2->VarType;
				ti = 0;
				if (vt == ::VT_EMPTY) {
					ti = vti2->TypeInfo;
					tk = ti->GetTypeKind();
				}
			}

			if( vt == ::VT_EMPTY )
			switch ( tk ) //ti->GetTypeKind())
			{
			case TLI::TKIND_ENUM:
				vt = ::VT_I4; // TODO Check This
				break;
			case TLI::TKIND_INTERFACE:
				vt = ::VT_UNKNOWN;
				ptrLevelOffset = 1;
				break;
			case TLI::TKIND_DISPATCH:
				vt = ::VT_DISPATCH;
				ptrLevelOffset = 1;
				break;
			case TLI::TKIND_RECORD:
				throw std::runtime_error("cannot handle record in VARIANT");
				break;
			case TLI::TKIND_ALIAS:
				throw std::runtime_error("cannot handle alias in VARIANT");
				break;
			case TLI::TKIND_UNION:
				throw std::runtime_error("cannot handle union in VARIANT");
				break;
			case TLI::TKIND_COCLASS:
//				vt = ti->DefaultInterface->VarType;
				vt = ::VT_UNKNOWN;
// TODO: use VT_DISPATCH when DefaultInterface of coclass is dual

				ptrLevelOffset = 1;
				break;
			case TLI::TKIND_MODULE:
				throw std::runtime_error("cannot handle module in VARIANT");
				break;
			case TLI::TKIND_MAX:
				throw std::runtime_error("cannot handle enum-max in VARIANT");
				break;
			default:
				throw std::runtime_error("cannot handle unknown empty type in VARIANT");
			}
		}
		int level =get_pointer_level() - (bump_pointer_level ? 1 : 0);
		switch ( level -ptrLevelOffset )
		{
			case 0:
				break;
			case 1:
				vt |= ::VT_BYREF;
				break;
			default:
				throw std::runtime_error("Cannot handle level of indirection");
		}
		return vt;
	}
};

class Parameter {
	ParameterInfoPtr pi_;
	Type type_;
	mutable string name_;

	bool force_retval_;
public:
	explicit Parameter(const ParameterInfoPtr& pi) {
		force_retval_ = false;
		pi_ = pi;
		type_ = Type(pi_->VarTypeInfo);
	}

	Parameter(const VarTypeInfoPtr& vti, int pl_mod, bool fr)
	{
		force_retval_ = fr;
		pi_ = 0;
		type_ = Type(vti, pl_mod);
	}

	enum direction {
		DIR_IN     = 0,
		DIR_OUT    = 1,
		DIR_INOUT  = 2,
		DIR_RETVAL = 3
	};

	string get_name(bool prefix = true) const {
		if (name_.size() == 0) {
			if (pi_ == 0)
				name_ = "_x";
			else {
				string n = nso.trans_symbol(pi_->GetName());
				if (n.size() == 0)
				{
					static int c = 0;
					std::ostringstream os;
					os << ++c;
					name_ += os.str();

				} else 	name_ += n;
				if (name_.size() == 0) name_ = "_x";
			}
		}
		if (prefix)
			return "par_" + name_;
		else
			return name_;
	}

	string get_tmp_name() const {
		string s = get_name();
		s += "_tmp";
		return s;
	}

	const Type &get_type() const { return type_; }

	string as_raw(bool IsReference = false,bool UseVariantType = false) const
	{
		std::ostringstream os;
		if (type_.requires_wrapper()) {
			if (get_direction() == DIR_IN && type_.get_pointer_level() > 1) os << "const ";
		} else {
			if (get_direction() == DIR_IN && type_.get_pointer_level(false) > 0)
				os << "const ";
			}

		unsigned vsz;
		os << type_.as_raw(vsz,UseVariantType) << " ";
		if(IsReference)
		{
			os << "&";
		}
		os << get_name();

		if (vsz > 0) {
			os << "[" << vsz << "]";
		}

		return os.str();
	}

	string cast_variant_member_to_raw() const
	{
		return type_.cast_variant_member_to_raw();
	}

	string cast_raw_to_variant_member() const
	{
		return type_.cast_raw_to_variant_member();
	}

	string as_convert() const
	{
		std::ostringstream os;
		switch (get_direction()) {
			case Parameter::DIR_IN:
				{
					const Type& t = get_type();
					if (!t.requires_wrapper()) {
						switch (t.get_pointer_level()) {
							case 0:
								break;
							case 1:
								os << "&";
								break;
							default:
#ifndef NDEBUG
								return "<<CANNOT HANDLE LEVEL OF INDIRECTION>>";
#endif // NDEBUG
								throw std::runtime_error("Cannot handle level of indirection");
						}
						os << get_name();
					} else {
						os << t.get_in_converter(get_name());
					}
				}
				break;
			case Parameter::DIR_OUT:
				if (get_type().requires_wrapper()) {
					os << get_type().get_out_converter(get_name());
				} else {
					os << "&" << get_name();
				}
				break;
			case Parameter::DIR_INOUT:
				if (get_type().requires_wrapper()) {
					os << get_type().get_inout_converter(get_name());
				} else {
					os << "&" << get_name();
				}
				break;
				break;
		}
		return os.str();
	}

	bool is_optional()
	{
		if (get_direction() == Parameter::DIR_INOUT) return false;
		if (pi_ == 0) return false;
		int f = pi_->Flags;
		if (f & PARAMFLAG_FHASDEFAULT || f & PARAMFLAG_FOPT) return true;
		return false;
	}

	string default_value()
	{
		int f = pi_->Flags;
		if (f & PARAMFLAG_FHASDEFAULT) {
			return type_.default_value(pi_->DefaultValue);
		} else {
			return type_.default_value();
		}
	}

	enum direction get_direction() const {
		if (force_retval_) return DIR_RETVAL;
		int f;
		if (pi_)
			f = pi_->Flags;
		else
			f = 0;
		f &= ~PARAMFLAG_FHASDEFAULT;
		f &= ~PARAMFLAG_FOPT;
		f &= ~PARAMFLAG_FLCID;
		switch (f) {
		case PARAMFLAG_FIN:
			return DIR_IN;
		case PARAMFLAG_FOUT:
			return DIR_OUT;
		case PARAMFLAG_FIN | PARAMFLAG_FOUT:
			return DIR_INOUT;
		case PARAMFLAG_FOUT | PARAMFLAG_FRETVAL:
			return DIR_RETVAL;
		case 0:
			if (type_.ok_as_out()) return DIR_INOUT;
			return DIR_IN;
		default:
			throw std::logic_error("Unexpected PARAMFLAG");
		}
	}

	bool is_outgoing() const {
		return get_direction() != DIR_IN;
	}
	bool is_incomming() const {
		enum direction  dir = get_direction();
		return dir != DIR_OUT && dir != DIR_RETVAL;
	}
	bool is_retval() const {
		return get_direction() == DIR_RETVAL;
	}

	string constructed_converter(const string &indent, const string &name, long member_id ) const
	{
		switch(get_direction() )
		{
			case Parameter::DIR_INOUT:
				return get_type().constructed_converter( indent, name, true, member_id);
			case Parameter::DIR_OUT:
				return get_type().constructed_converter( indent, name, false, member_id);
			//case Parameter::DIR_RETVAL:
				//return get_type().constructed_converter( indent, name, false, -1);
		}
		return "";
	}

	string get_argument_to_raw(bool asVariantType = false,  long member_id = -1)
	{
		std::ostringstream os;

		switch (get_direction()) {
			case Parameter::DIR_IN:
				{
					const Type& t = get_type();
					if (!t.requires_wrapper()) {
						switch (t.get_pointer_level()) {
							case 0:
								break;
							case 1:
								os << "&";
								break;
							default:
								throw std::runtime_error("Cannot handle level of indirection");
						}
						os << get_name();
					} else {
						os << t.get_in_converter(get_name());
					}
				}
				break;
			case Parameter::DIR_INOUT:
				{
					const Type& t = get_type();
					if (!t.requires_wrapper())
					{
						switch (t.get_pointer_level()) {
							case 0:
								break;
							case 1:
								os << "&";
								break;
							default:
								throw std::runtime_error("Cannot handle level of indirection");
						}
						os << get_name();
					} else
					{
						os << t.get_inout_converter(get_name(), asVariantType, member_id);
					}
				}
				break;
			case Parameter::DIR_RETVAL:
				{
					const Type& t = get_type();
					if (!t.requires_wrapper() || asVariantType)
					{
						long pointerlevel = t.get_pointer_level();
						if (asVariantType && t.get_unaliased_vartype() == TLI::VT_EMPTY )
						{
							switch (t.get_unaliased_typekind()) //ti->GetTypeKind())
							{
							case TLI::TKIND_INTERFACE:
							case TLI::TKIND_DISPATCH:
								--pointerlevel;
								break;
							}
						}
						switch (pointerlevel) {
							case 0:
								break;
							case 1:
								os << "&";
								break;
							default:
								throw std::runtime_error("Cannot handle level of indirection");
						}
						os << "_result_";
					} else
					{
						os << t.get_out_converter("_result_", asVariantType, -1);
					}
				}
				break;
			case Parameter::DIR_OUT:
				{
					const Type& t = get_type();
					if (!t.requires_wrapper())
					{
						switch (t.get_pointer_level()) {
							case 0:
								break;
							case 1:
								os << "&";
								break;
							default:
								throw std::runtime_error("Cannot handle level of indirection");
						}
						os << get_name();
					} else
					{
						os << t.get_out_converter(get_name(), asVariantType, member_id);
					}
				}
				break;
		}
		return os.str();
	}

	void get_library_includes( std::set<string> &library_includes) const
	{
		type_.get_library_includes(library_includes);
	}

	void check_referenced_interfaces(require_interface *req_cb)
	{
		if( type_.get_unaliased_vartype() == TLI::VT_EMPTY )
		{
			switch( type_.get_unaliased_typekind() )
			{
				case TLI::TKIND_INTERFACE:
				case TLI::TKIND_DISPATCH:
				{
					std::string n = type_.as_name();
					(*req_cb)(n);
				}

			}
		}
	}
	string as_param_streamable(bool is_dispmember)
	{
		std::ostringstream os2;
		std::string name;
		long pointerlevel = type_.get_pointer_level(true);
		if (type_.is_special_pointer() )
			--pointerlevel;
		while(pointerlevel-- > 0)
			name += "*";
		name += get_name();
		if (!type_.requires_wrapper())
			os2 << name;
		else
			os2 << type_.get_impl_in_constructor(name,is_dispmember, true);
		return os2.str();

	}

	string as_param_impl(bool is_dispmember)
	{
		std::ostringstream os2;
		switch (get_direction())
		{
			case Parameter::DIR_IN:
				if (!type_.requires_wrapper() && (type_.get_pointer_level() == 1))
					os2 << "*";
				os2 << type_.get_impl_in_constructor(get_name(),is_dispmember);
				break;
			case Parameter::DIR_INOUT:
				if (type_.requires_wrapper())
					os2 << type_.get_impl_inout_constructor(get_name(),is_dispmember);
				else
					os2 << "*" << get_name();
				break;
			case Parameter::DIR_OUT:
				if (type_.requires_wrapper())
					os2 << get_tmp_name();
				else
					os2 << "*" << get_name();
				break;
			default:
				{
					throw std::runtime_error( "Cannot handle parameter direction");
				}
		}
		return os2.str();
	}
};

class Interface;

class InterfaceMember {
	MemberInfoPtr mi_;
	typedef std::vector<Parameter*> PARAMETERS;
	PARAMETERS parameters_;
	bool has_retval_;
	Parameter *retval_;

	bool has_disp_retval_;
	Parameter *disp_retval_type_;

	bool is_second_put_;
	bool is_dispmember_;
	bool is_restricted_;

	Interface* itf_;

	int force_ik_;

	string name_;
public:
	InterfaceMember(const MemberInfoPtr& m, bool IsDispMember, Interface* itf, const std::string &ifname, int force_ik = 0)
		: is_dispmember_(IsDispMember),has_retval_(false), retval_(NULL),is_second_put_(false), itf_(itf), force_ik_(force_ik)
	{
		mi_ = m;
		update();
		name_ = nso.trans_interface_method(ifname, mi_->Name);
		is_restricted_ = ((mi_->GetAttributeMask() & ::FUNCFLAG_FRESTRICTED)!=0);
	}

	~InterfaceMember() {
		delete_all(parameters_.begin(), parameters_.end());
	}

	bool can_implement()
	{
		return !is_dispmember_ || !is_restricted_;
	}

	string get_description()
	{
		_bstr_t s = mi_->GetHelpString( GetThreadLocale() );
		if (!s) return "";
		return (const char*)s;
	}

	void mark_as_second_put()
	{
		is_second_put_ = true;
	}

	short get_vtable_offset() const {
		return mi_->GetVTableOffset();
	}

	bool is_property() {
		int ik = force_ik_ ? force_ik_ : mi_->InvokeKind;
		if (ik == TLI::INVOKE_PROPERTYGET || ik == TLI::INVOKE_PROPERTYPUT || ik == TLI::INVOKE_PROPERTYPUTREF) return true;
		/*        if (ik != TLI::INVOKE_FUNC) {
		throw std::logic_error("Invalid invoke kind");
	}*/
		return false;
	}

	int get_number_of_parameters() const {
		return parameters_.size() + (has_disp_retval_ ? 1 : 0);
	}

	bool has_retval() const { return has_retval_; }

	const Type& get_property_type() {
		if (has_disp_retval_) return disp_retval_type_->get_type();
		if (!is_property()) throw std::logic_error ("InterfaceMember::get_property_type --- Unexpected");
		if (has_retval_) return retval_->get_type();
		if (parameters_.size() == 0)
			throw std::logic_error ((string)"InterfaceMember::get_property_type " + get_name() + " --- no parameters");
		PARAMETERS::const_iterator it = parameters_.end() - 1;
		return (*it)->get_type();
	}


	const Parameter& get_retval() const {
		return *retval_;
	}

	string get_raw_name() const {
		string s;

		switch (force_ik_ ? force_ik_ : mi_->GetInvokeKind()) {
		case TLI::INVOKE_PROPERTYGET:
			s = "get_";
			break;
		case TLI::INVOKE_PROPERTYPUT:
			s = "put_";
			break;
		case TLI::INVOKE_PROPERTYPUTREF:
			s = "putref_";
			break;
		case TLI::INVOKE_FUNC:
			s = "raw_";
			break;
		default:
			throw std::logic_error("Invalid invoke kind");
		}

		s += nso.trans_symbol(mi_->Name);

		return s;
	}

	string get_name() {
		return name_;
	}

	string get_wrapper_name() {
		string s;

		switch (force_ik_ ? force_ik_ : mi_->GetInvokeKind()) {
		case TLI::INVOKE_PROPERTYGET:
			s = "Get";
			break;
		case TLI::INVOKE_PROPERTYPUT:
			s = "Put";
			break;
		case TLI::INVOKE_PROPERTYPUTREF:
			if (is_second_put_)
				s = "Putref";
			else
				s = "Put";
			break;
		case TLI::INVOKE_FUNC:
			s = "";
			break;
		default:
			throw std::logic_error("Invalid invoke kind");
		}

		s += nso.trans_symbol(mi_->Name);

		return s;
	}

	int get_invoke_kind() const {
		return force_ik_ ? force_ik_ : mi_->InvokeKind;
	}

	string get_wrapper(const string& className);

	string get_definition() {
		std::ostringstream os;
		os << "virtual STDMETHODIMP " << get_raw_name() << "(";
		bool first = true;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			if (first)
				first = false;
			else
				os << ", ";
			os << (*it)->as_raw();
		}

		os << ") = 0;\n";

		return os.str();
	}

	string get_impl_decl() {
		std::ostringstream os;
		os << "STDMETHODIMP " << get_raw_name() << "(";
		bool first = true;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			if (first)
				first = false;
			else
				os << ", ";
			os << (*it)->as_raw();
		}

		os << ");\n";

		return os.str();
	}

	string get_nutshell_decl() {
		std::ostringstream os;

		os << "char " << get_raw_name() << "_test(void (_B::* pm)(nil));\n";
		os << "    long " << get_raw_name() << "_test("<<get_wrapper_function_return_type() << " (_B::* pm)(" << get_wrapper_function_arguments(false) << "));\n";

		os << "    inline HRESULT " << get_raw_name() << "(";
		bool first = true;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			if (first)
				first = false;
			else
				os << ", ";
			os << (*it)->as_raw();
		}
		if (first)
			first = false;
		else
			os << ", ";
		os << "impl::false_type";
		os << ");\n";

		os << "    inline HRESULT " << get_raw_name() << "(";
		first = true;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			if (first)
				first = false;
			else
				os << ", ";
			os << (*it)->as_raw();
		}
		if (first)
			first = false;
		else
			os << ", ";
		os << "impl::true_type";
		os << ");\n";

		os << "    STDMETHODIMP " << get_raw_name() << "(";
		first = true;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			if (first)
				first = false;
			else
				os << ", ";
			os << (*it)->as_raw();
		}
		os << ");\n";

		return os.str();
	}

	string get_connection_point_raw_impl() {
		std::ostringstream os;
		os <<
			"    template<typename HANDLER>\n"
			"    void Fire_" << get_name() << "(";
		if( ! is_dispmember_ )
		{
			std::string str =get_wrapper_function_arguments(false);
			os <<  str;
			if (!str.empty()) os << ", ";
		}
		else
		{
			bool first = true;
			for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it)
			{
				if (first)
					first = false;
				else
					os << ", ";
				os << (*it)->as_raw( false, true );
			}
			if(!first) os << ", ";
		}
		os << " HANDLER &_par_h )\n";
		os << "    {\n";

		if( !is_dispmember_ )
		{
			os <<
					"        for (CONNECTIONS::iterator next,it = connections_.begin(); it != connections_.end(); it=next) {\n"
					"            next=it;++next;\n"
					"            if( type_traits::is_null_vtable_entry((void *)it->second.get(), "
												<< get_vtable_offset() <<")) continue;\n"
					"            if( _par_h.check_fail(it->second.raw()->" << get_raw_name() << "(";
			bool first = true;
			for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it)
			{
				if (first)
					first = false;
				else
					os << ", ";
//				os << (*it)->get_name();
				os << (*it)->as_convert();
			}
			os << ")))\n"
				  "                if (_par_h.on_fail( connections_, it)) break;\n"
				  "        }\n";
		}
		else
		{
			os << get_dispatch_prelude_impl("        ");
			os << "\n";
			os <<
				"        for (CONNECTIONS::iterator next,it = connections_.begin(); it != connections_.end(); it=next)\n"
				"        {\n"
				"            next=it;++next;\n";
			os << get_dispatch_raw_impl("            ",true,"it->second");
			os <<
				"            if( _par_h.check_fail( _hr_ ) )  _par_h.on_fail( connections_, it);\n"
				"        }\n";
		}
		os << "    }\n";
		return os.str();
	}
	string get_dispatch_prelude_impl(const string &indent)
	{
		std::ostringstream os;
//		os << indent << "HRESULT _hr_ = S_OK;\n";
//		os << indent<<"variant_t vars[" << parameters_.size() << "];\n";
		if (parameters_.size() > 0)
			os << indent<< "VARIANT vars[" << parameters_.size() << "];\n";
		else
			os << indent << "VARIANT* vars = 0;\n";
		return os.str();
	}
	string get_dispatch_raw_impl(const string &indent, bool RawCalls, const string &InvokePrefix)
	{
		long dispId = mi_->GetMemberId();
		std::ostringstream os;
//		os << indent << "variant_t result;\n";
		int idx = 1;
		int params = parameters_.size();
		for (PARAMETERS::const_iterator par = parameters_.begin(); par != parameters_.end(); ++par,++idx)
		{
			std::ostringstream osv;
			if( !RawCalls)
				os << (*par)->constructed_converter(indent, (*par)->get_name(), idx);

			osv <<  "vars[" << params-idx << "]";
			os <<indent << (*par)->get_type().get_variant_member(osv.str(), false)<< " = ";
			if(RawCalls)
				os << (*par)->cast_raw_to_variant_member() << (*par)->get_name() << "; ";
			else
				os << (*par)->get_argument_to_raw( true, idx ) << "; ";

			if( !(*par)->get_type().is_variant() ) // Don't want to do this to a variant.. bad
			{
				os << "V_VT(&(vars[" << params-idx <<  "])) = " << (*par)->get_type().get_variant_type() << ";\n";
			}
		}
		//os << indent << "DISPPARAMS disp = { vars->in_ptr(), 0, " << parameters_.size() << ", 0 };\n";
		if (is_propertyput()) {
			os << indent << "DISPID did = DISPID_PROPERTYPUT;\n";
			os << indent << "DISPPARAMS disp = { vars, &did, " << parameters_.size() << ", 1 };\n";
		}
		else {
			os << indent << "DISPPARAMS disp = { vars, 0, " << parameters_.size() << ", 0 };\n";
		}
		//os << indent <<"com_ptr<IDispatch> pDisp( it->second );\n";
		os << indent << "HRESULT _hr_ = ";
		if (InvokePrefix.size() == 0)
			os << "raw(this)";
		else
			os << InvokePrefix << ".raw()";
		os << "->Invoke(" <<
			dispId << ", IID_NULL, LOCALE_USER_DEFAULT, " << get_invoke_flag() << ", &disp, " << ( has_disp_retval_ ? "&_result_" : "0" ) << ", 0, 0);\n";

		return os.str();
	}

	bool is_propertyput() const
	{
		switch (get_invoke_kind())
		{
		case TLI::INVOKE_PROPERTYPUT:
		case TLI::INVOKE_PROPERTYPUTREF:
			return true;
		default:
			return false;
		}
	}

	string get_invoke_flag() const {
		switch (get_invoke_kind()) {
		case TLI::INVOKE_PROPERTYGET:
			return "DISPATCH_PROPERTYGET";
			break;
		case TLI::INVOKE_PROPERTYPUT:
			return "DISPATCH_PROPERTYPUT";
			break;
		case TLI::INVOKE_PROPERTYPUTREF:
			return "DISPATCH_PROPERTYPUTREF";
			break;
		default:
			return "DISPATCH_METHOD";
		}
	}

	string get_connection_point_impl( const string &prefix, const string &callprefix, const string &templ_decl, const string &templ_select , bool nothrow, bool noraise) {
		std::ostringstream os;

		if( !templ_decl.empty() )
			os << "    " << templ_decl << '\n';
		os << "    " << get_wrapper_function_header(true, prefix/*"Fire_"*/);
		if( nothrow ) os << " throw()";
		os 	<< "\n    {\n";

		if (has_retval_) {
			throw std::runtime_error("[retval] not allowed in [source] interface");
		}

		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			int dir = (*it)->get_direction();

			switch (dir)
			{
			case Parameter::DIR_OUT:
				throw std::runtime_error("[out] parameter not allowed in [source] interface");
			case Parameter::DIR_INOUT:
//				throw std::runtime_error("[in, out] parameter not allowed in [source] interface");
			default:
				break;
			}
		}
    	os << "        comet::cp_throw handler;" << std::endl;
		os << "        " << callprefix << get_name() << "(";
		bool first = true;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it)
		{
			if (first)
				first = false;
			else
				os << ", ";

			if( !is_dispmember_ )
			{
				os << (*it)->get_name();
			}
			else
			{
				os << (*it)->get_argument_to_raw( false );
			}
		}
		if(!templ_select.empty())
		{
			if( !first) os << ", ";
			os << templ_select;
		}

		os << ")";
		if(!noraise) os <<" | raise_exception";
		os << ";\n";

		os << "    }\n";
		return os.str();
	}

	int ID() const { return mi_->GetMemberId(); }

	string get_dispatch_flag()
	{
		switch (get_invoke_kind()) {
		case TLI::INVOKE_PROPERTYGET:
			return "DISPATCH_PROPERTYGET";
			break;
		case TLI::INVOKE_PROPERTYPUT:
			return "DISPATCH_PROPERTYPUT";
			break;
		case TLI::INVOKE_PROPERTYPUTREF:
			return "DISPATCH_PROPERTYPUTREEF";
			break;
		default:
			return "DISPATCH_METHOD";
		}
	}
	string get_dispatch_type_desc()
	{
		switch (get_invoke_kind()) {
		case TLI::INVOKE_PROPERTYGET:
			return "Get Property";
			break;
		case TLI::INVOKE_PROPERTYPUT:
			return "Put Property";
			break;
		case TLI::INVOKE_PROPERTYPUTREF:
			return "Put Property by Reference";
			break;
		default:
			return "Method";
		}
	}

	string get_dispatch_impl(const string &indent)
	{
		std::ostringstream os;
		os << "if ( (Flags & " << get_dispatch_flag() << ") != 0) {\n";
		os  << indent /*<< "case " << mi_->GetMemberId()*/
			<< "        // " << get_name() << ": " << get_dispatch_type_desc() <<  "\n";
		int params = parameters_.size();
		int i=1;
		os  << indent << "        if (DispParams->cArgs != " << params << ") return DISP_E_BADPARAMCOUNT;\n\n";

		if(params > 0 )
		{
			bool first=true;
			for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it,++i)
			{
				std::stringstream osvm;
				osvm << "DispParams->rgvarg[" << (params-i) << "]";
				os  << indent << "        " << (*it)->as_raw(false,true) << ";\n"
					<< indent << "        if ("
					<< "V_VT(&DispParams->rgvarg[" << (params-i) << "]) != " << (*it)->get_type().get_variant_type() <<")\n"
					<< indent << "        {\n"
					<< indent << "            std::list<variant_t>::iterator vit= argconvert_copy.insert(argconvert_copy.end(), variant_t());\n"
					<< indent << "            if ( VariantChangeType( vit->inout(), "
					<< "&DispParams->rgvarg[" << (params-i)  << "], 0, " << (*it)->get_type().get_variant_type() << ") != S_OK )\n"
					<< indent << "                    return DISP_E_BADVARTYPE;\n"
					<< indent << "            " <<  (*it)->get_name() << " = " << (*it)->cast_variant_member_to_raw()
							<< (*it)->get_type().get_variant_member( "vit->get()",false) <<";\n"
					<< indent << "        }\n"
					<< indent << "        else " << (*it)->get_name() << " = " << (*it)->cast_variant_member_to_raw()
							<<  (*it)->get_type().get_variant_member( osvm.str(), false) << ";\n"  ;
			}
		}
		os << indent << "        try {\n"
		   << get_impl_body( indent + "            " )
		   << InterfaceMember::get_impl_body_catch(indent+"        ", get_wrapper_name() ) << '\n';

		os << indent << "    } ";
		return os.str();
	}

	string get_impl(const string& prefix) {
		std::ostringstream os;

		os << "template<typename _B, typename _S, typename _TL>\n"
		"STDMETHODIMP " << prefix << "Impl<_B, _S, _TL>::"<< get_raw_name() << "(";
		bool first = true;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			if (first)
				first = false;
			else
				os << ", ";
			os << (*it)->as_raw();
		}

		os << ")\n{\n";

		bool pointer_checking = false;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it)
		{
			if ((*it)->is_outgoing()) {
				os << "    COMET_ASSERT(" << (*it)->get_name() << ");\n";
				pointer_checking = true;
			}
		}
		if (pointer_checking) {
			os << "#ifndef COMET_NO_POINTER_CHECKING\n";
			for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it)
			{
				if ((*it)->is_outgoing())
					os << "    if (!" << (*it)->get_name() << ") return E_POINTER;\n";
			}
			os << "#endif\n";
		}

		string indent("    ");
		os << get_pointer_initialise(indent) // COM says that allocated members must be initialised on fail, so initialise.
			<< get_impl_body_try(indent)
			<< get_impl_body(indent + "    ")
			<< get_impl_body_catch(indent, get_wrapper_name())
			<< "}\n";

		return os.str();
	}

	static string get_impl_body_try(const string &indent)
	{
		 return  indent + "try {\n";
	}
	static string get_impl_body_catch(const string &indent, const string &wrapperName)
	{
		std::ostringstream os;
		os  << indent <<  "} COMET_CATCH_CLASS(";
		if (wrapperName == "")
			os << "bstr_t()";
		else
			os << "L\"" << wrapperName  << "\"";
		os << ");\n"
			<< indent <<  "return S_OK;\n";
		return os.str();
	}

	string get_interface_name();

	string get_pointer_initialise( const string &indent)
	{
		std::ostringstream os;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it)
		{
			try {
				switch( (*it)->get_direction() )
				{
					case Parameter::DIR_OUT:
					case Parameter::DIR_RETVAL:
						if (  (*it)->get_type().requires_pointer_initialise() )
							os << indent << (*it)->get_type().get_pointer_initialise( (*it)->get_name()) << ";\n";
						break;
				}
			}
			CATCH_ALL("parameter '" <<(*it)->get_name() << "'"  )
		}
		return os.str();
	}

	string get_impl_body(const string &indent)
	{
		std::ostringstream os;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it)
		{
			try {
				if ((*it)->get_type().requires_wrapper())
				{
					int dir = (*it)->get_direction();

					switch (dir)
					{
						case Parameter::DIR_OUT:
							os  << indent << (*it)->get_type().as_cpp_out() << " " << (*it)->get_tmp_name()
								<< (*it)->get_type().default_constructor() << ";\n";
							break;
						case Parameter::DIR_INOUT:
							break;
						default:
							break;
					}
				}
			}
			CATCH_ALL("parameter '" <<(*it)->get_name() << "'"  )
		}


		if ( do_log_interfaces)
		{
			os <<  "        if (call_logger_<true>::can_log_call())\n"
				   "        {\n";
			bool first=true;
			bool last_was_out=false;
			for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it)
			{
				const string &name = (*it)->get_name(false);
				try{
					if ((*it)->is_retval()) continue;
					bool incoming = (*it)->is_incomming();
					bool comma = !first;
					if (first)
					{
						first=false;
						os << "            tostringstream log_os_;\n";
					}
					if (incoming)
					{
						if (last_was_out)
						{
							last_was_out = false;
							os << ";\n";
						}
						os  << "            impl::do_comet_log( log_os_ << _T(\""
							<< (comma ? ",":"")
							<< name << "=\"), " << (*it)->as_param_streamable(is_dispmember_) << ");\n";
					}
					else
					{
						if (!last_was_out)
						{
							os << "            log_os_";
							last_was_out = true;
						}
						os << " << _T(\"" << (comma ? ",":"") << name << "\")";
					}

				}
				CATCH_ALL("Parameter '" << name <<"'"  )
			}
			if (last_was_out) os << ";\n";
			os <<  "            call_logger_<true>::log_call( _T(\"" << get_interface_name() << "\"), _T(\"" << get_name() << "\"), "
			   << (first ? "tstring()" : " log_os_.str()")
			   << ");\n"
				  "        }\n";
		}

		os << indent << "/*"<< get_help_string(" ","\n") << indent << get_wrapper_decl("  ", true, false) << indent << "*/\n";
		{
			std::ostringstream os2;

			os2 << "static_cast<_B*>(this)->" << get_wrapper_name() << "(";

			bool first = true;
			for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it)
			{
				try{
					if ((*it)->get_direction() == Parameter::DIR_RETVAL) break;

					if (first)
						first = false;
					else
					{
						os2 << ", ";
					}

					os2 << (*it)->as_param_impl(is_dispmember_);
				}
				CATCH_ALL("Parameter '" <<(*it)->get_name(false) <<"'"  )
			}

			os2 << ")";

			if (has_disp_retval_) {
				os << indent << "if (Result) {\n"
				   << indent << "    " << disp_retval_type_->get_type().get_variant_member("Result", true, true) << " = " << retval_->get_type().get_cpp_detach(os2.str()) << ";\n";
				if(!disp_retval_type_->get_type().is_variant(true))
				   os << indent << "    V_VT(Result) = " <<  disp_retval_type_->get_type().get_variant_type(true) << ";\n";
				os << indent << "} else {\n"
				   << indent << "    " << os2.str() << ";\n"
				   << indent << "}\n";
			} else if (has_retval_) {
				os << indent << "*" << retval_->get_name() << " = " << retval_->get_type().get_cpp_detach(os2.str()) << ";\n";
			} else {
				os << indent << os2.str() << ";\n";
			}

		}

		if ( do_log_interfaces)
		{
//			std::ostringstream los;
			os <<  "        if (call_logger_<true>::can_log_return())\n"
				   "        {\n";
			bool first=true;
			for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it)
			{
				if ((*it)->is_outgoing() && ! (*it)->is_retval())
				{
					try {
						bool comma=!first;
						if (first)
						{
							os << "            tostringstream log_os_;\n";
							first=false;
						}

						os << "            impl::do_comet_log( log_os_ << _T(\"" << (comma ? ",":"")
							<< (*it)->get_name(false) << "=\"), " << (*it)->as_param_streamable(is_dispmember_) << ");\n";
					}
					CATCH_ALL("parameter '" <<(*it)->get_name(false) <<"'"  )
				}
			}
			bool retval = has_disp_retval_ || has_retval_;
			if( retval)
			{
				os <<  "            tostringstream log_osr_;\n";
			}

			if (has_disp_retval_)
			{
				os <<  "            if (Result)\n"
					   "            {\n"
					   "                impl::do_comet_log(log_osr_, variant_t::create_const_reference(*Result));\n"
					   "            }\n";
			}
			else if (has_retval_)
			{
				os <<  "            impl::do_comet_log(log_osr_, " << retval_->as_param_streamable(is_dispmember_) << ");\n";
			}

			os <<  "            call_logger_<true>::log_return(_T(\"" << get_interface_name() << "\"), _T(\"" << get_name() << "\"), "
				<< (first ? "tstring(), " : " log_os_.str(), ")
				<< (retval?"log_osr_.str()":"tstring()")
				<< ");\n"
				   "        }\n";
		}

		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			if ((*it)->get_type().requires_wrapper())
			{
				try {
					int dir = (*it)->get_direction();

					switch (dir)
					{
						case Parameter::DIR_OUT:
							os  << indent
								<< "*" << (*it)->get_name() << " = "
								<< (*it)->get_type().get_cpp_detach((*it)->get_tmp_name()) << ";\n";
							break;
						case Parameter::DIR_INOUT:
							// nothing special
						default:
							break;
					}
				}
				CATCH_ALL("parameter '" <<(*it)->get_name(false) <<"'"  )
			}
		}


		return os.str();
	}

	string get_nutshell(const string& prefix) {
		std::ostringstream os;

		os << "template<typename _B, typename _S>\n";
		os << "inline HRESULT " << prefix << "Nutshell<_B, _S>::"<< get_raw_name() << "(";
		bool first = true;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it)
		{
			try {
				if (first)
					first = false;
				else
					os << ", ";
				os << (*it)->as_raw();
			}
			CATCH_ALL("parameter  '" <<(*it)->get_name(false) <<"'"  )
		}
		if (!first) os << ", ";
		os << "impl::false_type)\n{\n";

		if (has_retval_) {
			os << "#ifndef COMET_NO_POINTER_CHECKING\n";
			os << "    if (!" << retval_->get_name() << ") return E_POINTER;\n";
			os << "#endif\n";
		}

		os << "    try {\n";

		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			if ((*it)->get_type().requires_wrapper()) {
				int dir = (*it)->get_direction();

				switch (dir)
				{
				case Parameter::DIR_OUT:
					os << "        ";
					os << (*it)->get_type().as_cpp_out() << " " << (*it)->get_tmp_name() << ";\n";
					break;
				case Parameter::DIR_INOUT:
					os << "        ";
					os << (*it)->get_type().as_cpp_inout() << " " << (*it)->get_tmp_name() << "(" << (*it)->get_type().get_cpp_attach(string("*") + (*it)->get_name()) << ");\n";
					break;
				default:
					break;
				}
			}
		}

		os << "        ";
		if (has_retval_) {
			os << "*" << retval_->get_name() << " = ";
		}

		{
			std::ostringstream os2;
			os2 << "static_cast<_B*>(this)->" << get_wrapper_name() << "(";

			first = true;
			for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
				if ((*it)->get_direction() == Parameter::DIR_RETVAL) break;

				if (first)
					first = false;
				else
					os2 << ", ";

				int dir = (*it)->get_direction();

				const Type& t = (*it)->get_type();
				switch (dir)
				{
				case Parameter::DIR_IN:
					if (!t.requires_wrapper()) {
						if (t.get_pointer_level() == 1) os2 << "*";
					}
					os2 << t.get_impl_in_constructor((*it)->get_name(),is_dispmember_);
					break;
				case Parameter::DIR_INOUT:
					if ((*it)->get_type().requires_wrapper())
					{
						os2 << (*it)->get_tmp_name();
					}
					else
						os2 << "*" << (*it)->get_name();
					break;
				case Parameter::DIR_OUT:
					if ((*it)->get_type().requires_wrapper())
						os2 << (*it)->get_tmp_name();
					else
						os2 << "*" << (*it)->get_name();
					break;
				default:
					throw std::exception("InterfaceMember::get_impl --- Cannot handle parameter direction.");
				}

			}

			os2 << ")";

			if (has_retval_)
				os << retval_->get_type().get_cpp_detach(os2.str());
			else
				os << os2.str();
		}
		os << ";\n";

		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			if ((*it)->get_type().requires_wrapper()) {
				int dir = (*it)->get_direction();

				switch (dir)
				{
				case Parameter::DIR_OUT:
				case Parameter::DIR_INOUT:
					os << "        ";
					os << "*" << (*it)->get_name() << " = " << (*it)->get_type().get_cpp_detach((*it)->get_tmp_name()) << ";\n";
					break;
				default:
					break;
				}
			}
		}
		os << get_impl_body_catch("    ", get_wrapper_name());

		os << "}\n";

		return os.str();
	}

	string get_nutshell_1(const string& prefix, const string& itf_name) {
		std::ostringstream os;

		os << "template<typename _B, typename _S>\n"
			  "inline HRESULT " << prefix << "Nutshell<_B, _S>::"<< get_raw_name() << "(";
		bool first = true;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			if (first)
				first = false;
			else
				os << ", ";
			os << (*it)->as_raw();
		}
		if (!first) os << ", ";
		os <<
			"impl::true_type)\n"
			"{\n"
			"    const com_ptr<" << itf_name << ">& ptr_ = static_cast<_B*>(this)->get_forward_" << itf_name << "();\n"
			"    if (ptr_ == 0) return E_UNEXPECTED;\n"
			"    return ptr_->" << get_raw_name() << "(";
		first = true;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			if (first)
				first = false;
			else
				os << ", ";
			os << (*it)->get_name();
		}
		os << ");\n"
		   "}\n";
		return os.str();
	}

	string get_nutshell_0(const string& prefix) {
		std::ostringstream os;

		os <<
			"template<typename _B, typename _S>\n"
			"STDMETHODIMP " << prefix << "Nutshell<_B, _S>::"<< get_raw_name() << "(";
		bool first = true;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			if (first)
				first = false;
			else
				os << ", ";
			os << (*it)->as_raw();
		}
		os << ")\n"
			"{\n"
			"    // Overriding method requires the following declaration:\n"
			"    // " << get_wrapper_function_header(false, "", false) << "\n"
			"    const int __sz = sizeof( " << get_raw_name() << "_test(static_cast<_B*>(0)->" << get_wrapper_name() << ") );\n"
			"    typedef impl::is_one<__sz>::val __val;\n"
			"    return " << get_raw_name() << "(";
		first = true;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			if (first)
				first = false;
			else
				os << ", ";
			os << (*it)->get_name();
		}
		if (!first) os << ", ";
		os << "__val()";

		os << ");\n"
			"}\n";
		return os.str();
	}

	string get_help_string(const std::string& indent="", const std::string &def="")
	{
			_bstr_t help = mi_->GetHelpString(LOCALE_SYSTEM_DEFAULT);
			if(help.length()!=0)
			{
				return indent + "// " + string(help) + "\n" ;
			}
			return def;
	}

	string get_wrapper_decl( const std::string &indent="", bool declaration=true, bool declare_inline=true, bool html = false) {
		std::stringstream os;
		os <<  indent << get_wrapper_function_header(true, "", declare_inline, html);
		if(declaration)
		{
			os << ";\n";
		}
		else
		{
			os << "\n"<<indent<<"{\n"<< indent << "    throw E_NOTIMPL;\n"<< indent<< "}\n\n";
		}
		return os.str();
	}

	void get_library_includes( std::set<string> &library_includes) const
	{
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it)
		{
			(*it)->get_library_includes(library_includes);
		}
		if(has_retval_ )
			retval_->get_library_includes(library_includes);
	}
	void check_referenced_interfaces(require_interface *req_cb)
	{
		for (PARAMETERS::reverse_iterator it = parameters_.rbegin(); it != parameters_.rend(); ++it)
		{
			(*it)->check_referenced_interfaces(req_cb);
		}
	}
private:
	string get_wrapper_function_arguments(bool is_decl, bool html = false) {
		std::ostringstream os;
		bool first = true;

		long distance_to_default = parameters_.size();
		for (PARAMETERS::reverse_iterator it = parameters_.rbegin(); it != parameters_.rend(); ++it)
		{
			if ((*it)->get_direction() != Parameter::DIR_RETVAL) {

				if ((*it)->get_direction() == Parameter::DIR_OUT || (*it)->is_optional() == false) break;

			}

			distance_to_default--;
		}

		int count=1;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it,++count)
		{
			try {
				if ((*it)->get_direction() == Parameter::DIR_RETVAL) break;

				if (first)
					first = false;
				else
					os << ", ";

				unsigned vsz = 0;
				switch ((*it)->get_direction()) {
				case Parameter::DIR_IN:
					os << (*it)->get_type().as_cpp_in(vsz, html);
					os << " " << (*it)->get_name(!html);
					if (vsz > 0) {
						os << "[" << vsz << "]";
					}
					if (distance_to_default == 0 && is_decl && (*it)->is_optional()) os << " = " << (*it)->default_value();
					break;
				case Parameter::DIR_OUT:
					os << (*it)->get_type().as_cpp_out(html) << ampersand(html);
					os << " " << (*it)->get_name(!html);
					break;
				case Parameter::DIR_INOUT:
					os << (*it)->get_type().as_cpp_inout(html) << ampersand(html);
					os << " " << (*it)->get_name(!html);
	//				if (distance_to_default == 0 && is_decl && (*it)->is_optional()) os << " = " << (*it)->default_value();
					break;
				}

				if (distance_to_default > 0 )
				{
					distance_to_default--;
				}
			}
			CATCH_ALL("parameter " << count << " '" <<(*it)->get_name(false) <<"'"  )
		}
		return os.str();
	}


	string get_wrapper_function_return_type(bool html = false) {
		std::ostringstream os;

		if (has_retval_) {
			os << retval_->get_type().as_cpp_retval(html) << " ";
		} else {
			os << "void ";
		}

		return os.str();
	}

	string get_wrapper_function_header(bool is_decl, const string& prefix = "", bool declare_inline = true, bool html = false) {
		std::ostringstream os;

		if (declare_inline) os << "inline ";

		os << get_wrapper_function_return_type(html);

		os << prefix << get_wrapper_name() << "(";

		os << get_wrapper_function_arguments(is_decl, html);

		os << ")";

		return os.str();
	}

	void update();

};

class Interface {
	InterfaceInfoPtr itfi_;
	typedef std::vector<InterfaceMember*> MEMBERS;
	MEMBERS members_;

	struct Property {
		InterfaceMember *put;
		InterfaceMember *get;
		InterfaceMember *second_put;
		explicit Property() : put(0), second_put(0), get(0) {}
	};

	typedef std::map<string, Property> PROPERTYMAP;
	PROPERTYMAP properties_;
	bool is_dispinterface_;
	bool do_implement_;
	bool is_foreign_;
	bool do_stub_;
	bool do_server_wrapper_;
	std::string real_name_;

public:
	Interface(const InterfaceInfoPtr& i, bool isdispinterface)
		: is_dispinterface_(isdispinterface), do_implement_(true), is_foreign_(false), do_stub_(false),
		do_server_wrapper_(false)  {

		itfi_ = i;
		if (itfi_->ImpliedInterfaces->GetCount() == 0)
		{
			std::ostringstream os;
			os << "Skipping interface " << get_name() << " - no base interface.";
			throw std::runtime_error(os.str());
		}

		real_name_ = nso.trans_symbol(itfi_->GetName());
		update();
	}

	~Interface() {
		delete_all(members_.begin(), members_.end());
	}

	string get_description()
	{
		_bstr_t s = itfi_->GetHelpString( GetThreadLocale() );
		if (!s) return "";
		return (const char*)s;
	}

	string get_html_detail()
	{
		std::ostringstream os;

		os << "<h3><a name=" << get_name() << ">" << get_name() << "</a></h3>\n";
		os << "<p>" << get_description() << "</p>\n";

		for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it)
		{
			try {
				os << "<p>" << (*it)->get_wrapper_decl("", true, false, true) << "\n<br>\n<i>" << (*it)->get_description() << "</i></p>\n";
			} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
			//	os << (*it)->get_connection_point_impl() << std::endl;
		}

		return os.str();
	}

	string get_typedef()
	{
		string rv = "typedef com_ptr<";
		rv += get_name();
		rv += "> ";
		rv += get_name();
		rv += "Ptr;\n";
		return rv;
	}

	string get_name(bool with_ns = true)  const {
		if (with_ns) return nso.get_name_with_ns(itfi_);
		return real_name_;
	}
	void dont_implement() { do_implement_ = false; do_stub_ = false; }
	bool implement() const  { return do_implement_; }
	void is_foreign( bool newval) { is_foreign_ = newval; }
	bool is_foreign() const  { return is_foreign_; }

	bool do_stub() const  { return do_stub_;}
	void do_stub( bool newval) { do_stub_ = newval;}

	bool implement_iface() { return !is_foreign_ && (do_implement_ || do_stub_); }
	bool only_stub_iface() { return do_stub_; }

	bool do_server_wrapper() { return do_server_wrapper_;}
	void do_server_wrapper( bool newval ) {do_server_wrapper_=newval;}

	string get_connection_point_impl()
	{

		std::ostringstream os;

		os << "template<> class connection_point<"  << get_name() << "> : public connection_point_impl<" << get_name() <<"> {\n";
		os << "public:\n";

		os << "    connection_point(IUnknown* pUnk) : connection_point_impl<" << get_name() << ">(pUnk) {}\n\n";
		if (!only_stub_iface())
		{
			for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it)
			{
				try {
					if ((*it)->can_implement())
					{
						os << (*it)->get_connection_point_impl("Fire_", "Fire_", "", "handler", false, true) << std::endl;
					}
				} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
			}
//			for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it)
//			{
//				os << (*it)->get_connection_point_impl("SafeFire_", "", "comet::cp_nothrow_remove()", true,true) << std::endl;
//			}
			for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it)
			{
				try {
					if ((*it)->can_implement())
						os << (*it)->get_connection_point_raw_impl() << std::endl;
				} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
			}
		}
		else
		{
			os << " // Unrequired source interface\n\n";
		}
		os << "};\n\n";

		return os.str();
	}

	long get_base_vtable_size()
	{
		InterfacesPtr itfs =  itfi_->ImpliedInterfaces;
		if(itfs->Count == 0 )
		{
			return 0;
		}
		InterfaceInfoPtr itf=itfs->Item[1];
		MembersPtr membs = itf->Members;

		if(membs->Count > 0 )
		{
			return membs->Item[membs->Count]->VTableOffset + 4;
		}
		return 0;
	}

	string get_base_name() {
		string s;
		if( itfi_->ImpliedInterfaces->GetCount() > 0 )
		{
			return nso.get_name_with_ns(itfi_->ImpliedInterfaces->Item[1]);
		}
		else
		{
			s="";
		}
		return s;
	}
	TypeInfoPtr get_base_interface( bool ignore_unk_disp = true )
	{
		TypeInfoPtr ret;
		if( itfi_->ImpliedInterfaces->GetCount() > 0 )
		{
			ret = itfi_->ImpliedInterfaces->Item[1];
			string nsn = nso.get_name_with_ns(ret);
			if (nsn == "IDispatch" || nsn == "IUnknown")
			{
				ret = NULL;
			}
		}
		return ret;
	}

/*	string get_base_name_with_ns() {
		string s;
		//s = itfi_->Parent->Name;
		if( itfi_->ImpliedInterfaces->GetCount() > 0 )
		{
			s = itfi_->ImpliedInterfaces->Item[1]->Parent->Name;
			if (s == "stdole") s = "";
			s += "::";
			s += itfi_->ImpliedInterfaces->Item[1]->Name;
		}
		else
		{
			s="";
		}

		return s;
	}
*/
	string get_uuid() {
		string s = itfi_->GUID;
		s = s.substr(1, s.size() - 2);
		return s;
	}

	string get_forward_decl() {
		std::ostringstream os;
		os << "struct DECLSPEC_UUID(\"" << get_uuid() << "\") " << get_name() << ";\n";
		return os.str();
	}

	string get_prop_wrapper()
	{
		if(properties_.begin() == properties_.end()) return "";


		std::ostringstream os;

		std::string basename = get_base_name();

		os << "template<> struct prop_wrapper<"  << get_name() << "> : public "  << get_name();
		os << " {\n";
		for (PROPERTYMAP::iterator it = properties_.begin(); it != properties_.end(); ++it) {
			string t;
			// Continue if get and put types are pointer_level incompatible
			// Maybe it will work???

			if (it->second.get) {
				if (it->second.put && it->second.put->get_property_type().get_pointer_level() != it->second.get->get_property_type().get_pointer_level() - 1) continue;

				if (it->second.get->get_number_of_parameters() == 0) continue;

				if (!it->second.get->has_retval()) continue;
			}

			os << "    __declspec(property(";
			if (it->second.put) {
				unsigned vsz = 0;
				t = it->second.put->get_property_type().as_cpp_in(vsz);
				if (vsz != 0) throw std::runtime_error("VT_VECTOR properties are not supported.");
				os << "put=" << it->second.put->get_wrapper_name();
				if (it->second.get) os << ", ";
			}
			if (it->second.get) {
				os << "get=" << it->second.get->get_wrapper_name();
				t = it->second.get->get_property_type().as_cpp_retval();
			}
			os << "))\n    " << t << " " << it->first << ";\n";
		}
		os << "};\n";

		return os.str();
	}

	string get_stub()
	{
		std::ostringstream os;
		os << "\t// Interface " << get_name(false) << "\n\t//\n\n"
			<< get_wrapper_decls( "\t", false, true, true);
		return os.str();
	}

	string get_wrapper_decl()
	{
		std::ostringstream os;

		std::string basename = get_base_name();

		os << "template<> struct wrap_t<" << get_name() << ">";

		if( !basename.empty() && basename != "::IUnknown" && basename != "::IDispatch")
			os << " : public wrap_t<" << basename << ">";
		os << " {\n";


		if(!only_stub_iface())
		{
			os << "\n    // Wrapper declarations\n";
			for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it) {
				try {
					if ((*it)->can_implement())
					{
						os << (*it)->get_help_string("    ");
						os << "    " << (*it)->get_wrapper_decl();
					}
				} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
			}

			if(properties_.begin() != properties_.end())
			{
				os << "\n    // Properties\n";
				os << "#ifdef COMET_ALLOW_DECLSPEC_PROPERTY\n";
				for (PROPERTYMAP::iterator it = properties_.begin(); it != properties_.end(); ++it) {
					string t;
					// Continue if get and put types are pointer_level incompatible
					// Maybe it will work???

					if (it->second.get) {
						if (it->second.put && it->second.put->get_property_type().get_pointer_level() != it->second.get->get_property_type().get_pointer_level() - 1)
						{
								continue;
						}

						if (it->second.get->get_number_of_parameters() == 0)
						{
								continue;
						}


						if (!it->second.get->has_retval())
						{
								continue;
						}

					}

					os << "    __declspec(property(";

					int index_dim = 1;
					if (it->second.put) {
						unsigned vsz = 0;
						t = it->second.put->get_property_type().as_cpp_in(vsz);
						index_dim = it->second.put->get_number_of_parameters()- 1;
						if (vsz != 0) throw std::runtime_error("VT_VECTOR properties are not supported.");
						os << "put=" << it->second.put->get_wrapper_name() ;
						if (it->second.get) os << ", ";
					}
					if (it->second.get) {
						os << "get=" << it->second.get->get_wrapper_name();
						index_dim = it->second.get->get_number_of_parameters() - 1;

						t = it->second.get->get_property_type().as_cpp_retval();
					}
					os << "))\n    " << t << " " << it->first;
					while (index_dim--)
						os << "[]";
					os << ";\n";
				}
				os << "#endif\n";
			}
		}
		else
			os << "\n    // Unrequired interface\n";

		os << "};\n";

		return os.str();
	}

	bool is_dispinterface() const { return is_dispinterface_; }

	string get_decl() {

		std::ostringstream os;

		std::string basename = get_base_name();

		os << "struct DECLSPEC_UUID(\"" << get_uuid() << "\") " << get_name();
		if( !basename.empty()) os << " : public " << get_base_name();
		os << " {\n";

			if(!is_dispinterface_ && members_.size() > 0)
			{
				int vtable_pos = get_base_vtable_size();
				int entry_no= (vtable_pos/4);
				bool ispublic=true;
				if(!only_stub_iface())
				{
					os << "\n    // Raw methods\n";
				}
				else
				{
					 os << "\nprivate:  // VTable fillers for Raw methods on unrequired interface.\n";
				}

				for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it, vtable_pos+=4, ++entry_no)
				{
					int vtable_offset=(*it)->get_vtable_offset();
					while( vtable_pos < vtable_offset )
					{
						if(ispublic) { os << "private:  // VTable fillers\n"; ispublic=false;}
						os << "    virtual HRESULT vtable_filler__" << entry_no << "() = 0;\n";
						vtable_pos +=4;
						++entry_no;
					}
					try {
						if(!only_stub_iface())
						{
							if(!ispublic) {  os << "public:\n"; ispublic=true;}
							os << "    " << (*it)->get_definition();
						}
						else
						{
							os << "    virtual void noimpl__" << (*it)->get_raw_name() << "() = 0;\n";
						}
					} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
				}
			}

/*			os << "\n    // Wrapper declarations\n";
			for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it) {
				os << "    " << (*it)->get_wrapper_decl();
			}

			if(properties_.begin() != properties_.end())
			{
				os << "\n    // Properties\n";
				os << "#ifdef COMET_ALLOW_DECLSPEC_PROPERTY\n";
				for (PROPERTYMAP::iterator it = properties_.begin(); it != properties_.end(); ++it) {
					string t;
					// Continue if get and put types are pointer_level incompatible
					// Maybe it will work???

					if (it->second.get) {
						if (it->second.put && it->second.put->get_property_type().get_pointer_level() != it->second.get->get_property_type().get_pointer_level() - 1) continue;

						if (it->second.get->get_number_of_parameters() == 0) continue;

						if (!it->second.get->has_retval()) continue;
					}

					os << "    __declspec(property(";
					if (it->second.put) {
						unsigned vsz = 0;
						t = it->second.put->get_property_type().as_cpp_in(vsz);
						if (vsz != 0) throw std::runtime_error("VT_VECTOR properties are not supported.");
						os << "put=" << it->second.put->get_wrapper_name();
						if (it->second.get) os << ", ";
					}
					if (it->second.get) {
						os << "get=" << it->second.get->get_wrapper_name();
						t = it->second.get->get_property_type().as_cpp_retval();
					}
					os << "))\n    " << t << " " << it->first << ";\n";
				}
				os << "#endif\n";
			}
		*/

		os << "};\n";

		return os.str();
	}

	string get_wrappers() {

		string s;

		string prefix = "wrap_t<" + get_name() + ">::";

		for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it) {
			try {
				DEBUG(2, "\t" << (*it)->get_name() << std::endl);
				if ((*it)->can_implement())
					s += (*it)->get_wrapper(prefix) + "\n";
			} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
		}
		return s;
	}

	string get_impl_forward_decl(const string& tl_name) {

		std::ostringstream os;

		os << "template<typename _B, typename _S = " << get_name() << ", typename _TL = " << tl_name << "_type_library >\n"
			  "struct ATL_NO_VTABLE DECLSPEC_UUID(\"" << get_uuid() << "\") " << get_name() << "Impl;\n";

		return os.str();
	};

	string get_impl_decl() {

		std::ostringstream os;

		int attr_mask = itfi_->AttributeMask;

		os << "template<typename _B, typename _S, typename _TL>\nstruct ATL_NO_VTABLE DECLSPEC_UUID(\""
			<< get_uuid() << "\") " << get_name() << "Impl : public ";

		string bn = get_base_name();
		if (bn == "::IUnknown") {
			os << "_S";
		} else if (bn == "::IDispatch") {
			os << "impl_dispatch<" << "_S, _TL>";
		} else {
			os << bn << "Impl<_B, _S, _TL>";
		}
		os << "\n{\n";

		os << "    typedef " << get_name() << " interface_is;\n\n";

		if (is_dispinterface_ )
		{
			os <<
				"    private:\n"
				"        STDMETHOD(Invoke)(DISPID id, REFIID riid, LCID lcid, WORD Flags, DISPPARAMS *DispParams, VARIANT* Result, EXCEPINFO* pe, UINT* pu);";
		}
		else
		{
			int vtable_pos = get_base_vtable_size();
			int entry_no= (vtable_pos/4);
			os << "\n    // Raw method wrappers\n";
			bool ispublic=true;
			for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it, vtable_pos+=4, ++entry_no)
			{
				int vtable_offset=(*it)->get_vtable_offset();
				while( vtable_pos < vtable_offset )
				{
					if(ispublic) { os << "private:  // VTable Fillers\n"; ispublic=false;}
					os << "    HRESULT vtable_filler__" << entry_no << "() { return E_NOTIMPL; } \n";
					vtable_pos +=4;
					++entry_no;
				}
				if(!ispublic) {  os << "public:\n"; ispublic=true;}
				try {
					os << "    " << (*it)->get_impl_decl();
				} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
			}
		}

		os << "\n    //\n    // These are the methods you need to implement: \n    //\n/*\n";
		os << get_wrapper_decls( "    ", true, false,true);
		os << "*/\n";
		os << "};\n";

		return os.str();
	}
	string get_wrapper_decls(const std::string &indent ,  bool declaration=true, bool declare_inline = true, bool helpstring = false)
	{
		std::stringstream os;
		for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it)
		{
			try {
				if ((*it)->can_implement())
				{
					if( helpstring)
						os << (*it)->get_help_string(indent);
					os << (*it)->get_wrapper_decl(indent, declaration, declare_inline);
				}
			} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
		}
		return os.str();
	}

	string get_impl() {

		string s;

		string prefix = get_name();

		if (is_dispinterface_ )
		{
			 s = get_dispatch_impl(prefix);
		}
		else
		{
			for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it) {
				try {
					s += (*it)->get_impl(prefix) + '\n';
				} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
			}
		}
		return s;
	}

	string get_dispatch_impl(const string &prefix) {

		std::ostringstream os;

		os << "template<typename _B, typename _S, typename _TL>\n";

		string indent("    ");

		os  << "STDMETHODIMP "<< prefix << "Impl<_B, _S, _TL>::Invoke(DISPID id, REFIID , LCID , WORD Flags, DISPPARAMS *DispParams, VARIANT* Result, EXCEPINFO* pe, UINT* pu)\n"
			"{\n"
			"  std::list<variant_t> argconvert_copy;\n"
			"  if (DispParams->cNamedArgs > 0) return DISP_E_NONAMEDARGS;\n"
			<<  InterfaceMember::get_impl_body_try(indent) <<
			"        switch(id)\n"
			"        {\n";

		typedef std::map<int, std::list<InterfaceMember*> > MS;
		MS ms;
		for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it)
			if ((*it)->can_implement())
				ms[(*it)->ID()].push_back( *it );


		for (MS::iterator it = ms.begin(); it != ms.end(); ++it)
		{
			os << "        case " << it->first << ":\n";

			bool not_first = false;
			os << "            ";
			for (std::list<InterfaceMember*>::iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
				try {
				if (not_first) os << "else "; else { not_first = true; }
				os << (*it2)->get_dispatch_impl("        ");
				} CATCH_ALL("dispatch member '" <<(*it2)->get_name() <<"'"  )
			}
			os << "\n            break;\n";
		}

		os <<
			"        default:\n"
			"                return DISP_E_MEMBERNOTFOUND;\n"
			"        }\n"
			<< InterfaceMember::get_impl_body_catch(indent, "") <<
			"}\n";

		return os.str();
	}

	string get_nutshell_forward_decl() {

		std::ostringstream os;

		os << "template<typename _B, typename _S = " << get_name();
		os << ">\nstruct ATL_NO_VTABLE DECLSPEC_UUID(\"" << get_uuid() << "\") " << get_name() << "Nutshell;\n";
		return os.str();
	};

	string get_nutshell_decl(const string& tl_name) {

		std::ostringstream os;

		int attr_mask = itfi_->AttributeMask;

		os << "template<typename _B, typename _S"; // = " << get_name();
		os << ">\nstruct ATL_NO_VTABLE DECLSPEC_UUID(\"" << get_uuid() << "\") " << get_name() << "Nutshell : public ";

		string bn = get_base_name();
		if (bn == "::IUnknown") {
			os << "_S";
		} else if (bn == "::IDispatch") {
			os << "impl_dispatch<" << "_S, " << tl_name << "_type_library >";
		} else {
			os << bn << "Nutshell<_B, _S>";
		}
		os << "\n{\n";

		os << "public:\n";

		os << "    typedef " << get_name() << " interface_is;\n\n";

		for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it) {
			try {
				if ((*it)->can_implement())
					os << "    " << (*it)->get_nutshell_decl();
			} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
		}

		for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it) {
			try {
				if ((*it)->can_implement())
					os << "    void " << (*it)->get_wrapper_name() << "(nil);\n";
			} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
		}

		os << "\n    const com_ptr<" << get_name() << ">& get_forward_" << get_name() << "() { return static_cast<_B*>(this)->get_forward(); }\n";

		os << "};\n";

		return os.str();
	}

	string get_nutshell() {

		string s;

		string prefix = get_name();

		for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it) {
			try {
				if ((*it)->can_implement())
				{
					s += (*it)->get_nutshell_0(prefix) + '\n';
					s += (*it)->get_nutshell(prefix) + '\n';
					s += (*it)->get_nutshell_1(prefix, get_name()) + '\n';
				}
			} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
		}
		return s;
	}
	void get_library_includes( std::set<string> &library_includes) const
	{

		if (is_dispinterface_ )
		{
			library_includes.insert("list");
			library_includes.insert("comet/variant.h");
		}

		for (MEMBERS::const_iterator it = members_.begin(); it != members_.end(); ++it)
		{
			try {
				if ((*it)->can_implement())
					(*it)->get_library_includes( library_includes);
			} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
		}
	}

	string get_sa_trait_specialisation() const
	{
		std::ostringstream os;
		bool dispatch = true;

		InterfaceInfoPtr iip=itfi_;
		while( true)
		{
			if( iip->ImpliedInterfaces->GetCount() == 0 )
				return "";
			iip =iip->ImpliedInterfaces->Item[1];
			if (iip == NULL )
				return "";

			string n = iip->Name;
			if( n == "IUnknown" )
			{
				dispatch = false;
				break;
			}
			if( n == "IDispatch" )
			{
				dispatch = true;
				break;
			}
		}

		os << "    template<> struct sa_traits< com_ptr<" << get_name() << "> > "
			": public interface_sa_traits<" << get_name() << ", " << (dispatch?"VT_DISPATCH":"VT_UNKNOWN")
			<< ", " << (dispatch? "FADF_DISPATCH":"FADF_UNKNOWN")<<  "> {};\n";
		return os.str();

	}

private:

	void add_prop(InterfaceMember* pIm)
	{
		Property& prop = properties_[pIm->get_name()];
		switch (pIm->get_invoke_kind()) {
			case TLI::INVOKE_PROPERTYGET:
				prop.get = pIm;
				break;
			case TLI::INVOKE_PROPERTYPUT:
				if (prop.put) prop.second_put = prop.put;
				prop.put = pIm;
				break;
			case TLI::INVOKE_PROPERTYPUTREF:
				if (prop.put)
					prop.second_put = pIm;
				else
					prop.put = pIm;
				break;
			default:
				throw std::logic_error("unexpected invoke kind");
		}
	}

	void update() {
		MembersPtr ms = itfi_->Members;
		for (int i=1; i<=ms->Count; ++i) {

			InterfaceMember *pIm;
			if (is_dispinterface_ && ms->Item[i]->InvokeKind == 0)
			{
				pIm = new InterfaceMember(ms->Item[i], is_dispinterface_, this, real_name_, TLI::INVOKE_PROPERTYPUT);
				members_.push_back(pIm);
				add_prop(pIm);

				pIm = new InterfaceMember(ms->Item[i], is_dispinterface_, this, real_name_, TLI::INVOKE_PROPERTYGET);
				members_.push_back(pIm);
				add_prop(pIm);
			}
			else
			{
				pIm = new InterfaceMember(ms->Item[i], is_dispinterface_, this, real_name_);

				members_.push_back(pIm);

				if (pIm->is_property()) add_prop(pIm);
			}
		}

		for (PROPERTYMAP::iterator it = properties_.begin(); it != properties_.end(); ++it) {
			if (it->second.second_put) {
				it->second.second_put->mark_as_second_put();
			}
		}
	}
public:
	void check_referenced_interfaces(require_interface *req_cb, bool only_inherited =false)
	{
		if( !only_inherited)
		{
			for (MEMBERS::iterator it = members_.begin(); it != members_.end(); ++it)
			{
				try {
					if ((*it)->can_implement())
						(*it)->check_referenced_interfaces(req_cb);
				} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
			}
		}

		if( itfi_->ImpliedInterfaces->GetCount() > 0 )
		{
			std::string bn;
			bn= itfi_->ImpliedInterfaces->Item[1]->Name;

			if (bn != "::IUnknown"  && bn != "::IDispatch")
			{
				(*req_cb)(nso.trans_symbol(bn));
			}
		}
	}
};

class RecordMember {
	MemberInfoPtr mi_;
	Type type_;
	string name_;
public:
	RecordMember(const MemberInfoPtr& mi, const std::string &recordname)
		: mi_(mi)
	{
		update();
		name_ = nso.trans_interface_method(recordname, mi_->Name);
	}

	string get_name() { return name_; }

	const Type &get_type() const { return type_; }
	Type &get_type() { return type_; }

	short get_vtable_offset() const { return mi_->GetVTableOffset(); }

	void get_library_includes( std::set<string> &library_includes) const
	{
		type_.get_library_includes(library_includes);
	}

private:
	void update()
	{
		type_ = Type(mi_->ReturnType);
	}
};

class TypeItem {
public:
	virtual string get_name(bool with_ns = false) const = 0;
	virtual string get_definition() const = 0;
	virtual std::vector<string> get_dependents() const = 0;
	virtual string get_sa_trait_specialisation() const = 0;
	virtual string get_uuid() const = 0;
	virtual string get_forward_decl() const = 0;
	virtual void get_library_includes( std::set<string> &library_includes) const = 0;
};

class Record : public TypeItem {
	RecordInfoPtr ri_;

	typedef std::vector<RecordMember*> RECORDMEMBERS;
	RECORDMEMBERS recordMembers_;
	string name_;
public:
	Record(const RecordInfoPtr& ri) : ri_(ri) {
		update();
		name_ = nso.trans_symbol(ri_->Name);
	}

	~Record() {
		delete_all(recordMembers_.begin(), recordMembers_.end());
	}

	string get_forward_decl() const {
		std::ostringstream os;
		os << "struct DECLSPEC_UUID(\"" << get_uuid() << "\") " << get_name() << ";\n";
		return os.str();
	}

	string get_uuid() const {
		string s = ri_->GUID;
		s = s.substr(1, s.size() - 2);
		return s;
	}
	string get_name(bool with_ns = false) const {
		if (with_ns)
			return nso.get_name_with_ns(ri_);
		return name_;
	}

	string get_raw_name(bool with_ns = false) const {
		if (with_ns) return nso.get_name_with_ns(ri_, false, "raw_");
		return string("raw_") + nso.trans_symbol(ri_->GetName());
	}

	bool requires_wrapper() const
	{
		for (RECORDMEMBERS::const_iterator it = recordMembers_.begin(); it != recordMembers_.end(); ++it) {
			if ((*it)->get_type().requires_wrapper()) return true;
		}
		return false;
	}

	bool permits_wrapper() const
	{
		try {
			for (RECORDMEMBERS::const_iterator it = recordMembers_.begin(); it != recordMembers_.end(); ++it) {
				try {
					unsigned vsz;
					(*it)->get_type().as_cpp_struct_type(vsz);
				} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
			}
		}
		catch (std::exception&)
		{
			return false;
		}
		return true;
	}

	string get_definition() const {
		std::ostringstream os;

		if (requires_wrapper() && permits_wrapper()) {

		os << "struct " << get_raw_name();
		os << " {\n";

		for (RECORDMEMBERS::const_iterator it = recordMembers_.begin(); it != recordMembers_.end(); ++it) {
			unsigned vsz;
			try {
				os << "    " << (*it)->get_type().as_raw(vsz) << " " << (*it)->get_name();
	//			os << "    " << (*it)->get_type().as_cpp_struct_type(vsz) << " " << (*it)->get_name();
				if (vsz > 0) os << "[" << vsz << "]";
				os << ";\n";
			} CATCH_ALL("record member '" <<(*it)->get_name() <<"'"  )
		}

		os << "};\n\n";

		os << "struct " << get_name();
		os << " {\n";

		for (RECORDMEMBERS::const_iterator it = recordMembers_.begin(); it != recordMembers_.end(); ++it) {
			unsigned vsz;
			try {
	//			os << "    " << (*it)->get_type().as_raw(vsz) << " " << (*it)->get_name();
				os << "    " << (*it)->get_type().as_cpp_struct_type(vsz) << " " << (*it)->get_name();
				if (vsz > 0) os << "[" << vsz << "]";
				os << ";\n";
			} CATCH_ALL("member '" <<(*it)->get_name() <<"'"  )
		}

		os << std::endl;


		os << "    " << get_name() << "() {}\n\n";
		os << "    " << get_name() << "(const impl::auto_attach_t<" << get_raw_name() << ">& x)\n";
		os << "    { memcpy(this, &x.get(), sizeof(" << get_raw_name() << ")); }\n\n";

		os << "    " << get_raw_name() << " comet_in() const\n";
		os << "    { return *reinterpret_cast<const " << get_raw_name() << "*>(this); }\n";

		os << "    " << get_raw_name() << "* comet_in_ptr() const\n";
		os << "    { return const_cast<" << get_raw_name() << "*>(reinterpret_cast<const " << get_raw_name() << "*>(this)); }\n";

		os << "    " << get_raw_name() << "* comet_inout()\n";
		os << "    { return const_cast<" << get_raw_name() << "*>(reinterpret_cast<const " << get_raw_name() << "*>(this)); }\n\n";

		os << "    " << get_raw_name() << "* comet_out()\n";
		os << "    {\n";
		os << "      this->~" << get_name() << "();\n";
		os << "      new (this) " << get_name() << "();\n";
		os << "      return const_cast<" << get_raw_name() << "*>(reinterpret_cast<const " << get_raw_name() << "*>(this));\n";
		os << "    }\n\n";

		os << "    static const " << get_name() << "& comet_create_const_reference(const " << get_raw_name() << "& x)\n";
		os << "    { return *reinterpret_cast<const " << get_name() << "*>(&x); }\n";

		os << "    static " << get_name() << "& comet_create_reference(" << get_raw_name() << "& x)\n";
		os << "    { return *reinterpret_cast<" << get_name() << "*>(&x); }\n";

		os << "    static " << get_raw_name() << " comet_detach(" << get_name() << "& x)\n";
		os << "    {\n";
		os << "      " << get_raw_name() << " rv;\n";
		os << "      memcpy(&rv, &x, sizeof(" << get_raw_name() << "));\n";
		os << "      new (&x) " << get_name() << "();\n";
		os << "      return rv;\n";
		os << "    };\n";

		os << "};\n\n";

		os << "COMET_STATIC_ASSERT( sizeof(" << get_name() << ") == sizeof(" << get_raw_name() << ") );\n";

		}
		else
		{
		os << "struct " << get_name();
		os << " {\n";

		for (RECORDMEMBERS::const_iterator it = recordMembers_.begin(); it != recordMembers_.end(); ++it) {
			try {
				unsigned vsz;
				os << "    " << (*it)->get_type().as_raw(vsz) << " " << (*it)->get_name();
				//			os << "    " << (*it)->get_type().as_cpp_struct_type(vsz) << " " << (*it)->get_name();
				if (vsz > 0) os << "[" << vsz << "]";
				os << ";\n";
			} CATCH_ALL("record member '" <<(*it)->get_name() <<"'"  )
		}

		os << std::endl;


		os << "    " << get_name() << "() {}\n\n";
		os << "    " << get_name() << "(const impl::auto_attach_t<" << get_name() << ">& x)\n";
		os << "    { memcpy(this, &x.get(), sizeof(" << get_name() << ")); }\n\n";

		os << "    " << get_name() << " comet_in() const\n";
		os << "    { return *this; }\n";

		os << "    " << get_name() << "* comet_in_ptr() const\n";
		os << "    { return const_cast<" << get_name() << "*>(this); }\n";

		os << "    " << get_name() << "* comet_inout()\n";
		os << "    { return const_cast<" << get_name() << "*>(this); }\n";

		os << "    " << get_name() << "* comet_out()\n";
		os << "    { return const_cast<" << get_name() << "*>(this); }\n";

		os << "    static const " << get_name() << "& comet_create_const_reference(const " << get_name() << "& x)\n";
		os << "    { return x; }\n";

		os << "    static " << get_name() << "& comet_create_reference(" << get_name() << "& x)\n";
		os << "    { return x; }\n";

		os << "    static " << get_name() << " comet_detach(" << get_name() << "& x)\n";
		os << "    { return x; }\n";

		os << "};\ntypedef " << get_name() << " " << get_raw_name() << ";\n";

		}
		return os.str();
	}

	string get_sa_trait_specialisation() const
	{
		std::ostringstream os;

		os << std::endl;
		os << "    template<> struct sa_traits<" << get_name(true) << ">\n"
			  "    {\n"
			  "        typedef " << get_raw_name(true) << " raw;\n"
			  "        enum { vt = VT_RECORD };\n"
			  "        enum { check_type = stct_features_ok };\n"
			  "        enum { extras_type = stet_record };\n"
			  "        typedef " << get_name(true) << " value_type;\n"
			  "        typedef " << get_name(true) << "& reference;\n"
			  "        typedef const " << get_name(true) << "& const_reference;\n"
			  "        static reference create_reference(raw& x) { return *reinterpret_cast<" << get_name(true) << "*>(&x); }\n"
			  "        static const_reference create_const_reference(raw& x) { return *reinterpret_cast<const " << get_name(true) << "*>(&x); }\n"
			  "        typedef sa_iterator<" << get_name(true) << ", nonconst_traits<" << get_name(true) << "> > iterator;\n"
			  "        typedef sa_iterator<" << get_name(true) << ", const_traits<" << get_name(true) << "> > const_iterator;\n"
			  "        static bool are_features_ok(unsigned short f) { return (f & 0x1fff) == FADF_RECORD; }\n"
			  "        static com_ptr<IRecordInfo> get_record_info()\n"
			  "        {\n"
			  "          static IRecordInfo* ri = 0;\n"
			  "          if (!ri) impl::create_record_info(uuidof<" << nso.namespace_name() << "::type_library>(),\n"
			  "                                            uuidof<" << get_name(true) << ">(),\n"
			  "                                            " << nso.namespace_name() << "::type_library::major_version,\n"
			  "                                            " << nso.namespace_name() << "::type_library::minor_version,\n"
			  "                                            ri);\n"
			  "          return ri;\n"
			  "        }\n"
			  "    };\n";

		return os.str();
	}

	std::vector<string> get_dependents() const {
		std::vector<string> rv;
		for (RECORDMEMBERS::const_iterator it = recordMembers_.begin(); it != recordMembers_.end(); ++it) {
			string name;
			if ((*it)->get_type().is_struct_or_union(name)) rv.push_back(name);
		}
		return rv;
	}
	virtual void get_library_includes( std::set<string> &library_includes) const
	{
		for (RECORDMEMBERS::const_iterator it = recordMembers_.begin(); it != recordMembers_.end(); ++it) {
			(*it)->get_library_includes(library_includes);
		}
	}

private:
	void update() {
		MembersPtr ms = ri_->Members;
		for (int i=1; i<=ms->Count; ++i) {
			MemberInfoPtr mi = ms->Item[i];

			RecordMember *pRm = new RecordMember(mi, name_);

			recordMembers_.push_back(pRm);
		}
	}
};

class UnionMember {
	MemberInfoPtr mi_;
	Type type_;
	string name_;
public:
	UnionMember(const MemberInfoPtr& mi, const string &unionName) : mi_(mi) {
		update();
		name_ = nso.trans_interface_method(unionName, mi_->Name);
	}

	string get_name() {
		return name_;
	}

	const Type &get_type() const { return type_; }
	Type &get_type() { return type_; }

	void get_library_includes( std::set<string> &library_includes) const
	{
		type_.get_library_includes(library_includes);
	}
private:
	void update() {
		type_ = Type(mi_->ReturnType);
	}
};

class Union : public TypeItem {
	UnionInfoPtr ui_;

	typedef std::vector<UnionMember*> UNIONMEMBERS;
	UNIONMEMBERS unionMembers_;
public:
	Union(const UnionInfoPtr& ui) : ui_(ui) {
		update();
	}

	~Union() {
		delete_all(unionMembers_.begin(), unionMembers_.end());
	}

	string get_forward_decl() const {
		std::ostringstream os;
		os << "struct DECLSPEC_UUID(\"" << get_uuid() << "\") " << get_name() << ";\n";
		return os.str();
	}
	string get_uuid() const {
		string s = ui_->GUID;
		s = s.substr(1, s.size() - 2);
		return s;
	}

	string get_name(bool with_ns = false) const {
		if (with_ns)
		return nso.get_name_with_ns(ui_);
		return nso.trans_symbol(ui_->GetName());
	}

	string get_definition() const {
		std::ostringstream os;

		os << "union " << get_name() << " {\n";

		for (UNIONMEMBERS::const_iterator it = unionMembers_.begin(); it != unionMembers_.end(); ++it) {
			unsigned vsz;
			try {
				os << "    " << (*it)->get_type().as_raw(vsz) << " " << (*it)->get_name();
				if (vsz > 0) os << "[" << vsz << "]";
				os << ";\n";
			} CATCH_ALL("union member '" <<(*it)->get_name() <<"'"  )
		}

		os << "};\n";

		return os.str();
	}

	string get_sa_trait_specialisation() const
	{
		return "";
	}

	std::vector<string> get_dependents() const {
		std::vector<string> rv;
		for (UNIONMEMBERS::const_iterator it = unionMembers_.begin(); it != unionMembers_.end(); ++it) {
			try {
				string name;
				if ((*it)->get_type().is_struct_or_union(name)) rv.push_back(name);
			} CATCH_ALL("union member '" <<(*it)->get_name() <<"'"  )
		}
		return rv;
	}
	virtual void get_library_includes( std::set<string> &library_includes) const
	{
		for (UNIONMEMBERS::const_iterator it = unionMembers_.begin(); it != unionMembers_.end(); ++it) {
			(*it)->get_library_includes(library_includes);
		}
	}

private:
	void update() {
		std::string unionName = get_name(false);
		MembersPtr ms = ui_->Members;
		for (int i=1; i<=ms->Count; ++i) {
			MemberInfoPtr mi = ms->Item[i];

			UnionMember *pUm = new UnionMember(mi, unionName);

			unionMembers_.push_back(pUm);
		}
	}
};

class EnumMember {
	MemberInfoPtr mi_;
	string name_;
public:
	EnumMember(const MemberInfoPtr& mi) : mi_(mi) {
		update();
	}

	string get_name() {
		return name_;
	}

	unsigned long get_value() {
		return (unsigned long)(long) mi_->Value;
	}

private:
	void update() {
		// nothing
		name_ =nso.trans_symbol(static_cast<const char*>(mi_->Name));
	}
};

class Constant {
	MemberInfoPtr mi_;
public:
	Constant(const MemberInfoPtr& mi) : mi_(mi) {
	}

	string get_name(bool with_ns = true) {
		if (with_ns)
		return nso.get_name_with_ns(mi_);
		return nso.trans_symbol(mi_->GetName());
	}

	string get_value() {
		return static_cast<const char*>(_bstr_t(mi_->Value));
	}

	string get_definition() {
		std::ostringstream os;

		unsigned vsz;

		Type type(mi_->ReturnType);
		os << "static const " << type.as_raw(vsz) << " " << get_name(false) << " = ";
		os << type.default_value(mi_->Value) << ";\n";

		return os.str();

	}
};

class Enum {
	ConstantInfoPtr ci_;
	typedef std::vector<EnumMember*> ENUMMEMBERS;
	ENUMMEMBERS enumMembers_;
public:
	Enum(const ConstantInfoPtr& ci) : ci_(ci) {
		update();
	}

	~Enum() {
		delete_all(enumMembers_.begin(), enumMembers_.end());
	}

	string get_name(bool with_ns = true) {
		if (with_ns)
		return nso.get_name_with_ns(ci_);
		return nso.trans_symbol(ci_->GetName());
	}

	string get_definition() {
		std::ostringstream os;

		os << "enum " << get_name() << " {\n";

		bool first = true;
		for (ENUMMEMBERS::iterator it = enumMembers_.begin(); it != enumMembers_.end(); ++it) {
			if (first)
				first = false;
			else
				os << ",\n";
			os << "    " << (*it)->get_name() << " = " << (*it)->get_value();
		}

		os << "\n};\n";

		return os.str();
	}

	string get_sa_trait_specialisation() {
		std::ostringstream os;

		os << "    template<> struct sa_traits<" << get_name() << "> : public basic_sa_traits<" << get_name() << ", VT_I4> {};\n";

		return os.str();
	}
private:
	void update() {
		MembersPtr ms = ci_->Members;

		for (int i=1; i<=ms->Count; ++i) {
			MemberInfoPtr mi = ms->Item[i];

			EnumMember *pEM = new EnumMember(mi);

			enumMembers_.push_back(pEM);
		}
	}
};

class Alias {
	IntrinsicAliasInfoPtr ci_;
	Type type_;

	std::string name_;
public:
	Alias(const IntrinsicAliasInfoPtr& ci) : ci_(ci) {
		type_ = Type(ci->ResolvedType);
		name_ = nso.trans_symbol(ci->Name);
	}

	Alias(const ConstantInfoPtr& ci) {
		type_ = Type(ci->ResolvedType);
		name_ = nso.trans_symbol(ci->Name);
	}

	Alias(const InterfaceInfoPtr& ti) {
		type_ = Type(ti->ResolvedType);
		name_ = nso.trans_symbol(ti->Name);
	}

	void get_library_includes( std::set<string> &library_includes) const
	{
		type_.get_library_includes(library_includes);
	}


	string get_name()
	{
		unsigned vsz;
		return type_.as_raw(vsz);
	}
	string get_definition() {
		std::ostringstream os;

		unsigned vsz;
		string rawtype = type_.as_raw(vsz);
		if(rawtype == "::GUID")
			os << "// ";

		os << "typedef " << rawtype << " " << name_;
		if (vsz > 0) os << "[" << vsz << "]";
		os << ";\n";


		return os.str();
	}
private:
};


class Coclass {
	CoClassInfoPtr cci_;
public:
	Coclass(const CoClassInfoPtr& cci, TypeLibrary* tl)
		: cci_(cci)
	{
		update(tl);
	}

	string get_description()
	{
		_bstr_t s = cci_->GetHelpString( GetThreadLocale() );
		if (!s) return "";
		return (const char*)s;
	}

	void get_library_includes( std::set<string> &library_includes) const
	{
		InterfacesPtr itfs = cci_->Interfaces;
		for (int i=1; i<=itfs->Count; ++i) {
			InterfaceInfoPtr ipx = itfs->Item[i];
			InterfaceInfoPtr ip = ipx->VTableInterface;
			if (ip == NULL) ip=ipx;
			if (nso.get_name_with_ns(ip) == "::IDispatch") {
				library_includes.insert("comet/dispatch.h");
			}
		}
	}

	string get_name(bool with_ns = true) {
		if (with_ns)
		return nso.get_name_with_ns(cci_);
		return nso.trans_symbol(cci_->GetName());
	}

	string get_uuid() {
		string s = cci_->GUID;
		s = s.substr(1, s.size() - 2);
		return s;
	}

	// NOTE: MRG Not sure exactly
	bool is_source()
	{
		return ((cci_->GetAttributeMask() & ::VARFLAG_FSOURCE) != 0);
	}

	string get_definition(const string& type_library, bool server_mode) {
		std::ostringstream os;
		os << "struct DECLSPEC_UUID(\"" << get_uuid() << "\") " << get_name() << " {\n";

		os << "    typedef " ;

		output_typelist tos(os);

		InterfacesPtr itfs = cci_->Interfaces;
		for (int i=1; i<=itfs->Count; ++i)
		{
			InterfaceInfoPtr ipx = itfs->Item[i];
			InterfaceInfoPtr ip = ipx->VTableInterface;
			if (ip == NULL)
				ip=ipx;	// This is a Disp interfaces!

			// Ignore source interfaces
			if (ipx->GetAttributeMask() & 0x2) continue;

			string name = ip->Name;
			if (name == "IUnknown") {
			} else {
				tos << nso.get_name_with_ns(ip);
			}
		}
		tos.end();

		os << " interfaces;\n";


		bool has_source_interface = false;

		os << "    typedef ";
		for (int i=1; i<=itfs->Count; ++i)
		{
			InterfaceInfoPtr ipx = itfs->Item[i];
			InterfaceInfoPtr ip = ipx->VTableInterface;
			if (ip == NULL)
			{
				// Disp interfaces are NOT ignored!
				ip=ipx;
			}

			// Ignore non- source interfaces
			if (!(ipx->GetAttributeMask() & 0x2)) continue;

			tos << nso.get_name_with_ns(ip);
			has_source_interface = true;
		}
		tos.end();
		os << " source_interfaces;\n";

		if (server_mode) {
			os << "    typedef ";

			bool isFirst = true;
			for (int i=1; i<=itfs->Count; ++i)
			{
				InterfaceInfoPtr ipx = itfs->Item[i];
				InterfaceInfoPtr ip = ipx->VTableInterface;
				if (ip == NULL)
				{
					// Disp interfaces are NOT ignored!
					ip=ipx;
				}

				// Treat source interfaces different
				if (ipx->GetAttributeMask() & 0x2) {
				}
				else {
					string name = ip->Name;
					if (name == "IUnknown") {
						// do nothing
					} else {
						std::ostringstream elts;
						std::string ifname =nso.get_name_with_ns(ip);
						if (ifname == "::IDispatch")
						{
							elts << "dynamic_dispatch< coclass_implementation<" << get_name() << "> > ";
						}
						else
						{
							elts << ifname <<  "Impl< coclass_implementation<" << get_name() <<  "> "
								", " << ifname << ", " << type_library << "_type_library" 
								" > ";
						}
						tos << elts.str();
					}
				}
			}
			if(has_source_interface)
			{
				tos << "implement_cpc<  source_interfaces > ";
				isFirst = false;
			}

			tos.end();
			os << "interface_impls;\n";

			os << "    typedef " << nso.namespace_name() << "_type_library type_library;\n";
		}

		os << "    static const TCHAR* name() { return _T(\"" << get_name() << "\"); }\n";

		os << "    enum { major_version = " << cci_->MajorVersion << ", minor_version = " << cci_->MinorVersion << " };\n";

		std::string default_interface = nso.get_name_with_ns(cci_->DefaultInterface);

		os << "    static com_ptr<" << default_interface << "> create() {return com_ptr<" << default_interface << ">(uuidof<" << get_name() << ">()); };\n";
		os << "};\n";

		os << std::endl;
		return os.str();
	}

	string get_stub(const string& type_library)
	{
		bool aggregateable = (0!=(cci_->AttributeMask & ::TYPEFLAG_FAGGREGATABLE ));
		bool createable = (0!=(cci_->AttributeMask & ::TYPEFLAG_FCANCREATE ));


		typedef std::list< Interface* > INTFS;
		INTFS interfaces;

		bool isFirst = true;
		bool has_source = false;
		InterfacesPtr itfs = cci_->Interfaces;
		for (int i=1; i<= itfs->Count; ++i)
		{
			InterfaceInfoPtr ipx = itfs->Item[i];

			bool inherits = false;

			// Treat source interfaces different
			if (ipx->GetAttributeMask() & 0x2)
			{
				has_source=true;
			}
			else
			{
				do
				{
					InterfaceInfoPtr ip = ipx->VTableInterface;
					Interface* pItf;
					if (ip == NULL)// dispinterface
					{
						pItf = new Interface(ipx, true);
					}
					else
					{
						pItf = new Interface(ip, false);
					}

					interfaces.push_back( pItf );

					ipx = pItf->get_base_interface( true );
					inherits = (ipx != NULL);
				}
				while( inherits);
			}
		}

		std::stringstream os;
		// Comments
		_bstr_t help =  cci_->GetHelpString(LOCALE_SYSTEM_DEFAULT);
		if(help.length()!=0)
		{
			os << "// " << (const char *)help << "\n//\n";
		}

		if(createable)
		{
			os  << "template<>\n"
				<< "class coclass_implementation<" << /*this_ns <<*/  get_name() << ">\n"
				<< "\t: public " <<(aggregateable?"aggregateable_":"") <<
					"coclass<"<<  /*this_ns <<*/   get_name() << ">\n";
		}
		else
		{
			std::string name = "coclass_";
			name += get_name();
			os  << "class " << name << '\n';
			if(aggregateable)
				os << "\t: public aggregateable_object<\n";
			else
				os << "\t: public simple_object<\n";

			bool first = true;
			for(INTFS::const_iterator it = interfaces.begin(); it != interfaces.end(); ++it)
			{
				if(!first) os << ",\n";
				else first=false;

				os << "\t\t" << (*it)->get_name() << "Impl<" << name << ">";
			}
			if( has_source )
			{
				if(!first) os << ",\n";
				else first=false;

				os << "\t\timplement_cpc<" <<  get_name() << "::source_interfaces >";
			}
			if(!first) os << ",\n";
			else first=false;
			os << "\t\tIProvideClassInfoImpl<" << get_name() << ">\n\t>\n";
		}
		os	<< "{\npublic:\n";

		bool first=true;
		for(INTFS::const_iterator it = interfaces.begin(); it != interfaces.end(); ++it)
		{
			try {
				std::string name =(*it)->get_name();
				if(first) first=false;
				else os << '\n';
				if (name == "IUnknown")
				{
					// do nothing
				}
				else
				{

					Interface *Itf = (*it);

					os << "\t// Interface " << name << "\n\t//\n\n"
						<< (*it)->get_wrapper_decls( "\t", false, true, true);
				}
			}
			CATCH_ALL("interface '" <<(*it)->get_name() <<"'"  )
		}

		os	<< "};\n\n" ;

		delete_all(interfaces.begin(), interfaces.end());

		return os.str();
	}

	void check_referenced_interfaces(require_interface *req_cb)
	{
		InterfacesPtr itfs = cci_->Interfaces;
		for (int i=1; i<=itfs->Count; ++i)
		{
			InterfaceInfoPtr ipx = itfs->Item[i];
			InterfaceInfoPtr ip = ipx->VTableInterface;
			if (ip == NULL)
			{
				// Disp interfaces are NOT ignored!
				ip=ipx;
			}

			string name = (const char *)ip->Name;
			if ((name != "IUnknown") && (name != "IDispatch"))
			{
				(*req_cb)(nso.trans_symbol(name));
			}
		}
	}

private:
	void update(TypeLibrary* tl);
};

class LibraryOptions
{
	typedef std::set<string> SYMBOLS;
	SYMBOLS symbols_;
	enum SymbolOption
	{
		so_server_wrapper=0x1,
		so_stub_only=0x2
	};
	typedef std::map<string,long> OPTIONS;
	OPTIONS extra_options_;
	bool exclude_; // Exclusive/ inclusive
	bool server_mode_;
	bool nutshell_mode_;
	bool output_is_directory_;
	bool wizard_mode_;
	bool use_namespaces_;
	bool out_html_;
	bool out_h_;
	bool no_foreign_;
	bool pare_symbols_;
	bool output_library_;
	char wizard_type_;
	std::string outputfile_;

	std::string namespace_override_;
	std::string library_name_;

	public:
		LibraryOptions()
			:exclude_(true), server_mode_(true), nutshell_mode_(false),
			output_is_directory_(true), wizard_mode_(false), use_namespaces_(false),
			out_h_(true), out_html_(true), no_foreign_(false), pare_symbols_(false), wizard_type_('a'), output_library_(false)
		{
		}

		void set_library_name( const std::string &library_name ) { library_name_ = library_name; }
		void set_namespace_override(const std::string &namespaces_name ) { namespace_override_ = namespaces_name; }

		std::string get_namespace_override() const
		{
			return namespace_override_;
		}
		std::string get_library_name() const
		{
			return library_name_;
		}

		void output_library(bool lib) { output_library_ = lib; }
		bool output_library()const { return output_library_; }

		void server_mode(bool server_mode){ server_mode_ = server_mode; }
		bool server_mode() const { return server_mode_; }
		void nutshell_mode(bool nutshell_mode){ nutshell_mode_ = nutshell_mode; }
		bool nutshell_mode() const { return nutshell_mode_; }
		void output_is_directory(bool output_is_directory){ output_is_directory_ = output_is_directory; }
		bool output_is_directory() const { return output_is_directory_; }
		void wizard_mode(bool wizard_mode, char wizard_type)
		{ wizard_mode_ = wizard_mode; wizard_type_ = wizard_type;}
		bool wizard_mode() const { return wizard_mode_;}
		char wizard_type() const { return wizard_type_;}
		void use_namespaces(bool usenamespaces) { use_namespaces_ = usenamespaces; }
		bool use_namespaces() const { return use_namespaces_;}
		void output_html( bool usehtml){ out_html_ = usehtml;}
		bool output_html() const { return out_html_; }
		void output_h( bool useh){ out_h_ = useh;}
		bool output_h() const { return out_h_; }
		void exclude_foreign(bool exc){ no_foreign_= exc; }
		bool exclude_foreign(){ return no_foreign_;}

		bool pare_symbols() const { return pare_symbols_ ;}
		void pare_symbols(bool newval) { pare_symbols_ =newval;}


		void set_output_file( const std::string &filename, bool is_directory = 1)
		{
			outputfile_ = filename;
			output_is_directory_ = is_directory;
		}

		std::string make_output_filename(const char* ext)
		{

			if(output_is_directory_)
			{
				std::string fn=outputfile_;
				if( !fn.empty() )
				{
					char last = fn[fn.length()-1];
					if(last != '\\' && last != '/')
					{
						fn += '\\';
					}
				}
				fn +=  get_library_name() + ext;
				return  fn;
			}
			return outputfile_;
		}

		bool has_exclude() const
		{
			if( !exclude_) return true;
			return !symbols_.empty();
		}

		bool expose_symbol(const std::string &symbol) const
		{
			return ((symbols_.count(symbol) > 0) ? !exclude_:exclude_ );
		}

		bool server_wrapper_for(const std::string &symbol) const
		{
			OPTIONS::const_iterator it=extra_options_.find(symbol);
			if ( it== extra_options_.end()) return false;
			return ((*it).second & so_server_wrapper)!=0;
		}
		bool stub_only_for(const std::string &symbol) const
		{
			OPTIONS::const_iterator it=extra_options_.find(symbol);
			if ( it== extra_options_.end()) return false;
			return ((*it).second & so_stub_only)!=0;
		}

		void include_symbol(const std::string &symbol)
		{
			if(exclude_)
				symbols_.erase(symbol);
			else
				symbols_.insert(symbol);
		}
		void exclude_symbol(const std::string &symbol)
		{
			if(!exclude_)
				symbols_.erase(symbol);
			else
				symbols_.insert(symbol);
		}
		void parse_symbols( const std::string &arg)
		{
			exclude_ = false;
			for( std::string::size_type mark = arg.find_first_not_of(" \t,:");
					mark != std::string::npos;
					mark = arg.find_first_not_of(" \t,:",mark ))
			{
				std::string::size_type pos  = arg.find_first_of(" \t,:",mark);
				std::string newval( arg, mark, pos-mark);
				if(!newval.empty())
				{
					symbols_.insert(newval);
					//std::cout << "Found: " << newval.c_str() << "\n";
				}
				mark = pos;
			}

		}
		void load_symbols(const std::string &filename, bool exclude= false)
		{
			exclude_ = exclude;
			symbols_.clear();
			std::ifstream in(filename.c_str());
			if(in)
			{
				while (!in.eof())
				{
					std::string newval;
					std::getline(in, newval);
					std::string::size_type pos = newval.find_last_not_of(" \t");
					newval.replace(pos+1,std::string::npos,"");
					if(! newval.empty() && newval[0]!=';' && newval[0]!='#')
					{
						char ch =newval[0];
						if( ch == '*' || ch == '~')
						{
							newval.replace(0,1,"");
							if(!exclude)
								symbols_.insert(newval);
							OPTIONS::iterator it = extra_options_.insert(OPTIONS::value_type(newval,0)).first;
							(*it).second = (ch =='*'?so_server_wrapper:so_stub_only);
						}
						else
						{
							symbols_.insert(newval);
						}
					}
				}
			}
			else
			{
				std::ostringstream em;
				em << "Unable to open symbol file: '" << filename << "'";
				throw std::runtime_error(em.str());
			}

		}
		void dump_symbols()
		{
			std::cout << "Dump:\n";
			for( SYMBOLS::const_iterator it = symbols_.begin(); it != symbols_.end(); ++it)
			{
				std::cout << (*it).c_str() << '\n';
			}
			std::cout << "------------\n";
		}
};


class TypeLibrary {

	_TLIApplicationPtr app_;
	_TypeLibInfoPtr tli_;

	typedef std::map<string, Interface*> INTERFACES;
	INTERFACES interfaces_;
	bool has_server_wrappers_;

	std::set<string> source_interfaces_;

	typedef std::list<Coclass*> COCLASSES;
	COCLASSES coclasses_;

	typedef std::map<string, TypeItem*> TYPEITEMS;
	TYPEITEMS type_items_;

	typedef std::list<Alias*> ALIASES;
	ALIASES aliases_;

	typedef std::list<Constant*> CONSTANTS;
	CONSTANTS constants_;

	typedef std::list<Enum*> ENUMS;
	ENUMS enums_;

	LibraryOptions &options_;
public:
	TypeLibrary( LibraryOptions &options) : options_(options), has_server_wrappers_(false)
	{
		app_.CreateInstance(__uuidof(TLIApplication));
		app_->ResolveAliases = false;
		has_server_wrappers_=options_.server_mode();
	}

	~TypeLibrary() {
		cleanup();
	}

	bool has_server_wrappers(){ return has_server_wrappers_; }
	void has_server_wrappers( bool newval){ has_server_wrappers_ = newval; }

	void add_source_interface(const InterfaceInfoPtr& ip)
	{
		source_interfaces_.insert(nso.trans_symbol(ip->Name));
		insert_interface( ip, true ); //< Make sure it is there!
	}

	int major_version()
	{
		return tli_->MajorVersion;
	}

	int minor_version()
	{
		return tli_->MinorVersion;
	}

	// Check to see if the type is from a different type-library
	bool is_foreign_type( const TypeInfoPtr& ti)
	{
		return (ti->Parent != tli_) && (ti->Parent->Name != tli_->Name);
	}

	string get_definition()
	{
		std::ostringstream os;

		os << "struct " << get_name() << "_type_library {\n";

		os << "    enum { major_version = " << major_version() << ", minor_version = " << minor_version() << " };\n";

		os << "    typedef ";


		output_typelist tos(os);

		for( COCLASSES::const_iterator it = coclasses_.begin(); it != coclasses_.end(); ++it)
		{
			if( (*it)->is_source())
			{
				string name =(*it)->get_name();
				tos << name;
			}
		}

		tos.end();
		os << " coclasses;\n";

		os << "};\n\n";

		return os.str();
	}

	void load(const string&fn) {
		tli_ = app_->TypeLibInfoFromFile(fn.c_str());
	}

	void process()
	{
		update();
	}

	string get_name() const {
		return nso.foreign_namespace(get_raw_name());
	}

	string get_raw_name() const {
		return nso.trans_symbol(tli_->Name);
	}

	string get_uuid() const {
		string s = tli_->GUID;
		s = s.substr(1, s.size() - 2);
		return s;
	}

	string get_interface_html_list()
	{
		std::ostringstream os;
		os << "<table>\n";
		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it)
		{
			try{
				if( !it->second->implement_iface() ) continue;

				std::string name = it->second->get_name();
				os << "<tr><td>";
				if(name[0]!=':' && !it->second->only_stub_iface())
				{
					std::string href = name;
					std::string::size_type where = href.find("::");
					if(where != std::string::npos)
					{
						href.replace(where,2,".htm#");
					}
					else
					{
						href.insert(0,"#");
					}

					os << "<a href=\"" << href << "\">" << name << "</a>";
				}
				else
				{
					os << name;
				}
				os << "</td><td><i>" << it->second->get_description() << "</i></td></tr>\n";
			}
			CATCH_ALL("interface '" <<(*it).second->get_name() <<"'"  )
		}
		os << "</table>\n";
		return os.str();
	};

	string get_interface_html_detail()
	{
		std::ostringstream os;
		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it)
		{
			try{
				if( it->second->implement_iface() && !it->second->only_stub_iface())
				{
					os << "<hr>\n";
					os << it->second->get_html_detail();
				}
			} CATCH_ALL("interface '" <<(*it).second->get_name() <<"'"  )
		}
		return os.str();
	}

	string get_coclass_html_list()
	{
		std::ostringstream os;
		os << "<table>\n";
		for (COCLASSES::iterator it = coclasses_.begin(); it != coclasses_.end(); ++it)
		{
			os << "<tr><td>" << (*it)->get_name() << "</td><td><i>" << (*it)->get_description() << "</i></td></tr>\n";
		}
		os << "</table>\n";
		return os.str();
	};

	string get_interface_decls( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		typedef std::stack<Interface*> INTERFACE_STACK;
		INTERFACE_STACK interfaces_todo;

		typedef std::set<string> SET;
		SET interfaces_done;

		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it) {
			if (interfaces_done.find(it->first) == interfaces_done.end()) {
				Interface *pI = it->second;

				if( pI->implement_iface() )
				{
					interfaces_todo.push(pI);
					while (interfaces_done.find(pI->get_base_name()) == interfaces_done.end()) {
						INTERFACES::iterator it2 = interfaces_.find(pI->get_base_name());
						if (it2 != interfaces_.end()) {
							pI = it2->second;
							interfaces_todo.push(pI);
						} else
							break;
					}

					while (!interfaces_todo.empty()) {
						Interface *pI = interfaces_todo.top();
						try {
							od(os);
							os << pI->get_decl()  << '\n';
							interfaces_done.insert(pI->get_name());
							interfaces_todo.pop();
						}
						CATCH_ALL("interface '" <<pI->get_name() <<"'"  )
					}
				}
			}
		}

		return os.str();
	}

	string get_wrapper_decls( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		typedef std::stack<Interface*> INTERFACE_STACK;
		INTERFACE_STACK interfaces_todo;

		typedef std::set<string> SET;
		SET interfaces_done;

		nso.enter_namespace();

		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it) {
			if (interfaces_done.find(it->first) == interfaces_done.end()) {
				Interface *pI = it->second;
				if( pI->implement_iface() )
				{
					interfaces_todo.push(pI);
					while (interfaces_done.find(pI->get_base_name()) == interfaces_done.end()) {
						INTERFACES::iterator it2 = interfaces_.find(pI->get_base_name());
						if (it2 != interfaces_.end()) {
							pI = it2->second;
							interfaces_todo.push(pI);
						} else
							break;
					}

					while (!interfaces_todo.empty()) {
						Interface *pI = interfaces_todo.top();
						try {
							nso.leave_namespace();

							od(os);
							os << pI->get_wrapper_decl() << '\n';

							nso.enter_namespace();
							interfaces_done.insert(pI->get_name());
							interfaces_todo.pop();
						} CATCH_ALL("interface '" <<pI->get_name() <<"'"  )
					}
				}
			}
		}

		nso.leave_namespace();

		return os.str();
	}

	string get_interface_prop_wrappers() {
		string s;

		typedef std::stack<Interface*> INTERFACE_STACK;
		INTERFACE_STACK interfaces_todo;

		typedef std::set<string> SET;
		SET interfaces_done;

		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it) {
			Interface *pI = it->second;
			try {
				if( pI->implement_iface())
				{
					s += pI->get_prop_wrapper()  + '\n';
				}
			}	CATCH_ALL("interface '" <<pI->get_name() <<"'"  )
		}

		return s;
	}

	string get_interface_forward_decls( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it)
		{
			Interface *pI = it->second;
			try {
				if( pI->implement_iface())
				{
					od(os);
					os << pI->get_forward_decl();
				}
			} CATCH_ALL("interface '" <<pI->get_name() <<"'"  )
		}

		for (TYPEITEMS::iterator it = type_items_.begin(); it != type_items_.end(); ++it)
		{
			try{
			od(os);
			os << it->second->get_forward_decl();
			} CATCH_ALL("type '" <<it->second->get_name() <<"'"  )
		}
		if( od.did_output()) os << '\n';

		return os.str();
	}

	string get_interface_typedefs( const std::string & desc)
	{
		oneoff_output od(desc);
		std::ostringstream os;

		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it)
		{
			Interface *pI = it->second;
			if( pI->implement_iface())
			{
				try {
					od(os);
					os << pI->get_typedef();
				} CATCH_ALL("interface '" <<pI->get_name() <<"'"  )
			}
		}
		if( od.did_output()) os << '\n';

		return os.str();
	}

	string get_interface_wrappers( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it)
		{
			Interface *pI = it->second;
			if( pI->implement_iface() && !pI->only_stub_iface())
			{
				try {
					od(os);
					DEBUG(1, it->second->get_name() << std::endl);
					os << pI->get_wrappers();
				} CATCH_ALL("interface '" <<pI->get_name() <<"'"  )
			}
		}
		return os.str();
	}

	string get_interface_impl_decl( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it) {
			Interface *pI = it->second;
			if( pI->do_server_wrapper())
			{
				try {
					od(os);
					os << pI->get_impl_decl() << '\n';
				} CATCH_ALL("interface '" <<pI->get_name() <<"'"  )
			}
		}

		return os.str();
	}

	string get_interface_nutshell_decl( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it) {
			Interface *pI = it->second;
			if( pI->implement_iface())
			{
				try {
					od(os);
					os << pI->get_nutshell_decl(get_name()) << '\n';
				} CATCH_ALL("nutshell decl for interface '" <<pI->get_name() <<"'"  )
			}
		}

		return os.str();
	}

	string get_interface_impl_forward_decl( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it) {
			Interface *pI = it->second;
			if( pI->implement_iface())
			{
				try {
					od(os);
					os << pI->get_impl_forward_decl(get_name()) << '\n';
				} CATCH_ALL("forward decl for interface '" <<pI->get_name() <<"'"  )
			}
		}

		return os.str();
	}

	string get_interface_nutshell_forward_decl( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it) {
			Interface *pI = it->second;
			if( pI->implement_iface())
			{
				try {
					od(os);
					os << pI->get_nutshell_forward_decl() << '\n';
				} CATCH_ALL("interface '" <<pI->get_name() <<"'"  )
			}
		}

		return os.str();
	}

	string get_interface_impl( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it) {
			Interface *pI = it->second;
			if( pI->do_server_wrapper())
			{
				try {
					od(os);
					os << pI->get_impl();
				} CATCH_ALL("implementation of interface '" <<pI->get_name() <<"'"  )
			}
		}

		return os.str();
	}

	string get_interface_nutshell( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it) {
			Interface *pI = it->second;
			if( pI->implement_iface())
			{
				try {
					od(os);
					os << pI->get_nutshell();
				} CATCH_ALL("nutshell implementation of interface '" <<pI->get_name() <<"'"  )
			}
		}

		return os.str();
	}

	string get_coclass_defs( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		for (COCLASSES::iterator it = coclasses_.begin(); it != coclasses_.end(); ++it)
		{
			try {
				od(os);
				os << (*it)->get_definition(get_name(),options_.server_mode());
			} CATCH_ALL("coclass '" <<(*it)->get_name() <<"'"  )
		}

		return os.str();
	}
	string get_coclass_stubs( const std::string & desc) {
		oneoff_output od(desc);
		std::stringstream s;
		for (COCLASSES::iterator it = coclasses_.begin(); it != coclasses_.end(); ++it)
		{
			try {
				od(s);
				s << (*it)->get_stub( get_name() );
			} CATCH_ALL("coclass '" <<(*it)->get_name() <<"'"  )
		}

		return s.str();
	}
	string get_interface_stubs(const string &desc) 
	{
		oneoff_output od(desc);
		std::stringstream s;
		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it) 
		{
			try {
				od(s);
				if ( it->second->implement() )
					s << it->second->get_stub();
			} CATCH_ALL("interface '" << it->second->get_name() <<"'"  )
		}
		return s.str();
	}
	string get_coclass_names()
	{
		std::ostringstream os;
		for (COCLASSES::iterator it = coclasses_.begin(); it != coclasses_.end(); ++it)
		{
//			try {
				os << (*it)->get_name() << '\n';
//			} CATCH_ALL( "coclass '" <<(*it)->get_name() <<"'"  )
		}
		return os.str();
	}
	string get_interface_names()
	{
		std::ostringstream os;
		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it) 
		{
			os << it->second->get_name() << '\n';
		}
		return os.str();
	}


	string get_connection_point_impls(const std::string &desc)
	{
		oneoff_output od(desc);
		std::stringstream os;

		for (std::set<string>::iterator it = source_interfaces_.begin(); it != source_interfaces_.end(); ++it)
		{
			Interface* itf = interfaces_[*it];
			if( !itf->is_foreign() && itf->implement() )
			{
				try {
					od(os);
					os << itf->get_connection_point_impl();
				} CATCH_ALL("connection points for interface '" <<itf->get_name() <<"'"  )
			}
		}
		return os.str();
	}

	void build_item_defs(std::ostream& s, std::set<string>& items_done, std::set<string>& items_in_stack, TypeItem* item)
	{
		std::vector<string> subs = item->get_dependents();
		for (std::vector<string>::iterator it = subs.begin(); it != subs.end(); ++it)
		{
			if (items_done.find(*it) == items_done.end())
			{
				if (type_items_.find(*it) != type_items_.end())
				{
					// not found in the done items, but may be already in stack
					// break the circular reference here
					if (items_in_stack.find(*it) != items_in_stack.end())
					{
						items_in_stack.insert(*it);
						build_item_defs(s, items_done, items_in_stack, type_items_.find(*it)->second);
					}
				}
			}
		}

		items_done.insert(item->get_name());
		s << item->get_definition() << '\n';
	};

	string get_item_defs( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		typedef std::set<string> SET;
		SET items_done;

		for (TYPEITEMS::iterator it = type_items_.begin(); it != type_items_.end(); ++it) {
			if (items_done.find(it->first) == items_done.end()) {
				try {
					od(os);
					SET items_in_stack;
					build_item_defs(os, items_done, items_in_stack, it->second);
				} CATCH_ALL("type '" <<(*it).second->get_name() <<"'"  )
			}
		}

		return os.str();
	}

	string get_alias_defs( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		for (ALIASES::iterator it = aliases_.begin(); it != aliases_.end(); ++it)
		{
			try {
				od(os);
				os << (*it)->get_definition();
			} CATCH_ALL("alias '" <<(*it)->get_name() <<"'"  )
		}

		return os.str();
	}

	string get_constant_defs( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		for (CONSTANTS::iterator it = constants_.begin(); it != constants_.end(); ++it)
		{
			try {
				od(os);
				os << (*it)->get_definition();
			} CATCH_ALL("constant '" <<(*it)->get_name() <<"'"  )
		}

		return os.str();
	}

	string get_enum_defs( const std::string & desc) {
		oneoff_output od(desc);
		std::ostringstream os;

		for (ENUMS::iterator it = enums_.begin(); it != enums_.end(); ++it)
		{
			try {
				od(os);
				os << (*it)->get_definition() + '\n';
			} CATCH_ALL("enum '" <<(*it)->get_name() <<"'"  )
		}

		return os.str();
	}

	string get_enum_sa_trait_specialisations( const std::string & desc) {
		oneoff_output od(desc + "namespace impl {\n");
		std::ostringstream os;


		for (ENUMS::iterator it = enums_.begin(); it != enums_.end(); ++it)
		{
			try {
				od(os);
				os << (*it)->get_sa_trait_specialisation();
			} CATCH_ALL("enum '" <<(*it)->get_name() <<"'"  )
		}

		for (TYPEITEMS::iterator it = type_items_.begin(); it != type_items_.end(); ++it)
		{
			od(os);
			os << it->second->get_sa_trait_specialisation();
		}
		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it)
		{
			Interface *pI = it->second;
			if( pI->implement_iface() && !pI->only_stub_iface())
			{
				od(os);
				os << pI->get_sa_trait_specialisation();
			}
		}

		if(od.did_output())
			os << "} // namespace\n";
		return os.str();
	};

	string make_uuid_of(const string& iid, const string& name, const string& base, bool is_interface, bool add_ns = true)
	{
		std::ostringstream os;

		if (add_ns)
			os << "template<> struct comtype<" << nso.namespace_name() << "::" << name << "> {\n";
		else
			os << "template<> struct comtype<" << name << "> {\n";
		os << "    static const IID& uuid()\n"
			  "    { static const GUID iid = " << make_guid(iid) << "; return iid; }\n"
			  "    typedef " << base << " base;\n";
		if (is_interface && (options_.server_mode() || options_.nutshell_mode()))
		{
			os << "    template<typename B> struct implementation {\n";
			if (options_.server_mode())
				os << "        typedef " << name << "Impl<B> normal;\n";
			if (options_.nutshell_mode())
				os << "        typedef " << name << "Nutshell<B> nutshell;\n";
			os << "    };\n";
//			if ( do_log_interfaces )
//				os << "     static const TCHAR *name(){ return _T(\"" << name << "\");}\n";
		}
		os << "};\n\n";

		return os.str();
	}

	string get_named_guids() {
		std::ostringstream s;

		s << make_uuid_of(get_uuid(), get_name() + "_type_library", "nil", false);

		for (COCLASSES::iterator it = coclasses_.begin(); it != coclasses_.end(); ++it) {
			try {
				s << make_uuid_of((*it)->get_uuid(), (*it)->get_name(), "nil", false, false);
			} CATCH_ALL(" generating uuid of coclass '" <<(*it)->get_name() <<"'"  )
		}

		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it)
		{
			Interface *pI = it->second;
			if( pI->implement_iface())
			{
				try {
					s << make_uuid_of(it->second->get_uuid(), pI->get_name(), pI->get_base_name(),
							!pI->only_stub_iface() , false);
				} CATCH_ALL("interface '" <<pI->get_name() <<"'"  )
			}
		}

		for (TYPEITEMS::iterator it = type_items_.begin(); it != type_items_.end(); ++it) {
			s << make_uuid_of(it->second->get_uuid(), it->second->get_name(true), "nil", false , false);
		}

		return s.str();
	}
	string get_library_includes()
	{
		std::set<string> library_includes;
		library_includes.insert("comet/tstring.h");

		if (do_log_interfaces)
			library_includes.insert("comet/calllog.h");

		for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it)
		{
			Interface *itf = (*it).second;
			if( itf->implement_iface() || itf->implement())
				(*it).second->get_library_includes( library_includes);
		}
		if (options_.server_mode())	for (COCLASSES::iterator it = coclasses_.begin(); it != coclasses_.end(); ++it)
		{
			(*it)->get_library_includes( library_includes );
		}
		for (std::set<string>::iterator it = source_interfaces_.begin() ; it != source_interfaces_.end(); ++it)
		{
			// Make sure one of the source-interfaces is being implemented.
			Interface* itf = interfaces_[*it];
			if( !itf->is_foreign() && itf->implement() )
			{
				library_includes.insert("comet/cp.h");
				if( !itf->only_stub_iface() )
				{
					library_includes.insert("comet/cptraits.h");
					break;
				}
			}
		}
		for (TYPEITEMS::iterator it = type_items_.begin(); it != type_items_.end(); ++it)
		{
			it->second->get_library_includes( library_includes);

		}
		for (ALIASES::iterator it = aliases_.begin(); it != aliases_.end(); ++it)
		{
			(*it)->get_library_includes( library_includes);
		}
		if (options_.nutshell_mode())
		{
			library_includes.insert("comet/type_traits.h");
		}

		std::stringstream os;
		for( std::set<string>::const_iterator it = library_includes.begin(); it != library_includes.end(); ++it)
		{
			os << "#include <" << (*it) << ">\n";
		}
		return os.str();
	}

	struct do_require_interface : require_interface
	{
		do_require_interface( TypeLibrary *self ) : self_(self){}
		void operator()( const std::string &name)
		{
			self_->require_stub(name);
		}
		TypeLibrary *self_;
	};

	void require_stub( const std::string &name)
	{
		INTERFACES::iterator it = interfaces_.find(name);
		if( it != interfaces_.end() )
		{
			Interface *itf = (*it).second;
			if(! itf->implement())
			{
				itf->do_stub( true);
				do_require_interface callback(this);
				itf->check_referenced_interfaces(&callback, true ) ; // Check inheritance heirarchy
			}
		}
	}

	void check_referenced_interfaces()
	{
		if( options_.has_exclude() && options_.pare_symbols())
		{
			do_require_interface callback(this);

			for (INTERFACES::iterator it = interfaces_.begin(); it != interfaces_.end(); ++it)
			{
				Interface *itf = (*it).second;
				if( itf->implement())
				{
					itf->check_referenced_interfaces( &callback );
				}
			}
			for (COCLASSES::iterator it = coclasses_.begin(); it != coclasses_.end(); ++it)
			{
				(*it)->check_referenced_interfaces( &callback);
			}
		}
	}
	std::string get_wrapper() const
	{
		std::ostringstream s;
		s << "#include \"" << get_raw_name() << ".h\"\n\n"
		"using namespace comet;\n\n"
		"typedef com_server< " << get_raw_name() << "::type_library > SERVER;\n\n"
		"// Declare and implement standard COM DLL entry points.\n"
		"// These forward the DLL calls to a static methods on SERVER:\n"
		"COMET_DECLARE_DLL_FUNCTIONS(SERVER)\n";
		return s.str();
	}

private:
	void cleanup() {
		delete_all_2nd(interfaces_.begin(), interfaces_.end());
		interfaces_.clear();

		delete_all(coclasses_.begin(), coclasses_.end());
		coclasses_.clear();

		delete_all_2nd(type_items_.begin(), type_items_.end());
		type_items_.clear();

		delete_all(aliases_.begin(), aliases_.end());
		aliases_.clear();

		delete_all(enums_.begin(), enums_.end());
		enums_.clear();
	}

	void insert_interface( const InterfaceInfoPtr &ip, bool is_source=false)
	{
		InterfaceInfoPtr vip = ip->VTableInterface;
		std::auto_ptr<Interface> pItf;
		bool dispinterface;
		if (vip == NULL)
		{
			vip=ip;
			dispinterface=true;
		}
		else
		{
			dispinterface= false;
		}
		pItf = std::auto_ptr<Interface>(new Interface(vip, dispinterface));

		std::string name =pItf->get_name(false);

		pItf->is_foreign(is_foreign_type(vip));

		if(!options_.expose_symbol(name)|| (pItf->is_foreign() && options_.exclude_foreign())  )
		{
			pItf->dont_implement();
		}
		if( pItf->implement() )
		{
			if( options_.server_mode() ||  is_source
					|| options_.server_wrapper_for( name) )
			{
				if( !is_source || !pItf->is_foreign() )
				{
					pItf->do_server_wrapper(true);
					has_server_wrappers(true);
				}
			}
			else if(  options_.stub_only_for( name) )
			{
				pItf->do_stub( true );
			}
		}
		else if( !options_.pare_symbols() )
		{
			pItf->do_stub(true);
		}
		interfaces_[name] = pItf.release();
	}

	void update() {
		InterfacesPtr itfs = tli_->Interfaces;

//		if(!options_.wizard_mode())
		{
			for (int i=1; i<=itfs->Count; ++i)
			{
				if (itfs->Item[i]->TypeKind == TLI::TKIND_ALIAS)
				{
					Alias *pA = new Alias(itfs->Item[i]);
					aliases_.push_back(pA);
				} else {
					try {
						insert_interface( itfs->Item[i]);
					} catch (const std::exception& err)
					{
						std::cout << "  " << err.what() << std::endl;
					}
				}
			}
		}

		CoClassesPtr cocs = tli_->CoClasses;
		for (int i=1; i<=cocs->Count; ++i)
		{
			CoClassInfoPtr ci = cocs->Item[i];

			std::auto_ptr<Coclass> pCls ( new Coclass(ci, this) );

			if(options_.expose_symbol(pCls->get_name(false)))
			{
				coclasses_.push_back(pCls.release());
			}
		}

		if(!options_.wizard_mode())
		{
			RecordsPtr recs = tli_->Records;
			for (int i=1; i<=recs->Count; ++i)
			{
				RecordInfoPtr ri = recs->Item[i];

				if(ri->TypeKind == TLI::TKIND_ALIAS )
				{
					ConstantInfoPtr pi = static_cast<ConstantInfo *>(static_cast<RecordInfo *>(ri));
					Alias *pA = new Alias(pi);
					aliases_.push_back(pA);
				}
				else
				{
					std::auto_ptr<Record> pRec ( new Record(ri) );

					std::string name = pRec->get_name();
					if( options_.expose_symbol(name))
					{
						type_items_.insert( std::make_pair(name , pRec.release() ) );
					}
				}
			}

			UnionsPtr unis = tli_->Unions;
			for (int i=1; i<=unis->Count; ++i)
			{
				UnionInfoPtr ui = unis->Item[i];

				std::auto_ptr<Union> pRec ( new Union(ui) );

				std::string name = pRec->get_name();
				if( options_.expose_symbol(name))
				{
					type_items_.insert( std::make_pair( name, pRec.release() ) );
				}
			}

			DeclarationsPtr decs = tli_->Declarations;
			for (int i=1; i<=decs->Count; ++i)
			{
				DeclarationInfoPtr dec = decs->Item[i];

			}

			IntrinsicAliasesPtr ias = tli_->IntrinsicAliases;
			for (int i=1; i<=ias->Count; ++i)
			{
				IntrinsicAliasInfoPtr ia = ias->Item[i];

				Alias *pA = new Alias(ia);
				aliases_.push_back(pA);
			}

			ConstantsPtr cs = tli_->Constants;
			for (int i=1; i<=cs->Count; ++i)
			{
				ConstantInfoPtr ci = cs->Item[i];

				int tk = ci->TypeKind;

				switch (tk) {
					case TLI::TKIND_ENUM:
						{
							Enum *pE = new Enum(ci);
							enums_.push_back(pE);
							break;
						}
					case TLI::TKIND_ALIAS:
						{
							Alias *pA = new Alias(ci);
							aliases_.push_back(pA);
							break;
						}
					case TLI::TKIND_MODULE:
						{
							MembersPtr ms = ci->Members;
							for (int i=1; i<=ms->Count; ++i) {
								MemberInfoPtr ti = ms->Item[i];
								if (ti->DescKind == TLI::DESCKIND_VARDESC) {
									Constant *pC = new Constant(ti);
									constants_.push_back(pC);
								}
							}
						}
						break;
					default:
						throw std::logic_error("Unexpected TypeKind");
						break;
				}
			}
			check_referenced_interfaces();
		}
	}

};


void Coclass::update(TypeLibrary* tl) {
	InterfacesPtr itfs = cci_->Interfaces;
	for (int i=1; i<=itfs->Count; ++i)
	{
		InterfaceInfoPtr ipx = itfs->Item[i];
		InterfaceInfoPtr ip = ipx->VTableInterface;
		if (ip == NULL)
		{
			// Disp interfaces are NOT ignored!
			ip=ipx;
		}

		// Only source interfaces
		if (ipx->GetAttributeMask() & 0x2) {
			tl->add_source_interface(ip);
		}

	}
}

string Type::get_raw(int pl, unsigned& vsz, bool use_variant_type, bool force_use_of_ns ) const {

	if (pl < 0) {
#ifndef NDEBUG
		return "<<INVALID POINTER LEVEL>>";
#endif
		throw std::runtime_error("Invalid pointer level");
	}
	string s;
	int vt = get_unaliased_vartype(); //vti_->VarType;

	vsz = 0;

	if (vt & TLI::VT_VECTOR) {
		SAFEARRAY* psa;
		short l = vti_->ArrayBounds(&psa);

		unsigned i = 1;
		long sz;
		SafeArrayGetUBound(psa, i, &sz);
		if (sz != 1) throw std::runtime_error("Cannot handle multi-dimensional vectors.");

		long j = 2;
		unsigned* p = (unsigned*)psa->pvData;
		vsz = p[1] + p[0];

		::SafeArrayDestroy(psa);
		vt &= ~TLI::VT_VECTOR;
	}
	if (vt & TLI::VT_ARRAY) {
		s = "SAFEARRAY *";
		goto end;
	}
	if (vt & TLI::VT_BYREF) throw std::runtime_error("VARTYPE ByRef is not supported.");
	if (vt & TLI::VT_RESERVED) throw std::runtime_error("VARTYPE Reserved is invalid.");

	switch (vt) {
	case TLI::VT_I2:
		s = "short";
		break;
	case TLI::VT_I4:
		s = "long";
		break;
	case TLI::VT_I8:
		s = "__int64";
		break;
	case TLI::VT_R4:
		s = "float";
		break;
	case TLI::VT_R8:
		s = "double";
		break;
	case TLI::VT_BSTR:
		s = "BSTR";
		break;
	case TLI::VT_DISPATCH:
		s = "IDispatch *";
		break;
	case TLI::VT_BOOL:
		s = "VARIANT_BOOL";
		break;
	case TLI::VT_VARIANT:
		s = "VARIANT";
		break;
	case TLI::VT_UNKNOWN:
		s = "IUnknown *";
		break;
	case TLI::VT_DATE:
		s = "DATE";
		break;
	case TLI::VT_CY:
		s = "CURRENCY";
		break;
	case TLI::VT_I1:
		s = "signed char";
		break;
	case TLI::VT_UI1:
		s = "unsigned char";
		break;
	case TLI::VT_UI2:
		s = "unsigned short";
		break;
	case TLI::VT_UI4:
		s = "unsigned long";
		break;
	case TLI::VT_UI8:
		s = "unsigned __int64";
		break;
	case TLI::VT_INT:
		s = "int";
		break;
	case TLI::VT_UINT:
		s = "unsigned";
		break;
	case TLI::VT_VOID:
		s = "void";
		break;
	case TLI::VT_HRESULT:
		s = "HRESULT";
		break;
	case TLI::VT_LPSTR:
		s = "char*";
		break;
	case TLI::VT_LPWSTR:
		s = "wchar_t*";
		break;
	case TLI::VT_EMPTY:
		{
			TypeInfoPtr ti;
			ti = vti_->TypeInfo;

			string name =nso.get_name_with_ns(ti, force_use_of_ns);

			TypeKinds tk = get_unaliased_typekind();
			switch (tk) {
			case TLI::TKIND_ENUM:
//				if(use_variant_type)
//					name="long";
//				else
				if( !is_aliased_type())
					s = "enum ";
				break;
			case TLI::TKIND_RECORD:
			{
				name = nso.get_name_with_ns(get_unaliased_typeinfo(), true);
				if (name != "::GUID")
					name = nso.get_name_with_ns(ti, force_use_of_ns, "raw_");
			}break;
			case TLI::TKIND_INTERFACE:
				if(use_variant_type)
					name = "IUnknown";
				break;
			case TLI::TKIND_DISPATCH:
				if(use_variant_type)
					name = "IDispatch";
				break;
			case TLI::TKIND_ALIAS:
				break;
			case TLI::TKIND_UNION:
				break;
			case TLI::TKIND_COCLASS:
				ti = ti->DefaultInterface;
				name = nso.get_name_with_ns(ti, force_use_of_ns);
				break;
			case TLI::TKIND_MODULE:
			case TLI::TKIND_MAX:
			default:
				s = "?";
			}
			s +=  name;
			break;
		}
	case TLI::VT_ERROR:
		s = "HRESULT";
		break;
	case TLI::VT_PTR:
	case TLI::VT_SAFEARRAY:
	case TLI::VT_CARRAY:
	case TLI::VT_USERDEFINED:
	case TLI::VT_RECORD:
	case TLI::VT_FILETIME:
	case TLI::VT_BLOB:
	case TLI::VT_DECIMAL:
	case TLI::VT_STREAM:
	case TLI::VT_STORAGE:
	case TLI::VT_STREAMED_OBJECT:
	case TLI::VT_STORED_OBJECT:
	case TLI::VT_BLOB_OBJECT:
	case TLI::VT_CF:
	case TLI::VT_CLSID:
	case TLI::VT_NULL:
	default:
		throw std::runtime_error("Unknown VARTYPE");
	}
end:
	for (int i=0; i<pl; ++i) {
		s += "*";
	}
	return s;
}

string
or_append(const string &str,const string &str2, bool &orred)
{
	if(str2.empty())
	{
		return str;
	}
	if(!str.empty())
	{
		orred = true;
		return str + " | " + str2;
	}
	return str2;
}
string
cond_bracket(const string &str, bool bracket)
{
	if(!bracket) return str;
	return "(" + str + ")";
}

string vt2string(VARTYPE vt)
{
	string s = "";
	string byref;
	if (vt & TLI::VT_BYREF) byref = "VT_BYREF";
	bool orred=false;

	if (vt & TLI::VT_VECTOR) s = "VT_VECTOR";
	if (vt & TLI::VT_ARRAY) s =or_append(s,"VT_ARRAY",orred);
	if (vt & TLI::VT_RESERVED) s =or_append(s,"VT_RESERVED",orred);

#define VT2STRING(x) case TLI::x: s = or_append(s , #x,orred); break

	switch (vt & ~(TLI::VT_VECTOR|TLI::VT_ARRAY|TLI::VT_BYREF) ) {
	VT2STRING(VT_I2);
	VT2STRING(VT_I4);
	VT2STRING(VT_R4);
	VT2STRING(VT_R8);
	VT2STRING(VT_BSTR);
	VT2STRING(VT_DISPATCH);
	VT2STRING(VT_BOOL);
	VT2STRING(VT_VARIANT);
	VT2STRING(VT_UNKNOWN);
	VT2STRING(VT_I1);
	VT2STRING(VT_UI1);
	VT2STRING(VT_UI2);
	VT2STRING(VT_UI4);
	VT2STRING(VT_INT);
	VT2STRING(VT_UINT);
	VT2STRING(VT_VOID);
	VT2STRING(VT_HRESULT);
	VT2STRING(VT_LPSTR);
	VT2STRING(VT_LPWSTR);
	VT2STRING(VT_EMPTY);
	VT2STRING(VT_CY);
	VT2STRING(VT_ERROR);
	VT2STRING(VT_DATE);
	VT2STRING(VT_I8);
	VT2STRING(VT_UI8);
	VT2STRING(VT_PTR);
	VT2STRING(VT_SAFEARRAY);
	VT2STRING(VT_CARRAY);
	VT2STRING(VT_USERDEFINED);
	VT2STRING(VT_RECORD);
	VT2STRING(VT_FILETIME);
	VT2STRING(VT_BLOB);
	VT2STRING(VT_DECIMAL);
	VT2STRING(VT_STREAM);
	VT2STRING(VT_STORAGE);
	VT2STRING(VT_STREAMED_OBJECT);
	VT2STRING(VT_STORED_OBJECT);
	VT2STRING(VT_BLOB_OBJECT);
	VT2STRING(VT_CF);
	VT2STRING(VT_CLSID);
	VT2STRING(VT_NULL);
	default:
		throw std::runtime_error("Unknown VARTYPE");
	}
	s = or_append(s , byref,orred);
	return cond_bracket(s,orred);
}

string vt2refmacro(VARTYPE vt)
{
	if(vt & TLI::VT_ARRAY)
	{
		return (( vt & TLI::VT_BYREF)==0)?"V_ARRAY":"V_ARRAYREF";
	}
	switch(vt)
	{
		case TLI::VT_UI1: return "V_UI1";
		case TLI::VT_I2: return "V_I2";
		case TLI::VT_I4: return "V_I4";
		case TLI::VT_R4: return "V_R4";
		case TLI::VT_R8: return "V_R8";
		case TLI::VT_BOOL: return "V_BOOL";
		case TLI::VT_ERROR: return "V_ERROR";
		case TLI::VT_CY: return "V_CY";
		case TLI::VT_DATE: return "V_DATE";
		case TLI::VT_BSTR: return "V_BSTR";
		case TLI::VT_DECIMAL: return "V_DECIMAL";
		case TLI::VT_UNKNOWN: return "V_UNKNOWN";
		case TLI::VT_DISPATCH: return "V_DISPATCH";
		case TLI::VT_BYREF|TLI::VT_UI1: return "V_UI1REF";
		case TLI::VT_BYREF|TLI::VT_I2: return "V_I2REF";
		case TLI::VT_BYREF|TLI::VT_I4: return "V_I4REF";
		case TLI::VT_BYREF|TLI::VT_R4: return "V_R4REF";
		case TLI::VT_BYREF|TLI::VT_R8: return "V_R8REF";
		case TLI::VT_BYREF|TLI::VT_BOOL: return "V_BOOLREF";
		case TLI::VT_BYREF|TLI::VT_ERROR: return "V_ERRORREF";
		case TLI::VT_BYREF|TLI::VT_CY: return "V_CYREF";
		case TLI::VT_BYREF|TLI::VT_DATE: return "V_DATEREF";
		case TLI::VT_BYREF|TLI::VT_BSTR: return "V_BSTRREF";
		case TLI::VT_BYREF|TLI::VT_DECIMAL: return "V_DECIMALREF";
		case TLI::VT_BYREF|TLI::VT_UNKNOWN: return "V_UNKNOWNREF";
		case TLI::VT_BYREF|TLI::VT_DISPATCH: return "V_DISPATCHREF";
		case TLI::VT_BYREF|TLI::VT_VARIANT: return "V_VARIANTREF";
		case TLI::VT_BYREF: return "V_BYREF";
		case TLI::VT_I1: return "V_I1";
		case TLI::VT_UI2: return "V_UI2";
		case TLI::VT_UI4: return "V_UI4";
		case TLI::VT_INT: return "V_INT";
		case TLI::VT_UINT: return "V_UINT";
		case TLI::VT_BYREF|TLI::VT_I1: return "V_I1REF";
		case TLI::VT_BYREF|TLI::VT_UI2: return "V_UI2REF";
		case TLI::VT_BYREF|TLI::VT_UI4: return "V_UI4REF";
		case TLI::VT_BYREF|TLI::VT_INT: return "V_INTREF";
		case TLI::VT_BYREF|TLI::VT_UINT: return "V_UINTREF";
	}
	return "";
}



string Type::get_cpp(int pl, unsigned& vsz, bool& special, bool no_ref, bool is_definition, bool html, bool is_struct) const
{
	vsz = 0;
	if (pl < 0) {
#ifndef NDEBUG
		return "<<INVALID POINTER LEVEL>>";
#endif
		throw std::exception("Invalid pointer level");
	}

	special = false;
	string s;
	bool is_safearray = false;
	int vt =  get_unaliased_vartype();
	//int vt = vti_->VarType;

	if (vt & TLI::VT_VECTOR) {
		SAFEARRAY* psa;
		short l = vti_->ArrayBounds(&psa);

		unsigned i = 1;
		long sz;
		SafeArrayGetUBound(psa, i, &sz);
		if (sz != 1) throw std::runtime_error("Cannot handle multi-dimensional vectors.");

		long j = 2;
		unsigned* p = (unsigned*)psa->pvData;
		vsz = p[1] + p[0];

		::SafeArrayDestroy(psa);
		vt &= ~TLI::VT_VECTOR;
	}
	if (vt & TLI::VT_BYREF) throw std::runtime_error("VARTYPE ByRef is not supported.");
	if (vt & TLI::VT_RESERVED) throw std::runtime_error("VARTYPE Reserved is invalid.");

	if (vt & TLI::VT_ARRAY) {
		is_safearray = true;
		vt &= ~TLI::VT_ARRAY;
	}

	switch (vt) {
	case TLI::VT_I2:
		s = "short";
		break;
	case TLI::VT_I4:
		s = "long";
		break;
	case TLI::VT_I8:
		s = "__int64";
		break;
	case TLI::VT_R4:
		s = "float";
		break;
	case TLI::VT_R8:
		s = "double";
		break;
	case TLI::VT_BSTR:
		if (is_safearray || pl < 2)
			s = "bstr_t";
		else
			throw std::runtime_error("Cannot handle [in] BSTR**");
		special = true;
		break;
	case TLI::VT_DISPATCH:
		if (html)
			s = "com_ptr&lt;IDispatch&gt;";
		else
			s = "com_ptr<IDispatch>";
		if (!is_safearray && pl != 0) throw std::runtime_error("Cannot handle [in] IDispatch**");
		special = true;
		break;
	case TLI::VT_BOOL:
		if (!is_safearray && pl > 0) throw std::runtime_error("Cannot handle [in] VARIANT_BOOL*");
		s = ((is_struct && !is_safearray)?"variant_bool_t":"bool");
		special = true;
		break;
	case TLI::VT_VARIANT:
		if (is_safearray || pl < 2)
			s = "variant_t";
		else
			throw std::runtime_error("Cannot handle [in] VARIANT**");
		special = true;
		break;
	case TLI::VT_UNKNOWN:
		if (html)
			s = "com_ptr&lt;IUnknown&gt;";
		else
			s = "com_ptr<IUnknown>";
		if (!is_safearray && pl != 0) throw std::runtime_error("Cannot handle [in] IUnknown**");
		special = true;
		break;
	case TLI::VT_DATE:
		s = "datetime_t";
		break;
	case TLI::VT_CY:
		if (is_safearray ||pl < 2)
			s = "currency_t";
		else
			throw std::runtime_error("Cannot handle [in] CURRENCY**");
		special = true;
		break;
	case TLI::VT_I1:
		s = "signed char";
		break;
	case TLI::VT_UI1:
		s = "unsigned char";
		break;
	case TLI::VT_UI2:
		s = "unsigned short";
		break;
	case TLI::VT_UI4:
		s = "unsigned long";
		break;
	case TLI::VT_UI8:
		s = "unsigned __int64";
		break;
	case TLI::VT_INT:
		s = "int";
		break;
	case TLI::VT_UINT:
		s = "unsigned";
		break;
	case TLI::VT_VOID:
		if (pl > 0) {
			s = "void*";
			pl--;
		} else {
			s = "void";
		}
		break;
	case TLI::VT_HRESULT:
		s = "HRESULT";
		break;
	case TLI::VT_LPSTR:
		s = "char*";
		break;
	case TLI::VT_LPWSTR:
		s = "wchar_t*";
		break;
	case TLI::VT_EMPTY:
		{
			TypeInfoPtr ti = vti_->TypeInfo;
			// TypeKinds tk = ti->GetTypeKind();
			TypeKinds tk = get_unaliased_typekind();
			switch (tk) {
			case TLI::TKIND_ENUM:
				{
					s = (is_definition && !is_aliased_type()) ? "enum " : "";

					std::string n = nso.get_name_with_ns(ti);
					if (html)
						s += "<a href=""#" + n + ">" + n + "</a>";
					else
						s += n;
				}
				break;
			case TLI::TKIND_RECORD:
			{
				string unwrapped_type = nso.get_name_with_ns(get_unaliased_typeinfo(), true);
				if ( unwrapped_type == "::GUID")
					s = "uuid_t";
				else
				{
					std::string n = nso.get_name_with_ns(ti);
					if (html)
						s = "<a href=""#" + n + ">" + n + "</a>";
					else
						s = n;
				}
			} break;
			case TLI::TKIND_INTERFACE:
			case TLI::TKIND_DISPATCH:
				if (html)
					s = "com_ptr&lt;";
				else
					s = "com_ptr<";
/*				if( use_variant_type )
					s += ((tk == TLI::TKIND_INTERFACE)?"IUnknown":"IDispatch");
				else*/
				{
					std::string n = nso.get_name_with_ns(ti);
					if (html)
						s += "<a href=""#" + n + ">" + n + "</a>";
					else
						s += n;
				}
				if (html)
					s += "&gt;";
				else
					s += ">";
				if (!is_safearray )
				{
					if (pl != 1) {
						throw std::runtime_error("Cannot handle [in] <Itf>**");
					}
					pl = 0;
				}
				special = true;
				break;
			case TLI::TKIND_ALIAS:
				{
					std::string n = nso.get_name_with_ns(ti);

					VarTypeInfoPtr ti2 = ti->ResolvedType;
					VARTYPE vt2 = ti2->VarType;

					if(is_pointer_alias())
					{
						special = true;

						if (html)
							n = "com_ptr&lt;" + n + "&gt;";
						else
							n = "com_ptr<" + n + ">";
					}

					if (html)
						s = "<a href=\"#" + n + "\">" + n + "</a>";
					else
						s = n;
				}
				break;
			case TLI::TKIND_COCLASS:
				ti = ti->DefaultInterface;
				if (html)
					s = "com_ptr&lt;";
				else
					s = "com_ptr<";
				{
					std::string n = nso.get_name_with_ns(ti);
					if (html)
						s += "<a href=\"#" + n + "\">" + n + "</a>";
					else
						s += n;
				}
				if (html)
					s += "&gt;";
				else
					s += ">";
				if(!is_safearray)
				{
					if (pl != 1) {
						throw std::runtime_error("Cannot handle [in] <Itf>**");
					}
					pl = 0;
				}
				special = true;
				break;
			case TLI::TKIND_UNION:
				{
					std::string n = nso.get_name_with_ns(ti);
					if (html)
						s = "<a href=\"#" + n + "\">" + n + "</a>";
					else
						s = n;
				}
				break;
			case TLI::TKIND_MODULE:
			case TLI::TKIND_MAX:
			default:
				s = "?";
				s += nso.get_name_with_ns(ti);
			}
			break;
		}
	case TLI::VT_ERROR:
		s = "HRESULT";
		break;
	case TLI::VT_PTR:
	case TLI::VT_SAFEARRAY:
	case TLI::VT_CARRAY:
	case TLI::VT_USERDEFINED:
	case TLI::VT_RECORD:
	case TLI::VT_FILETIME:
	case TLI::VT_BLOB:
	case TLI::VT_DECIMAL:
	case TLI::VT_STREAM:
	case TLI::VT_STORAGE:
	case TLI::VT_STREAMED_OBJECT:
	case TLI::VT_STORED_OBJECT:
	case TLI::VT_BLOB_OBJECT:
	case TLI::VT_CF:
	case TLI::VT_CLSID:
	case TLI::VT_NULL:
	default:
		throw std::runtime_error("Unknown VARTYPE");
	}

	if (pl > 1) {
#ifndef NDEBUG
		return "<<POINTER LEVEL TOO HIGH>>";
#endif NDEBUG
		throw std::runtime_error("Pointer level too high for [in] parameter");
	}
	if (is_safearray) {
		if (s.end()[-1] == '>') s+= " ";
		if (html)
			s = string("safearray_t&lt;") + s + "&gt;";
		else
			s = string("safearray_t<") + s + ">";
		special = true;
	}

	if (pl == 1) {
		if (!special) {
			if (no_ref)
				s += "*";
			else
				s += ampersand(html);
		}
	}

	if (vsz > 0 && special) throw std::runtime_error("Cannot handle VT_VECTOR on types that requires C++ wrappers.");

	return s;
}

string Type::as_raw(unsigned& vsz, bool UseVariantType ) const
{
	return get_raw(get_pointer_level(false), vsz, UseVariantType);
}

string Type::as_raw_retval(bool with_ns) const
{
	unsigned vsz = 0;
	std::string rv = get_raw(get_pointer_level() - 1, vsz, false, with_ns);
	if (vsz != 0) throw std::runtime_error("Cannot handle VT_VECTOR retval");
	return rv;
}

string Type::as_cpp_struct_type(unsigned& vsz) const
{
	bool special = false;
	string s = get_cpp(get_pointer_level(), vsz, special, true, true, false, true);
	return s;
}

string Type::as_cpp_in( unsigned& vsz, bool html) const {
	bool special = false;
	string s = get_cpp(get_pointer_level(false), vsz, special, false, true, html);
	if (special) {
		int vt = get_unaliased_vartype(); //vti_->VarType;
		if ((vt & TLI::VT_ARRAY) != 0) {
			s  = "const " + s + ampersand(html);
		} else {
			switch (vt) {
			case ::VT_BSTR:
			case ::VT_VARIANT:
			case ::VT_UNKNOWN:
			case ::VT_DISPATCH:
			case ::VT_EMPTY:
			case ::VT_DATE:
			case ::VT_CY:
				s = "const " + s + ampersand(html);
				break;
			default:
				break;
			}
		}
	}
	else if (get_pointer_level() > 0) s = "const " + s;
	return s;
}

string Type::as_cpp_out(bool html) const {
	bool special = false;
	unsigned vsz = 0;
	string s;
	VARTYPE vt = get_unaliased_vartype(); //vti_->VarType;
	if (vt == TLI::VT_VOID && get_pointer_level() == 1)
		s = get_cpp(get_pointer_level(), vsz, special, true, true, html);
	else
		s = get_cpp(get_pointer_level() - 1, vsz, special, true, true, html);
	if (vsz != 0) throw std::runtime_error("VT_VECTOR outgoing parameters are not supported.");
	return s;
}

string Type::as_cpp_inout(bool html) const {
	bool special = false;
	unsigned vsz = 0;
	string s;
	VARTYPE vt = get_unaliased_vartype(); //vti_->VarType;
	if (vt == TLI::VT_VOID && get_pointer_level() == 1)
		s = get_cpp(get_pointer_level(), vsz, special, false, true, html);
	else
		s = get_cpp(get_pointer_level() - 1, vsz, special, false, true, html);
	if (vsz != 0) throw std::runtime_error("VT_VECTOR outgoing parameters are not supported.");
	return s;
}

string Type::as_cpp_retval(bool html) const {
	bool special = false;
	unsigned vsz = 0;
	string s = get_cpp(get_pointer_level() - 1, vsz, special, true, false, html);
	if (vsz != 0) throw std::runtime_error("VT_VECTOR outgoing parameters are not supported.");
	return s;
}

string Type::get_cpp_detach(const string& var_name) const {
	int vt =  get_unaliased_vartype();
	if (vt & TLI::VT_ARRAY) {
		string s;// = var_name;
		bool dummy;
		unsigned vsz = 0;
		s = get_cpp(0, vsz, dummy);
		if (vsz != 0) throw std::runtime_error("VT_VECTOR outgoing parameters are not supported.");
		s += "::detach(";
		s += var_name;
		s += ")";
		return s;
	} else
	switch (vt) {
	case ::VT_BOOL:
		{
			string s;
			s = var_name;
			s += " ? COMET_VARIANT_TRUE : COMET_VARIANT_FALSE";
			return s;
		}
	case ::VT_EMPTY:
		{
			TypeInfoPtr ti =  vti_->TypeInfo;
			TypeKinds tk = get_unaliased_typekind();
			switch (tk) {
			case TLI::TKIND_COCLASS:
				ti = ti->DefaultInterface;
			case TLI::TKIND_INTERFACE:
			case TLI::TKIND_DISPATCH:
				{
					string s;
					s = "com_ptr<";
					s += nso.get_name_with_ns(ti);
					s += ">::detach(";
					s += var_name;
					s += ")";
					return s;
					break;
				}
			case TLI::TKIND_RECORD:
				{
					string unwrapped_type = nso.get_name_with_ns(get_unaliased_typeinfo(), true);
					if ( unwrapped_type == "::GUID")
						break;
					string s;
					s = nso.get_name_with_ns(ti);
					s += "::comet_detach(";
					s += var_name;
					s += ")";
					return s;
					break;
				}
			default:
				break;
			}
		}
		break;
	case ::VT_UNKNOWN:
		{
			string s;
			s = "com_ptr<IUnknown>::detach(";
			s += var_name;
			s += ")";
			return s;
		}
	case ::VT_DISPATCH:
		{
			string s;
			s = "com_ptr<IDispatch>::detach(";
			s += var_name;
			s += ")";
			return s;
		}
	case ::VT_BSTR:
		{
			string s;
			s = "bstr_t::detach(";
			s += var_name;
			s += ")";
			return s;
		}
	case ::VT_VARIANT:
		{
			string s;
			s = "variant_t::detach(";
			s += var_name;
			s += ")";
			return s;
		}
	case ::VT_CY:
		{
			string s;
			s = "currency_t::detach(";
			s += var_name;
			s += ")";
			return s;
		}
	case ::VT_DATE:
		{
			string s;
			s = "datetime_t::detach(";
			s += var_name;
			s += ")";
			return s;
		}
	default:
		break;
	}
	return var_name;
}

string Type::get_return_constructor(const string& r, bool cast_itf) const {
	string s;

	int vt = get_unaliased_vartype();

	if (vt & TLI::VT_ARRAY) {
		s = "auto_attach(";
		s += r;
		s += ")";
		if (get_pointer_level() != 1) throw std::exception("Invalid return type [VT_ARRAY]");
	} else
	switch (vt) {
	case TLI::VT_BOOL:
		s = r;
		s += " != COMET_VARIANT_FALSE";
		if (get_pointer_level() != 1) throw std::exception("Invalid return type [VT_BOOL]");
		break;
	case TLI::VT_DATE:
		s = "datetime_t(";
		s += r;
		s += ")";
		if (get_pointer_level() != 1) throw std::exception("Invalid return type [VT_BOOL]");
		break;
	case TLI::VT_BSTR:
	case TLI::VT_DISPATCH:
	case TLI::VT_VARIANT:
/*	case TLI::VT_CY:  */
	case TLI::VT_UNKNOWN:
		s = "auto_attach(";
		s += r;
		s += ")";
		if (get_pointer_level() != 1) throw std::exception( std::string("Invalid return type [" + vt2string(vt) +"]").c_str());
		break;
	case TLI::VT_EMPTY:
		{
			TypeInfoPtr ti =vti_->TypeInfo;
			switch ( get_unaliased_typekind() ) {
			case TLI::TKIND_COCLASS:
			case TLI::TKIND_INTERFACE:
			case TLI::TKIND_DISPATCH:
				if (cast_itf) {
					s = "try_cast(";
					s += r;
					s += ")";
				}
				else {
					s = "auto_attach(";
					s += r;
					s += ")";
				}
			if (get_pointer_level() != 2) throw std::exception("Invalid return type [VT_EMPTY]");
				break;
			case TLI::TKIND_RECORD:
			{
				string unwrapped_type = nso.get_name_with_ns(get_unaliased_typeinfo(), true);
				if ( unwrapped_type == "::GUID")
					 return r;
				s = "auto_attach(";
				s += r;
				s += ")";
				if (get_pointer_level() != 1) throw std::exception( std::string("Invalid return type [" + vt2string(vt) +"]").c_str());
			}break;
			case TLI::TKIND_ENUM:
				s = (is_aliased_type()?"":"/*enum */ ");
				s += nso.get_name_with_ns(ti);
				s += "(" + r + ")";
				break;
			case TLI::TKIND_ALIAS:
			default:
				return r;
			}
			break;
		}
	default:
		return r;
	}

	return s;
}

string Type::get_cpp_attach(const string& r) const {
	string s;

	int vt = get_unaliased_vartype();

	if (vt & TLI::VT_ARRAY) {
		s = "auto_attach(";
		s += r;
		s += ")";
	} else
	switch (vt) {
	case TLI::VT_BOOL:
		s = r + "!= COMET_VARIANT_FALSE";
		break;
	case TLI::VT_BSTR:
	case TLI::VT_DISPATCH:
	case TLI::VT_VARIANT:
/*	case TLI::VT_DATE: */
/*    case TLI::VT_CY: */
	case TLI::VT_UNKNOWN:
		s = "auto_attach(";
		s += r;
		s += ")";
		break;
	case TLI::VT_EMPTY:
		{
			TypeInfoPtr ti =vti_->TypeInfo;
			switch ( get_unaliased_typekind() ) {
			case TLI::TKIND_INTERFACE:
			case TLI::TKIND_DISPATCH:
				s = "auto_attach(";
				s += r;
				s += ")";
				break;
			case TLI::TKIND_RECORD:
			{
				string unwrapped_type = nso.get_name_with_ns(get_unaliased_typeinfo(), true);
				if ( unwrapped_type == "::GUID")
					return r;
				s = "auto_attach(";
				s += r;
				s += ")";
			} break;
			case TLI::TKIND_ALIAS:
			default:
				return r;
			}
			break;
		}
	default:
		return r;
	}

	return s;
}

string Type::get_impl_in_constructor(const string& r, bool asVariantType, bool ignore_pointer_level ) const {
	string s;

	int vt =  get_unaliased_vartype();
	long pointer_level = (ignore_pointer_level?0: get_pointer_level());

	if (vt & TLI::VT_ARRAY) {
		if (pointer_level > 1) throw std::exception("Invalid [in] type [VT_BSTR]. Reason: pointer_level > 1)");
		bool dummy;
		unsigned vsz = 0;
		s = get_cpp(0, vsz, dummy);
		if (vsz != 0) throw std::runtime_error("Invalid [in] type [VT_VECTOR]. Reason: vsz != 0");
		s += "::create_const_reference(";
		if (pointer_level == 1) s += '*';
		s += r;
		s += ")";
	} else
	switch (vt) {
	case TLI::VT_BOOL:
		if (pointer_level > 0) throw std::exception("Invalid [in] type [VT_BOOL]. Reason: pointer_level > 0");
		if (pointer_level == 1) s = "*"; else s = "";
		s += r;
		s += "!= COMET_VARIANT_FALSE";
		break;
	case TLI::VT_BSTR:
		if (pointer_level > 1) throw std::exception("Invalid [in] type [VT_BSTR]. Reason: pointer_level > 1");
		s = "bstr_t::create_const_reference(";
		if (pointer_level == 1) s += '*';
		s += r;
		s += ")";
		break;
	case TLI::VT_DISPATCH:
		if (pointer_level != 0) throw std::exception("Invalid [in] type [VT_DISPATCH]. Reason: pointer_level != 0");
		s = "com_ptr<IDispatch>::create_const_reference(";
		s += r;
		s += ")";
		break;
	case TLI::VT_VARIANT:
		if (pointer_level > 1) throw std::exception("Invalid [in] type [VT_VARIANT]. Reason: pointer_level > 1");
		s = "variant_t::create_const_reference(";
		if (pointer_level == 1) s += '*';
		s += r;
		s += ")";
		break;
	case TLI::VT_CY:
		if (pointer_level != 0) throw std::exception("Invalid [in] type [VT_CY]. Reason: pointer_level != 0");
		s = "currency_t(";
		s += r;
		s += ")";
		break;
	case TLI::VT_DATE:
		if (pointer_level != 0) throw std::exception("Invalid [in] type [VT_DATE]. Reason: pointer_level != 0");
		s = "datetime_t(";
		s += r;
		s += ")";
		break;

	case TLI::VT_UNKNOWN:
		if (pointer_level != 0) throw std::exception("Invalid [in] type [VT_UNKNOWN]. Reason: pointer_level != 0");
		s = "com_ptr<IUnknown>::create_const_reference(";
		s += r;
		s += ")";
		break;
	case TLI::VT_EMPTY:
		{
			TypeInfoPtr ti = vti_->TypeInfo;
			TypeKinds tk = get_unaliased_typekind();
			if( !asVariantType)
			{
				switch ( tk ) {
				case TLI::TKIND_INTERFACE:
				case TLI::TKIND_DISPATCH:
					s = "com_ptr<";
					s += nso.get_name_with_ns(ti);
					s += ">::create_const_reference(";
					s += r;
					s += ")";
				if (!ignore_pointer_level && pointer_level != 1) throw std::exception("Invalid [in] type [VT_EMPTY]. Reason: pointer_level != 1");
					break;
				case TLI::TKIND_RECORD:
				{
					string unwrapped_type = nso.get_name_with_ns(get_unaliased_typeinfo(), true);
					if ( unwrapped_type == "::GUID")
					{
						s = "uuid_t::create_const_reference(";
						if (pointer_level == 1) s += "*";
						s += r;
						s += ")";
					}
					else
					{
						s = nso.get_name_with_ns(ti) + "::comet_create_const_reference(";
						if (pointer_level == 1) s += "*";
						s += r;
						s += ")";
//						return r;
					}
				} break;
				default:
					return r;
				}
			}
			else
			{
				switch ( tk )
				{
					case TLI::TKIND_INTERFACE:
					case TLI::TKIND_DISPATCH:
						s = "try_cast(";
						s += r;
						s += ")";
					if (!ignore_pointer_level && pointer_level != 1) throw std::exception("Invalid [in] type [VT_EMPTY]. Reason: pointer_level != 1");
						break;
					default:
						return r;
				}
			}
			break;
		}
	default:
		return r;
	}

	return s;
}

string Type::get_impl_inout_constructor(const string& r, bool asVariantType ) const {
	string s;

	int vt = get_unaliased_vartype();

	if (vt & TLI::VT_ARRAY) {
		if (get_pointer_level() != 1) throw std::exception("Invalid [in, out] type [VT_BSTR]");
		bool dummy;
		unsigned vsz = 0;
		s = get_cpp(0, vsz, dummy);
		if (vsz != 0) throw std::runtime_error("Invalid [in, out] type [VT_VECTOR].");
		s += "::create_reference(*";
		s += r;
		s += ")";
	} else
	switch (vt) {
	case TLI::VT_BOOL:
		if (get_pointer_level() != 1) throw std::exception("Invalid [in, out] type [VT_BOOL]");
		s = "impl::bool_adapter_t(";
		s += r;
		s += ").ref()";
		break;
	case TLI::VT_BSTR:
		if (get_pointer_level() != 1) throw std::exception("Invalid [in, out] type [VT_BSTR]");
		s = "bstr_t::create_reference(*";
		s += r;
		s += ")";
		break;
	case TLI::VT_DISPATCH:
		if (get_pointer_level() != 1) throw std::exception("Invalid [in, out] type [VT_DISPATCH]");
		s = "com_ptr<IDispatch>::create_reference(*";
		s += r;
		s += ")";
		break;
	case TLI::VT_VARIANT:
		if (get_pointer_level() != 1) throw std::exception("Invalid [in, out] type [VT_VARIANT]");
		s = "variant_t::create_reference(*";
		s += r;
		s += ")";
		break;
	case TLI::VT_CY:
		if (get_pointer_level() != 1) throw std::exception("Invalid [in, out] type [VT_CY]");
		s = "currency_t::create_reference(*";
		s += r;
		s += ")";
		break;
	case TLI::VT_DATE:
		if (get_pointer_level() != 1) throw std::exception("Invalid [in, out] type [VT_DATE]");
		s = "datetime_t::create_reference(*";
		s += r;
		s += ")";
		break;

	case TLI::VT_UNKNOWN:
		if (get_pointer_level() != 1) throw std::exception("Invalid [in, out] type [VT_UNKNOWN]");
		s = "com_ptr<IUnknown>::create_reference(*";
		s += r;
		s += ")";
		break;
	case TLI::VT_EMPTY:
		{
			TypeInfoPtr ti = vti_->TypeInfo;
			TypeKinds tk = get_unaliased_typekind();
			if( !asVariantType)
			{
				switch (tk ) {
				case TLI::TKIND_INTERFACE:
				case TLI::TKIND_DISPATCH:
					if (get_pointer_level() != 2) throw std::exception("Invalid [in, out] type [VT_EMPTY]");
					s = "com_ptr<";
					s += nso.get_name_with_ns(ti);
					s += ">::create_reference(*";
					s += r;
					s += ")";
					break;
				case TLI::TKIND_RECORD:
				{
					string unwrapped_type = nso.get_name_with_ns(get_unaliased_typeinfo(), true);
					if ( unwrapped_type == "::GUID")
					{
						if (get_pointer_level() != 1) throw std::exception("Invalid [in, out] type [VT_EMPTY, GUID]");
						s = "uuid_t::create_reference(*";
						s += r;
						s += ")";
					}
					else
					{
						if (get_pointer_level() != 1) throw std::exception("Invalid [in, out] type [VT_EMPTY, TKIND_RECORD]");
						s = nso.get_name_with_ns(ti) + "::comet_create_reference(*";
						s += r;
						s += ")";
//						return r;
					}
				}	break;
				default:
					return "*" + r;
//					return r;
				}
			}
			else
			{
				switch (tk )
				{
					case TLI::TKIND_INTERFACE:
					case TLI::TKIND_DISPATCH:
						s = "try_cast(";
						s += r;
						s += ")";
					if (get_pointer_level() != 1) throw std::exception("Invalid [in, out] type [VT_EMPTY]");
						break;
					default:
						return "*" + r;
				}
			}
			break;
		}
	default:
		return r;
	}

	return s;
}

bool Type::requires_wrapper() const {

	int vt = get_unaliased_vartype();

	if (vt & TLI::VT_ARRAY) return true;
	switch (vt) {
	case TLI::VT_BOOL:
	case TLI::VT_BSTR:
	case TLI::VT_DISPATCH:
	case TLI::VT_VARIANT:
	case TLI::VT_DATE:
	case TLI::VT_CY:
	case TLI::VT_UNKNOWN:
		return true;
	case TLI::VT_EMPTY:
		{
			TypeInfoPtr ti = vti_->TypeInfo;
			switch (get_unaliased_typekind()){
			case TLI::TKIND_INTERFACE:
			case TLI::TKIND_DISPATCH:
			case TLI::TKIND_COCLASS:
			case TLI::TKIND_ENUM:
				return true;
			case TLI::TKIND_RECORD:
				return true;
				/*
					string unwrapped_type = nso.get_name_with_ns(get_unaliased_typeinfo(), true);
					if ( unwrapped_type == "::GUID")
				if (nso.get_name_with_ns(get_unaliased_typeinfo(), true) == "::GUID") return true;
				return true;
//				return false;
				*/
			case TLI::TKIND_ALIAS:
			default:
				return false;
			}
			break;
		}
	default:
		return false;
	}
}

bool
Type::requires_pointer_initialise() const
{

	int vt = get_unaliased_vartype();

	if (vt & TLI::VT_ARRAY) return get_pointer_level() == 1;
	switch (vt) {
		case TLI::VT_BSTR:
		case TLI::VT_DISPATCH:
		case TLI::VT_VARIANT:
		case TLI::VT_UNKNOWN:
			return get_pointer_level() == 1;
		case TLI::VT_EMPTY:
		{
			TypeInfoPtr ti = vti_->TypeInfo;
			switch (get_unaliased_typekind()){
				case TLI::TKIND_INTERFACE:
				case TLI::TKIND_DISPATCH:
					return get_pointer_level() == 1;
				case TLI::TKIND_RECORD:
				case TLI::TKIND_ALIAS:
				default:
					return false;
			}
			break;
		}
		default:
			return false;
	}
}


string
Type::get_pointer_initialise(const string &var) const
{
	int vt = get_unaliased_vartype();

	if (vt & TLI::VT_ARRAY)
		return "(*" + var + ") = 0";
	switch (vt) {
		case TLI::VT_VARIANT:
			return "::VariantInit(" + var + ")";
		case TLI::VT_BSTR:
		case TLI::VT_DISPATCH:
		case TLI::VT_UNKNOWN:
			return "(*" + var + ") = 0";
		case TLI::VT_EMPTY:
		{
			TypeInfoPtr ti = vti_->TypeInfo;
			switch (get_unaliased_typekind()){
				case TLI::TKIND_INTERFACE:
				case TLI::TKIND_DISPATCH:
					return "(*" + var + ") = 0";
			}
			break;
		}
	}
	return "";
}

void InterfaceMember::update() {
		ParametersPtr ps = mi_->Parameters;
		has_retval_ = false;
		has_disp_retval_ = false;

//		if (mi_->Name == _bstr_t("DoesContainPoint")) __asm int 3

		for (int i=1; i<=ps->Count; ++i) {
			Parameter *pP = new Parameter(ps->Item[i]);

			parameters_.push_back(pP);

			if (pP->get_direction() == Parameter::DIR_RETVAL) {
				if (i < ps->Count) throw std::runtime_error("Methods retval parameter is not rightmost");
				has_retval_ = true;
				retval_ = pP;
			}
		}

		if (itf_->is_dispinterface()) {
			Type type(mi_->ReturnType);

			if (!type.is_void_or_hresult())
			{
				if (has_retval_) throw std::logic_error("Assertion failed");
				switch (force_ik_ ? force_ik_ : mi_->GetInvokeKind())
				{
				case TLI::INVOKE_PROPERTYGET:


					has_retval_ = true;
					if (pointer_level(mi_->ReturnType) == 0)
					{
						has_disp_retval_ = true;
						retval_ = new Parameter(mi_->ReturnType, 1, true);
						disp_retval_type_ = new Parameter(mi_->ReturnType, 1, true);
					} else {
						retval_ = new Parameter(mi_->ReturnType, 0, true);
//						disp_property_type_ = new Parameter(mi_->ReturnType, 0);
						parameters_.push_back( new Parameter(mi_->ReturnType, 0, true ) );
					}
					break;
				case TLI::INVOKE_PROPERTYPUT:
				case TLI::INVOKE_PROPERTYPUTREF:
//					disp_property_type_ = new Parameter(mi_->ReturnType, 0, false );
//					is_disp_property_ = true;

					parameters_.push_back( new Parameter(mi_->ReturnType, 0, false ) );
					break;
				default:
					has_retval_ = true;
					if (pointer_level(mi_->ReturnType) == 0)
					{
						has_disp_retval_ = true;
						retval_ = new Parameter(mi_->ReturnType, 1, true);
						disp_retval_type_ = new Parameter(mi_->ReturnType, 1, true);
					} else {
						retval_ = new Parameter(mi_->ReturnType, 0, true);
						parameters_.push_back( new Parameter(mi_->ReturnType, 0, true ) );
					}
				}
			}

		}
	}

string InterfaceMember::get_interface_name() { return itf_->get_name(); }

string InterfaceMember::get_wrapper(const string& className) {
	std::ostringstream os;

	os << get_wrapper_function_header(false, className) << "\n";
	os << "{\n";

	if (has_retval_) {
		if (has_disp_retval_)
			os << "    VARIANT _result_;\n";
		else
			os << "    " << retval_->get_type().as_raw_retval(true) << " _result_;\n";
	}

	if( ! is_dispmember_ )
	{
		os << "    HRESULT _hr_ = reinterpret_cast<" << itf_->get_name() << "*>(this)->" << get_raw_name() << "(";
		bool first = true;
		for (PARAMETERS::const_iterator it = parameters_.begin(); it != parameters_.end(); ++it) {
			if (first)
				first = false;
			else
				os << ", ";

			if ((*it)->get_direction() == Parameter::DIR_RETVAL) {
				os << "&_result_";
				break;
			}

			os << (*it)->as_convert();
		}

		os << ");\n";
	}
	else
	{
//		os << "    HRESULT _hr_ = S_OK;\n";
		os  << get_dispatch_prelude_impl("    ")
			<< get_dispatch_raw_impl("    ", false,"" );
//			<< "    _hr_ = result;\n";
	}

	os << "    if (FAILED(_hr_)) throw_com_error(reinterpret_cast<" << itf_->get_name() << "*>(this), _hr_);\n";

	if (has_retval_) {
		if (has_disp_retval_)
			os << "    return " << retval_->get_type().get_return_constructor( retval_->get_type().get_variant_member("_result_", false, true), true) <<";\n";
		else
			os << "    return " << retval_->get_type().get_return_constructor("_result_", false) <<";\n";
	}

	os << "}\n";

	return os.str();
}



void
process_file( const std::string &filename, LibraryOptions &options, const std::string& rs )
{
	TypeLibrary tl(options);

	required_tlbs.clear();

	if (rs.size() > 0)
		tl.load((filename + "\\" + rs).c_str());
	else
		tl.load(filename.c_str());



	// Set up the
	nso.set_library_name(tl.get_raw_name() );
	nso.set_namespace_name(options.get_namespace_override());
	// Get the overloaded name
	options.set_library_name(nso.namespace_name());

	// Get the comment to insert if there is one.
	std::string comment = nso.get_namespace_entry("@comment");
	if (!comment.empty())
	{
		comment.insert(0, "//");
		comment.append("\n");
	}

	bool fos_open = false;
	std::ofstream fos;
	std::ostream *pos;

	string fn;

	tl.process();
	if ( options.output_h())
	{

		fn = options.make_output_filename(".h");
		if( !fn.empty())
		{

			std::cout << filename << (options.wizard_mode()?">>": "->") << fn.c_str() << "\n";

			fos.open(fn.c_str(), (options.wizard_mode()?(std::ios_base::out|std::ios_base::app|std::ios_base::ate)
						:(std::ios_base::trunc | std::ios_base::out)));
			fos_open = true;
			pos=&fos;
		}
		else
		{
			pos = &std::cout;
		}



		std::ostream &os = *pos;

		if( options.wizard_mode() )
		{
			nso.ignore_namespace( !options.use_namespaces());
//			if( !options.use_namespaces() ) ignore_namespace=true;

			if (options.output_library() )
				os << tl.get_wrapper() << '\n';

			switch( options.wizard_type() )
			{
				case 'w':
					os <<  tl.get_coclass_stubs( "\n" );
					break;
				case 'W':
					os <<  tl.get_coclass_stubs( "\n//\n// Coclasses\n//\n\n" );
					break;
				case 'I':
					os << tl.get_interface_stubs("\n");
					break;
				case 'c':
					os << tl.get_coclass_names();
					break;
				case 'i':
					os << tl.get_interface_names();
					break;
			}
			return;
		}

		if (!os) throw std::exception("Failed to open output file for writing.");

		os << "//\n// Generated by tlb2h for Comet " COMET_STR_(COMET_MAJOR_VER) " " COMET_STR_(COMET_BUILDTYPE) " "
			  COMET_STR_(COMET_MINOR_VER)" (" COMET_STR_(COMET_BUILD)")\n//\n\n"
		<< comment // Insert extra comment if needed
		<< "#ifndef COMET_TYPELIB_" << nso.namespace_name() << "_INCLUDED\n"
		"#define COMET_TYPELIB_" << nso.namespace_name() << "_INCLUDED\n\n"
		"#pragma pack(push, 8)\n\n"

		"#include <comet/config.h>\n"

		"#if COMET_BUILD != " COMET_STR_(COMET_BUILD) "\n"
		"#error This header was generated with a different version of tlb2h. Please rebuild\n"
		"#endif\n\n"

		"#include <comet/assert.h>\n#include <comet/ptr.h>\n"
		"#include <comet/util.h>\n#include <comet/uuid.h>\n"
		"#include <comet/common.h>\n" ;
		os << tl.get_library_includes();


		if ( options.server_mode() | options.nutshell_mode()) os << "\n#include <comet/server.h>\n";

		os << "\nnamespace comet {\n";

		nso.enter_namespace();
		//namespace_name = options.namespace_name();

		os << "\nnamespace " << nso.namespace_name() << " {\n\n";

		os << "// VC workaround. (sigh!)\n";
		os << "struct DECLSPEC_UUID(\"" << tl.get_uuid() << "\") " << nso.namespace_name() << "_type_library;\n";
		os << "typedef " << nso.namespace_name() << "_type_library type_library;\n\n";


		os << tl.get_interface_forward_decls("//\n// Forward references and typedefs\n//\n\n");

		os << tl.get_interface_typedefs("//\n// Interface typedefs\n//\n\n") ;

		if( tl.has_server_wrappers() )
			os << tl.get_interface_impl_forward_decl("//\n// Impl Wrappers (forward declaration)\n//\n\n");

		if (options.nutshell_mode())
			os << tl.get_interface_nutshell_forward_decl("//\n// Nutshell Wrappers (forward declaration)\n//\n\n");

		os << tl.get_coclass_defs("//\n// Coclass definitions\n//\n\n") << std::endl;

		os << tl.get_definition();

		os << "} // namespace\n\n";
		nso.leave_namespace();

		os << "//\n// Named GUIDs\n//\n\n";
		os << tl.get_named_guids();
		os << std::endl;

		oneoff_output_to_stream os_ns ( os, "\nnamespace " + nso.namespace_name() + " {\n\n");
		nso.enter_namespace();

		os_ns << tl.get_item_defs("//\n// Record/Union definitions\n//\n\n");

		os_ns << tl.get_enum_defs("//\n// Enum declarations\n//\n\n");

		nso.leave_namespace();

		const std::string &trait_specialisation = tl.get_enum_sa_trait_specialisations("//\n// Enum sa_trait declarations\n//\n\n");
		if( !trait_specialisation.empty() )
		{
			os_ns.close( "} // namespace\n\n");
			os << trait_specialisation;
			os_ns.open();
		}

		//os << "\nnamespace " << nso.namespace_name() << " {\n";
		nso.enter_namespace();

		os_ns << tl.get_alias_defs("//\n// Typedef declarations\n//\n\n") << std::endl;

		os_ns << tl.get_constant_defs("//\n// Module constants\n//\n\n") << std::endl;

		os_ns << tl.get_interface_decls("//\n// Interface declarations\n//\n\n");

		nso.leave_namespace();

		std::ostringstream os2;

		os2 << tl.get_wrapper_decls("//\n// Interface wrapper declarations\n//\n\n");

		os2 << tl.get_interface_wrappers("//\n// Method wrappers\n//\n\n");

		if( !os2.str().empty() )
		{
			os_ns.close( "} // namespace\n\n");
			os << os2.str();
			os_ns.open();
		}

		nso.enter_namespace();

		if( tl.has_server_wrappers() )
		{
			os_ns << tl.get_interface_impl_decl("//\n// Impl Wrappers (declaration)\n//\n\n");

			os_ns << tl.get_interface_impl("//\n// Impl Wrappers (implementation)\n//\n\n");
		}

		if (options.nutshell_mode()) {

			os_ns << tl.get_interface_nutshell_decl("//\n// Nutshell Wrappers (declaration)\n//\n\n");

			os_ns << tl.get_interface_nutshell("//\n// Nutshell Wrappers (implementation)\n//\n\n");

		}

		os_ns.close( "} // end of namespace\n\n");

		nso.leave_namespace();

		/*	os << "//\n// Interface property wrappers\n//\n\n";
			os << "#ifdef COMET_ALLOW_DECLSPEC_PROPERTY\n";
			os << tl.get_interface_prop_wrappers() << std::endl;
			os << "#endif\n";*/

		if( tl.has_server_wrappers() )
		{
			os << tl.get_connection_point_impls( "//\n// Connection Point implementation\n//\n\n");
		}

		os << "} // end of namespace\n\n";
		os << "#pragma pack(pop)\n";
		os << "#endif\n";
		if(fos_open)
		{
			fos.close();
			fos_open=false;
		}
	}

	if (required_tlbs.empty() == false) {
		std::cout << "  Requires:" << std::endl;
		for (std::set<string>::const_iterator it = required_tlbs.begin(); it != required_tlbs.end(); ++it)
			std::cout << "    " << *it << std::endl;
	}
	if (options.output_html())
	{

		fn = options.make_output_filename(".htm");
		if( !fn.empty())
		{

			std::cout << filename << (options.wizard_mode()?">>": "->") << fn.c_str() << "\n";

			fos.open(fn.c_str(), (options.wizard_mode()?(std::ios_base::out|std::ios_base::app|std::ios_base::ate)
						:(std::ios_base::trunc | std::ios_base::out)));
			pos=&fos;
		}
		else
		{
			pos = &std::cout;
		}

		std::ostream &os = *pos;

		os << "<!DOCTYPE HTML PUBLIC ""-//W3C//DTD HTML 4.0 Transitional//EN"">\n";
		os << "<html><head><title>" << nso.namespace_name() << "</title><meta name=""GENERATOR"" content=""tlb2h""></head>\n";
		os << "<body>\n";

		os << "<h1>" << nso.namespace_name() << "</h2>\n";
		nso.enter_namespace();
		os << "<h2>Content:</h2>\n";

		os << "<h3>Coclasses:</h3>\n";
		os << tl.get_coclass_html_list();

		os << "<h3>Interfaces:</h3>\n";
		os << tl.get_interface_html_list();

		os << "<h2>Details:</h2>";
		os << tl.get_interface_html_detail();

		nso.leave_namespace();

		os << "</body></html>";

		if(fos_open)
		{
			fos.close();
			fos_open=false;
		}
	}
}

int main(int argc, char* argv[])
{
	CoInitialize(0);

	try {
		std::list<std::string> inputfiles;
		std::string output_dir;
		bool has_error=false;

		LibraryOptions options;
		options.server_mode(true);
		bool decided_mode = false;
		options.wizard_mode(false,0);
		bool show_version = false;

		for(int i=1;i< argc && !has_error; ++i)
		{
			bool next_is_option = ((i+1) < argc) && (argv[i+1][0] == '-' && argv[i+1][0] == '/');
			if( argv[i][0]=='-' || argv[i][0]=='/')
			{
				if(argv[i][1]=='\0')
				{
					has_error=true;
					break;
				}
				char argv2=argv[i][2];

				bool has_plus = (argv2 == '+') ;
				bool has_minus = (argv2 == '-') ;
				if( has_plus || has_minus )
				{
					argv2=argv[i][3];
				}
				bool old_decided=decided_mode;
				bool old_wizard= options.wizard_mode();
				switch(argv[i][1])
				{
					case 'c':
						decided_mode=true;
						options.wizard_mode(false,0);
						options.server_mode(false);
						break;
					case 'n':
						decided_mode=true;
						options.wizard_mode(false,0);
						options.nutshell_mode(true);
						break;

					case 'o':
						if(!next_is_option )
						{
							++i;
							options.set_output_file(argv[i],!options.wizard_mode());
						}
						else
						{
							throw std::exception("Expecting filename after -o");
						}
						break;
					case 's':
						if(!next_is_option )
						{
							++i;
							options.pare_symbols(has_minus);
							decided_mode=true;
							options.wizard_mode(false,0);
							options.load_symbols( argv[i] );
						}
						else
						{
							throw std::exception("Expecting filename after -s");
						}
						break;
					case 'h':
							options.output_html(argv[i][2]!='-');
						break;
					case 'H':
						options.output_html(true);
						options.output_h(false);
						break;

					case 'x':
						if(!next_is_option )
						{
							++i;
							decided_mode=true;
							options.pare_symbols(has_minus);
							options.wizard_mode(false,0);
							options.load_symbols( argv[i], true );
						}
						else
						{
							throw std::exception("Expecting filename after -x");
						}
						break;
					case 'r':
						if(!next_is_option  )
						{
							++i;
							options.set_namespace_override(argv[i]);
						}
						else
						{
							throw std::exception("Expecting namespace after -r");
						}
						break;
					case 'R':
						if(!next_is_option)
						{
							++i;
							nso.load_namespaces( argv[i]);
						}
						else
						{
							throw std::exception("Expecting filename after -R");
						}
						break;
					case 'w':
							
						if(!next_is_option )
						{
							char mode = 'w';
							if (argv2 == 'i')
							{
								mode = 'I';
								argv2 = 0;
							}
							++i;
							decided_mode=true;
							options.wizard_mode(true, mode);

							options.parse_symbols( argv[i] );
							options.output_is_directory(false);
							options.use_namespaces(has_plus);
						}
						else
						{
							throw std::exception("Expecting filename after -s");
						}
						break;
					case 'W':
						switch(argv2)
						{
							case 'c':
							case 'i':
								decided_mode=true;
								options.wizard_mode(true,argv2);
								options.output_is_directory(false);
								options.use_namespaces(has_plus);
								argv2 = 0;
								break;
							default:
								decided_mode=true;
								options.wizard_mode(true,'W');
								options.output_is_directory(false);
								options.use_namespaces(has_plus);
								break;
						}
						break;
					case 'F':
						options.exclude_foreign(true);
						break;
					case 'v':
						show_version = true;
						break;
					case 'L':
						do_log_interfaces = true;
						break;
					case 'C':
						if (!next_is_option)
						{
							++i;
							nso.add_namespace_mapping( "@comment", argv[i]);
						}
						else
						{
							throw std::runtime_error("Comment expected");
						}
						break;
					case 'l':
						options.wizard_mode(true,'l');
						options.output_library(true);
						options.output_is_directory(false);
						decided_mode = true;
						break;

#ifndef NDEBUG
					case 'D':
						if (next_is_option)
							g_debug = 1;
						else
							std::istringstream(argv[++i]) >> g_debug;
						break;
#endif // NDEBUG
				}
				if(old_decided==decided_mode && old_wizard != options.wizard_mode())
				{
					throw std::exception("Canot mix two modes of operation");
				}
				if (argv2 != '\0' )
				{ has_error = true; break; }
			}
			else
			{
				inputfiles.push_back( argv[i]);
			}
		}

		if( show_version || has_error || inputfiles.empty() )
		{
			std::cout << "Comet header generator.  Version " COMET_STR_(COMET_MAJOR_VER) " " COMET_STR_(COMET_BUILDTYPE) " "
			  COMET_STR_(COMET_MINOR_VER)" (" COMET_STR_(COMET_BUILD)")\n"
			  COMET_COPYRIGHT "\n\n";
			if(show_version && inputfiles.empty())
				goto end;
		}

		if(has_error || inputfiles.empty())
		{
			std::cout <<
				"usage: tlb2h [-c] [-n] [-o directory] [-s[+-] symbolfile] [-x symbolfile] [-r namespace] [-R symboltransfile] {typelibraries}\n"
				"  -c                   Client wrappers (exclude server wrappers)\n"
				"  -n                   Include nutshell wrappers\n"
				"  -o {directory}       Specify output directory\n"
				"  -r {namespace}       Rename library namespace\n"
				"  -F                   Exclude foreign source implementations\n"
				"  -h[+-]               Ouput/suppress html (default output)\n"
				"  -H                   Output only html\n"
				"  -R {translate file} Specify namespace/symbol mapping file\n"
				"            {OriginalSymbol}={NewSymbolName}\n"
				"            @comment={header comment}\n"
				"  -x {symbol file}     Exclude symbols in file\n"
				"  -s[+-] {symbol file} Output only symbols in file\n"
				"            [~*]{Symbol}\n"
				"  -L                   Output logging for interface\n"
				"  -v                   Output version\n"
				"  -C \"{comment}\"     Set a comment to be inserted in the header files (also @comment=)\n"
				"Wizard: tlb2h [-w[+] coclass[{,coclass}] | -wi[+] interface[{,interface}] | -W] [-o outputfile] typelibrary\n"
				"  -w[+] {coclass list} Specify comma separated coclasses to output coclass-stubs for.\n"
				"                       '+' to qualify types with namespaces\n"
				"  -wi[+] {interface list} Specify comma separated interfaces to output interfaces-stubs for.\n"
				"                       '+' to qualify types with namespaces\n"
				"  -W[+]                Output all coclasses. '+' to qualify types with namespaces\n"
				"  -Wc                  Output class names only.\n"
				"  -Wi                  Output interface names only.\n"
				"  -l                   Output library header\n"
				"  -o {filename}        Specify output file. Defaults to stdout.\n"
#ifndef NDEBUG
				"  -D {log level}       Logging\n"
#endif
			;
			goto end;
		}


		for(std::list<string>::const_iterator it = inputfiles.begin(); it!=inputfiles.end(); ++it)
		{
			WIN32_FIND_DATA	fileData;
			HANDLE hSearch;
			std::string rs;
			std::string fn = *it;
			// First check if there is a trailing resource specifier
			std::string::size_type where = fn.find_last_of("\\/");
			if (where != it->npos)
			{
				rs.assign( fn, where + 1, it->size() - where);
				if (rs.find_first_not_of("0123456789") == std::string::npos) {
					// rs is the resource specifier, now strip it from the filename

					fn = string(fn, 0, where);
				} else rs = "";
			}


			where = fn.find_last_of("\\/");
			if(where == fn.npos)
				where = 0;
			else
				++where;
			std::string basedir(fn,0,where);

			hSearch = FindFirstFile(fn.c_str() , &fileData );

			if ( hSearch == INVALID_HANDLE_VALUE )
			{
				char fn2[512];
				char* last;
				SearchPath( 0, fn.c_str(), 0, 512, fn2, &last);

				process_file( fn2, options, rs );

//				throw std::exception("Unable to find file");
			}
			else
			{
				do
				{
					if ( fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
					{
						continue;
					}
					std::string full_name= (basedir + fileData.cFileName);

					process_file( full_name, options, rs );

					// continue searching until all files have been attempted
				} while ( FindNextFile( hSearch, &fileData ) );

				// finished searching for files
				FindClose( hSearch );
			}
		}

		int i = 0;
	} catch (_com_error& err) {
		bstr_t desc = err.Description();
		if (!desc)
			std::cerr << "tlb2h: " << err.ErrorMessage() << std::endl;
		else
			std::cerr << "tlb2h: " << static_cast<const char*>(desc) << std::endl;
		CoUninitialize();
		return -1;
	} catch (std::exception& err) {
		std::cerr << "tlb2h: " << err.what() << std::endl;
		CoUninitialize();
		return -1;
	}

end:
	CoUninitialize();

	return 0;
}
