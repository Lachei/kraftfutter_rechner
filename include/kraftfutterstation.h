#pragma once

#include <string_view>
#include <array>
#include "static_types.h"

template<int RATIONS_PER_KG = 10, int MAX_RATIONS_IN_FLIGHT = 500>
struct kraftfutterstation {
	// communication messages
	// the main computer initiates the communication by sending the messages
	// req_p0 req_p1 (req_p2_0 | req_p2_1 | req_p2_2 | req_p2_3) (req_p3_0 | req_p3_1)
	// The station, if station 0 only answers if cow in it, station 2 always answers,
	// if no cow simply 0 0 0 is transferred
	// The answer is transmitted between req_p2 and req_p3 and looks like
	// 0x6 n2 n1 n0 0x4 0x20
	// 0x6 n2 n1 n0 0x4 0x1e
	// 0x6 0 0 0 0x4 0x14
	enum message_id {
		req_p0 = 0,
		req_p1,
		req_p2_0,
		req_p2_1,
		req_p2_2,
		req_p2_3,
		req_p3_0,
		req_p3_1,
	};
	static constexpr std::array<string_view> {
		"`RR",  // req_p0
		"aRR",  // req_p1,
		"@R4V", // req_p2_0,
		"AR4V", // req_p2_1,
		"BR4V", // req_p2_2,
		"CR4V", // req_p2_3,
		"0x1 1 0x4 5", // req_p3_0
		"0x11 1 0x4 5", // req_p3_1
	};

	static kraftfutterstation& Default() {
		static kraftfutterstation station{};
		return station;
	}

	struct halsband_rationen{
		int halsband;
		int rations_count;
	};

	static_vector<halsband_rationen, MAX_RATIONS_IN_FLIGHT, int> halband_rationen{};

	// BLOCKING
	// Raw recieve task, does only queue the recieved packages
	void recieve_packages() {

	}

	// NONBLOCKING
	// Raw transmit task, does not concern itself with package analysis
	// Transits a bunch of packages and returns the required time to wait
	// until the next message shall be sent 
	// (do a vTaskDelay(amount_of_time) after the call)
	int transmit_packages() {

	}


	// NONBLOCKING
	// Analyzes and updates the cow rations depending on the recieved packages
	// Also queues up new packages to be sent
	// Should be called every ~5ms for good performance
	void update_cow_rations() {
		// check recieved packages
		// queue up sending messages
	}
};

