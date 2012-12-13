#include "HdlcAnalyzer.h"
#include "HdlcAnalyzerSettings.h"
#include <AnalyzerChannelData.h>
#include <AnalyzerHelpers.h>
#include <iostream>

using namespace std;

// NOTE: Assuming big endian!
// TODO: Drop frame on error (...1111111...)!
// TODO: Show escape bytes?
// TODO: support ...011111101111110... (as a setting? check-box?)
// TODO: escape + abort code?
// TODO: abort procedure in the process function
// TODO: Transmission of 0xFF in byte async is possible?

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

	double halfPeriod = (1.0 / double( mSettings->mBitRate * 2 )) * 1000000.0;
	mSampleRateHz = this->GetSampleRate();
	mSamplesInHalfPeriod = U32( ( mSampleRateHz * halfPeriod ) / 1000000.0 );
	mSamplesIn7Bits = mSamplesInHalfPeriod * 7;	
	
	mPreviousBitState = mHdlc->GetBitState();
	mConsecutiveOnes = 0;
	mReadingFrame = false;
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
	mReadingFrame = false;	
	
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
			
			mHdlc->Advance( mSamplesInHalfPeriod * 0.5 );
			mPreviousBitState = mHdlc->GetBitState();
			mHdlc->Advance( mSamplesInHalfPeriod * 0.5 );
			
			flagEncountered = true;
		}
		else // non-flag
		{
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
	
	for(U32 i=0; i < flags.size(); ++i)
	{
		Frame frame = CreateFrame(HDLC_FIELD_FLAG, flags.at(i).startSample, 
								  flags.at(i).endSample, HDLC_FLAG_FILL);
		if( i == flags.size() - 1 )
		{
			frame.mData1 = HDLC_FLAG_START;
		}
		mResults->AddFrame( frame );
	}
	
}

/*
// TODO: debug...
HdlcByte HdlcAnalyzer::BitSyncProcessFirstByteAfterFlag( HdlcByte firstAddressByte )
{
	
	U64 byteValue= 0;
	DataBuilder dbyte;
	dbyte.Reset( &byteValue, AnalyzerEnums::LsbFirst, 8 );
	
	U64 value2 = firstAddressByte.value;
	BitExtractor bit_extractor( value2, AnalyzerEnums::LsbFirst, 8 );
	U32 consecutiveOnes = 1;
	BitState previousBit = BIT_LOW;
	bool fiveBitsInARow = false;
	
	for( U32 i=0; i<8; ++i )
	{
		BitState bit = BIT_LOW;
		if (consecutiveOnes == 5) 
		{
			fiveBitsInARow = true;
			//bit_extractor.GetNextBit();
		}
		else
		{
			//bit = bit_extractor.GetNextBit();
		}
		
		if( bit == BIT_HIGH && i>0 )
		{
			if ( bit == previousBit ) 
			{
				consecutiveOnes++;
			}
			else 
			{
				consecutiveOnes = 1;
			}
		}
		
		if( consecutiveOnes != 5 )
		{
			dbyte.AddBit( bit );
		}
		previousBit = bit;
	}
	
	//cerr << "#### " << int(firstAddressByte.value) << " | " << byteValue << endl;
	
	HdlcByte retByte = firstAddressByte;
	
	if( fiveBitsInARow ) 
	{
		// Read Remaining bit
		dbyte.AddBit( BitSyncReadBit() );
		retByte.value = U8( byteValue );
		retByte.endSample += mSamplesInHalfPeriod * 2;
	}
	
	return retByte;
	
}
*/

BitState HdlcAnalyzer::BitSyncReadBit()
{
	mHdlc->Advance( mSamplesInHalfPeriod * 0.5 );
	
	BitState bit = mHdlc->GetBitState();
	BitState ret;
	if( bit == BIT_HIGH )
	{
		if( bit == mPreviousBitState )
		{
			mConsecutiveOnes++;
			if( mReadingFrame && mConsecutiveOnes == 5)
			{
				mHdlc->Advance( mSamplesInHalfPeriod );
				mConsecutiveOnes = 0;
				mPreviousBitState = BIT_LOW;
			}
			else 
			{
				mPreviousBitState = bit;
			}
			ret = BIT_HIGH;
		}
		else 
		{
			ret = BIT_LOW;
			mConsecutiveOnes = 0;
			mPreviousBitState = bit;
		}
	}
	else
	{
		if( bit == mPreviousBitState )
		{
			ret = BIT_HIGH;
		}
		else
		{
			ret = BIT_LOW;
		}
		mPreviousBitState = bit;
	}
	
	mHdlc->Advance( mSamplesInHalfPeriod * 0.5 );
	return ret;
}

bool HdlcAnalyzer::FlagComing()
{
	return !mHdlc->WouldAdvancingCauseTransition( mSamplesIn7Bits-1 ) &&
		   mHdlc->WouldAdvancingCauseTransition( mSamplesIn7Bits );
}

HdlcByte HdlcAnalyzer::BitSyncReadByte()
{
	
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
	for(U32 i=0; i<8 ; ++i)
	{
		BitState bit = BitSyncReadBit();
		cerr << ((bit == BIT_HIGH) ? 1 : 0) << " ";
		dbyte.AddBit( bit );
	}
	cerr << endl;
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
		else if ( asyncByte.value == HDLC_FLAG_VALUE ) 
		{
			readBytes.push_back( asyncByte );
			flagEncountered = true;
		}
	}
	
	// 2) Generate the flag frames and return non-flag byte after the flags
	for( U32 i=0; i<readBytes.size()-1; ++i )
	{
		HdlcByte asyncByte = readBytes[i];
		
		Frame frame = CreateFrame( HDLC_FIELD_FLAG, asyncByte.startSample, asyncByte.endSample);
		
		if (i==readBytes.size()-2) // start flag
		{
			frame.mData1 = HDLC_FLAG_START;
		}
		else // fill flag
		{
			frame.mData1 = HDLC_FLAG_FILL;
		}
		
		mResults->AddFrame( frame );
	}
	
	HdlcByte nonFlagByte = readBytes.back();
	return nonFlagByte;
	
}

void HdlcAnalyzer::ProcessAddressField( HdlcByte byteAfterFlag )
{
	
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
		for( ; ; ) 
		{
			Frame frame = CreateFrame( HDLC_FIELD_EXTENDED_ADDRESS, addressByte.startSample, 
									  addressByte.endSample, addressByte.value, i++ );
			mResults->AddFrame( frame );
			mCurrentFrameBytes.push_back(addressByte.value);

			U8 lsbBit = byteAfterFlag.value & 0x01;
			if( !lsbBit ) // End of Extended Address Field?
			{
				return;
			}
			
			// Next address byte
			addressByte = ReadByte();
			
		}
	}
}

// NOTE: Little-endian (first byte is the LSB byte)
void HdlcAnalyzer::ProcessControlField()
{
	if ( mSettings->mHdlcControl == HDLC_BASIC_CONTROL_FIELD ) // Basic Control Field of 1 byte
	{
		HdlcByte controlByte = ReadByte();
		Frame frame = CreateFrame( HDLC_FIELD_BASIC_CONTROL, controlByte.startSample, 
								   controlByte.endSample, controlByte.value);
		mResults->AddFrame( frame );
		mCurrentFrameBytes.push_back(controlByte.value); // append control byte
	}
	else // Extended Control Field
	{
		U64 data1=0;
		U64 startSample, endSample;
				
		switch( mSettings->mHdlcControl )
		{
			case HDLC_EXTENDED_CONTROL_FIELD_MOD_128: 
			{
				HdlcByte byte0 = ReadByte();
				HdlcByte byte1 = ReadByte();
				data1 |= byte0.value;
				data1 |= (byte1.value << 8);
				startSample = byte0.startSample;
				endSample = byte1.endSample;
				mCurrentFrameBytes.push_back(byte0.value); // append control byte
				mCurrentFrameBytes.push_back(byte1.value); // append control byte
				break;
			}
			case HDLC_EXTENDED_CONTROL_FIELD_MOD_32768: 
			{
				HdlcByte byte0 = ReadByte();
				HdlcByte byte1 = ReadByte();
				HdlcByte byte2 = ReadByte();
				HdlcByte byte3 = ReadByte();
				
				data1 |= (byte0.value);
				data1 |= (byte1.value << 8);
				data1 |= (byte2.value << 16);
				data1 |= (byte3.value << 24);
				
				startSample = byte0.startSample;
				endSample = byte3.endSample;
				
				mCurrentFrameBytes.push_back(byte0.value); // append control byte
				mCurrentFrameBytes.push_back(byte1.value); // append control byte
				mCurrentFrameBytes.push_back(byte2.value); // append control byte
				mCurrentFrameBytes.push_back(byte3.value); // append control byte
				
				break;
			}
			case HDLC_EXTENDED_CONTROL_FIELD_MOD_2147483648:
			{
				HdlcByte byte0 = ReadByte();
				HdlcByte byte1 = ReadByte();
				HdlcByte byte2 = ReadByte();
				HdlcByte byte3 = ReadByte();
				HdlcByte byte4 = ReadByte();
				HdlcByte byte5 = ReadByte();
				HdlcByte byte6 = ReadByte();
				HdlcByte byte7 = ReadByte();
				
				data1 = byte0.value | (byte1.value << 8) |
						(byte2.value << 16) | (byte3.value << 24) |
						(byte4.value << 32) | (byte5.value << 40) |
						(byte6.value << 48) | (byte7.value << 56);
				
				startSample = byte0.startSample;
				endSample = byte7.endSample;
				
				mCurrentFrameBytes.push_back(byte0.value); // append control byte
				mCurrentFrameBytes.push_back(byte1.value); // append control byte
				mCurrentFrameBytes.push_back(byte2.value); // append control byte
				mCurrentFrameBytes.push_back(byte3.value); // append control byte
				mCurrentFrameBytes.push_back(byte4.value); // append control byte
				mCurrentFrameBytes.push_back(byte5.value); // append control byte
				mCurrentFrameBytes.push_back(byte6.value); // append control byte
				mCurrentFrameBytes.push_back(byte7.value); // append control byte
				
				break;
			}
				
		}			
	
		Frame frame = CreateFrame(HDLC_FIELD_EXTENDED_CONTROL, startSample, endSample, data1);
		mResults->AddFrame( frame );
	
	}

}

vector<HdlcByte> HdlcAnalyzer::ReadProcessAndFcsField()
{
	vector<HdlcByte> infoAndFcs;
	bool endOfFrame=false;
	for( ; ; )
	{
		HdlcByte asyncByte = ReadByte();
		if( asyncByte.value == HDLC_FLAG_VALUE ) 
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
	vector<HdlcByte> informationAndFcs = ReadProcessAndFcsField();
	InfoAndFcsField(informationAndFcs);
}

void HdlcAnalyzer::InfoAndFcsField(vector<HdlcByte> informationAndFcs)
{
	vector<HdlcByte> information = informationAndFcs;
	vector<HdlcByte> fcs;
	
	// split information and fcs vector
	switch( mSettings->mHdlcFcs )
	{
		case HDLC_CRC8:
		{
			fcs.push_back(information.back());
			information.pop_back();
			break;
		}
		case HDLC_CRC16:
		{
			fcs.insert(fcs.end(), information.end()-2, information.end());
			information.erase(information.end()-2, information.end());
			break;
		}
		case HDLC_CRC32:
		{
			fcs.insert(fcs.end(), information.end()-4, information.end());
			information.erase(information.end()-4, information.end());
			break;
		}
	}
	
	ProcessInformationField(information);
	
	// Add information bytes to the frame bytes
	vector<U8> informationBytes = HdlcBytesToVectorBytes(information);
	mCurrentFrameBytes.insert(mCurrentFrameBytes.end(), informationBytes.begin(), informationBytes.end());
	
	ProcessFcsField(fcs);
	
}

void HdlcAnalyzer::ProcessInformationField(const vector<HdlcByte> & information)
{
	for(U32 i=0; i<information.size(); ++i)
	{
		HdlcByte byte = information.at(i);
		Frame frame = CreateFrame( HDLC_FIELD_INFORMATION, byte.startSample, 
								   byte.endSample, byte.value, i);
		mResults->AddFrame( frame );
		mCurrentFrameBytes.push_back(byte.value); // append control byte
	}
}

void HdlcAnalyzer::RemoveEscapesAndInvert()
{
	// TODO: process mCurrentFrameBytes removing escapes and inverting bit 5
}

void HdlcAnalyzer::ProcessFcsField(const vector<HdlcByte> & fcs)
{
	vector<U8> calculatedFcs;
	
	if( mSettings->mTransmissionMode == HDLC_TRANSMISSION_BYTE_ASYNC ) 
	{
		RemoveEscapesAndInvert();
	}
	
	switch( mSettings->mHdlcFcs )
	{
		case HDLC_CRC8:
		{
			calculatedFcs = HdlcSimulationDataGenerator::Crc8( mCurrentFrameBytes );
			break;
		}
		case HDLC_CRC16:
		{
			calculatedFcs = HdlcSimulationDataGenerator::Crc16( mCurrentFrameBytes );
			break;
		}
		case HDLC_CRC32:
		{
			calculatedFcs = HdlcSimulationDataGenerator::Crc32( mCurrentFrameBytes );
			break;
		}
	}

	vector<U8> readFcs = HdlcBytesToVectorBytes(fcs);
	
	Frame frame = CreateFrame(HDLC_FIELD_FCS, fcs.front().startSample, fcs.back().endSample, 
							  VectorToValue(readFcs), VectorToValue(calculatedFcs) );
	
	if ( calculatedFcs != readFcs ) // CRC ok
	{
		frame.mFlags = DISPLAY_AS_ERROR_FLAG;
	}
	
	mResults->AddFrame( frame );
	// Put a marker in the beggining of the HDLC frame
	mResults->AddMarker( frame.mEndingSampleInclusive, AnalyzerResults::Stop, mSettings->mInputChannel );

	
}

HdlcByte HdlcAnalyzer::ReadByte()
{
	return ( mSettings->mTransmissionMode == HDLC_TRANSMISSION_BYTE_ASYNC )
		   ? ByteAsyncReadByte() : BitSyncReadByte();
}

HdlcByte HdlcAnalyzer::ByteAsyncReadByte()
{
	HdlcByte ret = ByteAsyncReadByte_();
	if( ret.value == HDLC_ESCAPE_SEQ_VALUE ) // escape byte read
	{
		Frame frame = CreateFrame(HDLC_ESCAPE_SEQ, ret.startSample, ret.endSample );
		mResults->AddFrame( frame );
		mCurrentFrameBytes.push_back( ret.value );
		// read next byte and invert bit 5
		ret = ByteAsyncReadByte_();
	}
	return ret;
}

HdlcByte HdlcAnalyzer::ByteAsyncReadByte_()
{
	// Line must be HIGH here
	if ( mHdlc->GetBitState() == BIT_LOW )
	{
		mHdlc->AdvanceToNextEdge();
	}
	
	mHdlc->AdvanceToNextEdge(); // high->low transition (start bit)
	
	mHdlc->Advance( mSamplesInHalfPeriod * 0.5 );
	BitState startBit = mHdlc->GetBitState(); // start bit position
	
	U64 byteStartSample = mHdlc->GetSampleNumber() + mSamplesInHalfPeriod * 0.5; 
	
	U64 byteValue2= 0;
	DataBuilder dbyte;
	dbyte.Reset( &byteValue2, AnalyzerEnums::LsbFirst, 8 );
	
	for(U32 i=0; i<8 ; ++i)
	{
		mHdlc->Advance( mSamplesInHalfPeriod );
		dbyte.AddBit( mHdlc->GetBitState() );
	}
	
	U8 byteValue = U8( byteValue2 );

	U64 byteEndSample = mHdlc->GetSampleNumber() + mSamplesInHalfPeriod * 0.5; 
 
	mHdlc->Advance( mSamplesInHalfPeriod );
	BitState endBit = mHdlc->GetBitState(); // stop bit position

	HdlcByte asyncByte { byteStartSample, byteEndSample, byteValue };
	
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

vector<U8> HdlcAnalyzer::HdlcBytesToVectorBytes(const vector<HdlcByte> & asyncBytes) const
{
	vector<U8> ret;
	for(U32 i=0; i < asyncBytes.size(); ++i)
	{
		ret.push_back(asyncBytes[i].value);
	}
	return ret;
}

// TODO: check endianness
U64 HdlcAnalyzer::VectorToValue(const vector<U8> & v) const
{
	U64 value=0;
	U32 j= 8 * (v.size()-1);
	for(U32 i=0; i < v.size(); ++i)
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