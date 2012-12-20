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

void HdlcAnalyzerResults::GenBubbleText( U64 frame_index, DisplayBase display_base, bool tabular ) 
{
	ClearResultStrings();
	Frame frame = GetFrame( frame_index );
    
	switch( frame.mType ) 
	{
		case HDLC_FIELD_FLAG: 
			GenFlagFieldString( frame, tabular );
			break;
		case HDLC_FIELD_BASIC_ADDRESS:
		case HDLC_FIELD_EXTENDED_ADDRESS:
			GenAddressFieldString( frame, display_base, tabular );
			break;
		case HDLC_FIELD_BASIC_CONTROL:
		case HDLC_FIELD_EXTENDED_CONTROL:
			GenControlFieldString( frame, display_base, tabular );
			break;
		case HDLC_FIELD_HCS:
			GenFcsFieldString( frame, display_base, tabular );
			break;
		case HDLC_FIELD_INFORMATION:
			GenInformationFieldString( frame, display_base, tabular );
			break;
		case HDLC_FIELD_FCS:
			GenFcsFieldString( frame, display_base, tabular );
			break;
		case HDLC_ESCAPE_SEQ:
			GenEscapeFieldString( tabular );
			break;
		case HDLC_ABORT_SEQ:
			GenAbortFieldString( tabular );
			break;

	}
}

void HdlcAnalyzerResults::GenFlagFieldString( const Frame & frame, bool tabular ) 
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
		AddResultString( "F" );
		AddResultString( "FL" );
		AddResultString( "FLAG" );
		AddResultString( flagTypeStr, " FLAG" );
	}
	AddResultString( flagTypeStr, " Flag Delimiter" );
}

void HdlcAnalyzerResults::GenAddressFieldString( const Frame & frame, DisplayBase display_base, bool tabular ) 
{
	char addressStr[ 64 ];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, addressStr, 64 );
	char byteNumber[ 64 ];
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

void HdlcAnalyzerResults::GenInformationFieldString( const Frame & frame, const DisplayBase display_base,
													bool tabular ) 
{
	
	char informationStr[ 64 ];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, informationStr, 64 );
	char numberStr[ 64 ];
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
	char number_str[ 128 ];
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
	
	char readFcsStr[ 128 ];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, fcsBits, readFcsStr, 128 );
	/*
	char calculatedFcsStr[128];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 16, number_str, 128 );
	*/
	
	char* fieldNameStr=0;
	if( frame.mType == HDLC_FIELD_FCS )
	{
		if( frame.mFlags & DISPLAY_AS_ERROR_FLAG )
		{
			fieldNameStr = "!FCS";
		}
		else
		{
			fieldNameStr = "FCS";
		}
	}
	else
	{
		if( frame.mFlags & DISPLAY_AS_ERROR_FLAG )
		{
			fieldNameStr = "!HCS";
		}
		else
		{
			fieldNameStr = "HCS";
		}
	}
		
	if( frame.mFlags & DISPLAY_AS_ERROR_FLAG ) 
	{
		if( !tabular ) 
		{
			AddResultString( fieldNameStr , " CRC", crcTypeStr, " ERROR"  );
		}
		AddResultString( fieldNameStr, " CRC", crcTypeStr, " ERROR [", readFcsStr, "]" );	
	}
	else 
	{
		if( !tabular ) 
		{
			AddResultString( fieldNameStr, " CRC", crcTypeStr, "->ok" );
		}
		AddResultString( fieldNameStr, " CRC", crcTypeStr, "->ok [", readFcsStr, "]" );
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
	std::ofstream fileStream( file, std::ios::out );

	U64 triggerSample = mAnalyzer->GetTriggerSample();
	U32 sampleRate = mAnalyzer->GetSampleRate();

	fileStream << "Time [s],Address,Control,HCS,Information,FCS" << std::endl;

	U64 numFrames = GetNumFrames();
	U64 frameNumber = 0;
	for( ; ; )
	{
		Frame firstAddressframe = GetFrame( frameNumber++ );
		
		// 1)  Time [s]
		char timeStr[128];
		AnalyzerHelpers::GetTimeString( firstAddressframe.mStartingSampleInclusive, triggerSample, sampleRate, timeStr, 128 );
		fileStream << timeStr << ",";
		
		// 2) Address Field
		if ( !(firstAddressframe.mType == HDLC_FIELD_BASIC_ADDRESS || 
			 firstAddressframe.mType == HDLC_FIELD_EXTENDED_ADDRESS) ) // ERROR
		{
			fileStream << "," << endl;
			continue;
		}
		if( mSettings->mHdlcAddr == HDLC_BASIC_ADDRESS_FIELD )
		{
			char addressStr[64];
			AnalyzerHelpers::GetNumberString( firstAddressframe.mData1, display_base, 8, addressStr, 64 );
			fileStream << addressStr << ",";
		}
		else // Check for extended address
		{
			Frame nextAddress = firstAddressframe;
			for( ; ; )
			{
				char addressStr[64];
				AnalyzerHelpers::GetNumberString( nextAddress.mData1, display_base, 8, addressStr, 64 );
				fileStream << addressStr << ",";
				
				bool endOfAddress = ( ( nextAddress.value & 0x01 ) == 0 );
				if( endOfAddress ) // no more bytes of address?
				{
					break;
				}
				else
				{
					nextAddress = GetFrame( frameNumber++ );
				}
				
			}
		}
		
		// 3) Control Field
		
		
		if( UpdateExportProgressAndCheckForCancel( i, num_frames ) == true )
		{
			return;
		}
	}
	*/

}

void HdlcAnalyzerResults::GenerateFrameTabularText( U64 frame_index, DisplayBase display_base )
{
	GenBubbleText( frame_index, display_base, true );
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