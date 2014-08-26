//
//  CameraCapturer.h
//  ndnrtc-demo
//
//  Created by Peter Gusev on 8/25/14.
//  Copyright (c) 2014 REMAP. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

@protocol CameraCapturerDelegate;

@interface CameraCapturer : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>

@property (nonatomic, weak) id<CameraCapturerDelegate> delegate;

-(NSArray*)getDeviceList;
-(void)selectDeviceWithId:(NSUInteger)deviceIdx;

-(NSArray*)getDeviceConfigurationsListLocalized;
-(void)selectDeviceConfigurationWithIdx:(NSUInteger)configurationIdx;

-(void)startCapturing;
-(void)stopCapturing;

@end

@protocol CameraCapturerDelegate <NSObject>

@required
-(void)cameraCapturer:(CameraCapturer*)capturer didDeliveredBGRAFrameData:(NSData*)frameData;
-(void)cameraCapturer:(CameraCapturer*)capturer didObtainedError:(NSError*)error;

@end