//
//  ndnrtc-object-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

//#define DEBUG
#define NDN_LOGGING
#define NDN_DETAILED

#define NDN_TRACE
#define NDN_INFO
#define NDN_WARN
#define NDN_ERROR
#define NDN_DEBUG

#include "gtest/gtest.h"
#include "simple-log.h"
#include "ndnrtc-object.h"
#include "test-common.h"

using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

//********************************************************************************
/**
 * @name Parameter class tests
 */
TEST(ParametersTest, CreateDeleteBools) {
    {
        bool bvalue = true;
        
        NdnParams::Parameter p(NdnParams::ParameterTypeBool,&bvalue);
        
        EXPECT_TRUE(p.getType()==NdnParams::ParameterTypeBool);
        bool res = *((bool*)p.getValue());
        EXPECT_TRUE(res == bvalue);
    }
    {
        bool bvalue = false;
        
        NdnParams::Parameter p(NdnParams::ParameterTypeBool,&bvalue);
        
        EXPECT_TRUE(p.getType()==NdnParams::ParameterTypeBool);
        bool res = *((bool*)p.getValue());
        EXPECT_TRUE(res == bvalue);
    }
}
TEST(ParametersTest, CreateDeleteInts) {
    {
        int value = 34;
        
        NdnParams::Parameter p(NdnParams::ParameterTypeInt,&value);
        
        EXPECT_TRUE(p.getType()==NdnParams::ParameterTypeInt);
        int res = *((int*)p.getValue());
        EXPECT_TRUE(res == value);
    }
    {
        int value = -100;
        
        NdnParams::Parameter p(NdnParams::ParameterTypeInt,&value);
        
        EXPECT_TRUE(p.getType()==NdnParams::ParameterTypeInt);
        int res = *((int*)p.getValue());
        EXPECT_TRUE(res == value);
    }
}

TEST(ParametersTest, CreateDeleteStrings) {
    {
        char *value = "teststring1";
        
        NdnParams::Parameter p(NdnParams::ParameterTypeString,value);
        
        EXPECT_TRUE(p.getType()==NdnParams::ParameterTypeString);
        char *res = (char*)p.getValue();
        EXPECT_TRUE(strcmp(value,res)==0);
    }
    {
        char *value = "";
        
        NdnParams::Parameter p(NdnParams::ParameterTypeString,value);
        
        EXPECT_TRUE(p.getType()==NdnParams::ParameterTypeString);
        char* res = (char*)p.getValue();
        EXPECT_TRUE(strcmp(value,res)==0);
    }
    {
        char *value = "superlongsuperlongsuperlongsuperlongsuperlongsuperlongsuperlong\
        superlongsuperlongsuperlongsuperlongsuperlongsuperlongsuperlong\
        superlongsuperlongsuperlongsuperlongsuperlongsuperlongsuperlong\
        superlongsuperlongsuperlongsuperlongsuperlongsuperlongsuperlong\
        superlongsuperlongsuperlongsuperlongsuperlongsuperlongsuperlong\
        superlongsuperlongsuperlongsuperlongsuperlongsuperlongsuperlong";
        
        NdnParams::Parameter p(NdnParams::ParameterTypeString,value);
        
        EXPECT_TRUE(p.getType()==NdnParams::ParameterTypeString);
        char* res = (char*)p.getValue();
        EXPECT_TRUE(strcmp(value,res)==0);
    }
}
TEST(ParametersTest, DefaultCtr) {
    {
        char *value = "teststring1";
        
        NdnParams::Parameter p;
        
        p.setTypeAndValue(NdnParams::ParameterTypeString, value);
        
        EXPECT_TRUE(p.getType()==NdnParams::ParameterTypeString);
        
        char *res = (char*)p.getValue();
        EXPECT_TRUE(strcmp(value,res)==0);
    }
}

TEST(ParametersTest, PtrParameters) {
    {
        char *value = "teststring1";
        
        NdnParams::Parameter *p = new NdnParams::Parameter(NdnParams::ParameterTypeString,value);
        
        char *res = (char*)p->getValue();
        EXPECT_TRUE(strcmp(value,res)==0);
        
        delete p;
    }
}

//********************************************************************************
/**
 * @name NdnParams class tests
 */
#if 0
class NdnParamsTester : public NdnParams
{
public:
    int getBoolParam(const std::string &name, bool *p) { return getParamAsBool(name, p); };
    int getIntParam(const std::string &name, int *p) { return getParamAsInt(name, p); };
    int getStringParam(const std::string &name, char **p) { return getParamAsString(name, p); };
    
    bool getBoolParam(const std::string &name) { return getParamAsBool(name); };
    std::string getStringParam(const std::string &name) { return getParamAsString(name); };
    int getIntParam(const std::string &name) { return getParamAsInt(name); };
};


TEST(ParamsTest, DefaultParams) {
    shared_ptr<NdnParams> p = NdnParams::defaultParams();
}
TEST(ParamsTest, SetParams) {
    shared_ptr<NdnParamsTester> p(new NdnParamsTester(NdnParams::defaultParams().get));
    
    {
        p->setBoolParam("p1", true);
        bool res;
        
        ASSERT_EQ(p->getBoolParam("p1",&res), 0);
        EXPECT_TRUE(res);
        EXPECT_LT(p->getBoolParam("non-existent param",&res), 0);
        
        p->setBoolParam("p2", false);
        EXPECT_EQ(p->getBoolParam("p2",&res), 0);
        EXPECT_FALSE(res);
    }
    {
        int res;
        p->setIntParam("p3", 123);
        ASSERT_EQ(p->getIntParam("p3",&res), 0);
        EXPECT_EQ(res, 123);
        
        p->setIntParam("p4", -123);
        ASSERT_EQ(p->getIntParam("p4",&res), 0);
        EXPECT_EQ(res, -123);
    }
    {
        char *res = NULL;
        
        p->setStringParam("p5", "value5");
        ASSERT_EQ(p->getStringParam("p5",&res), 0);
        EXPECT_STREQ("value5",res);
        
        p->setStringParam("p6", "");
        ASSERT_EQ(p->getStringParam("p6",&res), 0);
        EXPECT_STREQ("",res);
    }
    
    delete p;
}
TEST(ParamsTest, ParamsObjectMethods) {
    shared_ptr<NdnParamsTester> p  = NdnParams::defaultParams();
    
    {
        p->setBoolParam("p1", true);
        bool res;
        
        EXPECT_TRUE(p->getBoolParam("p1"));
        
        p->setBoolParam("p2", false);
        EXPECT_FALSE(p->getBoolParam("p2"));
        
        EXPECT_FALSE(p->getBoolParam("p2 "));
    }
    {
        int res;
        p->setIntParam("p3", 123);
        EXPECT_EQ(123, p->getIntParam("p3"));
        
        p->setIntParam("p4", -123);
        EXPECT_EQ(-123, p->getIntParam("p4"));
        
        EXPECT_EQ(-INT_MAX, p->getIntParam("p4 "));
    }
    {
        p->setStringParam("p5", "value5");
        EXPECT_STREQ("value5", p->getStringParam("p5").c_str());
        
        p->setStringParam("p6", "");
        EXPECT_STREQ("", p->getStringParam("p6").c_str());
        
        EXPECT_STREQ("", p->getStringParam("p6 ").c_str());
    }
    
    delete p;
}

TEST(ParamsTest, NonExistentParams)
{
    NdnParamsTester *p = (NdnParamsTester*) NdnParams::defaultParams();
    
    void *res = NULL;
    
    EXPECT_EQ(p->size(), 0);
    EXPECT_LT(p->getBoolParam("non-existent param",(bool*)&res), 0);
    EXPECT_LT(p->getIntParam("non-existent param",(int*)&res), 0);
    EXPECT_LT(p->getStringParam("non-existent param",(char**)&res), 0);
    EXPECT_EQ(p->size(), 0);
    
    delete p;
}
TEST(ParamsTest, OverwriteParams)
{
    NdnParamsTester *p = (NdnParamsTester*) NdnParams::defaultParams();
    
    {
        p->setBoolParam("p1", true);
        
        bool res;
        int oldSize = p->size();
        
        ASSERT_EQ(p->getBoolParam("p1",&res), 0);
        EXPECT_TRUE(res);
        
        p->setBoolParam("p1", false);
        EXPECT_EQ(p->getBoolParam("p1",&res), 0);
        EXPECT_FALSE(res);
        
        EXPECT_EQ(p->size(), oldSize);
    }
    {
        int res;
        p->setIntParam("p2", 123);
        ASSERT_EQ(p->getIntParam("p2",&res), 0);
        EXPECT_EQ(res, 123);
        
        int oldSize = p->size();
        
        p->setIntParam("p2", -123);
        ASSERT_EQ(p->getIntParam("p2",&res), 0);
        EXPECT_EQ(res, -123);
        
        EXPECT_EQ(p->size(),oldSize);
    }
    { // string
        char *res = NULL;
        
        p->setStringParam("p5", "value5");
        ASSERT_EQ(p->getStringParam("p5",&res), 0);
        EXPECT_STREQ("value5",res);
        
        int oldSize = p->size();
        
        p->setStringParam("p5", "");
        ASSERT_EQ(p->getStringParam("p5",&res), 0);
        EXPECT_STREQ("",res);
        
        p->setStringParam("p5", "new value5");
        ASSERT_EQ(p->getStringParam("p5",&res), 0);
        EXPECT_STREQ("new value5", res);
        
        EXPECT_EQ(p->size(),oldSize);
    }
    
    delete p;
}
TEST(ParamsTest, AddParams)
{
    NdnParamsTester *p1 = (NdnParamsTester*) NdnParams::defaultParams();
    NdnParamsTester *p2 = (NdnParamsTester*) NdnParams::defaultParams();
    
    p1->setBoolParam("p1", true);
    p1->setStringParam("p2", "hello");
    
    p2->setIntParam("p3", 123);
    p2->setStringParam("p4", "world");
    
    int totalSize = p1->size()+p2->size();
    
    p1->addParams(*p2);
    
    EXPECT_EQ(p1->size(),totalSize);
    
    bool bres;
    int ires;
    char *cres = NULL;
    
    EXPECT_EQ(p1->getBoolParam("p1",&bres), 0);
    EXPECT_TRUE(bres);
    
    EXPECT_EQ(p1->getStringParam("p2",&cres), 0);
    EXPECT_STREQ(cres,"hello");
    
    EXPECT_EQ(p1->getIntParam("p3",&ires), 0);
    EXPECT_EQ(123,ires);
    
    free(cres);
    EXPECT_EQ(p1->getStringParam("p4",&cres), 0);
    EXPECT_STREQ(cres,"world");
    
    
    delete p1;
    delete p2;
}
TEST(ParamsTest, ResetParams)
{
    NdnParamsTester *p1 = (NdnParamsTester*) NdnParams::defaultParams();
    NdnParamsTester *p2 = (NdnParamsTester*) NdnParams::defaultParams();
    
    p1->setBoolParam("p1", true);
    p1->setStringParam("p2", "hello");
    
    p2->setIntParam("p3", 123);
    p2->setStringParam("p4", "world");
    
    p1->resetParams(*p2);
    
    EXPECT_EQ(p1->size(),p2->size());
    
    int ires;
    char *cres = NULL;
    
    EXPECT_EQ(p1->getIntParam("p3",&ires), 0);
    EXPECT_EQ(123,ires);
    
    free(cres);
    EXPECT_EQ(p1->getStringParam("p4",&cres), 0);
    EXPECT_STREQ(cres,"world");
    free(cres);
}
#endif
//********************************************************************************
/**
 * @name NdnrtcObject class tests
 */

class NdnRtcObjectTester : public NdnRtcObject
{
public:
    int postError(int code, char *msg) {
        return notifyError(code, msg);
    };
    int postErrorNoParams(){
        return notifyErrorNoParams();
    };
    int postErrorBadArg(const char *pname){
        return notifyErrorBadArg(pname);
    };
    bool isObserved() { return hasObserver(); };
};

class NdnRtcObjectTest : public ::testing::Test, public INdnRtcObjectObserver
{
public:
    void onErrorOccurred(const char *errorMessage){
        errorOccurred_ = true;
        errorMessage_ = (char*)errorMessage;
    };
    
protected:
    bool errorOccurred_ = false;
    char *errorMessage_ = NULL;
};

TEST_F(NdnRtcObjectTest, CreateDelete)
{
    NdnRtcObject *obj = new NdnRtcObject();
    delete obj;
}
TEST_F(NdnRtcObjectTest, CreateDeleteNoParams)
{
    NdnRtcObjectTester *obj = ( NdnRtcObjectTester *) new NdnRtcObject();
    
    delete obj;
}
TEST_F(NdnRtcObjectTest, CreateDeleteWithParams)
{
    shared_ptr<NdnParams> paramsSPtr(new NdnParams());
    NdnRtcObjectTester *obj = ( NdnRtcObjectTester *) new NdnRtcObject(DefaultParams);
    
    delete obj;
}
TEST_F(NdnRtcObjectTest, CreateDeleteWithObserver)
{
    shared_ptr<NdnParams> paramsSPtr(new NdnParams());
    NdnRtcObjectTester *obj = ( NdnRtcObjectTester *) new NdnRtcObject(DefaultParams, this);
    
    EXPECT_TRUE(obj->isObserved());
    
    delete obj;
}
TEST_F(NdnRtcObjectTest, SetObserver)
{
    NdnRtcObjectTester *obj = ( NdnRtcObjectTester *) new NdnRtcObject();
    
    EXPECT_FALSE(obj->isObserved());
    obj->setObserver(this);
    EXPECT_TRUE(obj->isObserved());
    
    delete obj;
}
TEST_F(NdnRtcObjectTest, ErrorNotifies)
{
    shared_ptr<NdnParams> paramsSPtr(new NdnParams());    
    NdnRtcObjectTester *obj = ( NdnRtcObjectTester *) new NdnRtcObject(DefaultParams, this);
    
    EXPECT_FALSE(errorOccurred_);
    obj->postError(-1, "error msg");
    EXPECT_TRUE(errorOccurred_);
    EXPECT_STREQ("error msg", errorMessage_);

    errorOccurred_ = false;
    obj->postError(-1, "");
    EXPECT_TRUE(errorOccurred_);
    EXPECT_STREQ("", errorMessage_);
    
    errorOccurred_ = false;
    obj->postErrorNoParams();
    EXPECT_TRUE(errorOccurred_);

    errorOccurred_ = false;
    obj->postErrorBadArg("value1");
    EXPECT_TRUE(errorOccurred_);
    EXPECT_NE(nullptr, strstr(errorMessage_, "value1"));    
    
    delete obj;
}