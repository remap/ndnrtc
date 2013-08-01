/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM /Users/gpeetonn/Documents/code/CCN/ndnrtc/cpp/idl/ndINrtc.idl
 */

#ifndef __gen_ndINrtc_h__
#define __gen_ndINrtc_h__


#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif

/* starting interface:    INrtc */
#define INRTC_IID_STR "86ad66e9-b3f7-4e6a-beee-802b1d9ff442"

#define INRTC_IID \
  {0x86ad66e9, 0xb3f7, 0x4e6a, \
    { 0xbe, 0xee, 0x80, 0x2b, 0x1d, 0x9f, 0xf4, 0x42 }}

class NS_NO_VTABLE INrtc : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(INRTC_IID)

  /* long Test (in long a, in long b); */
  NS_IMETHOD Test(int32_t a, int32_t b, int32_t *_retval) = 0;

};

  NS_DEFINE_STATIC_IID_ACCESSOR(INrtc, INRTC_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_INRTC \
  NS_IMETHOD Test(int32_t a, int32_t b, int32_t *_retval); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_INRTC(_to) \
  NS_IMETHOD Test(int32_t a, int32_t b, int32_t *_retval) { return _to Test(a, b, _retval); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_INRTC(_to) \
  NS_IMETHOD Test(int32_t a, int32_t b, int32_t *_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->Test(a, b, _retval); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class _MYCLASS_ : public INrtc
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_INRTC

  _MYCLASS_();

private:
  ~_MYCLASS_();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(_MYCLASS_, INrtc)

_MYCLASS_::_MYCLASS_()
{
  /* member initializers and constructor code */
}

_MYCLASS_::~_MYCLASS_()
{
  /* destructor code */
}

/* long Test (in long a, in long b); */
NS_IMETHODIMP _MYCLASS_::Test(int32_t a, int32_t b, int32_t *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_ndINrtc_h__ */
