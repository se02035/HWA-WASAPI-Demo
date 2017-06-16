#include "pch.h"
#include "dxgi1_4.h"
#include "assert.h"
#include <Audioclient.h>
#include "DbgLog.h"
#include <mfidl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include "SamplePlayer.h"
#include "BasicTimer.h"

using namespace NativePlayerComponent;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;

SamplePlayer::SamplePlayer()
{
	//Initialize();
}

void SamplePlayer::Initialize()
{
	InitDxSampleFileLog();
	m_MFTSample.Initialize(L"http://www.hochmuth.com/mp3/Beethoven_12_Variation.mp3");
	m_fPipelineSetup = true;
	m_timer = ref new BasicTimer();
	return;
}

void SamplePlayer::Shutdown()
{
	m_MFTSample.ShutDown();
	return;
}

void SamplePlayer::VolumeIncrease()
{
	m_MFTSample.VolumeIncrease();
	return;
}

void SamplePlayer::VolumeDecrease()
{
	m_MFTSample.VolumeDecrease();
	return;
}

void SamplePlayer::Mute()
{
	m_MFTSample.Mute();
	return;
}

void SamplePlayer::Unmute()
{
	m_MFTSample.Unmute();
	return;
}
