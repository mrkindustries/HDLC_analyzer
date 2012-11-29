#ifndef HDLC_SIMULATION_DATA_GENERATOR
#define HDLC_SIMULATION_DATA_GENERATOR

#include <SimulationChannelDescriptor.h>
#include <string>
class HdlcAnalyzerSettings;

class HdlcSimulationDataGenerator
{
public:
	HdlcSimulationDataGenerator();
	~HdlcSimulationDataGenerator();

	void Initialize( U32 simulation_sample_rate, HdlcAnalyzerSettings* settings );
	U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channel );

protected:
	HdlcAnalyzerSettings* mSettings;
	U32 mSimulationSampleRateHz;

	SimulationChannelDescriptor mHdlcSimulationData;

};
#endif //HDLC_SIMULATION_DATA_GENERATOR