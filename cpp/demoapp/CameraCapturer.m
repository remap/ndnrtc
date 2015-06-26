//
//  CameraCapturer.m
//  ndnrtc-demo
//
//  Created by Peter Gusev on 8/25/14.
//  Copyright (c) 2014 REMAP. All rights reserved.
//

#import "CameraCapturer.h"

@interface CameraCapturer ()

@property (nonatomic) NSArray	*videoDevices;
@property (nonatomic) NSArray	*observers;
@property (nonatomic) AVCaptureSession *session;
@property (nonatomic) AVCaptureDeviceInput *videoDeviceInput;
@property (nonatomic) AVCaptureVideoDataOutput *videoOutput;
@property (assign) AVCaptureDeviceFormat *videoDeviceFormat;
@property (nonatomic) NSLock *lock;

+(NSArray*)getDevices;

@end

@implementation CameraCapturer

+(NSArray*)getDeviceList
{
    NSArray *devices = [self getDevices];
    
    return [devices valueForKeyPath: @"localizedName"];
}

-(void)selectDeviceWithId:(NSUInteger)deviceIdx
{
    if (deviceIdx < [self videoDevices].count)
        [self setSelectedVideoDevice: [[self videoDevices] objectAtIndex:deviceIdx]];
}

+(NSArray*)getDeviceConfigurationsList:(NSUInteger)deviceIdx
{
    NSArray *deviceList = [self getDevices];
    
    if (deviceIdx < deviceList.count)
        return [[[deviceList objectAtIndex:deviceIdx] formats] valueForKeyPath: @"localizedName"];
    
    return nil;
}

-(void)selectDeviceConfigurationWithIdx:(NSUInteger)configurationIdx
{
    if (configurationIdx < [self selectedVideoDevice].formats.count)
        [self setVideoDeviceFormat: [[self selectedVideoDevice].formats objectAtIndex: configurationIdx]];
}

-(void)startCapturing
{
    [[self session] startRunning];
}

-(void)stopCapturing
{
    [[self session] stopRunning];
}

+(CGSize)frameSizeForConfiguration:(NSUInteger)configurationIdx
                         forDevice:(NSUInteger)deviceIdx
{
    CGSize frameSize = CGSizeZero;
    NSArray *devices = [self getDevices];

    if (deviceIdx < devices.count)
    {
        AVCaptureDevice *device = [devices objectAtIndex: deviceIdx];
        AVCaptureDeviceFormat *format = [device.formats objectAtIndex:configurationIdx];
        CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions((CMVideoFormatDescriptionRef)[format formatDescription]);
        
        frameSize.width = dimensions.width;
        frameSize.height = dimensions.height;
    }
    
    return frameSize;
}

//******************************************************************************
- (id)init
{
    self = [super init];
    
    if (self) {
        _lock = [[NSLock alloc] init];
        
        // Create a capture session
        _session = [[AVCaptureSession alloc] init];
        
        _session.sessionPreset = AVCaptureSessionPresetHigh;
        
        // Capture Notification Observers
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        id runtimeErrorObserver = [notificationCenter addObserverForName:AVCaptureSessionRuntimeErrorNotification
                                                                  object:_session
                                                                   queue:[NSOperationQueue mainQueue]
                                                              usingBlock:^(NSNotification *note) {
                                                                  dispatch_async(dispatch_get_main_queue(), ^(void) {
                                                                      [self presentError: [[note userInfo] objectForKey:AVCaptureSessionErrorKey]];
                                                                  });
                                                              }];
        id didStartRunningObserver = [notificationCenter addObserverForName:AVCaptureSessionDidStartRunningNotification
                                                                     object:_session
                                                                      queue:[NSOperationQueue mainQueue]
                                                                 usingBlock:^(NSNotification *note) {
                                                                 }];
        id didStopRunningObserver = [notificationCenter addObserverForName:AVCaptureSessionDidStopRunningNotification
                                                                    object:_session
                                                                     queue:[NSOperationQueue mainQueue]
                                                                usingBlock:^(NSNotification *note) {
                                                                }];
        id deviceWasConnectedObserver = [notificationCenter addObserverForName:AVCaptureDeviceWasConnectedNotification
                                                                        object:nil
                                                                         queue:[NSOperationQueue mainQueue]
                                                                    usingBlock:^(NSNotification *note) {
                                                                        [self refreshDevices];
                                                                    }];
        id deviceWasDisconnectedObserver = [notificationCenter addObserverForName:AVCaptureDeviceWasDisconnectedNotification
                                                                           object:nil
                                                                            queue:[NSOperationQueue mainQueue]
                                                                       usingBlock:^(NSNotification *note) {
                                                                           [self refreshDevices];
                                                                       }];
        _observers = [[NSArray alloc] initWithObjects:runtimeErrorObserver, didStartRunningObserver, didStopRunningObserver, deviceWasConnectedObserver, deviceWasDisconnectedObserver, nil];
        
        _videoOutput = [[AVCaptureVideoDataOutput alloc] init];
        
        dispatch_queue_t queue = dispatch_queue_create("capture_queue", NULL);
        [_videoOutput setSampleBufferDelegate:self queue:queue];
        
        _videoOutput.videoSettings = [NSDictionary dictionaryWithObject:
                                     [NSNumber numberWithInt:kCVPixelFormatType_32BGRA]
                                                                forKey:(id)kCVPixelBufferPixelFormatTypeKey];
        
        [_session addOutput: _videoOutput];
        
        // Select devices if any exist
        AVCaptureDevice *videoDevice = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
        if (videoDevice)
            [self setSelectedVideoDevice:videoDevice];
        
        // Initial refresh of device list
        [self refreshDevices];
    }
    return self;
}

-(void)dealloc
{
    [[self session] stopRunning];
    
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    for (id observer in [self observers])
        [notificationCenter removeObserver:observer];
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
  didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection
{
    
}


- (void)captureOutput:(AVCaptureOutput *)captureOutput
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection
{
    if (self.delegate && [self.delegate respondsToSelector:@selector(cameraCapturer:didDeliveredBGRAFrameData:)])
    {
        [_lock lock];
        
        CVImageBufferRef videoFrame = CMSampleBufferGetImageBuffer(sampleBuffer);
        const int kFlags = 0;
        
        if (CVPixelBufferLockBaseAddress(videoFrame, kFlags) == kCVReturnSuccess)
        {
            void *baseAddress = CVPixelBufferGetBaseAddress(videoFrame);
            size_t bytesPerRow = CVPixelBufferGetBytesPerRow(videoFrame);
            int frameHeight = CVPixelBufferGetHeight(videoFrame);
            int frameSize = bytesPerRow * frameHeight;
            
#if 0
            VideoCaptureCapability tempCaptureCapability;
            tempCaptureCapability.width = _frameWidth;
            tempCaptureCapability.height = _frameHeight;
            tempCaptureCapability.maxFPS = _frameRate;
            tempCaptureCapability.rawType = kVideoBGRA;
#endif
            
            NSData *frameData = [NSData dataWithBytes: baseAddress length: frameSize];
            [self.delegate cameraCapturer: self didDeliveredBGRAFrameData: frameData];

            CVPixelBufferUnlockBaseAddress(videoFrame, kFlags);
        }
        
        [_lock unlock];
    }
}

- (AVCaptureDeviceFormat *)videoDeviceFormat
{
    return [[self selectedVideoDevice] activeFormat];
}

- (void)setVideoDeviceFormat:(AVCaptureDeviceFormat *)deviceFormat
{
    NSError *error = nil;
    AVCaptureDevice *videoDevice = [self selectedVideoDevice];
    if ([videoDevice lockForConfiguration:&error]) {
        [videoDevice setActiveFormat:deviceFormat];
        [videoDevice unlockForConfiguration];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^(void) {
            [self presentError:error];
        });
    }
}

- (AVCaptureDevice *)selectedVideoDevice
{
    return [_videoDeviceInput device];
}

- (void)setSelectedVideoDevice:(AVCaptureDevice *)selectedVideoDevice
{
    [[self session] beginConfiguration];
    
    if ([self videoDeviceInput]) {
        // Remove the old device input from the session
        [_session removeInput:[self videoDeviceInput]];
        [self setVideoDeviceInput:nil];
    }
    
    if (selectedVideoDevice) {
        NSError *error = nil;
        
        // Create a device input for the device and add it to the session
        AVCaptureDeviceInput *newVideoDeviceInput = [AVCaptureDeviceInput deviceInputWithDevice:selectedVideoDevice error:&error];
        if (newVideoDeviceInput == nil) {
            dispatch_async(dispatch_get_main_queue(), ^(void) {
                [self presentError:error];
            });
        } else {
            if (![selectedVideoDevice supportsAVCaptureSessionPreset:[_session sessionPreset]])
                [[self session] setSessionPreset:AVCaptureSessionPresetHigh];
            
            [[self session] addInput:newVideoDeviceInput];
            [self setVideoDeviceInput:newVideoDeviceInput];
        }
    }
    
    [[self session] commitConfiguration];
}

- (void)refreshDevices
{
    [self setVideoDevices:[[AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo] arrayByAddingObjectsFromArray:[AVCaptureDevice devicesWithMediaType:AVMediaTypeMuxed]]];
    
    [[self session] beginConfiguration];
    
    if (![[self videoDevices] containsObject:[self selectedVideoDevice]])
        [self setSelectedVideoDevice:nil];
    
    [[self session] commitConfiguration];
}

-(void)presentError:(NSError*)error
{
    if (self.delegate && [self.delegate respondsToSelector:@selector(cameraCapturer:didObtainedError:)])
        [self.delegate cameraCapturer: self didObtainedError: error];
}

+(NSArray*)getDevices
{
    return [[AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo] arrayByAddingObjectsFromArray:[AVCaptureDevice devicesWithMediaType:AVMediaTypeMuxed]];
}

@end

//******************************************************************************
//******************************************************************************
// AVCaptureDeviceFormat additions

@interface AVCaptureDeviceFormat (CCAdditions)

@property (readonly) NSString *localizedName;

@end

@implementation AVCaptureDeviceFormat (CCAdditions)

- (NSString *)localizedName
{
    NSString *localizedName = nil;
    
    CMMediaType mediaType = CMFormatDescriptionGetMediaType([self formatDescription]);
    
    switch (mediaType)
    {
        case kCMMediaType_Video:
        {
            CFStringRef formatName = CMFormatDescriptionGetExtension([self formatDescription], kCMFormatDescriptionExtension_FormatName);
            CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions((CMVideoFormatDescriptionRef)[self formatDescription]);
            localizedName = [NSString stringWithFormat:@"%@, %d x %d", formatName, dimensions.width, dimensions.height];
        }
            break;
            /*
        case kCMMediaType_Audio:
        {
            CFStringRef formatName = NULL;
            AudioStreamBasicDescription const *originalASBDPtr = CMAudioFormatDescriptionGetStreamBasicDescription((CMAudioFormatDescriptionRef)[self formatDescription]);
            if (originalASBDPtr)
            {
                size_t channelLayoutSize = 0;
                AudioChannelLayout const *channelLayoutPtr = CMAudioFormatDescriptionGetChannelLayout((CMAudioFormatDescriptionRef)[self formatDescription], &channelLayoutSize);
                
                CFStringRef channelLayoutName = NULL;
                if (channelLayoutPtr && (channelLayoutSize > 0))
                {
                    UInt32 propertyDataSize = (UInt32)sizeof(channelLayoutName);
                    AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutName, (UInt32)channelLayoutSize, channelLayoutPtr, &propertyDataSize, &channelLayoutName);
                }
                
                if (channelLayoutName && (0 == CFStringGetLength(channelLayoutName)))
                {
                    CFRelease(channelLayoutName);
                    channelLayoutName = NULL;
                }
                
                AudioStreamBasicDescription modifiedASBD = *originalASBDPtr;
                if (channelLayoutName)
                {
                    // If the format will include the description of a channel layout, zero out mChannelsPerFrame so that the number of channels does not redundantly appear in the format string.
                    modifiedASBD.mChannelsPerFrame = 0;
                }
                
                UInt32 propertyDataSize = (UInt32)sizeof(formatName);
                AudioFormatGetProperty(kAudioFormatProperty_FormatName, (UInt32)sizeof(modifiedASBD), &modifiedASBD, &propertyDataSize, &formatName);
                if (!formatName)
                {
                    formatName = CMFormatDescriptionGetExtension([self formatDescription], kCMFormatDescriptionExtension_FormatName);
                    if (formatName)
                        CFRetain(formatName);
                }
                
                if (formatName)
                {
                    if (channelLayoutName)
                    {
                        localizedName = [NSString stringWithFormat:@"%@, %@", formatName, channelLayoutName];
                    }
                    else
                    {
                        localizedName = [NSString stringWithFormat:@"%@", formatName];
                    }
                    
                    CFRelease(formatName);
                }
                
                if (channelLayoutName)
                    CFRelease(channelLayoutName);
            }
        }
            break;*/
        default:
            break;
    }
    
    return localizedName;
}

@end
