#ifndef HDLC_SIMULATION_DATA_GENERATOR
#define HDLC_SIMULATION_DATA_GENERATOR

#include <SimulationChannelDescriptor.h>
#include "HdlcAnalyzerSettings.h"
#include <string>
#include <vector>

using namespace std;

class HdlcSimulationDataGenerator
{
public:
	HdlcSimulationDataGenerator();
	~HdlcSimulationDataGenerator();

	void Initialize( U32 simulation_sample_rate, HdlcAnalyzerSettings* settings );
	U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channel );

protected:
	
	U64 SecondsToSamples( U64 us ) const;

	void CreateHDLCFrame( const vector<U8> & address, const vector<U8> & control, const vector<U8> & data );
	
	void CreateFlagSequence();
	void Transmit( const vector<U8> & stream );
	void CreateBit( BitState bitState );
	
	vector<U8> GenFcs( HdlcFcsType fcsType, const vector<U8> & stream );
	vector<U8> Crc8( const vector<U8> & stream );
	vector<U8> Crc16( const vector<U8> & stream );
	vector<U8> Crc32( const vector<U8> & stream );
	
	vector<U8> GenAddressField( HdlcAddressType addressType, U64 addressBytes, U8 value );
	vector<U8> GenControlField( HdlcFrameType frameType, HdlcControlType controlType, U8 value );
	vector<U8> GenDataField( U16 size, U8 value );

	U64 mSamplesInHalfPeriod;
	
	HdlcAnalyzerSettings* mSettings;
	U32 mSimulationSampleRateHz;
	
	bool mPfBitValue;

	SimulationChannelDescriptor mHdlcSimulationData;

};
#endif //HDLC_SIMULATION_DATA_GENERATOR