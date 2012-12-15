#ifndef HDLC_ANALYZER_H
#define HDLC_ANALYZER_H

#include <Analyzer.h>
#include "HdlcAnalyzerResults.h"
#include "HdlcSimulationDataGenerator.h"

struct HdlcByte 
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
	
	// Functions to read and process a HDLC frame
	void ProcessHDLCFrame();
	HdlcByte ProcessFlags();
	void ProcessAddressField( HdlcByte byteAfterFlag );
	void ProcessControlField();
	void ProcessInfoAndFcsField();
	vector<HdlcByte> ReadProcessAndFcsField();
	void InfoAndFcsField(const vector<HdlcByte> & informationAndFcs);
	void ProcessInformationField(const vector<HdlcByte> & information);
	void ProcessFcsField(const vector<HdlcByte> & fcs);
	HdlcByte ReadByte();
	
	// Bit Sync Transmission functions
	void BitSyncProcessFlags();
	BitState BitSyncReadBit();	
	HdlcByte BitSyncReadByte();
	HdlcByte BitSyncProcessFirstByteAfterFlag( HdlcByte firstAddressByte );
	bool FlagComing();
	bool AbortComing();
	
	// Byte Async Transmission functions
	HdlcByte ByteAsyncProcessFlags();
	void GenerateFlagsFrames( vector<HdlcByte> readBytes ) ;
	HdlcByte ByteAsyncReadByte();
	HdlcByte ByteAsyncReadByte_();
	
	// Helper functions
	Frame CreateFrame( U8 mType, U64 mStartingSampleInclusive, U64 mEndingSampleInclusive, 
					   U64 mData1=0, U64 mData2=0, U8 mFlags=0 ) const;
	vector<U8> HdlcBytesToVectorBytes(const vector<HdlcByte> & asyncBytes) const;
	U64 VectorToValue(const vector<U8> & v) const;
	
protected:
	
	std::auto_ptr< HdlcAnalyzerSettings > mSettings;
	std::auto_ptr< HdlcAnalyzerResults > mResults;
	AnalyzerChannelData* mHdlc;
	
	U32 mSampleRateHz;
	U32 mSamplesInHalfPeriod;
	U32 mSamplesIn7Bits;
	U32 mSamplesIn8Bits;
	
	vector<U8> mCurrentFrameBytes;
	
	BitState mPreviousBitState;
	U32 mConsecutiveOnes;
	bool mReadingFrame;
	bool mAbortFrame;
	
	HdlcSimulationDataGenerator mSimulationDataGenerator;
	bool mSimulationInitilized;

};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer( );
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer( Analyzer* analyzer );

#endif //HDLC_ANALYZER_H
