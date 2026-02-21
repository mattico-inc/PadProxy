# PadProxy Indie Marketing Strategy

## Why This Exists

Couch gaming on PC is broken. Consoles figured this out years ago: you press the button on the controller, the system wakes up, and you're playing. On PC, the experience is: get up from the couch, walk across the room, press the power button, walk back, sit down, pick up the controller, wait for Bluetooth to pair, hope it actually connects this time.

No product exists today that solves this cleanly. Wake-on-LAN requires network configuration and a second device. Bluetooth on PC is unreliable for gamepads. USB dongles solve pairing but not wake. There's a gap, and PadProxy fills it.

**This is not a money-making venture.** The goal is to put this device into the hands of people who want it. Selling units on Tindie covers the cost of components and PCB runs so the project can sustain itself. If it breaks even, that's a success. If it funds a v2 with a better enclosure, even better. But the motivation is: this thing should exist, and now it does.

## Product Positioning

**One-liner:** Press the home button on your controller. Your PC wakes up. You're gaming.

**Elevator pitch:** PadProxy is a tiny open-source device that lives inside your PC case. It connects to your Bluetooth gamepad and proxies input over USB. When you press the home button, it wakes your PC automatically. No more walking across the room to hit the power button -- just grab your controller from the couch and go.

**Who this is for:**
- PC gamers who use wireless controllers from the couch (living room setups, HTPCs)
- Steam Big Picture / couch gaming enthusiasts
- Home theater PC builders
- Raspberry Pi / DIY hardware tinkerers
- People frustrated by Bluetooth pairing issues on PC (PadProxy presents as a standard wired USB gamepad)

**What makes it different:**
- Wake-from-sleep via controller (nothing else does this cleanly)
- Open-source hardware and software (MIT + CERN-OHL-P-2.0)
- Broad controller support (Xbox, PlayStation, Switch Pro, 8BitDo, generic HID)
- Tiny form factor, mounts inside the PC case
- Appears as a standard wired USB gamepad to the OS -- zero driver issues

## Channel Strategy

### Primary: Tindie

Tindie is the right place to list this. The audience is technical, they understand open-source hardware, and they expect indie projects. Low listing fees, no upfront cost, and it's where people go when they're looking for exactly this kind of thing.

**Pricing target:** $35-45 assembled, $20-25 kit (PCB + components, user supplies Pico 2 W)

Pricing covers components, PCB fabrication, and assembly time. This isn't about margins -- it's about making the project self-sustaining so more people can get one. Selling at cost doesn't work long-term (one bad batch eats you alive), but the goal is accessible, not profitable.

**Why two SKUs:**
- Assembled lowers the barrier for people who just want the device
- Kit lowers the price and appeals to the DIY crowd
- Kit buyers often become contributors -- they've soldered the board, they understand the hardware, they file better bug reports

### Not Now: Kickstarter

Kickstarter is a poor fit:
- BOM is ~$10/unit; there's no tooling investment that requires crowdfunding capital
- The audience is niche -- hard to generate the viral momentum Kickstarter rewards
- Fulfillment obligations add stress to something that should be fun
- A failed campaign looks worse than never running one

**When Kickstarter might make sense:** If a v2 needs injection-molded enclosures or a custom PCB with an integrated wireless module -- something that requires real upfront tooling costs -- and there's proven demand from actual users.

### Not Now: Amazon / Major Retail

This isn't a mass-market product. The audience is a specific slice of PC gamers, and they're best reached where they already hang out (Reddit, HN, YouTube, Tindie).

## Getting the Word Out

The point of "marketing" here is simple: let people who have this problem know that a solution exists. Most of these tactics are free because the product genuinely solves a problem -- it just needs to reach the right people.

### Do These (Free / Low Cost)

#### 1. Reddit Posts
Post genuine "I built this" content. The story is authentic -- you built it because you wanted it, and nobody else had made one. Share that.

Communities to post in:
- r/pcgaming -- largest relevant audience
- r/raspberry_pi -- loves Pico projects
- r/homelab -- HTPC overlap
- r/SteamDeck -- couch gaming adjacent
- r/8bitdo -- controller enthusiasts
- r/htpc -- direct target audience
- r/BuildaPC -- hardware tinkerers

**Format:** Photo/GIF of the device + the story of why you built it + link to GitHub. Mention the Tindie listing for people who want one pre-built, but lead with the project, not a sales pitch. Reddit can tell the difference.

#### 2. Hacker News
Open-source hardware projects do well on HN. The clean codebase, CERN-OHL-P license, and the engineering depth of the power state machine are exactly what this audience appreciates. Post as a "Show HN" with the GitHub link.

#### 3. Demo Video (Phone Quality Is Fine)
Film the core experience:
1. Show the PC off / sleeping
2. Pick up controller from couch, press home button
3. PC wakes, controller connects
4. Gaming within seconds

Before/after format is powerful: show the annoying version (walk to PC, press button, walk back, pick up controller, wait for Bluetooth pairing) vs. the PadProxy version. 30-60 seconds max. Post natively to Reddit (not as a YouTube link -- native video gets more engagement).

Also create a 5-10 second GIF version for embedding in the README, Tindie listing, and forum posts.

#### 4. YouTube Reviewers
Send free units to 3-5 small/mid YouTube channels that cover:
- Raspberry Pi projects
- PC peripherals and accessories
- HTPC / living room gaming setups
- DIY electronics

Target channels with 5K-100K subscribers. They're responsive to outreach and genuinely interested in covering projects like this.

#### 5. GitHub as Outreach
For an open-source project, the repo itself is how people find you:
- Keep the README polished with a hero image/GIF at the top
- Respond to issues promptly
- Tag releases properly
- Add a "Supported Controllers" compatibility list (people search for this)
- Link to Tindie from the README for people who want a pre-built unit

### Don't Do These

| Tactic | Why Not |
|--------|---------|
| Hire a marketing team/agency | They won't understand the niche, and this isn't a revenue play that justifies the spend. |
| Paid ads (Google, Facebook, Instagram) | The audience is too niche for broad targeting. Waste of money. |
| Professional video production | The demo sells itself. A phone video of the real experience is more authentic anyway. |
| Influencer sponsorships | Send free units instead. Paying for placements is overkill for a passion project. |
| Trade shows / Maker Faire | Fun but the time and cost don't match the reach. |
| Press outreach (The Verge, etc.) | They won't cover a niche indie device. If they find it organically, great. |

## Branding (Keep It Minimal)

This is an open-source hardware project, not a lifestyle brand. You need:

1. **A logo** -- simple wordmark or icon. Inkscape, 30 minutes. It goes on the Tindie listing, the README, and the PCB silkscreen.
2. **A color** -- pick one accent color for consistency.
3. **A tagline** -- "Wake. Connect. Play." or similar. Short enough for a Tindie subtitle.

You do NOT need:
- Brand guidelines document
- Multiple logo variants
- Custom fonts
- A design system

## Pricing

The goal is sustainability, not profit. Price needs to cover materials + fabrication + assembly time so the project can keep going without losing money on every unit.

| Component | Cost |
|-----------|------|
| BOM (components) | ~$10 |
| PCB fabrication (per unit at low volume) | ~$3-5 |
| 3D printed enclosure | ~$2-3 |
| Assembly and testing time | ~$5-10 |
| **Total cost per assembled unit** | **~$20-28** |

| SKU | Price | Notes |
|-----|-------|-------|
| Assembled unit | $39-45 | Includes enclosure, tested, ready to install |
| Kit (PCB + SMD components) | $19-25 | User supplies Pico 2 W, solders headers |
| PCB only | $9-12 | Bare board for experienced builders |

Prices include a buffer for the inevitable dud board, shipping supplies, and Tindie's cut. The everything-needed-to-build-your-own files are always free on GitHub.

## Keeping Track

Keep it simple:
- Tindie page views and orders
- GitHub stars, forks, and issues
- Reddit post engagement (are the right people seeing it?)
- Support questions (if these spike, the docs need work -- not the marketing)

## Timeline

### Phase 1: Pre-Launch (Now)
- [ ] Commit hardware files to repo (PCB, enclosure)
- [ ] Build 5-10 assembled units
- [ ] Take product photos (natural light, clean background)
- [ ] Film 30-second demo video
- [ ] Create GIF for README
- [ ] Write Tindie listing (see `tindie-listing.md`)
- [ ] Create simple logo
- [ ] Write setup and pairing docs

### Phase 2: Soft Launch
- [ ] List on Tindie
- [ ] Update README with hero image/GIF and Tindie link
- [ ] Post to r/raspberry_pi (friendliest audience, good for feedback)
- [ ] Fix any issues from early adopters
- [ ] Iterate on docs based on questions people ask

### Phase 3: Broader Launch
- [ ] Post to r/pcgaming, r/htpc, r/homelab
- [ ] Submit to Hacker News (Show HN)
- [ ] Send units to 3-5 YouTube reviewers
- [ ] Add compatibility list based on real user reports

### Phase 4: Sustain
- [ ] Respond to community feedback
- [ ] Iterate on hardware based on real-world use
- [ ] Keep the GitHub repo alive and welcoming to contributors
- [ ] If demand is there and a v2 makes sense, consider Kickstarter for tooling costs
