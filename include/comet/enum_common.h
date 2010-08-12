/** \file 
  * Code common to enumerator implementations.
  */
/*
 * Copyright © 2000 Sofus Mortensen
 *
 * This material is provided "as is", with absolutely no warranty 
 * expressed or implied. Any use is at your own risk. Permission to 
 * use or copy this software for any purpose is hereby granted without 
 * fee, provided the above notices are retained on all copies. 
 * Permission to modify the code and to distribute modified code is 
 * granted, provided the above notices are retained, and a notice that 
 * the code was modified is included with the above copyright notice. 
 *
 * This header is part of comet.
 * http://www.lambdasoft.dk/comet
 */

#ifndef COMET_ENUM_IMPL_H
#define COMET_ENUM_IMPL_H

#include <comet/config.h>

#include <comet/variant.h>
#include <comet/stl.h>

namespace comet {

	namespace impl {

		template<typename T> struct type_policy;

		template<> struct type_policy<VARIANT>
		{
			template<typename S>
			static void init(VARIANT& t, const S& s) 
			{ ::VariantInit(&t); t = variant_t::detach( variant_t(s) ); }

			static void clear(VARIANT& t) { ::VariantClear(&t); }	
		};

		template<> struct type_policy<CONNECTDATA>
		{
			template<typename S>
			static void init(CONNECTDATA& t, const S& s)
			{
				t.dwCookie = s.first;
				t.pUnk =
					com_ptr<IUnknown>::detach(com_ptr<IUnknown>(s.second));
			}

			static void clear(CONNECTDATA& t) { t.pUnk->Release(); }
		};
	}
	
	/** \struct enumerated_type_of enum.h comet/enum.h
	  * Traits wrapper mapping COM Enumeration interface to element.
	  */
	template<typename EnumItf> struct enumerated_type_of;

	template<> struct enumerated_type_of<IEnumVARIANT>
	{ typedef VARIANT is; };

	template<> struct enumerated_type_of<IEnumConnectionPoints>
	{ typedef IConnectionPoint* is; };

	template<> struct enumerated_type_of<IEnumConnections>
	{ typedef CONNECTDATA is; };
	
	/** \struct ptr_converter  enum.h comet/enum.h
	  * IUnknown Converter for containers containing Comet objects.
	  * \relates stl_enumeration
	  */
	template<typename T> struct ptr_converter : std::unary_function< com_ptr<IUnknown>, T>
	{
		com_ptr<IUnknown> operator()(const T& x) { return impl::cast_to_unknown(x); }
	};
	
	/** \struct ptr_converter_select1st  enum.h comet/enum.h
	  * IUnknown Converter for containers containing Comet objects as the first
	  * elment of a pair.
	  * \relates stl_enumeration
	  */
	template<typename T> struct ptr_converter_select1st : std::unary_function< com_ptr<IUnknown>, T>
	{
		com_ptr<IUnknown> operator()(const T& x) { return impl::cast_to_unknown(x.first); }
	};
	
	/** \struct ptr_converter_select2nd  enum.h comet/enum.h
	  * IUnknown Converter for containers containing Comet objects as the second
	  * elment of a pair.
	  * \relates stl_enumeration
	  */
	template<typename T> struct ptr_converter_select2nd : std::unary_function< com_ptr<IUnknown>, T>
	{
		com_ptr<IUnknown> operator()(const T& x) { return impl::cast_to_unknown(x.second); }
	};
}	
	
#endif
