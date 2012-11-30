#include "HdlcAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "HdlcAnalyzer.h"
#include "HdlcAnalyzerSettings.h"
#include <iostream>
#include <fstream>

HdlcAnalyzerResults::HdlcAnalyzerResults( HdlcAnalyzer* analyzer, HdlcAnalyzerSettings* settings )
:	AnalyzerResults(),
	mSettings( settings ),
	mAnalyzer( analyzer )
{
}

HdlcAnalyzerResults::~HdlcAnalyzerResults()
{
}

void HdlcAnalyzerResults::GenerateBubbleText( U64 frame_index, Channel& /*channel*/, DisplayBase display_base )
{
	GenBubbleText(frame_index, display_base, false);
}

// TODO: show more info, like extended or basic
void HdlcAnalyzerResults::GenAddressFieldString(const Frame & frame, DisplayBase display_base, bool tabular) 
{
	char number_str[128];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 16, number_str, 128 );
	
	if( !tabular ) 
	{
		AddResultString( "A" );
		AddResultString( "AD" );
		AddResultString( "ADDR" );
		AddResultString( "A[", number_str, "]" );
	}
	AddResultString( "Address Field [", number_str, "]" );
}

void HdlcAnalyzerResults::GenInformationFieldString(const Frame & frame, const DisplayBase display_base,
													bool tabular ) 
{
	if( !tabular ) 
	{
		AddResultString( "I" );
		AddResultString( "INFO" );
		AddResultString( "INFORMATION" );
	}
	AddResultString( "Information Field" );
}


void HdlcAnalyzerResults::GenBubbleText( U64 frame_index, DisplayBase display_base, bool tabular ) 
{
	ClearResultStrings();
	Frame frame = GetFrame( frame_index );
    
	switch( frame.mType ) 
	{
		case HDLC_FIELD_FLAG: 
			GenFlagFieldString(tabular);
			break;
		case HDLC_FIELD_ADDRESS:
			GenAddressFieldString(frame, display_base, tabular);
			break;
		case HDLC_FIELD_CONTROL:
			GenControlFieldString(frame, display_base, tabular);
			break;
		case HDLC_FIELD_INFORMATION:
			GenInformationFieldString(frame, display_base, tabular);
			break;
		case HDLC_FIELD_FCS:
			GenFcsFieldString(frame, display_base, tabular);
			break;
	}
}

// TODO: Show type of frame I, U or S  and extended or basic
void HdlcAnalyzerResults::GenControlFieldString( const Frame & frame, DisplayBase display_base, bool tabular )
{
	char number_str[128];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 16, number_str, 128 );
	
	if( !tabular ) 
	{
		AddResultString( "CTL" );
		AddResultString( "CTL [", number_str, "]" );
	}
	AddResultString( "Control Field [", number_str, "]" );
}

// TODO: show algorithm CRC8,16,32
void HdlcAnalyzerResults::GenFcsFieldString( const Frame & frame, DisplayBase display_base, bool tabular )
{
	char number_str[128];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 16, number_str, 128 );
	if( !tabular ) 
	{
		AddResultString( "CRC" );
	}
	AddResultString( "CRC [", number_str, "]" );
}

void HdlcAnalyzerResults::GenFlagFieldString(bool tabular) 
{
	if( !tabular ) 
	{
		AddResultString("F");
		AddResultString("FL");
		AddResultString("FLAG");
	}
	AddResultString("Flag Delimiter");
}

void HdlcAnalyzerResults::GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id )
{
	/*
	std::ofstream file_stream( file, std::ios::out );

	U64 trigger_sample = mAnalyzer->GetTriggerSample();
	U32 sample_rate = mAnalyzer->GetSampleRate();

	file_stream << "Time [s],Value" << std::endl;

	U64 num_frames = GetNumFrames();
	for( U32 i=0; i < num_frames; i++ )
	{
		Frame frame = GetFrame( i );
		
		char time_str[128];
		AnalyzerHelpers::GetTimeString( frame.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, 128 );

		char number_str[128];
		AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );

		file_stream << time_str << "," << number_str << std::endl;

		if( UpdateExportProgressAndCheckForCancel( i, num_frames ) == true )
		{
			file_stream.close();
			return;
		}
	}

	file_stream.close();
	*/
}

void HdlcAnalyzerResults::GenerateFrameTabularText( U64 frame_index, DisplayBase display_base )
{
	GenBubbleText(frame_index, display_base, true);
}

void HdlcAnalyzerResults::GeneratePacketTabularText( U64 packet_id, DisplayBase display_base )
{
	ClearResultStrings();
	AddResultString( "not supported" );
}

void HdlcAnalyzerResults::GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base )
{
	ClearResultStrings();
	AddResultString( "not supported" );
}