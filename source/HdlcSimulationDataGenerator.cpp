#include "HdlcSimulationDataGenerator.h"
#include "HdlcAnalyzerSettings.h"
#include <AnalyzerHelpers.h>
#include <cstdlib>

HdlcSimulationDataGenerator::HdlcSimulationDataGenerator()
{
}

HdlcSimulationDataGenerator::~HdlcSimulationDataGenerator()
{
}

void HdlcSimulationDataGenerator::Initialize( U32 simulation_sample_rate, HdlcAnalyzerSettings* settings )
{
	mSimulationSampleRateHz = simulation_sample_rate;
	mSettings = settings;

	mHdlcSimulationData.SetChannel( mSettings->mInputChannel );
	mHdlcSimulationData.SetSampleRate( simulation_sample_rate );
	mHdlcSimulationData.SetInitialBitState( BIT_LOW );
	
	double halfPeriod = (1.0 / double( mSettings->mBitRate * 2 )) * 1000000.0; 	// half period in useconds.
	mSamplesInHalfPeriod = USecsToSamples( halfPeriod );		 				// number of samples in a half period.
	
	mHdlcSimulationData.Advance( mSamplesInHalfPeriod * 8 );	 				// Advance 4 periods
	
}

U64 HdlcSimulationDataGenerator::USecsToSamples( U64 us ) const
{
	return ( mSimulationSampleRateHz * us ) / 1000000;
}

U32 HdlcSimulationDataGenerator::GenerateSimulationData( U64 largest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channel )
{
	U64 adjusted_largest_sample_requested = AnalyzerHelpers::AdjustSimulationTargetSample( largest_sample_requested, sample_rate, mSimulationSampleRateHz );

	srand( time( NULL ) );
	
	U8 value=0;
	U16 size=0;
	U8 informationValue=0;
	U64 addressBytes=1;
	U8 controlValue=0;
	U64 idxFrames=0;
	HdlcFrameType frameTypes[3] = { HDLC_I_FRAME, HDLC_S_FRAME, HDLC_U_FRAME };
	
	while( mHdlcSimulationData.GetCurrentSampleNumber() < adjusted_largest_sample_requested )
	{
		// Two consecutive flags
		CreateFlag();
		CreateFlag();
		
		vector<U8> address = GenAddressField(mSettings->mHdlcAddr, addressBytes, 0xFF);
		vector<U8> control = GenControlField(frameTypes[idxFrames++%3], mSettings->mHdlcControl, 0x0F/*controlValue++*/);
		vector<U8> information = GenInformationField(/*size++*/ 2, 0x0F/*informationValue++*/);
		
		CreateHDLCFrame( address, control, information );
		
		// Two consecutive flags
		CreateFlag();
		CreateFlag();
	}
	
	*simulation_channel = &mHdlcSimulationData;
	return 1;
}

void HdlcSimulationDataGenerator::CreateFlag() 
{
	if( mSettings->mTransmissionMode == HDLC_TRANSMISSION_BIT_SYNC ) 
	{
		CreateFlagBitSeq();
	}
	else // HDLC_TRANSMISSION_BYTE_ASYNC
	{
		CreateAsyncByte( HDLC_FLAG_VALUE );
	}
}
		

vector<U8> HdlcSimulationDataGenerator::GenAddressField( HdlcAddressType addressType, 
														 U64 addressBytes, 
														 U8 value ) const
{
	vector<U8> addrRet;
	if( addressType == HDLC_BASIC_ADDRESS_FIELD ) 
	{
		addrRet.push_back(value);
	}
	else // addressType == HDLC_EXTENDED_ADDRESS_FIELD
	{
		for( U32 i=0; i<addressBytes; ++i ) 
		{
			U8 mask = ( i == addressBytes - 1 ) ? 0x00 : 0x01; // EA bit (Lsb is set to 1 to extend the address)
			addrRet.push_back( value | mask );
		}
	}
	return addrRet;
}
	
// TODO read ISO/IEC 13239:2002(E) pag. 26
vector<U8> HdlcSimulationDataGenerator::GenControlField( HdlcFrameType frameType, 
														 HdlcControlType controlType, 
														 U8 value ) const
{
	vector<U8> controlRet;
	switch( frameType ) 
	{
		case HDLC_I_FRAME: 
		case HDLC_S_FRAME:
		{
			// first byte
			U8 ctrl = value | U8(frameType);
			controlRet.push_back(ctrl);
			switch( controlType ) 
			{
				case HDLC_EXTENDED_CONTROL_FIELD_MOD_128:
					controlRet.push_back(value); // second byte
					break;
				case HDLC_EXTENDED_CONTROL_FIELD_MOD_32768:
					controlRet.push_back(value); // second byte
					controlRet.push_back(value); // third byte
					controlRet.push_back(value); // fourth byte
					break;
				case HDLC_EXTENDED_CONTROL_FIELD_MOD_2147483648: 
					controlRet.push_back(value); // second byte
					controlRet.push_back(value); // third byte
					controlRet.push_back(value); // fourth byte
					controlRet.push_back(value); // fifth byte
					controlRet.push_back(value); // sixth byte
					controlRet.push_back(value); // seventh byte
					controlRet.push_back(value); // eighth byte
					break;
			}
			break;
		}
		case HDLC_U_FRAME: // U frames are always of 8 bits 
		{
			U8 ctrl = value | U8(HDLC_U_FRAME);
			controlRet.push_back(ctrl);
			break;
		}
	}
	return controlRet;
}
	
vector<U8> HdlcSimulationDataGenerator::GenInformationField( U16 size, U8 value ) const
{
	vector<U8> informationRet(size, value);
	return informationRet;
}

void HdlcSimulationDataGenerator::CreateHDLCFrame( const vector<U8> & address, const vector<U8> & control, 
												   const vector<U8> & information )
{
	vector<U8> allFields; 
	
	allFields.insert(allFields.end(), address.begin(), address.end());
	allFields.insert(allFields.end(), control.begin(), control.end());
	allFields.insert(allFields.end(), information.begin(), information.end());
	
	// Calculate the crc of the address, control and data fields
	vector<U8> fcs = GenFcs(mSettings->mHdlcFcs, allFields);
	allFields.insert(allFields.end(), fcs.begin(), fcs.end());
	
	// Transmit the frame in bit-sync or byte-async
	if( mSettings->mTransmissionMode == HDLC_TRANSMISSION_BIT_SYNC )
	{
		TransmitBitSync(allFields);
	}
	else
	{
		TransmitByteAsync(allFields);
	}
}

vector<U8> HdlcSimulationDataGenerator::GenFcs( HdlcFcsType fcsType, const vector<U8> & stream ) const
{
	vector<U8> crcRet;
	switch(fcsType) 
	{
		case HDLC_CRC8: 
			crcRet = Crc8(stream);
			break;
		case HDLC_CRC16:
			crcRet = Crc16(stream);
			break;
		case HDLC_CRC32:
			crcRet = Crc32(stream);
			break;
	}
	return crcRet;
}

void HdlcSimulationDataGenerator::TransmitBitSync( const vector<U8> & stream ) 
{
	// Opening flag
	CreateFlagBitSeq();
	
	U8 consecutiveOnes = 0;
	BitState previousBit = BIT_LOW;
	// For each byte of the stream
	for( U32 s=0; s<stream.size(); ++s) 
	{
		// For each bit of the byte stream
		BitExtractor bit_extractor( stream[s], AnalyzerEnums::LsbFirst, 8 );
		for( U32 i=0; i<8; ++i )
		{
			BitState bit = bit_extractor.GetNextBit();
			
			if( consecutiveOnes == 4 ) // if five 1s in a row, then insert a 0 and continue
			{
				CreateSyncBit( BIT_LOW );
				consecutiveOnes = 0;
			}

			if( bit == BIT_HIGH ) 
			{
				if( previousBit == BIT_HIGH ) 
				{
					consecutiveOnes++;
				}
				else 
				{
					consecutiveOnes = 0;
				}
				
			}
			else // bit low
			{
				consecutiveOnes = 0;
			}
			
			CreateSyncBit( bit );
			previousBit = bit;
		}
	}
	
	// Closing flag
	CreateFlagBitSeq();

}

void HdlcSimulationDataGenerator::CreateFlagBitSeq() 
{
	mHdlcSimulationData.Transition();
	mHdlcSimulationData.Advance( mSamplesInHalfPeriod * 7 );
	mHdlcSimulationData.Transition();
	mHdlcSimulationData.Advance( mSamplesInHalfPeriod );
}

// Maps the bit to the signal using NRZI 
void HdlcSimulationDataGenerator::CreateSyncBit( BitState bitState ) 
{
	if( bitState == BIT_LOW ) // BIT_LOW == transition, BIT_HIGH == no transition
	{
		mHdlcSimulationData.Transition();
	}
	mHdlcSimulationData.Advance( mSamplesInHalfPeriod );
}

void HdlcSimulationDataGenerator::TransmitByteAsync( const vector<U8> & stream ) 
{
	// Opening flag
	CreateAsyncByte( HDLC_FLAG_VALUE );
	
	for( U32 i=0; i < stream.size(); ++i )
	{
		const U8 byte = stream[i];
		switch ( byte ) 
		{
			case HDLC_FLAG_VALUE: // 0x7E
				CreateAsyncByte( HDLC_ESCAPE_SEQ_VALUE );			// 7D escape
				CreateAsyncByte( Bit5Inv(HDLC_FLAG_VALUE) );		// 5E
				break;
			case HDLC_ESCAPE_SEQ_VALUE: // 0x7D
				CreateAsyncByte( HDLC_ESCAPE_SEQ_VALUE );			// 7D escape
				CreateAsyncByte( Bit5Inv(HDLC_ESCAPE_SEQ_VALUE) );	// 5D
				break;
			default:
				CreateAsyncByte( byte );							// normal byte
		}
		
		// Fill between bytes (0 to 8 bits of value 1)
		AsyncByteFill( rand() % 8 );
	}
	
	// Closing flag
	CreateAsyncByte( HDLC_FLAG_VALUE );
	
}

void HdlcSimulationDataGenerator::AsyncByteFill( U32 N )
{
	// 0) If the line is not high we must set it high
	if( mHdlcSimulationData.GetCurrentBitState() == BIT_LOW ) 
	{
		mHdlcSimulationData.Transition();
	}
	// Fill N high periods
	mHdlcSimulationData.Advance( mSamplesInHalfPeriod * N );
}

// ISO/IEC 13239:2002(E) pag. 17
void HdlcSimulationDataGenerator::CreateAsyncByte( U8 byte ) 
{
	
	// 0) If the line is not high we must set it high
	if( mHdlcSimulationData.GetCurrentBitState() == BIT_LOW ) 
	{
		mHdlcSimulationData.Transition();
		mHdlcSimulationData.Advance( mSamplesInHalfPeriod );
	}
	
	// 1) Start bit (BIT_HIGH -> BIT_LOW)
	mHdlcSimulationData.TransitionIfNeeded( BIT_LOW );
	mHdlcSimulationData.Advance( mSamplesInHalfPeriod );
		
	// 2) Transmit byte
	BitExtractor bit_extractor( byte, AnalyzerEnums::LsbFirst, 8 );
	for( U32 i=0; i < 8; ++i )
	{
		BitState bit = bit_extractor.GetNextBit();
		mHdlcSimulationData.TransitionIfNeeded( bit );
		mHdlcSimulationData.Advance( mSamplesInHalfPeriod );
	}
	
	// 3) Stop bit (BIT_LOW -> BIT_HIGH)
	mHdlcSimulationData.TransitionIfNeeded( BIT_HIGH );
	mHdlcSimulationData.Advance( mSamplesInHalfPeriod );
	
}

//
////////////////////// Static functions /////////////////////////////////////////////////////
//

vector<U8> HdlcSimulationDataGenerator::Crc8( const vector<U8> & stream )
{
	vector<U8> crc8Ret(1, 0);
	// TODO
	return crc8Ret;
}

vector<U8> HdlcSimulationDataGenerator::Crc16( const vector<U8> & stream )
{
	vector<U8> crc16Ret(2, 0);
	// TODO
	return crc16Ret;
}

vector<U8> HdlcSimulationDataGenerator::Crc32( const vector<U8> & stream )
{
	vector<U8> crc32Ret(4, 0);
	// TODO
	return crc32Ret;

}

U8 HdlcSimulationDataGenerator::Bit5Inv( U8 value ) 
{
	return value ^ 0x20;
}