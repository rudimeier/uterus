Synopsis
========

Set of tools to convert, merge, filter and export tick data files.

+ github page: <https://github.com/hroptatyr/uterus>
+ downloads: <https://bitbucket.org/hroptatyr/uterus/downloads>

Problem 1:
----------
I have a tonne of end of day MetaStock data.  I have this tool XYZ but
it expects data in OMZ format, one stock at a time.
I have found this converter ABC:
  http://www.example.com/ABC.html
but they discontinued the software and they don't respond to my emails.
Does anyone have any other converters or a copy of ABC? 


Problem 2:
----------
HELP PLEASE!!  I've just spent almost two days trying to import intraday
Metastock data into XYZ and I've given up.  Importing Metastock EOD data
is however very easy; but I don't know what the problem is when I try to
import 1-min or 5-mn intraday Metastock data into XYZ.


Problem 3:
----------
I am currently using a local real time provider that streams live data
to my Metastock Pro v8 Real Time Program but sometimes I lose my
internet connection and end up with gaps in my intraday charts.  However
I am able to get access to info showing course of trades for individual
stocks on a website.  I cut and paste this information into excel and
save it as an ASCII text file.  Quite a laborious task!  I then use
XYZ to convert the ASCII file into Metastock format but I get
errors that say: Warning #2052: No data from source file converted and
Error #2056: Invalid source file format.  I was wondering what the
problem is.


If you don't know what we're on about you'll be fine, you have no real
use for uterus and need not read on.  The uterus experiment attempts to
provide a concept to solve aforementioned problems.

It's the vision of a bunch of guys with a strong emphasis on data
integrity and maintainability.  We constantly moan about the lack of
standards, poor documentation of market-typical solutions and longevity
of established formats, well, if there is such a thing as an established
format.

While there are several good solutions out there to store massive
amounts of intraday tick data, there is not a single good way to
actually get the data there.  Every data provider comes up with their
own more or less suitable ad-hoc idea of a file exchange format.

Many solutions just work as advertised, no doubt about that, but, and
that's a big but, on a one-off basis!  Certainly it's fine to expect
that a local Australian dealer that provides ASX data will stamp their
ticks in AEST and AUD.  Fine, as long as you remember New South Wales'
daylight saving time rules.  And fine, as long as you remember that some
electricity futures are quoted in NZD.  Mixing such data with
Northern-American data requires you to put up with the oddities of
different time zones and currencies, at best your program allows you to
bring them into shape, i.e. convert them to a common time zone or
currency.  That's not an automatic process however and data from a
different data vendor needs different treatment.


DISCLAIMER:
-----------
Being deliberately vague, uterus promises nothing and keeps probably
even less.  It is irregularly maintained and by no means complete.

The ute file format
- provides a container for chronologically sorted quote and tick data
- allows to store meta data within the file (contract specs, etc.)
- is optimised for cli tools to automate common tasks

The ute tool set solves frequent tasks on the command line
- `mux' takes several other formats and converts them to ute
- `print' can generate output suitable for sql import, diff, grep, etc.
- `shnot' generates snapshots of the market at specific times
- `chandle' generates a market summary, candles of equal length
- `diff' shows differences between two or more ute files
- `grep' finds all ticks in an interval or all ticks of an instrument
