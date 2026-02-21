# Launch Post Drafts

Ready-to-adapt drafts for Reddit and Hacker News. Adjust tone per subreddit.

---

## Reddit: r/raspberry_pi (Soft Launch)

**Title:** I built a Pico 2 W device that wakes my PC when I press the home button on my controller

**Body:**

I got tired of getting up from the couch to press the power button on my PC every time I wanted to play a game. So I built PadProxy.

It's a small board based on the Raspberry Pi Pico 2 W that sits inside my PC case. It connects to my Xbox controller over Bluetooth and proxies the input to the PC as a standard USB gamepad. The key trick: when the PC is off or sleeping, pressing the home button on the controller triggers the PC's power button through the front panel header.

**How it works:**
- Pico 2 W connects to Bluetooth gamepads via Bluepad32
- Presents as a USB HID gamepad to the PC (TinyUSB)
- Monitors the PC power state via the front panel power LED
- When the PC is off and a controller connects, it pulses the power button pin

The whole thing is open source (MIT for software, CERN-OHL-P for hardware): [GitHub link]

I'm also selling assembled units and kits on Tindie if anyone wants one without building it themselves: [Tindie link]

Happy to answer questions about the build, the firmware architecture, or the hardware design. The power management state machine was the trickiest part to get right -- there's a detailed design doc in the repo if anyone's curious.

[Photo of device]
[GIF of it in action]

---

## Reddit: r/pcgaming (Broader Launch)

**Title:** I made a device that wakes my PC when I press the Xbox/PlayStation home button -- no more getting off the couch

**Body:**

Quick demo: [GIF/video]

I built a small device called PadProxy that sits inside my PC case. It pairs with my wireless controller over Bluetooth and forwards the inputs to the PC over USB. When the PC is off or asleep, pressing the home button on the controller wakes it up.

**Why I built it:** My PC is across the room from my couch. Every gaming session started with: get up, walk to PC, press power, walk back, sit down, pick up controller, wait for Bluetooth to pair. Now it's just: press the Xbox button, wait 10 seconds, I'm in.

**What it supports:**
- Xbox Wireless (One, Series X|S)
- PlayStation (DS4, DualSense)
- Switch Pro Controller
- 8BitDo controllers
- Most Bluetooth gamepads

The PC just sees a standard wired USB controller, so it works with every game -- no driver issues, no pairing problems on the PC side.

It's open source on GitHub and I'm selling assembled units on Tindie for anyone who wants one: [links]

[Photos]

---

## Reddit: r/htpc

**Title:** PadProxy: open-source device that bridges BT gamepads to USB and wakes your HTPC from the controller

**Body:**

Built this for my living room HTPC setup and figured others here might find it useful.

PadProxy is a Raspberry Pi Pico 2 W based device that mounts inside your PC case. It:

1. Pairs with your Bluetooth gamepad (Xbox, PS, Switch Pro, 8BitDo)
2. Appears to the PC as a standard wired USB gamepad (no drivers needed)
3. Wakes the PC from off/sleep when you press the home button on the controller

The wake feature is the main selling point for HTPC use -- the experience becomes identical to a console. Press the button, the TV input switches (if you have CEC), the PC boots, and you're in Big Picture mode.

It connects to an internal USB header and passes through the front panel power button header (so your regular power button still works).

Open source (hardware + software): [GitHub]
Selling assembled units and kits on Tindie: [Tindie]

Installation takes about 10 minutes. Happy to answer questions.

---

## Hacker News: Show HN

**Title:** Show HN: PadProxy -- Open-source RP2350 device for wireless gamepad-to-USB with PC wake

**Body (text post):**

GitHub: [link]

PadProxy is an open-source hardware device (Raspberry Pi Pico 2 W) that sits inside a PC case and provides two things:

1. Bluetooth-to-USB gamepad bridging (your wireless controller appears as a standard wired USB HID device)
2. PC wake-on-controller (press the home button on a sleeping/off PC and it wakes up via the front panel power header)

The firmware is C11 on the Pico SDK, using Bluepad32 for Bluetooth and TinyUSB for USB HID. The interesting engineering challenge was the power state machine -- the device needs to reliably detect whether the PC is off, booting, on, or sleeping using only the front panel power LED as a signal, and coordinate power button triggering with USB enumeration timing. The design doc for this is at [link to power-management-design.md].

Hardware is licensed CERN-OHL-P-2.0 (permissive), firmware is MIT. KiCad files and 3D printable enclosure included in the repo.

Selling assembled units on Tindie [link] but everything needed to build your own is in the repo.

---

## Adaptation Notes

**Tone per community:**
- r/raspberry_pi -- Technical, project-focused, emphasize the Pico 2 W and the build process
- r/pcgaming -- Problem/solution focused, emphasize the user experience, minimize jargon
- r/htpc -- Practical, emphasize the console-like experience and HTPC integration
- r/homelab -- Technical, emphasize the open-source aspect and self-hosting ethos
- r/8bitdo -- Controller-focused, emphasize compatibility and "it just works"
- Hacker News -- Engineering-focused, emphasize the interesting technical problems

**Timing:**
- Post on Tuesday-Thursday mornings (US time) for best engagement
- Space posts across subreddits by at least 3-5 days
- Don't cross-post -- write native posts for each community
- Soft launch on r/raspberry_pi first to catch issues before the bigger audiences

**Rules to follow:**
- Never lead with a sales pitch. Lead with the project, the problem, or the build story.
- Include photos/GIFs in every post (Reddit heavily favors visual content)
- Respond to every comment in the first 24 hours
- If someone asks a question you don't have a good answer for, that's a doc or FAQ to write
- Never be defensive about criticism -- thank people for feedback and iterate
