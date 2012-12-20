#include "HdlcAnalyzer.h"
#include "HdlcAnalyzerSettings.h"
#include <AnalyzerChannelData.h>
#include <AnalyzerHelpers.h>
#include <iostream>

// TODO: Adjust AbortComing() and FrameComing() with REAL DATA
// TODO Suport:
// 		* HCS field (very similar to FCS field)
// 		* Frame Format Field

using namespace std;

HdlcAnalyzer::HdlcAnalyzer()
:	Analyzer(),  
	mSettings( new HdlcAnalyzerSettings() ),
	mSimulationInitilized( false )
{
	SetAnalyzerSettings( mSettings.get() );
}

HdlcAnalyzer::~HdlcAnalyzer()
{
	KillThread();
}

void HdlcAnalyzer::SetupAnalyzer()
{
	mResults.reset( new HdlcAnalyzerResults( this, mSettings.get() ) );
	SetAnalyzerResults( mResults.get() );
	mResults->AddChannelBubblesWillAppearOn( mSettings->mInputChannel );
	mHdlc = GetAnalyzerChannelData( mSettings->mInputChannel );

	double halfPeriod = (1.0 / double( mSettings->mBitRate * 2 ) ) * 1000000.0;
	mSampleRateHz = this->GetSampleRate();
	mSamplesInHalfPeriod = U32( ( mSampleRateHz * halfPeriod ) / 1000000.0 );
	mSamplesIn7Bits = mSamplesInHalfPeriod * 7;	
	mSamplesIn8Bits = mSamplesInHalfPeriod * 8;
	
	mPreviousBitState = mHdlc->GetBitState();
	mConsecutiveOnes = 0;
	mReadingFrame = false;
	mAbortFrame = false;
	mCurrentFrameIsSFrame = false;
}

void HdlcAnalyzer::WorkerThread()
{
	SetupAnalyzer();
	
	if( mSettings->mTransmissionMode == HDLC_TRANSMISSION_BIT_SYNC )
	{
		// Synchronize
		mHdlc->AdvanceToNextEdge();
	}
	
	// Main loop
	for( ; ; )
	{
		ProcessHDLCFrame();

		mResults->CommitResults();
		ReportProgress( mHdlc->GetSampleNumber() );
		CheckIfThreadShouldExit();
		
	}
}

//
/////////////// SYNC BIT TRAMISSION ///////////////////////////////////////////////
//

void HdlcAnalyzer::ProcessHDLCFrame()
{
	mCurrentFrameBytes.clear();
	
	HdlcByte addressByte = ProcessFlags();
	
	ProcessAddressField( addressByte );
	ProcessControlField();
	ProcessInfoAndFcsField();
	
	if( mAbortFrame ) // The frame has been aborted at some point
	{
		mResults->AddMarker( mHdlc->GetSampleNumber(), AnalyzerResults::ErrorX, mSettings->mInputChannel );
		if( mSettings->mTransmissionMode == HDLC_TRANSMISSION_BIT_SYNC )
		{
			// After abortion, synchronize again
			mHdlc->AdvanceToNextEdge();
		}
	}
	
	mReadingFrame = false;
	mAbortFrame = false;
	mCurrentFrameIsSFrame = false;
	
}

HdlcByte HdlcAnalyzer::ProcessFlags()
{
	HdlcByte addressByte;
	if( mSettings->mTransmissionMode == HDLC_TRANSMISSION_BIT_SYNC ) 
	{
		BitSyncProcessFlags();
		mReadingFrame = true;
		addressByte = ReadByte();
	}
	else 
	{
		mReadingFrame = true;
		addressByte = ByteAsyncProcessFlags();
	}
	
	return addressByte;
}

// NOTE: We assume two 0 between flags, i.e.: ...0111111001111110...
// NOTE: For one-zero btw flags won't use BitSyncReadByte()...
// NOTE: check for end of line (if no flags at all...)
// Interframe time fill: ISO/IEC 13239:2002(E) pag. 21
void HdlcAnalyzer::BitSyncProcessFlags()
{
	bool flagEncountered = false;
	vector<HdlcByte> flags;
	for( ; ; )
	{
		if( FlagComing() )
		{
			HdlcByte bs;
			bs.value = 0;
			
			bs.startSample = mHdlc->GetSampleNumber();
			mHdlc->AdvanceToNextEdge();
			bs.endSample = mHdlc->GetSampleNumber();
			
			flags.push_back(bs);
			
			if( !mSettings->mSharedZero )
			{
				mHdlc->Advance( mSamplesInHalfPeriod * 0.5 );
				mPreviousBitState = mHdlc->GetBitState();
				mHdlc->Advance( mSamplesInHalfPeriod * 0.5 );
			}
			
			flagEncountered = true;
		}
		else // non-flag
		{
			if( mSettings->mSharedZero )
			{
				mHdlc->Advance( mSamplesInHalfPeriod * 0.5 );
				mPreviousBitState = mHdlc->GetBitState();
				mHdlc->Advance( mSamplesInHalfPeriod * 0.5 );
			}
			
			if( flagEncountered )
			{
				break;
			}
			else // non-flag byte before a byte-flag is ignored
			{
				mHdlc->AdvanceToNextEdge();
			}
		}
	}
	
	for( U32 i=0; i < flags.size(); ++i )
	{
		Frame frame = CreateFrame( HDLC_FIELD_FLAG, flags.at(i).startSample, 
								  flags.at(i).endSample, HDLC_FLAG_FILL );
		if( i == flags.size() - 1 )
		{
			frame.mData1 = HDLC_FLAG_START;
		}
		mResults->AddFrame( frame );
	}
	
}

BitState HdlcAnalyzer::BitSyncReadBit()
{
	BitState ret;
	
	mHdlc->Advance( mSamplesInHalfPeriod * 0.5 );
	BitState bit = mHdlc->GetBitState(); // sample the bit
	
	if( bit == mPreviousBitState )
	{
		mConsecutiveOnes++;
		if( mReadingFrame && mConsecutiveOnes == 5 )
		{
			mHdlc->Advance( mSamplesInHalfPeriod );
			mConsecutiveOnes = 0;
			mPreviousBitState = mHdlc->GetBitState();
		}
		else 
		{
			mPreviousBitState = bit;
		}
		
		ret = BIT_HIGH;
	}
	else // bit changed so it's a 0
	{
		mConsecutiveOnes = 0;
		mPreviousBitState = bit;
		
		ret = BIT_LOW;
	}
	
	mHdlc->Advance( mSamplesInHalfPeriod * 0.5 );
	return ret;
}

bool HdlcAnalyzer::FlagComing()
{
	// TODO: check here if tolerance
	return !mHdlc->WouldAdvancingCauseTransition( mSamplesIn7Bits-1 ) &&
		   mHdlc->WouldAdvancingCauseTransition( mSamplesIn7Bits );
}

bool HdlcAnalyzer::AbortComing()
{
	// At least 7 bits in 1...
	// TODO: check here if tolerance
	return !mHdlc->WouldAdvancingCauseTransition( mSamplesIn7Bits + mSamplesInHalfPeriod * 0.5 );
}

HdlcByte HdlcAnalyzer::BitSyncReadByte()
{
	if( mReadingFrame && AbortComing() )
	{
			// Create "Abort Frame" frame
			U64 startSample = mHdlc->GetSampleNumber();
			mHdlc->Advance( mSamplesIn8Bits );
			U64 endSample = mHdlc->GetSampleNumber();
			
			Frame frame = CreateFrame( HDLC_ABORT_SEQ, startSample, endSample );
			mResults->AddFrame( frame );
	
			mAbortFrame = true;
			return HdlcByte();
	}
	
	if( mReadingFrame && FlagComing() )
	{
		U64 startSample = mHdlc->GetSampleNumber();
		mHdlc->AdvanceToNextEdge();
		U64 endSample = mHdlc->GetSampleNumber();
		HdlcByte bs = { startSample, endSample, HDLC_FLAG_VALUE };
		return bs;
	}

	U64 byteValue= 0;
	DataBuilder dbyte;
	dbyte.Reset( &byteValue, AnalyzerEnums::LsbFirst, 8 );
	U64 startSample = mHdlc->GetSampleNumber();
	for( U32 i=0; i<8 ; ++i )
	{
		BitState bit = BitSyncReadBit();
		dbyte.AddBit( bit );
	}
	U64 endSample = mHdlc->GetSampleNumber() - mSamplesInHalfPeriod;
	HdlcByte bs = { startSample, endSample, U8( byteValue ) };
	return bs;
}

//
/////////////// ASYNC BYTE TRAMISSION ///////////////////////////////////////////////
//

// Interframe time fill: ISO/IEC 13239:2002(E) pag. 21
HdlcByte HdlcAnalyzer::ByteAsyncProcessFlags()
{
	bool flagEncountered = false;
	// 1) Read bytes until non-flag byte
	vector<HdlcByte> readBytes;
	for( ; ; )
	{
		HdlcByte asyncByte = ReadByte(); 
		if( asyncByte.value != HDLC_FLAG_VALUE && flagEncountered ) // NOTE: ignore non-flag bytes!
		{
			readBytes.push_back( asyncByte );
			break;
		}
		else if( asyncByte.value == HDLC_FLAG_VALUE ) 
		{
			readBytes.push_back( asyncByte );
			flagEncountered = true;
		}
		if( mAbortFrame ) 
		{ 
			GenerateFlagsFrames( readBytes );
			return HdlcByte(); 
		}

	}
	
	GenerateFlagsFrames( readBytes );
	
	HdlcByte nonFlagByte = readBytes.back();
	return nonFlagByte;
	
}

void HdlcAnalyzer::GenerateFlagsFrames( vector<HdlcByte> readBytes ) 
{
	// 2) Generate the flag frames and return non-flag byte after the flags
	for( U32 i=0; i<readBytes.size()-1; ++i )
	{
		HdlcByte asyncByte = readBytes[ i ];
		
		Frame frame = CreateFrame( HDLC_FIELD_FLAG, asyncByte.startSample, asyncByte.endSample );
		
		if( i==readBytes.size() - 2 ) // start flag
		{
			frame.mData1 = HDLC_FLAG_START;
		}
		else // fill flag
		{
			frame.mData1 = HDLC_FLAG_FILL;
		}
		
		mResults->AddFrame( frame );
	}	
}

void HdlcAnalyzer::ProcessAddressField( HdlcByte byteAfterFlag )
{
	if( mAbortFrame )
	{
		return;
	}
	
	if( mSettings->mHdlcAddr == HDLC_BASIC_ADDRESS_FIELD ) 
	{
		Frame frame = CreateFrame( HDLC_FIELD_BASIC_ADDRESS, byteAfterFlag.startSample, 
								  byteAfterFlag.endSample, byteAfterFlag.value );
		mResults->AddFrame( frame );
		mCurrentFrameBytes.push_back( byteAfterFlag.value ); // append the address byte
		
		// Put a marker in the beggining of the HDLC frame
		mResults->AddMarker( byteAfterFlag.startSample, AnalyzerResults::Start, mSettings->mInputChannel );
		
	}
	else // HDLC_EXTENDED_ADDRESS_FIELD
	{
		int i=0;
		HdlcByte addressByte = byteAfterFlag;
		// Put a marker in the beggining of the HDLC frame
		mResults->AddMarker( addressByte.startSample, AnalyzerResults::Start, mSettings->mInputChannel );
		for( ; ; ) 
		{
			Frame frame = CreateFrame( HDLC_FIELD_EXTENDED_ADDRESS, addressByte.startSample, 
									  addressByte.endSample, addressByte.value, i++ );
			mResults->AddFrame( frame );
			mCurrentFrameBytes.push_back( addressByte.value );

			U8 lsbBit = addressByte.value & 0x01;
			if( !lsbBit ) // End of Extended Address Field?
			{
				return;
			}
			
			// Next address byte
			addressByte = ReadByte(); if( mAbortFrame ) { return; }
			
		}
	}
}

HdlcFrameType HdlcAnalyzer::GetFrameType( U8 value ) const
{
	if( value & 0x01 )
	{
		if( value & 0x02 )
		{
			return HDLC_U_FRAME;
		}
		else
		{
			return HDLC_S_FRAME;
		}
	}
	else // bit-0 = 0
	{
		return HDLC_I_FRAME;
	}
}

void HdlcAnalyzer::ProcessControlField()
{
	if( mAbortFrame )
	{
		return;
	}
	
	if( mSettings->mHdlcControl == HDLC_BASIC_CONTROL_FIELD ) // Basic Control Field of 1 byte
	{
		HdlcByte controlByte = ReadByte(); if( mAbortFrame ) { return; }
		
		HdlcFrameType frameType = GetFrameType( controlByte.value );
		
		Frame frame = CreateFrame( HDLC_FIELD_BASIC_CONTROL, controlByte.startSample, 
								   controlByte.endSample, controlByte.value, frameType );
		mResults->AddFrame( frame );
		mCurrentFrameBytes.push_back( controlByte.value ); // append control byte
	}
	else // Extended Control Field
	{
		U64 data1=0;
		U64 startSample=0, endSample=0;
		
		// Read first byte and check type of frame
		HdlcByte byte0 = ReadByte(); if( mAbortFrame ) { return; }
		HdlcFrameType frameType = GetFrameType( byte0.value );
		
		if( frameType != HDLC_U_FRAME )
		{
			mCurrentFrameIsSFrame = ( frameType == HDLC_S_FRAME );
			
			switch( mSettings->mHdlcControl )
			{
				case HDLC_EXTENDED_CONTROL_FIELD_MOD_128: 
				{
					HdlcByte byte1 = ReadByte(); if( mAbortFrame ) { return; }
					data1 |= byte0.value;
					data1 |= (byte1.value << 8);
					startSample = byte0.startSample;
					endSample = byte1.endSample;
					mCurrentFrameBytes.push_back( byte0.value ); // append control byte
					mCurrentFrameBytes.push_back( byte1.value ); // append control byte
					break;
				}
				case HDLC_EXTENDED_CONTROL_FIELD_MOD_32768: 
				{
					HdlcByte byte1 = ReadByte(); if( mAbortFrame ) { return; }
					HdlcByte byte2 = ReadByte(); if( mAbortFrame ) { return; }
					HdlcByte byte3 = ReadByte(); if( mAbortFrame ) { return; }
					
					data1 |= (byte0.value);
					data1 |= (byte1.value << 8);
					data1 |= (byte2.value << 16);
					data1 |= (byte3.value << 24);
					
					startSample = byte0.startSample;
					endSample = byte3.endSample;
					
					mCurrentFrameBytes.push_back( byte0.value ); // append control byte
					mCurrentFrameBytes.push_back( byte1.value ); // append control byte
					mCurrentFrameBytes.push_back( byte2.value ); // append control byte
					mCurrentFrameBytes.push_back( byte3.value ); // append control byte
					
					break;
				}
				case HDLC_EXTENDED_CONTROL_FIELD_MOD_2147483648:
				{
					HdlcByte byte1 = ReadByte(); if( mAbortFrame ) { return; }
					HdlcByte byte2 = ReadByte(); if( mAbortFrame ) { return; }
					HdlcByte byte3 = ReadByte(); if( mAbortFrame ) { return; }
					HdlcByte byte4 = ReadByte(); if( mAbortFrame ) { return; }
					HdlcByte byte5 = ReadByte(); if( mAbortFrame ) { return; }
					HdlcByte byte6 = ReadByte(); if( mAbortFrame ) { return; }
					HdlcByte byte7 = ReadByte(); if( mAbortFrame ) { return; }
					
					data1 = byte0.value | (byte1.value << 8) |
							(byte2.value << 16) | (byte3.value << 24) |
							(byte4.value << 32) | (byte5.value << 40) |
							(byte6.value << 48) | (byte7.value << 56);
					
					startSample = byte0.startSample;
					endSample = byte7.endSample;
					
					mCurrentFrameBytes.push_back( byte0.value ); // append control byte
					mCurrentFrameBytes.push_back( byte1.value ); // append control byte
					mCurrentFrameBytes.push_back( byte2.value ); // append control byte
					mCurrentFrameBytes.push_back( byte3.value ); // append control byte
					mCurrentFrameBytes.push_back( byte4.value ); // append control byte
					mCurrentFrameBytes.push_back( byte5.value ); // append control byte
					mCurrentFrameBytes.push_back( byte6.value ); // append control byte
					mCurrentFrameBytes.push_back( byte7.value ); // append control byte
					
					break;
				}
					
			}			
		}
		else // U-frame
		{
			data1 |= byte0.value;
			startSample = byte0.startSample;
			endSample = byte0.endSample;
			mCurrentFrameBytes.push_back( byte0.value ); // append control byte
		}
	
		Frame frame = CreateFrame( HDLC_FIELD_EXTENDED_CONTROL, startSample, endSample, data1, frameType );
		mResults->AddFrame( frame );
	
	}

}

vector<HdlcByte> HdlcAnalyzer::ReadProcessAndFcsField()
{
	vector<HdlcByte> infoAndFcs;
	for( ; ; )
	{
		HdlcByte asyncByte = ReadByte(); if( mAbortFrame ) { return infoAndFcs; }
		if( asyncByte.value == HDLC_FLAG_VALUE ) // End of frame found
		{
			Frame frame = CreateFrame( HDLC_FIELD_FLAG, asyncByte.startSample, 
										asyncByte.endSample, HDLC_FLAG_END );
			mResults->AddFrame( frame );
			break;
		}
		else  // information or fcs byte
		{
			infoAndFcs.push_back( asyncByte );
		}
	}
	
	return infoAndFcs;
	
}

void HdlcAnalyzer::ProcessInfoAndFcsField()
{
	if( mAbortFrame )
	{
		return;
	}
	
	vector<HdlcByte> informationAndFcs = ReadProcessAndFcsField();

	InfoAndFcsField( informationAndFcs );
}

void HdlcAnalyzer::InfoAndFcsField(const vector<HdlcByte> & informationAndFcs)
{
	vector<HdlcByte> information = informationAndFcs;
	vector<HdlcByte> hcs;
	vector<HdlcByte> fcs;
	
	if( !mAbortFrame ) 
	{
		// split information and fcs vector
		switch( mSettings->mHdlcFcs )
		{
			case HDLC_CRC8:
			{
				if( ( !information.empty() && ( !mSettings->mWithHcsField ) ) || 
					( information.size() >= 2 && ( mSettings->mWithHcsField ) ) )
				{
					if( mSettings->mWithHcsField )
					{
						hcs.push_back( information.front() );
						information.erase( information.begin() );
					}
					fcs.push_back( information.back() );
					information.pop_back();
				}
				break;
			}
			case HDLC_CRC16:
			{
				if( ( information.size() >= 2 && !mSettings->mWithHcsField ) || 
					( information.size() >= 4 && mSettings->mWithHcsField ) ||
					( information.size() >= 2 && mSettings->mWithHcsField && mCurrentFrameIsSFrame ) )
				{
					if( mSettings->mWithHcsField && !mCurrentFrameIsSFrame )
					{
						hcs.insert( hcs.end(), information.begin(), information.begin()+2 );
						information.erase( information.begin(), information.begin()+2 );
					}
					fcs.insert( fcs.end(), information.end()-2, information.end() );
					information.erase( information.end()-2, information.end() );
				}
				break;
			}
			case HDLC_CRC32:
			{
				if( ( information.size() >= 4 && ( !mSettings->mWithHcsField ) ) || 
					( information.size() >= 8 && ( mSettings->mWithHcsField ) ) )
				{
					if( mSettings->mWithHcsField )
					{
						hcs.insert( hcs.end(), information.begin(), information.begin()+4 );
						information.erase( information.begin(), information.begin()+4 );
					}
					fcs.insert( fcs.end(), information.end()-4, information.end() );
					information.erase( information.end()-4, information.end() );
				}
				break;
			}
		}
	}

	if( !mAbortFrame ) 
	{
		if( !hcs.empty() )
		{
			ProcessFcsField( hcs, HDLC_CRC_HCS );
		}
	}
	
	ProcessInformationField( information );
		
	if( !mAbortFrame ) 
	{
		if( !fcs.empty() )
		{
			ProcessFcsField( fcs, HDLC_CRC_FCS );
		}
	}
	
}

void HdlcAnalyzer::ProcessInformationField( const vector<HdlcByte> & information )
{
	for( U32 i=0; i<information.size(); ++i )
	{
		HdlcByte byte = information.at( i );
		Frame frame = CreateFrame( HDLC_FIELD_INFORMATION, byte.startSample, 
								   byte.endSample, byte.value, i );
		mResults->AddFrame( frame );
		mCurrentFrameBytes.push_back( byte.value ); // append control byte
	}
}

bool HdlcAnalyzer::CrcOk( const vector<U8> & remainder ) const
{
	for( U32 i=0; i < remainder.size(); ++i )
	{
		if( remainder.at( i ) != 0x00 )
		{
			return false;
		}
	}
	return true;
}

void HdlcAnalyzer::ProcessFcsField( const vector<HdlcByte> & fcs, HdlcCrcField crcFieldType )
{
	vector<U8> calculatedFcs;
	vector<U8> readFcs = HdlcBytesToVectorBytes( fcs );
	
	switch( mSettings->mHdlcFcs )
	{
		case HDLC_CRC8:
		{
			calculatedFcs = HdlcSimulationDataGenerator::Crc8( mCurrentFrameBytes, readFcs );
			break;
		}
		case HDLC_CRC16:
		{
			calculatedFcs = HdlcSimulationDataGenerator::Crc16( mCurrentFrameBytes, readFcs );
			break;
		}
		case HDLC_CRC32:
		{
			calculatedFcs = HdlcSimulationDataGenerator::Crc32( mCurrentFrameBytes, readFcs );
			break;
		}
	}
	
	// Add the HCS bytes (The FCS checks all the bytes between the flags)
	if( crcFieldType == HDLC_CRC_HCS )
	{
		for( U32 i=0; i < readFcs.size(); ++i )
		{
			mCurrentFrameBytes.push_back( readFcs.at( i ) );
		}
	}

	HdlcFieldType frameType = ( crcFieldType == HDLC_CRC_HCS ) ? HDLC_FIELD_HCS : HDLC_FIELD_FCS;
	Frame frame = CreateFrame( frameType, fcs.front().startSample, fcs.back().endSample, 
							  VectorToValue(readFcs), VectorToValue(calculatedFcs) );
	
	// Check if crc is ok (i.e. is equal to 0)
	if( !CrcOk( calculatedFcs ) ) // CRC ok
	{
		frame.mFlags = DISPLAY_AS_ERROR_FLAG;
	}
	
	mResults->AddFrame( frame );
	
	if( crcFieldType == HDLC_CRC_FCS )
	{
		// Put a marker in the end of the HDLC frame
		mResults->AddMarker( frame.mEndingSampleInclusive, AnalyzerResults::Stop, mSettings->mInputChannel );
	}
	
}

HdlcByte HdlcAnalyzer::ReadByte()
{
	return ( mSettings->mTransmissionMode == HDLC_TRANSMISSION_BYTE_ASYNC )
		   ? ByteAsyncReadByte() : BitSyncReadByte();
}

HdlcByte HdlcAnalyzer::ByteAsyncReadByte()
{
	HdlcByte ret = ByteAsyncReadByte_();
	
	// Check for escape character
	if( mReadingFrame && ( ret.value == HDLC_ESCAPE_SEQ_VALUE ) ) // escape byte read
	{
		Frame frame = CreateFrame( HDLC_ESCAPE_SEQ, ret.startSample, ret.endSample );
		mResults->AddFrame( frame );
		ret = ByteAsyncReadByte_();
		
		if( ret.value == HDLC_FLAG_VALUE ) // abort sequence = ESCAPE_BYTE + FLAG_BYTE (0x7D-0x7E)
		{
			// Create "Abort Frame" frame
			Frame frame = CreateFrame( HDLC_ABORT_SEQ, ret.startSample, ret.endSample );
			mResults->AddFrame( frame );
			mAbortFrame = true;
		}
		else
		{
			// Real data: with the bit-5 inverted (that's what we use for the crc)
			mCurrentFrameBytes.push_back( HdlcSimulationDataGenerator::Bit5Inv( ret.value ) );
		}
	}
	
	return ret;
}

HdlcByte HdlcAnalyzer::ByteAsyncReadByte_()
{
	// Line must be HIGH here
	if( mHdlc->GetBitState() == BIT_LOW )
	{
		mHdlc->AdvanceToNextEdge();
	}
	
	mHdlc->AdvanceToNextEdge(); // high->low transition (start bit)
	
	mHdlc->Advance( mSamplesInHalfPeriod * 0.5 );
	// BitState startBit = mHdlc->GetBitState(); // start bit position
	
	U64 byteStartSample = mHdlc->GetSampleNumber() + mSamplesInHalfPeriod * 0.5; 
	
	U64 byteValue2= 0;
	DataBuilder dbyte;
	dbyte.Reset( &byteValue2, AnalyzerEnums::LsbFirst, 8 );
	
	for( U32 i=0; i<8 ; ++i )
	{
		mHdlc->Advance( mSamplesInHalfPeriod );
		dbyte.AddBit( mHdlc->GetBitState() );
	}
	
	U8 byteValue = U8( byteValue2 );

	U64 byteEndSample = mHdlc->GetSampleNumber() + mSamplesInHalfPeriod * 0.5; 
 
	mHdlc->Advance( mSamplesInHalfPeriod );
	// BitState endBit = mHdlc->GetBitState(); // stop bit position

	HdlcByte asyncByte = { byteStartSample, byteEndSample, byteValue };
	
	return asyncByte;
}


//
///////////////////////////// Helper functions ///////////////////////////////////////////
//

// "Ctor" for the Frame class
Frame HdlcAnalyzer::CreateFrame( U8 mType, U64 mStartingSampleInclusive, U64 mEndingSampleInclusive,
								 U64 mData1, U64 mData2, U8 mFlags ) const
{
	Frame frame;
	frame.mStartingSampleInclusive = mStartingSampleInclusive;
	frame.mEndingSampleInclusive = mEndingSampleInclusive;
	frame.mType = mType;
	frame.mData1 = mData1;
	frame.mData2 = mData2;
	frame.mFlags = mFlags;
	return frame;
}

vector<U8> HdlcAnalyzer::HdlcBytesToVectorBytes( const vector<HdlcByte> & asyncBytes ) const
{
	vector<U8> ret;
	for( U32 i=0; i < asyncBytes.size(); ++i )
	{
		ret.push_back( asyncBytes[ i ].value );
	}
	return ret;
}

// TODO: check endianness
U64 HdlcAnalyzer::VectorToValue( const vector<U8> & v ) const
{
	U64 value=0;
	U32 j= 8 * ( v.size() - 1 );
	for( U32 i=0; i < v.size(); ++i )
	{
		value |= (v.at(i) << j);
		j-=8;
	}
	return value;
}

bool HdlcAnalyzer::NeedsRerun()
{
	return false;
}

U32 HdlcAnalyzer::GenerateSimulationData( U64 minimum_sample_index, U32 device_sample_rate, SimulationChannelDescriptor** simulation_channels )
{
	if( mSimulationInitilized == false )
	{
		mSimulationDataGenerator.Initialize( GetSimulationSampleRate(), mSettings.get() );
		mSimulationInitilized = true;
	}

	return mSimulationDataGenerator.GenerateSimulationData( minimum_sample_index, device_sample_rate, simulation_channels );
}

U32 HdlcAnalyzer::GetMinimumSampleRateHz()
{
	return mSettings->mBitRate * 4;
}

const char* HdlcAnalyzer::GetAnalyzerName() const
{
	return "HDLC";
}

const char* GetAnalyzerName()
{
	return "HDLC";
}

Analyzer* CreateAnalyzer()
{
	return new HdlcAnalyzer();
}

void DestroyAnalyzer( Analyzer* analyzer )
{
	delete analyzer;
}