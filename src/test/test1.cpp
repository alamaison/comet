#define COMET_ASSERT_THROWS_ALWAYS

#include <comet/comet.h>
#include <comet/server.h>
#include <comet/datetime.h>
#include <comet/safearray.h>
#include <comet/unittest.h>
#include <comet/lw_lock.h>
#include <comet/uuid.h>
#include <iostream>
#include <sstream>
#include <exception>

using namespace std;
using namespace comet;

// An interface declaration used by some of the tests
struct IDummyInterface : public IUnknown
{
};

template<>
struct comet::comtype<IDummyInterface>
{
	typedef comet::nil base;
	static const IID& uuid() {
		static const GUID iid =
		{ 0x546aefac, 0x6ecc, 0x44bf, { 0x8d, 0x17, 0x13, 0x83, 0x46, 0x24, 0xd4, 0x15 } };
		return iid;
	}
};

// Compile-time tests:
// 1. com_ptr<IForward> should be usable even though IForward is not defined.
struct IForward;

com_ptr<IForward> return_forward();

void accept_forward(const com_ptr<IForward>&);

#if 0
Visual C++ lets you compile this:
struct astruct
{
	com_ptr<IForward> forward_;
};

// but Intel does not -
// it would be nice if you could as it would mean that you
// could arbitrarily replace raw pointers with smart
// pointers.

// I'm not sure which compiler is correct here.
#endif

// 2. http://groups.yahoo.com/group/tlb2h/message/25
// There was an error in the definition of impl::sa_traits
// for char and unsigned char instantiations.
// While we are at it, check for the other safearray
// traits too.

// NOTE: This function is never meant to be
// executed - the test is purely to make sure that
// it compiles with no warnings or errors.

template<typename T>
struct check_array
{
	static void check() { T *a = &safearray_t<T>()[0]; }
};
template<typename T, typename S>
struct check_array_special
{
	static void check() { T *a = &safearray_t<S>()[0]; }
};

void check_safearray_traits()
{
	check_array<char>::check();
	check_array<unsigned char>::check();
	check_array<long>::check();
	check_array<unsigned long>::check();
	check_array<short>::check();
	check_array<unsigned short>::check();
	check_array<float>::check();
	check_array<double>::check();
	check_array<com_ptr<::IUnknown> >::check();
	check_array<com_ptr<::IDispatch> >::check();
	check_array<currency_t>::check();
	check_array<datetime_t>::check();
	check_array<variant_bool_t>::check();
	check_array_special<variant_bool_t, bool>::check();
}

// 3. http://groups.yahoo.com/group/tlb2h/message/18
void check_upcast()
{
	com_ptr<IDummyInterface> d;
	com_ptr<IUnknown> unknown;
	unknown = d;
}

// This tests for a reference counting bug encountered in Comet A5 -
// the destructor for com_ptr<> was missing so Release() was never called.
template<>
struct comet::test<0>
{
	void run()
	{
		long count_before = comet::module().rc();
		bool destructor_executed;
		{
			struct A : public simple_object<IDummyInterface>
			{
				bool &destructor_executed_;
				A(bool &destructor_executed) : destructor_executed_(destructor_executed) {}
				~A() { destructor_executed_ = true; }
			};
			com_ptr<IDummyInterface> pA = new A(destructor_executed);
			if(comet::module().rc() != count_before + 1)
				throw runtime_error("Reference count not incremented!");
		}
		if(destructor_executed != true)
			throw runtime_error("Destructor not executed!");
		if(comet::module().rc() != count_before)
			throw runtime_error("Reference count left changed!");
	}
};

// This tests for a bug we found where the assignment operator for key_base was missing.
template<>
struct comet::test<1>
{
	void run()
	{
		regkey k1 = regkey(HKEY_LOCAL_MACHINE).open("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_READ );
		regkey k2;
		k2 = k1;
		k1.close();
		// This will fail if it is the exact same bug that we ran into before.
		k2.flush();
	}
};

// This tests for a VC++ 6.0 bug where the compiler-generated assignment operator for name_iterator
// was incorrect.
template<>
struct comet::test<2>
{
	void run()
	{
		regkey::info_type::subkeys_type::iterator it;
		{
			regkey key = regkey(HKEY_LOCAL_MACHINE).open("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_READ );
			regkey::subkeys_type subkeys = key.enumerate().subkeys();

			// Invoke assignment operator for name_iterator
			it = subkeys.begin();
			key.close();
			if(it == subkeys.end())
				throw runtime_error("There are no subkeys - this is a very strange Windows installation!");
		}
		// Dereference the key iterator after everything else has been closed.
		*it;
	}
};

// This tests for this bug:
// http://groups.yahoo.com/group/tlb2h/message/27
// Note however, if it's the exact same bug as before,
// it will access violate rather than throwing an exception.
template<>
struct comet::test<3>
{
	void run()
	{
		BSTR v = 0;
		bstr_t s(v);
		if(s.w_str().length() != 0)
			throw runtime_error("w_str() broken");
		if(s.s_str().length() != 0)
			throw runtime_error("s_str() broken");
	}
};

// This one doesn't actually get to throw the runtime error -
// it just access violates
template<>
struct comet::test<4>
{
	void run()
	{
		bstr_t g;
		if(g != 0)
			throw runtime_error("operator!= busted");
	}
};

template<>
struct comet::test<5>
{
	void run()
	{
		typedef make_list< int, long, int >::result LIST;
		if (typelist::index_of< LIST, long >::value != 1) throw runtime_error("typelist::index_of is broken");
	}
};

struct Test_fun
{
	int operator()(int x, int y) { return x + y; }
};

int myFunction(int x,  int y) { return x + y; }
void myFunction2(int x,  int y) { }

struct Test1
{
	double foo(const double& x, int y) { return x + y;}
};

template<>
struct comet::test<6>
{
	void run()
	{
		//
	}
};

template<>
struct comet::test<7>
{
	variant_t foo() { return L"foo";  }

	void s1(const std::string&) {}
	void s2(const std::wstring&) {}
	void s3(const bstr_t&) {}
	void s4(const wchar_t*) {}

	void run()
	{

		bstr_t s = foo();
		bstr_t s1( foo() );
		std::wstring t;

		bstr_t s0( foo() );
		s = foo();

		t = s.w_str();

	}
};

template<typename U>
void compile_operators(const U& y)
{
	variant_t x;

	x != y;
	x == y;
	x <  y;
	x <= y;
	x >  y;
	x >= y;

	y != x;
	y == x;
	y <  x;
	y <= x;
	y >  x;
	y >= x;

	variant_t v1(y);
	variant_t v2 = y;

	v1 = y;
}

template<typename U>
void compile_operators2(const U& y)
{
	variant_t x;

	x != y;
	x == y;
	x <  y;
	x <= y;
	x >  y;
	x >= y;

	y != x;
	y == x;
	y <  x;
	y <= x;
	y >  x;
	y >= x;

	x != U();
	x == U();
	x <  U();
	x <= U();
	x >  U();
	x >= U();

	U() != x;
	U() == x;
	U() <  x;
	U() <= x;
	U() >  x;
	U() >= x;

	variant_t v1(y);
	variant_t v2 = y;

	v1 = y;
}

template<>
struct comet::test<8>
{

	void run()
	{
		variant_t v = 8;
		variant_t w = short(8);

		if (v != w) throw std::runtime_error("variant_t is broken");

		// Checking that variant_t comparisons can be compiled
		compile_operators2(variant_t());

		compile_operators2(short());
		compile_operators2(long());
		compile_operators2(int());
		compile_operators2(float());
		compile_operators2(double());

//		compile_operators2(bstr<true>());
		compile_operators2(bstr_t());
		compile_operators2(variant_t());
		compile_operators("");
		compile_operators(L"");

		compile_operators2(std::wstring());
		compile_operators2(std::string());

	}
};


template<>
struct comet::test<9>
{
	void run()
	{
		IStorage* itf2 = 0;

		com_ptr<IPersist> itf1( com_cast(itf2) );

		itf1 = com_cast(itf1);

	}
};

template<>
struct comet::test<10>
{
	void run()
	{
		variant_t v1 = L"foo";
		variant_t v2( L"foo" );
		variant_t v3; v3 = L"foo";

		std::wstring s1 = v1;
		std::wstring s2( v2 );
		std::wstring s3; s3 = v3;

		if (s1 != L"foo") throw std::runtime_error("variant_t/wstring conversion is broken");
		if (s2 != L"foo") throw std::runtime_error("variant_t/wstring conversion is broken");
		if (s3 != L"foo") throw std::runtime_error("variant_t/wstring conversion is broken");

		variant_t v1_ = s1;
		variant_t v2_( s2 );
		variant_t v3_; v3_ = s3;

		if (v1_ != L"foo") throw std::runtime_error("variant_t/wstring conversion is broken");
		if (v2_ != L"foo") throw std::runtime_error("variant_t/wstring conversion is broken");
		if (v3_ != L"foo") throw std::runtime_error("variant_t/wstring conversion is broken");


	}
};

template<>
struct comet::test<11>
{

	void dummy(const bstr_t&) {}

	void run()
	{
		variant_t v1 = L"foo";
		variant_t v2( L"foo" );
		variant_t v3; v3 = L"foo";

		bstr_t s1 = v1;
		bstr_t s2( v2 );
		bstr_t s3; s3 = v3;

		if (s1 != L"foo") throw std::runtime_error("variant_t/wstring conversion is broken");
		if (s2 != L"foo") throw std::runtime_error("variant_t/wstring conversion is broken");
		if (s3 != L"foo") throw std::runtime_error("variant_t/wstring conversion is broken");

		variant_t v1_ = s1;
		variant_t v2_( s2 );
		variant_t v3_; v3_ = s3;

		if (v1_ != L"foo") throw std::runtime_error("variant_t/wstring conversion is broken");
		if (v2_ != L"foo") throw std::runtime_error("variant_t/wstring conversion is broken");
		if (v3_ != L"foo") throw std::runtime_error("variant_t/wstring conversion is broken");


		dummy(v1);
	}
};

template<>
struct comet::test<12>
{
	void run()
	{
		DATE d1 = 0;
		datetime_t d2( d1 );
	}
};

template<>
struct comet::test<13>
{
	void run()
	{
		{
			currency_t c1(12.3);
			currency_t c2 = 123;
			currency_t c3 = 123L;
		}

		{
			currency_t c1(12.3);
			if (c1 != 12.3) throw std::runtime_error("currency_t is broken");
			c1 = 12.3;
			if (c1 != 12.3) throw std::runtime_error("currency_t is broken");

			currency_t c2(123);
			if (c2 != 123) throw std::runtime_error("currency_t is broken");
			c2 = 123;
			if (c2 != 123) throw std::runtime_error("currency_t is broken");

			currency_t c3(123L);
			if (c3 != 123L) throw std::runtime_error("currency_t is broken");
			c3 = 123L;
			if (c3 != 123L) throw std::runtime_error("currency_t is broken");
		}

		currency_t c( 123.456 );
		variant_t v = c;

		if (v != 123.456) throw std::runtime_error("currency_t/variant_t conversions broken");

	}
};


template<>
struct comet::test<14>
{
	void run()
	{
		safearray_t<long> sa(6, 1);
		{
			sa[1] = 2;
			sa[2] = 6;
			sa[3] = 4;
			sa[4] = 3;
			sa[5] = 1;
			sa[6] = 5;
		}
#ifdef _DEBUG
		// Check safearray debug iterators.

		safearray_t<long> sa_other(1,1);
		try {
			if (sa_other.begin() == sa.begin()) { }
			throw std::runtime_error("safearray_t: debug iterator busted - comparison between foreign iterators");
		} catch (const assert_failed &) { /*Expected*/ }
		try {
			if (sa_other.begin() < sa.begin()) { }
			throw std::runtime_error("safearray_t: debug iterator busted - comparison between foreign iterators");
		} catch (const assert_failed &) { /*Expected*/ }
		try {
			if (sa_other.begin() > sa.begin()) { }
			throw std::runtime_error("safearray_t: debug iterator busted - comparison between foreign iterators");
		} catch (const assert_failed &) { /*Expected*/ }
		try {
			if (sa_other.begin() <= sa.begin()) { }
			throw std::runtime_error("safearray_t: debug iterator busted - comparison between foreign iterators");
		} catch (const assert_failed &) { /*Expected*/ }
		try {
			if (sa_other.begin() >= sa.begin()) { }
			throw std::runtime_error("safearray_t: debug iterator busted - comparison between foreign iterators");
		} catch (const assert_failed &) { /*Expected*/ }
		try {
			sa_other[2] = 0;
			throw std::runtime_error("safearray_t: debug iterator busted - out of range access");
		} catch (const assert_failed &) { /*Expected*/ }
		try {
			--sa_other.begin();
			throw std::runtime_error("safearray_t: debug iterator busted - iterator out of range");
		} catch (const assert_failed &) { /*Expected*/ }
		try {
			*sa_other.end() = 0;
			throw std::runtime_error("safearray_t: debug iterator busted - assignment to end()");
		} catch (const assert_failed &) { /*Expected*/ }
		try {
			++sa_other.end();
			throw std::runtime_error("safearray_t: debug iterator busted - iterate beyond end()");
		} catch (const assert_failed &) { /*Expected*/ }
		try {
			sa_other.begin()+3;
			throw std::runtime_error("safearray_t: debug iterator busted - assign beyond end()");
		} catch (const assert_failed &) { /*Expected*/ }

#endif

		std::sort(sa.begin(), sa.end());

		safearray_t<long> sa2 = sa;
		{

			if (sa2[1] != 1 ||
				sa2[2] != 2 ||
				sa2[3] != 3 ||
				sa2[4] != 4 ||
				sa2[5] != 5 ||
				sa2[6] != 6) throw std::runtime_error("safearray_t is busted!");
		}

		std::sort(sa2.rbegin(), sa2.rend());
		if (sa2[1] != 6 ||
			sa2[2] != 5 ||
			sa2[3] != 4 ||
			sa2[4] != 3 ||
			sa2[5] != 2 ||
			sa2[6] != 1)
		{
			throw std::runtime_error("safearray_t reverse sort is busted!");
		}

		sa2.erase(sa2.begin() +2);
		if (sa2.size() != 5 ||
			sa2[1] != 6 ||
			sa2[2] != 5 ||
			sa2[3] != 3 ||
			sa2[4] != 2 ||
			sa2[5] != 1)
		{
			std::cout << sa2.size()<< ':';
			for (safearray_t<long>::const_iterator it = sa2.begin(); it != sa2.end(); ++it)
			{
				std::cout << *it << ',';
			}
			std::cout << endl;
			throw std::runtime_error("safearray_t erase(it) is busted!");
		}

		sa2.pop_back();
		if (sa2.size() != 4 ||
			sa2[1] != 6 || sa2[2] != 5 || sa2[3] != 3 || sa2[4] != 2 )
		{
			throw std::runtime_error("safearray_t pop_back is busted!");
		}
		sa2.pop_front();
		if (sa2.size() != 3 ||
			sa2[1] != 5 || sa2[2] != 3 || sa2[3] != 2 )
		{
			throw std::runtime_error("safearray_t pop_front is busted!");
		}
		sa2.push_front(9);
		if (sa2.size() != 4 ||
			sa2[1] != 9 || sa2[2] != 5 || sa2[3] != 3 || sa2[4] != 2 )
		{
			throw std::runtime_error("safearray_t push_front is busted!");
		}
		sa2.push_back(9);
		if (sa2.size() != 5 ||
			sa2[1] != 9 || sa2[2] != 5 || sa2[3] != 3 || sa2[4] != 2 || sa2[5] != 9)
		{
			throw std::runtime_error("safearray_t push_back is busted!");
		}

		sa2.erase(sa2.begin(), sa2.end());
		if (sa2.size() != 0)
		{
			throw std::runtime_error("safearray_t erase(begin,end) is busted!");
		}
		try
		{
			sa2.pop_back();
		}
		catch(...)
		{
			throw std::runtime_error("safearray_t pop_back of empty list is busted!");
		}

		safearray_t<bool> sab(5,2);
		sab[6] = true;
		sab[5] = false;
		sab[4] = false;
		sab[3] = false;
		sab[2] = true;

		if ( !sab[2] || sab[3] || sab[4] || sab[5] || !sab[6] ) throw std::runtime_error("safearray_t<bool> is busted");

		std::ostringstream os;
/*		for (safearray_t<long>::reverse_iterator it = sa.rbegin(); it != sa.rend(); ++it)
			os << *it << std::endl;*/
	}
};

template<>
struct comet::test<15>
{
	void run()
	{

		safearray_t<bstr_t> sa( 2, 1 );
		sa[1] = L"String no. 1";
		sa[2] = L"String no. 2";

		SAFEARRAY* psa = sa.detach();

		{
			safearray_t<bstr_t>& sa2 = safearray_t<bstr_t>::create_reference(psa);

			sa2[1] = L"FOO";
		}

		{
			safearray_t<bstr_t>& sa2 = safearray_t<bstr_t>::create_reference(psa);

			if (sa2[1] != L"FOO") throw std::runtime_error("safearray_t<>::create_reference is broken");
		}

	}
};

template<>
struct comet::test<16>
{
	struct MyThread : public thread
	{
		const lw_lock& lock_;

		MyThread(const lw_lock& lock)
			: lock_(lock)
		{}

		DWORD thread_main()
		{
			auto_reader_lock arl(lock_);
			Sleep(100);
			return 0;
		}
	};

	void run()
	{
		lw_lock lock;

		MyThread t(lock);
		t.start();
		{
			auto_writer_lock awl(lock);
			Sleep(100);
		}
		if (WaitForSingleObject(t.handle(), 5000) != WAIT_OBJECT_0) throw std::runtime_error("class thread is broken");

	}
};

template<>
struct comet::test<17>
{
	void run()
	{
		bstr_t bs = L"Sofus";
		int l = bs.length();

		std::string t = "Sofus";
		int l1 = t.length();

		std::string s = bs.s_str();
		int l2 = s.length();

		if (l1 != l2) throw std::runtime_error("bstr_t to std::string conversion is flawed");
		if (s != t) throw std::runtime_error("bstr_t to std::string conversion is flawed");
	}
};

template<>
struct comet::test<18>
{
	void run()
	{
		wchar_t ws1[] = L"foo\0bar"; // 7 characters terminated with a null

		std::wstring ws2( ws1, 7 );

		if (ws2.length() != 7) throw std::logic_error("Test is broken");

		bstr_t bs1 = ws2;

		if (bs1.length() != 7) throw std::runtime_error("bstr_t does not support embedded nulls (failed converting from std::string)");

		if (bs1 != ws2) throw std::runtime_error("bstr_t does not support embedded nulls.");

		const char s1[] = "foo\0bar"; // 7 characters terminated with a null

		std::string s2( s1, 7 );

		if (s2.length() != 7) throw std::logic_error("Test is broken");

		bstr_t bs2 = s2;

		if (bs2.length() != 7) throw std::runtime_error("bstr_t does not support embedded nulls (failed converting from std::wstring)");

		if (bs2 != ws2) throw std::runtime_error("bstr_t does not support embedded nulls.");

		ws2[6] = 'z'; // change string to L"foo\0baz"
		if (bs1 == ws2) throw std::runtime_error("bstr_t does not support embedded nulls (comparison ignores characters after null).");
		if (bs2 == ws2) throw std::runtime_error("bstr_t does not support embedded nulls (comparison ignores characters after null)");

		if (bs1 >= ws2) throw std::runtime_error("bstr_t does not support embedded nulls (comparison ignores characters after null).");
		if (bs2 >= ws2) throw std::runtime_error("bstr_t does not support embedded nulls (comparison ignores characters after null).");

		ws2[4] = 'a'; // change string to L"foo\0aaz"

		if (bs1 <= ws2) throw std::runtime_error("bstr_t does not support embedded nulls (comparison ignores characters after null).");
		if (bs2 <= ws2) throw std::runtime_error("bstr_t does not support embedded nulls (comparison ignores characters after null).");
	}
};

template<>
struct comet::test<19>
{
	void run()
	{
		BSTR s = SysAllocString(L"test");
		VARIANT raw_v;
		VariantInit(&raw_v);
		raw_v.vt = VT_BSTR | VT_BYREF;
		raw_v.pbstrVal = &s;

		{
			const variant_t& v = variant_t::create_reference(raw_v);

			bstr_t bs = v;

		}

		HRESULT hr = VariantClear(&raw_v);
	}
};

template<>
struct comet::test<20>
{
	void run()
	{
		{
			bstr_t s = L"Sofus Mortensen";
			std::sort(s.begin(), s.end());
		}

		{
			bstr_t s;
			std::sort(s.begin(), s.end());
		}

		{
			std::wstring s1 = L"Sofus Mortensen";
			bstr_t s2( s1.begin(), s1.end() );

			s2.assign( s1.begin(), s1.end() );
		}

		{
			bstr_t s(80,  L' ');
		}

	}
};

template<>
struct comet::test<21>
{
	void run()
	{
		bstr_t s = L"foo";
		bstr_t t;

		t = s;
		if (s.begin() == t.begin()) throw std::exception("default assignment operator is not copying the string");
	}
};

template<>
struct comet::test<22>
{
	void run()
	{
		bstr_t s = "foo";
		if (s.length() != 3) throw std::exception("conversion from MBCS to wide char is broken");

		int l = s.length();

		std::string s1 = s;
		if (s1.length() != 3) throw std::exception("conversion from wide char to narrow char is broken");

		std::wstring s2 = s;
		if (s2.length() != 3) throw std::exception("conversion from bstr_t to wstring");
	}
};


template<>
struct comet::test<23>
{
	void run()
	{
		variant_t a(false), b(L"XYZ"), c(true);

		if( a == b || !(a != b) )
			throw std::exception("variant_t::operator== is broken");

		if( b == a || !(b != a) )
			throw std::exception("variant_t::operator== is broken");

		if( a == c || !(a != c) )
			throw std::exception("variant_t::operator== is broken");

		if( b == c || !(b != c) )
			throw std::exception("variant_t::operator== is broken");

		if( c == b || !(c != b) )
			throw std::exception("variant_t::operator== is broken");
	}
};

template<>
struct comet::test<24>
{
	void run()
	{
		uuid_t x = IID_IUnknown;

		{
			uuid_t y ;
			std::stringstream f;
			f << x; f >> y;
			if (!f) throw std::exception("uuid_t: streaming broken - stream is bad");
			if (x != y) throw std::exception("uuid_t: streaming broken");
		}

		{
			uuid_t y ;
			std::wstringstream f;
			f << x; f >> y;
			if (!f) throw std::exception("uuid_t: streaming broken - stream is bad");
			if (x != y) throw std::exception("uuid_t: streaming broken");
		}

		{
			uuid_t y ;
			std::wstringstream f;
			f << x; f >> y >> y;
			if (!f.eof()) throw std::exception("uuid_t: streaming broken - stream should be eof");
			if (x != y) throw std::exception("uuid_t: streaming broken");
		}

		{
			uuid_t y ;
			std::stringstream f;
			f << " \t\n" << x; f >> y;
			if (!f) throw std::exception("uuid_t: streaming broken - stream is bad");
			if (x != y) throw std::exception("uuid_t: streaming broken - no ws skipping");
		}

		{
			uuid_t y ;
			std::wstringstream f;
			f << L" \t\n"<< x; f >> y;
			if (!f) throw std::exception("uuid_t: streaming broken - stream is bad");
			if (x != y) throw std::exception("uuid_t: streaming broken - no ws skipping");
		}

		{
			uuid_t y ;
			std::stringstream f;
			f << x << ',';
			f >> y;
			if (x != y) throw std::exception("uuid_t: streaming broken");
			if (!f) throw std::exception("uuid_t: streaming broken - stream is bad");

			char c = 0;
			f >> c;
			if (c != ',') throw std::exception("uuid_t: streaming broken - ate too much from stream");
		}

		{
			uuid_t y ;
			std::wstringstream f;
			f << x << L","; f >> y;
			if (x != y) throw std::exception("uuid_t: streaming broken");
			if (!f) throw std::exception("uuid_t: streaming broken - stream is bad");

			wchar_t c = 0;
			f >> c;
			if (c != L',') throw std::exception("uuid_t: streaming broken - ate too much from stream");
		}

		{
			uuid_t y;
			std::stringstream f;
			f.exceptions(std::ios::badbit);

			f << "00";
			try {
				f >> y;
				throw std::exception("uuid_t: streaming broken - should have thrown exception - ill format");
			}
			catch (std::ios::failure&)
			{
				// ok
			}

		}

		{
			uuid_t y;
			std::wstringstream f;
			f.exceptions(std::ios::badbit);

			f << L"123123123123123123123123123123123123123";
			try {
				f >> y;
				throw std::exception("uuid_t: streaming broken - should have thrown exception - ill format");
			}
			catch (std::ios::failure&)
			{
				// ok
			}

		}

	}
};

template<>
struct comet::test<25>
{
	void run()
	{
		uuid_t u1 = uuid_t::create();

		bstr_t s1( u1 );

		{
			uuid_t u2( s1 );
			if (u1 != u2) throw std::exception("uuid_t <-> bstr_t doesn't work");
		}

		bstr_t s2(u1, true); // Braces

		{
			uuid_t u2(s2);
			if (u1 != u2) throw std::exception("uuid_t does not support UUID in curly brackets.");
		}
	}
};

template<>
struct comet::test<26>
{
	void run()
	{
		safearray_t<variant_t> a;
		a.push_back( L"Foo" );
		a.push_back( L"Bar" );
		std::sort( a.begin(), a.end() );
	}
};

void expect_equal(const variant_t& lhs, const variant_t& rhs)
{
	if (!(lhs == rhs))
		throw runtime_error("lhs == rhs not true");
	if (lhs != rhs)
		throw runtime_error("lhs != rhs");
	if (lhs < rhs)
		throw runtime_error("lhs < rhs");
	if (lhs > rhs)
		throw runtime_error("lhs > rhs");
}

void expect_less_than(const variant_t& lhs, const variant_t& rhs)
{
	if (!(lhs < rhs))
		throw runtime_error("lhs < rhs not true");
	if (lhs == rhs)
		throw runtime_error("lhs == rhs");
	if (!(lhs != rhs))
		throw runtime_error("lhs != rhs not true");
	if (lhs > rhs)
		throw runtime_error("lhs > rhs");
}

void expect_greater_than(const variant_t& lhs, const variant_t& rhs)
{
	if (!(lhs > rhs))
		throw runtime_error("lhs > rhs not true");
	if (lhs == rhs)
		throw runtime_error("lhs == rhs");
	if (!(lhs != rhs))
		throw runtime_error("lhs != rhs not true");
	if (lhs < rhs)
		throw runtime_error("lhs < rhs");
}

// I1, UI2, VT_UI4, UI8 and UINT

template<>
struct comet::test<27>
{
	void run()
	{
		expect_equal(char(0), char(0));
		expect_less_than(char(0), char(1));
		expect_greater_than(char(1), char(0));
	}
};

template<>
struct comet::test<28>
{
	void run()
	{
		expect_equal(unsigned char(0), unsigned char(0));
		expect_less_than(unsigned char(0), unsigned char(1));
		expect_greater_than(unsigned char(1), unsigned char(0));
	}
};

template<>
struct comet::test<29>
{
	void run()
	{
		expect_equal(short(0), short(0));
		expect_less_than(short(0), short(1));
		expect_greater_than(short(1), short(0));
	}
};

template<>
struct comet::test<30>
{
	void run()
	{
		expect_equal(unsigned short(0), unsigned short(0));
		expect_less_than(unsigned short(0), unsigned short(1));
		expect_greater_than(unsigned short(1), unsigned short(0));
	}
};

template<>
struct comet::test<31>
{
	void run()
	{
		expect_equal(int(0), int(0));
		expect_less_than(int(0), int(1));
		expect_greater_than(int(1), int(0));
	}
};

template<>
struct comet::test<32>
{
	void run()
	{
		expect_equal(unsigned int(0), unsigned int(0));
		expect_less_than(unsigned int(0), unsigned int(1));
		expect_greater_than(unsigned int(1), unsigned int(0));
	}
};

template<>
struct comet::test<33>
{
	void run()
	{
		expect_equal(long(0), long(0));
		expect_less_than(long(0), long(1));
		expect_greater_than(long(1), long(0));
	}
};

template<>
struct comet::test<34>
{
	void run()
	{
		expect_equal(unsigned long(0), unsigned long(0));
		expect_less_than(unsigned long(0), unsigned long(1));
		expect_greater_than(unsigned long(1), unsigned long(0));
	}
};

template<>
struct comet::test<35>
{
	void run()
	{
		expect_equal(LONGLONG(0), LONGLONG(0));
		expect_less_than(LONGLONG(0), LONGLONG(1));
		expect_greater_than(LONGLONG(1), LONGLONG(0));
	}
};

template<>
struct comet::test<36>
{
	void run()
	{
		expect_equal(ULONGLONG(0), ULONGLONG(0));
		expect_less_than(ULONGLONG(0), ULONGLONG(1));
		expect_greater_than(ULONGLONG(1), ULONGLONG(0));
	}
};

COMET_TEST_MAIN
