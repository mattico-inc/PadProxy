# Launch Post Drafts

Ready-to-adapt drafts for Reddit and Hacker News. Adjust tone per subreddit.

The core message across all posts: **this thing should exist and didn't, so I built it.** Not a sales pitch. Not a startup. Just a thing that makes couch gaming on PC work the way it should.

---

## Reddit: r/raspberry_pi (Soft Launch)

**Title:** I built a Pico 2 W device that wakes my PC when I press the home button on my controller

**Body:**

I've been doing couch gaming on my PC for a while and the one thing that always bugged me was: on a console, you press the button and you're playing. On PC, you have to get up, walk across the room, press the power button, walk back, and then deal with Bluetooth pairing. It's such a small thing but it completely breaks the experience.

Nothing existed to fix this, so I built PadProxy.

It's a small board based on the Raspberry Pi Pico 2 W that sits inside my PC case. It connects to my Xbox controller over Bluetooth and proxies the input to the PC as a standard USB gamepad. The key trick: when the PC is off or sleeping, pressing the home button on the controller triggers the PC's power button through the front panel header.

**How it works:**
- Pico 2 W connects to Bluetooth gamepads via Bluepad32
- Presents as a USB HID gamepad to the PC (TinyUSB)
- Monitors the PC power state via the front panel power LED
- When the PC is off and a controller connects, it pulses the power button pin

The whole thing is open source (MIT for software, CERN-OHL-P for hardware): [GitHub link]

Everything you need to build your own is in the repo. I'm also putting together assembled units and kits on Tindie for anyone who'd rather not source parts: [Tindie link]

Happy to answer questions about the build, the firmware architecture, or the hardware design. The power management state machine was the trickiest part to get right -- there's a detailed design doc in the repo if anyone's curious.

[Photo of device]
[GIF of it in action]

---

## Reddit: r/pcgaming (Broader Launch)

**Title:** I made a device that wakes my PC when I press the Xbox/PlayStation home button -- no more getting off the couch

**Body:**

Quick demo: [GIF/video]

I do most of my gaming from the couch with a wireless controller. The one thing that's always been annoying: on a PlayStation or Xbox, you press the button and the system wakes up. On PC, you have to get up, walk to the PC, hit the power button, walk back, sit down, grab the controller, and wait for Bluetooth to decide to work.

It's a small annoyance but it adds up, and nothing existed to solve it. So I built PadProxy.

It's a small open-source device that sits inside your PC case. It pairs with your wireless controller over Bluetooth and forwards the inputs to the PC over USB. When the PC is off or asleep, pressing the home button on the controller wakes it up. Console-level convenience on PC.

**What it supports:**
- Xbox Wireless (One, Series X|S)
- PlayStation (DS4, DualSense)
- Switch Pro Controller
- 8BitDo controllers
- Most Bluetooth gamepads

The PC just sees a standard wired USB controller, so it works with every game -- no driver issues, no pairing problems on the PC side.

The whole project is open source on GitHub -- you can build your own from the design files. I'm also selling assembled units on Tindie for anyone who wants one ready to go: [links]

Not trying to start a business here -- just built something that should have existed and figured others might want one too.

[Photos]

---

## Reddit: r/htpc

**Title:** Open-source device that bridges BT gamepads to USB and wakes your HTPC from the controller

**Body:**

Built this for my living room HTPC setup because I wanted the console experience -- press the button, system wakes up, start playing.

PadProxy is a Raspberry Pi Pico 2 W based device that mounts inside your PC case. It:

1. Pairs with your Bluetooth gamepad (Xbox, PS, Switch Pro, 8BitDo)
2. Appears to the PC as a standard wired USB gamepad (no drivers needed)
3. Wakes the PC from off/sleep when you press the home button on the controller

For HTPC use this is the missing piece -- the experience becomes identical to a console. Press the button, the TV input switches (if you have CEC), the PC boots, and you're in Big Picture mode. No keyboard or mouse needed, no getting off the couch.

It connects to an internal USB header and passes through the front panel power button header (so your regular power button still works).

The project is fully open source (hardware + software): [GitHub]

You can build your own from the repo, or I have assembled units and kits on Tindie: [Tindie]

Installation takes about 10 minutes. Happy to answer questions.

---

## Hacker News: Show HN

**Title:** Show HN: PadProxy -- Open-source RP2350 device for wireless gamepad-to-USB with PC wake

**Body (text post):**

GitHub: [link]

PadProxy is an open-source hardware device (Raspberry Pi Pico 2 W) that sits inside a PC case and provides two things:

1. Bluetooth-to-USB gamepad bridging (your wireless controller appears as a standard wired USB HID device)
2. PC wake-on-controller (press the home button on a sleeping/off PC and it wakes up via the front panel power header)

The motivation: consoles have had "press button to wake" for years. PC has nothing equivalent for wireless gamepads. Wake-on-LAN requires network config and a second device. Bluetooth on PC is unreliable for gamepads. This fills the gap.

The firmware is C11 on the Pico SDK, using Bluepad32 for Bluetooth and TinyUSB for USB HID. The interesting engineering challenge was the power state machine -- the device needs to reliably detect whether the PC is off, booting, on, or sleeping using only the front panel power LED as a signal, and coordinate power button triggering with USB enumeration timing. The design doc for this is at [link to power-management-design.md].

Hardware is licensed CERN-OHL-P-2.0 (permissive), firmware is MIT. KiCad files and 3D printable enclosure are in the repo -- everything needed to build your own.

Assembled units available on Tindie [link] for anyone who'd rather not DIY, but the project is open source first and foremost.

---

## Adaptation Notes

**Tone per community:**
- r/raspberry_pi -- Technical, project-focused. Emphasize the Pico 2 W, the build process, and the engineering challenges.
- r/pcgaming -- Problem/solution focused. Emphasize the user experience and the "this should exist" angle. Minimize jargon.
- r/htpc -- Practical. Emphasize the console-like experience and HTPC integration.
- r/homelab -- Technical. Emphasize the open-source aspect and DIY ethos.
- r/8bitdo -- Controller-focused. Emphasize compatibility and "it just works."
- Hacker News -- Engineering-focused. Emphasize the interesting technical problems and the open-source licensing.

**Consistent thread across all posts:** This isn't a product launch or a startup pitch. You built something because it didn't exist, it should have, and now you're sharing it. The Tindie listing is there for convenience, not as the point.

**Timing:**
- Post on Tuesday-Thursday mornings (US time) for best engagement
- Space posts across subreddits by at least 3-5 days
- Don't cross-post -- write native posts for each community
- Soft launch on r/raspberry_pi first to catch issues before the bigger audiences

**Rules to follow:**
- Never lead with a sales pitch. Lead with the problem, the project, or the build story.
- Include photos/GIFs in every post (Reddit heavily favors visual content)
- Respond to every comment in the first 24 hours
- If someone asks a question you don't have a good answer for, that's a doc or FAQ to write
- Never be defensive about criticism -- thank people for feedback and iterate
