#include "aurora_extreme.hpp"
#include "aurora_intention.hpp"
#include <iostream>
#include <cstring>

int main() {
    // Test CRYPTO Ed25519 reale
    uint8_t pk[32], sk[64], sig[64];
    const char* msg = "Messaggio di test";
    size_t msg_len = strlen(msg);

    CRYPTO::ed25519_keypair(pk, sk);
    CRYPTO::ed25519_sign(sig, (const uint8_t*)msg, msg_len, sk);

    // Verifica corretta
    bool ok = CRYPTO::ed25519_verify(sig, (const uint8_t*)msg, msg_len, pk);
    std::cout << "Verifica firma: " << (ok ? "OK" : "FALLITA") << std::endl;

    // Modifica il messaggio (test negativo)
    const char* msg2 = "Messaggio alterato";
    bool ok2 = CRYPTO::ed25519_verify(sig, (const uint8_t*)msg2, strlen(msg2), pk);
    std::cout << "Verifica firma su messaggio alterato: " << (ok2 ? "OK" : "FALLITA") << std::endl;

    // Modifica la firma (test negativo)
    sig[0] ^= 0xFF;
    bool ok3 = CRYPTO::ed25519_verify(sig, (const uint8_t*)msg, msg_len, pk);
    std::cout << "Verifica firma alterata: " << (ok3 ? "OK" : "FALLITA") << std::endl;

    // Test base64
    std::string b64 = CRYPTO::b64(pk, 32);
    std::cout << "Public key base64: " << b64 << std::endl;

    return 0;
}
