/*
 * ath9k monitor mode packet generator
 * Paweł Foremski <pforemski@gmail.com> 2011
 * IITiS PAN Gliwice
 */

#include <netinet/ether.h>
#include <libpjf/main.h>

#include "generator.h"
#include "inject.h"

int main(int argc, char *argv[])
{
	mmatic *mm = mmatic_create();
	mmatic *mmtmp = mmatic_create();
	struct mg *mg;

	mg = mmalloc(sizeof(struct mg));
	mg->mm = mm;
	mg->mmtmp = mmtmp;

	if (mgi_init(mg) <= 0)
		die("no available interfaces found");

	struct ether_addr bssid = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
	struct ether_addr dst   = { 0x00, 0x00, 0x00, 0x00, 0x02, 0x00 };
	struct ether_addr src   = { 0x00, 0x00, 0x00, 0x00, 0x01, 0x00 };
	mgi_inject(mg, 0, &bssid, &dst, &src, "ALA MA KOTA", 11);

	mmatic_free(mm);
	return 0;
}
