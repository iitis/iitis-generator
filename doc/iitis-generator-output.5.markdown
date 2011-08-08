iitis-generator-output(5) -- iitis-generator file-system output
===============================================================

## SYNOPSIS

	2011.07/
	*-- session-name/
	    *--- 2011.07.26-15:40:25/
	        *-- 1/
	        |   *-- internal-stats.txt
	        |   *-- linestats.txt
	        |   *-- mon0/
	        |   |   *-- interface.txt
	        |   |   *-- link-2->1.txt
	        |   *-- mon1/
	        |   |   *-- interface.txt
	        *-- 2/
	        |   *-- internal-stats.txt
	        |   *-- linestats.txt
	        |   *-- mon0/
	        |   |   *-- dump.pcap
	        |   |   *-- interface.txt
	        |   |   *-- link-1->2.txt
	        *-- generator.conf
	        *-- traffic.txt

## DESCRIPTION

iitis-generator(1) is a tool for distributed, statistical evaluation of wireless networks. Each
network node that takes part in an evaluation experiment runs the `iitis-generator` program. This
manual page documents the structure of its file-system output and format of constituent files.

## OUTPUT STRUCTURE

`iitis-generator` has a concept of root directory for the program output. This property can be set
using either command-line or configuration file option named "root".

On each run of `iitis-generator`, after the synchronization phase, a five-level directory structure
is initialized:

	origin-Y.M/                      (1)
	*-- session-name/                (2)
	    *--- origin-Y.M.D-H:M:S/     (3)
	        *-- node-id/             (4)
	          *-- interface ...      (5)

With the following meaning:

  * `origin-Y.M` is the year and month of the origin - e.g. "2007.07"
  * `session-name` is the session name
  * `origin-Y.M.D-H:M:S` is the full date and time of the origin - e.g. "2011.07.26-15:40:25"
  * `node-id` is the ID number of the current node
  * `interface` is the name of network interface, e.g. "mon0"

See iitis-generator(1) for the concepts of session name and origin.

In case session name is not set, level (2) is skipped. Several directories on level (5) are created
- each test network interface has its own directory. Also, each node will create its own directory
on level (4). Running `iitis-generator` with the same session name many times will result in many
directories on level (3).

## OUTPUT CONTENTS

Both the traffic and the config files are copied as-is to level (3).

On level (4), following files are created:

  * `internal-stats.txt`: statistics of the `iitis-generator` internals
  * `linestats.txt`: aggregated statistics of all lines from the traffic file

On level (5), following files may be created:

 * `interface.txt`: statistics of network interface
 * `link-X->Y.txt`: statistics for traffic coming from node X to Y
 * `dump.pcap`: PCAP file with dumped interface traffic
 * and other statistic files

## STATISTIC FILES

Statistics are stored in text files. Data is organized in a tabular form. First line holds column
names and always starts with "#time ...". Below is a short example:

	#time rcv_ok rcv_ok_bytes rcv_dup rcv_dup_bytes rcv_lost rssi rate antnum
	1 1 1500 0 0 0 -7 4.90909 0
	2 2 3000 0 0 0 -17.3388 12.2119 0
	3 1 1500 0 0 0 -21.1863 14.9006 0
	4 1 1500 0 0 0 -24.3343 17.1005 0
	5 1 1500 0 0 0 -26.9099 18.9004 0

First column gives time since the origin. Besides, there are two kinds of columns:

  * `counter`: an integer, counts occurances, bytes, etc.; it is set back to 0 after writing its
    value to a statistics file, so actually the values found in statistics are their first derivatives
    in time
  * `gauge`: a real number, simply gives the current value; in case column value is an aggregate
    constructed off several other gauges, an EWMA value is given

## AUTHOR AND COPYRIGHT INFO

`iitis-generator` was written by Pawel Foremski <pjf@iitis.pl>. Copyright (C) 2011 IITiS PAN Gliwice
<http://www.iitis.pl/>.

## SEE ALSO

iitis-generator(1), iitis-generator-conf(5), iitis-generator-traffic(5), [IITiS WiFi
lab](https://sites.google.com/site/iitiswifilab/)
