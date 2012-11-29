#include "HdlcSimulationDataGenerator.h"
#include "HdlcAnalyzerSettings.h"

#include <AnalyzerHelpers.h>

HdlcSimulationDataGenerator::HdlcSimulationDataGenerator()
{
}

HdlcSimulationDataGenerator::~HdlcSimulationDataGenerator()
{
}

void HdlcSimulationDataGenerator::Initialize( U32 simulation_sample_rate, HdlcAnalyzerSettings* settings )
{
	mSimulationSampleRateHz = simulation_sample_rate;
	mSettings = settings;

	mHdlcSimulationData.SetChannel( mSettings->mInputChannel );
	mHdlcSimulationData.SetSampleRate( simulation_sample_rate );
	mHdlcSimulationData.SetInitialBitState( BIT_HIGH );
}

U32 HdlcSimulationDataGenerator::GenerateSimulationData( U64 largest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channel )
{
	U64 adjusted_largest_sample_requested = AnalyzerHelpers::AdjustSimulationTargetSample( largest_sample_requested, sample_rate, mSimulationSampleRateHz );

	while( mHdlcSimulationData.GetCurrentSampleNumber() < adjusted_largest_sample_requested )
	{
		
	}
	
	*simulation_channel = &mHdlcSimulationData;
	return 1;
}
