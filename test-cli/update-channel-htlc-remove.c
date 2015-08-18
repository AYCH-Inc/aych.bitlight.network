#include <ccan/crypto/shachain/shachain.h>
#include <ccan/short_types/short_types.h>
#include <ccan/tal/tal.h>
#include <ccan/opt/opt.h>
#include <ccan/str/hex/hex.h>
#include <ccan/err/err.h>
#include <ccan/read_write_all/read_write_all.h>
#include "lightning.pb-c.h"
#include "bitcoin/base58.h"
#include "pkt.h"
#include "bitcoin/script.h"
#include "permute_tx.h"
#include "bitcoin/signature.h"
#include "commit_tx.h"
#include "bitcoin/pubkey.h"
#include "find_p2sh_out.h"
#include "protobuf_convert.h"
#include "version.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
	const tal_t *ctx = tal_arr(NULL, char, 0);
	struct sha256 seed, htlc_rhash, revocation_hash;
	struct pkt *pkt;
	unsigned update_num;
	UpdateAddHtlc *u;
	bool routefail = false;

	err_set_progname(argv[0]);

	opt_register_noarg("--help|-h", opt_usage_and_exit,
			   "<seed> <update-number> <update-pkt>\n"
			   "Create a new HTLC (timedout) remove message",
			   "Print this message.");
	opt_register_noarg("--routefail", opt_set_bool, &routefail,
			   "Generate a routefail instead of timedout");
	opt_register_version();

 	opt_parse(&argc, argv, opt_log_stderr_exit);

	if (argc != 4)
		opt_usage_exit_fail("Expected 3 arguments");

	if (!hex_decode(argv[1], strlen(argv[1]), &seed, sizeof(seed)))
		errx(1, "Invalid seed '%s' - need 256 hex bits", argv[1]);
	update_num = atoi(argv[2]);
	if (!update_num)
		errx(1, "Update number %s invalid", argv[2]);

	u = pkt_from_file(argv[3], PKT__PKT_UPDATE_ADD_HTLC)->update_add_htlc;
	proto_to_sha256(u->r_hash, &htlc_rhash);
	
	/* Get next revocation hash. */
	shachain_from_seed(&seed, update_num, &revocation_hash);
	sha256(&revocation_hash,
	       revocation_hash.u.u8, sizeof(revocation_hash.u.u8));

	if (routefail)
		pkt = update_htlc_routefail_pkt(ctx, &revocation_hash,
						&htlc_rhash);
	else
		pkt = update_htlc_timedout_pkt(ctx, &revocation_hash,
					       &htlc_rhash);
	if (!write_all(STDOUT_FILENO, pkt, pkt_totlen(pkt)))
		err(1, "Writing out packet");

	tal_free(ctx);
	return 0;
}

