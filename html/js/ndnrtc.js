var localVideo;
var miniVideo;
var remoteVideo;
var localStream;
var remoteStream;
var started = false;
var turnDone = false;
var channelReady = false;
var signalingReady = false;
var msgQueue = [];
// Set up audio and video regardless of what devices are present.
var sdpConstraints = {'mandatory': {
    'OfferToReceiveAudio': true,
    'OfferToReceiveVideo': true }};
var isVideoMuted = false;
var isAudioMuted = false;
var mediaConstraints = { "audio": true, "video": true};
var ndnrtc = window.nrtcObject;

var  getUserMedia = navigator.mozGetUserMedia.bind(navigator);

// Attach a media stream to an element.
function attachMediaStream(element, stream) {
    console.log("Attaching media stream");
    
    element.mozSrcObject = stream;
    element.play();
}

// initialization
function initialize()
{
    console.log("initialization");
    
    card = document.getElementById('card');
    localVideo = document.getElementById('localVideo');
    miniVideo = document.getElementById('miniVideo');
    remoteVideo = document.getElementById('remoteVideo');
    
    ndnrtc.StartConference({"width":640, "height":480, "windowWidth":640, "windowWidth":480},function (state, args){
                           console.log("got response "+state+" with args: "+args);
                           });
    
//    doGetUserMedia();
}

function onUserMediaSuccess(stream) {
    console.log('User has granted access to local media.');
    // Call the polyfill wrapper to attach the media stream to this element.
    attachMediaStream(localVideo, stream);
    localVideo.style.opacity = 1;
    localStream = stream;
    
    console.log(ndnrtc.toSource());
    ndnrtc.AddUserMedia(stream);
}

function onUserMediaError(error) {
    console.log('Failed to get access to local media. Error code was ' +
                error.code);
    alert('Failed to get access to local media. Error code was ' + error.code + '.');
}

function doGetUserMedia() {
    try {
        getUserMedia(mediaConstraints, onUserMediaSuccess,
                     onUserMediaError);
        console.log('Requested access to local media with mediaConstraints:\n' +
                    '  \'' + JSON.stringify(mediaConstraints) + '\'');
    }
    catch (e) {
        alert('getUserMedia() failed. Is this a WebRTC capable browser?');
        console.log('getUserMedia failed with exception: ' + e.message);
    }
}
