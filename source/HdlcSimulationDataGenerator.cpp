#include "HdlcSimulationDataGenerator.h"
#include "HdlcAnalyzerSettings.h"

#include <AnalyzerHelpers.h>

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
	
	double halfPeriod = 1.0 / double( mSettings->mBitRate * 2 ); // half period in seconds.
	mSamplesInHalfPeriod = SecondsToSamples( halfPeriod );		 // number of samples in a half period.
	mHdlcSimulationData.Advance( mSamplesInHalfPeriod * 8 );	 // Advance 4 periods
	
	mPfBitValue = true;
	
}

U64 HdlcSimulationDataGenerator::SecondsToSamples( U64 us ) const
{
	return ( mSimulationSampleRateHz * us );
}

U32 HdlcSimulationDataGenerator::GenerateSimulationData( U64 largest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channel )
{
	U64 adjusted_largest_sample_requested = AnalyzerHelpers::AdjustSimulationTargetSample( largest_sample_requested, sample_rate, mSimulationSampleRateHz );

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
		CreateFlagSequence();
		CreateFlagSequence();
		
		vector<U8> address = GenAddressField(mSettings->mHdlcAddr, addressBytes, 0x00);
		vector<U8> control = GenControlField(frameTypes[idxFrames++%3], mSettings->mHdlcControl, controlValue++);
		vector<U8> information = GenInformationField(size++, informationValue++);
		
		CreateHDLCFrame( address, control, information );
		
		// Two consecutive flags
		CreateFlagSequence();
		CreateFlagSequence();
		
	}
	
	*simulation_channel = &mHdlcSimulationData;
	return 1;
}

vector<U8> HdlcSimulationDataGenerator::GenAddressField( HdlcAddressType addressType, 
														 U64 addressBytes, 
														 U8 value )
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
														 U8 value ) 
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
				case HDLC_EXTENDED_CONTROL_FIELD_MOD_32768:
					controlRet.push_back(value); // third byte
					controlRet.push_back(value); // fourth byte
				case HDLC_EXTENDED_CONTROL_FIELD_MOD_2147483648: 
					controlRet.push_back(value); // fifth byte
					controlRet.push_back(value); // sixth byte
					controlRet.push_back(value); // seventh byte
					controlRet.push_back(value); // eighth byte
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
	
vector<U8> HdlcSimulationDataGenerator::GenInformationField( U16 size, U8 value ) 
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
	
	CreateFlagSequence();
	Transmit(allFields);	
	CreateFlagSequence();
}

vector<U8> HdlcSimulationDataGenerator::GenFcs( HdlcFcsType fcsType, const vector<U8> & stream ) 
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

void HdlcSimulationDataGenerator::Transmit( const vector<U8> & stream ) 
{
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
			
			if( consecutiveOnes == 5 ) // if five 1s in a row, then insert a 0 and continue
			{
				CreateBit( BIT_LOW );
				previousBit = BIT_LOW;
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
			CreateBit( bit );
			previousBit = bit;
		}
	}
}

void HdlcSimulationDataGenerator::CreateFlagSequence() 
{
	mHdlcSimulationData.Transition();
	mHdlcSimulationData.Advance( mSamplesInHalfPeriod * 7 );
	mHdlcSimulationData.Transition();
	mHdlcSimulationData.Advance( mSamplesInHalfPeriod );
}

// Maps the bit to the signal using NRZI 
void HdlcSimulationDataGenerator::CreateBit( BitState bitState ) 
{
	if( bitState == BIT_LOW ) // BIT_LOW == transition, BIT_HIGH == no transition
	{
		mHdlcSimulationData.Transition();
	}
	mHdlcSimulationData.Advance( mSamplesInHalfPeriod );
}
