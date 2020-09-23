+++
title = "Detecting Zerologon (CVE-2020-1472) with Zeek"
tags = [
    "zeek",
    "cve",
    "zerologon",
    "CVE-2020-1472",
]
categories = [
    "detection",
    "open-source",
]
date = "2020-09-16"
draft = false
+++

_This article was cross-posted on [Corelight's
blog](https://corelight.blog/2020/09/16/detecting-zerologon-cve-2020-1472-with-zeek/)_.

CVE-2020-1472 aka Zerologon, disclosed by Tom Tervoort of
[Secura](https://www.secura.com/blog/zero-logon), is an illustrative
case study of how a small implementation mistake in cryptographic
routines cascades into a privilege escalation vulnerability that
allows an attacker to change the password of any unpatched Active
Directory domain controllers to which they have network access. Upon
successful exploitation, the attacker is free to alter additional
credentials, escalate to the level of a domain admin, and move
laterally to other machines in the domain. At a high level, the
encryption scheme as implemented has a 1/256 chance of encrypting a
plaintext message of all zeroes to a ciphertext message of all zeroes,
which eventually leads to setting a zero length password. If this
sounds as interesting to you as it was to me, I'd recommend reading
the more [in-depth technical
report](https://www.secura.com/pathtoimg.php?id=2055) also from
Secura.

To assist, we've [open sourced a Zeek
package](https://github.com/corelight/zerologon) that detects both
attempted and successful exploits. Using Secura's excellent, [defanged
proof-of-concept Python
tool](https://github.com/SecuraBV/CVE-2020-1472), we generated sample
PCAPs for unsuccessful and successful attacks on both Windows Server
2016 and 2019 domain controllers. These pcaps are included in the
repository. I would be remiss if I didn't also mention these
techniques for detection: [Sigma rule by
SOCPrime](https://twitter.com/andriinb/status/1304676530350628864?s=1),
and [Splunk by Shannon
Davis](https://www.linkedin.com/feed/update/urn:li:activity:6711471711751168000/). These
served as inspiration for the Zeek package.

There are fully functional exploit tools for this CVSS 10.0 rated
vulnerability already floating around publicly, so I recommend reading
[Microsoft's support
guide](https://support.microsoft.com/en-us/help/4557222/how-to-manage-the-changes-in-netlogon-secure-channel-connections-assoc)
entry, patching your domain controllers, and looking for signs of
historic exploitation attempts in your logs, and looking for future
attempts with this Zeek package.

Below are snippets from the Zeek package that define the Notices that
are generated and the global "knobs" that when `redef`ed can be used
to tune for different network traffic profiles. The package will run
in both clustered and non-clustered environments.

```bro
redef enum Notice::Type += {
    Zerologon_Attempt,
    Zerologon_Password_Change
};

export {
    # Time window of attack. A higher value will catch more careful
    # attackers, but at the potential cost of more false positives.
    global expire = 2min &redef;
    # Minimum required number of NetrServerReqChallenge and
    # NetrServerAuthenticate3 pairs before considering it to be an
    # attempted attack.
    global cutoff = 20 &redef;
    # Change this to T if you only want a notice to be generated for a
    # successful exploit.
    global notice_on_exploit_only = F &redef;
}
```

Happy hunting!
