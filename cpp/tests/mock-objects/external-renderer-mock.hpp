// 
// external-renderer-mock.hpp
//
//  Created by Peter Gusev on 09 August 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __external_renderer_mock_h__
#define __external_renderer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <include/interfaces.hpp>

class MockExternalRenderer : public ndnrtc::IExternalRenderer
{
public:
	MOCK_METHOD2(getFrameBuffer, uint8_t*(int,int));
	MOCK_METHOD4(renderBGRAFrame, void(const ndnrtc::FrameInfo& frameInfo,int,int,const uint8_t*));
};

#endif
