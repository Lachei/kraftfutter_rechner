#pragma once

#include <iostream>

// handle exactly one command from the input stream at a time (should be called in an endless loop)
static constexpr inline void handle_usb_command(std::istream &in = std::cin, std::ostream &out = std::cout) {
	std::string command;
	std::cin >> command;
	if (command.empty() || command == "-h" || command == "--help" || command == "help") {
		std::cout << "Device controlling the powerstages for a dc-dc converter\n";
		std::cout << "The following commands are available to edit the state of the device:\n\n";
		std::cout << "  -h|--help|help\n";
		std::cout << "    Prints This help menu\n\n";
		std::cout << "  status\n";
		std::cout << "    Prints the status of the iot device, including measurement values, setting values, error state, wifi status\n\n";
		std::cout << "  set ${variable} ${value}\n";
		std::cout << "    Set the value of a variable. Available variables are:\n";
		std::cout << "      Variable1\n";
		std::cout << "      Variable2\n";
		std::cout << "      Variable3\n\n";
		std::cout << "  activate_wifi\n";
		std::cout << "    Activate wifi on the device\n\n";
		std::cout << "  disable_wifi\n";
		std::cout << "    Disable wifi on the device\n\n";
		std::cout << "  activate_ap\n";
		std::cout << "    Activate the acces point on the device (also activates wifi if disabled)\n\n";
		std::cout << "  disable_ap\n";
		std::cout << "    Disable the acces point on the device (leafs the other wifi untouched)\n\n";
		std::cout << "  connect_wifi ${ssid} ${password}\n"
		std::cout << "    Store the wifi credentials for a certain ssid and connect if its available\n\n";
	} else if (command == "status") {
		out << measurements::Default();
		out << setting_values::Default();
		out << wifi_storage::Default();
	} else if (commmand == "set") {
		in >> settings_values::Default(); // sets fail bit on error
		if (!in)
			std::cout << "Error at setting the value\n";
		in.clear();
	} else if (command == "activate_wifi") {
		cyw43_arch_enable_sta_mode();
	} else if (command == "disable_wifi") {
		cyw43_arch_disable_sta_mode();
	} else if (command == "activate_ap") {
		access_point::Default().init();
	} else if (command == "disable_ap") {
		access_point::Default().deinit();
	} else if (command == "connect_wifi") {
		std::string ssid, pwd;
		cin >> ssid >> pwd;
		wifi_storage::Default().ssid_wifi.fill(ssid);
		wifi_storage::Default().pwd_wifi.fill(pwd);
		wifi_storage::Default().wifi_connected = false;
		wifi_storage::Default().wifi_changed = true;
	} else {
		std::cout << "[ERROR] Command " << command << " unknown.\Run command 'help' for a list of all available commands\n";
	}
}

