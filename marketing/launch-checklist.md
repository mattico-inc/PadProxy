# PadProxy Launch Checklist

A concrete, ordered checklist for going from "working prototype" to "available for anyone who wants one."

## Stage 1: Hardware Ready

- [ ] PCB design finalized and committed to `pcb/`
- [ ] Gerber files generated and verified with manufacturer (JLCPCB / PCBWay)
- [ ] Order first PCB batch (minimum 10 boards)
- [ ] 3D enclosure designed and committed to `case/`
- [ ] Print test enclosures, verify fit with PCB + Pico 2 W
- [ ] Assemble 5-10 complete units
- [ ] Test each unit end-to-end (BT pairing, USB HID, PC wake, all controller types)

## Stage 2: Documentation

- [ ] Write `docs/setup.md` -- step-by-step installation inside a PC case
- [ ] Write `docs/pairing.md` -- controller pairing instructions per brand
- [ ] Write `docs/troubleshooting.md` -- common issues and fixes
- [ ] Update main README with hero image/GIF at top
- [ ] Add supported controllers compatibility table (tested vs. reported)

## Stage 3: Visual Assets

- [ ] Take product photos (aim for 5-8 images):
  - Device standalone on clean background
  - Device next to common objects for scale (quarter, USB stick)
  - Device mounted inside a PC case
  - Device with a controller in frame
  - Close-up of PCB / components
  - Before/after: messy cable vs. clean install
- [ ] Film 30-second demo video (phone + tripod):
  - Show PC off/sleeping
  - Pick up controller, press home button
  - PC wakes, controller connects
  - Game launches, playing within seconds
- [ ] Create 5-10 second GIF from the demo video
- [ ] Design simple logo (Inkscape, SVG + PNG export)
- [ ] Create Tindie listing banner image (960x720 recommended)

## Stage 4: Tindie Listing

- [ ] Create Tindie seller account
- [ ] Write listing title and description (see `tindie-listing.md`)
- [ ] Upload product photos
- [ ] Set pricing (assembled + kit SKUs)
- [ ] Set shipping options and rates
- [ ] Configure inventory count
- [ ] Preview listing, proofread everything
- [ ] Publish listing (initially unlisted/private for final review)
- [ ] Go live

## Stage 5: Soft Launch

- [ ] Update README.md -- replace "Coming soon!" with actual Tindie link
- [ ] Post to r/raspberry_pi with photos + story
- [ ] Monitor comments, respond to questions
- [ ] Collect feedback, note recurring questions
- [ ] Update docs based on early feedback
- [ ] Fix any hardware/firmware issues from first buyers
- [ ] Ship orders promptly (aim for 1-2 day turnaround)

## Stage 6: Broader Launch

- [ ] Post to r/pcgaming (wait at least 1 week after soft launch)
- [ ] Post to r/htpc
- [ ] Post to r/homelab
- [ ] Post to r/8bitdo
- [ ] Submit to Hacker News as "Show HN"
- [ ] Identify 3-5 YouTube reviewers to contact
- [ ] Draft outreach email (see `reviewer-outreach.md` if created)
- [ ] Ship free units to reviewers
- [ ] Cross-post demo video to relevant Discord servers

## Stage 7: Ongoing

- [ ] Respond to Tindie reviews and questions (every single one)
- [ ] Update compatibility list as users report new controllers
- [ ] Iterate on enclosure design based on feedback
- [ ] Adjust kit vs. assembled inventory based on what people actually want
- [ ] Reorder PCBs before stock runs out (lead time: 1-2 weeks from JLCPCB)
- [ ] Keep GitHub issues and contributions healthy -- this is open source first
