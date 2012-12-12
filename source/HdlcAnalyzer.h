#ifndef HDLC_ANALYZER_H
#define HDLC_ANALYZER_H

#include <Analyzer.h>
#include "HdlcAnalyzerResults.h"
#include "HdlcSimulationDataGenerator.h"

struct AsyncByte 
{
	U64 startSample;
	U64 endSample;
	U8 value;
};

typedef AsyncByte BitSequence;

class HdlcAnalyzerSettings;
class ANALYZER_EXPORT HdlcAnalyzer : public Analyzer
{
public:
	HdlcAnalyzer();
	virtual ~HdlcAnalyzer();
	virtual void WorkerThread();

	virtual U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels );
	virtual U32 GetMinimumSampleRateHz();

	virtual const char* GetAnalyzerName() const;
	virtual bool NeedsRerun();

protected:
	
	void SetupAnalyzer();
	
	// General function to read a byte
	AsyncByte ReadByte();
	
	// Function to read and process a HDLC frame
	void ProcessHDLCFrame();
	
	// Bit Sync Transmission functions
	BitSequence BitSyncProcessFlags();
	BitState BitSyncReadBit();	
	BitSequence BitSyncReadByte();
	
	// Byte Async Transmission functions
	AsyncByte ByteAsyncProcessFlags();
	AsyncByte ByteAsyncReadByte();
	
	void ProcessAddressField( AsyncByte byteAfterFlag );
	void ProcessControlField();
	vector<AsyncByte> ReadProcessAndFcsField();
	void ProcessInfoAndFcsField();
	void InfoAndFcsField(vector<AsyncByte> informationAndFcs);
	void ProcessInformationField(const vector<AsyncByte> & information);
	void ProcessFcsField(const vector<AsyncByte> & fcs);
	
	// Helper functions
	Frame CreateFrame( U8 mType, U64 mStartingSampleInclusive, U64 mEndingSampleInclusive, 
					   U64 mData1=0, U64 mData2=0, U8 mFlags=0 ) const;
	U8 Bit5Inv( U8 value ) const;
	vector<U8> AsyncBytesToVectorBytes(const vector<AsyncByte> & asyncBytes) const;
	U64 VectorToValue(const vector<U8> & v) const;
	
	std::auto_ptr< HdlcAnalyzerSettings > mSettings;
	std::auto_ptr< HdlcAnalyzerResults > mResults;
	AnalyzerChannelData* mHdlc;
	
	U32 mSampleRateHz;
	U32 mSamplesInHalfPeriod;
	U32 mSamplesIn7Bits;
	
	vector<U8> mCurrentFrameBytes;
	
	BitState mPreviousBitState;
	
	HdlcSimulationDataGenerator mSimulationDataGenerator;
	bool mSimulationInitilized;

};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer( );
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer( Analyzer* analyzer );

#endif //HDLC_ANALYZER_H
