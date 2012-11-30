#include "HdlcAnalyzerSettings.h"
#include <AnalyzerHelpers.h>

// TODO: test user interface!

HdlcAnalyzerSettings::HdlcAnalyzerSettings()
:	mInputChannel( UNDEFINED_CHANNEL ),
	mBitRate( 1500 ),
	mHdlcAddr( HDLC_BASIC_ADDRESS_FIELD ),
	mHdlcControl( HDLC_BASIC_CONTROL_FIELD ),
	mHdlcFcs( HDLC_CRC16 )
{
	mInputChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
	mInputChannelInterface->SetTitleAndTooltip( "HDLC", "Standard HDLC" );
	mInputChannelInterface->SetChannel( mInputChannel );

	mBitRateInterface.reset( new AnalyzerSettingInterfaceInteger() );
	mBitRateInterface->SetTitleAndTooltip( "Bit Rate (Bits/S)",  "Specify the bit rate in bits per second." );
	mBitRateInterface->SetMax( 6000000 );
	mBitRateInterface->SetMin( 1 );
	mBitRateInterface->SetInteger( mBitRate );

	mHdlcAddrInterface.reset( new AnalyzerSettingInterfaceNumberList() );
	mHdlcAddrInterface->SetTitleAndTooltip( "Address Field Type", "Specify the address field type of an HDLC frame." );
	mHdlcAddrInterface->AddNumber( HDLC_BASIC_ADDRESS_FIELD, "Basic", "Basic Address Field (8 bits)" );
	mHdlcAddrInterface->AddNumber( HDLC_EXTENDED_ADDRESS_FIELD, "Extended", "Extended Address Field (8 or more bits)" );
	mHdlcAddrInterface->SetNumber( mHdlcAddr );
	
	mHdlcControlInterface.reset( new AnalyzerSettingInterfaceNumberList() );
	mHdlcControlInterface->SetTitleAndTooltip( "Control Field Format", "Specify the Control Field type of a HDLC frame." );
	mHdlcControlInterface->AddNumber( HDLC_BASIC_CONTROL_FIELD, "Basic - Modulo 8", "Control Field of 8 bits" );
	mHdlcControlInterface->AddNumber( HDLC_EXTENDED_CONTROL_FIELD_MOD_128, "Extended - Modulo 128", "Control Field of 16 bits" );
	mHdlcControlInterface->AddNumber( HDLC_EXTENDED_CONTROL_FIELD_MOD_32768, "Extended - Modulo 32768", "Control Field of 32 bits" );
	mHdlcControlInterface->AddNumber( HDLC_EXTENDED_CONTROL_FIELD_MOD_2147483648, "Extended - Modulo 2147483648", "Control Field of 64 bits" );
	mHdlcControlInterface->SetNumber( mHdlcControl );
	
	mHdlcFcsInterface.reset( new AnalyzerSettingInterfaceNumberList() );
	mHdlcFcsInterface->SetTitleAndTooltip( "FCS Type", "Specify the Frame Check Sequence of an HDLC frame" );
	mHdlcFcsInterface->AddNumber( HDLC_CRC8, "CRC-8", "8-bit Cyclic Redundancy Check" );
	mHdlcFcsInterface->AddNumber( HDLC_CRC16, "CRC-16-CCITT", "16-bit Cyclic Redundancy Check" );
	mHdlcFcsInterface->AddNumber( HDLC_CRC32, "CRC-32", "32-bit Cyclic Redundancy Check" );
	mHdlcFcsInterface->SetNumber( mHdlcAddr );
	
	AddInterface( mInputChannelInterface.get() );
	AddInterface( mBitRateInterface.get() );
	AddInterface( mHdlcAddrInterface.get() );
	AddInterface( mHdlcControlInterface.get() );
	AddInterface( mHdlcFcsInterface.get() );
	
	AddExportOption( 0, "Export as text/csv file" );
	AddExportExtension( 0, "text", "txt" );
	AddExportExtension( 0, "csv", "csv" );

	ClearChannels();
	AddChannel( mInputChannel, "Hdlc", false );
}

HdlcAnalyzerSettings::~HdlcAnalyzerSettings()
{
}

bool HdlcAnalyzerSettings::SetSettingsFromInterfaces()
{
	mInputChannel = mInputChannelInterface->GetChannel();
	mBitRate = mBitRateInterface->GetInteger();
	mHdlcAddr = HdlcAddressType( U32( mHdlcAddrInterface->GetNumber() ) );
	mHdlcControl = HdlcControlType( U32( mHdlcControlInterface->GetNumber() ) );
	mHdlcFcs = HdlcFcsType( U32( mHdlcFcsInterface->GetNumber() ) );
	
	ClearChannels();
	AddChannel( mInputChannel, "Hdlc", true );
	
	return true;
}

void HdlcAnalyzerSettings::UpdateInterfacesFromSettings()
{
	mInputChannelInterface->SetChannel( mInputChannel );
	mBitRateInterface->SetInteger( mBitRate );
	mHdlcAddrInterface->SetNumber( mHdlcAddr );
	mHdlcControlInterface->SetNumber( mHdlcControl );
	mHdlcFcsInterface->SetNumber( mHdlcFcs );
}

void HdlcAnalyzerSettings::LoadSettings( const char* settings )
{
	SimpleArchive text_archive;
	text_archive.SetString( settings );

	text_archive >> mInputChannel;
	text_archive >> mBitRate;
	text_archive >> *( U32* ) &mHdlcAddr;
	text_archive >> *( U32* ) &mHdlcControl;
	text_archive >> *( U32* ) &mHdlcFcs;

	ClearChannels();
	AddChannel( mInputChannel, "Hdlc", true );

	UpdateInterfacesFromSettings();
}

const char* HdlcAnalyzerSettings::SaveSettings()
{
	SimpleArchive text_archive;

	text_archive << mInputChannel;
	text_archive << mBitRate;
	text_archive << U32( mHdlcAddr );
	text_archive << U32( mHdlcControl );
	text_archive << U32( mHdlcFcs );

	return SetReturnString( text_archive.GetString() );
}
