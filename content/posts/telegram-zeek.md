+++
title = "Telegram Zeek, you’re my main notice"
tags = [
    "zeek",
    "telegram",
    "notice-framework",
    "notice",
]
categories = [
    "notice",
    "open-source",
]
date = "2021-07-28"
draft = false
+++


_This article was originally posted on [Zeek's
blog](https://zeek.org/2021/07/29/telegram-zeek-youre-my-main-notice/)._

# Notices in Zeek

Zeek’s Notice Framework enables network operators to specify how potentially
interesting network findings can be reported. This decoupling of detection and
reporting highlights Zeek’s flexibility: a notice-worthy event in network A may
be run-of-the-mill in network B. Much like detections, reporting needs will
likely differ between networks as well. Out of the box, Zeek includes eight
actions that can be taken when a notice is generated: four send emails, one
generates an entry in the notice.log, one is a no-op, one drops an address based
on NetControl policy, and one adds metadata to the `Notice::Info` record. A good
start, but I must admit the email-heavy focus feels very 1990s.

Beyond being a tad passé, I realized a more technical shortcoming when trying to
send notices about my home network to my phone: most residential ISPs block
outgoing SMTP traffic to curb spam. Thankfully, we can easily define new
`Notice::Actions` to get around that pesky filtering done by ISPs. I’ve created
and released a Zeek package, zeek-notice-telegram, that sends a message to a
user or group chat on Telegram when the new action is added to a notice. The
README on GitHub explains how to set it up. I’ll walk you through a simple
example that uses the package as well as the package itself so you can write
your own action.

# Using zeek-notice-telegram

My home network is small and I’m a paranoid security professional, so naturally
I run [Corelight@Home](https://corelight.com/blog/corelight-at-home). I mostly
analyze the logs after the fact, but I’d like to do more real time
monitoring. My first thought was to be notified on my phone when new hosts join
my network. The existing known hosts policy script more or less does this. If
enabled, it generates a log of all hosts that completed a TCP handshake
successfully. By default, it resets the set of known hosts after 24 hours. This
makes sense for a large enterprise where storing all this data in memory could
cause problems. But for my small home network that maxes out at 256 hosts, I
will disable this resetting behavior. Now known hosts will be logged when a
never-before-seen IP address is used on my network. But how can I integrate
notices and Telegram to an existing policy script?

At a high-level, the steps I will take are: (1) enable the known hosts policy
script, (2) redefine some variables from the `Known::` namespace to fit our new
use case, (3) generate a notice when a known host would be logged by the system,
and (4) add our new action to the notice so a message is sent directly to me
over Telegram. Below is the full content of `telegram-demo.zeek`, which is also
available in the repository:


{{< highlight bro "linenos=table,linenostart=0" >}}
@load policy/protocols/conn/known-hosts

module TelegramDemo;

export {
    redef enum Notice::Type += { NewKnownHost };
    redef Site::local_nets += { 192.168.1.0/24 };
    redef Notice::telegram_token "REDEF-TOKEN";
    redef Notice::telegram_chat_id = "REDEF-ID";
    global hosts: set[addr];
    }

event zeek_init()
    {
    Known::hosts = TelegramDemo::hosts;
    }

event Known::log_known_hosts(rec: Known::HostsInfo)
    {
    NOTICE([$note=NewKnownHost,
            $msg=fmt("New host %s appeared at %D", rec$host, rec$ts),
            $identifier=cat(rec$host)]);
    }

hook Notice::policy(n: Notice::Info)
    {
    if ( n$note == NewKnownHost )
        add n$actions[Notice::ACTION_TELEGRAM];
    }
{{< / highlight >}}

**Figure 1: A short demonstration of how to use the zeek-notice-telegram package.**

First, we load the known hosts policy script, which exposes the events we will
use (step 1) and define the module’s namespace. The export block redefines a new
notice type, the local CIDR block I use at home as well as the two
Telegram-specific values that identify my bot’s token and my user ID,
respectively. These last two have been redacted. In order to run this demo on
your network, you will need to substitute your values for these variables. See
the configuration section of the README for more detail.

Next the code corresponds step 2. We define a new set for the hosts. Note that
unlike the analogous line in `known_hosts.zeek`, ours does not have the
`&create_expire` attribute. Next, we replace the hosts set used in the known
hosts policy script, with the one we defined earlier. With this change, it will
track hosts for as long as Zeek is running. In the current implementation, this
will not track hosts across restarts of Zeek. This can be addressed by changing
the `Broker::BackendType` to anything except for `Broker::MEMORY` and is left as
an exercise for the reader.

The rest of the script is one event (step 3) and one hook (step 4): the
`log_known_hosts` event fires when a newly observed host is about to be logged
to the `known_hosts.log`. The `Notice::policy` hook allows us to update a notice
before it is forwarded to the actions assigned to it. In the event in step 3, we
generate a notice with our newly defined `NewKnownHost Notice::Type`. In the
hook in step 4, we add the Telegram action to any notices that have the note
type of `NewKnownHost`. Without this guard, all notices would be sent over
Telegram.

Be aware that `ACTION_TELEGRAM` could be added directly when calling the
`NOTICE` function by including it in the actions field. However, this tightly
couples the detection event with the reporting action, which one shouldn’t
do. Sending a message for every new host on my small home network will be
tolerable, but on a university network with tens of thousands of students it
would be completely unsustainable. The idiomatic way to do this in Zeek is to
use the `Notice::policy` hook as I did above. This allows network operators to
better suit my code to their use cases.

Here’s a video of the notices I received on my phone over Telegram when I ran
the following Zeek command on a PCAP collected from my home network: `$ zeek -Cr
~/test.pcap zeek-notice-telegram telegram-demo.zeek`

{{< youtube FGg_CMFmXEE >}}

**Figure 2: zeek-notice-telegram in action!**

If you pay close attention, you’ll notice the timestamps of the events show
much longer gaps in time than we see based on the message inter-arrival times.
Since we’re reading from a PCAP, Zeek is processing the packets faster than in
real time. If this were running on my Zeek instance at home, I would have
received these messages over approximately seven hours.

# So you want to write your own notice action?

Venerable reader, now you know how to use my extension to send notices over
Telegram. But what if this doesn’t fit your use case and you need to send
notices over a different media? Fear not, I will now walk you through the
zeek-notice-telegram package itself to prepare you to write your very own
extension to the Notice Framework.

The code for `telegram.zeek` is a bit longer, so I won’t do a line-by-line
walkthrough, but conceptually you can write your own messaging notice action
with the following four steps:

1. Define your action, so other folks can use it.
1. Add a hook, so notices with your action fire correctly.
1. Format your message payload.
1. Send your message.

I recommend relying on messaging technology that has an HTTP API for delivery.
Zeek has a built-in way to make HTTP requests and HTTP is unlikely to be
blocked by other network administrators, unlike what we saw earlier with SMTP.

First, we redefine the Action enum to have our new action:

{{< highlight bro "linenos=table,linenostart=0" >}}
module Notice;

export {
    redef enum Action += {
        ACTION_TELEGRAM,
    };
{{< / highlight >}}

**Figure 3: Adding our new notice action.**

Note that we are doing our work in the Notice namespace, since we are extending
that module.

Second, we add a hook so notices that contain our new action will run our
messaging code:

{{< highlight bro "linenos=table,linenostart=0" >}}
hook notice(n: Notice::Info)
    {
    if ( ACTION_TELEGRAM in n$actions )
        telegram_send_notice(telegram_payload(n));
    }
{{< / highlight >}}

**Figure 4: Adding our hook.**

When our package is loaded, every notice that is raised in other parts of the
system will first run this hook. The hook checks for the presence of our
action, and runs two functions to format the payload of our message and send
the message over Telegram.

Third, the function `telegram_payload` takes a `Notice::Info` record and returns
a string representing the text of the message. The text will always contain the
notice type and notice message fields from the record, and optionally includes
the sub-message, connection details, and source if present. Many fields in a
`Notice::Info` record are marked `&optional`, so use the `?$` operator to check
to see if they’re defined before including them.

Fourth, we send the message over Telegram using the HTTP API:

{{< highlight bro "linenos=table,linenostart=0" >}}
function telegram_send_notice(text: string)
    {
    if (telegram_token == "REDEF-TOKEN" || telegram_chat_id == "REDEF-ID")
        {
        Reporter::warning("Notice::telegram_token and Notice::telegram_chat_id must be redef'd to use Notice::ACTION_TELEGRAM");
        return;
        }
    local url = cat_sep("/", "", telegram_endpoint, cat("bot", telegram_token), "sendMessage");
    local request: ActiveHTTP::Request = ActiveHTTP::Request(
        $url=url,
        $method="POST",
        $client_data=fmt("chat_id=%s&text=%s", telegram_chat_id, text)
        );
    
    when ( local result = ActiveHTTP::request(request) )
        {
        if ( result$code != 200 )
        Reporter::warning(fmt("Telegram notice failed (%d): %s", result$code, result$body));
        }
    }
{{< / highlight >}}

**Figure 5: Sending the message.**

First, we check to see if either of the Telegram-specific variables have not
been redefined, and if so, outputs a warning and returns early so we do not
needlessly use Telegram’s API with a request that will fail. Next, we construct
the request using Zeek’s ActiveHTTP module. These will be specific to whichever
service you rely on to deliver your messages. Finally, we perform the API
request, and output an error if it does not return successfully.

That’s all there is to extending the Notice Framework. And one more thing: your
action doesn’t need to be limited to sending a message. For example, it could
add useful metadata or drop future connections to an offending host. The
possibilities are limited only by your imagination.

# Conclusion

Even as an intermediate Zeekscript programmer, I was surprised at not only how
simple it was to add a new notice action but how little code was needed to use
the new action for a non-trivial application. This demonstrates the flexibility
of Zeek and how it can be leveraged for quality of life improvements without
much programming. If the zeek-notice-telegram package tickled your fancy, you
may also want to check out the zeek-notice-slack package that does the same
thing but over Slack. Let us know about your Notice Framework extensions and
happy hunting!
