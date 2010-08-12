/** \file 
  * Implement _NewEnum style classes and iterators.
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

#ifndef COMET_STL_ENUM_H
#define COMET_STL_ENUM_H

#include <comet/config.h>

#include <comet/enum_common.h>
#include <comet/server.h>
#include <comet/stl.h>
#include <comet/variant.h>

namespace comet {

	/** \class stl_enumeration_t  enum.h comet/enum.h
	  * Implements _NewEnum style COM object.
	  * \param Itf Enumeration Interface.
	  * \param C STL Style container. 
	  * \param T Iteration Element type (VARIANT)
	  * \param CONVERTER Converts container element to \p T type. (std::identity<C::value_type>)
	  * \sa stl_enumeration create_enum
	  */
    template<
		typename Itf, typename C, typename T=VARIANT,
		typename CONVERTER=std::identity<COMET_STRICT_TYPENAME C::value_type>,
		typename TH = ::IUnknown>
	class stl_enumeration_t : public simple_object<Itf>
	{
		typedef impl::type_policy<T> policy;
	public:
		/// \name Interface \p Itf
		//@{
		STDMETHOD(Next)(ULONG celt, T *rgelt, ULONG* pceltFetched)
		{
			if (pceltFetched)
				*pceltFetched = 0;
			if (!rgelt)
				return E_POINTER;

			UINT i = 0;
			typename C::const_iterator backup_it_ = it_;
			try {
				for (;i<celt && it_ != container_.end(); ++i, ++it_) {
					policy::init(rgelt[i], converter_(*it_));
				}
				if (pceltFetched) *pceltFetched = i;
			}
			catch (...) {
				it_ = backup_it_;
				for (size_t j=0; j<=i; ++j) policy::clear(rgelt[j]);
				return E_FAIL;
			}
			return i == celt ? S_OK : S_FALSE;
		}
		
		STDMETHOD(Reset)()
		{
			try {
				it_ = container_.begin();
			}
			catch (...) {
				return E_FAIL;
			}
			return S_OK;
		}
		
		STDMETHOD(Skip)(ULONG celt)
		{
			try {
				while (celt--) it_++;
			} catch (...) {
				return E_FAIL;
			}
			return S_OK;
		}
		
		STDMETHOD(Clone)(Itf** ppenum)
		{
			try {
				stl_enumeration_t* new_enum =
					new stl_enumeration_t(container_, pOuter_.in(), converter_);
				new_enum->AddRef();
				*ppenum = new_enum;
			} catch (...) {
				return E_FAIL;
			}
			return S_OK;
		}
		//@}
		
		stl_enumeration_t(
			const C& c, TH* pOuter = 0, const CONVERTER& converter=CONVERTER())
			: container_(c), pOuter_(pOuter), converter_(converter)
		{
			it_ = container_.begin();
		}
		
		~stl_enumeration_t() 
		{
		}
		
		const C& container_;
		typename C::const_iterator it_;
		com_ptr<TH> pOuter_;
		CONVERTER converter_;

	private:
		stl_enumeration_t(const stl_enumeration_t&);
		stl_enumeration_t& operator=(const stl_enumeration_t&);
	};
	
	/**
	 * STL Enumeration creation helper.
	 *
	 * Creates the enumeration with the element type specified by the
	 * enumerated_type_of policy.  To specify the element type directly, use
	 * stl_enumeration_t.
	 *
	 * \param ET Enumeration Type e.g. IEnumUnknown.
	 */
	template<typename ET>
	struct stl_enumeration
	{

		/** Auto-Create a _NewEnum enumerator from an STL container.
		  * No contained object.
		  * \param container STL Container.
		  */
		template<typename C>
		static com_ptr<ET> create(const C& container)
		{
			typedef typename enumerated_type_of<ET>::is T;
			typedef std::identity<COMET_STRICT_TYPENAME C::value_type>
				CONVERTER;
			return new stl_enumeration_t<ET, C, T, CONVERTER, IUnknown>(
				container, 0);
		}

		/** Auto-Create a _NewEnum enumerator from an STL container.
		  * \param container STL Container.
		  * \param th Outer or \e this pointer.
		  */
		template<typename C, typename TH>
		static com_ptr<ET> create(const C& container, TH* th)
		{
			typedef typename enumerated_type_of<ET>::is T;
			typedef std::identity<COMET_STRICT_TYPENAME C::value_type>
				CONVERTER;
			return new stl_enumeration_t<ET, C, T, CONVERTER, TH>(
				container, th);
		}

		/** Auto-Create a _NewEnum enumerator from an STL container, specifying
		  * a converter.
		  * \param container STL Container.
		  * \param th Outer or \e this pointer.
		  * \param converter Converter type (convert Container element to
		  * iterator interface element types).
		  */
		template<typename C, typename TH, typename CONVERTER>
		static com_ptr<ET> create(
			const C& container, TH* th, const CONVERTER& converter)
		{
			typedef typename enumerated_type_of<ET>::is T;
			return new stl_enumeration_t<ET, C, T, CONVERTER, TH>(
				container, th, converter);
		}
	};
	
	/*! Creates IEnumVARIANT enumeration of a STL container.
	 * \param container STL Container.
	 * \param th Outer or \e this pointer.
		\code
			com_ptr<IEnumVARIANT> get__NewEnum() {
				return create_enum( collection_, this );
			}
		\endcode
	  * \relates stl_enumeration
	*/
	template<typename C, typename TH>
	com_ptr<IEnumVARIANT> create_enum(const C& container, TH* th = 0)
	{
		return stl_enumeration<IEnumVARIANT>::create(container, th); 
	}
	
	//! Creates IEnumVARIANT enumeration of a STL container with a converter.
	/*! \param container STL Container.
	  * \param th Outer or \e this pointer.
	  * \param converter Converter type (convert Container element to VARIANT)
	  * \sa ptr_converter ptr_converter_select1st ptr_converter_select2nd
	  * \relates stl_enumeration
	  */
	template<typename C, typename TH, typename CONVERTER>
	com_ptr<IEnumVARIANT> create_enum(const C& container, TH* th, CONVERTER converter)
	{
		return stl_enumeration<IEnumVARIANT>::create(container, th, converter); 
	}

}	
	
#endif
