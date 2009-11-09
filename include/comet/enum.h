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

#ifndef COMET_ENUM_H
#define COMET_ENUM_H

#include <comet/config.h>

#include <comet/variant.h>
#include <comet/server.h>
#include <comet/util.h>

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
			static void init(CONNECTDATA& t, const S& s) {
				t.dwCookie = s.first;
				t.pUnk = com_ptr<IUnknown>::detach( com_ptr<IUnknown>(s.second) );
			}

			static void clear(CONNECTDATA& t) { t.pUnk->Release(); }
		};
	}

	/** \class stl_enumeration_t  enum.h comet/enum.h
	  * Implements _NewEnum style COM object.
	  * \param Itf Enumeration Interface.
	  * \param C STL Style container. 
	  * \param T Iteration Element type (VARIANT)
	  * \param CONVERTER Converts container element to \p T type. (std::identity<C::value_type>)
	  * \sa stl_enumeration create_enum
	  */
    template<typename Itf, typename C, typename T = VARIANT, typename CONVERTER = std::identity<COMET_STRICT_TYPENAME C::value_type>, typename TH = ::IUnknown> class stl_enumeration_t : public simple_object< Itf > {
		typedef impl::type_policy<T> policy;
	public:
		/// \name Interface \p Itf
		//@{
		STDMETHOD(Next)(ULONG celt, T *rgelt, ULONG* pceltFetched)
		{
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
				stl_enumeration_t* new_enum = new stl_enumeration_t(container_, pOuter_.in(), converter_);
				new_enum->AddRef();
				*ppenum = new_enum;
			} catch (...) {
				return E_FAIL;
			}
			return S_OK;
		}
		//@}
		
		stl_enumeration_t(const C& c, TH* pOuter = 0, const CONVERTER &converter = CONVERTER() )
			: container_(c), pOuter_(pOuter) , converter_(converter)
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
	
	/** \struct enumerated_type_of enum.h comet/enum.h
	  * Traits wrapper mapping COM Enumeration interface to element.
	  */
	template<typename EnumItf> struct enumerated_type_of;
	template<> struct enumerated_type_of<IEnumVARIANT> { typedef VARIANT is; };
	template<> struct enumerated_type_of<IEnumConnectionPoints> { typedef IConnectionPoint* is; };
	template<> struct enumerated_type_of<IEnumConnections> { typedef CONNECTDATA is; };
	
	/** \struct stl_enumeration  enum.h comet/enum.h
	  * STL Enumeration creation helper.
	  * \param ET Enumeration Type (COM Interface).
	  */
	template<typename ET> struct stl_enumeration {

		/** Auto-Create a _NewEnum enumerator from an STL container.
		  * No contained object.
		  * \param container STL Container.
		  */
		template<typename C>
		static com_ptr<ET> create(const C& container)
		{
			typedef typename enumerated_type_of<ET>::is T;
			typedef std::identity<COMET_STRICT_TYPENAME C::value_type> CONVERTER;
			return new stl_enumeration_t<ET, C, T, CONVERTER,::IUnknown>(container, 0);
		}

		/** Auto-Create a _NewEnum enumerator from an STL container.
		  * \param container STL Container.
		  * \param th Outer or \e this pointer.
		  */
		template<typename C, typename TH>
		static com_ptr<ET> create(const C& container, TH* th )
		{
			typedef typename enumerated_type_of<ET>::is T;
			typedef std::identity<COMET_STRICT_TYPENAME C::value_type> CONVERTER;
			return new stl_enumeration_t<ET, C, T, CONVERTER,TH>(container, th);
		}

		/** Auto-Create a _NewEnum enumerator from an STL container, specifying
		  * a converter.
		  * \param container STL Container.
		  * \param th Outer or \e this pointer.
		  * \param converter Converter type (convert Container element to
		  * iterator interface element types).
		  */
		template<typename C, typename TH, typename CONVERTER>
		static com_ptr<ET> create(const C& container, TH* th, const CONVERTER &converter)
		{
			typedef typename enumerated_type_of<ET>::is T;
			return new stl_enumeration_t<ET, C, T, CONVERTER, TH>(container, th, converter);
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
	template<typename C, typename TH> com_ptr<IEnumVARIANT> create_enum(const C& container, TH* th = 0)
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
	template<typename C, typename TH, typename CONVERTER> com_ptr<IEnumVARIANT> create_enum(const C& container, TH* th, CONVERTER converter )
	{
		return stl_enumeration<IEnumVARIANT>::create(container, th, converter); 
	}
	
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
	
	/** \class variant_iterator enum.h comet/enum.h
	  * STL style iterator for IEnumVariant interface.
	  */
	class variant_iterator
	{
		com_ptr<IEnumVARIANT> enum_;
		variant_t ce_;
	public:
		/** Constructor.
		  */
		variant_iterator( const com_ptr<IEnumVARIANT>& e ) : enum_(e) {
			next();
		}
		
		variant_iterator() {}
		
		/** Move to next element.
		  */
		variant_iterator& operator++() {
			next();
			return *this;
		}
		
		bool operator!=(const variant_iterator& v) {
			if (!v.enum_.is_null()) throw std::logic_error("variant_iterator comparison does not work");
			
			return !enum_.is_null();
		}
		
		/** Move to next element (post increment).
		  */
		variant_iterator operator++(int) {
			variant_iterator t(*this);
			operator++();
			return t;
		}
		
		/** Current element.
		  */
		variant_t& operator*() {
			return ce_;
		}
	private:
		void next() {
			if (enum_) {
				unsigned long x = 0;
				enum_->Next(1, ce_.out(), &x) | raise_exception;
				if (x == 0) enum_ = 0;
			}
		}
	};
	
	/** \class itf_iterator enum.h comet/enum.h
	  * STL style Iterator for IEnumVARIANT interface returning a contained
	  * interface pointer.
	  */
	template<typename Itf> class itf_iterator
	{
		com_ptr<IEnumVARIANT> enum_;
		com_ptr<Itf> p_;
	public:
		/** Constructor.
		  */
		itf_iterator( const com_ptr<IEnumVARIANT>& e ) : enum_(e) {
			next();
		}
		
		itf_iterator() {}
		
		/** Move to next element.
		  */
		itf_iterator& operator++() {
			next();
			return *this;
		}
		
		bool operator!=(const itf_iterator& v) {
			if (v.enum_) throw std::logic_error("itf_iterator comparison does not work");
			
			return enum_ != 0;
		}
		
		/** Move to next element.
		  */
		itf_iterator operator++(int) {
			itf_iterator t(*this);
			operator++();
			return t;
		}
		
		/** Acess element.
		  */
		com_ptr<Itf>& operator*() {
			return p_;
		}
	private:
		void next() {
			if (enum_) {
				unsigned long x = 0;
				variant_t v;
				enum_->Next(1, v.out(), &x) | raise_exception;
				if (x == 0) {
					enum_ = 0;
					p_ = 0;
				}
				else {
					p_ = try_cast(v);
				}
			}
		}
	};
	
}	
	
#endif
