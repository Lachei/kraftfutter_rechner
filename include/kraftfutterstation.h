#pragma once

#include <string_view>
#include <array>
#include "static_types.h"
#include "mutex.h"
#include "uart_storage.h"
#include "ranges_util.h"
#include "kuhspeicher.h"

template<int MAX_STATIONS = 4, int RATIONS_PER_KG = 10, int MAX_RATIONS_IN_FLIGHT = 64, int REC_BUFFER_SIZE = 32>
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
	// Feed msg:
	// @S1 0x4 0x8 is acked with 0x06 for feed at station 0
	// AS1 0x4 0x8 is acked with 0x06 for feed at station 1
	struct message_timeout {
		std::string_view message{};
		uint32_t timeout{};
	};
	struct messages {
		constexpr static message_timeout 
			req_p0 = {"`RR",60000},
			req_p1 = {"aRR",60000},  // req_p1,
			req_p2 = {"@R\u0004V", 80000}, // req_p2 as template replace @,
			req_feed = {"@S1\u0004\u0008", 60000}, // req_f template for feeed, replace @ with the corerct
			req_p3_0 = {"\u00011\u00045", 70000}, // req_p3_0
			req_p3_1 = {"\u00111\u00045", 70000}; // req_p3_1
	};
	enum state {
		send_req_p0,
		send_req_p1,
		send_req_p2,
		await_ack_cow,
		send_req_feed,
		await_ack_feed,
		send_req_p3,
	};

	static kraftfutterstation& Default() {
		static kraftfutterstation station{};
		return station;
	}

	struct halsband_ration {
		int halsband;
		int rations_count;
	};

	struct rec_package {
		uint64_t ack_time{};
		int halsband{};
	};

	state state{send_req_p0};
	static_vector<halsband_ration, MAX_RATIONS_IN_FLIGHT, uint8_t> halsband_rationen{};
	std::array<uint64_t, MAX_STATIONS> station_last_feeds{};
	std::array<int, MAX_STATIONS> station_cur_cow{};
	mutex receive_mutex{};
	static_ring_buffer<rec_package, REC_BUFFER_SIZE, uint8_t> received_packages{};
	static_string<16> send_buffer{};
	uint64_t cow_request_time{};
	int cur_station{};

	// BLOCKING
	// Raw recieve task, does only queue the recieved packages
	void receive_packages() {
		int pos_after_ack{};
		for (;;) {
			char data = uart_futterstationen::Default().getc();
			uint64_t receive_time = time_us_64();
			bool timeout{};
			{
				scoped_lock lock{receive_mutex};
				uint64_t del_pack_start = receive_time - received_packages.back().ack_time;
				static constexpr uint64_t max_package_del = 8000;
				timeout = del_pack_start > max_package_del;
			}
			if (data == 0x6) {
				pos_after_ack = 0;
				scoped_lock lock{receive_mutex};
				received_packages.push({.ack_time = receive_time});
			}
			else if (pos_after_ack == 1 && !timeout) {
				scoped_lock lock{receive_mutex};
				received_packages.back().halsband += 100 * int(data - '0');
			}
			else if (pos_after_ack == 2 && !timeout) {
				scoped_lock lock{receive_mutex};
				received_packages.back().halsband += 10 * int(data - '0');
			}
			else if (pos_after_ack == 3 && !timeout) {
				scoped_lock lock{receive_mutex};
				received_packages.back().halsband += int(data - '0');
			}
				
			++pos_after_ack;
		}
	}

	// NONBLOCKING
	// Function to send out request packages and check receive repsonses from stations
	// Is internally based on a simply state machine
	// Transits a bunch of packages and returns the required time to wait
	// until the next message shall be sent 
	// (do a vTaskDelay(amount_of_time) after the call)
	int handle_station_communication() {
		uint64_t time_start = time_us_64();
		const auto get_wait_time = [time_start](uint32_t timeout){ return int((time_start + timeout - time_us_64()) / 1000); };
		switch (state) {
		case send_req_p0:
			uart_futterstationen::Default().puts(messages::req_p0.message);
			state = send_req_p1;
			return get_wait_time(messages::req_p0.timeout);
		case send_req_p1:
			uart_futterstationen::Default().puts(messages::req_p1.message);
			state = send_req_p2;
			return get_wait_time(messages::req_p1.timeout);
		case send_req_p2:
			send_buffer.fill(messages::req_p2.message);
			send_buffer[0] = '@' + cur_station;
			uart_futterstationen::Default().puts(send_buffer.sv());
			cow_request_time = time_start;
			state = await_ack_cow;
			return get_wait_time(messages::req_p2.timeout);
		case await_ack_cow: {
			scoped_lock lock{receive_mutex};
			const auto &p = received_packages.back();
			bool cow_in_station = p.ack_time > cow_request_time && p.halsband != 0;
			halsband_ration *entry = cow_in_station && time_start - station_last_feeds[cur_station] > settings::Default().dispense_timeout * 1e6
				? halsband_rationen | find{p.halsband, &halsband_ration::halsband} : nullptr;

			if (cow_in_station) {
				station_cur_cow[cur_station] = p.halsband;
			}
			if (cow_in_station && !entry) {
				// try fetch new ration
				float amount = kuhspeicher::Default().feed_cow(p.halsband, cur_station);
				if (amount > (1.f / RATIONS_PER_KG) && (entry = halsband_rationen.push())) {
					entry->halsband = p.halsband;
					entry->rations_count = amount * RATIONS_PER_KG;
				} else
					LogError("Cow with number {} could not be fed, kg: {}", p.halsband, amount);
			}
			if (entry) {
				// dispense previously fetched rations
				entry->rations_count -= 1;
				if (entry->rations_count <= 0)
					halsband_rationen.remove(entry - halsband_rationen.begin());
				station_last_feeds[cur_station] = time_start;
				state = send_req_feed;
			} 
			if (state == await_ack_cow) {
				station_cur_cow[cur_station] = 0;
				state = send_req_p3;
			}
			return 0;
		}
		case send_req_feed:
			send_buffer.fill(messages::req_feed.message);
			send_buffer[0] = '@' + cur_station;
			uart_futterstationen::Default().puts(send_buffer.sv());
			cow_request_time = time_start;
			state = await_ack_feed;
			return get_wait_time(messages::req_feed.timeout);
		case await_ack_feed: {
			scoped_lock lock{receive_mutex};
			const auto &p = received_packages.back();
			// only remove the cow if the feed was successfull
			if (p.ack_time > cow_request_time) {
			}
			state = send_req_p3;
			return 0;
		}
		case send_req_p3:
			if (cur_station & 1)
				uart_futterstationen::Default().puts(messages::req_p3_1.message);
			else
				uart_futterstationen::Default().puts(messages::req_p3_0.message);
			cur_station = (cur_station + 1) % MAX_STATIONS;
			state = send_req_p0;
			return get_wait_time(messages::req_p3_0.timeout);
		default: state = send_req_p0; return 0;
		}
	}
};

