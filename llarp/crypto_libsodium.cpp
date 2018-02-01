#include <assert.h>
#include <llarp/crypto.h>
#include <sodium.h>

namespace llarp {
namespace sodium {
static bool xchacha20(llarp_buffer_t buff, llarp_sharedkey_t k,
                      llarp_nounce_t n) {
  uint8_t *base = (uint8_t *)buff.base;
  return crypto_stream_xchacha20_xor(base, base, buff.sz, n, k) == 0;
}

static bool dh(llarp_sharedkey_t *shared, uint8_t *client_pk,
               uint8_t *server_pk, uint8_t *remote_key, uint8_t *local_key) {
  uint8_t *out = *shared;
  const size_t outsz = SHAREDKEYSIZE;
  crypto_generichash_state h;
  if (crypto_scalarmult(out, local_key, remote_key) == -1) return false;
  crypto_generichash_init(&h, NULL, 0U, outsz);
  crypto_generichash_update(&h, client_pk, sizeof(llarp_pubkey_t));
  crypto_generichash_update(&h, server_pk, sizeof(llarp_pubkey_t));
  crypto_generichash_update(&h, out, crypto_scalarmult_BYTES);
  crypto_generichash_final(&h, out, outsz);
  return true;
}

static bool dh_client(llarp_sharedkey_t *shared, llarp_pubkey_t pk,
                      llarp_tunnel_nounce_t n, llarp_seckey_t sk) {
  llarp_pubkey_t local_pk;
  crypto_scalarmult_base(local_pk, sk);
  if (dh(shared, local_pk, pk, pk, sk)) {
    return crypto_generichash(*shared, SHAREDKEYSIZE, *shared, SHAREDKEYSIZE, n,
                              TUNNOUNCESIZE) != -1;
  }
  return false;
}

static bool dh_server(llarp_sharedkey_t *shared, llarp_pubkey_t pk,
                      llarp_tunnel_nounce_t n, llarp_seckey_t sk) {
  llarp_pubkey_t local_pk;
  crypto_scalarmult_base(local_pk, sk);
  if (dh(shared, pk, local_pk, pk, sk)) {
    return crypto_generichash(*shared, SHAREDKEYSIZE, *shared, SHAREDKEYSIZE, n,
                              TUNNOUNCESIZE) != -1;
  }
  return false;
}

static bool hash(llarp_hash_t *result, llarp_buffer_t buff) {
  const uint8_t *base = (const uint8_t *)buff.base;
  return crypto_generichash(*result, HASHSIZE, base, buff.sz, nullptr, 0) != -1;
}

static bool hmac(llarp_hash_t *result, llarp_buffer_t buff,
                 llarp_seckey_t secret) {
  const uint8_t *base = (const uint8_t *)buff.base;
  return crypto_generichash(*result, sizeof(llarp_hash_t), base, buff.sz,
                            secret, HMACSECSIZE) != -1;
}

static bool sign(llarp_sig_t *result, llarp_seckey_t secret,
                 llarp_buffer_t buff) {
  const uint8_t *base = (const uint8_t *)buff.base;
  return crypto_sign_detached(*result, nullptr, base, buff.sz, secret) != -1;
}

static bool verify(llarp_pubkey_t pub, llarp_buffer_t buff, llarp_sig_t sig) {
  const uint8_t *base = (const uint8_t *)buff.base;
  return crypto_sign_verify_detached(sig, base, buff.sz, pub) != -1;
}

static void randomize(llarp_buffer_t buff) {
  randombytes((unsigned char *)buff.base, buff.sz);
}

static void keygen(struct llarp_keypair *keys) {
  randombytes(keys->sec, sizeof(llarp_seckey_t));
  unsigned char sk[64];
  crypto_sign_seed_keypair(keys->pub, sk, keys->sec);
}
}  // namespace sodium
}  // namespace llarp

extern "C" {
void llarp_crypto_libsodium_init(struct llarp_crypto *c) {
  assert(sodium_init() != -1);
  c->xchacha20 = llarp::sodium::xchacha20;
  c->dh_client = llarp::sodium::dh_client;
  c->dh_server = llarp::sodium::dh_server;
  c->hash = llarp::sodium::hash;
  c->hmac = llarp::sodium::hmac;
  c->sign = llarp::sodium::sign;
  c->verify = llarp::sodium::verify;
  c->randomize = llarp::sodium::randomize;
  c->keygen = llarp::sodium::keygen;
}
}