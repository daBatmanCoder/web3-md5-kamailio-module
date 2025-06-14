/*
 * Web3 Authentication Module for Kamailio
 * Provides blockchain-based SIP authentication using Oasis Sapphire testnet
 * Replaces traditional MD5 digest authentication with Web3 verification
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <stdint.h>
#include <ctype.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/error.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/digest/digest.h"
#include "../../core/parser/parse_authorization.h"
#include "../../core/parser/parse_authenticate.h"
#include "../../core/data_lump.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"

MODULE_VERSION

#define DEFAULT_RPC_URL "https://testnet.sapphire.oasis.dev"
#define DEFAULT_CONTRACT_ADDRESS "0x1b55e67Ce5118559672Bf9EC0564AE3A46C41000"
#define MAX_AUTH_HEADER_SIZE 2048
#define MAX_FIELD_SIZE 256

// Module parameters
static char* rpc_url = DEFAULT_RPC_URL;
static char* contract_address = DEFAULT_CONTRACT_ADDRESS;

// Keccak-256 implementation constants
#define KECCAK_ROUNDS 24

static const uint64_t keccak_round_constants[KECCAK_ROUNDS] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};

static const int rho_offsets[24] = {
    1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14, 27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44
};

static const int pi_offsets[24] = {
    10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4, 15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1
};

// Structure to hold SIP digest auth components
typedef struct {
    str username;
    str realm;
    str uri;
    str nonce;
    str response;
    str method;
} sip_auth_t;

// Structure to hold response data from CURL
struct ResponseData {
    char *memory;
    size_t size;
};

// Function prototypes
static int web3_auth_check(struct sip_msg* msg, char* p1, char* p2);
static int web3_auth_with_realm(struct sip_msg* msg, char* realm_param, char* p2);
static int mod_init(void);
static void mod_destroy(void);

// Rotate left function for Keccak
static inline uint64_t rotl64(uint64_t x, int n) {
    return (x << n) | (x >> (64 - n));
}

// Keccak permutation function
static void keccak_f1600(uint64_t state[25]) {
    for (int round = 0; round < KECCAK_ROUNDS; round++) {
        // Theta step
        uint64_t C[5];
        for (int i = 0; i < 5; i++) {
            C[i] = state[i] ^ state[i + 5] ^ state[i + 10] ^ state[i + 15] ^ state[i + 20];
        }
        
        for (int i = 0; i < 5; i++) {
            uint64_t D = C[(i + 4) % 5] ^ rotl64(C[(i + 1) % 5], 1);
            for (int j = 0; j < 25; j += 5) {
                state[j + i] ^= D;
            }
        }
        
        // Rho and Pi steps
        uint64_t current = state[1];
        for (int i = 0; i < 24; i++) {
            int j = pi_offsets[i];
            uint64_t temp = state[j];
            state[j] = rotl64(current, rho_offsets[i]);
            current = temp;
        }
        
        // Chi step
        for (int j = 0; j < 25; j += 5) {
            uint64_t t[5];
            for (int i = 0; i < 5; i++) {
                t[i] = state[j + i];
            }
            for (int i = 0; i < 5; i++) {
                state[j + i] = t[i] ^ ((~t[(i + 1) % 5]) & t[(i + 2) % 5]);
            }
        }
        
        // Iota step
        state[0] ^= keccak_round_constants[round];
    }
}

// Keccak-256 hash function
void keccak256(const uint8_t *input, size_t input_len, uint8_t output[32]) {
    uint64_t state[25] = {0};
    uint8_t *state_bytes = (uint8_t *)state;
    
    // Absorb phase
    size_t rate = 136; // (1600 - 256) / 8 for Keccak-256
    size_t offset = 0;
    
    while (input_len >= rate) {
        for (size_t i = 0; i < rate; i++) {
            state_bytes[i] ^= input[offset + i];
        }
        keccak_f1600(state);
        offset += rate;
        input_len -= rate;
    }
    
    // Final block with remaining input
    for (size_t i = 0; i < input_len; i++) {
        state_bytes[i] ^= input[offset + i];
    }
    
    // Padding
    state_bytes[input_len] ^= 0x01;
    state_bytes[rate - 1] ^= 0x80;
    
    // Final permutation
    keccak_f1600(state);
    
    // Extract output
    memcpy(output, state_bytes, 32);
}

// Calculate function selector from function signature
char* get_function_selector(const char* function_signature) {
    uint8_t hash[32];
    keccak256((const uint8_t*)function_signature, strlen(function_signature), hash);
    
    // Take first 4 bytes and convert to hex string
    char* selector = pkg_malloc(11); // "0x" + 8 hex chars + null terminator
    if (!selector) return NULL;
    
    snprintf(selector, 11, "0x%02x%02x%02x%02x", 
             hash[0], hash[1], hash[2], hash[3]);
    
    return selector;
}

// Helper function to pad string data to 32-byte boundaries
char* pad_string_data(const char* str, size_t* padded_length) {
    size_t len = strlen(str);
    size_t padded_len = ((len + 31) / 32) * 32; // Round up to nearest 32 bytes
    
    if (padded_len == 0) padded_len = 32;
    
    char* padded = pkg_malloc(padded_len * 2 + 1);
    if (!padded) return NULL;
    
    memset(padded, '0', padded_len * 2);
    
    // Convert string to hex
    for (size_t i = 0; i < len; i++) {
        sprintf(padded + i * 2, "%02x", (unsigned char)str[i]);
    }
    
    // Fill the rest with zeros
    for (size_t i = len * 2; i < padded_len * 2; i++) {
        padded[i] = '0';
    }
    
    padded[padded_len * 2] = '\0';
    *padded_length = padded_len;
    return padded;
}

// Encode call data for getDigestHash(string,string,string,string,string)
char* encode_digest_hash_call(const char* str1, const char* str2, const char* str3, const char* str4, const char* str5) {
    char* selector = get_function_selector("getDigestHash(string,string,string,string,string)");
    if (!selector) return NULL;
    
    size_t len1 = strlen(str1), len2 = strlen(str2), len3 = strlen(str3), len4 = strlen(str4), len5 = strlen(str5);
    size_t padded_len1, padded_len2, padded_len3, padded_len4, padded_len5;
    
    char* padded_str1 = pad_string_data(str1, &padded_len1);
    char* padded_str2 = pad_string_data(str2, &padded_len2);
    char* padded_str3 = pad_string_data(str3, &padded_len3);
    char* padded_str4 = pad_string_data(str4, &padded_len4);
    char* padded_str5 = pad_string_data(str5, &padded_len5);
    
    if (!padded_str1 || !padded_str2 || !padded_str3 || !padded_str4 || !padded_str5) {
        if (selector) pkg_free(selector);
        if (padded_str1) pkg_free(padded_str1);
        if (padded_str2) pkg_free(padded_str2);
        if (padded_str3) pkg_free(padded_str3);
        if (padded_str4) pkg_free(padded_str4);
        if (padded_str5) pkg_free(padded_str5);
        return NULL;
    }
    
    size_t offset1 = 0xA0;
    size_t offset2 = offset1 + 32 + padded_len1;
    size_t offset3 = offset2 + 32 + padded_len2;
    size_t offset4 = offset3 + 32 + padded_len3;
    size_t offset5 = offset4 + 32 + padded_len4;
    
    size_t total_size = 8 + (64 * 5) + (64 * 5) + strlen(padded_str1) + strlen(padded_str2) + 
                       strlen(padded_str3) + strlen(padded_str4) + strlen(padded_str5) + 1;
    char* call_data = pkg_malloc(total_size);
    
    if (!call_data) {
        pkg_free(selector);
        pkg_free(padded_str1); pkg_free(padded_str2); pkg_free(padded_str3); 
        pkg_free(padded_str4); pkg_free(padded_str5);
        return NULL;
    }
    
    snprintf(call_data, total_size,
        "%s"                              // function selector
        "%064lx"                          // offset to string 1
        "%064lx"                          // offset to string 2
        "%064lx"                          // offset to string 3
        "%064lx"                          // offset to string 4
        "%064lx"                          // offset to string 5
        "%064lx%s"                        // length + data for string 1
        "%064lx%s"                        // length + data for string 2
        "%064lx%s"                        // length + data for string 3
        "%064lx%s"                        // length + data for string 4
        "%064lx%s",                       // length + data for string 5
        selector + 2,                     // remove "0x" prefix
        offset1, offset2, offset3, offset4, offset5,
        len1, padded_str1,
        len2, padded_str2,
        len3, padded_str3,
        len4, padded_str4,
        len5, padded_str5);
    
    pkg_free(selector);
    pkg_free(padded_str1); pkg_free(padded_str2); pkg_free(padded_str3); 
    pkg_free(padded_str4); pkg_free(padded_str5);
    
    return call_data;
}

// Callback function to write response data from CURL
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, struct ResponseData *response) {
    size_t realsize = size * nmemb;
    char *ptr = realloc(response->memory, response->size + realsize + 1);
    
    if (!ptr) {
        LM_ERR("Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    
    response->memory = ptr;
    memcpy(&(response->memory[response->size]), contents, realsize);
    response->size += realsize;
    response->memory[response->size] = 0;
    
    return realsize;
}

// Extract result from JSON response
char *extract_result(const char *json) {
    const char *pattern = "\"result\":\"";
    char *result_start = strstr(json, pattern);
    if (!result_start) return NULL;
    
    result_start += strlen(pattern);
    char *result_end = strchr(result_start, '"');
    if (!result_end) return NULL;
    
    size_t len = result_end - result_start;
    char *result = pkg_malloc(len + 1);
    if (!result) return NULL;
    
    memcpy(result, result_start, len);
    result[len] = '\0';
    return result;
}

// Strip trailing zeros from hash result (take first 32 hex chars)
void strip_trailing_zeros(const char* hex_result, char* stripped, size_t stripped_size) {
    if (!hex_result || strlen(hex_result) < 66) {
        strcpy(stripped, "");
        return;
    }
    
    size_t copy_len = 32;
    if (copy_len >= stripped_size) copy_len = stripped_size - 1;
    
    memcpy(stripped, hex_result + 2, copy_len);
    stripped[copy_len] = '\0';
}

// Extract authentication credentials from SIP message
static int extract_credentials(struct sip_msg* msg, sip_auth_t* auth) {
    struct hdr_field* h;
    struct auth_body* cred = NULL;
    
    // Parse all headers
    if (parse_headers(msg, HDR_EOH_F, 0) < 0) {
        LM_ERR("Error parsing headers\n");
        return -1;
    }
    
    // Look for Authorization or Proxy-Authorization header
    for (h = msg->headers; h; h = h->next) {
        if (h->type == HDR_AUTHORIZATION_T || h->type == HDR_PROXYAUTH_T) {
            cred = (struct auth_body*)h->parsed;
            if (!cred) {
                if (parse_authorization_header(msg, h) < 0) {
                    LM_ERR("Error parsing authorization header\n");
                    continue;
                }
                cred = (struct auth_body*)h->parsed;
            }
            if (cred && cred->digest.username.s) {
                break;
            }
        }
    }
    
    if (!cred || !cred->digest.username.s) {
        LM_ERR("No valid authorization header found\n");
        return -1;
    }
    
    // Extract credentials
    auth->username = cred->digest.username;
    auth->realm = cred->digest.realm;
    auth->uri = cred->digest.uri;
    auth->nonce = cred->digest.nonce;
    auth->response = cred->digest.response;
    
    // Set method from SIP message
    auth->method.s = msg->first_line.u.request.method.s;
    auth->method.len = msg->first_line.u.request.method.len;
    
    return 0;
}

// Make RPC call to verify authentication against blockchain
static int verify_sip_auth(const sip_auth_t* auth) {
    CURL *curl;
    CURLcode res;
    struct ResponseData response = {0};
    int auth_result = -1;
    
    // Convert str to null-terminated strings
    char username[MAX_FIELD_SIZE], realm[MAX_FIELD_SIZE], method[MAX_FIELD_SIZE];
    char uri[MAX_FIELD_SIZE], nonce[MAX_FIELD_SIZE], client_response[MAX_FIELD_SIZE];
    
    snprintf(username, sizeof(username), "%.*s", auth->username.len, auth->username.s);
    snprintf(realm, sizeof(realm), "%.*s", auth->realm.len, auth->realm.s);
    snprintf(method, sizeof(method), "%.*s", auth->method.len, auth->method.s);
    snprintf(uri, sizeof(uri), "%.*s", auth->uri.len, auth->uri.s);
    snprintf(nonce, sizeof(nonce), "%.*s", auth->nonce.len, auth->nonce.s);
    snprintf(client_response, sizeof(client_response), "%.*s", auth->response.len, auth->response.s);
    
    LM_INFO("Web3 Auth: username=%s, realm=%s, method=%s, uri=%s, nonce=%s\n", 
            username, realm, method, uri, nonce);
    
    // Encode call data (username, realm, method, uri, nonce)
    char* call_data = encode_digest_hash_call(username, realm, method, uri, nonce);
    if (!call_data) {
        LM_ERR("Error encoding call data\n");
        return -1;
    }
    
    // Initialize curl
    curl = curl_easy_init();
    if (!curl) {
        LM_ERR("Failed to initialize curl\n");
        pkg_free(call_data);
        return -1;
    }
    
    // Prepare JSON-RPC payload
    char* payload = pkg_malloc(8192);
    if (!payload) {
        curl_easy_cleanup(curl);
        pkg_free(call_data);
        return -1;
    }
    
    snprintf(payload, 8192,
        "{\"jsonrpc\":\"2.0\",\"method\":\"eth_call\",\"params\":[{\"to\":\"%s\",\"data\":\"0x%s\"},\"latest\"],\"id\":1}",
        contract_address, call_data);
    
    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, rpc_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    // Set headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Perform the request
    res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        LM_INFO("Blockchain response: %s\n", response.memory);
        
        // Check for error in response
        if (strstr(response.memory, "\"error\"")) {
            if (strstr(response.memory, "User not found")) {
                LM_WARN("User not found in contract - authorization rejected\n");
            } else {
                LM_ERR("Error getting digest hash from contract\n");
            }
            auth_result = -1;
        } else {
            // Extract result
            char *result_hex = extract_result(response.memory);
            if (result_hex) {
                LM_INFO("Raw blockchain result: %s\n", result_hex);
                
                // Strip trailing zeros (take first 32 hex chars)
                char expected_response[64];
                strip_trailing_zeros(result_hex, expected_response, sizeof(expected_response));
                
                LM_INFO("Expected response: %s, Client response: %s\n", expected_response, client_response);
                
                // Compare responses
                if (strcmp(expected_response, client_response) == 0) {
                    LM_INFO("Web3 authentication successful - responses match!\n");
                    auth_result = 1;
                } else {
                    LM_WARN("Web3 authentication failed - response mismatch\n");
                    auth_result = -1;
                }
                
                pkg_free(result_hex);
            } else {
                LM_ERR("Could not extract result from blockchain response\n");
                auth_result = -1;
            }
        }
        
        if (response.memory) free(response.memory);
    } else {
        LM_ERR("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        auth_result = -1;
    }
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    pkg_free(call_data);
    pkg_free(payload);
    
    return auth_result;
}

// Main authentication check function - called from Kamailio config
static int web3_auth_check(struct sip_msg* msg, char* p1, char* p2) {
    sip_auth_t auth = {0};
    
    LM_INFO("Web3 authentication check started\n");
    
    // Extract credentials from SIP message headers
    if (extract_credentials(msg, &auth) < 0) {
        LM_ERR("Failed to extract credentials from SIP message\n");
        return -1;
    }
    
    // Verify against blockchain
    return verify_sip_auth(&auth);
}

// Authentication check with specific realm parameter
static int web3_auth_with_realm(struct sip_msg* msg, char* realm_param, char* p2) {
    // For now, just call the main auth function
    // TODO: Implement realm-specific logic if needed
    return web3_auth_check(msg, NULL, NULL);
}

// Module initialization function
static int mod_init(void) {
    LM_INFO("Web3 Auth module initializing...\n");
    
    // Initialize curl globally
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        LM_ERR("Failed to initialize curl globally\n");
        return -1;
    }
    
    LM_INFO("Web3 Auth module initialized successfully\n");
    LM_INFO("Using RPC URL: %s\n", rpc_url);
    LM_INFO("Using contract address: %s\n", contract_address);
    
    return 0;
}

// Module cleanup function
static void mod_destroy(void) {
    LM_INFO("Web3 Auth module destroying...\n");
    curl_global_cleanup();
    LM_INFO("Web3 Auth module destroyed\n");
}

// Module parameters that can be set in kamailio.cfg
static param_export_t params[] = {
    {"rpc_url", PARAM_STRING, &rpc_url},
    {"contract_address", PARAM_STRING, &contract_address},
    {0, 0, 0}
};

// Module commands that can be called from kamailio.cfg
static cmd_export_t cmds[] = {
    {"web3_auth_check", (cmd_function)web3_auth_check, 0, 0, 0, 
     REQUEST_ROUTE | FAILURE_ROUTE},
    {"web3_auth_with_realm", (cmd_function)web3_auth_with_realm, 1, fixup_spve_null, 0,
     REQUEST_ROUTE | FAILURE_ROUTE},
    {0, 0, 0, 0, 0, 0}
};

// Module exports structure - defines what this module provides to Kamailio
struct module_exports exports = {
    "web3_auth",        /* module name */
    DEFAULT_DLFLAGS,    /* dlopen flags */
    cmds,               /* exported functions */
    params,             /* exported parameters */
    0,                  /* exported statistics */
    0,                  /* exported MI functions */
    0,                  /* exported pseudo-variables */
    0,                  /* extra processes */
    mod_init,           /* module initialization function */
    0,                  /* response function */
    mod_destroy,        /* destroy function */
    0                   /* child initialization function */
}; 