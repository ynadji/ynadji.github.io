+++
title = "The Cure for Exceptional Zeek Package Testing (Part 3)"
tags = [
    "zeek",
    "btest",
    "testing",
    "packages",
    "input-framework",
]
categories = [
    "testing",
    "open-source",
]
date = "2026-08-23"
draft = false
+++

_This article was originally posted on [Zeek's
blog](https://zeek.org/2026/07/the-cure-for-exceptional-zeek-package-testing-part-3)._

# Introduction

Back in [Part 1]({{< ref "exceptional-zeek-package-testing-part-1" >}}) and
[Part 2]({{< ref "exceptional-zeek-package-testing-part-2" >}}), we presented
our motivational package,
[`ip-distance`](https://github.com/corelight/ip-distance), how you can easily
write unit tests in [BTest](https://github.com/zeek/btest), and how you can
generate events—either by hand or through Zeek's event tracing utility—to test
your Zeek content. However, there's another case I'd like to cover where you can
use Zeek's ability to read its own logs using the Input framework to exercise
your package's functionality. This final part of our blog series covers #4
below:

1. Writing good unit tests for auxiliary functions.
2. Hand-crafting events to exercise your package's functionality.
3. Automatically generating events from a PCAP using Zeek's built-in event
   tracing flag.
4. **Using a Zeek log and the Input Framework to drive your package's functionality.**

Not only do we show how this can be used it BTests, but we also show how it can
be used to do longitudinal analysis of your Zeek scripts in the event you have
the requisite log files on disk. Let's dive in.

# Log-driven

After shipping one of my Zeek packages, I started to notice a few false
negatives appearing on some of our test sensors. Unfortunately, these only
cropped up after weeks or months of the package running, so getting a small PCAP
to reproduce was unfeasible. Luckily, a few properties of my situation led me to
a solution:

- I _did_ have access to months' worth of historical Zeek logs where the
  problems appeared.
- The Zeek package in question only relied on a few fields in the conn log.
- Zeek's Input Framework's default reader `Input::READER_ASCII` ingests Zeek TSV
  formatted logs.

Once I had analyzed weeks of "traffic" from the logs, I was able to find a
single log line that caused a false negative. Armed with this, I realized I
could use the same technique to build a test for this to ensure another
regression of the same type wouldn't occur. This test case is simpler to
present, but I will also describe the debugging approach in more detail in
[Longitudinal Analysis](#longitudinal-analysis) for interested parties.

So how can we consume logs with the Input Framework? I had only used the Input
Framework to ingest TSV formatted input files into a Zeek `table` for use in
scripts. But there's another option to [read data into
events](https://docs.zeek.org/en/v8.2.0/frameworks/input.html#reading-data-to-events)
that would work. We use `Input::add_event` to read our log, which in turn uses
an event handler to fire the `connection_state_remove` event we did in our prior
event-driven examples. This test is a bit more involved, as it requires knowing
a bit more about BTest as well as how the Input Framework works. Let's dive in.

First, we need to have a conn log and alter the `btest.cfg` to make the log
easily referenceable by the test itself. We generate a conn log from the PCAP we
used in [Part 2]({{< ref "exceptional-zeek-package-testing-part-2" >}})'s Event
Traces section and place it in
[`testing/Files/conn.log`](https://github.com/corelight/ip-distance/blob/main/testing/Files/conn.log?ref_type=heads).
Next, we add line 12 in our `testing/btest.cfg` file (Listing 1), which creates
an environment variable `${FILES}` we can reference in all of our BTests.
Finally, we add an additional keyword to our BTest preamble on line 2 of
`log-driver.zeek` (Listing 2). `@TEST-COPY-FILE` copies the file from the
specified path to the current working directory while executing the test. This
way, we can refer to the file simply as `"conn.log"` in our test.

**Listing 1:** Our
[`testing/btest.cfg`](https://github.com/corelight/ip-distance/blob/main/testing/btest.cfg?ref_type=heads)
file.

```ini {linenos=true}
[btest]
TestDirs    = ipdistance
TmpDir      = %(testbase)s/.tmp
BaselineDir = %(testbase)s/Baseline
IgnoreDirs  = .tmp
IgnoreFiles = *.tmp *.swp #* *.trace .DS_Store

[environment]
ZEEKPATH=`%(testbase)s/Scripts/get-zeek-env zeekpath`
ZEEK_PLUGIN_PATH=`%(testbase)s/Scripts/get-zeek-env zeek_plugin_path`
ZEEK_SEED_FILE=%(testbase)s/Files/random.seed
FILES=%(testbase)s/Files
PATH=`%(testbase)s/Scripts/get-zeek-env path`
PACKAGE=%(testbase)s/../scripts
TZ=UTC
LC_ALL=C
TRACES=%(testbase)s/Traces
TMPDIR=%(testbase)s/.tmp
TEST_DIFF_CANONIFIER=%(testbase)s/Scripts/diff-remove-timestamps
```

Now that we have the configuration ready, let's clarify how the test shown in
Listing 2 works. Lines 9–25 should be somewhat familiar: it's a helper function
that given a
[`Conn::Info`](https://docs.zeek.org/en/lts/scripts/base/protocols/conn/main.zeek.html#type-Conn::Info)
record (what is logged in conn.log) returns a `connection` record that we can
provide to `connection_state_remove`. Lines 27–30 are the event we define that
will fire on every record of the input file we are reading. As in our earlier
examples, we directly queue the `connection_state_remove` event this time using
the record parsed from a log. The `zeek_init` on lines 32–37 defines our event
stream with `Input::add_event` and uses our `event` handler `handler` for the
`$ev` argument. Each record it receives will be passed to that event, which in
turn will queue `connection_state_remove` to call into our package's logic.
`Input::remove` removes the input after the first read has completed.

Due to how events are handled when no PCAP is being read, we need to ensure that
_all_ the events are generated and handled before we terminate Zeek. Otherwise,
the Zeek process will terminate before we process our manually queued events and
we won't see our log generated. To get around this, we add `redef
exit_only_after_terminate = T;` on line 6. This does what it says on the tin:
only exit Zeek after we manually call `terminate()`. Lines 39–43 do exactly
that. After we finish parsing the input file, we call `terminate()` to shut down
Zeek. Line 41 isn't necessary here, but is required when running Zeek in an
environment where other input files are being processed. We keep it here to
encourage idiomatic Zeek programming. We can edit the input log however we like,
or even synthetically generate them, to introduce cases that didn't occur in
practice but that we'd like to test anyway.

**Listing 2:**
[`testing/ipdistance/log-driver.zeek`](https://github.com/corelight/ip-distance/blob/main/testing/ipdistance/log-driver.zeek?ref_type=heads)

```perl {linenos=true}
# @TEST-DOC: Run test driven by log file
# @TEST-COPY-FILE: ${FILES}/conn.log
# @TEST-EXEC: zeek $PACKAGE %INPUT
# @TEST-EXEC: btest-diff ip_distance.log

redef exit_only_after_terminate = T;
const path = "conn.log";

function conn_info_to_connection(data: Conn::Info): connection
	{
	local orig = endpoint($size=data?$orig_bytes ? data$orig_bytes : 0, $state=TCP_CLOSED, $flow_label=0);
	local resp = endpoint($size=data?$resp_bytes ? data$resp_bytes : 0, $state=TCP_CLOSED, $flow_label=0);

	return connection(
		$id=data$id,
		$orig=orig,
		$resp=resp,
		$start_time=data$ts,
		$duration=data?$duration ? data$duration : 0 secs,
		$service=data?$service ? split_string(data$service, /,/) as set[string] : set(),
		$history=data?$history ? data$history : "",
		$uid=data$uid,
		$conn=data
	);
	}

event handler(description: Input::EventDescription, tpe: Input::Event, data: Conn::Info)
	{
	event connection_state_remove(conn_info_to_connection(data));
	}

event zeek_init()
	{
	Input::add_event([$source=path, $name=path,
			  $fields=Conn::Info, $ev=handler]);
	Input::remove(path);
	}

event Input::end_of_data(name: string, source: string)
	{
	if ( source == path )
		terminate();
	}
```

# Longitudinal Analysis

We can use this same technique to run a Zeek package across several log files to
see how it would perform, which has helped me personally in debugging more
complicated scripts. Listing 3 shows the relevant code for this use case.

Lines 1–2 define a directory that contains our log files and a `vector` that
will contain them. Lines 6–27 are similar to our BTest example, although
instead of queuing the `connection_state_remove` event, we call a function
`connection_finished_handler` that is defined in the package we are debugging.
It also has a different signature than before. When reading a log file, the
`data` parameter is whatever record type is used by the log you're ingesting. In
this case, we're working with conn logs, so the type is `Conn::Info`. The rest
is a bit different.

Lines 60–70 define our `zeek_init`, which uses `Exec::run` to get a listing of
all the files in our log directory. When this asynchronous call completes, we
append the path of each file to our `vector` and call `process_file` defined on
lines 29–43 to process each one. If we're out of files to process, we return as
there's no more work to be done. Otherwise, we take the first file and use the
`Input::add_event` and `Input::remove` pattern we used prior. Our
`Input::end_of_data` handler (lines 45–58) is a bit more complicated, as we
have some additional bookkeeping to do. We return early if there are no more log
files or the `source` isn't from our earlier `add_event` call. Any additional
processing or calls to the Zeek package we're debugging go next. Then, we remove
the log file we just processed from our `vector`. If we still have log files to
process, we call `process_file` again, otherwise we `terminate()` the Zeek
process.

Using this approach, I was able to run over months of input logs in hours and
more quickly isolate the source of my bug. It would have been more cumbersome
and much more time-consuming to do this with PCAPs. It also let me quickly
iterate on improvements to my algorithm without having to wait weeks or months
testing on live traffic.

**Listing 3:** Analyzing Zeek logs with Zeek. Some details are removed as the
package being debugged is proprietary.

```perl {linenos=true}
const log_dir = "/path/to/log/data" &redef;
global log_files: vector of string;

redef exit_only_after_terminate = T;

function conn_info_to_connection(data: Conn::Info): connection
	{
	local orig = endpoint($size=data?$orig_bytes ? data$orig_bytes : 0, $state=TCP_CLOSED, $flow_label=0);
	local resp = endpoint($size=data?$resp_bytes ? data$resp_bytes : 0, $state=TCP_CLOSED, $flow_label=0);

	return connection(
		$id=data$id,
		$orig=orig,
		$resp=resp,
		$start_time=data$ts,
		$duration=data?$duration ? data$duration : 0 secs,
		$service=data?$service ? split_string(data$service, /,/) as set[string] : set(),
		$history=data?$history ? data$history : "",
		$uid=data$uid,
		$conn=data
	);
	}

event handler(description: Input::EventDescription, tpe: Input::Event, data: Conn::Info)
	{
	connection_finished_handler(conn_info_to_connection(data));
	}

function process_file()
	{
	if ( |log_files| == 0 )
		{
		print "no more log_files!";
		return;
		}

	local path = log_files[0];
	print fmt("processing %s...", path);

	Input::add_event([$source=path, $name=path,
			  $fields=Conn::Info, $ev=handler]);
	Input::remove(path);
	}

event Input::end_of_data(name: string, source: string)
	{
	if ( |log_files| == 0 || source != log_files[0] )
		return;

	# Manually run any processing/cleanup functions here!

	log_files = log_files[1:];

	if ( |log_files| > 0 )
		process_file();
	else
		terminate();
	}

event zeek_init()
	{
	when ( (local res = Exec::run([$cmd=fmt("ls %s", log_dir)])) )
		{
		for ( _, log in res$stdout )
			log_files += fmt("%s/%s", log_dir, log);

		print fmt("found %d log files to process", |log_files|);
		process_file();
		}
	}
```

## Caveats

I suspect this is not a commonly used technique because I ran into a few
roadblocks along the way. Specifically, there are some limitations to this
approach and I encountered at least one weird bug related to timing in the Zeek
event engine. Perhaps you, esteemed reader, can help us out here!

First, ingesting Zeek logs in this way cannot handle records that themselves
contain `&optional` records. I have a [draft
PR](https://github.com/zeek/zeek/pull/5170) to address this on Github, but it
needs some TLC before it is ready to merge. It turns out I didn't need it in the
end, but I hope to return to it someday to get it into shape.

Second, the logs need to be uncompressed for this approach to work, which limits
its utility for very large logs. While I was consuming the `conn` log, I was able
to prefilter it based on my use case to reasonable sizes, but the technique
would be more useful if it could work off of compressed logs. I'd like to add
this to Zeek as well, but alas, free time is finite.

Third, I ran into a roadblock when running on _a lot_ of log data from some of
our test sites. I described the issue in more detail on the [Zeek Community
slack](https://zeekorg.slack.com/archives/CTGDG83B8/p1773411044871609), but
essentially Zeek dies with:

```
fatal error: Dictionary (size 1784496) insertion distance too far: 65535
zsh: IOT instruction (core dumped)
```

The running theory here is the Zeek clock is not advancing as it normally does,
because we aren't processing traffic or a PCAP. The package makes heavy use of
the Zeek `table` data structure. The hunch is the standard cleanup routines are
based on the assumption that time doesn't stand still (enabling rebalancing of
the underlying data structures), so we end up exhausting the hash chains. We're
processing months of data here while frozen in time, so this isn't surprising.
If any of you Zeeksperts out there have any ideas I'm all ears!

# Conclusion

We showed four techniques you can use for testing exceptional cases for your
Zeek package where you may lack a PCAP; have a package that interfaces with an
external service; or want to drive your testing based on existing (or
synthetically generated!) Zeek logs. We also demonstrated how you can use the
final technique for long-term analysis and debugging of a Zeek package using
logs instead of PCAPs. Please let me know if you found this useful or have any
questions or comments. You can find me `@yacin` on the [Zeek Community
slack](https://zeek.org/slack). Happy hacking!
