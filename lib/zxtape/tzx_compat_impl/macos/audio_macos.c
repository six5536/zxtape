#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AUComponent.h>
// #include <Cocoa/Cocoa.h>
#include <AudioUnit/AudioUnitCarbonView.h>
#include <CoreAudio/CoreAudio.h>
#include <pthread.h>

#include "audio_macos.h"

// A macro to simplify error handling a bit.
#define SuccessOrBail(error)     \
  do {                           \
    err = error;                 \
    if (err != noErr) goto bail; \
  } while (false)

bool SettingsMute = false;
bool SettingsSoundSync = false;
bool SettingsSixteenBitSound = false;
bool SettingsStereo = false;
bool SettingsReverseStereo = false;
uint32 SettingsSoundPlaybackRate = 32000;
uint32 SettingsSoundInputRate = 32000;

SInt32 macSoundVolume = 80;       // %
uint32 macSoundBuffer_ms = 100;   // ms
uint32 macSoundInterval_ms = 16;  // ms
bool macSoundLagEnable = false;
uint16 aueffect = 0;

static AUGraph agraph;
static AUNode outNode;
static AudioUnit outAU;
static pthread_mutex_t mutex;
static UInt32 outStoredFrames, devStoredFrames;

/* Forward declarations */
static void ConnectAudioUnits(void);
static void DisconnectAudioUnits(void);
// static void SaveEffectPresets(void);
// static void LoadEffectPresets(void);
static void SetAudioUnitSoundFormat(void);
static void SetAudioUnitVolume(void);
static void StoreBufferFrameSize(void);
static void ChangeBufferFrameSize(void);
// static void ReplaceAudioUnitCarbonView (void);
// static void ResizeSoundEffectsDialog (HIViewRef);
static void MacFinalizeSamplesCallBack(void *);
static OSStatus MacAURenderCallBack(void *, AudioUnitRenderActionFlags *, const AudioTimeStamp *, UInt32, UInt32,
                                    AudioBufferList *);

void InitMacSound() {
  OSStatus err;

  AudioComponentDescription outdesc;

  // There are several Different types of AudioUnits.
  // Some audio units serve as Outputs, Mixers, or DSP
  // units. See AUComponent.h for listing

  outdesc.componentType = kAudioUnitType_Output;

  // Every Component has a subType, which will give a clearer picture
  // of what this components function will be.

  outdesc.componentSubType = kAudioUnitSubType_DefaultOutput;

  // All AudioUnits in AUComponent.h must use
  //"kAudioUnitManufacturer_Apple" as the Manufacturer
  outdesc.componentManufacturer = kAudioUnitManufacturer_Apple;

  outdesc.componentFlags = 0;
  outdesc.componentFlagsMask = 0;

  SuccessOrBail(NewAUGraph(&agraph));

  SuccessOrBail(AUGraphAddNode(agraph, &outdesc, &outNode));

  SuccessOrBail(AUGraphOpen(agraph));

  SuccessOrBail(AUGraphNodeInfo(agraph, outNode, NULL, &outAU));

  // // Finds a component that meets the desc spec's
  // comp = FindNextComponent(NULL, &desc);
  // if (comp == NULL) exit(-1);

  // // gains access to the services provided by the component
  // err = OpenAComponent(comp, theOutputUnit);

  // /***Getting the size of a Property***/
  // UInt32 size;

  // // Gets the size of the Stream Format Property and if it is writable
  // OSStatus result = AudioUnitGetPropertyInfo(*theUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0,
  //                                            &size, &outWritable);

  // // Get the current stream format of the output
  // result = AudioUnitGetProperty(*theUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, theDesc,
  // &size);

  // // Set the stream format of the output to match the input
  // result = AudioUnitSetProperty(*theUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, theInputBus,
  // theDesc,
  //                               size);

  SetAudioUnitSoundFormat();
  SetAudioUnitVolume();
  StoreBufferFrameSize();
  ChangeBufferFrameSize();

  SuccessOrBail(AUGraphInitialize(agraph));

  //
  ConnectAudioUnits();

  pthread_mutex_init(&mutex, NULL);
  // TODO S9xSetSamplesAvailableCallback(MacFinalizeSamplesCallBack, NULL);

bail:
  if (err != noErr) {
    printf("Error: %d\n", (int)err);
    assert(0);
  }
}

void DeinitMacSound(void) {
  OSStatus err;

  pthread_mutex_destroy(&mutex);
  DisconnectAudioUnits();
  err = AUGraphUninitialize(agraph);
  err = AUGraphClose(agraph);
  err = DisposeAUGraph(agraph);
}

static void SetAudioUnitSoundFormat(void) {
  OSStatus err;
  AudioStreamBasicDescription format;

#ifdef __BIG_ENDIAN__
  UInt32 endian = kLinearPCMFormatFlagIsBigEndian;
#else
  UInt32 endian = 0;
#endif

  memset(&format, 0, sizeof(format));

  format.mSampleRate = (Float64)SettingsSoundPlaybackRate;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = endian | (SettingsSixteenBitSound ? kLinearPCMFormatFlagIsSignedInteger : 0);
  format.mBytesPerPacket = 2 * (SettingsSixteenBitSound ? 2 : 1);
  format.mFramesPerPacket = 1;
  format.mBytesPerFrame = 2 * (SettingsSixteenBitSound ? 2 : 1);
  format.mChannelsPerFrame = 2;
  format.mBitsPerChannel = SettingsSixteenBitSound ? 16 : 8;

  err = AudioUnitSetProperty(outAU, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, sizeof(format));
}

static void SetAudioUnitVolume(void) {
  OSStatus err;

  err = AudioUnitSetParameter(outAU, kAudioUnitParameterUnit_LinearGain, kAudioUnitScope_Output, 0,
                              (float)macSoundVolume / 100.0f, 0);
}

static void StoreBufferFrameSize(void) {
  OSStatus err;
  UInt32 size;
  AudioDeviceID device;
  AudioObjectPropertyAddress address;

  address.mSelector = kAudioDevicePropertyBufferFrameSize;
  address.mScope = kAudioObjectPropertyScopeGlobal;
  address.mElement = kAudioObjectPropertyElementMain;

  size = sizeof(AudioDeviceID);
  err = AudioUnitGetProperty(outAU, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &device, &size);

  size = sizeof(UInt32);

  err = AudioObjectGetPropertyData(device, &address, 0, NULL, &size, &devStoredFrames);

  size = sizeof(UInt32);
  err = AudioUnitGetProperty(outAU, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0,
                             &outStoredFrames, &size);
}

static void ChangeBufferFrameSize(void) {
  OSStatus err;
  UInt32 numframes, size;
  AudioDeviceID device;
  AudioObjectPropertyAddress address;

  address.mSelector = kAudioDevicePropertyBufferFrameSize;
  address.mScope = kAudioObjectPropertyScopeGlobal;
  address.mElement = kAudioObjectPropertyElementMain;

  size = sizeof(AudioDeviceID);
  err = AudioUnitGetProperty(outAU, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &device, &size);

  size = sizeof(UInt32);

  if (macSoundInterval_ms == 0) {
    err = AudioObjectSetPropertyData(device, &address, 0, NULL, size, &devStoredFrames);

    err = AudioUnitSetProperty(outAU, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0,
                               &outStoredFrames, size);

    printf("Interval: system, Frames: %d/%d\n", (int)devStoredFrames, (int)outStoredFrames);
  } else {
    numframes = macSoundInterval_ms * SettingsSoundPlaybackRate / 1000;

    err = AudioObjectSetPropertyData(device, &address, 0, NULL, size, &numframes);

    err = AudioUnitSetProperty(outAU, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &numframes,
                               size);

    printf("Interval: %dms, Frames: %d\n", (int)macSoundInterval_ms, (int)numframes);
  }
}

static void ConnectAudioUnits(void) {
  OSStatus err;
  AURenderCallbackStruct callback;

  callback.inputProc = MacAURenderCallBack;
  callback.inputProcRefCon = NULL;

  err = AUGraphSetNodeInputCallback(agraph, outNode, 0, &callback);
}

static void DisconnectAudioUnits(void) {
  OSStatus err;

  err = AUGraphClearConnections(agraph);
}

static bool hackMe = 0;

static OSStatus MacAURenderCallBack(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags,
                                    const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumFrames,
                                    AudioBufferList *ioData) {
  // HACk
  hackMe = !hackMe;

  if (SettingsMute) {
    memset(ioData->mBuffers[0].mData, 0, ioData->mBuffers[0].mDataByteSize);
    *ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
  } else if (SettingsStereo) {
    unsigned int samples;

    samples = ioData->mBuffers[0].mDataByteSize;
    if (SettingsSixteenBitSound) samples >>= 1;

    pthread_mutex_lock(&mutex);
    // TODO S9xMixSamples((uint8 *)ioData->mBuffers[0].mData, samples);
    pthread_mutex_unlock(&mutex);
  } else  // Manually map L to R
  {
    unsigned int monosmp;

    monosmp = ioData->mBuffers[0].mDataByteSize >> 1;
    if (SettingsSixteenBitSound) monosmp >>= 1;

    pthread_mutex_lock(&mutex);
    // TODO S9xMixSamples((uint8 *)ioData->mBuffers[0].mData, monosmp);

    memset((uint8 *)ioData->mBuffers[0].mData, hackMe ? 0xFF : 0x00, monosmp);

    pthread_mutex_unlock(&mutex);

    if (SettingsSixteenBitSound) {
      for (int i = monosmp - 1; i >= 0; i--)
        ((int16_t *)ioData->mBuffers[0].mData)[i * 2 + 1] = ((int16_t *)ioData->mBuffers[0].mData)[i * 2] =
            ((int16_t *)ioData->mBuffers[0].mData)[i];
    } else {
      for (int i = monosmp - 1; i >= 0; i--)
        ((int8_t *)ioData->mBuffers[0].mData)[i * 2 + 1] = ((int8_t *)ioData->mBuffers[0].mData)[i * 2] =
            ((int8_t *)ioData->mBuffers[0].mData)[i];
    }
  }

  return (noErr);
}

static void MacFinalizeSamplesCallBack(void *userData) {
  pthread_mutex_lock(&mutex);
  // TODO S9xFinalizeSamples();
  pthread_mutex_unlock(&mutex);
}

void MacStartSound(void) {
  OSStatus err;
  Boolean r = false;

  err = AUGraphIsRunning(agraph, &r);
  if (err == noErr && r == false) {
    err = AUGraphStart(agraph);
    printf("AUGraph started.\n");
  }
}

void MacStopSound(void) {
  OSStatus err;
  Boolean r = false;

  err = AUGraphIsRunning(agraph, &r);
  if (err == noErr && r == true) {
    err = AUGraphStop(agraph);
    printf("AUGraph stopped.\n");
  }
}

bool S9xOpenSoundDevice(void) {
  OSStatus err;

  err = AUGraphUninitialize(agraph);

  SetAudioUnitSoundFormat();
  SetAudioUnitVolume();
  ChangeBufferFrameSize();

  err = AUGraphInitialize(agraph);

  return (true);
}

void PlayAlertSound(void) {
  //
  AudioServicesPlayAlertSound(kUserPreferredAlert);
}

// static void ReplaceAudioUnitCarbonView (void)
// {
// 	OSStatus				err;
// 	AudioUnit				editau;
// 	Component				cmp;
// 	ComponentDescription	desc;
// 	HIViewRef				pane, contentview, ctl;
// 	HIViewID				cid;
// 	Float32Point			location, size;
// 	Rect					rct;
// 	UInt32					psize;

// 	if (carbonView)
// 	{
// 		err = RemoveEventHandler(carbonViewEventRef);
// 		DisposeEventHandlerUPP(carbonViewEventUPP);
// 		carbonViewEventRef = NULL;
// 		carbonViewEventUPP = NULL;

// 		CloseComponent(carbonView);
// 		carbonView = NULL;
// 	}

// 	switch (cureffect)
// 	{
// 		case kAUGraphEQ:
// 			editau = eqlAU;
// 			break;

// 		case kAUReverb:
// 		default:
// 			editau = revAU;
// 			break;
// 	}

// 	desc.componentType         = kAudioUnitCarbonViewComponentType;
// 	desc.componentSubType      = kAUCarbonViewSubType_Generic;
// 	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
// 	desc.componentFlags        = 0;
// 	desc.componentFlagsMask    = 0;

// 	err = AudioUnitGetPropertyInfo(editau, kAudioUnitProperty_GetUIComponentList, kAudioUnitScope_Global, 0, &psize,
// NULL); 	if (err == noErr)
// 	{
// 		ComponentDescription	*editors;
// 		int						nEditors;

// 		nEditors = psize / sizeof(ComponentDescription);

// 		editors = new ComponentDescription[nEditors];

// 		err = AudioUnitGetProperty(editau, kAudioUnitProperty_GetUIComponentList, kAudioUnitScope_Global, 0,
// editors, &psize); 		if (err == noErr) 			desc = editors[0];

// 		delete [] editors;
// 	}

// 	HIViewFindByID(HIViewGetRoot(effectWRef), kHIViewWindowContentID, &contentview);

// 	cmp = FindNextComponent(NULL, &desc);
// 	if (cmp)
// 	{
// 		err = OpenAComponent(cmp, &carbonView);
// 		if (err == noErr)
// 		{
// 			EventTypeSpec	event[] = { { kEventClassControl, kEventControlBoundsChanged } };

// 			GetWindowBounds(effectWRef, kWindowContentRgn, &rct);
// 			location.x = 20;
// 			location.y = 96;
// 			size.x     = rct.right  - rct.left;
// 			size.y     = rct.bottom - rct.top;

// 			err = AudioUnitCarbonViewCreate(carbonView, editau, effectWRef, contentview, &location, &size,
// &pane);

// 			carbonViewEventUPP = NewEventHandlerUPP(SoundEffectsCarbonViewEventHandler);
// 			err = InstallControlEventHandler(pane, carbonViewEventUPP, GetEventTypeCount(event), event,
// (void *) effectWRef, &carbonViewEventRef);

// 			ResizeSoundEffectsDialog(pane);
// 		}
// 		else
// 			carbonView = NULL;
// 	}
// 	else
// 		carbonView = NULL;

// 	cid.id = 0;
// 	cid.signature = 'Enab';
// 	HIViewFindByID(contentview, cid, &ctl);
// 	SetControl32BitValue(ctl, (aueffect & cureffect) ? 1 : 0);
// }

// OSStatus SetupCallbacks(AudioUnit *theOutputUnit, AURenderCallbackStruct *renderCallback) {
//   OSStatus err = noErr;
//   memset(renderCallback, 0, sizeof(AURenderCallbackStruct));

//   // inputProc takes a name of a method that will be used as the
//   // input procedure when rendering data to the AudioUnit.
//   // The input procedure will be called only when the Audio Converter needs
//   // more data to process.

//   // Set "fileRenderProc" as the name of the input proc
//   renderCallback->inputProc = MyFileRenderProc;
//   // Can pass ref Con with callback, but this isnt needed in out example
//   renderCallback->inputProcRefCon = 0;

//   // Sets the callback for the AudioUnit to the renderCallback

//   err = AudioUnitSetProperty(*theOutputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
//                              renderCallback, sizeof(AURenderCallbackStruct));
//   // Note: Some old V1 examples may use
//   //"kAudioUnitProperty_SetInputCallback" which existed in
//   // the old API, instead of "kAudioUnitProperty_SetRenderCallback".
//   //"kAudioUnitProperty_SetRenderCallback" should
//   // be used from now on.

//   return err;
// }