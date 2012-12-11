#ifndef HDLC_ANALYZER_H
#define HDLC_ANALYZER_H

#include <Analyzer.h>
#include "HdlcAnalyzerResults.h"
#include "HdlcSimulationDataGenerator.h"

// TODO: Change functions names Byte por Async

struct AsyncByte 
{
	U64 startSample;
	U64 endSample;
	U8 value;
};

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
	
	// Bit Sync Transmission functions
	void ProcessBitSync();
	bool BitSyncProcessFlags();
	void BitSyncProcessAddressField();
	void BitSyncProcessControlField();
	void BitSyncProcessInfoAndFcsField();
	
	// Byte Async Transmission functions
	void ProcessByteAsync();
	AsyncByte ByteAsyncProcessFlags();
	void ByteAsyncProcessAddressField( AsyncByte byteAfterFlag );
	void ByteAsyncProcessControlField();
	vector<AsyncByte> ByteAsyncReadProcessAndFcsField();
	void ByteAsyncProcessInfoAndFcsField();
	void ByteAsyncInfoAndFcsField(vector<AsyncByte> informationAndFcs);
	void ByteAsyncProcessInformationField(const vector<AsyncByte> & information);
	void ByteAsyncProcessFcsField(const vector<AsyncByte> & fcs);
	AsyncByte ReadByte();
	
	// Helper functions
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
	
	HdlcSimulationDataGenerator mSimulationDataGenerator;
	bool mSimulationInitilized;

};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer( );
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer( Analyzer* analyzer );

#endif //HDLC_ANALYZER_H
