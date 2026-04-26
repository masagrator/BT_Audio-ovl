#define TESLA_INIT_IMPL // If you have more than one file using the tesla header, only define this in the main one
#include <tesla.hpp>    // The Tesla Header

/// Authentication
typedef enum {
    NifmAuthentication_Invalid                          = 0, ///< Invalid
    NifmAuthentication_Open                             = 1, ///< Open
    NifmAuthentication_Shared                           = 2, ///< Shared
    NifmAuthentication_Wpa                              = 3, ///< WPA
    NifmAuthentication_WpaPsk                           = 4, ///< WPA-PSK
    NifmAuthentication_Wpa2                             = 5, ///< WPA2
    NifmAuthentication_Wpa2Psk                          = 6, ///< WPA2-PSK
    NifmAuthentication_Unk7                             = 7, ///< Unknown
} NifmAuthentication;

/// Encryption
typedef enum {
    NifmEncryption_Invalid                              = 0, ///< Invalid
    NifmEncryption_None                                 = 1, ///< No password
    NifmEncryption_Wep                                  = 2, ///< WEP 
    NifmEncryption_Tkip                                 = 3, ///< TKIP
    NifmEncryption_Aes                                  = 4, ///< AES
} NifmEncryption;

typedef enum {
    NifmNetworkProfileType_User                         = BIT(0), ///< Saved by user
    NifmNetworkProfileType_SsidList                     = BIT(1), ///< Hardcoded list of Nintendo hotspots
    NifmNetworkProfileType_Temporary                    = BIT(2), ///< Temporary
} NifmNetworkProfileType;

/// SfNetworkProfileBasicInfo. Converted from/to \ref NifmNetworkProfileBasicInfo.
typedef struct {
    Uuid uuid;                                           ///< Uuid
    char network_name[0x40];                             ///< NUL-terminated Network Name string.
    u8 profile_type;                                     ///< \ref NifmNetworkProfileType
    u8 connection_type;                                  ///< \ref NifmInternetConnectionType
    u8 ssid_len;                                         ///< SSID length.
    char ssid[0x20];                                     ///< SSID string.
    u8 authentication;                                   ///< \ref NifmAuthentication
    u8 encryption;                                       ///< \ref NifmEncryption
} NifmSfNetworkProfileBasicInfo;

/// NetworkProfileBasicInfo. Converted from/to \ref NifmSfNetworkProfileBasicInfo.
typedef struct {
    Uuid uuid;                                           ///< Uuid
    char network_name[0x40];                             ///< NUL-terminated Network Name string.
    NifmNetworkProfileType profile_type;                 ///< \ref NifmNetworkProfileType
    NifmInternetConnectionType connection_type;          ///< \ref NifmInternetConnectionType
    u8 ssid_len;                                         ///< SSID length.
    char ssid[0x20];                                     ///< SSID string.
    u8 pad[3];                                           ///< Padding
    NifmAuthentication authentication;                   ///< \ref NifmAuthentication
    NifmEncryption encryption;                           ///< \ref NifmEncryption
} NifmNetworkProfileBasicInfo;

static void _nifmConvertSfToNetworkProfileBasicInfo(const NifmSfNetworkProfileBasicInfo *in, NifmNetworkProfileBasicInfo *out) {
    memset(out, 0, sizeof(*out));

    out->uuid = in->uuid;
    memcpy(out->network_name, in->network_name, sizeof(in->network_name));
    out->network_name[sizeof(out->network_name)-1] = 0;
    out->profile_type = (NifmNetworkProfileType)in->profile_type;
    out->connection_type = (NifmInternetConnectionType)in->connection_type;

    out->ssid_len = in->ssid_len;
    if (out->ssid_len > sizeof(out->ssid)) out->ssid_len = sizeof(out->ssid);
    if (out->ssid_len) memcpy(out->ssid, in->ssid, out->ssid_len);
    out->authentication = (NifmAuthentication)in->authentication;
    out->encryption = (NifmEncryption)in->encryption;
}

Result nifmEnumerateNetworkProfiles(NifmNetworkProfileType type, NifmNetworkProfileBasicInfo* buffer, s32 max_entries, s32* total_entries) {
    NifmSfNetworkProfileBasicInfo* tmp_ptr = (NifmSfNetworkProfileBasicInfo*)buffer;
    u8 in = (u8)type;
    serviceAssumeDomain(nifmGetServiceSession_GeneralService());
    Result rc = serviceDispatchInOut(nifmGetServiceSession_GeneralService(), 7, in, *total_entries,
        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out},
        .buffers = { { buffer, sizeof(tmp_ptr[0]) * max_entries } },
    );
    if (R_FAILED(rc)) return rc;
    s32 returned_entries = *total_entries < max_entries ? *total_entries : max_entries;
    for (s32 i = (returned_entries-1); i >= 0; i--) {
        NifmSfNetworkProfileBasicInfo tmp;
        memcpy(&tmp, &tmp_ptr[i], sizeof(tmp));
        _nifmConvertSfToNetworkProfileBasicInfo(&tmp, &buffer[i]);
    }
    return rc;
}

Result nifmRequestSetNetworkProfileId(NifmRequest* r, Uuid uuid) {
    if (!serviceIsActive(&r->s))
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);

    serviceAssumeDomain(&r->s);
    return serviceDispatchIn(&r->s, 9, uuid);
}

struct WiFiSpots {
	Uuid uuid;
	std::string name;
};
bool wifiEnabled = false;
std::vector<WiFiSpots> wifi_devices;
char toPrint[256] = "";
NifmRequest _request;
s32 last_index = -1;
Result connectionRc = 0;
bool requestOpen = true;

class GuiTest : public tsl::Gui {
public:
	GuiTest(u8 arg1, u8 arg2, bool arg3) {
		setsysGetWirelessLanEnableFlag(&wifiEnabled);
		if (wifiEnabled == true) {
			NifmInternetConnectionType type;
			u32 dummy;
			NifmInternetConnectionStatus status;
			Result rc = nifmGetInternetConnectionStatus(&type, &dummy, &status);
			if (R_SUCCEEDED(rc) && type == NifmInternetConnectionType_Ethernet) {
				wifiEnabled = false;
				sprintf(toPrint, "Ethernet connection detected!\nOverlay disabled!");
				return;
			}
		}
		if (wifiEnabled == false) {
			sprintf(toPrint, "Wi-Fi is disabled!\nOverlay disabled!");
			return;
		}
		else {
			NifmNetworkProfileData profile;
			Result rc = nifmGetCurrentNetworkProfile(&profile);
			if (R_FAILED(rc)) sprintf(toPrint, "Choose device.");
			else sprintf(toPrint, "Connected to:\n%s\nChoose device.", profile.network_name[0] ? profile.network_name : std::string(profile.wireless_setting_data.ssid, profile.wireless_setting_data.ssid_len).c_str());
		}
		s32 total_out = 0;
		Result rc = nifmEnumerateNetworkProfiles(NifmNetworkProfileType_User, nullptr, 0, &total_out);
		if (R_FAILED(rc) || total_out == 0) return;
		NifmNetworkProfileBasicInfo* basicInfo = new NifmNetworkProfileBasicInfo[total_out];
		nifmEnumerateNetworkProfiles(NifmNetworkProfileType_User, basicInfo, total_out, &total_out);
		for (s32 i = total_out-1; i >= 0; i--) {
			if (basicInfo[i].connection_type == NifmInternetConnectionType_WiFi) wifi_devices.emplace_back(basicInfo[i].uuid, (basicInfo[i].network_name[0] ? basicInfo[i].network_name : std::string(basicInfo[i].ssid, basicInfo[i].ssid_len)));
		}
		delete[] basicInfo;
	}

	// Called when this Gui gets loaded to create the UI
	// Allocate all elements on the heap. libtesla will make sure to clean them up when not needed anymore
	virtual tsl::elm::Element* createUI() override {
		// A OverlayFrame is the base element every overlay consists of. This will draw the default Title and Subtitle.
		// If you need more information in the header or want to change it's look, use a HeaderOverlayFrame.
		auto frame = new tsl::elm::OverlayFrame(APP_TITLE, APP_VERSION);
		if (wifiEnabled) {
			frame -> changeButtons("\uE0E1 Back  \uE0E0 Connect");
		}
		else frame -> changeButtons("\uE0E1  Back");

		// A list that can contain sub elements and handles scrolling
		auto list = new tsl::elm::List();
		
		list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
			renderer->drawString(toPrint, false, x, y+30, 20, renderer->a(0xFFFF));
		}), 120);

		if (wifiEnabled) {
			for (size_t i = 0; i < wifi_devices.size(); i++) {
				auto *clickableListItem = new tsl::elm::ListItem(wifi_devices[i].name);
				clickableListItem->setClickListener([i, this](u64 keys) { 
					if (wifiEnabled && requestOpen && (keys & HidNpadButton_A)) {
						if (connectionRc == UINT32_MAX) {
							nifmRequestCancel(&_request);
						}
						nifmRequestClose(&_request);
						connectionRc = nifmCreateRequest(&_request, true);
						if (R_SUCCEEDED(connectionRc)) connectionRc = nifmRequestSetNetworkProfileId(&_request, wifi_devices[i].uuid);
						if (R_SUCCEEDED(connectionRc)) connectionRc = nifmRequestSubmit(&_request);
						if (R_FAILED(connectionRc)) nifmRequestClose(&_request);
						else connectionRc = UINT32_MAX;
						last_index = i;
						requestOpen = false;
						for (size_t x = 0; x < i; x++) {
							this->requestFocus(this->getFocusedElement()->getParent(), tsl::FocusDirection::Up, false);
						}
						return true;
					}
					return false;
				});

				list->addItem(clickableListItem);
			}
		}

		// Add the list to the frame for it to be drawn
		frame->setContent(list);
		
		// Return the frame to have it become the top level element of this Gui
		return frame;
	}

	// Called once every frame to update values
	virtual void update() override {
		if (last_index > -1) {
			if (connectionRc == UINT32_MAX) {
				NifmRequestState tmp;
				Result rc = nifmGetRequestState(&_request, &tmp);
				if (R_FAILED(rc)) {
					connectionRc = rc;
					requestOpen = true;
				}
				if (tmp == NifmRequestState_OnHold) snprintf(toPrint, sizeof(toPrint), "Connecting to:\n%s...", wifi_devices[last_index].name.c_str());
				else {
					connectionRc = nifmGetResult(&_request);
					requestOpen = true;
				}
			}
			else if (R_SUCCEEDED(connectionRc)) snprintf(toPrint, sizeof(toPrint), "Successfully connected to:\n%s", wifi_devices[last_index].name.c_str());
			else {
				requestOpen = true;
				#define RESULT_WIFI_OFF 0x8ae6e
				#define RESULT_WIFI_NOT_FOUND 0x8986e
				#define RESULT_WIFI_NOT_FOUND_DISCONNECTED 0xfa66e
				if (connectionRc == RESULT_WIFI_OFF) {
					sprintf(toPrint, "Error! Wi-Fi turned off!\nOverlay disabled!");
					wifiEnabled = false;
				}
				else if (connectionRc == RESULT_WIFI_NOT_FOUND) {
					snprintf(toPrint, sizeof(toPrint), "Couldn't connect to:\n%s\n\nLast connection was maintained.", wifi_devices[last_index].name.c_str());
				}
				else if (connectionRc == RESULT_WIFI_NOT_FOUND_DISCONNECTED) {
					snprintf(toPrint, sizeof(toPrint), "Couldn't connect to:\n%s\n\nLast connection was not maintained!", wifi_devices[last_index].name.c_str());
				}
				else {
					snprintf(toPrint, sizeof(toPrint), "Error while connecting to:\n%s\nError code: 0x%x", wifi_devices[last_index].name.c_str(), connectionRc);
				}
			}
		}
		if (wifiEnabled == true) {
			setsysGetWirelessLanEnableFlag(&wifiEnabled);
			NifmInternetConnectionType type;
			u32 dummy;
			NifmInternetConnectionStatus status;
			Result rc = nifmGetInternetConnectionStatus(&type, &dummy, &status);
			if (R_SUCCEEDED(rc) && type == NifmInternetConnectionType_Ethernet) {
				wifiEnabled = false;
			}
		}
		if (wifiEnabled == false) {
			NifmInternetConnectionType type;
			u32 dummy;
			NifmInternetConnectionStatus status;
			Result rc = nifmGetInternetConnectionStatus(&type, &dummy, &status);
			if (R_SUCCEEDED(rc) && type == NifmInternetConnectionType_Ethernet) {
				sprintf(toPrint, "Ethernet connection detected!\nOverlay disabled!");
				return;
			}
			sprintf(toPrint, "Wi-Fi is disabled!\nOverlay disabled!");
		}
	}

	// Called once every frame to handle inputs not handled by other UI elements
	virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
		return false;   // Return true here to singal the inputs have been consumed
	}
};

class GuiTest2 : public tsl::Gui {
public:
	GuiTest2(u8 arg1, u8 arg2, bool arg3) {
		sprintf(toPrint, "Wi-Fi is disabled!\nEnable Wi-Fi in system settings.");
	}

	// Called when this Gui gets loaded to create the UI
	// Allocate all elements on the heap. libtesla will make sure to clean them up when not needed anymore
	virtual tsl::elm::Element* createUI() override {
		// A OverlayFrame is the base element every overlay consists of. This will draw the default Title and Subtitle.
		// If you need more information in the header or want to change it's look, use a HeaderOverlayFrame.
		auto frame = new tsl::elm::OverlayFrame(APP_TITLE, APP_VERSION);

		frame -> changeButtons("\uE0E1  Back");

		// A list that can contain sub elements and handles scrolling
		auto list = new tsl::elm::List();
		
		list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
			renderer->drawString(toPrint, false, x, y+30, 20, renderer->a(0xFFFF));
		}), 100);

		frame->setContent(list);
		
		// Return the frame to have it become the top level element of this Gui
		return frame;
	}

	// Called once every frame to update values
	virtual void update() override {}

	// Called once every frame to handle inputs not handled by other UI elements
	virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
		return false;   // Return true here to singal the inputs have been consumed
	}
};

class OverlayTest : public tsl::Overlay {
public:
	// libtesla already initialized fs, hid, pl, pmdmnt, hid:sys and set:sys
	virtual void initServices() override {

		tsl::hlp::doWithSmSession([]{
			
			setsysInitialize();
			nifmInitialize(NifmServiceType_User);
			nifmCreateRequest(&_request, true);
			setsysGetWirelessLanEnableFlag(&wifiEnabled);
		});
	
	}  // Called at the start to initialize all services necessary for this Overlay
	
	virtual void exitServices() override {
		nifmRequestCancel(&_request);
		nifmRequestClose(&_request);
		nifmExit();
		setsysExit();
		wifi_devices.clear();
	}  // Callet at the end to clean up all services previously initialized

	virtual void onShow() override {}    // Called before overlay wants to change from invisible to visible state
	
	virtual void onHide() override {}    // Called before overlay wants to change from visible to invisible state

	virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
		if (wifiEnabled == false) {
			return initially<GuiTest2>(1, 2, true);  // Initial Gui to load. It's possible to pass arguments to it's constructor like this
		}
		return initially<GuiTest>(1, 2, true);  // Initial Gui to load. It's possible to pass arguments to it's constructor like this
	}
};

int main(int argc, char **argv) {
	return tsl::loop<OverlayTest>(argc, argv);
}
