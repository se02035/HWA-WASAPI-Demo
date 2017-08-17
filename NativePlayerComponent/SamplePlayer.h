#pragma once

#include "BasicTimer.h"
#include "MFTSample.h"

namespace NativePlayerComponent
{
    public ref class SamplePlayer sealed
    {
    public:
        SamplePlayer();
		void Initialize(Platform::String^ pInputStream);
		void Shutdown();

		void VolumeIncrease();
		void VolumeDecrease();
		void Mute();
		void Unmute();

	private:
		//void Initialize();


		BasicTimer^ m_timer;
		CMFTSample m_MFTSample;
		bool m_fPipelineSetup;

    };

}
