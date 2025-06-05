# Kamailio Web3 Authentication Module

This module provides blockchain-based SIP authentication for Kamailio, replacing traditional MD5 digest authentication with Web3 verification using the Oasis Sapphire testnet.

## Features

- **Blockchain Authentication**: Verifies SIP digest authentication against smart contracts
- **Drop-in Replacement**: Replaces traditional auth_db functionality
- **Keccak-256 Hashing**: Implements native Keccak-256 for Ethereum compatibility
- **RPC Integration**: Connects to Oasis Sapphire testnet via JSON-RPC
- **Standard SIP Support**: Works with standard SIP digest authentication headers

## Prerequisites

### System Requirements

- Kamailio development environment
- libcurl development headers
- GCC compiler with C99 support

### Install Dependencies

#### Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install kamailio-dev libcurl4-openssl-dev build-essential
```

#### CentOS/RHEL:
```bash
sudo yum install kamailio-devel libcurl-devel gcc make
```

## Compilation

### Method 1: Standalone Compilation (Recommended for Testing)

```bash
# Navigate to the module directory
cd kamailio-web3-auth

# Compile standalone module
make standalone

# This creates web3_auth_module.so
```

### Method 2: Full Kamailio Build Environment

If you have the full Kamailio source:

```bash
# Copy module to Kamailio modules directory
cp web3_auth_module.c /path/to/kamailio/src/modules/web3_auth/
cp Makefile /path/to/kamailio/src/modules/web3_auth/

# Build from Kamailio root
cd /path/to/kamailio
make modules include_modules="web3_auth"
```

### Method 3: Test Compilation Only

```bash
# Test compilation without linking
make test-compile
```

## Installation

### Manual Installation

```bash
# Copy the compiled module to Kamailio modules directory
sudo cp web3_auth_module.so /usr/lib/x86_64-linux-gnu/kamailio/modules/web3_auth.so

# Or for other architectures, find your modules directory:
# kamailio -I | grep "module path"
```

### Using Make Install

```bash
# If compiled with full Kamailio environment
make install
```

## Configuration

### Load the Module

Add to your `kamailio.cfg`:

```
# Load the Web3 authentication module
loadmodule "web3_auth.so"

# Configure module parameters
modparam("web3_auth", "rpc_url", "https://testnet.sapphire.oasis.dev")
modparam("web3_auth", "contract_address", "0x1b55e67Ce5118559672Bf9EC0564AE3A46C41000")
```

### Module Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `rpc_url` | string | "https://testnet.sapphire.oasis.dev" | Blockchain RPC endpoint |
| `contract_address` | string | "0x1b55e67Ce5118559672Bf9EC0564AE3A46C41000" | Smart contract address |

### Replace Authentication Logic

#### Traditional Auth (Before):
```
# Old MD5-based authentication
if(!auth_check("$fd", "subscriber", "1")) {
    auth_challenge("$fd", "0");
    exit;
}
```

#### Web3 Auth (After):
```
# New blockchain-based authentication
if(!web3_auth_check()) {
    auth_challenge("$fd", "0");
    exit;
}
```

## Usage Examples

### Basic REGISTER Authentication

```
route[REGISTER] {
    if(!is_method("REGISTER")) return;

    # Challenge if no credentials provided
    if(!has_credentials("digest")) {
        auth_challenge("$fd", "0");
        exit;
    }

    # Web3 authentication check
    if(!web3_auth_check()) {
        xlog("L_WARN", "Web3 auth failed for $fU@$fd\n");
        auth_challenge("$fd", "0");
        exit;
    }

    # Save location if authenticated
    save("location");
    exit;
}
```

### Authentication with Specific Realm

```
# Use specific realm for authentication
if(!web3_auth_with_realm("sip.company.com")) {
    auth_challenge("sip.company.com", "0");
    exit;
}
```

### Complete Configuration Example

See `kamailio_web3_sample.cfg` for a complete working configuration.

## How It Works

1. **SIP Digest Extraction**: Module extracts digest authentication credentials from SIP headers
2. **Smart Contract Call**: Constructs Ethereum function call to `getDigestHash(username, realm, method, uri, nonce)`
3. **Blockchain Verification**: Sends JSON-RPC call to Oasis Sapphire testnet
4. **Response Comparison**: Compares blockchain response with client's digest response
5. **Authentication Result**: Returns success (1) or failure (-1) to Kamailio

## Smart Contract Integration

The module calls the following smart contract function:
```solidity
function getDigestHash(
    string memory username,
    string memory realm, 
    string memory method,
    string memory uri,
    string memory nonce
) public view returns (bytes32)
```

## Troubleshooting

### Common Issues

#### Module Load Error
```
ERROR: <core> [core/sr_module.c:514]: load_module(): could not open module </usr/lib/x86_64-linux-gnu/kamailio/modules/web3_auth.so>
```
**Solution**: Check module path and ensure libcurl is installed.

#### Compilation Errors
```bash
# Missing headers
sudo apt-get install kamailio-dev libcurl4-openssl-dev

# Check if Kamailio development files are available
pkg-config --cflags kamailio
```

#### Curl Initialization Failed
```
ERROR: web3_auth [web3_auth_module.c:XXX]: Failed to initialize curl globally
```
**Solution**: Ensure libcurl is properly installed and linked.

### Debug Logging

Enable detailed logging in `kamailio.cfg`:
```
debug=3
log_stderror=yes
```

Monitor logs:
```bash
tail -f /var/log/kamailio.log | grep web3_auth
```

### Testing the Module

Use the standalone test program:
```bash
# Compile test program
gcc -o test_web3_auth test_web3_auth.c -lcurl

# Test with sample credentials
./test_web3_auth 'username="testuser",realm="sip.example.com",uri="sip:sip.example.com",nonce="1234567890abcdef",response="expectedhash"'
```

## Performance Considerations

- **Network Latency**: Each auth check requires blockchain RPC call (~100-500ms)
- **Caching**: Consider implementing result caching for repeated nonce values
- **Timeout**: Default curl timeout is 10 seconds
- **Concurrent Calls**: libcurl handles concurrent requests efficiently

## Security Notes

- **HTTPS RPC**: Always use HTTPS for blockchain RPC endpoints
- **Nonce Validation**: Ensure nonces are properly validated to prevent replay attacks
- **Error Handling**: Failed blockchain calls result in authentication rejection
- **Logging**: Avoid logging sensitive authentication data

## Development

### Building from Source

```bash
git clone [repository-url]
cd kamailio-web3-auth
make standalone
```

### Module Structure

- `web3_auth_module.c`: Main module implementation
- `Makefile`: Build configuration
- `kamailio_web3_sample.cfg`: Sample configuration
- `test_web3_auth.c`: Test program

### Contributing

1. Fork the repository
2. Create feature branch
3. Add tests for new functionality
4. Submit pull request

## License

This module is distributed under the same license as Kamailio.

## Support

- Check logs for detailed error messages
- Verify blockchain RPC connectivity
- Ensure smart contract is deployed and accessible
- Test with standalone test program first

For issues and questions, refer to the project repository or Kamailio community forums. 