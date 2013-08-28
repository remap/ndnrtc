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

using namespace ndnrtc;

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
class NdnParamsTester : public NdnParams
{
public:
    int getBoolParam(const std::string &name, bool *p) { return getParamAsBool(name, p); };
    int getIntParam(const std::string &name, int *p) { return getParamAsInt(name, p); };
    int getStringParam(const std::string &name, char **p) { return getParamAsString(name, p); };
};


TEST(ParamsTest, DefaultParams) {
    NdnParams *p = NdnParams::defaultParams();
    delete p;
}
TEST(ParamsTest, SetParams) {
    NdnParamsTester *p = (NdnParamsTester*) NdnParams::defaultParams();
    
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
        EXPECT_EQ(strcmp(res,"value5"), 0);
        
        p->setStringParam("p6", "");
        ASSERT_EQ(p->getStringParam("p6",&res), 0);
        EXPECT_EQ(strcmp(res,""), 0);
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
        EXPECT_EQ(strcmp(res,"value5"), 0);
        
        int oldSize = p->size();
        
        p->setStringParam("p5", "");
        ASSERT_EQ(p->getStringParam("p5",&res), 0);
        EXPECT_EQ(strcmp(res,""), 0);
        
        p->setStringParam("p5", "new value5");
        ASSERT_EQ(p->getStringParam("p5",&res), 0);
        EXPECT_EQ(strcmp(res,"new value5"), 0);
        
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
    EXPECT_EQ(strcmp("hello",cres),0);
    
    EXPECT_EQ(p1->getIntParam("p3",&ires), 0);
    EXPECT_EQ(123,ires);
    
    free(cres);
    EXPECT_EQ(p1->getStringParam("p4",&cres), 0);
    EXPECT_EQ(strcmp("world",cres),0);
    
    
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
    EXPECT_EQ(strcmp("world",cres),0);
    free(cres);
}
