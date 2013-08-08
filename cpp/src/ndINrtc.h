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

/* starting interface:    INrtcDataCallback */
#define INRTCDATACALLBACK_IID_STR "e891b9db-9d68-422c-b153-e0960cac580f"

#define INRTCDATACALLBACK_IID \
  {0xe891b9db, 0x9d68, 0x422c, \
    { 0xb1, 0x53, 0xe0, 0x96, 0x0c, 0xac, 0x58, 0x0f }}

class NS_NO_VTABLE INrtcDataCallback : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(INRTCDATACALLBACK_IID)

  /* void OnNewData (in long size, in string data); */
  NS_IMETHOD OnNewData(int32_t size, const char * data) = 0;

};

  NS_DEFINE_STATIC_IID_ACCESSOR(INrtcDataCallback, INRTCDATACALLBACK_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_INRTCDATACALLBACK \
  NS_IMETHOD OnNewData(int32_t size, const char * data); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_INRTCDATACALLBACK(_to) \
  NS_IMETHOD OnNewData(int32_t size, const char * data) { return _to OnNewData(size, data); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_INRTCDATACALLBACK(_to) \
  NS_IMETHOD OnNewData(int32_t size, const char * data) { return !_to ? NS_ERROR_NULL_POINTER : _to->OnNewData(size, data); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class _MYCLASS_ : public INrtcDataCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_INRTCDATACALLBACK

  _MYCLASS_();

private:
  ~_MYCLASS_();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(_MYCLASS_, INrtcDataCallback)

_MYCLASS_::_MYCLASS_()
{
  /* member initializers and constructor code */
}

_MYCLASS_::~_MYCLASS_()
{
  /* destructor code */
}

/* void OnNewData (in long size, in string data); */
NS_IMETHODIMP _MYCLASS_::OnNewData(int32_t size, const char * data)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
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

  /* void ExpressInterest (in string interest, in INrtcDataCallback onDataCallback); */
  NS_IMETHOD ExpressInterest(const char * interest, INrtcDataCallback *onDataCallback) = 0;

  /* void PublishData (in string prefix, in string data); */
  NS_IMETHOD PublishData(const char * prefix, const char * data) = 0;

};

  NS_DEFINE_STATIC_IID_ACCESSOR(INrtc, INRTC_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_INRTC \
  NS_IMETHOD Test(int32_t a, int32_t b, int32_t *_retval); \
  NS_IMETHOD ExpressInterest(const char * interest, INrtcDataCallback *onDataCallback); \
  NS_IMETHOD PublishData(const char * prefix, const char * data); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_INRTC(_to) \
  NS_IMETHOD Test(int32_t a, int32_t b, int32_t *_retval) { return _to Test(a, b, _retval); } \
  NS_IMETHOD ExpressInterest(const char * interest, INrtcDataCallback *onDataCallback) { return _to ExpressInterest(interest, onDataCallback); } \
  NS_IMETHOD PublishData(const char * prefix, const char * data) { return _to PublishData(prefix, data); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_INRTC(_to) \
  NS_IMETHOD Test(int32_t a, int32_t b, int32_t *_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->Test(a, b, _retval); } \
  NS_IMETHOD ExpressInterest(const char * interest, INrtcDataCallback *onDataCallback) { return !_to ? NS_ERROR_NULL_POINTER : _to->ExpressInterest(interest, onDataCallback); } \
  NS_IMETHOD PublishData(const char * prefix, const char * data) { return !_to ? NS_ERROR_NULL_POINTER : _to->PublishData(prefix, data); } 

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

/* void ExpressInterest (in string interest, in INrtcDataCallback onDataCallback); */
NS_IMETHODIMP _MYCLASS_::ExpressInterest(const char * interest, INrtcDataCallback *onDataCallback)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void PublishData (in string prefix, in string data); */
NS_IMETHODIMP _MYCLASS_::PublishData(const char * prefix, const char * data)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_ndINrtc_h__ */
