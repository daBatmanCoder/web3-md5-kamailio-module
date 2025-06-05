# Kamailio Web3 Authentication Module

A complete blockchain-based SIP authentication module for Kamailio that replaces traditional MD5 digest authentication with Web3 verification using smart contracts on the Oasis Sapphire testnet.

## 🚀 Overview

This module transforms your Kamailio SIP server to use blockchain-based authentication instead of traditional password databases. It maintains full SIP compatibility while adding the security and decentralization benefits of Web3 technology.

## ✨ Features

- **🔗 Blockchain Authentication**: Verifies SIP digest authentication against smart contracts
- **🔄 Drop-in Replacement**: Seamlessly replaces traditional `auth_db` functionality  
- **🛡️ Keccak-256 Hashing**: Native Ethereum-compatible hashing implementation
- **🌐 RPC Integration**: Direct connection to Oasis Sapphire testnet via JSON-RPC
- **📞 Standard SIP Support**: Works with all standard SIP digest authentication
- **⚡ High Performance**: Optimized for production SIP environments

## 🏗️ Architecture

```
SIP Client → Kamailio → Web3 Auth Module → Blockchain RPC → Smart Contract
     ↑                                                            ↓
     ←────────── Authentication Response ←──────────────────────────
```

1. **SIP Digest Extraction**: Module extracts credentials from SIP headers
2. **Smart Contract Call**: Constructs Ethereum function call to `getDigestHash(username, realm, method, uri, nonce)`
3. **Blockchain Verification**: Sends JSON-RPC call to Oasis Sapphire testnet
4. **Response Comparison**: Compares blockchain response with client's digest response
5. **Authentication Result**: Returns success/failure to Kamailio

## 📁 Repository Contents

| File | Description |
|------|-------------|
| `web3_auth_module.c` | Main Kamailio module source code |
| `Makefile` | Build configuration with multiple compilation options |
| `deploy.sh` | Automated deployment script |
| `README_MODULE.md` | Detailed technical documentation |
| `kamailio_web3_sample.cfg` | Complete sample Kamailio configuration |

## 🚀 Quick Start

### Prerequisites
```bash
sudo apt-get update
sudo apt-get install kamailio-dev libcurl4-openssl-dev build-essential
```

### Installation
```bash
# Clone the repository
git clone https://github.com/daBatmanCoder/web3-md5-kamailio-module.git
cd web3-md5-kamailio-module

# Deploy using automated script
chmod +x deploy.sh
./deploy.sh

# Or compile manually
make standalone
sudo cp web3_auth_module.so /usr/lib/x86_64-linux-gnu/kamailio/modules/web3_auth.so
```

### Configuration
Add to your `kamailio.cfg`:
```
# Load the Web3 authentication module
loadmodule "web3_auth.so"

# Configure blockchain parameters
modparam("web3_auth", "rpc_url", "https://testnet.sapphire.oasis.dev")
modparam("web3_auth", "contract_address", "0x1b55e67Ce5118559672Bf9EC0564AE3A46C41000")

# Replace authentication logic
route[REGISTER] {
    if(!has_credentials("digest")) {
        auth_challenge("$fd", "0");
        exit;
    }
    
    # Web3 blockchain authentication
    if(!web3_auth_check()) {
        auth_challenge("$fd", "0");
        exit;
    }
    
    save("location");
    exit;
}
```

## 🔧 Smart Contract Integration

The module calls this smart contract function:
```solidity
function getDigestHash(
    string memory username,
    string memory realm, 
    string memory method,
    string memory uri,
    string memory nonce
) public view returns (bytes32)
```

Contract Address: `0x1b55e67Ce5118559672Bf9EC0564AE3A46C41000`  
Network: Oasis Sapphire Testnet

## 📖 Documentation

- **[README_MODULE.md](README_MODULE.md)** - Complete technical documentation
- **[kamailio_web3_sample.cfg](kamailio_web3_sample.cfg)** - Full configuration example
- **Troubleshooting** - Common issues and solutions in README_MODULE.md

## 🔒 Security Features

- **HTTPS RPC**: Secure blockchain communication
- **Nonce Validation**: Prevents replay attacks
- **Error Handling**: Failed blockchain calls result in authentication rejection
- **Memory Safety**: Proper memory management using Kamailio's allocators

## 🎯 Use Cases

- **Decentralized SIP Networks**: Remove central authentication dependencies
- **Web3 VoIP Services**: Integrate blockchain identity with SIP communications
- **Secure Enterprise Communications**: Leverage blockchain for enhanced security
- **Hybrid Authentication**: Combine traditional and blockchain-based auth

## 🚀 Performance

- **Network Latency**: ~100-500ms per auth check (blockchain RPC call)
- **Concurrent Handling**: Efficient libcurl-based concurrent request processing
- **Memory Usage**: Optimized for high-volume SIP environments
- **Timeout Handling**: 10-second default timeout with proper error handling

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the same terms as Kamailio.

## 🔗 Links

- [Kamailio](https://www.kamailio.org/) - The leading open source SIP server
- [Oasis Sapphire](https://oasisprotocol.org/) - Privacy-enabled blockchain platform
- [SIP RFC 3261](https://tools.ietf.org/html/rfc3261) - Session Initiation Protocol specification

## 🆘 Support

For issues and questions:
- Check the [troubleshooting section](README_MODULE.md#troubleshooting) in README_MODULE.md
- Open an issue in this repository
- Test with the standalone test program first

---

**Made with ❤️ for the decentralized future of communications** 