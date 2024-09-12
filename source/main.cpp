#define TESLA_INIT_IMPL // If you have more than one file using the tesla header, only define this in the main one
#include <tesla.hpp>    // The Tesla Header

char toPrint[256] = "";
bool bluetoothEnabled = false;
std::vector<SetSysBluetoothDevicesSettings> audio_devices;

class GuiTest : public tsl::Gui {
public:
	GuiTest(u8 arg1, u8 arg2, bool arg3) {
		setsysGetBluetoothEnableFlag(&bluetoothEnabled);
		if (bluetoothEnabled == false) {
			sprintf(toPrint, "Bluetooth is disabled!\nOverlay disabled!");
		}
		else sprintf(toPrint, "Choose device.");
		s32 total_out = 0;
		SetSysBluetoothDevicesSettings devices[0x20] = {0};
		setsysGetBluetoothDevicesSettings(&total_out, devices, 0x20);
		if (R_SUCCEEDED(setsysGetBluetoothDevicesSettings(&total_out, devices, 0x20))) {
			for (s32 i = 0; i < total_out; i++) {
				union {
					u8 u8_value[4];
					u32 u32_value;
				} value;
				value.u8_value[2] = devices[i].class_of_device.class_of_device[0];
				value.u8_value[1] = devices[i].class_of_device.class_of_device[1];
				value.u8_value[0] = devices[i].class_of_device.class_of_device[2];

				if(value.u32_value & BIT(21)) {
					audio_devices.push_back(devices[i]);
				}
			}
		}
	}

	// Called when this Gui gets loaded to create the UI
	// Allocate all elements on the heap. libtesla will make sure to clean them up when not needed anymore
	virtual tsl::elm::Element* createUI() override {
		// A OverlayFrame is the base element every overlay consists of. This will draw the default Title and Subtitle.
		// If you need more information in the header or want to change it's look, use a HeaderOverlayFrame.
		auto frame = new tsl::elm::OverlayFrame("BT Audio", APP_VERSION);
		if (bluetoothEnabled) {
			frame -> changeButtons("\uE0E1 Back  \uE0E0 Connect  \uE0E3 Disconnect");
		}
		else frame -> changeButtons("\uE0E1  Back");

		// A list that can contain sub elements and handles scrolling
		auto list = new tsl::elm::List();
		
		list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
			renderer->drawString(toPrint, false, x, y+30, 20, renderer->a(0xFFFF));
		}), 100);

		if (bluetoothEnabled) {
			for (size_t i = 0; i < audio_devices.size(); i++) {
				auto *clickableListItem = new tsl::elm::ListItem(audio_devices[i].name2);
				clickableListItem->setClickListener([i](u64 keys) { 
					if (keys & HidNpadButton_A) {
						tsl::hlp::doWithSmSession([]{
							btdrvInitialize();
						});
						Result rc = btdrvOpenAudioConnection(audio_devices[i].addr);
						if (R_FAILED(rc)) {
							if (rc == 0x190A71) {
								sprintf(toPrint, "Device is already connected!");
							}
							else sprintf(toPrint, "Something went wrong!\nResult: 0x%x", rc);
						}
						else sprintf(toPrint, "%s\nconnected! It may take\nfew seconds to redirect audio.", audio_devices[i].name2);
						btdrvExit();
					}
					if (keys & HidNpadButton_Y) {
						tsl::hlp::doWithSmSession([]{
							btdrvInitialize();
						});
						Result rc = btdrvCloseAudioConnection(audio_devices[i].addr);
						if (R_FAILED(rc)) {
							if (rc == 0x190471) {
								sprintf(toPrint, "Device is already disconnected!");
							}
							else sprintf(toPrint, "Something went wrong!\nResult: 0x%x", rc);
						}
						else snprintf(toPrint, sizeof(toPrint), "%s\ndisconnected! It may take\nfew seconds to redirect audio.", audio_devices[i].name2);
						btdrvExit();
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
		setsysGetBluetoothEnableFlag(&bluetoothEnabled);
		if (bluetoothEnabled == false) {
			sprintf(toPrint, "Bluetooth is disabled!\nOverlay disabled!");
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
		sprintf(toPrint, "Bluetooth is disabled!\nEnable bluetooth in system settings.");
	}

	// Called when this Gui gets loaded to create the UI
	// Allocate all elements on the heap. libtesla will make sure to clean them up when not needed anymore
	virtual tsl::elm::Element* createUI() override {
		// A OverlayFrame is the base element every overlay consists of. This will draw the default Title and Subtitle.
		// If you need more information in the header or want to change it's look, use a HeaderOverlayFrame.
		auto frame = new tsl::elm::OverlayFrame("BT Audio", APP_VERSION);

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
			setsysGetBluetoothEnableFlag(&bluetoothEnabled);
		});
	
	}  // Called at the start to initialize all services necessary for this Overlay
	
	virtual void exitServices() override {
		setsysExit();
		audio_devices.clear();
	}  // Callet at the end to clean up all services previously initialized

	virtual void onShow() override {}    // Called before overlay wants to change from invisible to visible state
	
	virtual void onHide() override {}    // Called before overlay wants to change from visible to invisible state

	virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
		if (bluetoothEnabled == false) {
			return initially<GuiTest2>(1, 2, true);  // Initial Gui to load. It's possible to pass arguments to it's constructor like this
		}
		return initially<GuiTest>(1, 2, true);  // Initial Gui to load. It's possible to pass arguments to it's constructor like this
	}
};

int main(int argc, char **argv) {
	return tsl::loop<OverlayTest>(argc, argv);
}
