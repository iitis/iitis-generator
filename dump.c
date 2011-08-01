/*
 * Paweł Foremski <pjf@iitis.pl> 2011
 * IITiS PAN Gliwice
 */

#include "dump.h"
#include "generator.h"

void mgd_dump(struct sniff_pkt *pkt)
{
	struct interface *interface = pkt->interface;
	struct mg *mg = interface->mg;
	char *dumpdir, *dumpfile;
	pcap_hdr_t ph;
	pcaprec_hdr_t pp;
	FILE *fp;
	int snaplen, inclen;

	if (mg->options.stats == 0 || !mg->stats_dir)
		return;

	if (mg->options.dumpsize) {
		snaplen = mg->options.dumpsize;
		inclen = MIN(mg->options.dumpsize, pkt->len);
	}

	fp = interface->dumpfile;
	if (!fp) {
		dumpdir  = mmatic_printf(mg->mmtmp, "%s/%s", mg->stats_dir, interface->name);
		dumpfile = mmatic_printf(mg->mmtmp, "%s/dump.pcap", dumpdir);

		pjf_mkdir_mode(dumpdir, mg->options.world ? 0777 : 0755);
		fp = fopen(dumpfile, "w");
		if (!fp) {
			dbg(0, "cant dump frames: writing to '%s' failed\n", dumpfile);
			return;
		}

		interface->dumpfile = fp;

		/* write global header */
		ph.magic_number  = PCAP_MAGIC_NUMBER;
		ph.version_major = 2;
		ph.version_minor = 4;
		ph.thiszone      = 0;
		ph.sigfigs       = 0;
		ph.snaplen       = snaplen;
		ph.network       = 127; /* LINKTYPE_IEEE802_11_RADIO, see http://www.tcpdump.org/linktypes.html */
		fwrite(&ph, sizeof ph, 1, fp);
	}

	/* write packet header */
	pp.ts_sec   = pkt->timestamp.tv_sec;
	pp.ts_usec  = pkt->timestamp.tv_usec;
	pp.incl_len = inclen;
	pp.orig_len = pkt->len;
	fwrite(&pp, sizeof pp, 1, fp);

	/* write packet */
	fwrite(&pkt->pkt, inclen, 1, fp);
}
