+++
title = "PrintNightmare, SMB3 encryption, and your network"
tags = [
    "zeek",
    "cve",
    "printnightmare",
    "CVE-2021-34527",
    "CVE-2021-1675",
]
categories = [
    "detection",
    "open-source",
]
date = "2021-07-07"
draft = false
+++

_This article was cross-posted on [Corelight's
blog](https://corelight.blog/2021/07/07/why-is-printnightmare-hard-to-detect/)_.

[CVE-2021-1675](https://msrc.microsoft.com/update-guide/vulnerability/CVE-2021-1675),
also tracked in
[CVE-2021-34527](https://msrc.microsoft.com/update-guide/vulnerability/CVE-2021-34527),
is a remote code execution vulnerability that targets the Windows Print Spooler
service. In a nutshell, there is a Distributed Computing Environment / Remote
Procedure Call (DCE/RPC) that allows authenticated users to add printer drivers
to the spooler service.  A malicious DLL instead can be included, which allows
a user to take over the machine running the spooler. In the event this machine
is also the domain controller, the implications are worse, as the included code
will run at a higher level of privilege.

Existing approaches for detection rely on hunting for [Windows
events](https://github.com/LaresLLC/CVE-2021-1675#carbon-black-hunting-query-for-cve-2021-1675)
or [system-level artifacts](https://github.com/SigmaHQ/sigma/pull/1593/files),
but detecting on the network presents unique challenges. The attack relies on
adding a printer driver using the DCE/RPC commands `RpcAddPrinterDriver` or
`RpcAddPrinterDriverEx`. Unfortunately, there are legitimate uses of this, so
relying fully on this command completing successfully as a detection could be
error prone. To make matters worse, [one
POC](https://github.com/cube0x0/CVE-2021-1675) in the wild wraps the DCE/RPC
calls in SMB3 encryption. Compare the first screenshot that clearly shows the
relevant RPC call versus the second from the aforementioned POC that only shows
encrypted payloads.


{{< image src="../../../../images/blog/printnightmare-1.png" alt="PCAP in Wireshark showing PrintNightmare exploit carried over DCE/RPC in the clear." style="border-radius: 8px;" >}}
_Figure 1: PCAP in Wireshark showing PrintNightmare exploit carried over DCE/RPC
in the clear._

{{< image src="../../../../images/blog/printnightmare-2.png" alt="PrintNightmare POC written in impacket with payloads encrypted by SMB3." style="border-radius: 8px;" >}}
_Figure 2: PrintNightmare POC written in
[impacket](https://github.com/SecureAuthCorp/impacket) with payloads encrypted
by SMB3. Graciously shared with us by [Zander
Work](https://twitter.com/captainGeech42)._

So in order to identify the DCE/RPC commands, one must either somehow intercept
and decrypt the payloads or somehow infer the commands from encrypted
communications, as we have done with other protocols.

Given the severity of the issue, we are releasing a [Zeek
package](https://github.com/corelight/CVE-2021-1675) that identifies printer
driver additions that occur in the clear over DCE/RPC. While this isn’t
guaranteed to only identify this exploit, the command appears to occur rarely
on our test networks. Your mileage may vary, however. We hope this helps the
NDR community identify potential instances of the exploit and as always, we
appreciate any comments or feedback you may have. As of July 6 Microsoft has
released additional patches to address this issue, as well as other mitigations
if updating a system is not feasible.
