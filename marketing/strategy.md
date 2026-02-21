# PadProxy Indie Marketing Strategy

## Product Positioning

**One-liner:** Press the home button on your controller. Your PC wakes up. You're gaming.

**Elevator pitch:** PadProxy is a tiny open-source device that lives inside your PC case. It connects to your Bluetooth gamepad and proxies input over USB. When you press the home button, it wakes your PC automatically. No more walking across the room to hit the power button -- just grab your controller from the couch and go.

**Target audience:**
- PC gamers who use wireless controllers from the couch (living room setups, HTPCs)
- Steam Big Picture / couch gaming enthusiasts
- Home theater PC builders
- Raspberry Pi / DIY hardware tinkerers
- People frustrated by Bluetooth pairing issues on PC (PadProxy presents as a standard wired USB gamepad)

**Key differentiators:**
- Wake-from-sleep via controller (no other product does this cleanly)
- Open-source hardware and software (MIT + CERN-OHL-P-2.0)
- Broad controller support (Xbox, PlayStation, Switch Pro, 8BitDo, generic HID)
- Tiny form factor, mounts inside the PC case
- Appears as a standard wired USB gamepad to the OS -- zero driver issues

## Channel Strategy

### Primary: Tindie

Tindie is the right first channel. The audience is technical, expects indie hardware, and understands what a Pico-based project is. Low listing fees, no upfront cost, built-in discovery for niche electronics.

**Pricing target:** $35-45 assembled, $20-25 kit (PCB + components, user supplies Pico 2 W)

**Why two SKUs:**
- Assembled captures the "just works" buyer
- Kit captures the DIY/hacker crowd and lowers the barrier to trying it
- Kit buyers become contributors and evangelists

### Secondary: Own Store

After proving demand on Tindie, set up a simple Shopify or BigCartel store. This gives better margins (no Tindie cut) and more control over branding. Only worth the effort after consistent Tindie sales.

### Not Now: Kickstarter

Kickstarter is a poor fit at this stage:
- BOM is ~$10/unit; no tooling investment that requires crowdfunding capital
- The audience is niche -- hard to generate the viral momentum Kickstarter rewards
- Fulfillment obligations with thin margins create stress, not opportunity
- A failed campaign looks worse than never running one

**When Kickstarter makes sense:** If a v2 needs injection-molded enclosures, a custom PCB with integrated wireless module, or other tooling that requires upfront capital -- and you have proven sales numbers to show demand.

### Not Now: Amazon / Major Retail

Premature. The volume isn't there, the margins don't support it, and the support burden would be overwhelming as a solo operation.

## Marketing Tactics

### Do These (Free / Low Cost)

#### 1. Reddit Launch Posts
The single highest-ROI marketing activity. Post genuine "I built this" content in:
- r/pcgaming -- largest relevant audience
- r/raspberry_pi -- loves Pico projects
- r/homelab -- HTPC overlap
- r/SteamDeck -- couch gaming adjacent
- r/8bitdo -- controller enthusiasts
- r/htpc -- direct target audience
- r/BuildaPC -- hardware tinkerers

**Format:** Photo/GIF of the device + short story of why you built it + link to GitHub. Let the comments drive interest to the Tindie listing. Do NOT lead with a sales pitch -- Reddit punishes that.

#### 2. Hacker News
Open-source hardware projects perform well on HN. The clean codebase, CERN-OHL-P license, and technical depth of the power management design are exactly what this audience appreciates. Post as a "Show HN" with the GitHub link.

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

Target channels with 5K-100K subscribers. They're hungry for content, responsive to outreach, and won't charge for reviews. A genuine review from a trusted small channel converts better than a paid placement.

#### 5. GitHub as Marketing
For technical products, the repo IS the marketing:
- Keep the README polished with a hero image/GIF at the top
- Respond to issues promptly
- Tag releases properly
- Add a "Supported Controllers" compatibility list (people search for this)
- Link to Tindie from the README

### Don't Do These (Yet)

| Tactic | Why Not |
|--------|---------|
| Hire a marketing team/agency | They won't understand the niche. Your budget buys prototypes and reviewer units instead. |
| Paid ads (Google, Facebook, Instagram) | The audience is too niche for broad targeting. CAC would be brutal. |
| Professional video production | The demo sells itself. Polish doesn't add value at this volume. |
| Influencer sponsorships | Send free units instead. Paid placements feel inauthentic for a $40 indie device. |
| Trade shows / Maker Faire | Time and cost don't justify the reach at this stage. |
| Press outreach (The Verge, etc.) | They won't cover a $40 niche device. Maybe after proven traction. |

## Branding (Keep It Minimal)

Don't over-invest in branding before you have sales. You need:

1. **A logo** -- simple wordmark or icon. Inkscape, 30 minutes. It goes on the Tindie listing, the README, and the PCB silkscreen.
2. **A color** -- pick one accent color for consistency. Use it in the logo and docs.
3. **A tagline** -- "Wake. Connect. Play." or similar. Short enough for a Tindie subtitle.

You do NOT need:
- Brand guidelines document
- Multiple logo variants
- Custom fonts
- A design system

## Pricing Strategy

| Component | Cost |
|-----------|------|
| BOM (components) | ~$10 |
| PCB fabrication (per unit at low volume) | ~$3-5 |
| 3D printed enclosure | ~$2-3 |
| Assembly labor (your time) | ~$5-10 |
| **Total cost per assembled unit** | **~$20-28** |

| SKU | Price | Margin | Notes |
|-----|-------|--------|-------|
| Assembled unit | $39-45 | ~40-50% | Includes enclosure, tested, ready to install |
| Kit (PCB + SMD components) | $19-25 | ~50-60% | User supplies Pico 2 W, solders headers |
| PCB only | $9-12 | ~60% | For experienced builders, bare board only |

Start at the higher end of each range. You can always run a sale or lower prices; raising prices is harder. Tindie buyers expect to pay a premium for indie/artisan hardware.

## Metrics to Track

Keep it simple. Track these weekly:
- Tindie page views and conversion rate
- Units sold (assembled vs. kit)
- GitHub stars and forks
- Reddit post engagement (upvotes, comments, saves)
- Support requests per unit sold (if this climbs, your docs need work)

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
- [ ] Fix any issues from first buyers
- [ ] Iterate on docs based on support questions

### Phase 3: Broader Launch
- [ ] Post to r/pcgaming, r/htpc, r/homelab
- [ ] Submit to Hacker News (Show HN)
- [ ] Send units to 3-5 YouTube reviewers
- [ ] Add compatibility list based on real user reports

### Phase 4: Sustain
- [ ] Respond to reviews and community feedback
- [ ] Iterate on hardware based on user issues
- [ ] Consider own store if Tindie volume justifies it
- [ ] Consider Kickstarter for v2 if demand is proven
