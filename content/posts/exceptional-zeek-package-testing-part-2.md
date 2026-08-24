+++
title = "The Cure for Exceptional Zeek Package Testing (Part 2)"
tags = [
    "zeek",
    "btest",
    "testing",
    "packages",
]
categories = [
    "testing",
    "open-source",
]
date = "2026-08-23"
draft = false
+++

_This article was originally posted on [Zeek's
blog](https://zeek.org/2026/07/the-cure-for-exceptional-zeek-package-testing-part-2)._

# Introduction

In [the previous blog]({{< ref "exceptional-zeek-package-testing-part-1" >}}) in
our series, we presented our motivational package,
[`ip-distance`](https://github.com/corelight/ip-distance), and how you can
easily write unit tests with [BTest](https://github.com/zeek/btest). This works
great for smaller auxiliary functions that aren't necessary driven by network
traffic, but what if a PCAP to drive our tests isn't available or can't be
shared along with the code? In this part of the blog series, we'll cover #2 and
#3 in the list below:

1. Writing good unit tests for auxiliary functions.
2. **Hand-crafting events to exercise your package's functionality.**
3. **Automatically generating events from a PCAP using Zeek's built-in event tracing flag.**
4. Using a Zeek log and the Input Framework to drive your package's
   functionality.

# Artisanal Event Generation

I'm a fan of the adage ["PCAP or it didn't
happen"](https://www.netresec.com/?page=Blog&month=2014-05&post=PCAP-or-it-didn%27t-happen),
but sometimes you come up short. It "didn't happen" and there is no PCAP; or you
have one, but can't release it publicly. Or maybe your Zeek package only works
when paired with some external service that communicates over the [WebSocket
API](https://docs.zeek.org/en/master/advanced/devel/websocket-api.html), or some
other run-time situation that you can't readily make happen in isolation. To
test a more advanced package like this, you would need to run this service (or
mock one up) alongside a running Zeek instance. Yuck. Alternatively, you can
test all of these cases by just manually creating and raising events in a BTest.

Zeek events are generally driven by the network traffic being analyzed, but that
isn't the only way. The [Zeek
documentation](https://docs.zeek.org/en/current/reference/zeekscript/statements.html#keyword-event)
tells us that "[the] `event` statement immediately queues invocation of an event
handler." We can use this to our advantage to make a test that manually fires
the event(s) consumed by our package to make it operate. Instead of our baseline
being from `print` statements, we will compare against whatever our package
normally generates. In our case, this will be the `ip_distance.log`. We're lucky
because our entry point is a single event: `connection_state_remove`, which
takes a `connection` record. As you'll see from the
[`manual-events.zeek`](https://github.com/corelight/ip-distance/blob/main/testing/ipdistance/manual-events.zeek?ref_type=heads)
below, the onerous part is creating the data structures that our `event`
requires.

```perl {linenos=true}
# @TEST-DOC: Run manually constructed events test
# @TEST-EXEC: zeek $PACKAGE %INPUT
# @TEST-EXEC: btest-diff ip_distance.log

global uid = 0;

function make_conn_id(orig_h: addr, resp_h: addr, orig_p: port &default=31337/tcp, resp_p: port &default=8080/tcp): conn_id
	{
	return conn_id($orig_h=orig_h, $orig_p=orig_p, $resp_h=resp_h, $resp_p=resp_p);
	}

function make_conn(orig_h: addr, resp_h: addr): Conn::Info
	{
	return Conn::Info(
		$ts=double_to_time(0.0),
		$uid=cat(++uid),
		$id=make_conn_id(orig_h, resp_h),
		$proto=tcp,
	);
	}

function make_connection(orig_h: addr, resp_h: addr): connection
	{
	local conn = make_conn(orig_h, resp_h);
	local orig = endpoint($size=42, $state=TCP_CLOSED, $flow_label=1);
	local resp = endpoint($size=42, $state=TCP_CLOSED, $flow_label=1);

	return connection(
		$id=conn$id,
		$orig=orig,
		$resp=resp,
		$start_time=double_to_time(0.0),
		$duration=0 secs,
		$service=set("http"),
		$history="ShAaDdF",
		$uid=conn$uid,
		$conn=conn,
	);
	}

event zeek_init()
	{
	event connection_state_remove(make_connection(1.0.0.0, 3.0.0.0));
	event connection_state_remove(make_connection(255.255.255.255, 0.0.0.0));
	event connection_state_remove(make_connection(3.1.33.7, 1.2.3.4));
	event connection_state_remove(make_connection(202.254.186.190, 218.218.190.239));
	}
```

The important bits are again the BTest keywords on lines 1–3 and the
`zeek_init` body on lines 41–47. The rest is all to make it easy to create the
`connection` record that `connection_state_remove` requires. Let's walk through
each of these three chunks.

The BTest keywords are nearly identical but instead we now also `btest-diff`
against the log the package generates. Easy peasy.

The `zeek_init` is similarly simple. Queue the events and let our package do the
rest! That just leaves the complicated bits of actually creating the
`connection` record.

We need to create a `connection` record, which is described in the [Zeek
scripting
reference](https://docs.zeek.org/en/lts/scripts/base/init-bare.zeek.html#type-connection).
What we care about are any required fields (i.e., not marked as `&optional`) and
the fields that our package consumes. In our case, this is just `id.orig_h` and
`id.resp_h`. `connection` is a meaty record that itself contains records, which
themselves contain records. Clicking through the above documentation tells us
that to create a `connection`, we must also create each of the following:

- [`conn_id`](https://docs.zeek.org/en/lts/scripts/base/init-bare.zeek.html#type-conn_id)
- [`endpoint`](https://docs.zeek.org/en/lts/scripts/base/init-bare.zeek.html#type-endpoint)
- [`Conn::Info`](https://docs.zeek.org/en/lts/scripts/base/protocols/conn/main.zeek.html#type-Conn::Info)
  (`&optional` but used by `ip-distance`.)

Don't fret, however! Since Zeek is a programming language, we can just define a
few helper functions to make our life as easy as it looks in our `zeek_init`. We
do just this in lines 5–39.

Our `make_conn_id` (7–10) function is the simplest one. A `conn_id` just
requires the 4-tuple of endpoints and ports. Since our package only cares about
the endpoints, those are the main parameters to our function. For the ports,
which we do not currently consider, we could just hardcode them. If we think we
may extend the package to take these into account, we can instead have them as
parameters to our function but use the `&default` attribute to give them sane
values if they are not provided.

`make_conn` (12–20) is a bit more complicated. It requires a `conn_id`, which
we can generate with `make_conn_id`, as well as a `time`, a UID, and a
[`transport_proto`](https://docs.zeek.org/en/v8.2.0/scripts/base/init-bare.zeek.html#type-transport_proto).
None of these are considered by our package, so we can pretty much use whatever
we want here. We use `double_to_time` to create a `time` type, the arbitrary
`tcp` for the `transport_proto`, and any old `string` for the UID. Since our log
does output the UID, we use the `string` representation of an increasing number
so we can more easily identify a test failure in the future.

We tie this all together with `make_connection` (22–39). We make our `conn` and
our `endpoint` records first. We do not make use of the fields in an `endpoint`
so we put whatever gives us joy here. Finally, we put these all together and
return our instantiated `connection` record.

We use the `btest` command as we did prior to generate our baseline et voilà:

```
### BTest baseline data generated by btest-diff. Do not edit. Use "btest -U/-u" to update. Requires BTest >= 0.63.
#separator \x09
#set_separator	,
#empty_field	(empty)
#unset_field	-
#path	ip_distance
#open XXXX-XX-XX-XX-XX-XX
#fields	uid	distance	norm_distance
#types	string	double	double
1	8192.0	0.007828
2	1046525.995962	1.0
3	8210.043179	0.007845
4	66180.876762	0.063239
#close XXXX-XX-XX-XX-XX-XX
```

We have a functional BTest without needing to have or share a PCAP!

So now you have an example for crafting your own artisanal events to test your
package. But I can see you might be worried here. The test is over half the
length of the entire Zeek script! And I had to dig through the documentation to
figure out how to manually construct the records for my event. I may be used to
this, but if you're new to Zeek I know this can be intimidating. Well, you're in
luck if you happen to have a PCAP but simply cannot share it or include it
directly in your BTests. We can make our lives easier by using Zeek's _event
traces_ ...

# Event Traces

Zeek's [event
tracing](https://docs.zeek.org/en/v8.2.0/reference/zeekscript/event-semantics.html#tracing-events)
records the events generated by Zeek (running either against a PCAP or live
traffic) simply by adding `-E event-trace.zeek` when running `zeek`. This
generates a Zeek script that we can directly run again with `zeek
event-trace.zeek`, which will exactly reproduce what we saw when running against
the PCAP. This eliminates all the manual work we had to do in our [Artisanal
Event Generation](#artisanal-event-generation) example. All that remains are two
important steps: sanitizing any sensitive information, and adding our BTest
keyword preamble. After this, we have a fully functional, event-driven BTest
without any programming or documentation reading. Work smart, not hard!

[`event-trace.zeek`](https://github.com/corelight/ip-distance/blob/main/testing/ipdistance/event-trace.zeek?ref_type=heads)
contains the output from `$ zeek -Cr sensitive.pcap ip-distance/scripts -E
event-trace.zeek` with the BTest keywords prepended. Here it is in all its
ugliness, a common property of generated code:

```perl {linenos=true}
# @TEST-DOC: Run event trace test generated from pcap
# @TEST-EXEC: zeek $PACKAGE %INPUT
# @TEST-EXEC: btest-diff ip_distance.log

module __EventTrace;

global __base_time = 1587508358.296407;

global connection_flipped_0__et: event();
global new_connection_1__et: event();
global net_done_2__et: event();
global connection_state_remove_3__et: event();
global zeek_done_4__et: event();

event zeek_init() &priority=-999999
	{
	event __EventTrace::connection_flipped_0__et();
	}

global __val1: conn_id;
global __val3: endpoint;
global __val6: connection;
event connection_flipped_0__et()
	{
	local __val0: conn_id_ctx = conn_id_ctx();
	__val1 = conn_id($orig_h=192.168.1.9, $orig_p=53778/tcp, $resp_h=34.194.201.2, $resp_p=443/tcp, $proto=6, $ctx=__val0);
	local __val2: endpoint = endpoint($size=0, $state=0, $num_pkts=0, $num_bytes_ip=0, $flow_label=0, $l2_addr="a4:83:e7:c0:06:e1", $vantage_t=double_to_interval(999.900000));
	__val3 = endpoint($size=0, $state=0, $num_pkts=0, $num_bytes_ip=0, $flow_label=0, $l2_addr="78:d2:94:bb:bf:62", $vantage_t=double_to_interval(999.900000));
	local __val4: set[string] = set();
	local __val5: set[string] = set();
	__val6 = connection($id=__val1, $orig=__val2, $resp=__val3, $start_time=double_to_time(__base_time), $duration=double_to_interval(0.000000), $service=__val4, $history="^", $uid="CHMZR43n9aNqhZHbn5", $failed_analyzers=__val5, $extract_orig=F, $extract_resp=F, $ftp_data_reuse=F);

	event connection_flipped(__val6);

	set_network_time(double_to_time(__base_time));
	event __EventTrace::new_connection_1__et();
	}

event new_connection_1__et()
	{
	event new_connection(__val6);

	set_network_time(double_to_time(__base_time));
	event __EventTrace::net_done_2__et();
	}

global __val7: Conn::Info; # from script
event net_done_2__et()
	{
	__val7 = Conn::Info($ts=double_to_time(__base_time), $uid="CHMZR43n9aNqhZHbn5", $id=__val1, $proto=tcp, $local_orig=T, $local_resp=F, $missed_bytes=0, $ip_proto=6); # from script
	__val6$conn = __val7; # from script

	event net_done(double_to_time(__base_time));

	set_network_time(double_to_time(__base_time));
	event __EventTrace::connection_state_remove_3__et();
	}

event connection_state_remove_3__et()
	{
	__val3$size = 56;
	__val3$state = 3;
	__val3$num_pkts = 1;
	__val3$num_bytes_ip = 108;
	__val6$history = "^d";

	event connection_state_remove(__val6);

	set_network_time(double_to_time(__base_time));
	event __EventTrace::zeek_done_4__et();
	}

event zeek_done_4__et()
	{
	__val7$conn_state = "OTH"; # from script
	__val7$history = "^d"; # from script
	__val7$orig_pkts = 0; # from script
	__val7$orig_ip_bytes = 0; # from script
	__val7$resp_pkts = 1; # from script
	__val7$resp_ip_bytes = 108; # from script

	event zeek_done();

	}

# constants of type time:
#	__base_time = 1587508358.296407

# constants of type string:
#	"78:d2:94:bb:bf:62"
#	"CHMZR43n9aNqhZHbn5"
#	"OTH"
#	"^"
#	"^d"
#	"a4:83:e7:c0:06:e1"

# constants of type port:
#	443/tcp
#	53778/tcp

# constants of type addr:
#	192.168.1.9
#	34.194.201.2
```

I can feel your eyes glazing over through time and space, but do not fear, for
it is the mind-killer. I'll point you to the relevant parts we need to get our
test in order and safe for your `git` history.

First things first, the preamble (lines 1–3) should put you at ease. It's
essentially identical to our [Artisanal Event
Generation](#artisanal-event-generation) example. That should come as no
surprise as it is working in the same way, just with generated events rather
than ones we wrote ourselves.

Next, we need to remove any sensitive information (PII, etc.) from the event
trace so we can safely use it. There sadly is no algorithm I can provide that
can guarantee PII removal, so you'll have to put on your thinking cap, chat with
your legal team, and sanitize where appropriate. Furthermore, the method of
anonymization you use can impact how your package works, so it's best to have
your package's internals fresh in your mind when pruning. Turn your attention to
lines 86–103.

This section of the event trace contains all the constants that were seen when
running against the input PCAP. They are broken down by the Zeek type. In
general, this is where the PII lives. If we take a look at each section one by
one and use some networking common sense, we can identify ones that ought to be
anonymized.

The `time` constants are unlikely to be a source of PII in our case. The
`string` constants, however, are a different story. Two are clearly MAC
addresses, which with some notable exceptions, do uniquely identify a device.
Since we don't care about these and they're strings, we can easily search and
replace them with whatever we would like without impacting the test. Other data
represented as a `string`, such as DNS query names or HTTP URIs, could contain
PII but we don't have to worry about that here. In our case, a `port` is fine to
share, but if you use uncommon ports you may not want to broadcast this out to
the world. Finally, the `addr` type is also something you'd likely want to hide.
I typically just change the first octet to `10` unless I know that would cause
problems for my algorithm. Here it will change the results, but not cause
problems for the distance metric, so we can change the IPs however we like. In
fact, there's nothing "holy" about the script (other than its visual
complexity); so this technique can also be used to generate additional corner
cases that the original PCAP doesn't cover. With just a bit of editing you can
get what you want.

So far we've explored testing with unit tests and two methods for testing based
on directly generating the events we care about. There's a fourth method I've
started using recently that works by using Zeek _logs_ as input instead of the
more typical PCAP approach. If your Zeek package only uses data readily
available in logs, it's another potential avenue for testing when PCAPs are not
handy. Check out [the final
part]({{< ref "exceptional-zeek-package-testing-part-3" >}}) in our three-part
series on exceptional Zeek package testing!
