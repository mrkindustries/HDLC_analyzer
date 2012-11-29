#ifndef HDLC_ANALYZER_SETTINGS
#define HDLC_ANALYZER_SETTINGS

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>

/////////////////////////////////////

// NOTE: terminology: 
//    * HDLC Frame == Saleae Logic Packet
//    * HDLC Field == Saleae Logic Frame
//    * HDLC transactions not supported

// enum for the types of HDLC frames (Information, Supervisory and Unnumbered)
enum HdlcFrameType { HDLC_I_FRAME = 0, HDLC_S_FRAME, HDLC_U_FRAME };
// enum for the Control field size
// enum to support the Address Field of more than 1 byte
enum HdlcAddressType { HDLC_BASIC_ADDRESS_FIELD = 0, HDLC_EXTENDED_ADDRESS_FIELD };
enum HdlcControlType { HDLC_BASIC_CONTROL_FIELD, 
					   HDLC_EXTENDED_CONTROL_FIELD_MOD_128, 
					   HDLC_EXTENDED_CONTROL_FIELD_MOD_32768, 
					   HDLC_EXTENDED_CONTROL_FIELD_MOD_2147483648 };
// enum for the type of Frame Check Sequence algorithm
enum HdlcFcsType { HDLC_CRC8 = 0, HDLC_CRC16, HDLC_CRC32 };

#define HDLC_FLAG 0x7E
#define HDLC_ESCAPE_SEQ 0x7D

/////////////////////////////////////

class HdlcAnalyzerSettings : public AnalyzerSettings
{
public:
	HdlcAnalyzerSettings();
	virtual ~HdlcAnalyzerSettings();

	virtual bool SetSettingsFromInterfaces();
	void UpdateInterfacesFromSettings();
	virtual void LoadSettings( const char* settings );
	virtual const char* SaveSettings();

	Channel mInputChannel;
	U32 mBitRate;

	HdlcAddressType mHdlcAddr;
	HdlcControlType mHdlcControl;
	
	HdlcFcsType mHdlcFcs;
	
protected:
	std::auto_ptr< AnalyzerSettingInterfaceChannel >	mInputChannelInterface;
	std::auto_ptr< AnalyzerSettingInterfaceInteger >	mBitRateInterface;
	std::auto_ptr< AnalyzerSettingInterfaceNumberList >	mHdlcAddrInterface;
	std::auto_ptr< AnalyzerSettingInterfaceNumberList >	mHdlcControlInterface;
	std::auto_ptr< AnalyzerSettingInterfaceNumberList >	mHdlcFcsInterface;
};

#endif //HDLC_ANALYZER_SETTINGS
