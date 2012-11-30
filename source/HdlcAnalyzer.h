#ifndef HDLC_ANALYZER_H
#define HDLC_ANALYZER_H

#include <Analyzer.h>
#include "HdlcAnalyzerResults.h"
#include "HdlcSimulationDataGenerator.h"

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
	std::auto_ptr< HdlcAnalyzerSettings > mSettings;
	std::auto_ptr< HdlcAnalyzerResults > mResults;
	AnalyzerChannelData* mHdlc;

	HdlcSimulationDataGenerator mSimulationDataGenerator;
	bool mSimulationInitilized;

};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer( );
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer( Analyzer* analyzer );

#endif //HDLC_ANALYZER_H
