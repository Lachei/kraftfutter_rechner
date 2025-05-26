#pragma once

#include "string_util.h"
#include "static_types.h"
#include "persistent_storage.h"
#include "mbedtls/sha256.h"

namespace crypto_internal {
static constexpr std::string_view CA_CERT=R"(-----BEGIN CERTIFICATE-----
MIICkzCCAjqgAwIBAgIUQY7vWaGzYTSE+IM2/d7Ke9vxNnwwCgYIKoZIzj0EAwIw
gaAxCzAJBgNVBAYTAkRFMRAwDgYDVQQIDAdCYXZhcmlhMQ4wDAYDVQQHDAVBbmdl
cjEPMA0GA1UECgwGTGFjaGVpMRIwEAYDVQQLDAlTdHVkaW9zdXMxHjAcBgNVBAMM
FURjRGNDb252ZXJ0ZXIgUm9vdCBDQTEqMCgGCSqGSIb3DQEJARYbam9zZWZzdHVt
cGZlZ2dlckBvdXRsb29rLmRlMB4XDTI1MDUwMTE5MzcyN1oXDTI1MDUzMTE5Mzcy
N1owgaAxCzAJBgNVBAYTAkRFMRAwDgYDVQQIDAdCYXZhcmlhMQ4wDAYDVQQHDAVB
bmdlcjEPMA0GA1UECgwGTGFjaGVpMRIwEAYDVQQLDAlTdHVkaW9zdXMxHjAcBgNV
BAMMFURjRGNDb252ZXJ0ZXIgUm9vdCBDQTEqMCgGCSqGSIb3DQEJARYbam9zZWZz
dHVtcGZlZ2dlckBvdXRsb29rLmRlMFYwEAYHKoZIzj0CAQYFK4EEAAoDQgAEjDRH
4Gugc9NYjHHhbwaqHgaoShU0gwosbq17LGxP5NgjusNvIlr7fcvPQIeguF4pKWkZ
hdplQCHkYaLgTGknRKNTMFEwHQYDVR0OBBYEFBjLFIX6ZmaQwm/8DE9nb4bK6ejT
MB8GA1UdIwQYMBaAFBjLFIX6ZmaQwm/8DE9nb4bK6ejTMA8GA1UdEwEB/wQFMAMB
Af8wCgYIKoZIzj0EAwIDRwAwRAIgaSh7gyS9biELLCglUuHYFMZwULKX6V1h9BFU
9PzKJKsCICHVnFLosTzYRoZo+L5kCAshBMv4i9uGUXg4Py9zsXRA
-----END CERTIFICATE-----)";
}

/** @brief Storage for crypto objects and utility functions to use the pre-defined certificates */
struct crypto_storage {
	std::string_view ca_cert{crypto_internal::CA_CERT};
	static_string<64> user_pwd{};

	static constexpr std::string_view qop{"auth"};
	static constexpr std::string_view realm{"user@webui.org"};
	static constexpr std::string_view algorithm{"SHA-256"};
	static constexpr std::string_view hex_map{"0123456789abcdef"};
	static constexpr int SHA_SIZE{32};
	
	static crypto_storage& Default() {
		static crypto_storage c{};
		[[maybe_unused]] static bool inited = [](){ c.load_from_persistent_storage(); return true; }();
		return c;
	}

	void load_from_persistent_storage() {
		persistent_storage_t::Default().read(&persistent_storage_layout::user_pwd, user_pwd);
		user_pwd.sanitize();
		user_pwd.make_c_str_safe();
		LogInfo("Loaded user pwd size: {}", user_pwd.size());
	}

	bool set_password(std::string_view password) {
		user_pwd.fill(password);
		persistent_storage_t::Default().write(user_pwd, &persistent_storage_layout::user_pwd);
		return false;
	}

	/** @brief checks the validity of the authorization header and returns the username if successfull. If not successfull returns an empty string_view */
	std::string_view check_authorization(std::string_view method, std::string_view auth_header_content) {
		constexpr char colon{':'};
		std::string_view username;
		std::string_view response;
		std::string_view nonce;
		std::string_view cnonce;
		std::string_view nc; // nonce count, must not be quoted
		std::string_view uri;

		std::string_view cur = extract_word(auth_header_content);
		if (cur != "Digest") {
			LogError("check_authorization_header(): Missing Digest key word at the beginning");
			return {};
		}
		int i = 0;
		for (cur = extract_word(auth_header_content, ','); cur.size() && i < 20; ++i, cur = extract_word(auth_header_content, ',')) {
			std::string_view key = extract_word(cur, '='); // now cur holds the value
			for(;key.size() && key.front() == ' '; key.remove_prefix(1));
			for(;key.size() && key.back() == ' '; key.remove_suffix(1));
			if (cur.size() && cur.front() == '"') // unquoting
				cur.remove_prefix(1);
			if (cur.size() && cur.back() == '"')
				cur.remove_suffix(1);
			if (key == "username")
				username = cur;
			else if (key == "realm") {
				if (cur != realm) {
					LogError("check_authorization_header(): bad realm '{}', should be {}", cur, realm);
					return {};
				}
			} else if (key == "qop") {
				if (cur != qop) {
					LogError("check_authorization_header(): bad qop '{}', should be {}", cur, qop);
					return {};
				}
			} else if (key == "algorithm") {
				if (cur != algorithm) {
					LogError("check_authorization_header(): bad alorithm '{}', should be {}", cur, algorithm);
					return {};
				}
			} else if (key == "response")
				response = cur;
			else if (key == "nonce")
				nonce = cur;
			else if (key == "cnonce")
				cnonce = cur;
			else if (key == "nc")
				nc = cur;
			else if (key == "uri")
				uri = cur;
			else
			 	LogWarning("check_authorization_header(): unkwnown key '{}'", key);
		}
		if (response.size() != SHA_SIZE * 2) {
			LogError("check_authorization_header(): response sha has the wrong length {}", response.size());
			return {};
		}
		
		std::array<uint8_t, SHA_SIZE * 4> sha_storage; // even though only 2 digests have to be held, they need to be converted to hex
		// H(A1) calculation
		mbedtls_sha256_context ctx;
		mbedtls_sha256_init(&ctx);
		mbedtls_sha256_starts(&ctx, 0); // 0 = SHA-256 (not SHA-224)
		mbedtls_sha256_update(&ctx, (const uint8_t*)username.data(), username.size());
		mbedtls_sha256_update(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update(&ctx, (const uint8_t*)realm.data(), realm.size());
		mbedtls_sha256_update(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update(&ctx, (const uint8_t*)user_pwd.data(), user_pwd.size());
		mbedtls_sha256_finish(&ctx, sha_storage.data());
		// H(A2) calculation
		mbedtls_sha256_starts(&ctx, 0); // 0 = SHA-256 (not SHA-224)
		mbedtls_sha256_update(&ctx, (const uint8_t*)method.data(), method.size());
		mbedtls_sha256_update(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update(&ctx, (const uint8_t*)uri.data(), uri.size());
		mbedtls_sha256_finish(&ctx, sha_storage.data() + SHA_SIZE * 2);
		// convert H1 and H2 to hex
		auto *d = sha_storage.data() + 4 * SHA_SIZE - 1;
		for (auto *c = sha_storage.data() + 3 * SHA_SIZE - 1; c >= sha_storage.data() + 2 * SHA_SIZE; --c) {
			*d-- = hex_map[(*c) & 0xf];
			*d-- = hex_map[(*c) >> 4];
		}
		for (auto *c = sha_storage.data() + SHA_SIZE - 1; c >= sha_storage.data(); --c) {
			*d-- = hex_map[(*c) & 0xf];
			*d-- = hex_map[(*c) >> 4];
		}
		// final hash calc
		mbedtls_sha256_starts(&ctx, 0); // 0 = SHA-256 (not SHA-224)
		mbedtls_sha256_update(&ctx, (const uint8_t*)sha_storage.data(), SHA_SIZE * 2);
		mbedtls_sha256_update(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update(&ctx, (const uint8_t*)nonce.data(), nonce.size());
		mbedtls_sha256_update(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update(&ctx, (const uint8_t*)nc.data(), nc.size());
		mbedtls_sha256_update(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update(&ctx, (const uint8_t*)cnonce.data(), cnonce.size());
		mbedtls_sha256_update(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update(&ctx, (const uint8_t*)qop.data(), qop.size());
		mbedtls_sha256_update(&ctx, (const uint8_t*)&colon, 1);
		mbedtls_sha256_update(&ctx, (const uint8_t*)sha_storage.data() + SHA_SIZE * 2, SHA_SIZE * 2);
		mbedtls_sha256_finish(&ctx, sha_storage.data());

		mbedtls_sha256_free(&ctx);

		// compare the response (size was already previously checked for equal size)
		const char *r = response.data();
		for (auto *c = sha_storage.data(); c < sha_storage.data() + SHA_SIZE; ++c) {
			if (hex_map[(*c) >> 4] != *r++ ||
			    hex_map[(*c) & 0xf] != *r++)
				return {};
		}
		return username;
	}
};

