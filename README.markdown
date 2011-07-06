User Notes
==========

* Take care not to set the frequency such that `iw info' command shows "(radar detection)" next to
  it. There are WiFi channels for which frame injection is banned by the law.
* If using fixed transmission rate (as opposite to automatic selection by software), make sure the
  chosen bitrate is supported. Use `iw phy` and look for the "Bitrates" section in the output.
  * For the 2.4 GHz band, supported bitrates usually are: 1, 2, 5.5, 11, 6, 9, 12, 18, 24, 36, 48, 54
  * For the 5 GHz band, these are: 6, 9, 12, 18, 24, 36, 48, 54

Shortfalls
==========

* On RB433AH, with mac80211 packet injection, generator can do at most ~2500 pps (100 B frames) or no
  more than 25 Mbps (1500 B frames)
  * Simultaneous, parallel sendmsg() writes do not improve the performance - my guess is an
    architectural problem in packet injection facility that would require deeper investigation down
    the kernel, the mac80211 layer and the ath9k driver
  * A limited (in functionality) workaround would be to use the raw Ethernet or even UDP socket
    interface and rely on the Linux networking stack equipped with many layers of buffers
    * Would disable/degrade accurate control over frame timing, bitrate and ack

