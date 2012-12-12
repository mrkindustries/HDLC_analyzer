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
	
	// Parse flags until non-flag sequence
	BitSequence addressByte;
	if( mSettings->mTransmissionMode == HDLC_TRANSMISSION_BIT_SYNC ) 
	{
		addressByte = BitSyncProcessFlags();
	}
	else 
	{
		addressByte = ByteAsyncProcessFlags();
	}
		
	ProcessAddressField( addressByte );
	ProcessControlField();
	ProcessInfoAndFcsField();
}

// NOTE: We assume two 0 between flags, i.e.: ...0111111001111110...
// NOTE: For one-zero btw flags won't use BitSyncReadByte()...
// Interframe time fill: ISO/IEC 13239:2002(E) pag. 21
BitSequence HdlcAnalyzer::BitSyncProcessFlags()
{
	bool flagEncountered = false;
	vector<BitSequence> flags;
	for( ; ; )
	{
		BitSequence bs = BitSyncReadByte();
		//cerr << int( bs.value ) << endl;
		if( bs.value == HDLC_FLAG_VALUE )
		{
			flags.push_back(bs);
			flagEncountered = true;
		}
		else // non-flag
		{
			if( flagEncountered )
			{
				flags.push_back(bs);
				break;
			}
			else // non-flag byte before a byte-flag is ignored
			{
				mHdlc->AdvanceToNextEdge();
			}
		}
	}
	
	for(U32 i=0; i < flags.size()-1; ++i)
	{
		Frame frame = CreateFrame(HDLC_FIELD_FLAG, flags.at(i).startSample, 
								  flags.at(i).endSample, HDLC_FLAG_FILL);
		if( i == flags.size() - 2 )
		{
			frame.mData1 = HDLC_FLAG_START;
		}
		mResults->AddFrame( frame );
	}
	
	BitSequence firstAddressByte = flags.back();
	return firstAddressByte;
	
}

BitState HdlcAnalyzer::BitSyncReadBit()
{
	BitState ret = BIT_HIGH;
	mHdlc->Advance( mSamplesInHalfPeriod * 0.5 );
	BitState bit = mHdlc->GetBitState(); // sample bit
	if( bit != mPreviousBitState )
	{
		ret = BIT_LOW;
	}
	mPreviousBitState = bit;
	mHdlc->Advance( mSamplesInHalfPeriod * 0.5 );
	return ret;
}

BitSequence HdlcAnalyzer::BitSyncReadByte()
{
	U64 byteValue= 0;
	DataBuilder dbyte;
	dbyte.Reset( &byteValue, AnalyzerEnums::LsbFirst, 8 );
	U64 startSample = mHdlc->GetSampleNumber();
	for(U32 i=0; i<8 ; ++i)
	{
		BitState bit = BitSyncReadBit();
		// cerr << ((bit == BIT_HIGH) ? 1 : 0) << " ";
		dbyte.AddBit( bit );
	}
	// cerr << endl;
	U64 endSample = mHdlc->GetSampleNumber() - mSamplesInHalfPeriod;
	BitSequence bs = { startSample, endSample, U8( byteValue ) };
	return bs;
}

//
/////////////// ASYNC BYTE TRAMISSION ///////////////////////////////////////////////
//

// Interframe time fill: ISO/IEC 13239:2002(E) pag. 21
AsyncByte HdlcAnalyzer::ByteAsyncProcessFlags()
{
	bool flagEncountered = false;
	// 1) Read bytes until non-flag byte
	vector<AsyncByte> readBytes;
	for( ; ; )
	{
		AsyncByte asyncByte = ReadByte();
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
		AsyncByte asyncByte = readBytes[i];
		
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
	
	AsyncByte nonFlagByte = readBytes.back();
	return nonFlagByte;
	
}

void HdlcAnalyzer::ProcessAddressField( AsyncByte byteAfterFlag )
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
		AsyncByte addressByte = byteAfterFlag;
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
		AsyncByte controlByte = ReadByte();
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
				AsyncByte byte0 = ReadByte();
				AsyncByte byte1 = ReadByte();
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
				AsyncByte byte0 = ReadByte();
				AsyncByte byte1 = ReadByte();
				AsyncByte byte2 = ReadByte();
				AsyncByte byte3 = ReadByte();
				
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
				AsyncByte byte0 = ReadByte();
				AsyncByte byte1 = ReadByte();
				AsyncByte byte2 = ReadByte();
				AsyncByte byte3 = ReadByte();
				AsyncByte byte4 = ReadByte();
				AsyncByte byte5 = ReadByte();
				AsyncByte byte6 = ReadByte();
				AsyncByte byte7 = ReadByte();
				
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

vector<AsyncByte> HdlcAnalyzer::ReadProcessAndFcsField()
{
	vector<AsyncByte> infoAndFcs;
	bool endOfFrame=false;
	for( ; ; )
	{
		AsyncByte asyncByte = ReadByte();
		switch( asyncByte.value ) {
			case HDLC_FLAG_VALUE: // end of HDLC frame! 0x7E
			{
				Frame frame = CreateFrame( HDLC_FIELD_FLAG, asyncByte.startSample, 
										   asyncByte.endSample, HDLC_FLAG_END );
				mResults->AddFrame( frame );
				endOfFrame = true;
				break;
			}
			case HDLC_ESCAPE_SEQ_VALUE: // escape sequence 0x7D 
			{
				AsyncByte nextByte = ReadByte();
				nextByte.value = Bit5Inv( nextByte.value ); // invert bit 5
				infoAndFcs.push_back( nextByte );
				break;
			}
			default: // information or fcs byte
			{
				infoAndFcs.push_back( asyncByte );
			}
		}
		if( endOfFrame )
		{
			break;
		}
	}
	
	return infoAndFcs;
	
}

void HdlcAnalyzer::ProcessInfoAndFcsField()
{
	vector<AsyncByte> informationAndFcs = ReadProcessAndFcsField();
	InfoAndFcsField(informationAndFcs);
}

void HdlcAnalyzer::InfoAndFcsField(vector<AsyncByte> informationAndFcs)
{
	vector<AsyncByte> information = informationAndFcs;
	vector<AsyncByte> fcs;
	
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
	vector<U8> informationBytes = AsyncBytesToVectorBytes(information);
	mCurrentFrameBytes.insert(mCurrentFrameBytes.end(), informationBytes.begin(), informationBytes.end());
	
	ProcessFcsField(fcs);
	
}

void HdlcAnalyzer::ProcessInformationField(const vector<AsyncByte> & information)
{
	for(U32 i=0; i<information.size(); ++i)
	{
		AsyncByte byte = information.at(i);
		Frame frame = CreateFrame( HDLC_FIELD_INFORMATION, byte.startSample, 
								   byte.endSample, byte.value, i);
		mResults->AddFrame( frame );
		mCurrentFrameBytes.push_back(byte.value); // append control byte
	}
}

void HdlcAnalyzer::ProcessFcsField(const vector<AsyncByte> & fcs)
{
	vector<U8> calculatedFcs;
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

	vector<U8> readFcs = AsyncBytesToVectorBytes(fcs);
	
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

AsyncByte HdlcAnalyzer::ReadByte()
{
	return ( mSettings->mTransmissionMode == HDLC_TRANSMISSION_BYTE_ASYNC )
		   ? ByteAsyncReadByte() : BitSyncReadByte();
}

AsyncByte HdlcAnalyzer::ByteAsyncReadByte()
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

	AsyncByte asyncByte { byteStartSample, byteEndSample, byteValue };
	
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

vector<U8> HdlcAnalyzer::AsyncBytesToVectorBytes(const vector<AsyncByte> & asyncBytes) const
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

U8 HdlcAnalyzer::Bit5Inv( U8 value ) const 
{
	return value ^ 0x20;
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