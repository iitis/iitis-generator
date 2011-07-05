Notes
=====

* Take care not to set the frequency such that `iw info' command shows "(radar detection)" next to
  it. There are WiFi channels for which frame injection is banned by the law.
* If using fixed transmission rate (as opposite to automatic selection by software), make sure the
  chosen bitrate is supported. Use `iw phy` and look for the "Bitrates" section in the output.
  * For the 2.4 GHz band, supported bitrates usually are: 1, 2, 5.5, 11, 6, 9, 12, 18, 24, 36, 48, 54
  * For the 5 GHz band, these are: 6, 9, 12, 18, 24, 36, 48, 54
