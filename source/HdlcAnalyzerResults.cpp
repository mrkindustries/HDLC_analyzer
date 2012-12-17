#include "HdlcAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "HdlcAnalyzer.h"
#include "HdlcAnalyzerSettings.h"
#include <iostream>
#include <fstream>

// NOTE: If the Bubble has >30 characters, there is a bug and the bubbles dissapears/appears

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

void HdlcAnalyzerResults::GenBubbleText( U64 frame_index, DisplayBase display_base, bool tabular ) 
{
	ClearResultStrings();
	Frame frame = GetFrame( frame_index );
    
	switch( frame.mType ) 
	{
		case HDLC_FIELD_FLAG: 
			GenFlagFieldString(frame, tabular);
			break;
		case HDLC_FIELD_BASIC_ADDRESS:
		case HDLC_FIELD_EXTENDED_ADDRESS:
			GenAddressFieldString( frame, display_base, tabular );
			break;
		case HDLC_FIELD_BASIC_CONTROL:
		case HDLC_FIELD_EXTENDED_CONTROL:
			GenControlFieldString( frame, display_base, tabular );
			break;
		case HDLC_FIELD_INFORMATION:
			GenInformationFieldString(frame, display_base, tabular);
			break;
		case HDLC_FIELD_FCS:
			GenFcsFieldString(frame, display_base, tabular);
			break;
		case HDLC_ESCAPE_SEQ:
			GenEscapeFieldString(tabular);
			break;
		case HDLC_ABORT_SEQ:
			GenAbortFieldString(tabular);
			break;

	}
}

void HdlcAnalyzerResults::GenFlagFieldString(const Frame & frame, bool tabular) 
{
	char* flagTypeStr=0;
	switch( frame.mData1 )
	{
		case HDLC_FLAG_START: flagTypeStr = "Start"; break;
		case HDLC_FLAG_END: flagTypeStr = "End"; break;
		case HDLC_FLAG_FILL: flagTypeStr = "Fill"; break;
	}
	
	if( !tabular ) 
	{
		AddResultString("F");
		AddResultString("FL");
		AddResultString("FLAG");
		AddResultString(flagTypeStr, " FLAG");
	}
	AddResultString(flagTypeStr, " Flag Delimiter");
}

void HdlcAnalyzerResults::GenAddressFieldString(const Frame & frame, DisplayBase display_base, bool tabular) 
{
	char addressStr[64];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, addressStr, 64 );
	char byteNumber[64];
	AnalyzerHelpers::GetNumberString( frame.mData2, Decimal, 8, byteNumber, 64 );
	
	if( !tabular ) 
	{
		AddResultString( "A" );
		AddResultString( "AD" );
		AddResultString( "ADDR" );
		AddResultString( "ADDR ", byteNumber ,"[", addressStr, "]" );
	}
	AddResultString( "Address ", byteNumber , "[", addressStr, "]" );
}

void HdlcAnalyzerResults::GenInformationFieldString(const Frame & frame, const DisplayBase display_base,
													bool tabular ) 
{
	
	char informationStr[64];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, informationStr, 64 );
	char numberStr[64];
	AnalyzerHelpers::GetNumberString( frame.mData2, Decimal, 32, numberStr, 64 );
	
	if( !tabular ) 
	{
		AddResultString( "I" );
		AddResultString( "I ", numberStr );
		AddResultString( "I ", numberStr, " [", informationStr ,"]" );
	}
	AddResultString( "Info ", numberStr, " [", informationStr ,"]" );
}

void HdlcAnalyzerResults::GenControlFieldString( const Frame & frame, DisplayBase display_base, bool tabular )
{
	char number_str[128];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 16, number_str, 128 );
	
	char* frameTypeStr=0;
	switch( frame.mData2 )
	{
		case HDLC_I_FRAME: frameTypeStr = "I"; break;
		case HDLC_S_FRAME: frameTypeStr = "S"; break;
		case HDLC_U_FRAME: frameTypeStr = "U"; break;
	}
	
	if( !tabular ) 
	{
		AddResultString( "C" );
		AddResultString( "CTL" );
		AddResultString( "CTL [", number_str, "]" );
		AddResultString( "CTL [", number_str, "] - ", frameTypeStr, "-frame" );
	}
	AddResultString( "Control [", number_str, "] - ", frameTypeStr, "-frame" );
}

// TODO: show algorithm CRC8,16,32 and show success or wrong crc
void HdlcAnalyzerResults::GenFcsFieldString( const Frame & frame, DisplayBase display_base, bool tabular )
{
	U32 fcsBits=0;
	char* crcTypeStr=0;
	switch( mSettings->mHdlcFcs )
	{
		case HDLC_CRC8: fcsBits = 8; crcTypeStr= "8"; break;
		case HDLC_CRC16: fcsBits = 16; crcTypeStr= "16"; break;
		case HDLC_CRC32: fcsBits = 32; crcTypeStr= "32"; break;
	}
	
	char readFcsStr[128];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, fcsBits, readFcsStr, 128 );
	/*
	char calculatedFcsStr[128];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 16, number_str, 128 );
	*/
		
	if ( frame.mFlags & DISPLAY_AS_ERROR_FLAG ) 
	{
		if( !tabular ) 
		{
			AddResultString( "!CRC", crcTypeStr," ERROR"  );
		}
		AddResultString( "!CRC", crcTypeStr, " ERROR [", readFcsStr, "]" );	
	}
	else 
	{
		if( !tabular ) 
		{
			AddResultString( "CRC", crcTypeStr, "->ok" );
		}
		AddResultString( "CRC", crcTypeStr, "->ok [", readFcsStr, "]" );
	}
	
}

void HdlcAnalyzerResults::GenEscapeFieldString( bool tabular )
{
	if( !tabular ) 
	{
		AddResultString( "E" );
		AddResultString( "ESC" );
		AddResultString( "ESCAPE" );
		AddResultString( "ESCAPE (0x7D)" );
	}
	AddResultString( "ESCAPE (0x7D)" );
}

void HdlcAnalyzerResults::GenAbortFieldString( bool tabular )
{
	if( !tabular ) 
	{
		AddResultString( "AB" );
		AddResultString( "ABORT" );
	}
	AddResultString( "ABORT FRAME" );
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