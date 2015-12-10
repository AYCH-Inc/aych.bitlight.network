#include "bitcoin/privkey.h"
#include "bitcoin/shadouble.h"
#include "bitcoin/signature.h"
#include "lightningd.h"
#include "log.h"
#include "peer.h"
#include "secrets.h"
#include <ccan/crypto/sha256/sha256.h>
#include <ccan/crypto/shachain/shachain.h>
#include <ccan/mem/mem.h>
#include <ccan/noerr/noerr.h>
#include <ccan/read_write_all/read_write_all.h>
#include <ccan/short_types/short_types.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/rand.h>
#include <secp256k1.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct secret {
	/* Secret ID of our node; public is state->id. */
	struct privkey privkey;
};

void privkey_sign(struct peer *peer, const void *src, size_t len,
		  struct signature *sig)
{
	struct sha256_double h;

	sha256_double(&h, memcheck(src, len), len);
	if (!sign_hash(&peer->state->secret->privkey, &h, sig))
		fatal("Failed to sign %zu bytes", len);
}

struct peer_secrets {
	/* Two private keys, one for commit txs, one for final output. */
	struct privkey commit, final;
	/* Seed from which we generate revocation hashes. */
	struct sha256 revocation_seed;
};

static void new_keypair(struct lightningd_state *state,
			struct privkey *privkey, struct pubkey *pubkey)
{
	do {
		if (RAND_bytes(privkey->secret, sizeof(privkey->secret)) != 1)
			fatal("Could not get random bytes for privkey");
	} while (!pubkey_from_privkey(privkey, pubkey, SECP256K1_EC_COMPRESSED));
}

void peer_secrets_init(struct peer *peer)
{
	peer->secrets = tal(peer, struct peer_secrets);

	new_keypair(peer->state, &peer->secrets->commit, &peer->our_commitkey);
	new_keypair(peer->state, &peer->secrets->final, &peer->our_finalkey);
	if (RAND_bytes(peer->secrets->revocation_seed.u.u8,
		       sizeof(peer->secrets->revocation_seed.u.u8)) != 1)
		fatal("Could not get random bytes for revocation seed");
}

void peer_get_revocation_preimage(const struct peer *peer, u64 index,
				  struct sha256 *preimage)
{
	shachain_from_seed(&peer->secrets->revocation_seed, index, preimage);
}
	
void peer_get_revocation_hash(const struct peer *peer, u64 index,
			      struct sha256 *rhash)
{
	struct sha256 preimage;

	peer_get_revocation_preimage(peer, index, &preimage);
	sha256(rhash, preimage.u.u8, sizeof(preimage.u.u8));
}

void secrets_init(struct lightningd_state *state)
{
	int fd;

	state->secret = tal(state, struct secret);

	fd = open("privkey", O_RDONLY);
	if (fd < 0) {
		if (errno != ENOENT)
			fatal("Failed to open privkey: %s", strerror(errno));

		log_unusual(state->base_log, "Creating privkey file");
		new_keypair(state, &state->secret->privkey, &state->id);

		fd = open("privkey", O_CREAT|O_EXCL|O_WRONLY, 0400);
		if (fd < 0)
		 	fatal("Failed to create privkey file: %s",
			      strerror(errno));
		if (!write_all(fd, state->secret->privkey.secret,
			       sizeof(state->secret->privkey.secret))) {
			unlink_noerr("privkey");
		 	fatal("Failed to write to privkey file: %s",
			      strerror(errno));
		}
		if (fsync(fd) != 0)
		 	fatal("Failed to sync to privkey file: %s",
			      strerror(errno));
		close(fd);

		fd = open("privkey", O_RDONLY);
		if (fd < 0)
			fatal("Failed to reopen privkey: %s", strerror(errno));
	}
	if (!read_all(fd, state->secret->privkey.secret,
		      sizeof(state->secret->privkey.secret)))
		fatal("Failed to read privkey: %s", strerror(errno));
	close(fd);
	if (!pubkey_from_privkey(&state->secret->privkey, &state->id,
				 SECP256K1_EC_COMPRESSED))
		fatal("Invalid privkey");

	log_info(state->base_log, "ID: ");
	log_add_hex(state->base_log, state->id.der, pubkey_derlen(&state->id));
}
