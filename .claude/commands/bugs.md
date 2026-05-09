---
description: List active bugs sorted by severity.
---

List `docs/bugs/active/`. For each:

- ID and title
- Severity (with visual indicator: 🔴 Critical / 🟠 High / 🟡 Medium / ⚪ Low)
- Status (open / investigating / fix-proposed)
- Logged date
- One-line symptom (truncate if long)

Sort: Critical → High → Medium → Low. Within severity, oldest first.

Then summarize:

- Total active count by severity
- Anything Critical or High that's been open more than 7 days (call out — these should be addressed)
- If 0 active bugs: say so plainly.

Keep output to a single screen. If there are more than ~15 bugs, show first 15 + "(N more, run `ls docs/bugs/active/` to see all)".

Don't propose fixing anything. This is a status command.
