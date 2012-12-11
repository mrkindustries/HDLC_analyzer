#include "HdlcAnalyzer.h"
#include "HdlcAnalyzerSettings.h"
#include <AnalyzerChannelData.h>
#include <AnalyzerHelpers.h>
#include <iostream>

using namespace std;

// TODO start stop delimiters indicators
// NOTE: Assuming big endian!
// TODO: Drop frame on error (...1111111...)!
// TODO: Poner "flag start" y "flag end" y "flag de fill" en los results, quedaría mas claro
// TODO: "ctor" para frame struct

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
}

void HdlcAnalyzer::WorkerThread()
{
	SetupAnalyzer();
	
	// Main loop
	for( ; ; )
	{
		if ( mSettings->mTransmissionMode == HDLC_TRANSMISSION_BIT_SYNC )
		{
			ProcessBitSync();
		}
		else // HDLC_TRANSMISSION_BYTE_ASYNC
		{
			ProcessByteAsync();
		}
		
		mResults->CommitResults();
		ReportProgress( mHdlc->GetSampleNumber() );
		CheckIfThreadShouldExit();

	}

}

//
/////////////// SYNC BIT TRAMISSION ///////////////////////////////////////////////
//

void HdlcAnalyzer::ProcessBitSync()
{
	// Parse flags until non-flag sequence
	for( ; ; ) 
	{
		mHdlc->AdvanceToNextEdge();
		if ( BitSyncProcessFlags() )
			break;
	}
		
	BitSyncProcessAddressField();
	BitSyncProcessControlField();
	BitSyncProcessInfoAndFcsField();
}

// NOTE: We assume two 0 between flags, i.e.: ...0111111001111110...
// TODO: support ...011111101111110...
// Interframe time fill: ISO/IEC 13239:2002(E) pag. 21
bool HdlcAnalyzer::BitSyncProcessFlags()
{
	bool flagEncountered = false;
	for( ; ; )
	{
		U64 flagStart = mHdlc->GetSampleNumber();
		mHdlc->AdvanceToNextEdge();
		U64 flagEnd = mHdlc->GetSampleNumber();
		if( flagEnd - flagStart == mSamplesIn7Bits ) // is it a flag?
		{
			flagEncountered = true;
		}
		else 
		{
			if (flagEncountered) // a flag has been processed already
			{
				return true;
			}
		}
	}
}

void HdlcAnalyzer::BitSyncProcessAddressField( )
{
	
}

void HdlcAnalyzer::BitSyncProcessControlField()
{
	
}

void HdlcAnalyzer::BitSyncProcessInfoAndFcsField()
{
	
}

//
/////////////// ASYNC BYTE TRAMISSION ///////////////////////////////////////////////
//

void HdlcAnalyzer::ProcessByteAsync()
{
	mCurrentFrameBytes.clear();
	
	// Parse flags until non-flag byte
	AsyncByte byteAfterFlag = ByteAsyncProcessFlags();
		
	ByteAsyncProcessAddressField( byteAfterFlag );
	ByteAsyncProcessControlField();
	ByteAsyncProcessInfoAndFcsField();
}

// Interframe time fill: ISO/IEC 13239:2002(E) pag. 21
AsyncByte HdlcAnalyzer::ByteAsyncProcessFlags()
{
	for( ; ; )
	{
		AsyncByte asyncByte = ReadByte();
		if( asyncByte.value != HDLC_FLAG_VALUE )
		{
			return asyncByte;
		}
		else // it's a flag byte
		{
			// Create and add a Frame of type Flag
			Frame frame;
			frame.mType = HDLC_FIELD_FLAG;	
			frame.mFlags = 0;
			frame.mData1 = 0;
			frame.mData2 = 0;
			frame.mStartingSampleInclusive = asyncByte.startSample;
			frame.mEndingSampleInclusive = asyncByte.endSample;
			mResults->AddFrame( frame );
		}
	}
}

void HdlcAnalyzer::ByteAsyncProcessAddressField( AsyncByte byteAfterFlag )
{
	if( mSettings->mHdlcAddr == HDLC_BASIC_ADDRESS_FIELD ) 
	{
		Frame frame;
		frame.mType = HDLC_FIELD_BASIC_ADDRESS;	
		frame.mData1 = byteAfterFlag.value;
		frame.mData2 = 0; 
		frame.mFlags = 0;
		frame.mStartingSampleInclusive = byteAfterFlag.startSample;
		frame.mEndingSampleInclusive = byteAfterFlag.endSample;
		mResults->AddFrame( frame );
		mCurrentFrameBytes.push_back( byteAfterFlag.value ); // append the address byte
	}
	else // HDLC_EXTENDED_ADDRESS_FIELD
	{
		int i=0;
		AsyncByte addressByte = byteAfterFlag;
		for( ; ; ) 
		{
			Frame frame;
			frame.mType = HDLC_FIELD_EXTENDED_ADDRESS;	
			frame.mData1 = addressByte.value;
			frame.mData2 = i++; 
			frame.mFlags = 0;
			frame.mStartingSampleInclusive = addressByte.startSample;
			frame.mEndingSampleInclusive = addressByte.endSample;
			
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
void HdlcAnalyzer::ByteAsyncProcessControlField()
{
	if ( mSettings->mHdlcControl == HDLC_BASIC_CONTROL_FIELD ) // Basic Control Field of 1 byte
	{
		AsyncByte controlByte = ReadByte();
		Frame frame;
		frame.mType = HDLC_FIELD_BASIC_CONTROL;	
		frame.mData1 = controlByte.value;
		frame.mFlags = 0;
		frame.mStartingSampleInclusive = controlByte.startSample;
		frame.mEndingSampleInclusive = controlByte.endSample;
		mResults->AddFrame( frame );
		mCurrentFrameBytes.push_back(controlByte.value); // append control byte
	}
	else // Extended Control Field
	{
		U64 data1=0;
		U64 startSample, endSample;
		Frame frame;
		frame.mType = HDLC_FIELD_EXTENDED_CONTROL;	
		frame.mFlags = 0;
				
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
	
		frame.mData1 = data1;
		frame.mStartingSampleInclusive = startSample;
		frame.mEndingSampleInclusive = endSample;
		mResults->AddFrame( frame );
	
	}

}

U8 HdlcAnalyzer::Bit5Inv( U8 value ) const 
{
	return value ^ 0x20;
}

vector<AsyncByte> HdlcAnalyzer::ByteAsyncReadProcessAndFcsField()
{
	vector<AsyncByte> infoAndFcs;
	bool endOfFrame=false;
	for( ; ; )
	{
		AsyncByte asyncByte = ReadByte();
		switch( asyncByte.value ) {
			case HDLC_FLAG_VALUE: // end of HDLC frame! 0x7E
			{
				Frame frame;
				frame.mType = HDLC_FIELD_FLAG;	
				frame.mData1 = 0;
				frame.mData2 = 0;
				frame.mFlags = 0;
				frame.mStartingSampleInclusive = asyncByte.startSample;
				frame.mEndingSampleInclusive = asyncByte.endSample;
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

void HdlcAnalyzer::ByteAsyncProcessInfoAndFcsField()
{
	vector<AsyncByte> informationAndFcs = ByteAsyncReadProcessAndFcsField();
	ByteAsyncInfoAndFcsField(informationAndFcs);
}

void HdlcAnalyzer::ByteAsyncInfoAndFcsField(vector<AsyncByte> informationAndFcs)
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
	
	ByteAsyncProcessInformationField(information);
	
	// Add information bytes to the frame bytes
	vector<U8> informationBytes = AsyncBytesToVectorBytes(information);
	mCurrentFrameBytes.insert(mCurrentFrameBytes.end(), informationBytes.begin(), informationBytes.end());
	
	cerr << fcs.size() << " --- " << information.size() << endl;
	
	ByteAsyncProcessFcsField(fcs);
	
}

void HdlcAnalyzer::ByteAsyncProcessInformationField(const vector<AsyncByte> & information)
{
	for(U32 i=0; i<information.size(); ++i)
	{
		AsyncByte byte = information.at(i);
		Frame frame;
		frame.mType = HDLC_FIELD_INFORMATION;	
		frame.mData1 = byte.value;
		frame.mData2 = i;
		frame.mFlags = 0;
		frame.mStartingSampleInclusive = byte.startSample;
		frame.mEndingSampleInclusive = byte.endSample;
		mResults->AddFrame( frame );
		mCurrentFrameBytes.push_back(byte.value); // append control byte
	}
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

void HdlcAnalyzer::ByteAsyncProcessFcsField(const vector<AsyncByte> & fcs)
{
	Frame frame;
	frame.mType = HDLC_FIELD_FCS;	
	frame.mFlags = 0;
	
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
	
	frame.mData1 = VectorToValue(readFcs); // read Fcs
	frame.mData2 = VectorToValue(calculatedFcs); // calculated Fcs
	frame.mStartingSampleInclusive = fcs.front().startSample;
	frame.mEndingSampleInclusive = fcs.back().endSample;
	
	if ( calculatedFcs != readFcs ) // CRC ok
	{
		frame.mFlags = DISPLAY_AS_ERROR_FLAG;
	}
	
	mResults->AddFrame( frame );
	
}

AsyncByte HdlcAnalyzer::ReadByte()
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

/////////////////////////////////////////////////////////////////////////////

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