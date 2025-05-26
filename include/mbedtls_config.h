#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/* System support */
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_VERSION_C

/* mbed TLS modules */
#define MBEDTLS_SHA256_C

/* Enable required functions for SHA256 */
#define MBEDTLS_MD_C

/* Optional: reduce memory by disabling error strings and version info */
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES
#define MBEDTLS_VERSION_FEATURES

/* Optional: disable all error strings to save space */
#define MBEDTLS_ERROR_STRERROR_DUMMY

#endif /* MBEDTLS_CONFIG_H */

