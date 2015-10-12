#include "bitcoin/pubkey.h"
#include "bitcoin/signature.h"
#include "protobuf_convert.h"
#include <ccan/crypto/sha256/sha256.h>

Signature *signature_to_proto(const tal_t *ctx, const struct signature *sig)
{
	Signature *pb = tal(ctx, Signature);
	signature__init(pb);

	assert(sig_valid(sig));

#ifdef USE_SCHNORR
	memcpy(&pb->r1, sig->schnorr, 8);
	memcpy(&pb->r2, sig->schnorr + 8, 8);
	memcpy(&pb->r3, sig->schnorr + 16, 8);
	memcpy(&pb->r4, sig->schnorr + 24, 8);
	memcpy(&pb->s1, sig->schnorr + 32, 8);
	memcpy(&pb->s2, sig->schnorr + 40, 8);
	memcpy(&pb->s3, sig->schnorr + 48, 8);
	memcpy(&pb->s4, sig->schnorr + 56, 8);
#else
	/* FIXME: Need a portable way to encode signatures in libsecp! */
	
	/* Kill me now... */
	memcpy(&pb->r1, sig->sig.data, 8);
	memcpy(&pb->r2, sig->sig.data + 8, 8);
	memcpy(&pb->r3, sig->sig.data + 16, 8);
	memcpy(&pb->r4, sig->sig.data + 24, 8);
	memcpy(&pb->s1, sig->sig.data + 32, 8);
	memcpy(&pb->s2, sig->sig.data + 40, 8);
	memcpy(&pb->s3, sig->sig.data + 48, 8);
	memcpy(&pb->s4, sig->sig.data + 56, 8);
#endif
	
	return pb;
}

bool proto_to_signature(const Signature *pb, struct signature *sig)
{
	/* Kill me again. */
#ifdef USE_SCHNORR
	memcpy(sig->schnorr, &pb->r1, 8);
	memcpy(sig->schnorr + 8, &pb->r2, 8);
	memcpy(sig->schnorr + 16, &pb->r3, 8);
	memcpy(sig->schnorr + 24, &pb->r4, 8);
	memcpy(sig->schnorr + 32, &pb->s1, 8);
	memcpy(sig->schnorr + 40, &pb->s2, 8);
	memcpy(sig->schnorr + 48, &pb->s3, 8);
	memcpy(sig->schnorr + 56, &pb->s4, 8);
#else
	/* FIXME: Need a portable way to encode signatures in libsecp! */
	
	memcpy(sig->sig.data, &pb->r1, 8);
	memcpy(sig->sig.data + 8, &pb->r2, 8);
	memcpy(sig->sig.data + 16, &pb->r3, 8);
	memcpy(sig->sig.data + 24, &pb->r4, 8);
	memcpy(sig->sig.data + 32, &pb->s1, 8);
	memcpy(sig->sig.data + 40, &pb->s2, 8);
	memcpy(sig->sig.data + 48, &pb->s3, 8);
	memcpy(sig->sig.data + 56, &pb->s4, 8);
#endif

	return sig_valid(sig);
}

BitcoinPubkey *pubkey_to_proto(const tal_t *ctx, const struct pubkey *key)
{
	BitcoinPubkey *p = tal(ctx, BitcoinPubkey);
	struct pubkey check;

	bitcoin_pubkey__init(p);
	p->key.len = pubkey_derlen(key);
	p->key.data = tal_dup_arr(p, u8, key->der, p->key.len, 0);

	assert(pubkey_from_der(p->key.data, p->key.len, &check));
	assert(pubkey_eq(&check, key));
	return p;
}

bool proto_to_pubkey(const BitcoinPubkey *pb, struct pubkey *key)
{
	return pubkey_from_der(pb->key.data, pb->key.len, key);
}

Sha256Hash *sha256_to_proto(const tal_t *ctx, const struct sha256 *hash)
{
	Sha256Hash *h = tal(ctx, Sha256Hash);
	sha256_hash__init(h);

	/* Kill me now... */
	memcpy(&h->a, hash->u.u8, 8);
	memcpy(&h->b, hash->u.u8 + 8, 8);
	memcpy(&h->c, hash->u.u8 + 16, 8);
	memcpy(&h->d, hash->u.u8 + 24, 8);
	return h;
}

void proto_to_sha256(const Sha256Hash *pb, struct sha256 *hash)
{
	/* Kill me again. */
	memcpy(hash->u.u8, &pb->a, 8);
	memcpy(hash->u.u8 + 8, &pb->b, 8);
	memcpy(hash->u.u8 + 16, &pb->c, 8);
	memcpy(hash->u.u8 + 24, &pb->d, 8);
}

static bool proto_to_locktime(const Locktime *l, uint32_t off,
			      uint32_t *locktime)
{
	switch (l->locktime_case) {
	case LOCKTIME__LOCKTIME_SECONDS:
		*locktime = off + l->seconds;
		/* Check for wrap, or too low value */
		if (*locktime < 500000000)
			return false;
		break;
	case LOCKTIME__LOCKTIME_BLOCKS:
		if (l->blocks >= 500000000)
			return false;
		*locktime = l->blocks;
		break;
	default:
		return false;
	}
	return true;
}

bool proto_to_rel_locktime(const Locktime *l, uint32_t *locktime)
{
	return proto_to_locktime(l, 500000000, locktime);
}

bool proto_to_abs_locktime(const Locktime *l, uint32_t *locktime)
{
	return proto_to_locktime(l, 0, locktime);
}
