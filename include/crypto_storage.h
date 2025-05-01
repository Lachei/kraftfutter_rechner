#pragma once

#include "static_types.h"

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
	
	static crypto_storage& Default() {
		static crypto_storage c{};
		return c;
	}
};

